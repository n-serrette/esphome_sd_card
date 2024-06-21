#pragma once
#include "esphome/core/defines.h"
#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif

namespace esphome {
namespace esp32_camera_sd_card {

enum MemoryUnits: short {
  Byte = 0,
  KiloByte = 1,
  MegaByte = 2,
  GigaByte = 3,
  TeraByte = 4,
  PetaByte = 5
};

class Esp32CameraSDCardComponent : public Component {
#ifdef USE_SENSOR
  SUB_SENSOR(used_space)
  SUB_SENSOR(total_space)
#endif
public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  void write_file(const char *path, const uint8_t *buffer, size_t len);
protected:
  void update_sensors();
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
long double convertBytes(uint64_t, MemoryUnits);

}  // namespace esp32_camera_sd_card
}  // namespace esphome