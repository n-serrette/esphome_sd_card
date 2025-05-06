#include "storage_base.h"

#include "esphome/core/log.h"

namespace esphome {
namespace storage_base {

static const char *TAG = "storage_base";

#ifdef USE_SENSOR
FileSizeSensor::FileSizeSensor(sensor::Sensor *sensor, std::string const &path) : sensor(sensor), path(path) {}

void StorageBase::add_file_size_sensor(sensor::Sensor *sensor, std::string const &path) {
  this->file_size_sensors_.emplace_back(sensor, path);
}
#endif

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

long double convertBytes(uint64_t value, MemoryUnits unit) {
  return value * 1.0 / pow(1024, static_cast<uint64_t>(unit));
}

std::string memory_unit_to_string(MemoryUnits unit) {
  switch (unit) {
    case MemoryUnits::Byte:
      return "B";
    case MemoryUnits::KiloByte:
      return "KB";
    case MemoryUnits::MegaByte:
      return "MB";
    case MemoryUnits::GigaByte:
      return "GB";
    case MemoryUnits::TeraByte:
      return "TB";
    case MemoryUnits::PetaByte:
      return "PB";
  }
  return "unknown";
}

MemoryUnits memory_unit_from_size(size_t size) {
  short unit = MemoryUnits::Byte;
  double s = static_cast<double>(size);
  while (s >= 1024 && unit < MemoryUnits::PetaByte) {
    s /= 1024;
    unit++;
  }
  return static_cast<MemoryUnits>(unit);
}

std::string format_size(size_t size) {
  MemoryUnits unit = memory_unit_from_size(size);
  char buffer[32];
  snprintf(buffer, sizeof(buffer), "%.2f %s", convertBytes(size, unit), memory_unit_to_string(unit).c_str());
  return std::string(buffer);
}

}  // namespace storage_base
}  // namespace esphome