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

  uint8_t cardType = SD_MMC.cardType();

#ifdef USE_TEXT_SENSOR
  if(this->sd_card_type_text_sensor_ != nullptr)
    this->sd_card_type_text_sensor_->publish_state(sd_card_type_to_string(cardType));
#endif

  if(cardType == CARD_NONE){
      ESP_LOGE(TAG, "No SD_MMC card attached");
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
  #ifdef USE_TEXT_SENSOR
  LOG_TEXT_SENSOR("  ", "SD Card Type", this->sd_card_type_text_sensor_);
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


bool Esp32CameraSDCardComponent::create_directory(const char *path) {
  ESP_LOGV(TAG, "Create directory: %s", path);
  if (!SD_MMC.mkdir(path)) {
    ESP_LOGE(TAG, "Failed to create directory");
    return false;
  }
  return true;
}

bool Esp32CameraSDCardComponent::remove_directory(const char *path) {
  ESP_LOGV(TAG, "Remove directory: %s", path);
  if (!SD_MMC.rmdir(path)) {
    ESP_LOGE(TAG, "Failed to remove directory");
    return false;
  }
  return true;
}

bool Esp32CameraSDCardComponent::delete_file(const char *path) {
  ESP_LOGV(TAG, "Delete File: %s", path);
  if (!SD_MMC.remove(path)) {
    ESP_LOGE(TAG, "failed to remove file");
    return false;
  }
  return true;
}

std::string Esp32CameraSDCardComponent::sd_card_type_to_string(int type) const {
  switch(type) {
    case CARD_NONE:
      return "NONE";
    case CARD_MMC:
      return "MMC";
    case CARD_SD:
      return "SDSC";
    case CARD_SDHC:
      return "SDHC";
    case CARD_UNKNOWN:
    default:
      return "UNKNOWN";
  }
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