#include "esp32_camera_sd_card.h"

#include "esphome/core/log.h"

#include "FS.h"
#include "SD_MMC.h"

namespace esphome {
namespace esp32_camera_sd_card {

static const char *TAG = "esp32_camera_sd_card";

void Esp32CameraSDCardComponent::setup() {
  if (!SD_MMC.begin()) {
    ESP_LOGE(TAG, "Card Mount Failed");
    mark_failed();
    return;
  }
}

void Esp32CameraSDCardComponent::loop() {}

void Esp32CameraSDCardComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Esp32 Camera SD Card Component");
}

}  // namespace esp32_camera_sd_card
}  // namespace esphome