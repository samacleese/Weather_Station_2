# OTA Firmware Update — Design Spec
**Date:** 2026-04-13
**Status:** Approved

## Context

The Weather Station 2 is a battery-powered ESP32/Inkplate 10 device that wakes every 10 minutes, fetches weather data, renders an e-paper display, then deep-sleeps. Currently the only way to update firmware is via USB/serial. This spec describes a WiFi-based OTA (Over-The-Air) update system that:

- Checks for a new firmware version on each wakeup
- Prompts the user to confirm before flashing
- Verifies firmware authenticity before writing to flash
- Is safe enough for a battery device (no brick risk from power loss)

The firmware currently compiles to ~1.75MB. Asset externalization (Phase 0) shrinks this to ~1.25MB, giving comfortable headroom in dual-OTA partitions and reducing flash exposure from power loss during update.

---

## Architecture Overview

**Infrastructure:**
- **Manifest:** `GET /ota-firmware/data` on the local Pi json-store (HTTP, same pattern as `BatteryLogger`)
- **Binary:** `GET /ota-firmware/binaries/firmware.bin` on the local Pi json-store (raw binary endpoint — see dependency note below)
- **Signing:** ECDSA P-256; private key stays offline on developer machine, public key embedded in firmware
- **Version source of truth:** A `VERSION` file in the repo root, read by both CMake (injected as `FIRMWARE_VERSION` at build time) and `ota_publish.py` (written into the manifest `version` field)

**Dependency:** The json-store raw binary endpoint (`PUT/GET /:app/binaries/:id`) does not yet exist. This spec assumes it exists. A feature request will be filed with the json-store team once this spec is finalized.

