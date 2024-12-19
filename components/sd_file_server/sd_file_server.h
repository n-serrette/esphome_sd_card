#pragma once
#include "esphome/core/component.h"
#include "esphome/components/web_server_base/web_server_base.h"
#include "../sd_mmc_card/sd_mmc_card.h"

namespace esphome {
namespace sd_file_server {

class SDFileServer : public Component, public AsyncWebHandler {
 public:
  SDFileServer(web_server_base::WebServerBase *);
  void setup() override;
  void dump_config() override;
  bool canHandle(AsyncWebServerRequest *request) override;
  void handleRequest(AsyncWebServerRequest *request) override;

  void set_url_prefix(std::string const &);
  void set_root_path(std::string const &);
  void set_sd_mmc_card(sd_mmc_card::SdMmc *);

 protected:
  web_server_base::WebServerBase *base_;
  sd_mmc_card::SdMmc *sd_mmc_card_;

  std::string url_prefix_;
  std::string root_path_;

  std::string build_prefix() const;
  std::string extract_path_from_url(std::string const &) const;
  std::string build_absolute_path(std::string) const;
  void handle_index(AsyncWebServerRequest *);
};

}  // namespace sd_file_server
}  // namespace esphome