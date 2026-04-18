#pragma once
#include "esphome/core/component.h"
#include "esphome/components/web_server_base/web_server_base.h"
#include "../sd_card/sd_card.h"
#include <cstdio>
#include <string>
#include <vector>

namespace esphome {
namespace webserver_sd {

class SDFileServer : public Component, public AsyncWebHandler {
 public:
  explicit SDFileServer(web_server_base::WebServerBase *base);
  void setup() override;
  void dump_config() override;
  bool canHandle(AsyncWebServerRequest *request) const override;
  void handleRequest(AsyncWebServerRequest *request) override;
  bool isRequestHandlerTrivial() const override { return true; }

  void set_url_prefix(const std::string &prefix);
  void set_sd_path(const std::string &path);
  void set_sd_card(sd_card::SdCard *card);
  void set_deletion_enabled(bool allow);
  void set_download_enabled(bool allow);
  void set_upload_enabled(bool allow);

 protected:
  web_server_base::WebServerBase *base_ = nullptr;
  sd_card::SdCard *sd_card_ = nullptr;

  std::string url_prefix_;
  std::string sd_path_;
  bool deletion_enabled_ = false;
  bool download_enabled_ = false;
  bool upload_enabled_ = false;

  std::string build_prefix() const;
  std::string extract_path_from_url(const std::string &url) const;
  std::string build_absolute_path(std::string relative_path) const;
  void handle_index(AsyncWebServerRequest *request, const std::string &path) const;
  void handle_get(AsyncWebServerRequest *request) const;
  void handle_delete(AsyncWebServerRequest *request);
  void handle_upload(AsyncWebServerRequest *request);
  void handle_download(AsyncWebServerRequest *request, const std::string &path) const;
  void handle_download_stream(AsyncWebServerRequest *request, const std::string &path, const std::string &mime, size_t file_size) const;
};

struct Path {
  static constexpr char separator = '/';

  static std::string file_name(const std::string &path);
  static bool is_absolute(const std::string &path);
  static bool trailing_slash(const std::string &path);
  static std::string join(const std::string &first, const std::string &second);
  static std::string remove_root_path(std::string path, const std::string &root);
  static std::vector<std::string> split_path(std::string path);
  static std::string extension(const std::string &file);
  static std::string file_type(const std::string &file);
  static std::string mime_type(const std::string &file);
};

}  // namespace sd_file_server
}  // namespace esphome
