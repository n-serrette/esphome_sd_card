#include "sd_logger.h"

#include <cerrno>
#include <cstring>
#include <cstdio>
#include <ctime>
#include <map>

#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace sd_logger {

static const char *const TAG           = "sd_logger";
static const char *const CATALOG_REL   = "/catalog.bin";
static const char *const TASK_LOG_NAME = "sdlog_csv";

// -- Date helpers -------------------------------------------------------------

static void epoch_to_ymd(uint32_t ts, int *year, int *mon, int *day) {
  time_t t = static_cast<time_t>(ts);
  struct tm tm_info;
  gmtime_r(&t, &tm_info);
  if (year) *year = tm_info.tm_year + 1900;
  if (mon)  *mon  = tm_info.tm_mon + 1;
  if (day)  *day  = tm_info.tm_mday;
}

static uint32_t epoch_to_ymd_u32(uint32_t ts) {
  int y = 0, m = 0, d = 0;
  epoch_to_ymd(ts, &y, &m, &d);
  return static_cast<uint32_t>(y) * 10000u
       + static_cast<uint32_t>(m) * 100u
       + static_cast<uint32_t>(d);
}

// -- Catalog helpers ----------------------------------------------------------

// Scan catalog.bin on boot; any OPEN record -> CORRUPT (power-loss recovery).
static void catalog_scan_recover(const char *cat_path) {
  FILE *f = fopen(cat_path, "r+b");
  if (!f) return;  // first boot -- catalog does not exist yet
  CatalogRecord rec;
  uint32_t idx = 0;
  while (fread(&rec, sizeof(rec), 1, f) == 1) {
    if (rec.status == CATALOG_STATUS_OPEN) {
      ESP_LOGW(TAG, "Power-loss: record %u OPEN -> CORRUPT (%s)", idx, rec.filename);
      rec.status = CATALOG_STATUS_CORRUPT;
      fseek(f, static_cast<long>(idx) * static_cast<long>(sizeof(CatalogRecord)), SEEK_SET);
      fwrite(&rec, sizeof(rec), 1, f);
      fflush(f);
      fsync(fileno(f));
    }
    ++idx;
  }
  fclose(f);
}

// Append a new OPEN record.  Returns byte offset of the new record, or -1.
static long catalog_append_open(const char *cat_path, uint32_t ts, const char *rel_filename) {
  long offset = 0;
  {
    FILE *probe = fopen(cat_path, "rb");
    if (probe) {
      fseek(probe, 0, SEEK_END);
      offset = ftell(probe);
      fclose(probe);
    }
  }
  FILE *f = fopen(cat_path, "ab");
  if (!f) {
    ESP_LOGE(TAG, "catalog: fopen(ab) failed: %s (errno %d)", cat_path, errno);
    return -1;
  }
  CatalogRecord rec;
  memset(&rec, 0, sizeof(rec));
  rec.status          = CATALOG_STATUS_OPEN;
  rec.start_timestamp = ts;
  strlcpy(rec.filename, rel_filename, sizeof(rec.filename));
  if (fwrite(&rec, sizeof(rec), 1, f) != 1) {
    ESP_LOGE(TAG, "catalog: fwrite failed (errno %d)", errno);
    fclose(f);
    return -1;
  }
  fflush(f);
  fsync(fileno(f));
  fclose(f);
  return offset;
}

// Mark a record CLOSED and record its final file size.
static void catalog_update_closed(const char *cat_path, long offset, uint32_t file_size) {
  if (offset < 0) return;
  FILE *f = fopen(cat_path, "r+b");
  if (!f) return;
  if (fseek(f, offset, SEEK_SET) != 0) { fclose(f); return; }
  CatalogRecord rec;
  if (fread(&rec, sizeof(rec), 1, f) != 1) { fclose(f); return; }
  rec.status    = CATALOG_STATUS_CLOSED;
  rec.file_size = file_size;
  if (fseek(f, offset, SEEK_SET) == 0) {
    fwrite(&rec, sizeof(rec), 1, f);
    fflush(f);
    fsync(fileno(f));
  }
  fclose(f);
}

// -- ESPHome lifecycle --------------------------------------------------------

