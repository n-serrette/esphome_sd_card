#pragma once
#include "esphome/core/component.h"
#include "esphome/components/web_server_base/web_server_base.h"
#include "../sd_mmc_card/sd_card.h"
#include "../sd_mmc_card/memory_units.h"

namespace esphome {
namespace sd_file_server {

class SDFileServer : public Component, public AsyncWebHandler {
 public:
  SDFileServer(web_server_base::WebServerBase *);
  void setup() override;
  void dump_config() override;
  bool canHandle(AsyncWebServerRequest *request) override;
  void handleRequest(AsyncWebServerRequest *request) override;
  void handleUpload(AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len,
                    bool final) override;
  bool isRequestHandlerTrivial() override { return false; }

  void set_url_prefix(std::string const &);
  void set_root_path(std::string const &);
  void set_sd_mmc_card(sd_mmc_card::SdCard *);
  void set_deletion_enabled(bool);
  void set_download_enabled(bool);
  void set_upload_enabled(bool);

 protected:
  web_server_base::WebServerBase *base_;
  sd_mmc_card::SdCard *sd_mmc_card_;

  std::string url_prefix_;
  std::string root_path_;
  bool deletion_enabled_;
  bool download_enabled_;
  bool upload_enabled_;

  std::string build_prefix() const;
  std::string extract_path_from_url(std::string const &) const;
  std::string build_absolute_path(std::string) const;
  void write_row(AsyncResponseStream *response, sd_mmc_card::FileInfo const &info) const;
  void handle_index(AsyncWebServerRequest *, std::string const &) const;
  void handle_get(AsyncWebServerRequest *) const;
  void handle_delete(AsyncWebServerRequest *);
  void handle_download(AsyncWebServerRequest *, std::string const &) const;
};

struct Path {
  static constexpr char separator = '/';

  /* Return the name of the file */
  static std::string file_name(std::string const &);

  /* Is the path an absolute path? */
  static bool is_absolute(std::string const &);

  /* Does the path have a trailing slash? */
  static bool trailing_slash(std::string const &);

  /* Join two path */
  static std::string join(std::string const &, std::string const &);

  static std::string remove_root_path(std::string path, std::string const &root);

  static std::vector<std::string> split_path(std::string path);

  static std::string extension(std::string const &);

  static std::string file_type(std::string const &);

  static std::string mime_type(std::string const &);
};

}  // namespace sd_file_server
}  // namespace esphome