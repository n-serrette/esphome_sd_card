#include "sd_mmc_card.h"

#include "math.h"
#include "esphome/core/log.h"

namespace esphome {
namespace sd_mmc_card {

static const char *TAG = "sd_mmc_card";

#ifdef USE_SENSOR
FileSizeSensor::FileSizeSensor(sensor::Sensor *sensor, std::string const &path) : sensor(sensor), path(path) {}
#endif

void SdMmc::loop() {}

void SdMmc::dump_config() {
  ESP_LOGCONFIG(TAG, "SD MMC Component");
  ESP_LOGCONFIG(TAG, "Mode 1 bit: %d", this->mode_1bit_);
  LOG_PIN("  CLK Pin: ", this->clk_pin_);
  LOG_PIN("  CMD Pin: ", this->cmd_pin_);
  LOG_PIN("  DATA0 Pin: ", this->data0_pin_);
  if (!this->mode_1bit_) {
    LOG_PIN("  DATA1 Pin: ", this->data1_pin_);
    LOG_PIN("  DATA2 Pin: ", this->data2_pin_);
    LOG_PIN("  DATA3 Pin: ", this->data3_pin_);
  }
#ifdef USE_SENSOR
  LOG_SENSOR("  ", "Used space", this->used_space_sensor_);
  LOG_SENSOR("  ", "Total space", this->total_space_sensor_);
  for (auto &sensor : this->file_size_sensors_) {
    if (sensor.sensor != nullptr)
      LOG_SENSOR("  ", "File size", sensor.sensor);
  }
#endif
#ifdef USE_TEXT_SENSOR
  LOG_TEXT_SENSOR("  ", "SD Card Type", this->sd_card_type_text_sensor_);
#endif
}

size_t SdMmc::file_size(std::string const &path) { return this->file_size(path.c_str()); }

#ifdef USE_SENSOR
void SdMmc::add_file_size_sensor(sensor::Sensor *sensor, std::string const &path) {
  this->file_size_sensors_.emplace_back(sensor, path);
}
#endif

void SdMmc::set_clk_pin(GPIOPin *pin) { this->clk_pin_ = pin; }

void SdMmc::set_cmd_pin(GPIOPin *pin) { this->cmd_pin_ = pin; }

void SdMmc::set_data0_pin(GPIOPin *pin) { this->data0_pin_ = pin; }

void SdMmc::set_data1_pin(GPIOPin *pin) { this->data1_pin_ = pin; }

void SdMmc::set_data2_pin(GPIOPin *pin) { this->data2_pin_ = pin; }

void SdMmc::set_data3_pin(GPIOPin *pin) { this->data3_pin_ = pin; }

void SdMmc::set_mode_1bit(bool b) { this->mode_1bit_ = b; }

int Utility::get_pin_no(GPIOPin *pin) {
  if (pin == nullptr || !pin->is_internal())
    return -1;
  if (((InternalGPIOPin *) pin)->is_inverted())
    return -1;
  return ((InternalGPIOPin *) pin)->get_pin();
}

long double convertBytes(uint64_t value, MemoryUnits unit) {
  return value * 1.0 / pow(1024, static_cast<uint64_t>(unit));
}

}  // namespace sd_mmc_card
}  // namespace esphome