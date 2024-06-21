#pragma once
#include "esphome/core/defines.h"
#include "esphome/core/component.h"
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
protected:
  void update_sensors();
};
long double convertBytes(uint64_t, MemoryUnits);

}  // namespace esp32_camera_sd_card
}  // namespace esphome