# Performance Optimisation Plan

Five distinct bottlenecks identified. Listed by severity.

---

## 1. Per-row `fsync` in the logging task (HIGH)

**File:** `sd_logger/sd_logger.cpp` — `task_logging_entry_`

**Problem:**  
Every CSV row write immediately calls `fflush()` then `fsync()`. On SD cards,
`fsync` must flush the FAT write-cache through to NAND — typically 10–100 ms per
call. A sensor firing at 1 Hz will block the logging FreeRTOS task for ≥100 ms/s
just in fsync overhead. At higher rates the queue will fill and packets will be
dropped.

**Fix:**  
Batch fsync — only call `fsync` after N rows *or* after T wall-clock seconds
(e.g., 30 s), not on every row. Always call `fsync` at rotation/close (already
done there). Expose the interval as a configurable YAML key
(`fsync_interval: 30s`).

**Expected gain:** eliminates the dominant per-row latency spike; queue pressure
drops proportionally.

---

## 2. File open/close on every upload chunk (HIGH)

**File:** `webserver_sd/webserver_sd.cpp` — `handleUpload`

**Problem:**  
`handleUpload` is called once per chunk (typically 1–8 KB). `index == 0` calls
`write_file()` (open → write → close) and every subsequent chunk calls
`append_file()` (open → write → close). Each open/close pair flushes FAT
metadata, making multi-MB uploads disproportionately slow.

**Fix:**  
Keep a `FILE*` open across chunks for the lifetime of the request. Store it in
`request->_tempObject` (cast to `FILE*`) or in a small stack-allocated struct.
Open on `index == 0`, write raw on every call, `fclose` + `update_sensors` on
`final == true`.

**Expected gain:** order-of-magnitude improvement for files > a few KB;
eliminates O(N_chunks) FAT metadata writes.

---

## 3. `handle_download_stream` reads entire file into heap (HIGH)

**File:** `webserver_sd/webserver_sd.cpp` — `handle_download_stream`

**Problem:**  
Downloads call `sd_mmc_->read_file(path)` which allocates a `std::vector<uint8_t>`
up to 100 KB before calling `beginResponse`. On ESP32-S3 with a typical free heap
of ~100–150 KB this regularly fails, and even when it succeeds it monopolises the
largest contiguous heap block while the TCP stack is draining it.
`SdMmc::stream_file()` already exists for exactly this use case but is unused here.

**Fix:**  
Replace `read_file` + `beginResponse(data, size)` with
`request->beginChunkedResponse(mime, ...)` and call `sd_mmc_->stream_file()` with
a 4 KB stack buffer inside the chunk generator callback. This removes the heap
allocation entirely.

**Expected gain:** downloads no longer fail on files > free-heap; sustained
throughput improves because TCP pipelining and SD reads interleave.

---

## 4. `update_sensors()` called after every SD write (MEDIUM)

**File:** `sd_mmc/sd_mmc.cpp` — `write_file`, `append_file`, `create_directory`,
`remove_directory`, `delete_file`

**Problem:**  
`update_sensors()` calls `f_getfree()` which performs a full FAT cluster scan —
an expensive FATFS call (~tens of ms). It is triggered after *every* write,
including each of the repeated `append_file` calls during an upload (one per
chunk). Combined with issue #2, an upload of 100 × 4 KB chunks calls
`f_getfree()` 100 times.

**Fix:**  
Debounce sensor publication. Track a `last_sensor_update_ms_` timestamp and skip
the `f_getfree` call if fewer than 30 s have elapsed, except on explicit user
calls to `update_sensors()`. Alternatively, only call `update_sensors()` after
`fclose` (rotation/completion), never mid-stream.

**Expected gain:** removes the per-chunk FAT scan during uploads and logger
rotation; reduces main-loop blocking by `f_getfree` latency × call frequency.

---

## 5. Directory listing JSON built as a single heap string (LOW)

**File:** `webserver_sd/webserver_sd.cpp` — `handle_index`

**Problem:**  
`handle_index` accumulates the entire JSON response in a single `std::string` via
repeated `+=` before calling `response->print(json.c_str())` once. For directories
with > ~50 entries (common with daily CSV rotation) the string can exceed 4–8 KB,
triggering repeated heap reallocations while the `AsyncResponseStream` is idle.

**Fix:**  
Write directly to the `AsyncResponseStream` per entry using `response->print()`
inside the loop. Remove the accumulator string. The stream already chunks to TCP
transparently.

**Expected gain:** eliminates heap reallocation churn for large directories;
time-to-first-byte for the file listing improves.

---

## Summary Table

| # | Component | Area | Severity | Change type |
|---|-----------|------|----------|-------------|
| 1 | sd_logger | Per-row fsync | HIGH | Add fsync-interval batching |
| 2 | webserver_sd | Upload chunk open/close | HIGH | Persist FILE* across chunks |
| 3 | webserver_sd | Download heap buffer | HIGH | Switch to chunked stream |
| 4 | sd_mmc | update_sensors frequency | MEDIUM | Debounce `f_getfree` calls |
| 5 | webserver_sd | JSON listing string | LOW | Stream directly to response |
