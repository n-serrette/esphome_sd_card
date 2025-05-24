#include "sd_file_server.h"
#include "path.h"

#include <map>
#include "esphome/core/log.h"
#include "esphome/components/network/util.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace sd_file_server {

static const char *TAG = "sd_file_server";

#ifdef USE_ESP_IDF
esp_err_t httpd_send_all(httpd_req_t *r, const char *buffer, size_t len) {
  while (len > 0) {
    const auto sent = httpd_send(r, buffer, len);
    switch (sent) {
      case HTTPD_SOCK_ERR_INVALID:
      case HTTPD_SOCK_ERR_TIMEOUT:
      case HTTPD_SOCK_ERR_FAIL:
        ESP_LOGE(TAG, "httpd_send failed: %s", esp_err_to_name(sent));
        return sent;
    }
    ESP_LOGVV(TAG, "%d bytes written to httpd", sent);
    len -= sent;
    buffer += sent;
  }
  return ESP_OK;
}

esp_err_t httpd_printf(httpd_req_t *r, const char *fmt, ...) {
  va_list args;

  va_start(args, fmt);
  const int length = vsnprintf(nullptr, 0, fmt, args);
  va_end(args);

  std::string str;
  str.resize(length);

  va_start(args, fmt);
  vsnprintf(&str[0], length + 1, fmt, args);
  va_end(args);

  return httpd_send_all(r, str.data(), str.size());
}

esp_err_t httpd_print(httpd_req_t *r, const char *str) { return httpd_send_all(r, str, strlen(str)); }

esp_err_t esp_http_server_dispatch_event(int32_t event_id, const void *event_data, size_t event_data_size) {
  ESP_LOGVV(TAG, "Dispatching event %ld", event_id);
  esp_err_t err = esp_event_post(ESP_HTTP_SERVER_EVENT, event_id, event_data, event_data_size, ULONG_MAX);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to post esp_http_server event: %s", esp_err_to_name(err));
  }
  return err;
}

#endif  // USE_ESP_IDF

SDFileServer::SDFileServer(web_server_base::WebServerBase *base) : base_(base) {}

esp_err_t get_handler(httpd_req_t *req) {
  /* Send a simple response */
  const char resp[] = "URI GET Response";
  httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}

void SDFileServer::setup() { this->base_->add_handler(this); }

void SDFileServer::dump_config() {
  ESP_LOGCONFIG(TAG, "SD File Server:");
  ESP_LOGCONFIG(TAG, "  Address: %s:%u", network::get_use_address().c_str(), this->base_->get_port());
  ESP_LOGCONFIG(TAG, "  Url Prefix: %s", this->url_prefix_.c_str());
  ESP_LOGCONFIG(TAG, "  Root Path: %s", this->root_path_.c_str());
  ESP_LOGCONFIG(TAG, "  Deletation Enabled: %s", TRUEFALSE(this->deletion_enabled_));
  ESP_LOGCONFIG(TAG, "  Download Enabled : %s", TRUEFALSE(this->download_enabled_));
  ESP_LOGCONFIG(TAG, "  Upload Enabled : %s", TRUEFALSE(this->upload_enabled_));
}

bool SDFileServer::canHandle(AsyncWebServerRequest *request) {
  ESP_LOGD(TAG, "can handle %s %u", request->url().c_str(),
           str_startswith(std::string(request->url().c_str()), this->build_prefix()));
  return str_startswith(std::string(request->url().c_str()), this->build_prefix());
}

