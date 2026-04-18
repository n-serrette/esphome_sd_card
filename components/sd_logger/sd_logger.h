#pragma once

#include "esphome/core/component.h"
#include "esphome/core/time.h"
#include "esphome/components/time/real_time_clock.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "../sd_card/sd_card.h"

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
// Fixed-size POD struct placed on the FreeRTOS queue by loop() (producer).
// Must stay POD so xQueueSend copies it by value.
static constexpr size_t LOG_PACKET_PREFIX_LEN = 32;
static constexpr size_t LOG_PACKET_ROW_LEN    = 256;  // full CSV row incl. timestamp

struct LogPacket {
  uint32_t timestamp;                          // Unix epoch — used for rotation decisions
  char     file_prefix[LOG_PACKET_PREFIX_LEN]; // routing key in task, null-terminated
  char     row[LOG_PACKET_ROW_LEN];            // complete CSV row e.g. "1713149600,6000,95.3,D"
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

// ── SensorSlot ────────────────────────────────────────────────────────────────
// One column entry within a LogConfig.
struct SensorSlot {
  enum class Type : uint8_t { NUMERIC, TEXT };
  Type                     type;
  sensor::Sensor          *numeric_sensor{nullptr};
  text_sensor::TextSensor *text_sensor{nullptr};
  std::string              format;   // printf format string, e.g. "%.2f" (numeric only)
};

// ── LogConfig ─────────────────────────────────────────────────────────────────
// Static per-log configuration set during setup(); read-only in FreeRTOS task.
struct LogConfig {
  std::string             name;
  std::string             folder;
  std::string             subdir;          // precomputed: path/folder — used by task
  std::string             file_prefix;
  std::string             header;          // "" = auto-generated in setup()
  uint32_t                log_interval_ms;
  RotationPolicy          rotation;
  size_t                  max_file_size;   // bytes; only evaluated when rotation == SIZE
  std::vector<SensorSlot> slots;
};

// ── LogEntry ──────────────────────────────────────────────────────────────────
// Owned by SdLogger; loop()-side only (no task access).
struct LogEntry {
  LogConfig config;
  uint32_t  last_log_ms{0};   // millis() of last queue push
};

// ── SdLogger ──────────────────────────────────────────────────────────────────
class SdLogger : public Component {
 public:
  // ── Hardware + time wiring ──────────────────────────────────────────────────
  void set_sd_card(sd_card::SdCard *sd) { this->sd_card_ = sd; }
  void set_time(time::RealTimeClock *t) { this->time_ = t; }

  // ── FreeRTOS queue / task tuning ────────────────────────────────────────────
  void set_queue_size(uint8_t n) { this->queue_size_ = n; }
  void set_task_priority(uint8_t p) { this->task_priority_ = p; }
  void set_fsync_interval_ms(uint32_t ms) { this->fsync_interval_ms_ = ms; }

  // ── Base log path ────────────────────────────────────────────────────────────
  void set_path(const std::string &p) { this->path_ = p; }

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

  // ── Log registration — called from generated __init__.py code ───────────────
  void begin_log(const char *name, const char *folder, const char *file_prefix,
                 const char *header, uint32_t interval_ms,
                 uint8_t rotation, size_t max_file_size);
  void add_log_numeric_slot(sensor::Sensor *s, const char *format);
  void add_log_text_slot(text_sensor::TextSensor *s);
  void finalize_log();

  // ── Queue accessor ───────────────────────────────────────────────────────────
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

  // ── Directory helper ─────────────────────────────────────────────────────────
  void make_dirs_(const std::string &rel_path);  // recursive mkdir via sd_card_

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
  sd_card::SdCard       *sd_card_{nullptr};
  time::RealTimeClock *time_{nullptr};

  // ── FreeRTOS ─────────────────────────────────────────────────────────────────
  QueueHandle_t log_queue_{nullptr};
  TaskHandle_t  task_logging_{nullptr};
  TaskHandle_t  task_upload_{nullptr};
  uint8_t       queue_size_{50};
  uint8_t       task_priority_{1};
  uint32_t      fsync_interval_ms_{30000};  // ms between fsync calls per file

  // ── Log storage ───────────────────────────────────────────────────────────────
  std::vector<LogEntry> logs_;
  LogEntry             *pending_log_{nullptr};  // temporary during begin/finalize
  std::string           path_{"logs"};          // base SD subdirectory for all logs

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
};

}  // namespace sd_logger
}  // namespace esphome