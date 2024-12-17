#include "sd_file_server.h"
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
  ESP_LOGCONFIG(TAG, "  Url Prefix: %s", this->url_prefix_);
}

bool SDFileServer::canHandle(AsyncWebServerRequest *request) {
  return str_startswith(std::string(request->url().c_str()), this->url_prefix_);
}

void SDFileServer::handleRequest(AsyncWebServerRequest *request) {
  ESP_LOGD(TAG, request->url().c_str());
  if (str_startswith(std::string(request->url().c_str()), this->url_prefix_)) {
    auto *response = request->beginResponse(200, "text/plain", "Hello World!");
    request->send(response);
    return;
  }
}

void SDFileServer::set_url_prefix(std::string const &prefix) { this->url_prefix_ = prefix; }

}  // namespace sd_file_server
}  // namespace esphome