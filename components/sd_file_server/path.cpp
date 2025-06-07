#include "path.h"

#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

#include <algorithm>
#include <map>

namespace esphome {
namespace sd_file_server {

static const char *TAG = "sd_file_server_path";

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
