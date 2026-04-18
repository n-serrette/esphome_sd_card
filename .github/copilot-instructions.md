# Copilot Instructions — esphome-sd_card_web

## Project Overview
This is an ESPHome **external component** repository providing three custom components:
- `sd_card` — SD card driver (SDMMC peripheral, ESP-IDF)
- `sd_logger` — sensor data logger to SD card with FreeRTOS task queue and cloud upload
- `webserver_sd` — web interface for browsing/downloading SD card files

Target hardware: **ESP32-S3** (e.g. Lolin S3 Pro) running **ESP-IDF framework** (not Arduino).

## Repository Structure
```
components/
  sd_card/        # sd_card.h, sd_card.cpp, __init__.py, sensor.py, text_sensor.py
  sd_logger/     # sd_logger.h, sd_logger.cpp, __init__.py, sensor.py, text_sensor.py
  webserver_sd/  # webserver_sd.h, webserver_sd.cpp, __init__.py
example.yaml     # reference ESPHome YAML configuration
```

## Code Conventions

### C++ (components)
- Use the **ESPHome component pattern**: inherit from `esphome::Component`, implement `setup()` and `loop()`.
- Guard platform-specific code with `#ifdef USE_ESP_IDF` and `#ifdef USE_SENSOR` / `#ifdef USE_TEXT_SENSOR`.
- FreeRTOS tasks: use `xTaskCreatePinnedToCore`; prefer core 0 for upload tasks, core 1 for logging.
- Use `ESP_LOGI` / `ESP_LOGE` (ESP-IDF logging) inside FreeRTOS tasks. Use `ESP_LOGD` / `ESP_LOGW` elsewhere.
- Never call `vTaskDelay` from the main ESPHome loop; only inside dedicated FreeRTOS tasks.
- Structs that cross task boundaries must be **POD** (plain-old-data) so `xQueueSend` copies by value.
- `CatalogRecord` must remain exactly **64 bytes** (verified by `static_assert`).
- Use `#pragma pack(push, 1)` / `#pragma pack(pop)` around binary on-disk structs.
- File paths on SD are always absolute: `/sdcard/<relative>`. Use `sd_card_->build_path(filename)` to construct them.

### Python (`__init__.py`)
- Follow ESPHome schema conventions: `cv.Schema`, `cv.Optional`, `cv.Required`.
- Use `cg.add(var.set_xxx(...))` to call C++ setters.
- Guard optional YAML keys so hardware paths that don't need them compile cleanly (don't create tasks or objects for unconfigured features).

### HTTP upload (sd_logger)
- Use `esp_http_client` (ESP-IDF native). Do **not** use Arduino HTTPClient.
- Stream file bodies in 4 KB chunks via `esp_http_client_write`.
- Always set `Authorization: Bearer <token>`, `Content-Type: text/csv`, and `Content-Length` headers.
- Apply exponential backoff on failures (initial 30 s, doubles, cap 15 min, reset on success).

## Catalog File (`catalog.bin`)
Status byte values:
| Value | Constant | Meaning |
|-------|----------|---------|
| 0x01  | `CATALOG_STATUS_OPEN`     | File open (power-loss sentinel) |
| 0x02  | `CATALOG_STATUS_CLOSED`   | Fsynced and closed normally |
| 0x03  | `CATALOG_STATUS_CORRUPT`  | Was open on boot — incomplete |
| 0x04  | `CATALOG_STATUS_UPLOADED` | Successfully PUT to cloud |

On boot, any record with `CATALOG_STATUS_OPEN` must be transitioned to `CATALOG_STATUS_CORRUPT`.

## YAML Configuration Reference
See `example.yaml` for a complete working configuration. Key component IDs:
- `sd_card` → `sd_card` component
- `logger` → `sd_logger` component (not the built-in ESPHome logger)

## Do Not
- Do not use Arduino libraries or `#include <Arduino.h>`.
- Do not block the ESPHome main loop; use FreeRTOS tasks for long-running work.
- Do not hardcode SD mount path; always derive it via `sd_card_->build_path(...)`.
- Do not add features beyond what is requested — keep changes minimal and targeted.
