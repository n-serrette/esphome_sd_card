# esphome-sd_card_web

An [ESPHome](https://esphome.io) external component suite providing SD card logging and a web file browser for **ESP32-S3** (and ESP32) devices running the **ESP-IDF framework**.

Three components are included:

| Component | Purpose |
|---|---|
| [`sd_card`](#sd_card) | Low-level SD card driver (SDMMC peripheral) |
| [`sd_logger`](#sd_logger) | Sensor data logger to SD card, with optional cloud upload |
| [`webserver_sd`](#webserver_sd) | HTTP file browser/manager for the SD card |

---

## Requirements

- **Hardware**: ESP32 or ESP32-S3 (tested on [LOLIN S3 Pro](https://www.wemos.cc/en/latest/s3/s3_pro.html))
- **Framework**: `esp-idf` (not Arduino)
- **ESPHome**: 2025.7.0 or newer

---

## Installation

Add the external components to your ESPHome YAML:

```yaml
external_components:
  - source: github://andrewbackway/esphome-sd_card_web
    components: [sd_card, sd_logger, webserver_sd]
```

Add the required `sdkconfig_options` under your `esp32:` block:

```yaml
esp32:
  board: lolin_s3_pro
  variant: esp32s3
  framework:
    type: esp-idf
    sdkconfig_options:
      CONFIG_FATFS_LFN_HEAP: "y"
      CONFIG_FATFS_LFN: "2"
```

---

## `sd_card`

Drives the onboard SDMMC peripheral using the ESP-IDF native driver. Mounts the card as a FAT32 volume at `/sdcard`.

### Wiring — LOLIN S3 Pro (onboard TF slot)

Only three pins are physically connected on the LOLIN S3 Pro, so `mode_1bit: true` is required.

| SD signal | ESP32-S3 GPIO | YAML key |
|---|---|---|
| CMD (MOSI) | GPIO 11 | `cmd_pin` |
| CLK (SCK) | GPIO 12 | `clk_pin` |
| DATA0 (MISO) | GPIO 13 | `data0_pin` |

For 4-bit SDMMC (faster, wired on custom hardware), also connect `data1_pin`, `data2_pin`, and `data3_pin` and omit `mode_1bit` (defaults to `false`).

### YAML configuration

```yaml
sd_card:
  id: sd_card1
  clk_pin: 12
  cmd_pin: 11
  data0_pin: 13
  mode_1bit: true         # required for LOLIN S3 Pro; omit for 4-bit mode
  # data1_pin: 14         # 4-bit mode only
  # data2_pin: 15         # 4-bit mode only
  # data3_pin: 16         # 4-bit mode only
  # power_ctrl_pin:       # optional GPIO to power-cycle the SD card slot
  #   number: 10
  #   inverted: false
```

| Key | Required | Default | Description |
|---|---|---|---|
| `clk_pin` | Yes | — | Clock / SCK GPIO number |
| `cmd_pin` | Yes | — | CMD / MOSI GPIO number |
| `data0_pin` | Yes | — | DATA0 / MISO GPIO number |
| `data1_pin` | No | — | DATA1 (4-bit mode only) |
| `data2_pin` | No | — | DATA2 (4-bit mode only) |
| `data3_pin` | No | — | DATA3 (4-bit mode only) |
| `mode_1bit` | No | `false` | `true` = 1-bit SDMMC bus (only DATA0 used) |
| `power_ctrl_pin` | No | — | GPIO to control SD slot power rail |

### Sensors

```yaml
sensor:
  - platform: sd_card
    sd_card_id: sd_card1
    type: used_space         # or total_space / free_space
    name: "SD Used (bytes)"
    # Apply a filter to convert to GB:
    filters:
      - lambda: return (float)sd_card::convertBytes((uint64_t)x, sd_card::MemoryUnits::GigaByte);

  - platform: sd_card
    sd_card_id: sd_card1
    type: file_size
    path: "/logs/vehicle/vehicle_20260418.csv"
    name: "Log File Size"
```

| Type | Unit | Description |
|---|---|---|
| `used_space` | bytes | Bytes used on the FAT partition |
| `total_space` | bytes | Total partition capacity |
| `free_space` | bytes | Available bytes |
| `file_size` | bytes | Size of a specific file (requires `path`) |

Unit conversion helper: `sd_card::convertBytes(uint64_t bytes, sd_card::MemoryUnits unit)` — `unit` values: `Byte`, `KiloByte`, `MegaByte`, `GigaByte`, `TeraByte`, `PetaByte`.

### Text sensor

```yaml
text_sensor:
  - platform: sd_card
    sd_card_id: sd_card1
    sd_card_type:
      name: "SD Card Type"   # e.g. "SDHC"
```

### Automation actions

These actions can be used in ESPHome automations:

| Action | Parameters | Description |
|---|---|---|
| `sd_card.write_file` | `path`, `data` | Write bytes to a file (overwrite) |
| `sd_card.append_file` | `path`, `data` | Append bytes to a file |
| `sd_card.delete_file` | `path` | Delete a file |
| `sd_card.create_directory` | `path` | Create a directory |
| `sd_card.remove_directory` | `path` | Remove an empty directory |

---

## `sd_logger`

A non-blocking sensor data logger. Each configured **log** gets its own CSV file on the SD card. Data is captured in the main ESPHome loop and handed off to a dedicated **FreeRTOS task** via a queue, keeping the loop unblocked.

### How it works

```
ESPHome loop()
  │  Every log_interval:  build CSV row, push LogPacket to FreeRTOS queue
  ▼
FreeRTOS logging task (core 1, priority 1)
  │  Dequeue LogPacket → open/rotate/write CSV → fsync every fsync_interval
  ▼
SD card (FATFS / SDMMC)

(Optional) FreeRTOS upload task (core 0, priority 0)
  │  Walk catalog.bin for CLOSED records → HTTP PUT to upload_url
  ▼
Cloud endpoint
```

#### Catalog file

`sd_logger` maintains a binary catalog at `/sdcard/<path>/catalog.bin` (where `<path>` is the value of the `path` config key, default `logs`). Each record is exactly **64 bytes**:

| Offset | Size | Field | Description |
|---|---|---|---|
| 0 | 1 B | `status` | Lifecycle state (see table below) |
| 1 | 4 B | `start_timestamp` | Unix epoch when the file was created |
| 5 | 32 B | `filename` | Null-terminated relative path |
| 37 | 4 B | `file_size` | Final size in bytes (0 while open) |
| 41 | 23 B | `reserved` | Reserved for future use (e.g. CRC32) |

Status byte lifecycle:

| Value | Constant | Meaning |
|---|---|---|
| `0x01` | `CATALOG_STATUS_OPEN` | File is open — also acts as a power-loss sentinel |
| `0x02` | `CATALOG_STATUS_CLOSED` | File was fsynced and closed normally |
| `0x03` | `CATALOG_STATUS_CORRUPT` | Was `OPEN` on boot — file is incomplete |
| `0x04` | `CATALOG_STATUS_UPLOADED` | Successfully PUT to cloud endpoint |

On boot, any record still in `OPEN` state (e.g. due to a power cut) is transitioned to `CORRUPT` and the partial file is left on the card for manual inspection.

#### File rotation

| Policy | When a new file is created |
|---|---|
| `daily` | UTC date changes (midnight rollover) |
| `size` | Current file exceeds `max_file_size` bytes |

File names are generated as `<file_prefix>_<YYYYMMDD>_<HHMMSS>.csv`.

#### CSV format

The first row is the header. If `header` is omitted, it is auto-generated from sensor names. Subsequent rows are:

```
<unix_timestamp>,<sensor1_value>,<sensor2_value>,...
```

Column order: numeric sensor slots (in declaration order), then text sensor slots (in declaration order).

### YAML configuration

```yaml
sd_logger:
  id: logger
  sd_card_id: sd_card1
  time_id: sntp_time

  path: "logs"               # base folder on SD card (default: "logs")
  queue_size: 50             # max queued rows before drops (5–200, default: 50)
  task_priority: 1           # FreeRTOS priority 0–5 (default: 1)
  fsync_interval: 30s        # how often to fsync open files (default: 30s)

  # ── Cloud upload (experimental — omit upload_url to disable) ──────────────
  upload_url: "https://example.com/api/upload"
  bearer_token: !secret upload_token
  ping_url: "https://example.com/ping"
  ping_interval: 30s
  ping_timeout: 5s
  backoff_initial: 30s
  backoff_max: 15min

  sync_online:
    name: "Cloud Sync Online"
  sync_sending_backlog:
    name: "Cloud Sync Backlog"

  # ── Log definitions ───────────────────────────────────────────────────────
  logs:
    - name: "Vehicle Data"
      folder: "vehicle"
      file_prefix: "vehicle"
      log_interval: 1s
      rotation: daily
      sensors:
        - sensor_id: engine_rpm
          format: "%.0f"
        - sensor_id: vehicle_speed
          format: "%.1f"
      text_sensors:
        - sensor_id: gear_pos

    - name: "Gear Log"
      folder: "gear"
      file_prefix: "gear"
      log_interval: 5s
      rotation: size
      max_file_size: 10485760   # 10 MB
      text_sensors:
        - sensor_id: gear_pos
```

#### Top-level keys

| Key | Required | Default | Description |
|---|---|---|---|
| `sd_card_id` | Yes | — | ID of the `sd_card` component |
| `time_id` | Yes | — | ID of a `time` component (used for timestamps and daily rotation) |
| `path` | No | `"logs"` | Base directory on SD card for all log files and `catalog.bin` |
| `queue_size` | No | `50` | FreeRTOS queue depth (5–200). Packets dropped silently when full |
| `task_priority` | No | `1` | FreeRTOS task priority for the logging task (0–5) |
| `fsync_interval` | No | `30s` | How often the logging task calls `fsync` on open CSV files |
| `upload_url` | No | — | HTTP(S) endpoint for cloud upload (omit to disable upload task) |
| `bearer_token` | No | — | Bearer token sent as `Authorization: Bearer <token>` |
| `ping_url` | No | — | URL polled before each upload pass to check connectivity |
| `ping_interval` | No | `30s` | How often the upload task pings `ping_url` |
| `ping_timeout` | No | `3s` | HTTP timeout for the ping request |
| `backoff_initial` | No | `30s` | Initial retry delay after a failed upload |
| `backoff_max` | No | `15min` | Maximum retry delay (exponential backoff cap) |
| `sync_online` | No | — | Binary sensor — `true` when ping succeeds |
| `sync_sending_backlog` | No | — | Binary sensor — `true` when CLOSED files are pending upload |

#### Per-log keys (`logs:` list)

| Key | Required | Default | Description |
|---|---|---|---|
| `file_prefix` | Yes | — | Prefix for generated filenames (e.g. `"vehicle"` → `vehicle_20260418_120000.csv`) |
| `log_interval` | Yes | — | How often a row is appended (e.g. `1s`, `500ms`) |
| `name` | No | `""` | Human-readable label (used in logs) |
| `folder` | No | `""` | Subdirectory under `path` (e.g. `"vehicle"` → logs stored in `logs/vehicle/`) |
| `header` | No | _(auto)_ | CSV header row. Auto-generated from sensor names if omitted |
| `rotation` | No | `"daily"` | `"daily"` or `"size"` |
| `max_file_size` | No | `52428800` (50 MB) | Maximum file size in bytes — only evaluated when `rotation: size` |
| `sensors` | No | `[]` | List of numeric sensor slots |
| `text_sensors` | No | `[]` | List of text sensor slots |

Each entry in `sensors:` accepts `sensor_id` (required) and `format` (printf format string, default `"%.4f"`). Each entry in `text_sensors:` accepts only `sensor_id`.

### Cloud upload (experimental)

> **Status: work in progress.** The upload task infrastructure is in place but the full `esp_http_client` streaming implementation is not yet complete. Use at your own risk.

When `upload_url` is set, a second FreeRTOS task starts on core 0 at priority `task_priority - 1`. It:

1. Pings `ping_url` periodically and publishes `sync_online`.
2. Walks `catalog.bin` for any record with status `CLOSED`.
3. Issues an HTTP PUT with the file body streamed in 4 KB chunks, headers `Authorization: Bearer <token>`, `Content-Type: text/csv`, and `Content-Length`.
4. On a 2xx response, updates the catalog record to `UPLOADED`.
5. On failure, applies exponential backoff (initial → doubled each failure → capped at `backoff_max`, reset to `backoff_initial` on success).

The logging task and upload task never access the same file simultaneously: the logging task only holds `OPEN` records; the upload task only touches `CLOSED` records.

---

## `webserver_sd`

Registers HTTP request handlers with the ESPHome `web_server_base`, providing a browser-accessible interface for the SD card. Requires the built-in `web_server` component to be configured.

### Features

| Feature | YAML key | Default |
|---|---|---|
| Directory listing (always on) | — | enabled |
| File download | `enable_download` | `false` |
| File upload | `enable_upload` | `false` |
| File deletion | `enable_deletion` | `false` |

Files are streamed directly from the SD card to the HTTP response in chunks, so large files do not require heap buffering.

MIME types are inferred from the file extension (e.g. `.csv` → `text/csv`, `.json` → `application/json`).

### YAML configuration

```yaml
web_server:
  port: 80

webserver_sd:
  id: file_server
  sd_card_id: sd_card1
  url_prefix: "file"          # served at http://<device>/file/...
  sd_path: "/"                # root of SD card to expose
  enable_download: true
  enable_upload: true
  enable_deletion: true
```

| Key | Required | Default | Description |
|---|---|---|---|
| `sd_card_id` | Yes | — | ID of the `sd_card` component |
| `url_prefix` | No | `"file"` | URL path prefix (e.g. `"file"` → `http://<ip>/file/`) |
| `sd_path` | No | `"/"` | SD card path to expose as the root of the file browser |
| `enable_download` | No | `false` | Allow `GET` requests to download individual files |
| `enable_upload` | No | `false` | Allow `POST` requests to upload files |
| `enable_deletion` | No | `false` | Allow `DELETE` requests to remove files |

---

## Full example

See [`example.yaml`](example.yaml) for a complete, working configuration including simulated sensors.

---

## License

[MIT](LICENSE)
