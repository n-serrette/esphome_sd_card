#include "esp32_camera_sd_card.h"

#include "math.h"
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
  update_sensors();
}

void Esp32CameraSDCardComponent::loop() {}

void Esp32CameraSDCardComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Esp32 Camera SD Card Component");
  #ifdef USE_SENSOR
  LOG_SENSOR("  ", "Used space", this->used_space_sensor_);
  LOG_SENSOR("  ", "Total space", this->total_space_sensor_);
  #endif
}

void Esp32CameraSDCardComponent::write_file(const char *path, const uint8_t *buffer, size_t len) {
  ESP_LOGV(TAG, "Writing file: %s\n", path);

  File file = SD_MMC.open(path, FILE_WRITE);
  if (!file) {
    ESP_LOGE(TAG, "Failed to open file for writing");
    return;
  }

  file.write(buffer, len);
  file.close();
  this->update_sensors();
}

void Esp32CameraSDCardComponent::append_file(const char *path, const uint8_t *buffer, size_t len) {
  ESP_LOGV(TAG, "Appending to file: %s", path);

  File file = SD_MMC.open(path, FILE_APPEND);
  if(!file){
      ESP_LOGE(TAG, "Failed to open file for appending");
      return;
  }
  file.write(buffer, len);
  file.close();
  this->update_sensors();
}

void Esp32CameraSDCardComponent::update_sensors() {
#ifdef USE_SENSOR
  if(this->used_space_sensor_ != nullptr)
    this->used_space_sensor_->publish_state(SD_MMC.usedBytes());
  if(this->total_space_sensor_ != nullptr)
    this->total_space_sensor_->publish_state(SD_MMC.totalBytes());
#endif
}

long double convertBytes(uint64_t value, MemoryUnits unit) {
  return value * 1.0 / pow(1024, static_cast<uint64_t>(unit));
}

}  // namespace esp32_camera_sd_card
}  // namespace esphome