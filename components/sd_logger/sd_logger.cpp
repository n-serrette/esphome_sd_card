#include "sd_logger.h"

#include <cstring>

#include "esphome/core/log.h"

extern "C" {
#include "esp_heap_caps.h"
#include "esp_timer.h"
}

namespace esphome {
namespace sd_logger {

static const char *const TAG = "sd_logger";

// -- ESPHome lifecycle ---------------------------------------------------------

void SdLogger::setup() {
  // Phase 2: create FreeRTOS queue, scan catalog.bin for power-loss recovery,
  //          launch task_logging_ and task_upload_ tasks.
  ESP_LOGI(TAG, "SdLogger setup (stub - Phase 2 not yet implemented)");
}

void SdLogger::loop() {
  // Phase 2: attach sensor on_value callbacks once SNTP time becomes valid.
}

// -- Sink registration ---------------------------------------------------------

void SdLogger::add_numeric_sink(sensor::Sensor *sensor,
                                 const char *file_prefix,
                                 const char *header,
                                 const char *format,
                                 uint32_t log_interval_ms,
                                 bool force_write_on_change,
                                 uint8_t rotation,
                                 size_t max_file_size) {
  NumericSinkEntry entry;
  entry.sensor                     = sensor;
  entry.sink.file_prefix           = file_prefix;
  entry.sink.header                = header;
  entry.sink.format                = format;
  entry.sink.log_interval_ms       = log_interval_ms;
  entry.sink.force_write_on_change = force_write_on_change;
  entry.sink.rotation              = static_cast<RotationPolicy>(rotation);
  entry.sink.max_file_size         = max_file_size;
  this->numeric_sinks_.push_back(std::move(entry));
}

void SdLogger::add_text_sink(text_sensor::TextSensor *sensor,
                              const char *file_prefix,
                              const char *header,
                              uint32_t log_interval_ms,
                              bool force_write_on_change,
                              uint8_t rotation,
                              size_t max_file_size) {
  TextSinkEntry entry;
  entry.sensor                     = sensor;
  entry.sink.file_prefix           = file_prefix;
  entry.sink.header                = header;
  entry.sink.log_interval_ms       = log_interval_ms;
  entry.sink.force_write_on_change = force_write_on_change;
  entry.sink.rotation              = static_cast<RotationPolicy>(rotation);
  entry.sink.max_file_size         = max_file_size;
  this->text_sinks_.push_back(std::move(entry));
}

// -- Helpers -------------------------------------------------------------------

bool SdLogger::time_valid_() const {
  if (!this->time_) return false;
  return this->time_->now().is_valid();
}

void SdLogger::publish_sync_online_(bool v) {
  this->sync_online_ = v;
  if (this->sync_online_bs_) this->sync_online_bs_->publish_state(v);
}

void SdLogger::publish_sync_backlog_(bool v) {
  if (this->sync_sending_backlog_bs_)
    this->sync_sending_backlog_bs_->publish_state(v);
}

// -- FreeRTOS task stubs -------------------------------------------------------

void SdLogger::task_logging_entry_(void *param) {
  // Phase 2: pull LogPackets from queue, fopen in append mode, fprintf CSV
  //          row, fsync after every write, rotate files, update catalog.bin.
  (void)param;
  vTaskDelete(nullptr);
}

void SdLogger::task_upload_entry_(void *param) {
  // Phase 5: walk catalog.bin for CLOSED records, HTTP PUT each CSV file to
  //          upload_url_, mark UPLOADED on 2xx, exponential backoff on failure.
  (void)param;
  vTaskDelete(nullptr);
}

// -- HTTP stubs (Phase 5 restores full implementations) -----------------------

static void set_err_(std::string *out, const char *msg) {
  if (!out) return;
  if (!msg) { out->clear(); return; }
  out->assign(msg, std::min<size_t>(160, strlen(msg)));
}

bool SdLogger::http_request_(const char *url,
                              esp_http_client_method_t method,
                              const char *content_type,
                              const uint8_t *body, size_t body_len,
                              uint32_t timeout_ms,
                              int *http_status, std::string *resp_err) {
  (void)url; (void)method; (void)content_type;
  (void)body; (void)body_len; (void)timeout_ms;
  set_err_(resp_err, "not implemented");
  if (http_status) *http_status = -1;
  return false;
}

bool SdLogger::http_ping_(const char *url, uint32_t timeout_ms,
                           int *http_status, std::string *resp_err) {
  (void)url; (void)timeout_ms;
  set_err_(resp_err, "not implemented");
  if (http_status) *http_status = -1;
  return false;
}

bool SdLogger::send_http_put_(const std::string &body,
                               int *http_status, std::string *resp_err) {
  (void)body;
  set_err_(resp_err, "not implemented");
  if (http_status) *http_status = -1;
  return false;
}

bool SdLogger::send_http_ping_(int *http_status, std::string *resp_err) {
  set_err_(resp_err, "not implemented");
  if (http_status) *http_status = -1;
  return false;
}

}  // namespace sd_logger
}  // namespace esphome