void SDFileServer::handleRequest(AsyncWebServerRequest *request) {
  ESP_LOGD(TAG, "%s", request->url().c_str());
  if (str_startswith(std::string(request->url().c_str()), this->build_prefix())) {
    switch (request->method()) {
      case HTTP_GET:
        this->handle_get(request);
        break;
      // case HTTP_HEAD:
      //   this->handle_get(request, true);
      //   break;
      case HTTP_DELETE:
        this->handle_delete(request);
        break;
    }
  }

  void SDFileServer::handleUpload(AsyncWebServerRequest * request, const String &filename, size_t index, uint8_t *data,
                                  size_t len, bool final) {
    if (!this->upload_enabled_) {
      request->send(401, "application/json", "{ \"error\": \"file upload is disabled\" }");
      return;
    }
    std::string extracted = this->extract_path_from_url(std::string(request->url().c_str()));
    std::string path = this->build_absolute_path(extracted);

    if (index == 0 && !this->sd_mmc_card_->is_directory(path)) {
      auto response = request->beginResponse(401, "application/json", "{ \"error\": \"invalid upload folder\" }");
      response->addHeader("Connection", "close");
      request->send(response);
      return;
    }
    std::string file_name(filename.c_str());
    if (index == 0) {
      ESP_LOGD(TAG, "uploading file %s to %s", file_name.c_str(), path.c_str());
      this->sd_mmc_card_->write_file(Path::join(path, file_name).c_str(), data, len);
      return;
    }
    this->sd_mmc_card_->append_file(Path::join(path, file_name).c_str(), data, len);
    if (final) {
      auto response = request->beginResponse(201, "text/html", "upload success");
      response->addHeader("Connection", "close");
      request->send(response);
      return;
    }
  }

  void SDFileServer::set_url_prefix(std::string const &prefix) { this->url_prefix_ = prefix; }

  void SDFileServer::set_root_path(std::string const &path) { this->root_path_ = path; }

  void SDFileServer::set_sd_mmc_card(sd_mmc_card::SdCard * card) { this->sd_mmc_card_ = card; }

  void SDFileServer::set_deletion_enabled(bool allow) { this->deletion_enabled_ = allow; }

  void SDFileServer::set_download_enabled(bool allow) { this->download_enabled_ = allow; }

  void SDFileServer::set_upload_enabled(bool allow) { this->upload_enabled_ = allow; }

  void SDFileServer::handle_get(AsyncWebServerRequest * request) const {
    std::string extracted = this->extract_path_from_url(std::string(request->url().c_str()));
    std::string path = this->build_absolute_path(extracted);

    if (!this->sd_mmc_card_->is_directory(path)) {
      this->handle_download(request, path);
      return;
    }

    this->handle_index(request, path);
  }

  void SDFileServer::write_row(AsyncResponseStream * response, sd_mmc_card::FileInfo const &info) const {
    std::string uri = "/" + Path::join(this->url_prefix_, Path::remove_root_path(info.path, this->root_path_));
    std::string file_name = Path::file_name(info.path);
    response->print("<tr><td>");
    if (info.is_directory) {
      response->print("<a href=\"");
      response->print(uri.c_str());
      response->print("\">");
      response->print(file_name.c_str());
      response->print("</a>");
    } else {
      response->print(file_name.c_str());
    }
    response->print("</td><td>");

    if (info.is_directory) {
      response->print("Folder");
    } else {
      response->print("<span class=\"file-type\">");
      response->print(Path::file_type(file_name).c_str());
      response->print("</span>");
    }
    response->print("</td><td>");
    if (!info.is_directory) {
      response->print(sd_mmc_card::format_size(info.size).c_str());
    }
    response->print("</td><td><div class=\"file-actions\">");
    if (!info.is_directory) {
      if (this->download_enabled_) {
        response->print("<button onClick=\"download_file('");
        response->print(uri.c_str());
        response->print("','");
        response->print(file_name.c_str());
        response->print("')\">Download</button>");
      }
      if (this->deletion_enabled_) {
        response->print("<button onClick=\"delete_file('");
        response->print(uri.c_str());
        response->print("')\">Delete</button>");
      }
    }
    response->print("<div></td></tr>");
  }

  void SDFileServer::handle_index(AsyncWebServerRequest * request, std::string const &path) const {
    ESP_LOGVV(TAG, "handling index");
    AsyncResponseStream *response = request->beginResponseStream("text/html");
    response->print(F(R"(
  <!DOCTYPE html>
  <html lang=\"en\">
  <head>
    <meta charset=UTF-8>
    <meta name=viewport content=\"width=device-width, initial-scale=1,user-scalable=no\">
    <title>SD Card Files</title>
    <style>
    body {
      font-family: 'Segoe UI', system-ui, sans-serif;
      margin: 0;
      padding: 2rem;
      background: #f5f5f7;
      color: #1d1d1f;
    }
    h1 {
      color: #0066cc;
      margin-bottom: 1.5rem;
      display: flex;
      align-items: center;
      gap: 1rem;
    }
    .container {
      max-width: 1200px;
      margin: 0 auto;
      background: white;
      border-radius: 12px;
      box-shadow: 0 4px 12px rgba(0, 0, 0, 0.1);
      padding: 2rem;
    }
    table {
      width: 100%;
      border-collapse: collapse;
      margin-top: 1.5rem;
    }
    th, td {
      padding: 12px;
      text-align: left;
      border-bottom: 1px solid #e0e0e0;
    }
    th {
      background: #f8f9fa;
      font-weight: 500;
    }
    .file-actions {
      display: flex;
      gap: 8px;
    }
    button {
      padding: 6px 12px;
      border: none;
      border-radius: 6px;
      background: #0066cc;
      color: white;
      cursor: pointer;
      transition: background 0.2s;
    }
    button:hover {
      background: #0052a3;
    }
    .upload-form {
      margin-bottom: 2rem;
      padding: 1rem;
      background: #f8f9fa;
      border-radius: 8px;
    }
    .upload-form input[type="file"] {
      margin-right: 1rem;
    }
    .breadcrumb {
      margin-bottom: 1.5rem;
      font-size: 0.9rem;
      color: #666;
    }
    .breadcrumb a {
      color: #0066cc;
      text-decoration: none;
    }
    .breadcrumb a:hover {
      text-decoration: underline;
    }
    .breadcrumb a:not(:last-child)::after {
      display: inline-block;
      margin: 0 .25rem;
      content: ">";
    }
    .folder {
      color: #0066cc;
      font-weight: 500;
    }
    .file-type {
      color: #666;
      font-size: 0.9rem;
    }
    .folder-icon {
      width: 20px;
      height: 20px;
      margin-right: 8px;
      vertical-align: middle;
    }
    .header-actions {
      display: flex;
      align-items: center;
      gap: 1rem;
    }
    .header-actions button {
      background: #4CAF50;
    }
    .header-actions button:hover {
      background: #45a049;
    }
  </style>
  </head>
  <body>
  <div class="container">
    <div class="header-actions">
      <h1>SD Card Files</h1>
      <button onclick="window.location.href='/'">Go to web server</button>
    </div>
    <div class="breadcrumb">
      <a href="/">Home</a>)"));

    std::string current_path = "/";
    std::string relative_path = Path::join(this->url_prefix_, Path::remove_root_path(path, this->root_path_));
    std::vector<std::string> parts = Path::split_path(relative_path);
    for (std::string const &part : parts) {
      if (!part.empty()) {
        current_path = Path::join(current_path, part);
        response->print("<a href=\"");
        response->print(current_path.c_str());
        response->print("\">");
        response->print(part.c_str());
        response->print("</a>");
      }
    }
    response->print(F("</div>"));

    if (this->upload_enabled_)
      response->print(F("<div class=\"upload-form\"><form method=\"POST\" enctype=\"multipart/form-data\">"
                        "<input type=\"file\" name=\"file\"><input type=\"submit\" value=\"upload\"></form></div>"));

    response->print(F("<table><thead><tr>"
                      "<th>Name</th>"
                      "<th>Type</th>"
                      "<th>Size</th>"
                      "<th>Actions</th>"
                      "</tr></thead><tbody>"));

    auto entries = this->sd_mmc_card_->list_directory_file_info(path, 0);
    for (auto const &entry : entries)
      write_row(response, entry);

    response->print(F("</tbody></table>"
                      "<script>"
                      "function delete_file(path) {fetch(path, {method: \"DELETE\"});}"
                      "function download_file(path, filename) {"
                      "fetch(path).then(response => response.blob())"
                      ".then(blob => {"
                      "const link = document.createElement('a');"
                      "link.href = URL.createObjectURL(blob);"
                      "link.download = filename;"
                      "link.click();"
                      "}).catch(console.error);"
                      "} "
                      "</script>"
                      "</body></html>"));

    request->send(response);
  }

  void SDFileServer::handle_download(AsyncWebServerRequest * request, std::string const &path) const {
    if (!this->download_enabled_) {
      request->send(401, "application/json", "{ \"error\": \"file download is disabled\" }");
      return;
    }

    auto file = this->sd_mmc_card_->open(path.c_str(), "rb");
    if (!file) {
      request->send(401, "application/json", "{ \"error\": \"failed to open file\" }");
      return;
    }
#ifdef USE_ESP_IDF
    // httpd_resp_send(*request, "Hello there", HTTPD_RESP_USE_STRLEN);
    const auto data_len = file.size();
    httpd_printf(*request, "HTTP/1.1 %s\r\nContent-Type: %s\r\nContent-Length: %d\r\n", HTTPD_200,
                 Path::mime_type(path).c_str(), data_len);
    std::string buffer(1024, 0);
    httpd_print(*request, "Cache-Control: no-cache\r\n");
    httpd_print(*request, "\r\n");
    auto fd = httpd_req_to_sockfd(*request);
    esp_http_server_dispatch_event(HTTP_SERVER_EVENT_HEADERS_SENT, &fd, sizeof(int));

    for (auto remain = data_len; remain > 0;) {
      const auto read = file.read(buffer.data(), buffer.capacity());
      if (read == 0) {
        ESP_LOGE(TAG, "Unexpected end of file");
        break;
      }
      ESP_LOGVV(TAG, "Sending chunk of size: %d", read);
      httpd_send_all(*request, buffer.data(), read);
      remain -= read;
    }
    esp_http_server_event_data evt_data = {
        .fd = fd,
        .data_len = static_cast<int>(data_len),
    };
    esp_http_server_dispatch_event(HTTP_SERVER_EVENT_SENT_DATA, &evt_data, sizeof(esp_http_server_event_data));

#else   // USE_ESP_IDF
    request->send(request->beginResponse(file, path));
#endif  // USE_ESP_IDF
  }

  void SDFileServer::handle_delete(AsyncWebServerRequest * request) {
    if (!this->deletion_enabled_) {
      request->send(401, "application/json", "{ \"error\": \"file deletion is disabled\" }");
      return;
    }
    std::string extracted = this->extract_path_from_url(std::string(request->url().c_str()));
    std::string path = this->build_absolute_path(extracted);
    if (this->sd_mmc_card_->is_directory(path)) {
      request->send(401, "application/json", "{ \"error\": \"cannot delete a directory\" }");
      return;
    }
    if (this->sd_mmc_card_->delete_file(path)) {
      request->send(204, "application/json", "{}");
      return;
    }
    request->send(401, "application/json", "{ \"error\": \"failed to delete file\" }");
  }

  std::string SDFileServer::build_prefix() const {
    if (this->url_prefix_.length() == 0 || this->url_prefix_.at(0) != '/')
      return "/" + this->url_prefix_;
    return this->url_prefix_;
  }

  std::string SDFileServer::extract_path_from_url(std::string const &url) const {
    std::string prefix = this->build_prefix();
    return url.substr(prefix.size(), url.size() - prefix.size());
  }

  std::string SDFileServer::build_absolute_path(std::string relative_path) const {
    if (relative_path.size() == 0)
      return this->root_path_;

    std::string absolute = Path::join(this->root_path_, relative_path);
    return absolute;
  }

}  // namespace sd_file_server
}  // namespace esphome