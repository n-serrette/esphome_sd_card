#include "sd_mmc_card.h"

#ifdef USE_ESP_IDF
#include "math.h"
#include "esphome/core/log.h"
#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
#include "driver/sdmmc_types.h"

int constexpr SD_OCR_SDHC_CAP = (1 << 30);  // value defined in esp-idf/components/sdmmc/include/sd_protocol_defs.h

namespace esphome {
namespace sd_mmc_card {

static constexpr size_t FILE_PATH_MAX = ESP_VFS_PATH_MAX + CONFIG_SPIFFS_OBJ_NAME_LEN;
static const char *TAG = "sd_mmc_card";
static const std::string MOUNT_POINT("/sdcard");

void SdMmc::setup() {
  esp_vfs_fat_sdmmc_mount_config_t mount_config = {
      .format_if_mount_failed = false, .max_files = 5, .allocation_unit_size = 16 * 1024};

  sdmmc_host_t host = SDMMC_HOST_DEFAULT();
  sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();

  if (this->mode_1bit_) {
    slot_config.width = 1;
  } else {
    slot_config.width = 4;
  }

#ifdef SOC_SDMMC_USE_GPIO_MATRIX
  slot_config.clk = static_cast<gpio_num_t>(this->clk_pin_);
  slot_config.cmd = static_cast<gpio_num_t>(this->cmd_pin_);
  slot_config.d0 = static_cast<gpio_num_t>(this->data0_pin_);

  if (!this->mode_1bit_) {
    slot_config.d1 = static_cast<gpio_num_t>(this->data1_pin_);
    slot_config.d2 = static_cast<gpio_num_t>(this->data2_pin_);
    slot_config.d3 = static_cast<gpio_num_t>(this->data3_pin_);
  }
#endif

  // Enable internal pullups on enabled pins. The internal pullups
  // are insufficient however, please make sure 10k external pullups are
  // connected on the bus. This is for debug / example purpose only.
  slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

  auto ret = esp_vfs_fat_sdmmc_mount(MOUNT_POINT.c_str(), &host, &slot_config, &mount_config, &this->card_);

  if (ret != ESP_OK) {
    if (ret == ESP_FAIL) {
      this->init_error_ = ErrorCode::ERR_MOUNT;
    } else {
      this->init_error_ = ErrorCode::ERR_NO_CARD;
    }
    mark_failed();
    return;
  }

#ifdef USE_TEXT_SENSOR
  if (this->sd_card_type_text_sensor_ != nullptr)
    this->sd_card_type_text_sensor_->publish_state(sd_card_type());
#endif

  update_sensors();
}

void SdMmc::write_file(const char *path, const uint8_t *buffer, size_t len) {
  ESP_LOGV(TAG, "Writing file: %s\n", path);

  this->update_sensors();
}

void SdMmc::append_file(const char *path, const uint8_t *buffer, size_t len) {
  ESP_LOGV(TAG, "Appending to file: %s", path);

  this->update_sensors();
}

bool SdMmc::create_directory(const char *path) {
  ESP_LOGV(TAG, "Create directory: %s", path);

  this->update_sensors();
  return true;
}

bool SdMmc::remove_directory(const char *path) {
  ESP_LOGV(TAG, "Remove directory: %s", path);

  this->update_sensors();
  return true;
}

bool SdMmc::delete_file(const char *path) {
  ESP_LOGV(TAG, "Delete File: %s", path);

  this->update_sensors();
  return true;
}

std::vector<uint8_t> SdMmc::read_file(char const *path) {
  ESP_LOGV(TAG, "Read File: %s", path);

  return std::vector<uint8_t>();
}

std::vector<std::string> &SdMmc::list_directory_rec(const char *path, uint8_t depth, std::vector<std::string> &list) {
  return list;
}

std::vector<FileInfo> &SdMmc::list_directory_file_info_rec(const char *path, uint8_t depth,
                                                           std::vector<FileInfo> &list) {
  return list;
}

bool SdMmc::is_directory(const char *path) {
  return false;
}

size_t SdMmc::file_size(const char *path) { return -1; }

std::string SdMmc::sd_card_type() const {
  if (this->card_->is_sdio) {
    return "SDIO";
  } else if (this->card_->is_mmc) {
    return "MMC";
  } else {
    return (this->card_->ocr & SD_OCR_SDHC_CAP) ? "SDHC/SDXC" : "SDSC";
  }
  return "UNKNOWN";
}

void SdMmc::update_sensors() {
#ifdef USE_SENSOR
  if (this->card_ == nullptr)
    return;
  if (this->used_space_sensor_ != nullptr)
    this->used_space_sensor_->publish_state(-1);
  if (this->total_space_sensor_ != nullptr)
    this->total_space_sensor_->publish_state(static_cast<uint64_t>(this->card_->csd.capacity) *
                                             this->card_->csd.sector_size);

  for (auto &sensor : this->file_size_sensors_) {
    if (sensor.sensor != nullptr)
      sensor.sensor->publish_state(this->file_size(sensor.path));
  }
#endif
}

}  // namespace sd_mmc_card
}  // namespace esphome

#endif  // USE_ESP_IDF