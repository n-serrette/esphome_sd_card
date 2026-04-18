#include "webserver_sd.h"

#include <cerrno>
#include <cstdio>
#include <fcntl.h>
#include <memory>
#include <unistd.h>
#include <sys/stat.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_heap_caps.h"

#include "esp_http_server.h"
#include "esphome/components/network/util.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace webserver_sd {

static const char* TAG = "sd_file_server";

SDFileServer::SDFileServer(web_server_base::WebServerBase* base)
    : base_(base) {}

void SDFileServer::setup() { this->base_->add_handler(this); }

void SDFileServer::dump_config() {
  ESP_LOGCONFIG(TAG, "Webserver SD:");
  ESP_LOGCONFIG(TAG, "  Address: %s:%u", network::get_use_address(),
                this->base_->get_port());
  ESP_LOGCONFIG(TAG, "  Url Prefix: %s", this->url_prefix_.c_str());
  ESP_LOGCONFIG(TAG, "  Root Path: %s", this->sd_path_.c_str());
  ESP_LOGCONFIG(TAG, "  Deletion Enabled: %s",
                TRUEFALSE(this->deletion_enabled_));
  ESP_LOGCONFIG(TAG, "  Download Enabled : %s",
                TRUEFALSE(this->download_enabled_));
  ESP_LOGCONFIG(TAG, "  Upload Enabled : %s", TRUEFALSE(this->upload_enabled_));
}

bool SDFileServer::canHandle(AsyncWebServerRequest* request) const {
  char url_buf[AsyncWebServerRequest::URL_BUF_SIZE];
  return str_startswith(std::string(request->url_to(url_buf)),
                        this->build_prefix());
}

void SDFileServer::handleRequest(AsyncWebServerRequest* request) {
  char url_buf[AsyncWebServerRequest::URL_BUF_SIZE];
  std::string url = std::string(request->url_to(url_buf));
  if (!str_startswith(url, this->build_prefix()))
    return;

  auto method = request->method();

  if (method == HTTP_DELETE) {
    this->handle_delete(request);
    return;
  }

  if (method == HTTP_POST) {
    this->handle_upload(request);
    return;
  }

  if (method == HTTP_GET) {
    // Legacy workaround: ?delete param triggers deletion via GET
    if (request->hasParam("delete")) {
      this->handle_delete(request);
      return;
    }

    this->handle_get(request);
  }
}

