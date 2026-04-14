#pragma once

#include "esphome/core/component.h"
#include "esphome/core/time.h"
#include "esphome/components/time/real_time_clock.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "../sd_mmc/sd_mmc.h"

#include <vector>
#include <string>

extern "C" {
  #include "freertos/FreeRTOS.h"
  #include "freertos/task.h"
  #include "freertos/queue.h"
  #include "esp_system.h"
}

namespace esphome {
namespace sd_logger {

// ── LogPacket ─────────────────────────────────────────────────────────────────
// Fixed-size POD struct placed on the FreeRTOS queue by sensor on_value
// callbacks (producer).  Must stay POD so xQueueSend copies it by value.
static constexpr size_t LOG_PACKET_PREFIX_LEN = 32;
static constexpr size_t LOG_PACKET_VALUE_LEN  = 48;

struct LogPacket {
  uint32_t timestamp;                          // Unix epoch of the reading
  char     file_prefix[LOG_PACKET_PREFIX_LEN]; // sink file_prefix, null-terminated
  char     value[LOG_PACKET_VALUE_LEN];        // pre-formatted value string
};

// ── CatalogRecord ─────────────────────────────────────────────────────────────
// Exactly 64 bytes.  Written sequentially to /sdcard/catalog.bin for O(1)
// seeks when listing files.  Status byte acts as a "dirty bit":
//   0x01 = Open     — set before fopen; if seen on boot → power-loss recovery
//   0x02 = Closed   — fsynced and fclose'd normally
//   0x03 = Corrupt  — was Open on boot but file incomplete or missing
//   0x04 = Uploaded — cloud HTTP PUT confirmed with a 2xx response
static constexpr uint8_t CATALOG_STATUS_OPEN     = 0x01;
static constexpr uint8_t CATALOG_STATUS_CLOSED   = 0x02;
static constexpr uint8_t CATALOG_STATUS_CORRUPT  = 0x03;
static constexpr uint8_t CATALOG_STATUS_UPLOADED = 0x04;

#pragma pack(push, 1)
struct CatalogRecord {
  uint8_t  status;          //  1 byte
  uint32_t start_timestamp; //  4 bytes — Unix epoch when the file was created
  char     filename[32];    // 32 bytes — null-terminated relative filename
  uint32_t file_size;       //  4 bytes — final size in bytes (0 while open)
  uint8_t  reserved[23];    // 23 bytes — reserved for CRC32 / future metadata
  // Total: 1 + 4 + 32 + 4 + 23 = 64 bytes
};
#pragma pack(pop)

static_assert(sizeof(CatalogRecord) == 64, "CatalogRecord must be exactly 64 bytes");

// ── RotationPolicy ────────────────────────────────────────────────────────────
enum class RotationPolicy : uint8_t {
  DAILY = 0, // New file on UTC date change
  SIZE  = 1, // New file when current file exceeds max_file_size bytes
};

// ── LogSink ───────────────────────────────────────────────────────────────────
// Per-sensor logging configuration stored in the hub at initialisation time.
struct LogSink {
  std::string    file_prefix;
  std::string    header;           // CSV header line written once per new file
  std::string    format;           // printf-style format string, e.g. "%.2f"
  uint32_t       log_interval_ms;
  bool           force_write_on_change;
  RotationPolicy rotation;
  size_t         max_file_size;    // bytes; only evaluated when rotation == SIZE
};

// ── SdLogger ──────────────────────────────────────────────────────────────────
class SdLogger : public Component {
 public:
  // ── Hardware + time wiring ──────────────────────────────────────────────────
  void set_sd_mmc(sd_mmc::SdMmc *sd) { this->sd_mmc_ = sd; }
  void set_time(time::RealTimeClock *t) { this->time_ = t; }

  // ── FreeRTOS queue / task tuning ────────────────────────────────────────────
  void set_queue_size(uint8_t n) { this->queue_size_ = n; }
  void set_task_priority(uint8_t p) { this->task_priority_ = p; }
  void set_fsync_interval_ms(uint32_t ms) { this->fsync_interval_ms_ = ms; }

  // ── Cloud upload (optional; retained from previous design) ──────────────────
  void set_upload_url(const std::string &u) { this->upload_url_ = u; }
  void set_bearer_token(const std::string &t) { this->bearer_token_ = t; }
  void set_backoff_initial_ms(uint32_t ms) { this->backoff_initial_ms_ = ms; }
  void set_backoff_max_ms(uint32_t ms) { this->backoff_max_ms_ = ms; }
  void set_ping_url(const std::string &u) { this->ping_url_ = u; }
  void set_ping_interval_ms(uint32_t ms) { this->ping_interval_ms_ = ms; }
  void set_ping_timeout_ms(uint32_t ms) { this->ping_timeout_ms_ = ms; }

