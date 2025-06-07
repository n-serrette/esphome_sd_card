#include "sd_mmc_card.h"

#ifdef SDMMC_USE_SDMMC
#ifdef USE_ESP32_FRAMEWORK_ARDUINO

#include "math.h"
#include "esphome/core/log.h"

#include "SD_MMC.h"
#include "FS.h"

namespace esphome {
namespace sd_mmc_card {

static const char *TAG = "sd_mmc_card_esp32_arduino";

void SdMmc::setup() {
  if (this->power_ctrl_pin_ != nullptr)
    this->power_ctrl_pin_->setup();

  bool setPinResult = this->mode_1bit_ ? SD_MMC.setPins(this->clk_pin_, this->cmd_pin_, this->data0_pin_)
                                       : SD_MMC.setPins(this->clk_pin_, this->cmd_pin_, this->data0_pin_,
                                                        this->data1_pin_, this->data2_pin_, this->data3_pin_);

  if (!setPinResult) {
    this->init_error_ = ErrorCode::ERR_PIN_SETUP;
    this->mark_failed();
    return;
  }

  bool beginResult = this->mode_1bit_ ? SD_MMC.begin("/sdcard", this->mode_1bit_) : SD_MMC.begin();
  if (!beginResult) {
    this->init_error_ = ErrorCode::ERR_MOUNT;
    this->mark_failed();
    return;
  }

  uint8_t cardType = SD_MMC.cardType();

#ifdef USE_TEXT_SENSOR
  if (this->sd_card_type_text_sensor_ != nullptr)
    this->sd_card_type_text_sensor_->publish_state(sd_card_type_to_string(cardType));
#endif

  if (cardType == CARD_NONE) {
    this->init_error_ = ErrorCode::ERR_NO_CARD;
    this->mark_failed();
    return;
  }

  update_sensors();
}

File SdMmc::open(const char *path, const char *mode) {
  return SD_MMC.open(path, mode);
}

void SdMmc::write_file(const char *path, const uint8_t *buffer, size_t len, const char *mode) {
  File file = this->open(path, mode);
  if (!file) {
    ESP_LOGE(TAG, "Failed to open file for writing");
    return;
  }

  file.write(buffer, len);
  file.close();
  this->update_sensors();
}

bool SdMmc::create_directory(const char *path) {
  ESP_LOGV(TAG, "Create directory: %s", path);
  if (!SD_MMC.mkdir(path)) {
    ESP_LOGE(TAG, "Failed to create directory");
    return false;
  }
  this->update_sensors();
  return true;
}

bool SdMmc::remove_directory(const char *path) {
  ESP_LOGV(TAG, "Remove directory: %s", path);
  if (!SD_MMC.rmdir(path)) {
    ESP_LOGE(TAG, "Failed to remove directory");
    return false;
  }
  this->update_sensors();
  return true;
}

bool SdMmc::delete_file(const char *path) {
  ESP_LOGV(TAG, "Delete File: %s", path);
  if (!SD_MMC.remove(path)) {
    ESP_LOGE(TAG, "failed to remove file");
    return false;
  }
  this->update_sensors();
  return true;
}

std::vector<uint8_t> SdMmc::read_file(char const *path) {
  ESP_LOGV(TAG, "Read File: %s", path);
  File file = SD_MMC.open(path);
  if (!file) {
    ESP_LOGE(TAG, "Failed to open file for reading");
    return std::vector<uint8_t>();
  }

  std::vector<uint8_t> res;
  res.reserve(file.size());
  while (file.available()) {
    res.push_back(file.read());
  }
  return res;
}

std::vector<FileInfo> &SdMmc::list_directory_file_info_rec(const char *path, uint8_t depth,
                                                           std::vector<FileInfo> &list) {
  ESP_LOGV(TAG, "Listing directory file info: %s\n", path);

  File root = SD_MMC.open(path);
  if (!root) {
    ESP_LOGE(TAG, "Failed to open directory");
    return list;
  }
  if (!root.isDirectory()) {
    ESP_LOGE(TAG, "Not a directory");
    return list;
  }

  File file = root.openNextFile();
  while (file) {
    list.emplace_back(file.path(), file.size(), file.isDirectory());
    if (file.isDirectory()) {
      if (depth) {
        this->list_directory_file_info_rec(file.path(), depth - 1, list);
      }
    }
    file = root.openNextFile();
  }
  return list;
}

bool SdMmc::is_directory(const char *path) {
  File root = SD_MMC.open(path);
  if (!root) {
    ESP_LOGE(TAG, "Failed to open directory");
    return false;
  }
  return root.isDirectory();
}

size_t SdMmc::file_size(const char *path) {
  File file = SD_MMC.open(path);
  return file.size();
}

std::string SdMmc::sd_card_type_to_string(int type) const {
  switch (type) {
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

void SdMmc::update_sensors() {
#ifdef USE_SENSOR
  uint64_t used_bytes = SD_MMC.usedBytes();
  uint64_t total_bytes = SD_MMC.totalBytes();
  if (this->used_space_sensor_ != nullptr)
    this->used_space_sensor_->publish_state(used_bytes);
  if (this->total_space_sensor_ != nullptr)
    this->total_space_sensor_->publish_state(total_bytes);
  if (this->free_space_sensor_ != nullptr)
    this->free_space_sensor_->publish_state(total_bytes - used_bytes);

  for (auto &sensor : this->file_size_sensors_) {
    if (sensor.sensor != nullptr)
      sensor.sensor->publish_state(this->file_size(sensor.path));
  }
#endif
}

}  // namespace sd_mmc_card
}  // namespace esphome

#endif  // USE_ESP32_FRAMEWORK_ARDUINO
#endif // SDMMC_USE_SDMMC