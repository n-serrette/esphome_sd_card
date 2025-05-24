#pragma once

#include "esphome/core/defines.h"

#ifdef SDMMC_USE_SDMMC

#include "sd_card.h"
#include "sd_card_actions.h"
#include "memory_units.h"

#include "esphome/core/gpio.h"
#include "esphome/core/component.h"
#include "esphome/core/automation.h"

#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif

#ifdef USE_ESP_IDF
#include "sdmmc_cmd.h"
#endif

namespace esphome {
namespace sd_mmc_card {

#ifdef USE_SENSOR
struct FileSizeSensor {
  sensor::Sensor *sensor{nullptr};
  std::string path;

  FileSizeSensor() = default;
  FileSizeSensor(sensor::Sensor *, std::string const &path);
};
#endif


class SdMmc : public Component, public SdCard {
#ifdef USE_SENSOR
  SUB_SENSOR(used_space)
  SUB_SENSOR(total_space)
  SUB_SENSOR(free_space)
#endif
#ifdef USE_TEXT_SENSOR
  SUB_TEXT_SENSOR(sd_card_type)
#endif
 public:
  enum ErrorCode {
    ERR_PIN_SETUP,
    ERR_MOUNT,
    ERR_NO_CARD,
  };
  void setup() override;
  void loop() override;
  void dump_config() override;

  File open(const char *path, const char *mode) override;

  void write_file(const char *path, const uint8_t *buffer, size_t len, const char *mode) override;
  bool delete_file(const char *path) override;
  bool create_directory(const char *path) override;
  bool remove_directory(const char *path) override;
  std::vector<uint8_t> read_file(char const *path) override;
  bool is_directory(const char *path) override;
  size_t file_size(const char *path) override;
#ifdef USE_SENSOR
  void add_file_size_sensor(sensor::Sensor *, std::string const &path);
#endif

  void set_clk_pin(uint8_t);
  void set_cmd_pin(uint8_t);
  void set_data0_pin(uint8_t);
  void set_data1_pin(uint8_t);
  void set_data2_pin(uint8_t);
  void set_data3_pin(uint8_t);
  void set_mode_1bit(bool);
  void set_power_ctrl_pin(GPIOPin *);

 protected:
  ErrorCode init_error_;
  uint8_t clk_pin_;
  uint8_t cmd_pin_;
  uint8_t data0_pin_;
  uint8_t data1_pin_;
  uint8_t data2_pin_;
  uint8_t data3_pin_;
  bool mode_1bit_;
  GPIOPin *power_ctrl_pin_{nullptr};

#ifdef USE_ESP_IDF
  sdmmc_card_t *card_;
#endif
#ifdef USE_SENSOR
  std::vector<FileSizeSensor> file_size_sensors_{};
#endif
  void update_sensors();
#ifdef USE_ESP32_FRAMEWORK_ARDUINO
  std::string sd_card_type_to_string(int) const;
#endif
#ifdef USE_ESP_IDF
  std::string sd_card_type() const;
#endif
  std::vector<FileInfo> &list_directory_file_info_rec(const char *path, uint8_t depth, std::vector<FileInfo> &list) override;
  static std::string error_code_to_string(ErrorCode);
};


}  // namespace sd_mmc_card
}  // namespace esphome
#endif // SDMMC_USE_SDMMC