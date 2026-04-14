# Test Plan — ESPHome SD Card Black Box

## Hardware required
- ESP32-S3 dev board
- SD card (≥ 4 GB, FAT32 formatted)
- USB serial monitor (115200 baud)
- Wi-Fi AP reachable from the board
- Optional: logic analyser on SDMMC data lines
- Optional: second device to act as fake HTTP upload endpoint (e.g. `python -m http.server` or `ngrok` + request bin)

---

## T1 — Baseline: SD card mounts cleanly

**Goal:** Confirm `sd_mmc` initialises without error.

**Steps:**
1. Flash with minimal YAML — `sd_mmc` only, no logger or webserver.
2. Check serial log for `[sd_mmc] Card type:` line and capacity report.

**Pass:** No `ERR_MOUNT` / `ERR_NO_CARD` log entries; `sd_card_type` text sensor populates.

---

## T2 — Sensor callbacks attach after SNTP sync

**Goal:** Verify `loop()` only installs callbacks once the clock is valid.

**Steps:**
1. Flash full YAML with at least one numeric and one text sensor.
2. Observe serial log — confirm `Callbacks attached` log line appears **after** the SNTP sync message (search for `sntp` / `synchronized`).
3. Verify the message shows the correct count: `Callbacks attached: N numeric, M text`.

**Pass:** Log line appears after clock is valid; appears exactly once; correct counts.

---

## T3 — CSV file creation and correct headers

**Goal:** Files are created at the right path with correct header line.

**Steps:**
1. After T2 passes, wait 30 s for sensor data.
2. Remove SD card, inspect on PC.
3. Confirm file exists at `/<file_prefix>_YYYY-MM-DD.csv` (DAILY) or `/<file_prefix>_<epoch>.csv` (SIZE).
4. Open file; row 1 must match the `header:` value in YAML exactly.
5. Row 2+ must be `<unix_timestamp>,<value>`.

**Pass:** Correct path, correct header, well-formed rows.

---

## T4 — Non-blocking: main task loop latency under SD write load

**Goal:** SD writes happen in the consumer task and do not stall the main ESPHome loop.

**Steps:**
1. Add a `time.on_time` lambda that logs `millis()` every 100 ms to the ESPHome log.
2. Simultaneously drive sensors to fire at maximum rate (set `log_interval: 1s` across 5+ sensors).
3. Watch serial output — measure gap between the 100 ms timer log lines.

**Pass:** Timer fires within ±10 ms of 100 ms target throughout; no multi-hundred-millisecond stalls visible in log.

**What to look for if failing:** Consecutive timer lines separated by > 200 ms indicates SD I/O has leaked back to the main task.

---

## T5 — Queue back-pressure: drop without blocking

**Goal:** When the queue is full, new packets are dropped with a warning and the callback returns immediately.

**Steps:**
1. Temporarily reduce `queue_size: 5` in YAML.
2. Set 10 sensors all firing at `log_interval: 500ms`.
3. Look for `Queue full, dropping packet` log lines.
4. Confirm the timestamp timer from T4 still has low jitter.

**Pass:** Drop warnings appear; main loop latency unaffected.

---

## T6 — DAILY rotation

**Goal:** A new file is created at UTC midnight and the old one is marked CLOSED in `catalog.bin`.

**Steps:**
1. Set system clock to 23:59:45 UTC via SNTP (or temporarily spoof by adjusting `sntp_server` to a time well ahead).
2. Let the device run past 00:00:00.
3. Remove SD card; verify two files exist for the two dates.
4. Inspect `catalog.bin` in a hex editor:
   - Record 0: status byte `0x02` (CLOSED), non-zero `file_size`.
   - Record 1: status byte `0x02` or `0x01` (OPEN if still writing).

**Pass:** Two CSV files, catalog records reflect correct statuses.

---

## T7 — SIZE rotation

**Goal:** File rotates when it reaches `max_file_size`.

**Steps:**
1. Set `rotation: size`, `max_file_size: 10240` (10 KB), `log_interval: 100ms`.
2. Run for ~2 minutes.
3. Inspect SD card — multiple `<prefix>_<epoch>.csv` files; none exceeds ~10 KB.

**Pass:** Multiple files; each ≤ `max_file_size` + one row headroom.

---

## T8 — Power-loss recovery: catalog marks CORRUPT

**Goal:** An OPEN record from a dirty shutdown is marked CORRUPT on next boot.

**Steps:**
1. Let device write for 30 s.
2. Hard-power-cut the board (do not clean shutdown).
3. Reboot board.
4. Check serial log for `Power-loss: record N OPEN -> CORRUPT` message.
5. Inspect `catalog.bin` — previously OPEN record is now `0x03`.

**Pass:** Warning logged; record status correctly updated to CORRUPT.

---

## T9 — Webserver: directory listing (JSON)

**Goal:** `GET /file/` returns valid JSON with file list.