  // ── Status binary sensors ────────────────────────────────────────────────────
  void set_sync_online_binary_sensor(binary_sensor::BinarySensor *b) { this->sync_online_bs_ = b; }
  void set_sync_sending_backlog_binary_sensor(binary_sensor::BinarySensor *b) { this->sync_sending_backlog_bs_ = b; }

  // ── Sink registration — called from generated platform code ─────────────────
  // Registers a numeric (float) sensor for CSV logging.
  void add_numeric_sink(sensor::Sensor *sensor,
                        const char *file_prefix,
                        const char *header,
                        const char *format,
                        uint32_t log_interval_ms,
                        bool force_write_on_change,
                        uint8_t rotation,
                        size_t max_file_size);

  // Registers a text sensor for CSV logging.
  void add_text_sink(text_sensor::TextSensor *sensor,
                     const char *file_prefix,
                     const char *header,
                     uint32_t log_interval_ms,
                     bool force_write_on_change,
                     uint8_t rotation,
                     size_t max_file_size);

  // ── Queue accessor used by sensor on_value lambdas ───────────────────────────
  QueueHandle_t get_log_queue() const { return this->log_queue_; }

  // ── ESPHome lifecycle ────────────────────────────────────────────────────────
  void setup() override;
  void loop() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

 protected:
  // ── Helpers ──────────────────────────────────────────────────────────────────
  bool time_valid_() const;
  void publish_sync_online_(bool v);
  void publish_sync_backlog_(bool v);

  // ── FreeRTOS task entry points ───────────────────────────────────────────────
  static void task_logging_entry_(void *param); // Phase 2: CSV append + fsync
  static void task_upload_entry_(void *param);  // Phase 5: catalog walk + HTTP PUT

  // ── HTTP helpers (retained; used by upload task in Phase 5) ──────────────────
  bool http_request_(const char *url,
                     int method,
                     const char *content_type,
                     const uint8_t *body, size_t body_len,
                     uint32_t timeout_ms,
                     int *http_status, std::string *resp_err);
  bool http_ping_(const char *url, uint32_t timeout_ms,
                  int *http_status, std::string *resp_err);
  bool send_http_put_(const std::string &body, int *http_status, std::string *resp_err);
  bool send_http_ping_(int *http_status, std::string *resp_err);

  // ── Hardware ──────────────────────────────────────────────────────────────────
  sd_mmc::SdMmc       *sd_mmc_{nullptr};
  time::RealTimeClock *time_{nullptr};

  // ── FreeRTOS ─────────────────────────────────────────────────────────────────
  QueueHandle_t log_queue_{nullptr};
  TaskHandle_t  task_logging_{nullptr};
  TaskHandle_t  task_upload_{nullptr};
  uint8_t       queue_size_{50};
  uint8_t       task_priority_{1};
  uint32_t      fsync_interval_ms_{30000};  // ms between fsync calls per file

  // ── Sink storage ──────────────────────────────────────────────────────────────
  struct NumericSinkEntry {
    sensor::Sensor *sensor;
    LogSink         sink;
    uint32_t        last_log_ms{0};
    bool            has_logged{false};
    float           last_value{0.0f};
  };
  struct TextSinkEntry {
    text_sensor::TextSensor *sensor;
    LogSink                  sink;
    uint32_t                 last_log_ms{0};
    bool                     has_logged{false};
    std::string              last_value;
  };
  std::vector<NumericSinkEntry> numeric_sinks_;
  std::vector<TextSinkEntry>    text_sinks_;

  // ── Cloud upload config ───────────────────────────────────────────────────────
  std::string upload_url_;
  std::string bearer_token_;
  uint32_t    backoff_initial_ms_{30000};
  uint32_t    backoff_max_ms_{15 * 60 * 1000};
  std::string ping_url_;
  uint32_t    ping_interval_ms_{10000};
  uint32_t    ping_timeout_ms_{3000};
  bool        sync_online_{false};

  // ── Binary sensors ────────────────────────────────────────────────────────────
  binary_sensor::BinarySensor *sync_online_bs_{nullptr};
  binary_sensor::BinarySensor *sync_sending_backlog_bs_{nullptr};

  // ── Loop state ────────────────────────────────────────────────────────────────
  bool callbacks_attached_{false};
};

}  // namespace sd_logger
}  // namespace esphome