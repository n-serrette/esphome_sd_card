#include "sd_file_server.h"
#include <map>
#include "esphome/core/log.h"
#include "esphome/components/network/util.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace sd_file_server {

static const char *TAG = "sd_file_server";

SDFileServer::SDFileServer(web_server_base::WebServerBase *base) : base_(base) {}

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
    if (request->method() == HTTP_GET) {
      this->handle_get(request);
      return;
    }
    if (request->method() == HTTP_DELETE) {
      this->handle_delete(request);
      return;
    }
  }
}

void SDFileServer::handleUpload(AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data,
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

void SDFileServer::set_sd_mmc_card(sd_mmc_card::SdCard *card) { this->sd_mmc_card_ = card; }

void SDFileServer::set_deletion_enabled(bool allow) { this->deletion_enabled_ = allow; }

void SDFileServer::set_download_enabled(bool allow) { this->download_enabled_ = allow; }

void SDFileServer::set_upload_enabled(bool allow) { this->upload_enabled_ = allow; }

void SDFileServer::handle_get(AsyncWebServerRequest *request) const {
  std::string extracted = this->extract_path_from_url(std::string(request->url().c_str()));
  std::string path = this->build_absolute_path(extracted);

  if (!this->sd_mmc_card_->is_directory(path)) {
    handle_download(request, path);
    return;
  }

  handle_index(request, path);
}

void SDFileServer::write_row(AsyncResponseStream *response, sd_mmc_card::FileInfo const &info) const {
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

void SDFileServer::handle_index(AsyncWebServerRequest *request, std::string const &path) const {
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

void SDFileServer::handle_download(AsyncWebServerRequest *request, std::string const &path) const {
  if (!this->download_enabled_) {
    request->send(401, "application/json", "{ \"error\": \"file download is disabled\" }");
    return;
  }

  auto file = this->sd_mmc_card_->read_file(path);
  if (file.size() == 0) {
    request->send(401, "application/json", "{ \"error\": \"failed to read file\" }");
    return;
  }
#ifdef USE_ESP_IDF
  auto *response = request->beginResponse_P(200, Path::mime_type(path).c_str(), file.data(), file.size());
#else
  auto *response = request->beginResponseStream(Path::mime_type(path).c_str(), file.size());
  response->write(file.data(), file.size());
#endif

  request->send(response);
}

void SDFileServer::handle_delete(AsyncWebServerRequest *request) {
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

std::string Path::file_name(std::string const &path) {
  size_t pos = path.rfind(Path::separator);
  if (pos != std::string::npos) {
    return path.substr(pos + 1);
  }
  return "";
}

bool Path::is_absolute(std::string const &path) { return path.size() && path[0] == separator; }

bool Path::trailing_slash(std::string const &path) { return path.size() && path[path.length() - 1] == separator; }

std::string Path::join(std::string const &first, std::string const &second) {
  std::string result = first;
  if (!trailing_slash(first) && !is_absolute(second)) {
    result.push_back(separator);
  }
  if (trailing_slash(first) && is_absolute(second)) {
    result.pop_back();
  }
  result.append(second);
  return result;
}

std::string Path::remove_root_path(std::string path, std::string const &root) {
  if (!str_startswith(path, root))
    return path;
  if (path.size() == root.size() || path.size() < 2)
    return "/";
  return path.erase(0, root.size());
}

std::vector<std::string> Path::split_path(std::string path) {
  std::vector<std::string> parts;
  size_t pos = 0;
  while ((pos = path.find('/')) != std::string::npos) {
    std::string part = path.substr(0, pos);
    if (!part.empty()) {
      parts.push_back(part);
    }
    path.erase(0, pos + 1);
  }
  parts.push_back(path);
  return parts;
}

std::string Path::extension(std::string const &file) {
  size_t pos = file.find_last_of('.');
  if (pos == std::string::npos)
    return "";
  return file.substr(pos + 1);
}

std::string Path::file_type(std::string const &file) {
  static const std::map<std::string, std::string> file_types = {
      {"mp3", "Audio (MP3)"},   {"wav", "Audio (WAV)"}, {"png", "Image (PNG)"},   {"jpg", "Image (JPG)"},
      {"jpeg", "Image (JPEG)"}, {"bmp", "Image (BMP)"}, {"txt", "Text (TXT)"},    {"log", "Text (LOG)"},
      {"csv", "Text (CSV)"},    {"html", "Web (HTML)"}, {"css", "Web (CSS)"},     {"js", "Web (JS)"},
      {"json", "Data (JSON)"},  {"xml", "Data (XML)"},  {"zip", "Archive (ZIP)"}, {"gz", "Archive (GZ)"},
      {"tar", "Archive (TAR)"}, {"mp4", "Video (MP4)"}, {"avi", "Video (AVI)"},   {"webm", "Video (WEBM)"}};

  std::string ext = Path::extension(file);
  if (ext.empty())
    return "File";

  std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });
  auto it = file_types.find(ext);
  if (it != file_types.end())
    return it->second;
  return "File (" + ext + ")";
}

std::string Path::mime_type(std::string const &file) {
  static const std::map<std::string, std::string> file_types = {
      {"mp3", "audio/mpeg"},        {"wav", "audio/vnd.wav"},   {"png", "image/png"},       {"jpg", "image/jpeg"},
      {"jpeg", "image/jpeg"},       {"bmp", "image/bmp"},       {"txt", "text/plain"},      {"log", "text/plain"},
      {"csv", "text/csv"},          {"html", "text/html"},      {"css", "text/css"},        {"js", "text/javascript"},
      {"json", "application/json"}, {"xml", "application/xml"}, {"zip", "application/zip"}, {"gz", "application/gzip"},
      {"tar", "application/x-tar"}, {"mp4", "video/mp4"},       {"avi", "video/x-msvideo"}, {"webm", "video/webm"}};

  std::string ext = Path::extension(file);
  ESP_LOGD(TAG, "ext : %s", ext.c_str());
  if (!ext.empty()) {
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });
    auto it = file_types.find(ext);
    if (it != file_types.end())
      return it->second;
  }
  return "application/octet-stream";
}

}  // namespace sd_file_server
}  // namespace esphome