void SdLogger::setup() {
  this->log_queue_ = xQueueCreate(this->queue_size_, sizeof(LogPacket));
  if (!this->log_queue_) {
    ESP_LOGE(TAG, "Failed to create log queue (size=%u)", this->queue_size_);
    this->mark_failed();
    return;
  }

  if (this->sd_mmc_) {
    std::string cat_path = this->sd_mmc_->build_path(CATALOG_REL);
    catalog_scan_recover(cat_path.c_str());
  }

  BaseType_t ret = xTaskCreatePinnedToCore(
      task_logging_entry_,
      TASK_LOG_NAME,
      8192,
      this,
      this->task_priority_,
      &this->task_logging_,
      0);
  if (ret != pdPASS) {
    ESP_LOGE(TAG, "Failed to create logging task");
    this->mark_failed();
    return;
  }

  ESP_LOGI(TAG, "SdLogger ready -- queue %u packets, task prio %u",
           this->queue_size_, this->task_priority_);
}

void SdLogger::loop() {
  if (this->callbacks_attached_) return;
  if (!this->time_valid_()) return;

  for (auto &e : this->numeric_sinks_) {
    e.sensor->add_on_state_callback([this, &e](float value) {
      if (!this->log_queue_) return;
      auto t = this->time_->now();
      if (!t.is_valid()) return;  // drop packet if clock not set
      uint32_t now_ms = millis();
      bool enough_time = (now_ms - e.last_log_ms) >= e.sink.log_interval_ms;
      bool force = e.sink.force_write_on_change && (value != e.last_value);
      if (!enough_time && !force) return;
      LogPacket pkt;
      memset(&pkt, 0, sizeof(pkt));
      pkt.timestamp = static_cast<uint32_t>(t.timestamp);
      strlcpy(pkt.file_prefix, e.sink.file_prefix.c_str(), sizeof(pkt.file_prefix));
      snprintf(pkt.value, sizeof(pkt.value), e.sink.format.c_str(), value);
      if (xQueueSend(this->log_queue_, &pkt, 0) != pdTRUE)
        ESP_LOGW(TAG, "Queue full, dropping packet for %s", pkt.file_prefix);
      e.last_log_ms = now_ms;
      e.last_value  = value;
    });
  }

  for (auto &e : this->text_sinks_) {
    e.sensor->add_on_state_callback([this, &e](std::string value) {
      if (!this->log_queue_) return;
      auto t = this->time_->now();
      if (!t.is_valid()) return;
      uint32_t now_ms = millis();
      bool enough_time = (now_ms - e.last_log_ms) >= e.sink.log_interval_ms;
      bool force = e.sink.force_write_on_change && (value != e.last_value);
      if (!enough_time && !force) return;
      LogPacket pkt;
      memset(&pkt, 0, sizeof(pkt));
      pkt.timestamp = static_cast<uint32_t>(t.timestamp);
      strlcpy(pkt.file_prefix, e.sink.file_prefix.c_str(), sizeof(pkt.file_prefix));
      strlcpy(pkt.value, value.c_str(), sizeof(pkt.value));
      if (xQueueSend(this->log_queue_, &pkt, 0) != pdTRUE)
        ESP_LOGW(TAG, "Queue full, dropping packet for %s", pkt.file_prefix);
      e.last_log_ms = now_ms;
      e.last_value  = value;
    });
  }

  this->callbacks_attached_ = true;
  ESP_LOGI(TAG, "Callbacks attached: %u numeric, %u text",
           this->numeric_sinks_.size(), this->text_sinks_.size());
}

// -- Sink registration --------------------------------------------------------

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

// -- Helpers ------------------------------------------------------------------

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

// -- FreeRTOS CSV logging task -----------------------------------------------

