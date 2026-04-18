#pragma once
#include "esphome/core/gpio.h"
#include "esphome/core/defines.h"
#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif

#ifdef USE_ESP_IDF
#include "sdmmc_cmd.h"
#endif

namespace esphome {
namespace sd_card {

enum MemoryUnits : short { Byte = 0, KiloByte = 1, MegaByte = 2, GigaByte = 3, TeraByte = 4, PetaByte = 5 };

#ifdef USE_SENSOR
struct FileSizeSensor {
  sensor::Sensor *sensor{nullptr};
  std::string path;

  FileSizeSensor() = default;
  FileSizeSensor(sensor::Sensor *, std::string const &path);
};
#endif

struct FileInfo {
  std::string path;
  size_t size;
  bool is_directory;

  FileInfo(std::string const &, size_t, bool);
};

class SdCard : public Component {
#ifdef USE_SENSOR
  SUB_SENSOR(used_space)
  SUB_SENSOR(total_space)
  SUB_SENSOR(free_space)
#endif
#ifdef USE_TEXT_SENSOR
  SUB_TEXT_SENSOR(sd_card_type)
#endif
 public:
  enum ErrorCode {
    ERR_PIN_SETUP,
    ERR_MOUNT,
    ERR_NO_CARD,
  };
  void setup() override;
  void loop() override;
  void dump_config() override;
  void write_file(const char *path, const uint8_t *buffer, size_t len, const char *mode);
  void write_file(const char *path, const uint8_t *buffer, size_t len);
  void append_file(const char *path, const uint8_t *buffer, size_t len);
  bool delete_file(const char *path);
  bool delete_file(std::string const &path);
  bool create_directory(const char *path);
  bool remove_directory(const char *path);
  std::vector<uint8_t> read_file(char const *path);
  std::vector<uint8_t> read_file(std::string const &path);
  size_t read_file_chunk(const char *path, size_t offset, uint8_t *buffer, size_t buffer_size);
  using FileChunkCallback = std::function<bool(const uint8_t *data, size_t len)>;
  bool stream_file(const char *path, FileChunkCallback callback, size_t chunk_size = 4096);
  using FileInfoCallback = std::function<bool(const FileInfo &)>;
  // Streams directory entries one-by-one via callback; yields to FreeRTOS every 32 entries.
  // Must be called from a FreeRTOS task context (not the main ESPHome loop).
  void list_directory_file_info_stream(const char *path, uint8_t depth, FileInfoCallback callback);
  bool is_directory(const char *path);
  bool is_directory(std::string const &path);
  std::vector<std::string> list_directory(const char *path, uint8_t depth);
  std::vector<std::string> list_directory(std::string path, uint8_t depth);
  std::vector<FileInfo> list_directory_file_info(const char *path, uint8_t depth);
  std::vector<FileInfo> list_directory_file_info(std::string path, uint8_t depth);
  size_t file_size(const char *path);
  size_t file_size(std::string const &path);
#ifdef USE_SENSOR
  void add_file_size_sensor(sensor::Sensor *, std::string const &path);
#endif

  void set_clk_pin(uint8_t);
  void set_cmd_pin(uint8_t);
  void set_data0_pin(uint8_t);
  void set_data1_pin(uint8_t);
  void set_data2_pin(uint8_t);
  void set_data3_pin(uint8_t);
  void set_mode_1bit(bool);
  void set_power_ctrl_pin(GPIOPin *);

  std::string build_path(const std::string &path) const;
  std::string sd_card_type() const;
  void update_sensors();

 protected:
  ErrorCode init_error_;
  uint8_t clk_pin_;
  uint8_t cmd_pin_;
  uint8_t data0_pin_;
  uint8_t data1_pin_;
  uint8_t data2_pin_;
  uint8_t data3_pin_;
  bool mode_1bit_;
  GPIOPin *power_ctrl_pin_{nullptr};

  sdmmc_card_t *card_;
#ifdef USE_SENSOR
  std::vector<FileSizeSensor> file_size_sensors_{};
#endif
  uint32_t last_sensor_update_ms_{0};  // debounce guard for f_getfree cost
  std::vector<FileInfo> &list_directory_file_info_rec(const char *path, uint8_t depth, std::vector<FileInfo> &list);
  void list_directory_file_info_stream_rec(const char *path, uint8_t depth, FileInfoCallback &callback, uint32_t &count);
  static std::string error_code_to_string(ErrorCode);
};

template<typename... Ts> class SdCardWriteFileAction : public Action<Ts...> {
 public:
  SdCardWriteFileAction(SdCard *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(std::string, path)
  TEMPLATABLE_VALUE(std::vector<uint8_t>, data)

  void play(Ts... x) {
    auto path = this->path_.value(x...);
    auto buffer = this->data_.value(x...);
    this->parent_->write_file(path.c_str(), buffer.data(), buffer.size());
  }

 protected:
  SdCard *parent_;
};

template<typename... Ts> class SdCardAppendFileAction : public Action<Ts...> {
 public:
  SdCardAppendFileAction(SdCard *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(std::string, path)
  TEMPLATABLE_VALUE(std::vector<uint8_t>, data)

  void play(Ts... x) {
    auto path = this->path_.value(x...);
    auto buffer = this->data_.value(x...);
    this->parent_->append_file(path.c_str(), buffer.data(), buffer.size());
  }

 protected:
  SdCard *parent_;
};

template<typename... Ts> class SdCardCreateDirectoryAction : public Action<Ts...> {
 public:
  SdCardCreateDirectoryAction(SdCard *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(std::string, path)

  void play(Ts... x) {
    auto path = this->path_.value(x...);
    this->parent_->create_directory(path.c_str());
  }

 protected:
  SdCard *parent_;
};

template<typename... Ts> class SdCardRemoveDirectoryAction : public Action<Ts...> {
 public:
  SdCardRemoveDirectoryAction(SdCard *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(std::string, path)

  void play(Ts... x) {
    auto path = this->path_.value(x...);
    this->parent_->remove_directory(path.c_str());
  }

 protected:
  SdCard *parent_;
};

template<typename... Ts> class SdCardDeleteFileAction : public Action<Ts...> {
 public:
  SdCardDeleteFileAction(SdCard *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(std::string, path)

  void play(Ts... x) {
    auto path = this->path_.value(x...);
    this->parent_->delete_file(path.c_str());
  }

 protected:
  SdCard *parent_;
};

long double convertBytes(uint64_t, MemoryUnits);
std::string memory_unit_to_string(MemoryUnits);
MemoryUnits memory_unit_from_size(size_t);
std::string format_size(size_t);

}  // namespace sd_card
}  // namespace esphome
