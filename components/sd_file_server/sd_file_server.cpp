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
  ESP_LOGCONFIG(TAG, "  Url Prefix: %s", this->url_prefix_.c_str());
}

bool SDFileServer::canHandle(AsyncWebServerRequest *request) {
  ESP_LOGD(TAG, "can handle %s %u", request->url().c_str(),
           str_startswith(std::string(request->url().c_str()), this->buildPrefix()));
  return str_startswith(std::string(request->url().c_str()), this->buildPrefix());
}

void SDFileServer::handleRequest(AsyncWebServerRequest *request) {
  ESP_LOGD(TAG, request->url().c_str());
  if (str_startswith(std::string(request->url().c_str()), this->buildPrefix())) {
    this->handleIndex(request);
    return;
  }
}

void SDFileServer::set_url_prefix(std::string const &prefix) { this->url_prefix_ = prefix; }

void SDFileServer::set_root_path(std::string const &path) { this->root_path_ = path; }

void SDFileServer::set_sd_mmc_card(sd_mmc_card::SdMmc *card) { this->sd_mmc_card_ = card; }

void SDFileServer::handleIndex(AsyncWebServerRequest *request) {
  auto *response = request->beginResponseStream("text/html");
  response->print("<h1>SD Card Content</h1>");
  auto entries = this->sd_mmc_card_->list_directory(this->root_path_.c_str(), 0);
  for (auto const &entry : entries) {
    response->print("<span>");
    response->print(entry.c_str());
    response->print("</span></br>");
  }
  request->send(response);
}

std::string SDFileServer::buildPrefix() const {
  if (this->url_prefix_.length() == 0 || this->url_prefix_.at(0) != '/')
    return "/" + this->url_prefix_;
  return this->url_prefix_;
}

}  // namespace sd_file_server
}  // namespace esphome