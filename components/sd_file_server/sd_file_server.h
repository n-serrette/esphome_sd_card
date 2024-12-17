#pragma once
#include "esphome/core/component.h"
#include "esphome/components/web_server_base/web_server_base.h"

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

 protected:
  web_server_base::WebServerBase *base_;
  std::string url_prefix_;
};

}  // namespace sd_file_server
}  // namespace esphome