**Steps:**
1. Flash with `webserver_sd` enabled.
2. Open browser or `curl http://<device-ip>/file/`.
3. Verify response is valid JSON containing `current_path`, `items`, `breadcrumbs` keys.
4. Verify each item has `name`, `is_directory`, `uri`, and `size` (for files).

**Pass:** Valid JSON; all fields present; items match SD card contents.

---

## T10 — Webserver: streaming download (no heap allocation, no size limit)

**Goal:** Large CSV file (> 100 KB, previously blocked by the old 100 KB limit) downloads completely.

**Steps:**
1. Create or let the device generate a CSV file > 200 KB.
2. `curl -o downloaded.csv http://<device-ip>/file/<filename>.csv` while watching free heap in device log.
3. Confirm:
   - Download completes without 413 error.
   - `Content-Disposition: attachment; filename="<filename>.csv"` header present.
   - Heap does not drop by the file size (proving no full-buffer allocation).
   - Downloaded file byte-for-byte matches SD card file.

**Pass:** Full download; correct headers; heap delta < 8 KB during transfer.

---

## T11 — Webserver: upload

**Goal:** File upload stores correctly and sends 201 on both single-chunk and multi-chunk files.

**Steps:**
1. Upload a small file (< 1 KB, single chunk): `curl -F "file=@test.txt" http://<device-ip>/file/`
2. Upload a large file (> 8 KB, multi-chunk): same curl command with a larger file.
3. Verify 201 response both times.
4. Pull SD card; confirm both files present and byte-for-byte correct.

**Pass:** 201 both times; files intact. (This catches the pre-Phase-6 bug where single-chunk uploads never sent a response.)

---

## T12 — Webserver: delete via HTTP DELETE

**Goal:** `DELETE /file/<path>` removes the file and returns 204.

**Steps:**
1. `curl -X DELETE http://<device-ip>/file/test.txt`
2. Confirm 204 response.
3. `curl http://<device-ip>/file/` — item no longer in listing.
4. Confirm `?delete` legacy GET workaround also still works.

**Pass:** File removed; 204 response; not visible in listing.

---

## T13 — Concurrent load: simultaneous web requests + logging

**Goal:** Webserver serving a download does not stall the CSV logging task or vice versa.

**Steps:**
1. Start a large file download in the browser.
2. While download is in progress, watch serial log for continued CSV write confirmations.
3. Measure download speed before and after sensors firing at full rate.

**Pass:** CSV writes continue during download; download completes; no watchdog resets; no `Queue full` warnings during the test.

---

## T14 — `force_write_on_change` behaviour

**Goal:** A value change triggers an immediate write even if `log_interval` has not elapsed.

**Steps:**
1. Set `log_interval: 60s`, `force_write_on_change: true` on a sensor.
2. Manually trigger a state change on the sensor within the 60 s window.
3. Inspect CSV — confirm a row was written immediately at the change timestamp.

**Pass:** Row present for the change event; next timed row appears ~60 s later.

---

## Observability checklist

Check these in every test:

| Item | Expected |
|---|---|
| No `assert failed` / `Guru Meditation Error` | ✅ |
| No FreeRTOS stack overflow (`E (sd_logger) stack overflow`) | ✅ |
| `catalog.bin` records align to 64-byte boundaries | ✅ |
| Free heap stable over 1-hour run (no leak) | ✅ (monitor with `sensor: free_heap`) |
| SD card not corrupted after clean shutdown | ✅ |

---

## Suggested YAML snippet for testing

```yaml
esphome:
  name: blackbox-test
  platform: ESP32S3

sd_mmc:
  id: sd_card
  clk_pin: 36
  cmd_pin: 35
  data0_pin: 37
  data1_pin: 38
  data2_pin: 33
  data3_pin: 34

time:
  - platform: sntp
    id: sntp_time

sd_logger:
  id: logger
  sd_mmc_id: sd_card
  time_id: sntp_time
  queue_size: 50
  task_priority: 1
  sync_online:
    name: "Sync Online"
  sync_sending_backlog:
    name: "Sync Backlog"

sensor:
  - platform: sd_logger
    sd_logger_id: logger
    sensor_id: my_sensor
    file_prefix: "rpm"
    header: "timestamp,rpm"
    log_interval: 1s
    format: "%.0f"
    rotation: daily
    force_write_on_change: true

  - platform: template
    id: my_sensor
    name: "RPM Sim"
    lambda: "return (float)(millis() % 6000);"
    update_interval: 500ms

  - platform: template
    name: "Free Heap"
    lambda: "return (float)esp_get_free_heap_size();"
    update_interval: 5s

webserver_sd:
  id: file_server
  sd_mmc_id: sd_card
  url_prefix: "file"
  root_path: "/"
  enable_deletion: true
  enable_download: true
  enable_upload: true

web_server:
  port: 80
```
