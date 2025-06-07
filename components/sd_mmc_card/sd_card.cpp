#include "sd_card.h"

#include "esphome/core/log.h"

#include <algorithm>

namespace esphome {
namespace sd_mmc_card {

static const char *TAG = "sd_card";

FileInfo::FileInfo(std::string const &path, size_t size, bool is_directory)
    : path(path), size(size), is_directory(is_directory) {}

void SdCard::write_file(const char *path, const uint8_t *buffer, size_t len) {
  ESP_LOGV(TAG, "Writing to file: %s", path);
  this->write_file(path, buffer, len, "w");
}

void SdCard::append_file(const char *path, const uint8_t *buffer, size_t len) {
  ESP_LOGV(TAG, "Appending to file: %s", path);
  this->write_file(path, buffer, len, "a");
}

std::vector<std::string> SdCard::list_directory(const char *path, uint8_t depth) {
  std::vector<std::string> list;
  std::vector<FileInfo> infos = list_directory_file_info(path, depth);
  std::transform(infos.cbegin(), infos.cend(), list.begin(), [](FileInfo const &info) { return info.path; });
  return list;
}

std::vector<std::string> SdCard::list_directory(std::string path, uint8_t depth) {
  return this->list_directory(path.c_str(), depth);
}

std::vector<FileInfo> SdCard::list_directory_file_info(std::string path, uint8_t depth) {
  return this->list_directory_file_info(path.c_str(), depth);
}

std::vector<FileInfo> SdCard::list_directory_file_info(const char *path, uint8_t depth) {
  std::vector<FileInfo> list;
  this->list_directory_file_info_rec(path, depth, list);
  return list;
}

size_t SdCard::file_size(std::string const &path) { return this->file_size(path.c_str()); }

bool SdCard::is_directory(std::string const &path) { return this->is_directory(path.c_str()); }

bool SdCard::delete_file(std::string const &path) { return this->delete_file(path.c_str()); }

std::vector<uint8_t> SdCard::read_file(std::string const &path) { return this->read_file(path.c_str()); }

}  // namespace sd_mmc_card
}  // namespace esphome