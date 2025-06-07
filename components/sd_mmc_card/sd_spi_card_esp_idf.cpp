#include <cerrno>
#include "sd_spi_card.h"

#ifdef SDMMC_USE_SDSPI
#ifdef USE_ESP_IDF

#include "math.h"
#include "esphome/core/log.h"
extern "C" {
#include "esp_vfs.h"
#include "esp_vfs_fat.h"
}

#ifndef VFS_FAT_MOUNT_DEFAULT_CONFIG
#define VFS_FAT_MOUNT_DEFAULT_CONFIG() \
    { \
        .format_if_mount_failed = false, \
        .max_files = 5, \
        .allocation_unit_size = 0, \
        .disk_status_check_enable = false, \
    }
#endif // VFS_FAT_MOUNT_DEFAULT_CONFIG

int constexpr SD_OCR_SDHC_CAP = (1 << 30);  // value defined in esp-idf/components/sdmmc/include/sd_protocol_defs.h

namespace esphome {
namespace sd_mmc_card {

static constexpr size_t FILE_PATH_MAX = ESP_VFS_PATH_MAX + CONFIG_SPIFFS_OBJ_NAME_LEN;
static const char *TAG = "sd_mmc_card";
static const std::string MOUNT_POINT("/sdcard");

std::string build_path(const char *path) { return MOUNT_POINT + path; }

void SdSpi::loop() {}

void SdSpi::dump_config() {
  ESP_LOGCONFIG(TAG, "SD SPI Component");
  ESP_LOGCONFIG(TAG, "  Mode 1 bit: %s", TRUEFALSE(this->mode_1bit_));
  ESP_LOGCONFIG(TAG, "  Data Rate: %" PRIu32 "", this->data_rate_);
  if (this->power_ctrl_pin_ != nullptr) {
    LOG_PIN("  Power Ctrl Pin: ", this->power_ctrl_pin_);
  }
  LOG_PIN("  CS Pin:", this->cs_);

#ifdef USE_SENSOR
  LOG_SENSOR("  ", "Used space", this->used_space_sensor_);
  LOG_SENSOR("  ", "Total space", this->total_space_sensor_);
  LOG_SENSOR("  ", "Free space", this->free_space_sensor_);
  LOG_SENSOR("  ", "Max Frequency", this->max_frequency_sensor_);
  LOG_SENSOR("  ", "Real Frequency", this->real_frequency_sensor_);
  for (auto &sensor : this->file_size_sensors_) {
    if (sensor.sensor != nullptr)
      LOG_SENSOR("  ", "File size", sensor.sensor);
  }
#endif
#ifdef USE_TEXT_SENSOR
  LOG_TEXT_SENSOR("  ", "SD Card Type", this->sd_card_type_text_sensor_);
#endif

  if (this->is_failed()) {
    ESP_LOGE(TAG, "Setup failed : %s", SdSpi::error_code_to_string(this->init_error_).c_str());
    return;
  }
}

std::string SdSpi::error_code_to_string(SdSpi::ErrorCode code) {
  switch (code) {
    case ErrorCode::ERR_PIN_SETUP:
      return "Failed to set pins";
    case ErrorCode::ERR_MOUNT:
      return "Failed to mount card";
    case ErrorCode::ERR_NO_CARD:
      return "No card found";
    default:
      return "Unknown error";
  }
}

#ifdef USE_SENSOR
void SdSpi::add_file_size_sensor(sensor::Sensor *sensor, std::string const &path) {
  this->file_size_sensors_.emplace_back(sensor, path);
}
#endif

void SdSpi::setup() {
  if (this->power_ctrl_pin_ != nullptr)
    this->power_ctrl_pin_->setup();

  auto setup_input_pullup = [](GPIOPin *pin) {
    pin->pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
    pin->setup();
  };
  if (this->data1_pin_ != nullptr)
    setup_input_pullup(this->data1_pin_);
  if (this->data2_pin_ != nullptr)
    setup_input_pullup(this->data2_pin_);

  this->spi_setup();

  esp_vfs_fat_sdmmc_mount_config_t mount_config = VFS_FAT_MOUNT_DEFAULT_CONFIG();
  mount_config.format_if_mount_failed = false;
  mount_config.max_files = 5;
  mount_config.allocation_unit_size = 16 * 1024;
  mount_config.disk_status_check_enable = false;

  const auto init_err = sdspi_host_init();
  if (init_err != ESP_OK) {
    ESP_LOGE(TAG, "Failed init sdspi host: %s", esp_err_to_name(init_err));
    this->mark_failed();
    return;
  }
  ESP_LOGV(TAG, "sdspi host initialized");

  sdmmc_host_t host = SDSPI_HOST_DEFAULT();
  host.slot = this->spi_interface_;

  sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
  // this->data_rate_
  slot_config.host_id = this->spi_interface_;
  slot_config.gpio_cs = static_cast<gpio_num_t>(spi::Utility::get_pin_no(this->cs_));

  esp_err_t mount_error = ESP_OK;
  for (const auto freq_khz : {// SDMMC_FREQ_HIGHSPEED,
                              SDMMC_FREQ_DEFAULT, SDMMC_FREQ_PROBING}) {
    host.max_freq_khz = freq_khz;
    ESP_LOGV(
        TAG, "Mounting file system:\n spi host: %s\n max_freq_khz: %d",
        [](spi_host_device_t spi) -> const char * {
          switch (spi) {
            case SPI1_HOST:
              return "SPI1_HOST";
            case SPI2_HOST:
              return "SPI2_HOST";
            default:
              break;
          }
          return "unknown";
        }(slot_config.host_id),
        host.max_freq_khz);
    mount_error = esp_vfs_fat_sdspi_mount(MOUNT_POINT.c_str(), &host, &slot_config, &mount_config, &this->card_);
    if (mount_error != ESP_ERR_INVALID_RESPONSE) {
      break;
    }
  }
  if (mount_error != ESP_OK) {
    ESP_LOGE(TAG, "Failed to mount FAT fs: %s", esp_err_to_name(mount_error));
    switch (mount_error) {
      case ESP_FAIL:
      case ESP_ERR_INVALID_CRC:
        this->init_error_ = ErrorCode::ERR_MOUNT;
        break;
      case ESP_ERR_TIMEOUT:
      default:
        this->init_error_ = ErrorCode::ERR_NO_CARD;
    }
    mark_failed();
    return;
  }

#ifdef USE_TEXT_SENSOR
  if (this->sd_card_type_text_sensor_ != nullptr)
    this->sd_card_type_text_sensor_->publish_state(sd_card_type());
#endif

#ifdef USE_SENSOR
  if (this->max_frequency_sensor_)
    this->max_frequency_sensor_->publish_state(this->card_->max_freq_khz);
  if (this->real_frequency_sensor_)
    this->real_frequency_sensor_->publish_state(this->card_->real_freq_khz);
#endif  // USE_SENSOR
  update_sensors();
}

File SdSpi::open(const char *path, const char *mode) {
  const std::string &absolute_path = build_path(path);
  ESP_LOGVV(TAG, "Opening file %s (absolute: %s) %s mode", path, absolute_path.c_str(), mode);
  auto file = fopen(absolute_path.c_str(), mode);
  size_t file_size = 0;
  if (file) {
    struct stat info;
    auto fd = fileno(file);
    ESP_LOGVV(TAG, "Calling fstat(%i)", fd);
    if (fstat(fd, &info) < 0) {
      ESP_LOGE(TAG, "Failed to fstat file %s (absolute: %s): %s", path, absolute_path.c_str(), strerror(errno));
    } else {
      file_size = info.st_size;
    }
  }
  return File(file, file_size);
}

void SdSpi::write_file(const char *path, const uint8_t *buffer, size_t len, const char *mode) {
  std::string absolute_path = build_path(path);
  FILE *file = NULL;
  file = fopen(absolute_path.c_str(), mode);
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

bool SdSpi::create_directory(const char *path) {
  ESP_LOGV(TAG, "Create directory: %s", path);
  std::string absolute_path = build_path(path);
  if (mkdir(absolute_path.c_str(), 0777) < 0) {
    ESP_LOGE(TAG, "Failed to create a new directory: %s", strerror(errno));
    return false;
  }
  this->update_sensors();
  return true;
}

bool SdSpi::remove_directory(const char *path) {
  ESP_LOGV(TAG, "Remove directory: %s", path);
  if (!this->is_directory(path)) {
    ESP_LOGE(TAG, "Not a directory");
    return false;
  }
  std::string absolute_path = build_path(path);
  if (remove(absolute_path.c_str()) != 0) {
    ESP_LOGE(TAG, "Failed to remove directory: %s", strerror(errno));
  }
  this->update_sensors();
  return true;
}

bool SdSpi::delete_file(const char *path) {
  ESP_LOGV(TAG, "Delete File: %s", path);
  if (this->is_directory(path)) {
    ESP_LOGE(TAG, "Not a file");
    return false;
  }
  std::string absolute_path = build_path(path);
  if (remove(absolute_path.c_str()) != 0) {
    ESP_LOGE(TAG, "Failed to remove file: %s", strerror(errno));
  }
  this->update_sensors();
  return true;
}

std::vector<uint8_t> SdSpi::read_file(char const *path) {
  ESP_LOGV(TAG, "Read File: %s", path);

  std::string absolute_path = build_path(path);
  FILE *file = nullptr;
  file = fopen(absolute_path.c_str(), "rb");
  if (file == nullptr) {
    ESP_LOGE(TAG, "Failed to open file for reading");
    return std::vector<uint8_t>();
  }

  std::vector<uint8_t> res;
  size_t fileSize = this->file_size(path);
  res.resize(fileSize);
  size_t len = fread(res.data(), 1, fileSize, file);
  fclose(file);
  if (len == 0) {
    if (ferror(file)) {
      ESP_LOGE(TAG, "Failed to read file: %s", strerror(errno));
      return std::vector<uint8_t>();
    }
  }

  return res;
}

std::vector<FileInfo> &SdSpi::list_directory_file_info_rec(const char *path, uint8_t depth,
                                                           std::vector<FileInfo> &list) {
  ESP_LOGV(TAG, "Listing directory file info: %s\n", path);
  std::string absolute_path = build_path(path);
  DIR *dir = opendir(absolute_path.c_str());
  if (!dir) {
    ESP_LOGE(TAG, "Failed to open directory: %s", strerror(errno));
    return list;
  }
  char entry_absolute_path[FILE_PATH_MAX];
  char entry_path[FILE_PATH_MAX];
  const size_t dirpath_len = MOUNT_POINT.size();
  size_t entry_path_len = strlen(path);
  strlcpy(entry_path, path, sizeof(entry_path));
  strlcpy(entry_path + entry_path_len, "/", sizeof(entry_path) - entry_path_len);
  entry_path_len = strlen(entry_path);

  strlcpy(entry_absolute_path, MOUNT_POINT.c_str(), sizeof(entry_absolute_path));
  struct dirent *entry;
  while ((entry = readdir(dir)) != nullptr) {
    size_t file_size = 0;
    strlcpy(entry_path + entry_path_len, entry->d_name, sizeof(entry_path) - entry_path_len);
    strlcpy(entry_absolute_path + dirpath_len, entry_path, sizeof(entry_absolute_path) - dirpath_len);
    if (entry->d_type != DT_DIR) {
      struct stat info;
      if (stat(entry_absolute_path, &info) < 0) {
        ESP_LOGE(TAG, "Failed to stat file: %s '%s' %s", strerror(errno), entry->d_name, entry_absolute_path);
      } else {
        file_size = info.st_size;
      }
    }
    list.emplace_back(entry_path, file_size, entry->d_type == DT_DIR);
    if (entry->d_type == DT_DIR && depth)
      this->list_directory_file_info_rec(entry_absolute_path, depth - 1, list);
  }
  closedir(dir);
  return list;
}

bool SdSpi::is_directory(const char *path) {
  std::string absolute_path = build_path(path);
  DIR *dir = opendir(absolute_path.c_str());
  if (dir) {
    closedir(dir);
    return true;
  }
  return false;
}

size_t SdSpi::file_size(const char *path) {
  std::string absolute_path = build_path(path);
  struct stat info;
  size_t file_size = 0;
  if (stat(absolute_path.c_str(), &info) < 0) {
    ESP_LOGE(TAG, "Failed to stat file %s (absolute: %s): %s", path, absolute_path.c_str(), strerror(errno));
    errno = 0;
    return -1;
  }
  return info.st_size;
}

std::string SdSpi::sd_card_type() const {
  if (this->card_->is_sdio) {
    return "SDIO";
  } else if (this->card_->is_mmc) {
    return "MMC";
  } else {
    return (this->card_->ocr & SD_OCR_SDHC_CAP) ? "SDHC/SDXC" : "SDSC";
  }
  return "UNKNOWN";
}

void SdSpi::update_sensors() {
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
      sensor.sensor->publish_state(this->file_size(sensor.path.c_str()));
  }
#endif
}

}  // namespace sd_mmc_card
}  // namespace esphome

#endif  // USE_ESP_IDF
#endif  // SDMMC_USE_SDSPI