void SDFileServer::handle_upload(AsyncWebServerRequest *request) {
  if (!this->upload_enabled_) {
    request->send(403, "application/json", "{ \"error\": \"file upload is disabled\" }");
    return;
  }

  char url_buf[AsyncWebServerRequest::URL_BUF_SIZE];
  std::string extracted =
      this->extract_path_from_url(std::string(request->url_to(url_buf)));
  std::string path = this->build_absolute_path(extracted);

  // URL must identify a file, not a directory
  if (path.empty() || path.back() == '/' || this->sd_card_->is_directory(path)) {
    request->send(400, "application/json",
                  "{ \"error\": \"target URL must be a file path, not a directory\" }");
    return;
  }

  std::string abs_path = this->sd_card_->build_path(path);

  // POSIX open() — avoids newlib stdio call-stack depth.
  int fd = open(abs_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (fd < 0) {
    ESP_LOGE(TAG, "handle_upload: open failed for '%s' (errno %d)", abs_path.c_str(), errno);
    request->send(500, "application/json", "{ \"error\": \"failed to open file for writing\" }");
    return;
  }

  // Access the underlying ESP-IDF httpd handle — same technique as handle_download_stream.
  httpd_req_t *req_h = static_cast<httpd_req_t *>(*request);

  // Constants matching ESPHome's OTA handler (web_server_idf.cpp).
  static constexpr size_t CHUNK_SIZE  = 1460;        // TCP MSS
  static constexpr size_t YIELD_EVERY = 16 * 1024;   // yield every 16 KB

  // Recv buffer in internal DRAM — FatFS DMA cannot access PSRAM.
  uint8_t *buf = static_cast<uint8_t *>(
      heap_caps_malloc(CHUNK_SIZE, MALLOC_CAP_INTERNAL));
  if (!buf) {
    ESP_LOGE(TAG, "handle_upload: failed to allocate recv buffer");
    close(fd);
    request->send(503, "application/json", "{ \"error\": \"out of memory\" }");
    return;
  }

  size_t remaining = req_h->content_len;
  size_t bytes_since_yield = 0;
  bool ok = true;

  while (remaining > 0) {
    size_t want = (remaining < CHUNK_SIZE) ? remaining : CHUNK_SIZE;
    int n = httpd_req_recv(req_h, reinterpret_cast<char *>(buf), want);
    if (n <= 0) {
      ESP_LOGE(TAG, "handle_upload: recv error %d (errno %d)", n, errno);
      ok = false;
      break;
    }
    if (write(fd, buf, static_cast<size_t>(n)) != n) {
      ESP_LOGE(TAG, "handle_upload: write error (errno %d)", errno);
      ok = false;
      break;
    }
    remaining -= static_cast<size_t>(n);
    bytes_since_yield += static_cast<size_t>(n);
    if (bytes_since_yield >= YIELD_EVERY) {
      vTaskDelay(1);  // yield to watchdog / other tasks — same as ESPHome OTA
      bytes_since_yield = 0;
    }
  }

  heap_caps_free(buf);
  fsync(fd);
  close(fd);

  if (!ok) {
    unlink(abs_path.c_str());  // remove partial file on error
    httpd_resp_send_err(req_h, HTTPD_500_INTERNAL_SERVER_ERROR, nullptr);
    return;
  }

  this->sd_card_->update_sensors();
  ESP_LOGI(TAG, "Upload complete: %s (%u bytes)", abs_path.c_str(),
           static_cast<unsigned>(req_h->content_len));

  // Send 201 via raw httpd API — same pattern as handle_download_stream.
  httpd_resp_set_status(req_h, "201 Created");
  httpd_resp_set_type(req_h, "text/plain");
  httpd_resp_send(req_h, "upload success", HTTPD_RESP_USE_STRLEN);
}

void SDFileServer::set_url_prefix(const std::string& prefix) {
  this->url_prefix_ = prefix;
}
void SDFileServer::set_sd_path(const std::string& path) {
  this->sd_path_ = path;
}
void SDFileServer::set_sd_card(sd_card::SdCard* card) { this->sd_card_ = card; }
void SDFileServer::set_deletion_enabled(bool allow) {
  this->deletion_enabled_ = allow;
}
void SDFileServer::set_download_enabled(bool allow) {
  this->download_enabled_ = allow;
}
void SDFileServer::set_upload_enabled(bool allow) {
  this->upload_enabled_ = allow;
}

void SDFileServer::handle_get(AsyncWebServerRequest* request) const {
  char url_buf[AsyncWebServerRequest::URL_BUF_SIZE];
  std::string extracted =
      this->extract_path_from_url(std::string(request->url_to(url_buf)));
  std::string path = this->build_absolute_path(extracted);

  if (!this->sd_card_->is_directory(path)) {
    handle_download(request, path);
    return;
  }
  handle_index(request, path);
}

// Streams the directory listing as CSV (name,size,is_directory) using chunked
// transfer directly via the underlying httpd handle — no buffering, no heap
// accumulation per entry. Yields to FreeRTOS every 32 entries (inside sd_card).
void SDFileServer::handle_index(AsyncWebServerRequest* request,
                                const std::string& path) const {
  httpd_req_t *req_h = static_cast<httpd_req_t *>(*request);
  httpd_resp_set_status(req_h, "200 OK");
  httpd_resp_set_type(req_h, "text/plain; charset=utf-8");

  static const char HEADER[] = "name,size,is_directory\r\n";
  httpd_resp_send_chunk(req_h, HEADER, sizeof(HEADER) - 1);

  this->sd_card_->list_directory_file_info_stream(
      path.c_str(), 0, [req_h](const sd_card::FileInfo &entry) -> bool {
        std::string name = Path::file_name(entry.path);
        // CSV-escape name: wrap in double-quotes and double any inner quotes.
        std::string quoted;
        quoted.reserve(name.size() + 2);
        quoted += '"';
        for (char c : name) {
          if (c == '"') quoted += '"';  // RFC 4180: double the quote
          quoted += c;
        }
        quoted += '"';

        char row[320];
        snprintf(row, sizeof(row), "%s,%u,%s\r\n",
                 quoted.c_str(),
                 static_cast<unsigned>(entry.size),
                 entry.is_directory ? "true" : "false");
        return httpd_resp_send_chunk(req_h, row, HTTPD_RESP_USE_STRLEN) == ESP_OK;
      });

  httpd_resp_send_chunk(req_h, nullptr, 0);  // terminate chunked transfer
}

void SDFileServer::handle_download(AsyncWebServerRequest *request,
                                   const std::string &path) const {
  if (!this->download_enabled_) {
    request->send(403, "application/json",
                  "{ \"error\": \"file download is disabled\" }");
    return;
  }

  size_t file_size = this->sd_card_->file_size(path);
  if (file_size == static_cast<size_t>(-1)) {
    request->send(404, "application/json", "{ \"error\": \"file not found\" }");
    return;
  }

  std::string mime = Path::mime_type(path);
  this->handle_download_stream(request, path, mime, file_size);
}

void SDFileServer::handle_download_stream(AsyncWebServerRequest *request,
                                          const std::string &path,
                                          const std::string &mime,
                                          size_t file_size) const {
  std::string abs_path = this->sd_card_->build_path(path);

  // Use POSIX open/read instead of fopen/fread to avoid the hidden newlib
  // stdio buffer that malloc allocates and may land in PSRAM.  On ESP32-S3
  // the SDMMC AHB-DMA cannot access PSRAM, which causes a "Cache error /
  // MMU entry fault" crash.
  int fd = open(abs_path.c_str(), O_RDONLY);
  if (fd < 0) {
    ESP_LOGE(TAG, "handle_download_stream: open failed for '%s' (errno %d)", path.c_str(), errno);
    request->send(503, "application/json", "{ \"error\": \"file read failed\" }");
    return;
  }

  std::string fname = Path::file_name(path);
  std::string disposition = "attachment; filename=\"" + fname + "\"";

  // Use the underlying httpd_req_t directly for chunked transfer —
  // the ESP-IDF AsyncWebServerRequest wrapper has no beginChunkedResponse().
  httpd_req_t *req_h = static_cast<httpd_req_t *>(*request);
  httpd_resp_set_status(req_h, "200 OK");
  httpd_resp_set_type(req_h, mime.c_str());
  httpd_resp_set_hdr(req_h, "Content-Disposition", disposition.c_str());

  // Allocate the read buffer from internal DMA-capable DRAM (not the task
  // stack, not PSRAM) so that FatFS can DMA directly into it for aligned
  // sector reads without triggering a cache fault.
  const size_t BUF_SIZE = 4096;
  uint8_t *buf = static_cast<uint8_t *>(
      heap_caps_malloc(BUF_SIZE, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA));
  if (!buf) {
    ESP_LOGE(TAG, "handle_download_stream: failed to allocate read buffer");
    close(fd);
    request->send(503, "application/json", "{ \"error\": \"out of memory\" }");
    return;
  }

  ssize_t n;
  bool ok = true;
  while ((n = read(fd, buf, BUF_SIZE)) > 0) {
    if (httpd_resp_send_chunk(req_h, reinterpret_cast<const char *>(buf), n) != ESP_OK) {
      ESP_LOGW(TAG, "Streaming download: client disconnected for '%s'", path.c_str());
      ok = false;
      break;
    }
  }
  if (ok)
    httpd_resp_send_chunk(req_h, nullptr, 0);  // terminate chunked transfer
  heap_caps_free(buf);
  close(fd);
  ESP_LOGI(TAG, "Streaming download: %s (%u bytes)", path.c_str(), static_cast<unsigned>(file_size));
}

void SDFileServer::handle_delete(AsyncWebServerRequest* request) {
  if (!this->deletion_enabled_) {
    request->send(401, "application/json",
                  "{ \"error\": \"file deletion is disabled\" }");
    return;
  }

  char url_buf[AsyncWebServerRequest::URL_BUF_SIZE];
  std::string extracted =
      this->extract_path_from_url(std::string(request->url_to(url_buf)));
  std::string path = this->build_absolute_path(extracted);

  if (this->sd_card_->is_directory(path)) {
    request->send(401, "application/json",
                  "{ \"error\": \"cannot delete a directory\" }");
    return;
  }

  if (this->sd_card_->delete_file(path)) {
    request->send(204, "application/json", "{}");
    return;
  }

  request->send(401, "application/json",
                "{ \"error\": \"failed to delete file\" }");
}

std::string SDFileServer::build_prefix() const {
  if (this->url_prefix_.empty() || this->url_prefix_[0] != '/')
    return "/" + this->url_prefix_;
  return this->url_prefix_;
}

std::string SDFileServer::extract_path_from_url(const std::string& url) const {
  std::string prefix = this->build_prefix();
  return url.substr(prefix.size(), url.size() - prefix.size());
}

std::string SDFileServer::build_absolute_path(std::string relative_path) const {
  if (relative_path.empty()) return this->sd_path_;
  return Path::join(this->sd_path_, relative_path);
}

std::string Path::file_name(const std::string& path) {
  size_t pos = path.rfind(separator);
  if (pos != std::string::npos) return path.substr(pos + 1);
  return "";
}

bool Path::is_absolute(const std::string& path) {
  return !path.empty() && path[0] == separator;
}

bool Path::trailing_slash(const std::string& path) {
  return !path.empty() && path.back() == separator;
}

std::string Path::join(const std::string& first, const std::string& second) {
  std::string result = first;
  if (!trailing_slash(first) && !is_absolute(second))
    result.push_back(separator);
  if (trailing_slash(first) && is_absolute(second)) result.pop_back();
  result.append(second);
  return result;
}

std::string Path::remove_root_path(std::string path, const std::string& root) {
  if (!str_startswith(path, root)) return path;
  if (path.size() == root.size() || path.size() < 2) return "/";
  return path.erase(0, root.size());
}

std::vector<std::string> Path::split_path(std::string path) {
  std::vector<std::string> parts;
  size_t pos = 0;
  while ((pos = path.find('/')) != std::string::npos) {
    std::string part = path.substr(0, pos);
    if (!part.empty()) parts.push_back(part);
    path.erase(0, pos + 1);
  }
  if (!path.empty()) parts.push_back(path);
  return parts;
}

std::string Path::extension(const std::string &file) {
  size_t pos = file.find_last_of('.');
  if (pos == std::string::npos) return "";
  return file.substr(pos + 1);
}

std::string Path::file_type(const std::string &file) {
  std::string ext = extension(file);
  if (ext.empty()) return "file";
  std::string lower = ext;
  std::transform(lower.begin(), lower.end(), lower.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  if (lower == "csv" || lower == "log" || lower == "txt") return "text";
  if (lower == "jpg" || lower == "jpeg" || lower == "png" || lower == "bmp") return "image";
  if (lower == "mp3" || lower == "wav") return "audio";
  if (lower == "mp4" || lower == "avi" || lower == "webm") return "video";
  if (lower == "json" || lower == "xml") return "data";
  if (lower == "zip" || lower == "gz" || lower == "tar") return "archive";
  return "file";
}

std::string Path::mime_type(const std::string& file) {
  static const std::map<std::string, std::string> file_types = {
      {"mp3", "audio/mpeg"},        {"wav", "audio/vnd.wav"},
      {"png", "image/png"},         {"jpg", "image/jpeg"},
      {"jpeg", "image/jpeg"},       {"bmp", "image/bmp"},
      {"txt", "text/plain"},        {"log", "text/plain"},
      {"csv", "text/csv"},          {"html", "text/html"},
      {"css", "text/css"},          {"js", "text/javascript"},
      {"json", "application/json"}, {"xml", "application/xml"},
      {"zip", "application/zip"},   {"gz", "application/gzip"},
      {"tar", "application/x-tar"}, {"mp4", "video/mp4"},
      {"avi", "video/x-msvideo"},   {"webm", "video/webm"}};

  std::string ext = Path::extension(file);
  ESP_LOGD(TAG, "ext : %s", ext.c_str());
  if (!ext.empty()) {
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    auto it = file_types.find(ext);
    if (it != file_types.end()) return it->second;
  }
  return "application/octet-stream";
}

}  // namespace webserver_sd
}  // namespace esphome