**Phasing (phase-sequential; each phase's spike(s) gate its implementation):**
- Phase 0: PSRAM font spike (independent) → if passes, implement `AssetLoader` and `asset_packer.py`; SPIFFS integration deferred until Phase 1 partition table is live
- Phase 1: Partition table spike → dual-OTA + SPIFFS layout (USB flash, ships with Phase 0 SPIFFS image)
- Phase 2: Python signing tooling (host-side, no hardware dependency; can run in parallel with Phases 0–1)
- Phase 3: Streaming + ECDSA + OTA write spikes → `OTAManager` implementation

---

## Phase 0: Asset Externalization

### Spike (independent — can run before Phase 1)

Write a 20-line test sketch that:
1. Allocates a known font's bitmap data in PSRAM via `ps_malloc`
2. Constructs a `GFXfont` struct pointing to that PSRAM buffer
3. Calls `display.setFont()` and renders one character

**Success criterion:** Character renders correctly on hardware.
**If this spike fails:** Phase 0's design must be revisited before proceeding. Do not continue with asset externalization until this is resolved.

This spike runs against the existing `huge_app` partition layout — no partition table change is required. If the spike passes, implement `AssetLoader` and `asset_packer.py`, and generate the SPIFFS image. Full SPIFFS integration (mounting SPIFFS, testing `AssetLoader` end-to-end) cannot be validated until the Phase 1 partition table is live on hardware.

### Goal

Shrink the firmware binary from ~1.75MB to ~1.25MB by moving large fonts and cat images from PROGMEM into a SPIFFS partition, loaded at boot into PSRAM. On ESP32, `pgm_read_byte()` is a plain pointer dereference — PSRAM pointers work identically to PROGMEM pointers with Inkplate's rendering.

### Assets to Externalize

| Asset | ~Size | Approach |
|---|---|---|
| 6 cat images (3-bit packed, 300×300px) | ~270KB | Raw binary SPIFFS files; load via `ps_malloc` into PSRAM |
| Roboto_Medium 150pt | ~80KB | Binary SPIFFS file; construct `GFXfont` from PSRAM buffer |
| Roboto_Light 150pt | ~80KB | Same |
| Roboto_Light 104pt | ~40KB | Same |
| Smaller fonts (35pt and below) | ~30KB | Keep in PROGMEM — not worth the complexity |

**Total savings:** ~470KB from firmware binary.

### New Tooling: `tools/asset_packer.py`

Converts PROGMEM C arrays in the font/image headers to flat binary files suitable for SPIFFS upload:

```bash
python3 tools/asset_packer.py --output assets/spiffs_image/
```

The SPIFFS image is flashed once via USB using esptool. OTA never touches the SPIFFS partition — assets stay put unless SPIFFS is deliberately reflashed.

### New Module: `src/display/AssetLoader`

Wraps SPIFFS open + `ps_malloc` + `GFXfont` construction. Called once per boot before rendering. Provides:
- `uint8_t* loadImage(const char* path)` — loads raw image binary into PSRAM
- `GFXfont* loadFont(const char* bitmapPath, const char* glyphPath, ...)` — loads font data into PSRAM, constructs and returns `GFXfont` struct

---

## Phase 1: Partition Table

### Spike

Flash `cmake/ota_partitions.csv` via USB, boot device, verify:
- `esp_ota_get_next_update_partition()` returns non-NULL
- `Serial.println(update_partition->label)` prints `"app1"`

### Partition Layout (`cmake/ota_partitions.csv`)

```
nvs,      data, nvs,    0x9000,   0x5000
otadata,  data, ota,    0xe000,   0x2000
app0,     app,  ota_0,  0x10000,  0x140000   # 1.25MB
app1,     app,  ota_1,  0x150000, 0x140000   # 1.25MB
spiffs,   data, spiffs, 0x290000, 0x170000   # 1.4375MB for assets
```

- Each OTA partition: 1,310,720 bytes — comfortable for ~1.25MB firmware after Phase 0
- SPIFFS: 1,474,560 bytes — fits all externalized assets (~470KB) with ~3× margin

**Note:** The partition table change, Phase 0 SPIFFS image, and updated firmware all ship together in the same USB flash event. The new SPIFFS partition does not exist until this flash is performed; `AssetLoader` end-to-end integration testing happens after this flash.

**eeprom partition:** The current `huge_app` layout includes an `eeprom` partition at `0x310000` used by the Inkplate library for waveform storage (source of the boot warning "Waveform load failed! Upload new waveform in EEPROM"). The new layout drops the named `eeprom` partition. Before flashing, confirm whether the Inkplate library resolves EEPROM via the named partition or via ESP-IDF's NVS-backed EEPROM emulation. If it requires the named partition, add it back (at the cost of some SPIFFS space); if NVS-backed emulation suffices, the drop is safe. This must be resolved before the Phase 1 USB flash.

### Build System Changes

**`CMakeLists.txt`:**
- Replace `huge_app.csv` copy block with `ota_partitions.csv`
- Read `VERSION` file and inject as `FIRMWARE_VERSION` compile-time define
- Add `OTAManager.cpp`, `AssetLoader.cpp` to sources

**`cmake/BoardOptions.cmake`:**
- Disable `ARDUINO_INKPLATE_INKPLATE10_MENU_PARTITIONSCHEME_HUGE_APP`
- Enable custom partition scheme

---

## Phase 2: Signing Tooling

No hardware dependency. Can be developed in parallel with Phases 0 and 1.

### Version Sync

`VERSION` (repo root) is the single source of truth. Format: `YYYYMMDDNNN` (integer, one line, trailing newline).

- CMake reads `VERSION` and injects `FIRMWARE_VERSION` as a compile-time `#define`. The newline must be stripped:
  ```cmake
  file(READ "${CMAKE_SOURCE_DIR}/VERSION" FIRMWARE_VERSION)
  string(STRIP "${FIRMWARE_VERSION}" FIRMWARE_VERSION)
  target_compile_definitions(WeatherStation PRIVATE FIRMWARE_VERSION=${FIRMWARE_VERSION})
  ```
- `ota_publish.py` reads `VERSION` automatically from the repo root (no `--version` CLI argument). The script locates `VERSION` relative to its own path.
- To cut a release: bump `VERSION`, build, run `ota_publish.py`
- No manual constant to keep in sync

See GitHub issue #10.

### `tools/ota_keygen.py` (one-time)

```bash
python3 tools/ota_keygen.py --output-dir keys/
# Produces: keys/ecdsa_private.pem  (gitignored, keep offline)
#           keys/ecdsa_public.pem
# Also writes: src/security/OTAPublicKey.h  (DER-encoded public key as const uint8_t[])
```

Depends on: `cryptography` Python package.

### `tools/ota_publish.py`

```bash
python3 tools/ota_publish.py \
  --firmware build/WeatherStation.bin \
  --private-key keys/ecdsa_private.pem \
  --pi-host 192.168.1.2:5000
```

Steps:
1. Read `VERSION` file
2. Read `.bin`, compute SHA-256
3. Sign SHA-256 digest with ECDSA P-256 private key
4. Base64-encode DER signature
5. Write `manifest.json` locally
6. `PUT` manifest to `http://<pi-host>/ota-firmware/data`
7. `PUT` raw binary to `http://<pi-host>/ota-firmware/binaries/firmware.bin`

### Manifest Format

```json
{
  "version": 20260413001,
  "size": 1234567,
  "sha256": "a3f1d2...64 hex chars...",
  "signature": "base64encodedDERsignature==",
  "url": "http://192.168.1.2:5000/ota-firmware/binaries/firmware.bin",
  "min_battery_mv": 3600
}
```

| Field | Description |
|---|---|
| `version` | Integer build number from `VERSION` file; device compares `>` against compiled-in `FIRMWARE_VERSION` |
| `size` | Exact byte count of `.bin` |
| `sha256` | Hex SHA-256 of raw `.bin` |
| `signature` | Base64 ECDSA P-256 signature over the SHA-256 digest bytes |
| `url` | Direct HTTP link to raw binary on Pi json-store. The Pi IP is embedded in the manifest by `ota_publish.py` via `--pi-host`. If the Pi's IP changes, re-running `ota_publish.py` generates an updated manifest with the new URL — intentional for a controlled local network environment. |
| `min_battery_mv` | Device refuses OTA if battery is below this threshold |

### Why ECDSA P-256

- mbedTLS 2.x (shipped with ESP-IDF 4.x / Arduino ESP32) supports P-256 natively
- Ed25519 requires mbedTLS 3.x — not available in this toolchain
- HMAC-SHA256 requires embedding a secret key in firmware (symmetric = weaker)
- RSA-2048 is 10–20× slower on ESP32 with large keys
- P-256: 64-byte signature (DER-encoded ≈ 72 bytes), fast on ESP32

---

## Phase 3: OTAManager

### Spikes (run in order; each must pass before the next)

**Spike 1 — Streaming HTTP download:**
Stream any HTTP binary from the Pi in 4KB chunks, accumulate SHA-256, log final hash. Success: hash matches expected; completes without OOM or timeout.

**Spike 2 — ECDSA P-256 verify:**
Generate known P-256 key pair in Python; sign a test digest; hardcode key + signature + digest in test sketch; call `mbedtls_ecdsa_verify()`. Success: returns 0 for valid sig, non-zero for tampered digest.

**Spike 3 — OTA write:**
Stream a test binary to `app1` via `esp_ota_begin/write/end`, set boot partition, restart. Success: `esp_ota_get_running_partition()->label` prints `"app1"`.

**Spike 4 — Button GPIO poll (low risk):**
Display a message, poll `digitalRead(36)` for 10 seconds, print result to serial. Success: press reliably reads LOW; idle reads HIGH. Confirms GPIO 36 behaviour during active (non-sleep) operation matches the deep-sleep wakeup configuration.

### `UpdateInfo` Struct

```cpp
struct UpdateInfo {
    uint64_t version;       // YYYYMMDDNNN — exceeds uint32_t max, must be 64-bit
    uint32_t size;          // exact byte count from manifest (informational only; not passed to esp_ota_begin)
    char sha256[65];        // 64 hex chars + null terminator
    char signature[200];    // base64 DER ECDSA signature (~100 chars; 200 gives headroom)
    char url[128];          // URL to raw binary on Pi json-store
    int min_battery_mv;     // minimum battery in millivolts to proceed
};
```

### `OTAManager::checkForUpdate()`

- `GET /ota-firmware/data` via plain HTTP (same pattern as `BatteryLogger`)
- Parse with `ArduinoJson` (`StaticJsonDocument<1024>` — the manifest contains a 64-char sha256, ~100-char base64 signature, and URL; 512 bytes is too tight once ArduinoJson tree overhead is included)
- Compare `version` field to compiled-in `FIRMWARE_VERSION`
- Return `true` + populate `UpdateInfo` struct if manifest version is greater

### `OTAManager::performUpdate()`

Call sequence (order is critical for safe abort):

1. `esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &handle)` — obtain handle. `OTA_SIZE_UNKNOWN` is used because the manifest `size` field is not covered by the ECDSA signature (which signs only the binary's SHA-256 digest, not the manifest JSON). On a plain HTTP channel, passing an attacker-controlled `size` to `esp_ota_begin()` is unsafe. Content length is validated implicitly when the SHA-256 over the streamed bytes matches the manifest's `sha256` field.
2. For each 4KB chunk: `esp_ota_write(handle, chunk, len)` + SHA-256 accumulator update
3. After stream: verify computed SHA-256 against manifest `sha256` field
4. If SHA-256 passes: verify ECDSA P-256 signature over SHA-256 digest using `mbedtls_ecdsa_verify()` with embedded public key
5. If both checks pass: `esp_ota_end(handle)` → `esp_ota_set_boot_partition()` → `esp_restart()`
6. If either check fails: `esp_ota_abort(handle)` → return `false` (running partition untouched)

**Important:** `esp_ota_end()` is called only after both SHA-256 and ECDSA checks pass. Calling `esp_ota_abort(handle)` before `esp_ota_end()` is always safe — it discards the incomplete write. Calling abort *after* `esp_ota_end()` is undefined; this ordering prevents that.

### Wake Button

GPIO 36 (`WAKE_BUTTON_GPIO`) is already wired to the physical wake button. During the 30s confirmation window:
- `pinMode(36, INPUT)` (input-only pin, hardware pull-up on Inkplate 10)
- Poll `digitalRead(36) == LOW` in a 100ms loop

**WiFi during poll:** The WiFi connection stays active during the 30s window. If the connection to the Pi drops between the manifest fetch and the binary download, `performUpdate()` returns `false` and the device falls through to normal weather display — an acceptable failure mode.

### Integration in `Weather_Station_2.cpp`

Insert after `network->begin()`, before weather fetch:

```cpp
OTAManager ota(network);
OTAManager::UpdateInfo info;
if (ota.checkForUpdate(OTA_MANIFEST_URL, info)) {
    // Render splash screen (e-paper refresh takes 2-4s — acceptable)
    display.clearDisplay();
    display.setFont(/* 35pt */);
    display.setCursor(50, 350);
    display.printf("Firmware v%llu available\nPress WAKE to install\n(30s to skip)", info.version);
    // Battery shown on splash for user awareness; a second read is taken at
    // confirmation time for the safety threshold check (intentional — the
    // confirmation-time reading is what matters for flash safety).
    display.printf("\nBattery: %.2fV", (display.readBattery() + ADC_OFFSET));
    display.display();

    // Poll GPIO 36 for 30s
    unsigned long deadline = millis() + 30000;
    bool confirmed = false;
    while (millis() < deadline) {
        if (digitalRead(36) == LOW) { confirmed = true; break; }
        delay(100);
    }

    if (confirmed) {
        float volts = display.readBattery() + ADC_OFFSET;  // second read — safety threshold
        if ((int)(volts * 1000) < info.min_battery_mv) {
            display.clearDisplay();
            display.print("Battery too low for update.\nConnect charger and try again.");
            display.display();
            // Fall through to normal operation
        } else {
            display.clearDisplay();
            display.print("Downloading firmware...\nDo not power off.");
            display.display();
            if (!ota.performUpdate(info)) {
                display.clearDisplay();
                display.print("Update failed.\nContinuing normal operation.");
                display.display();
                delay(3000);
            }
            // On success: esp_restart() was called inside performUpdate — never reaches here
        }
    }
}
```

---

## Files to Create / Modify

### New Files

| File | Purpose |
|---|---|
| `VERSION` | Single source of truth for firmware version |
| `cmake/ota_partitions.csv` | Dual-OTA + SPIFFS partition table |
| `src/ota/OTAManager.h/.cpp` | OTA check + update logic |
| `src/display/AssetLoader.h/.cpp` | SPIFFS + PSRAM asset loading |
| `src/security/OTAPublicKey.h` | Embedded ECDSA P-256 public key |
| `tools/ota_keygen.py` | One-time key pair generation + C header output |
| `tools/ota_publish.py` | Sign firmware, write manifest, PUT to Pi json-store |
| `tools/asset_packer.py` | Convert PROGMEM C arrays → flat binary files for SPIFFS |

### Modified Files

| File | Change |
|---|---|
| `CMakeLists.txt` | Use `ota_partitions.csv`; inject `VERSION` as `FIRMWARE_VERSION`; add new sources |
| `cmake/BoardOptions.cmake` | Disable `HUGE_APP`, enable custom partition scheme |
| `Weather_Station_2.cpp` | Add OTA check block; load assets via `AssetLoader` |
| `src/display/KittyPics.h/.cpp` | Remove large PROGMEM arrays (replaced by SPIFFS files) |
| `assets/fonts/Roboto_Light.h`, `Roboto_Medium.h` | Remove large PROGMEM font sizes (replaced by SPIFFS files) |

---

## Verification / Test Plan

| Phase | Test | Success Criterion |
|---|---|---|
| Phase 0 spike | PSRAM font loading test sketch | Character renders correctly from PSRAM buffer |
| Phase 0 integration | SPIFFS assets flashed, `AssetLoader` integrated, full weather cycle | Display renders correctly; firmware ~1.25MB |
| Phase 1 spike | Flash `ota_partitions.csv` via USB, boot | `update_partition->label` prints `"app1"` |
| Phase 2 | `ota_keygen.py` + `ota_publish.py` run on host | Manifest + binary published to Pi json-store; manifest parses correctly |
| Phase 3 spike 1 | Streaming HTTP binary download | SHA-256 matches; no OOM or timeout |
| Phase 3 spike 2 | ECDSA P-256 verify on hardware | Returns 0 for valid sig, non-zero for tampered digest |
| Phase 3 spike 3 | OTA write test binary to `app1` | Boots from `app1` after restart |
| Phase 3 spike 4 | GPIO 36 poll during active operation | Button press reads LOW reliably |
| Phase 3 integration | End-to-end: publish → device detects → user confirms → flashes + restarts | Device boots updated firmware |
| Negative tests | Tampered SHA-256; tampered signature; low battery | Each rejected correctly; old partition untouched |

---

## What's Deliberately Excluded

- **Firmware encryption:** HTTP on the local network handles transport. AES encryption of the binary would require an AES key in the firmware, providing no real benefit.
- **Rollback:** If the new firmware is bad, reflash via USB. Automatic rollback requires a health-check mechanism that's out of scope.
- **Automatic install (no button):** Battery devices must never flash firmware unattended — power loss mid-flash bricks the device. The confirmation step is a safety requirement, not a UX choice.
- **Public hosting (GitHub Releases):** Everything stays on the local Pi json-store. No CA cert complexity for the OTA path.
- **json-store binary endpoint implementation:** Separate feature request to be filed with the json-store team.