void SdLogger::task_logging_entry_(void *param) {
  SdLogger *self = static_cast<SdLogger *>(param);

  // Build prefix -> sink config lookup (read-only after setup, no lock needed)
  std::map<std::string, const LogSink *> sink_map;
  for (const auto &e : self->numeric_sinks_)
    sink_map[e.sink.file_prefix] = &e.sink;
  for (const auto &e : self->text_sinks_)
    sink_map[e.sink.file_prefix] = &e.sink;

  const std::string cat_path = self->sd_mmc_->build_path(CATALOG_REL);

  // Per-prefix open file context (task-local, no sharing with other tasks)
  struct OpenFileCtx {
    FILE    *fp{nullptr};
    char     abs_path[96];
    uint32_t ymd{0};           // YYYYMMDD used for DAILY rotation
    size_t   bytes_written{0};
    long     catalog_offset{-1};
  };
  std::map<std::string, OpenFileCtx> open_files;

  LogPacket pkt;
  while (true) {
    if (xQueueReceive(self->log_queue_, &pkt, portMAX_DELAY) != pdTRUE) continue;

    auto it = sink_map.find(pkt.file_prefix);
    if (it == sink_map.end()) {
      ESP_LOGW(TAG, "Unknown file_prefix in queue: %.32s", pkt.file_prefix);
      continue;
    }
    const LogSink *sink = it->second;
    OpenFileCtx   &ctx  = open_files[pkt.file_prefix];

    // -- Rotation check -------------------------------------------------------
    const uint32_t cur_ymd = epoch_to_ymd_u32(pkt.timestamp);
    bool need_rotate = false;
    if (ctx.fp != nullptr) {
      if (sink->rotation == RotationPolicy::DAILY)
        need_rotate = (cur_ymd != ctx.ymd);
      else
        need_rotate = (ctx.bytes_written >= sink->max_file_size);
    }

    if (need_rotate) {
      fclose(ctx.fp);
      ctx.fp = nullptr;
      catalog_update_closed(cat_path.c_str(), ctx.catalog_offset,
                            static_cast<uint32_t>(ctx.bytes_written));
      ESP_LOGI(TAG, "Rotated: %s (%u B)", ctx.abs_path,
               static_cast<unsigned>(ctx.bytes_written));
    }

    // -- Open new file if needed ----------------------------------------------
    if (ctx.fp == nullptr) {
      char rel_path[64];
      if (sink->rotation == RotationPolicy::DAILY) {
        int y = 0, m = 0, d = 0;
        epoch_to_ymd(pkt.timestamp, &y, &m, &d);
        snprintf(rel_path, sizeof(rel_path), "/%s_%04d-%02d-%02d.csv",
                 pkt.file_prefix, y, m, d);
      } else {
        // SIZE rotation: use creation epoch as unique suffix
        snprintf(rel_path, sizeof(rel_path), "/%s_%u.csv",
                 pkt.file_prefix, pkt.timestamp);
      }

      long offset = catalog_append_open(cat_path.c_str(), pkt.timestamp, rel_path);
      ctx.catalog_offset = offset;
      ctx.ymd            = cur_ymd;
      ctx.bytes_written  = 0;

      std::string abs = self->sd_mmc_->build_path(rel_path);
      strlcpy(ctx.abs_path, abs.c_str(), sizeof(ctx.abs_path));

      ctx.fp = fopen(ctx.abs_path, "a");
      if (!ctx.fp) {
        ESP_LOGE(TAG, "fopen failed: %s (errno %d)", ctx.abs_path, errno);
        continue;
      }

      // Initialise bytes_written from the real file position so that SIZE
      // rotation thresholds and catalog file_size are accurate on reopen.
      fseek(ctx.fp, 0, SEEK_END);
      long existing_size = ftell(ctx.fp);
      ctx.bytes_written = (existing_size > 0) ? static_cast<size_t>(existing_size) : 0;

      // Write header only if the file is new (empty)
      if (existing_size == 0) {
        int hlen = fprintf(ctx.fp, "%s\n", sink->header.c_str());
        if (hlen > 0) {
          fflush(ctx.fp);
          fsync(fileno(ctx.fp));
          ctx.bytes_written += static_cast<size_t>(hlen);
        }
      }
      ESP_LOGI(TAG, "Opened: %s", ctx.abs_path);
    }

    // -- Write CSV row --------------------------------------------------------
    int written = fprintf(ctx.fp, "%u,%s\n", pkt.timestamp, pkt.value);
    if (written > 0) {
      fflush(ctx.fp);
      fsync(fileno(ctx.fp));
      ctx.bytes_written += static_cast<size_t>(written);
    } else {
      ESP_LOGE(TAG, "fprintf failed: %s (errno %d)", ctx.abs_path, errno);
    }
  }
  // Never reached; FreeRTOS tasks must not return.
  vTaskDelete(nullptr);
}

void SdLogger::task_upload_entry_(void *param) {
  // Phase 5: walk catalog.bin for CLOSED records, HTTP PUT each CSV file,
  //          mark UPLOADED on 2xx, exponential backoff on failure.
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
                              int method,
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
