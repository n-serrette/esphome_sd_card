#include "storage_base.h"

#include "esphome/core/log.h"

namespace esphome {
namespace storage_base {

static const char *TAG = "storage_base";

FileInfo::FileInfo(std::string const &path, size_t size, bool is_directory)
    : path(path), size(size), is_directory(is_directory) {}

void StorageBase::write_file(const char *path, const uint8_t *buffer, size_t len) {
  ESP_LOGV(TAG, "Writing to file: %s", path);
  this->write_file(path, buffer, len, "w");
}

void StorageBase::append_file(const char *path, const uint8_t *buffer, size_t len) {
  ESP_LOGV(TAG, "Appending to file: %s", path);
  this->write_file(path, buffer, len, "a");
}

bool StorageBase::delete_file(std::string const &path) { return this->delete_file(path.c_str()); }

bool StorageBase::create_directory(std::string const &path) { return this->create_directory(path.c_str()); }

bool StorageBase::remove_directory(std::string const &path) { return this->remove_directory(path.c_str()); }

std::vector<uint8_t> StorageBase::read_file(std::string const &path) { return this->read_file(path.c_str()); }

bool StorageBase::is_directory(std::string const &path) { return this->is_directory(path.c_str()); }

std::vector<std::string> StorageBase::list_directory(const char *path, uint8_t depth) {
  std::vector<std::string> list;
  std::vector<FileInfo> infos = this->list_directory_file_info(path, depth);
  std::transform(infos.cbegin(), infos.cend(), list.begin(), [](FileInfo const &info) { return info.path; });
  return list;
}

std::vector<std::string> StorageBase::list_directory(std::string path, uint8_t depth) {
  return this->list_directory(path.c_str(), depth);
}

std::vector<FileInfo> StorageBase::list_directory_file_info(std::string path, uint8_t depth) {
  return this->list_directory_file_info(path.c_str(), depth);
}

size_t StorageBase::file_size(std::string const &path) { return this->file_size(path.c_str()); }

}  // namespace storage_base
}  // namespace esphome