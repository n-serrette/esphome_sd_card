#pragma once

#include "esphome/core/component.h"

namespace esphome {
namespace esp32_camera_sd_card {

class Esp32CameraSDCardComponent : public Component {
public:
  void setup() override;
  void loop() override;
  void dump_config() override;
};

}  // namespace esp32_camera_sd_card
}  // namespace esphome