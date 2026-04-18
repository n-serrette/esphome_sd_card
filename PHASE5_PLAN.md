# Phase 5 — Cloud Upload Task

## Goal
Implement `SdLogger::task_upload_entry_()` in `sd_logger/sd_logger.cpp`.

---

## Behaviour

1. **Guard clause** — if `upload_url_` is empty, log a warning and `vTaskDelete(nullptr)` immediately. All upload config keys are optional in YAML.

2. **Connectivity ping loop**
   - Periodically (every `ping_interval_ms_`) call `http_ping_()` against `ping_url_`
   - On success → `publish_sync_online_(true)`
   - On failure → `publish_sync_online_(false)`, sleep and retry

3. **Catalog walk**
   - Open `catalog.bin` in `"rb"` mode
   - Read records sequentially; for each record with `status == CATALOG_STATUS_CLOSED`:
     - Build the absolute path via `sd_card_->build_path(rec.filename)`
     - Attempt HTTP PUT (see below)
     - On 2xx → update record in-place to `CATALOG_STATUS_UPLOADED`, fsync catalog
     - On failure → increment backoff, skip to next record, retry this one next pass

4. **HTTP PUT implementation** (`send_http_put_` / `http_request_`)
   - Restore full `esp_http_client` implementation (was stub)
   - Stream the CSV file body from SD card in chunks (e.g. 4 KB) using `esp_http_client_write`
   - Set `Authorization: Bearer <bearer_token_>` header
   - Set `Content-Type: text/csv`
   - Set `Content-Length` from `rec.file_size`
   - Timeout: configurable (suggest reusing `ping_timeout_ms_` or add dedicated key)

5. **Exponential backoff**
   - Starts at `backoff_initial_ms_` (default 30 s)
   - Doubles on each consecutive failure
   - Caps at `backoff_max_ms_` (default 15 min)
   - Resets to `backoff_initial_ms_` on first success

6. **`sync_sending_backlog_` sensor**
   - Set `true` when at least one CLOSED record exists at start of pass
   - Set `false` when pass completes with no remaining CLOSED records

---

## Files to change

| File | Change |
|---|---|
| `sd_logger/sd_logger.cpp` | Replace `task_upload_entry_` stub; implement `http_request_`, `http_ping_`, `send_http_put_`, `send_http_ping_` |
| `sd_logger/sd_logger.h` | Add `set_upload_task_stack_size()` setter if stack tuning needed; no structural changes expected |
| `sd_logger/__init__.py` | Launch `task_upload_` in `setup()` only when `upload_url` is configured (already guarded at runtime, but can skip task creation entirely) |

---

## Task creation (in `setup()`)

Currently `task_upload_` is never created. Add a second `xTaskCreatePinnedToCore` call after the logging task, pinned to core 0, priority `task_priority_ - 1` (lower than logger), stack 8 KB.

```cpp
if (!this->upload_url_.empty()) {
  xTaskCreatePinnedToCore(
      task_upload_entry_,
      "sdlog_upload",
      8192,
      this,
      this->task_priority_ > 0 ? this->task_priority_ - 1 : 0,
      &this->task_upload_,
      0);
}
```

---

## Notes

- The upload task must **not** close or modify any file that the logging task currently has open. Since the logging task only holds OPEN-status files, and the upload task only touches CLOSED-status files, there is no contention — no mutex required.
- `esp_http_client` must be initialised and cleaned up within the task (not in `setup()`).
- If the SD card is unmounted or `sd_card_` reports not ready, skip the pass and backoff.
