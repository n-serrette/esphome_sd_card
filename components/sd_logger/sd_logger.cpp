#include "sd_logger.h"

#include <cerrno>
#include <cstring>
#include <cstdio>
#include <ctime>
#include <cmath>
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

// -- Directory helpers -------------------------------------------------------

// Recursively ensure all components of a relative SD path exist.
// rel_path must be like "logs" or "logs/vehicle" (no leading slash, no
// /sdcard prefix).  Uses sd_card_->create_directory() which calls f_mkdir
// directly, bypassing the broken POSIX mkdir stub on this toolchain.
void SdLogger::make_dirs_(const std::string &rel_path) {
  // Walk each slash-separated prefix and create it if it doesn't exist.
  std::string::size_type pos = 0;
  while (pos != std::string::npos) {
    pos = rel_path.find('/', pos + 1);
    std::string part = "/" + rel_path.substr(0, pos == std::string::npos ? rel_path.size() : pos);
    if (!this->sd_card_->is_directory(part)) {
      this->sd_card_->create_directory(part.c_str());
    }
  }
}

// -- ESPHome lifecycle --------------------------------------------------------

void SdLogger::setup() {
  this->log_queue_ = xQueueCreate(this->queue_size_, sizeof(LogPacket));
  if (!this->log_queue_) {
    ESP_LOGE(TAG, "Failed to create log queue (size=%u)", this->queue_size_);
    this->mark_failed();
    return;
  }

  if (this->sd_card_) {
    std::string cat_path = this->sd_card_->build_path(CATALOG_REL);
    catalog_scan_recover(cat_path.c_str());

    // Create base log directory
    make_dirs_(this->path_);

    // Per-log: compute subdir, create it, auto-generate header
    for (auto &entry : this->logs_) {
      // Build subdir: path/folder or just path if folder is empty
      if (entry.config.folder.empty()) {
        entry.config.subdir = this->path_;
      } else {
        entry.config.subdir = this->path_ + "/" + entry.config.folder;
      }
      make_dirs_(entry.config.subdir);
      ESP_LOGI(TAG, "Log '%s' -> /%s", entry.config.file_prefix.c_str(), entry.config.subdir.c_str());

      // Auto-generate header if blank
      if (entry.config.header.empty()) {
        std::string h = "timestamp";
        for (const auto &slot : entry.config.slots) {
          h += ",";
          char id_buf[128] = {};
          if (slot.type == SensorSlot::Type::NUMERIC)
            slot.numeric_sensor->write_object_id_to(id_buf, sizeof(id_buf));
          else
            slot.text_sensor->write_object_id_to(id_buf, sizeof(id_buf));
          h += id_buf;
        }
        entry.config.header = h;
      }
    }
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

  ESP_LOGI(TAG, "SdLogger ready -- %u logs, queue %u packets, task prio %u",
           this->logs_.size(), this->queue_size_, this->task_priority_);
}

void SdLogger::loop() {
  if (!this->log_queue_) return;

  const uint32_t now_ms = millis();

  // Avoid the RTC read entirely when no log is due
  bool any_due = false;
  for (const auto &entry : this->logs_) {
    if ((now_ms - entry.last_log_ms) >= entry.config.log_interval_ms) {
      any_due = true;
      break;
    }
  }
  if (!any_due) return;

  if (!this->time_valid_()) return;
  auto t = this->time_->now();
  if (!t.is_valid()) return;

  const uint32_t ts = static_cast<uint32_t>(t.timestamp);

  for (auto &entry : this->logs_) {
    if ((now_ms - entry.last_log_ms) < entry.config.log_interval_ms) continue;

    // Build the CSV row: timestamp,val1,val2,...
    char row[LOG_PACKET_ROW_LEN];
    int  pos = snprintf(row, sizeof(row), "%u", ts);

    for (const auto &slot : entry.config.slots) {
      if (pos >= static_cast<int>(sizeof(row)) - 2) break;  // guard overflow
      row[pos++] = ',';
      if (slot.type == SensorSlot::Type::NUMERIC) {
        float v = slot.numeric_sensor->state;
        if (std::isnan(v)) {
          row[pos] = '\0';  // empty field
        } else {
          pos += snprintf(row + pos, sizeof(row) - pos, slot.format.c_str(), v);
        }
      } else {
        const std::string &sv = slot.text_sensor->state;
        size_t remain = sizeof(row) - pos;
        strlcpy(row + pos, sv.c_str(), remain);
        pos += static_cast<int>(sv.size() < remain ? sv.size() : remain - 1);
      }
    }

    LogPacket pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.timestamp = ts;
    strlcpy(pkt.file_prefix, entry.config.file_prefix.c_str(), sizeof(pkt.file_prefix));
    strlcpy(pkt.row, row, sizeof(pkt.row));

    if (xQueueSend(this->log_queue_, &pkt, 0) != pdTRUE)
      ESP_LOGW(TAG, "Queue full, dropping packet for %s", pkt.file_prefix);

    entry.last_log_ms = now_ms;
  }
}

// -- Log registration ---------------------------------------------------------

void SdLogger::begin_log(const char *name, const char *folder, const char *file_prefix,
                          const char *header, uint32_t interval_ms,
                          uint8_t rotation, size_t max_file_size) {
  this->pending_log_ = new LogEntry();
  this->pending_log_->config.name            = name;
  this->pending_log_->config.folder          = folder;
  this->pending_log_->config.file_prefix     = file_prefix;
  this->pending_log_->config.header          = header;
  this->pending_log_->config.log_interval_ms = interval_ms;
  this->pending_log_->config.rotation        = static_cast<RotationPolicy>(rotation);
  this->pending_log_->config.max_file_size   = max_file_size;
}

void SdLogger::add_log_numeric_slot(sensor::Sensor *s, const char *format) {
  if (!this->pending_log_) return;
  SensorSlot slot;
  slot.type           = SensorSlot::Type::NUMERIC;
  slot.numeric_sensor = s;
  slot.format         = format;
  this->pending_log_->config.slots.push_back(std::move(slot));
}

void SdLogger::add_log_text_slot(text_sensor::TextSensor *s) {
  if (!this->pending_log_) return;
  SensorSlot slot;
  slot.type        = SensorSlot::Type::TEXT;
  slot.text_sensor = s;
  this->pending_log_->config.slots.push_back(std::move(slot));
}

void SdLogger::finalize_log() {
  if (!this->pending_log_) return;
  this->logs_.push_back(std::move(*this->pending_log_));
  delete this->pending_log_;
  this->pending_log_ = nullptr;
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

  // Build prefix -> LogConfig lookup (read-only after setup, no lock needed)
  std::map<std::string, const LogConfig *> sink_map;
  for (const auto &entry : self->logs_)
    sink_map[entry.config.file_prefix] = &entry.config;

  const std::string cat_path = self->sd_card_->build_path(CATALOG_REL);

  // Per-prefix open file context (task-local, no sharing with other tasks)
  struct OpenFileCtx {
    FILE    *fp{nullptr};
    char     abs_path[96];
    uint32_t ymd{0};              // YYYYMMDD used for DAILY rotation
    size_t   bytes_written{0};
    long     catalog_offset{-1};
    TickType_t last_fsync_tick{0};
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
    const LogConfig *cfg = it->second;
    OpenFileCtx     &ctx = open_files[pkt.file_prefix];

    // -- Rotation check -------------------------------------------------------
    const uint32_t cur_ymd = epoch_to_ymd_u32(pkt.timestamp);
    bool need_rotate = false;
    if (ctx.fp != nullptr) {
      if (cfg->rotation == RotationPolicy::DAILY)
        need_rotate = (cur_ymd != ctx.ymd);
      else
        need_rotate = (ctx.bytes_written >= cfg->max_file_size);
    }

    if (need_rotate) {
      fflush(ctx.fp);
      fsync(fileno(ctx.fp));
      fclose(ctx.fp);
      ctx.fp = nullptr;
      catalog_update_closed(cat_path.c_str(), ctx.catalog_offset,
                            static_cast<uint32_t>(ctx.bytes_written));
      ESP_LOGI(TAG, "Rotated: %s (%u B)", ctx.abs_path,
               static_cast<unsigned>(ctx.bytes_written));
    }

    // -- Open new file if needed ----------------------------------------------
    if (ctx.fp == nullptr) {
      char rel_path[96];
      if (cfg->rotation == RotationPolicy::DAILY) {
        int y = 0, m = 0, d = 0;
        epoch_to_ymd(pkt.timestamp, &y, &m, &d);
        snprintf(rel_path, sizeof(rel_path), "/%s/%s_%04d-%02d-%02d.csv",
                 cfg->subdir.c_str(), cfg->file_prefix.c_str(), y, m, d);
      } else {
        // SIZE rotation: use creation epoch as unique suffix
        snprintf(rel_path, sizeof(rel_path), "/%s/%s_%u.csv",
                 cfg->subdir.c_str(), cfg->file_prefix.c_str(), pkt.timestamp);
      }

      long offset = catalog_append_open(cat_path.c_str(), pkt.timestamp, rel_path);
      ctx.catalog_offset = offset;
      ctx.ymd            = cur_ymd;
      ctx.bytes_written  = 0;

      std::string abs = self->sd_card_->build_path(rel_path);
      strlcpy(ctx.abs_path, abs.c_str(), sizeof(ctx.abs_path));

      ctx.fp = fopen(ctx.abs_path, "a");
      if (!ctx.fp) {
        ESP_LOGE(TAG, "fopen failed: %s (errno %d)", ctx.abs_path, errno);
        continue;
      }

      // Initialise bytes_written from real file position so SIZE rotation and
      // catalog file_size are accurate on reopen.
      fseek(ctx.fp, 0, SEEK_END);
      long existing_size = ftell(ctx.fp);
      ctx.bytes_written = (existing_size > 0) ? static_cast<size_t>(existing_size) : 0;

      // Write header only if the file is new (empty)
      if (existing_size == 0) {
        int hlen = fprintf(ctx.fp, "%s\n", cfg->header.c_str());
        if (hlen > 0) {
          fflush(ctx.fp);
          fsync(fileno(ctx.fp));
          ctx.bytes_written += static_cast<size_t>(hlen);
        }
      }
      ESP_LOGI(TAG, "Opened: %s", ctx.abs_path);
      ctx.last_fsync_tick = xTaskGetTickCount();
    }

    // -- Write CSV row --------------------------------------------------------
    int written = fprintf(ctx.fp, "%s\n", pkt.row);
    if (written > 0) {
      TickType_t now_tick = xTaskGetTickCount();
      if ((now_tick - ctx.last_fsync_tick) >= pdMS_TO_TICKS(self->fsync_interval_ms_)) {
        fflush(ctx.fp);
        fsync(fileno(ctx.fp));
        ctx.last_fsync_tick = now_tick;
      }
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
