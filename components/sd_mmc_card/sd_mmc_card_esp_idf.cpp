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

std::string build_path(const char *path) { return MOUNT_POINT + path; }

void SdMmc::setup() {
  if (this->power_ctrl_pin_ != nullptr)
    this->power_ctrl_pin_->setup();

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

void SdMmc::write_file(const char *path, const uint8_t *buffer, size_t len, const char *mode) {
  std::string absolut_path = build_path(path);
  FILE *file = NULL;
  file = fopen(absolut_path.c_str(), mode);
  if (file == NULL) {
    ESP_LOGE(TAG, "Failed to open file for writing");
    return;
  }
  bool ok = fwrite(buffer, 1, len, file);
  if (!ok) {
    ESP_LOGE(TAG, "Failed to write to file");
  }
  fclose(file);
  this->update_sensors();
}

bool SdMmc::create_directory(const char *path) {
  ESP_LOGV(TAG, "Create directory: %s", path);
  std::string absolut_path = build_path(path);
  if (mkdir(absolut_path.c_str(), 0777) < 0) {
    ESP_LOGE(TAG, "Failed to create a new directory: %s", strerror(errno));
    return false;
  }
  this->update_sensors();
  return true;
}

bool SdMmc::remove_directory(const char *path) {
  ESP_LOGV(TAG, "Remove directory: %s", path);
  if (!this->is_directory(path)) {
    ESP_LOGE(TAG, "Not a directory");
    return false;
  }
  std::string absolut_path = build_path(path);
  if (remove(absolut_path.c_str()) != 0) {
    ESP_LOGE(TAG, "Failed to remove directory: %s", strerror(errno));
  }
  this->update_sensors();
  return true;
}

bool SdMmc::delete_file(const char *path) {
  ESP_LOGV(TAG, "Delete File: %s", path);
  if (this->is_directory(path)) {
    ESP_LOGE(TAG, "Not a file");
    return false;
  }
  std::string absolut_path = build_path(path);
  if (remove(absolut_path.c_str()) != 0) {
    ESP_LOGE(TAG, "Failed to remove file: %s", strerror(errno));
  }
  this->update_sensors();
  return true;
}

std::vector<uint8_t> SdMmc::read_file(char const *path) {
  ESP_LOGV(TAG, "Read File: %s", path);

  std::string absolut_path = build_path(path);
  FILE *file = nullptr;
  file = fopen(absolut_path.c_str(), "rb");
  if (file == nullptr) {
    ESP_LOGE(TAG, "Failed to open file for reading");
    return std::vector<uint8_t>();
  }

  std::vector<uint8_t> res;
  size_t fileSize = this->file_size(path);
  res.resize(fileSize);
  size_t len = fread(res.data(), 1, fileSize, file);
  fclose(file);
  if (len < 0) {
    ESP_LOGE(TAG, "Failed to read file: %s", strerror(errno));
    return std::vector<uint8_t>();
  }

  return res;
}

std::vector<FileInfo> &SdMmc::list_directory_file_info_rec(const char *path, uint8_t depth,
                                                           std::vector<FileInfo> &list) {
  ESP_LOGV(TAG, "Listing directory file info: %s\n", path);
  std::string absolut_path = build_path(path);
  DIR *dir = opendir(absolut_path.c_str());
  if (!dir) {
    ESP_LOGE(TAG, "Failed to open directory: %s", strerror(errno));
    return list;
  }
  char entry_absolut_path[FILE_PATH_MAX];
  char entry_path[FILE_PATH_MAX];
  const size_t dirpath_len = MOUNT_POINT.size();
  size_t entry_path_len = strlen(path);
  strlcpy(entry_path, path, sizeof(entry_path));
  strlcpy(entry_path + entry_path_len, "/", sizeof(entry_path) - entry_path_len);
  entry_path_len = strlen(entry_path);

  strlcpy(entry_absolut_path, MOUNT_POINT.c_str(), sizeof(entry_absolut_path));
  struct dirent *entry;
  while ((entry = readdir(dir)) != nullptr) {
    size_t file_size = 0;
    strlcpy(entry_path + entry_path_len, entry->d_name, sizeof(entry_path) - entry_path_len);
    strlcpy(entry_absolut_path + dirpath_len, entry_path, sizeof(entry_absolut_path) - dirpath_len);
    if (entry->d_type != DT_DIR) {
      struct stat info;
      if (stat(entry_absolut_path, &info) < 0) {
        ESP_LOGE(TAG, "Failed to stat file: %s '%s' %s", strerror(errno), entry->d_name, entry_absolut_path);
      } else {
        file_size = info.st_size;
      }
    }
    list.emplace_back(entry_path, file_size, entry->d_type == DT_DIR);
    if (entry->d_type == DT_DIR && depth)
      list_directory_file_info_rec(entry_absolut_path, depth - 1, list);
  }
  closedir(dir);
  return list;
}

bool SdMmc::is_directory(const char *path) {
  std::string absolut_path = build_path(path);
  DIR *dir = opendir(absolut_path.c_str());
  if (dir) {
    closedir(dir);
  }
  return dir != nullptr;
}

size_t SdMmc::file_size(const char *path) {
  std::string absolut_path = build_path(path);
  struct stat info;
  size_t file_size = 0;
  if (stat(absolut_path.c_str(), &info) < 0) {
    ESP_LOGE(TAG, "Failed to stat file: %s", strerror(errno));
    return -1;
  }
  return info.st_size;
}

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

  FATFS *fs;
  DWORD fre_clust, fre_sect, tot_sect;
  uint64_t total_bytes = -1, free_bytes = -1, used_bytes = -1;
  auto res = f_getfree(MOUNT_POINT.c_str(), &fre_clust, &fs);
  if (!res) {
    tot_sect = (fs->n_fatent - 2) * fs->csize;
    fre_sect = fre_clust * fs->csize;

    total_bytes = static_cast<uint64_t>(tot_sect) * FF_SS_SDCARD;
    free_bytes = static_cast<uint64_t>(fre_sect) * FF_SS_SDCARD;
    used_bytes = total_bytes - free_bytes;
  }

  if (this->used_space_sensor_ != nullptr)
    this->used_space_sensor_->publish_state(used_bytes);
  if (this->total_space_sensor_ != nullptr)
    this->total_space_sensor_->publish_state(total_bytes);
  if (this->free_space_sensor_ != nullptr)
    this->free_space_sensor_->publish_state(free_bytes);

  for (auto &sensor : this->file_size_sensors_) {
    if (sensor.sensor != nullptr)
      sensor.sensor->publish_state(this->file_size(sensor.path));
  }
#endif
}

}  // namespace sd_mmc_card
}  // namespace esphome

#endif  // USE_ESP_IDF