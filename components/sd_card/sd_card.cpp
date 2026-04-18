#include "sd_card.h"

#include <algorithm>
#include <memory>
#include "math.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "freertos/task.h"
#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
#include "driver/sdmmc_types.h"

int constexpr SD_OCR_SDHC_CAP = (1 << 30);  // value defined in esp-idf/components/SdCard/include/sd_protocol_defs.h

namespace esphome {
namespace sd_card {

static const char *TAG = "sd_card";
static constexpr size_t FILE_PATH_MAX = ESP_VFS_PATH_MAX + 255;  // 255 = FAT LFN max
static const std::string MOUNT_POINT("/sdcard");

std::string SdCard::build_path(const std::string &path) const {
  std::string full = MOUNT_POINT + path;
  while (full.size() > MOUNT_POINT.size() && full.back() == '/')
    full.pop_back();
  return full;
}

#ifdef USE_SENSOR
FileSizeSensor::FileSizeSensor(sensor::Sensor *sensor, std::string const &path) : sensor(sensor), path(path) {}
#endif

void SdCard::setup() {
  ESP_LOGI(TAG, "Setting up SD MMC...");
  
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
    this->sd_card_type_text_sensor_->publish_state(this->sd_card_type());
#endif

  update_sensors();

  ESP_LOGI(TAG, "SD MMC mounted successfully");
}

void SdCard::loop() {}

void SdCard::dump_config() {
  ESP_LOGCONFIG(TAG, "SD MMC Component");
  ESP_LOGCONFIG(TAG, "  Mode 1 bit: %s", TRUEFALSE(this->mode_1bit_));
  ESP_LOGCONFIG(TAG, "  CLK Pin: %d", this->clk_pin_);
  ESP_LOGCONFIG(TAG, "  CMD Pin: %d", this->cmd_pin_);
  ESP_LOGCONFIG(TAG, "  DATA0 Pin: %d", this->data0_pin_);
  if (!this->mode_1bit_) {
    ESP_LOGCONFIG(TAG, "  DATA1 Pin: %d", this->data1_pin_);
    ESP_LOGCONFIG(TAG, "  DATA2 Pin: %d", this->data2_pin_);
    ESP_LOGCONFIG(TAG, "  DATA3 Pin: %d", this->data3_pin_);
  }

  if (this->power_ctrl_pin_ != nullptr) {
    LOG_PIN("  Power Ctrl Pin: ", this->power_ctrl_pin_);
  }

#ifdef USE_SENSOR
  LOG_SENSOR("  ", "Used space", this->used_space_sensor_);
  LOG_SENSOR("  ", "Total space", this->total_space_sensor_);
  LOG_SENSOR("  ", "Free space", this->free_space_sensor_);
  for (auto &sensor : this->file_size_sensors_) {
    if (sensor.sensor != nullptr)
      LOG_SENSOR("  ", "File size", sensor.sensor);
  }
#endif
#ifdef USE_TEXT_SENSOR
  LOG_TEXT_SENSOR("  ", "SD Card Type", this->sd_card_type_text_sensor_);
#endif

  if (this->is_failed()) {
    ESP_LOGE(TAG, "Setup failed : %s", SdCard::error_code_to_string(this->init_error_).c_str());
    return;
  }
}

void SdCard::write_file(const char *path, const uint8_t *buffer, size_t len, const char *mode) {
  std::string absolut_path = this->build_path(path);
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

void SdCard::write_file(const char *path, const uint8_t *buffer, size_t len) {
  ESP_LOGV(TAG, "Writing to file: %s", path);
  this->write_file(path, buffer, len, "w");
}

void SdCard::append_file(const char *path, const uint8_t *buffer, size_t len) {
  ESP_LOGV(TAG, "Appending to file: %s", path);
  this->write_file(path, buffer, len, "a");
}

bool SdCard::create_directory(const char *path) {
  ESP_LOGV(TAG, "Create directory: %s", path);
  std::string fatfs_path = "0:" + std::string(path);
  FRESULT res = f_mkdir(fatfs_path.c_str());
  if (res != FR_OK) {
    ESP_LOGE(TAG, "Failed to create directory '%s': FATFS error %d", path, (int)res);
    return false;
  }
  this->update_sensors();
  return true;
}

bool SdCard::remove_directory(const char *path) {
  ESP_LOGV(TAG, "Remove directory: %s", path);
  if (!this->is_directory(path)) {
    ESP_LOGE(TAG, "Not a directory");
    return false;
  }
  std::string absolut_path = this->build_path(path);
  if (remove(absolut_path.c_str()) != 0) {
    ESP_LOGE(TAG, "Failed to remove directory: %s", strerror(errno));
  }
  this->update_sensors();
  return true;
}

bool SdCard::delete_file(const char *path) {
  ESP_LOGV(TAG, "Delete File: %s", path);
  if (this->is_directory(path)) {
    ESP_LOGE(TAG, "Not a file");
    return false;
  }
  std::string absolut_path = this->build_path(path);
  if (remove(absolut_path.c_str()) != 0) {
    ESP_LOGE(TAG, "Failed to remove file: %s", strerror(errno));
  }
  this->update_sensors();
  return true;
}

bool SdCard::delete_file(std::string const &path) { return this->delete_file(path.c_str()); }

std::vector<uint8_t> SdCard::read_file(const char *path) {
  ESP_LOGI(TAG, "read_file: Starting read of '%s'", path);
  std::string absolut_path = this->build_path(path);
  FILE *file = fopen(absolut_path.c_str(), "rb");
  if (file == nullptr) {
    ESP_LOGE(TAG, "read_file: Failed to open file '%s': %s", path, strerror(errno));
    return std::vector<uint8_t>();
  }

  size_t fileSize = this->file_size(path);
  
  // Check if we have enough heap memory before attempting allocation
  // Note: ESP32 heap fragmentation can make free heap unreliable.
  // The 8KB margin is conservative but may not prevent all OOM cases.
  size_t free_heap = esp_get_free_heap_size();
  size_t required_memory = fileSize + 8192; // Safety margin for fragmentation
  
  ESP_LOGI(TAG, "read_file: File size: %u bytes, Free heap: %u bytes, Required: %u bytes", 
           fileSize, free_heap, required_memory);
  
  if (required_memory > free_heap) {
    ESP_LOGE(TAG, "read_file: Insufficient memory. Size: %u, Free heap: %u, Shortfall: %d", 
             fileSize, free_heap, (int)(required_memory - free_heap));
    fclose(file);
    return std::vector<uint8_t>();
  }
  
  if (fileSize > 102400) { // 100KB limit to prevent excessive memory usage
    ESP_LOGW(TAG, "read_file: File too large (%u bytes). Max allowed: 102400 bytes. Use stream_file() for large files.", fileSize);
    fclose(file);
    return std::vector<uint8_t>();
  }
  
  std::vector<uint8_t> res;
  
  // Try to reserve/resize - check if allocation succeeded
  ESP_LOGD(TAG, "read_file: Attempting to allocate %u bytes...", fileSize);
  res.resize(fileSize);
  if (res.size() != fileSize) {
    ESP_LOGE(TAG, "read_file: Memory allocation FAILED for %u bytes (free heap: %u). Heap fragmentation issue.", 
             fileSize, free_heap);
    fclose(file);
    return std::vector<uint8_t>();
  }
  ESP_LOGD(TAG, "read_file: Memory allocation successful");
  
  size_t len = fread(res.data(), 1, fileSize, file);
  
  if (len != fileSize) {
    ESP_LOGE(TAG, "read_file: Failed to read file: expected %u bytes, got %u. Error: %s", fileSize, len, strerror(errno));
    fclose(file);
    return std::vector<uint8_t>();
  }
  
  fclose(file);
  
  ESP_LOGI(TAG, "read_file: Successfully read %u bytes from '%s'. Free heap now: %u", 
           len, path, esp_get_free_heap_size());

  return res;
}

std::vector<uint8_t> SdCard::read_file(std::string const &path) { return this->read_file(path.c_str()); }

size_t SdCard::read_file_chunk(const char *path, size_t offset, uint8_t *buffer, size_t buffer_size) {
  std::string absolut_path = this->build_path(path);
  FILE *file = fopen(absolut_path.c_str(), "rb");
  if (file == nullptr) {
    ESP_LOGE(TAG, "Failed to open file for chunk read: %s", strerror(errno));
    return 0;
  }

  if (fseek(file, offset, SEEK_SET) != 0) {
    ESP_LOGE(TAG, "Failed to seek to offset %u: %s", offset, strerror(errno));
    fclose(file);
    return 0;
  }

  size_t bytes_read = fread(buffer, 1, buffer_size, file);
  fclose(file);
  return bytes_read;
}

bool SdCard::stream_file(const char *path, FileChunkCallback callback, size_t chunk_size) {
  ESP_LOGV(TAG, "Streaming file: %s with chunk size: %u", path, chunk_size);
  std::string absolut_path = this->build_path(path);
  FILE *file = fopen(absolut_path.c_str(), "rb");
  if (file == nullptr) {
    ESP_LOGE(TAG, "Failed to open file for streaming: %s", strerror(errno));
    return false;
  }

  uint8_t *buffer = new (std::nothrow) uint8_t[chunk_size];
  if (buffer == nullptr) {
    ESP_LOGE(TAG, "Failed to allocate chunk buffer of %u bytes", chunk_size);
    fclose(file);
    return false;
  }

  bool success = true;
  size_t bytes_read;
  while ((bytes_read = fread(buffer, 1, chunk_size, file)) > 0) {
    if (!callback(buffer, bytes_read)) {
      ESP_LOGW(TAG, "Callback returned false, stopping stream");
      success = false;
      break;
    }
  }

  delete[] buffer;
  fclose(file);
  return success;
}

std::vector<FileInfo> &SdCard::list_directory_file_info_rec(const char *path, uint8_t depth,
                                                           std::vector<FileInfo> &list) {
  ESP_LOGV(TAG, "Listing directory file info: %s\n", path);
  std::string fatfs_path = "0:" + std::string(path);
  FF_DIR dir;
  FRESULT res = f_opendir(&dir, fatfs_path.c_str());
  if (res != FR_OK) {
    ESP_LOGE(TAG, "Failed to open directory '%s': FATFS error %d", path, (int)res);
    return list;
  }

  // Build a base path without trailing slash (except bare "/").
  std::string base_path(path);
  if (base_path.size() > 1 && base_path.back() == '/')
    base_path.pop_back();

  FILINFO fno;
  while (f_readdir(&dir, &fno) == FR_OK && fno.fname[0] != '\0') {
    bool is_dir = (fno.fattrib & AM_DIR) != 0;
    std::string entry_path = (base_path == "/" ? "/" : base_path + "/") + fno.fname;
    size_t file_size = is_dir ? 0 : static_cast<size_t>(fno.fsize);
    list.emplace_back(entry_path, file_size, is_dir);
    if (is_dir && depth)
      list_directory_file_info_rec(entry_path.c_str(), depth - 1, list);
  }

  f_closedir(&dir);
  return list;
}

void SdCard::list_directory_file_info_stream(const char *path, uint8_t depth, FileInfoCallback callback) {
  uint32_t count = 0;
  list_directory_file_info_stream_rec(path, depth, callback, count);
}

void SdCard::list_directory_file_info_stream_rec(const char *path, uint8_t depth,
                                                 FileInfoCallback &callback,
                                                 uint32_t &count) {
  // Heap-allocate FF_DIR and FILINFO: with LFN+Unicode enabled FILINFO is
  // ~600 bytes and FF_DIR is ~100 bytes.  Keeping them on the call stack
  // would leave them live while the callback drives into the network send
  // path, easily overflowing the httpd task's stack.
  auto dir = std::unique_ptr<FF_DIR>(new (std::nothrow) FF_DIR);
  auto fno = std::unique_ptr<FILINFO>(new (std::nothrow) FILINFO);
  if (!dir || !fno) {
    ESP_LOGE(TAG, "list_directory_file_info_stream_rec: OOM allocating FATFS structs");
    return;
  }

  std::string fatfs_path = "0:" + std::string(path);
  FRESULT res = f_opendir(dir.get(), fatfs_path.c_str());
  if (res != FR_OK) {
    ESP_LOGE(TAG, "Failed to open directory '%s': FATFS error %d", path, (int)res);
    return;
  }

  std::string base_path(path);
  if (base_path.size() > 1 && base_path.back() == '/')
    base_path.pop_back();

  while (f_readdir(dir.get(), fno.get()) == FR_OK && fno->fname[0] != '\0') {
    bool is_dir = (fno->fattrib & AM_DIR) != 0;
    std::string entry_path = (base_path == "/" ? "/" : base_path + "/") + fno->fname;
    size_t entry_size = is_dir ? 0 : static_cast<size_t>(fno->fsize);

    ++count;
    if ((count & 31) == 0)
      vTaskDelay(1);  // yield to watchdog / other tasks every 32 entries

    if (!callback(FileInfo(entry_path, entry_size, is_dir))) {
      f_closedir(dir.get());
      return;
    }

    if (is_dir && depth)
      list_directory_file_info_stream_rec(entry_path.c_str(), depth - 1, callback, count);
  }

  f_closedir(dir.get());
}

bool SdCard::is_directory(const char *path) {
  std::string stripped(path);
  while (stripped.size() > 1 && stripped.back() == '/')
    stripped.pop_back();
  if (stripped == "/") return true;  // SD root is always a directory
  std::string fatfs_path = "0:" + stripped;
  FILINFO fno;
  FRESULT res = f_stat(fatfs_path.c_str(), &fno);
  if (res != FR_OK) return false;
  return (fno.fattrib & AM_DIR) != 0;
}

bool SdCard::is_directory(std::string const &path) { return this->is_directory(path.c_str()); }

size_t SdCard::file_size(const char *path) {
  // Use FATFS API directly — same reason as is_directory().
  std::string stripped(path);
  while (stripped.size() > 1 && stripped.back() == '/')
    stripped.pop_back();
  std::string fatfs_path = "0:" + stripped;
  FILINFO fno;
  FRESULT res = f_stat(fatfs_path.c_str(), &fno);
  if (res != FR_OK) {
    ESP_LOGE(TAG, "Failed to stat '%s': FATFS error %d", path, (int)res);
    return static_cast<size_t>(-1);
  }
  return static_cast<size_t>(fno.fsize);
}

size_t SdCard::file_size(std::string const &path) { return this->file_size(path.c_str()); }

std::string SdCard::sd_card_type() const {
  if (this->card_->is_sdio) {
    return "SDIO";
  } else if (this->card_->is_mmc) {
    return "MMC";
  } else {
    return (this->card_->ocr & SD_OCR_SDHC_CAP) ? "SDHC/SDXC" : "SDSC";
  }
}

void SdCard::update_sensors() {
#ifdef USE_SENSOR
  if (this->card_ == nullptr)
    return;

  static constexpr uint32_t SPACE_DEBOUNCE_MS = 30000;
  uint32_t now = millis();
  bool do_space = (this->last_sensor_update_ms_ == 0 ||
                   (now - this->last_sensor_update_ms_) >= SPACE_DEBOUNCE_MS);

  if (do_space) {
    this->last_sensor_update_ms_ = now;

    FATFS *fs;
    DWORD fre_clust, fre_sect, tot_sect;
    uint64_t total_bytes = 0, free_bytes = 0, used_bytes = 0;
    auto res = f_getfree(MOUNT_POINT.c_str(), &fre_clust, &fs);
    if (!res) {
      tot_sect = (fs->n_fatent - 2) * fs->csize;
      fre_sect = fre_clust * fs->csize;

      total_bytes = static_cast<uint64_t>(tot_sect) * FF_SS_SDCARD;
      free_bytes = static_cast<uint64_t>(fre_sect) * FF_SS_SDCARD;
      used_bytes = total_bytes - free_bytes;

      if (used_bytes > total_bytes) {
        ESP_LOGW(TAG, "SD card space calculation error: used (%llu) > total (%llu)", used_bytes, total_bytes);
        used_bytes = total_bytes;
      }

      ESP_LOGD(TAG, "SD card space - Total: %llu, Free: %llu, Used: %llu bytes", total_bytes, free_bytes, used_bytes);

      if (this->used_space_sensor_ != nullptr)
        this->used_space_sensor_->publish_state(used_bytes);
      if (this->total_space_sensor_ != nullptr)
        this->total_space_sensor_->publish_state(total_bytes);
      if (this->free_space_sensor_ != nullptr)
        this->free_space_sensor_->publish_state(free_bytes);
    } else {
      ESP_LOGE(TAG, "Failed to get SD card filesystem info: f_getfree returned %d", res);
    }
  }

  for (auto &sensor : this->file_size_sensors_) {
    if (sensor.sensor != nullptr)
      sensor.sensor->publish_state(this->file_size(sensor.path));
  }
#endif
}

std::vector<std::string> SdCard::list_directory(const char *path, uint8_t depth) {
  std::vector<std::string> list;
  std::vector<FileInfo> infos = list_directory_file_info(path, depth);
  std::transform(infos.cbegin(), infos.cend(), std::back_inserter(list), [](FileInfo const &info) { return info.path; });
  return list;
}

std::vector<std::string> SdCard::list_directory(std::string path, uint8_t depth) {
  return this->list_directory(path.c_str(), depth);
}

std::vector<FileInfo> SdCard::list_directory_file_info(const char *path, uint8_t depth) {
  std::vector<FileInfo> list;
  list_directory_file_info_rec(path, depth, list);
  return list;
}

std::vector<FileInfo> SdCard::list_directory_file_info(std::string path, uint8_t depth) {
  return this->list_directory_file_info(path.c_str(), depth);
}

#ifdef USE_SENSOR
void SdCard::add_file_size_sensor(sensor::Sensor *sensor, std::string const &path) {
  this->file_size_sensors_.emplace_back(sensor, path);
}
#endif

void SdCard::set_clk_pin(uint8_t pin) { this->clk_pin_ = pin; }

void SdCard::set_cmd_pin(uint8_t pin) { this->cmd_pin_ = pin; }

void SdCard::set_data0_pin(uint8_t pin) { this->data0_pin_ = pin; }

void SdCard::set_data1_pin(uint8_t pin) { this->data1_pin_ = pin; }

void SdCard::set_data2_pin(uint8_t pin) { this->data2_pin_ = pin; }

void SdCard::set_data3_pin(uint8_t pin) { this->data3_pin_ = pin; }

void SdCard::set_mode_1bit(bool b) { this->mode_1bit_ = b; }

void SdCard::set_power_ctrl_pin(GPIOPin *pin) { this->power_ctrl_pin_ = pin; }

std::string SdCard::error_code_to_string(SdCard::ErrorCode code) {
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

long double convertBytes(uint64_t value, MemoryUnits unit) {
  return value * 1.0 / pow(1024, static_cast<uint64_t>(unit));
}

std::string memory_unit_to_string(MemoryUnits unit) {
  switch (unit) {
    case MemoryUnits::Byte:
      return "B";
    case MemoryUnits::KiloByte:
      return "KB";
    case MemoryUnits::MegaByte:
      return "MB";
    case MemoryUnits::GigaByte:
      return "GB";
    case MemoryUnits::TeraByte:
      return "TB";
    case MemoryUnits::PetaByte:
      return "PB";
  }
  return "unknown";
}

MemoryUnits memory_unit_from_size(size_t size) {
  short unit = MemoryUnits::Byte;
  double s = static_cast<double>(size);
  while (s >= 1024 && unit < MemoryUnits::PetaByte) {
    s /= 1024;
    unit++;
  }
  return static_cast<MemoryUnits>(unit);
}

std::string format_size(size_t size) {
  MemoryUnits unit = memory_unit_from_size(size);
  char buffer[32];
  snprintf(buffer, sizeof(buffer), "%.2f %s", convertBytes(size, unit), memory_unit_to_string(unit).c_str());
  return std::string(buffer);
}

FileInfo::FileInfo(std::string const &path, size_t size, bool is_directory)
    : path(path), size(size), is_directory(is_directory) {}

}  // namespace sd_card
}  // namespace esphome
