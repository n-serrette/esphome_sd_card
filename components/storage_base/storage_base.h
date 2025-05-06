#pragma once
#include <vector>
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif

namespace esphome {
namespace storage_base {

#define LOG_STORAGE_SENSORS(prefix, obj) \
  if ((obj) != nullptr) { \
    LOG_SENSOR(prefix, "Used space", obj->used_space_sensor_); \
    LOG_SENSOR(prefix, "Total space", obj->total_space_sensor_); \
    LOG_SENSOR(prefix, "Free space", obj->free_space_sensor_); \
    for (auto &sensor : obj->file_size_sensors_) { \
      if (sensor.sensor != nullptr) \
        LOG_SENSOR(prefix, "File size", sensor.sensor); \
    } \
  }

#define LOG_STORAGE_TEXT_SENSORS(prefix, obj) \
  if ((obj) != nullptr) { \
    LOG_TEXT_SENSOR(prefix, "Storage Type", obj->storage_type_text_sensor_); \
  }

enum MemoryUnits : short { Byte = 0, KiloByte = 1, MegaByte = 2, GigaByte = 3, TeraByte = 4, PetaByte = 5 };

struct FileInfo {
  std::string path;
  size_t size;
  bool is_directory;

  FileInfo(std::string const &, size_t, bool);
};

#ifdef USE_SENSOR
struct FileSizeSensor {
  sensor::Sensor *sensor{nullptr};
  std::string path;

  FileSizeSensor() = default;
  FileSizeSensor(sensor::Sensor *, std::string const &path);
};
#endif

class StorageBase : public Component {
#ifdef USE_SENSOR
  SUB_SENSOR(used_space)
  SUB_SENSOR(total_space)
  SUB_SENSOR(free_space)
#endif
#ifdef USE_TEXT_SENSOR
  SUB_TEXT_SENSOR(storage_type)
#endif
 public:
  virtual void write_file(const char *path, const uint8_t *buffer, size_t len, const char *mode) = 0;
  virtual void write_file(const char *path, const uint8_t *buffer, size_t len);
  virtual void append_file(const char *path, const uint8_t *buffer, size_t len);
  virtual bool delete_file(const char *path) = 0;
  bool delete_file(std::string const &path);
  virtual bool create_directory(const char *path) = 0;
  bool create_directory(std::string const &path);
  virtual bool remove_directory(const char *path) = 0;
  bool remove_directory(std::string const &path);
  virtual std::vector<uint8_t> read_file(char const *path) = 0;
  std::vector<uint8_t> read_file(std::string const &path);
  virtual bool is_directory(const char *path) = 0;
  bool is_directory(std::string const &path);
  std::vector<std::string> list_directory(const char *path, uint8_t depth);
  std::vector<std::string> list_directory(std::string path, uint8_t depth);
  virtual std::vector<FileInfo> list_directory_file_info(const char *path, uint8_t depth) = 0;
  std::vector<FileInfo> list_directory_file_info(std::string path, uint8_t depth);
  virtual size_t file_size(const char *path) = 0;
  size_t file_size(std::string const &path);

#ifdef USE_SENSOR
  void add_file_size_sensor(sensor::Sensor *, std::string const &path);
#endif

 protected:
#ifdef USE_SENSOR
  std::vector<FileSizeSensor> file_size_sensors_{};
#endif
  virtual void update_sensors() = 0;
};

long double convertBytes(uint64_t, MemoryUnits);
std::string memory_unit_to_string(MemoryUnits);
MemoryUnits memory_unit_from_size(size_t);
std::string format_size(size_t);
}  // namespace storage_base
}  // namespace esphome