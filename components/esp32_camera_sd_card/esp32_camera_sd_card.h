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

namespace esphome {
namespace esp32_camera_sd_card {

enum MemoryUnits : short { Byte = 0, KiloByte = 1, MegaByte = 2, GigaByte = 3, TeraByte = 4, PetaByte = 5 };

#ifdef USE_SENSOR
struct FileSizeSensor {
  sensor::Sensor *sensor{nullptr};
  std::string path;

  FileSizeSensor() = default;
  FileSizeSensor(sensor::Sensor *, std::string const &path);
};
#endif

class Esp32CameraSDCardComponent : public Component {
#ifdef USE_SENSOR
  SUB_SENSOR(used_space)
  SUB_SENSOR(total_space)
#endif
#ifdef USE_TEXT_SENSOR
  SUB_TEXT_SENSOR(sd_card_type)
#endif
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  void write_file(const char *path, const uint8_t *buffer, size_t len);
  void append_file(const char *path, const uint8_t *buffer, size_t len);
  bool delete_file(const char *path);
  bool create_directory(const char *path);
  bool remove_directory(const char *path);
  std::vector<std::string> list_directory(const char *path, uint8_t depth);
  size_t file_size(const char *path);
  size_t file_size(std::string const &path);
#ifdef USE_SENSOR
  void add_file_size_sensor(sensor::Sensor *, std::string const &path);
#endif

  void set_clk_pin(GPIOPin *);
  void set_cmd_pin(GPIOPin *);
  void set_data0_pin(GPIOPin *);
  void set_data1_pin(GPIOPin *);
  void set_data2_pin(GPIOPin *);
  void set_data3_pin(GPIOPin *);

 protected:
  GPIOPin *clk_pin_;
  GPIOPin *cmd_pin_;
  GPIOPin *data0_pin_;
  GPIOPin *data1_pin_;
  GPIOPin *data2_pin_;
  GPIOPin *data3_pin_;
#ifdef USE_SENSOR
  std::vector<FileSizeSensor> file_size_sensors_{};
#endif
  void update_sensors();
  std::string sd_card_type_to_string(int) const;
  std::vector<std::string> &list_directory_rec(const char *path, uint8_t depth, std::vector<std::string> &list);
};

template<typename... Ts> class SDCardWriteFileAction : public Action<Ts...> {
 public:
  SDCardWriteFileAction(Esp32CameraSDCardComponent *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(std::string, path)
  TEMPLATABLE_VALUE(std::vector<uint8_t>, data)

  void play(Ts... x) {
    auto path = this->path_.value(x...);
    auto buffer = this->data_.value(x...);
    this->parent_->write_file(path.c_str(), buffer.data(), buffer.size());
  }

 protected:
  Esp32CameraSDCardComponent *parent_;
};

template<typename... Ts> class SDCardAppendFileAction : public Action<Ts...> {
 public:
  SDCardAppendFileAction(Esp32CameraSDCardComponent *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(std::string, path)
  TEMPLATABLE_VALUE(std::vector<uint8_t>, data)

  void play(Ts... x) {
    auto path = this->path_.value(x...);
    auto buffer = this->data_.value(x...);
    this->parent_->append_file(path.c_str(), buffer.data(), buffer.size());
  }

 protected:
  Esp32CameraSDCardComponent *parent_;
};

template<typename... Ts> class SDCardCreateDirectoryAction : public Action<Ts...> {
 public:
  SDCardCreateDirectoryAction(Esp32CameraSDCardComponent *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(std::string, path)

  void play(Ts... x) {
    auto path = this->path_.value(x...);
    this->parent_->create_directory(path.c_str());
  }

 protected:
  Esp32CameraSDCardComponent *parent_;
};

template<typename... Ts> class SDCardRemoveDirectoryAction : public Action<Ts...> {
 public:
  SDCardRemoveDirectoryAction(Esp32CameraSDCardComponent *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(std::string, path)

  void play(Ts... x) {
    auto path = this->path_.value(x...);
    this->parent_->remove_directory(path.c_str());
  }

 protected:
  Esp32CameraSDCardComponent *parent_;
};

template<typename... Ts> class SDCardDeleteFileAction : public Action<Ts...> {
 public:
  SDCardDeleteFileAction(Esp32CameraSDCardComponent *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(std::string, path)

  void play(Ts... x) {
    auto path = this->path_.value(x...);
    this->parent_->delete_file(path.c_str());
  }

 protected:
  Esp32CameraSDCardComponent *parent_;
};

struct Utility {
  static int get_pin_no(GPIOPin *);
};

long double convertBytes(uint64_t, MemoryUnits);

}  // namespace esp32_camera_sd_card
}  // namespace esphome