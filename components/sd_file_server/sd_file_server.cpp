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
  ESP_LOGCONFIG(TAG, "  Root Path: %s", this->root_path_.c_str());
}

bool SDFileServer::canHandle(AsyncWebServerRequest *request) {
  ESP_LOGD(TAG, "can handle %s %u", request->url().c_str(),
           str_startswith(std::string(request->url().c_str()), this->build_prefix()));
  return str_startswith(std::string(request->url().c_str()), this->build_prefix());
}

void SDFileServer::handleRequest(AsyncWebServerRequest *request) {
  ESP_LOGD(TAG, "%s", request->url().c_str());
  if (str_startswith(std::string(request->url().c_str()), this->build_prefix())) {
    this->handle_index(request);
    return;
  }
}

void SDFileServer::set_url_prefix(std::string const &prefix) { this->url_prefix_ = prefix; }

void SDFileServer::set_root_path(std::string const &path) { this->root_path_ = path; }

void SDFileServer::set_sd_mmc_card(sd_mmc_card::SdMmc *card) { this->sd_mmc_card_ = card; }

void SDFileServer::handle_index(AsyncWebServerRequest *request) {
  auto *response = request->beginResponseStream("text/html");
  response->print("<h1>SD Card Content</h1>");

  std::string extracted = this->extract_path_from_url(std::string(request->url().c_str()));
  std::string path = this->build_absolute_path(extracted);

  if (!this->sd_mmc_card_->is_directory(path)) {
    response->print(path.c_str());
    response->print(" is not a folder");
    request->send(response);
    return;
  }
  response->print("<h2>Folder ");
  response->print(path.c_str());
  response->print("</h2><div>");
  auto entries = this->sd_mmc_card_->list_directory(path, 0);
  for (auto const &entry : entries) {
    response->print("<span>");
    response->print(entry.c_str());
    response->print("</span></br>");
  }
  response->print("</div>");
  request->send(response);
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

  bool root_end_with_slash = this->root_path_.size() > 0 && this->root_path_.at(this->root_path_.size() - 1) == '/';
  bool relative_path_start_with_slash = relative_path.at(0) == '/';

  if (root_end_with_slash && relative_path_start_with_slash) {
    relative_path.erase(0, 1);
    return this->root_path_ + relative_path;
  }

  if (!root_end_with_slash && !relative_path_start_with_slash)
    return this->root_path_ + "/" + relative_path;

  return this->root_path_ + relative_path;
}

}  // namespace sd_file_server
}  // namespace esphome