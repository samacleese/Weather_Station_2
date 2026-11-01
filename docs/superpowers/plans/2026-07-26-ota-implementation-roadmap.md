# OTA Firmware Update — Implementation Roadmap

**Date:** 2026-07-26
**Spec:** [`docs/superpowers/specs/2026-04-13-ota-firmware-update-design.md`](../specs/2026-04-13-ota-firmware-update-design.md)
**Status:** Approved

## Context

The OTA design spec was approved in PR #25 and nothing has been implemented since. Its stated
prerequisites are now all closed: #10 (config extraction), #15 (containerized build), and #27
(board core upgrade — the project now builds against `soldered-inkplate-boards:esp32@3.0.0` with
`InkplateLibrary@11.1.2`).

This roadmap turns the spec into executable stories. It also records one scope change and five
places where the spec has drifted from the codebase since it was written.

## Scope change: asset externalization leaves the critical path

The spec's Phase 0 — moving fonts and cat images into a LittleFS partition and reconstructing
`GFXfont` structs in PSRAM — is a hard prerequisite there only because the spec assumes 1.25MB OTA
slots. The hardware does not require slots that small.

Flash is 4MB (`build/WeatherStation/flash_args`: `--flash-size 4MB`). The `soldered-inkplate-boards`
package already ships a dual-OTA scheme, `min_spiffs` ("Minimal SPIFFS (1.9MB APP with OTA/190KB
SPIFFS)"), with 1920KB slots:

```
nvs,      data, nvs,      0x9000,   0x5000
otadata,  data, ota,      0xe000,   0x2000
app0,     app,  ota_0,    0x10000,  0x1E0000   # 1,966,080 B
app1,     app,  ota_1,    0x1F0000, 0x1E0000   # 1,966,080 B
spiffs,   data, spiffs,   0x3D0000, 0x20000    # unused
coredump, data, coredump, 0x3F0000, 0x10000
```

The current binary is 1,757,408 B, leaving 209KB (10.6%) of headroom with no asset work at all.
The spec's layout, by contrast, leaves roughly 23KB after its projected ~470KB of savings — too
thin to ship against.

A custom table with no SPIFFS partition would give 1984KB slots (274KB headroom), but it requires
patching the installed board package's `boards.txt` in the `Containerfile`. The stock scheme was
chosen to avoid that; the 65KB difference is Track E's job to recover if it ever matters.

Asset externalization therefore becomes an independent size-reduction track (Track E), driven by
issues #20/#39/#14 and triggered when headroom actually erodes rather than by OTA. Build-time font
generation emitting only the sizes in use is the preferred lever there, and the PSRAM `GFXfont`
approach would make that harder to implement. The Phase 0 PSRAM font spike, `AssetLoader`, and
`asset_packer.py` are dropped rather than deferred.

## Spec corrections

| Spec says | Reality |
|---|---|
| `target_compile_definitions(WeatherStation PRIVATE FIRMWARE_VERSION=...)` (spec:142) | `project(WeatherStation NONE)` — `WeatherStation` is an `add_custom_target` shelling out to `arduino-cli`, so it has no compile properties. Defines go through the generated-header pattern instead: `configure_file(src/version.h.in → build/generated/weather_station_2/version.h)` (`cmake/Version.cmake:17`). |
| `--firmware build/WeatherStation.bin` (spec:165) | The artifact is `build/WeatherStation/Weather_Station_2.ino.bin` — arduino-cli names it after the sketch, not the CMake target. |
| `display.readBattery() + ADC_OFFSET` (spec:323, 336) | `ADC_OFFSET` was removed by PR #38. Use `InkplateBatteryReader::readVoltage()` (already constructed at `Weather_Station_2.ino:49`) and `BatteryMeter::voltageToPercent()`. |
| `#define OTA_MANIFEST_URL` hardcoded in the sketch (spec:246) | Breaks the config-extraction convention. It belongs in `config.cmake` / `config.cmake.example` / `src/user_settings.h.in` alongside `BATTERY_LOGGER_URL`. |
| The json-store binary endpoint "does not yet exist" (spec:26) | It exists and is deployed. json-store commit `274e8ec` implements `GET/PUT/DELETE /:app/binaries/:id`, verified live on the Pi. But `bin_id` is validated against `^[a-zA-Z0-9_-]+$`, so the spec's `binaries/firmware.bin` returns **400** — the id must be `firmware`, with no dot. |

Everything else in the spec stands: the ECDSA P-256 + `Prehashed()` sign/verify contract,
`OTA_SIZE_UNKNOWN` in `esp_ota_begin()`, the ESP-IDF native OTA API rather than Arduino `Update`,
the button-confirm safety requirement, and the local-network threat model.

## Story index

| Status | Story | Issue | Title |
|---|---|---|---|
| ✅ | A1 | #47 | `VERSION` file and `FIRMWARE_VERSION` constant |
| ✅ | A2 | #48 | `OTA_MANIFEST_URL` in config |
| ✅ | A3 | #49 | `tools/ota_keygen.py` and embedded public key |
| ✅ | A4 | #50 | `tools/ota_publish.py` |
| ❌ | A5 | #33 | Pin ArduinoJson host/device *(existing issue)* |
| ✅ | B1 | #51 | Dual-OTA partition scheme, verified on hardware |
| ✅ | B2 | #52 | Switch firmware target to the OTA scheme |
| ✅ | B3 | #53 | Binary size budget check |
| ✅ | C1 | #54 | Spike: streaming HTTP download with SHA-256 |
| ✅ | C2 | #55 | Spike: ECDSA P-256 verification on device |
| ✅ | C3 | #56 | Spike: OTA write to `app1` |
| ❌ | C4 | #57 | Spike: GPIO 36 poll during active operation |
| ❌ | D1 | #58 | `OTAManager::checkForUpdate` with host tests |
| ❌ | D2 | #59 | `OTAManager::performUpdate` |
| ❌ | D3 | #60 | Sketch integration and button confirmation |
| ❌ | D4 | #61 | End-to-end verification and negative tests |
| ❌ | E1 | #39, #20 | Build-time font generation, used sizes only *(existing issues)* |
| ❌ | E2 | #14 | Build-time kitty image generation *(existing issue)* |
| ❌ | E3 | #62 | LittleFS/PSRAM fallback if E1+E2 insufficient |

Status: ✅ done, 🔄 in progress or PR open, ❌ not started.

## Track dependencies

```
A (host tooling) ─┐
                  ├─> D (OTAManager) ─> D4 (end-to-end)
B (partitions) ───┴─> C (device spikes) ─┘

E (size reduction) — independent, triggered by B3
```

Tracks A and B are independent of each other and can run in parallel. C depends on B2. D depends
on A, B, and C.

---

## Track A — Host tooling

No hardware dependency.

### A1. `VERSION` file and `FIRMWARE_VERSION` constant

Add a `VERSION` file at the repo root (`YYYYMMDDNNN`, one line, trailing newline) as the single
source of truth for firmware version. Extend `cmake/Version.cmake` to read and strip it, and emit
`#define FIRMWARE_VERSION` into `src/version.h.in` alongside the existing `GIT_VERSION`.

`Version.cmake` has no `CMAKE_CONFIGURE_DEPENDS` re-trigger, so `GIT_VERSION` currently goes stale
until someone re-runs `./build.sh configure`. Add one for both `VERSION` and the git HEAD, matching
what `cmake/Layout.cmake:4` already does for `layout.json`.

The version exceeds `uint32_t`, so it must be declared `uint64_t` at the point of use.

### A2. `OTA_MANIFEST_URL` in config

Add `OTA_MANIFEST_URL` to `config.cmake.example` and a matching `#define` in
`src/user_settings.h.in`, following `BATTERY_LOGGER_URL`. Document it in `CLAUDE.md`.

### A3. `tools/ota_keygen.py` and embedded public key

One-time ECDSA P-256 keypair generation producing `keys/ecdsa_private.pem` (gitignored, kept
offline), `keys/ecdsa_public.pem`, and `src/security/OTAPublicKey.h` with the DER public key as a
`const uint8_t[]`.

Follow the house script convention: two `ABOUTME:` lines, then a module docstring with a `Usage:`
block, no shebang (see `tools/validate_battery_log.py`). Add `keys/` to `.gitignore`.

This needs the `cryptography` package. There is no `requirements.txt` today — add one and install
it in the `Containerfile` next to the existing `Pillow numpy pyserial`.

### A4. `tools/ota_publish.py`

Signs a built firmware image and publishes it with its manifest. Steps: read `VERSION` relative to
the script's own path; read `build/WeatherStation/Weather_Station_2.ino.bin`; compute the SHA-256
digest; sign the **32-byte digest** with
`key.sign(digest, ec.ECDSA(utils.Prehashed(hashes.SHA256())))`; base64-encode the DER signature;
write `manifest.json`; `PUT` the manifest to `/ota-firmware/data` and the binary to
`/ota-firmware/binaries/firmware`.

The `Prehashed()` call is the load-bearing detail — it is what makes the Python signature verifiable
by `mbedtls_ecdsa_verify()` on the device, which receives the digest rather than the 1.7MB binary.

Depends on A1 and A3.

### A5. Pin ArduinoJson to the same version on host and device (#33)

Pull this existing issue forward as an OTA prerequisite. The device is on 6.18.5 (`Containerfile`)
while host tests fetch 6.21.5 (`tests/unit/CMakeLists.txt`). D1's manifest parsing is host-unit-tested,
and this exact skew has already caused one silent `StaticJsonDocument` overflow where every parsed
field came back empty on the host but not the device.

---

## Track B — Partition table

### B1. Verify the `min_spiffs` partition scheme on hardware

No build changes: `PartitionScheme=min_spiffs` is already selectable via `--board-options`.

Verify with a throwaway sketch built with `PartitionScheme=min_spiffs` that
`esp_ota_get_next_update_partition()` returns non-NULL and `update_partition->label` prints `app1`.
This is the spike that gates the rest of Track B.

**Done 2026-11-01** on hardware. Serial output:

```
running: label=app0 subtype=0x10 address=0x010000 size=0x1e0000
boot: label=app0 subtype=0x10 address=0x010000 size=0x1e0000
next_update: label=app1 subtype=0x11 address=0x1f0000 size=0x1e0000
```

### B2. Switch the firmware target to the OTA scheme

Change the `WeatherStation` target's `--board-options` in `CMakeLists.txt` from
`PartitionScheme=huge_app` to `PartitionScheme=min_spiffs`. The two test-sketch targets stay on
`default`.

`cmake/BoardOptions.cmake` deliberately keeps `PartitionScheme` out of
`INKPLATE10_COMMON_BOARD_OPTIONS` because it differs per target; its comment names `huge_app` and
needs updating.

Flash over USB and confirm a normal weather cycle still renders. Depends on B1.

### B3. Binary size budget check

Add a post-build step comparing the `.bin` size against the partition's `upload.maximum_size` and
warning when headroom erodes. Without it, the first build that overflows a slot is discovered as a
flashing failure rather than a build warning. This is also the trigger mechanism for Track E.

The binary is already 89.4% of a `min_spiffs` slot, so the threshold cannot be the originally
proposed 85%, which would warn on every build from day one. B3 warns above 95%
(`FIRMWARE_SIZE_WARN_PERCENT` in `CMakeLists.txt`) and fails the build if the `.bin` exceeds the
`ota_1` partition.

---

## Track C — Device spikes

Run in order; each gates the next. Each is a throwaway sketch, not production code.

These should be standalone sketches modeled on `tests/GrayscaleCalibration/`, not AUnit tests — the
existing `DeviceUnitTests` target does not currently compile, because AUnit 1.7.1's `TEST` macro
expands to `test_<name>` and collides with `std::atomic_flag::test()` in libstdc++ 14.2.0.

All of Track C depends on B2.

### C1. Streaming HTTP download with SHA-256

Stream a binary from `/ota-firmware/binaries/firmware` in 4KB chunks, accumulating SHA-256, and log
the final hash. Use a bare `HTTPClient` following the `BatteryLogger` pattern —
`WeatherNetwork::get()` is HTTPS-only and sets a CA cert per hostname, so it cannot be reused for a
plain-HTTP local fetch.

**Success:** hash matches the expected value; completes without OOM or timeout.

**Done 2026-11-01** on hardware with the spike now at `tests/spikes/OtaUpdate/`
(`./build.sh build-spike OtaUpdate`, `./build.sh flash-spike OtaUpdate`, then
`python3 tools/serial_capture.py --seconds 120 --until SPIKE-END`). The spike reads the binary's
`url`, `size` and `sha256` from the published manifest at `OTA_MANIFEST_URL`, then downloads it
three times (the C2 version of the sketch downloads once):

| Run | Received | Time | Rate | Free heap: before / min during | SHA-256 |
|---|---|---|---|---|---|
| 1 | 1,757,408 B | 2.38 s | 739 kB/s | 224,416 / 215,796 B | match |
| 2 | 1,757,408 B | 2.40 s | 732 kB/s | 224,152 / 215,620 B | match |
| 3 | 1,757,408 B | 2.54 s | 691 kB/s | 224,152 / 214,280 B | match |

Streaming costs about 10KB of internal heap with a 4KB static buffer, and heap returns to the same
level each run. Largest free block stayed at 110,580 B. This measures download and hashing only;
C3 adds the flash-write cost.

### C2. ECDSA P-256 verification

Generate a keypair in Python, sign a test digest, hardcode key + signature + digest into a sketch,
and call `mbedtls_ecdsa_verify()`.

**Success:** returns 0 for a valid signature and non-zero for a tampered digest.

This is the spike most likely to fail and the one that validates the `Prehashed()` contract between
`ota_publish.py` and the device. It gates D2.

**Done 2026-11-01** on hardware, added to the same `tests/spikes/OtaUpdate/` sketch as C1. Instead of
a throwaway keypair, it checks the real release: the published manifest's signature (made by
`ota_publish.py`), the embedded `OTA_PUBLIC_KEY_DER` from `src/security/OTAPublicKey.h`, and the
digest of the firmware it just downloaded. The key loads with `mbedtls_pk_parse_public_key()` and
each case calls `mbedtls_pk_verify(MBEDTLS_MD_SHA256, digest, ...)`, which parses the DER signature
and calls `mbedtls_ecdsa_verify()`:

| Case | Return | Expected | Result |
|---|---|---|---|
| Manifest digest + manifest signature | `0` | valid | pass |
| Downloaded digest + manifest signature | `0` | valid | pass |
| Digest with one bit flipped | `-0x4E00` | rejected | pass |
| Signature with last byte flipped | `-0x4E00` | rejected | pass |

`-0x4E00` is `MBEDTLS_ERR_ECP_VERIFY_FAILED`: both rejections come from the signature math, not
from a parse error. The DER signature is 71 bytes. Each verification takes about 524 ms (software
ECC), which is fine once per update. The `Prehashed()` contract holds.

### C3. OTA write

Stream a test binary to `app1` via `esp_ota_begin`/`esp_ota_write`/`esp_ota_end`, set the boot
partition, and restart.

**Success:** `esp_ota_get_running_partition()->label` prints `app1`.

**Done 2026-11-01** on hardware, added to the same `tests/spikes/OtaUpdate/` sketch. After the C1 and
C2 checks it downloads a copy of its own binary and writes it to `app1`. The image that boots
from that write can then print its running partition, which the weather firmware doesn't do. That
copy was uploaded by hand to the Pi as `/ota-firmware/binaries/ota-update-spike`, next to the real
release, which is untouched:

```bash
curl -X PUT -H "Content-Type: application/octet-stream" \
    --data-binary @build/tests/spikes/OtaUpdate/OtaUpdate/OtaUpdate.ino.bin \
    http://192.168.1.2:5000/ota-firmware/binaries/ota-update-spike
```

When it boots from `app0`, the sketch calls `esp_ota_begin(target, size)`, streams 4KB chunks
through `esp_ota_write`, then calls `esp_ota_end` and `esp_ota_set_boot_partition`, and restarts.
When it boots from any other slot, it reports and stops, so it doesn't update in a loop.

| Check | Result |
|---|---|
| Boot after OTA write | `Running partition: app1 at 0x1f0000` |
| Plain reset afterwards | still `app1` |
| USB reflash | `app0`; the upload rewrites otadata at `0xe000` |
| SHA-256 of the streamed image | matches the uploaded binary |

Timing for the 1,050,624 B spike image:

| Step | Time |
|---|---|
| Erase (`esp_ota_begin`) | 1,165 ms |
| Download and write | 4,518 ms, of which 3,063 ms is `esp_ota_write` |
| Finalize (`esp_ota_end`) | 203 ms |

Writing to flash roughly halves the throughput seen in C1. Scaled linearly to the 1,757,408 B
firmware, a full update should take about 2 s to erase plus about 7.5 s to stream. That is an
estimate; D2 will measure the real figure.

### C4. GPIO 36 poll during active operation

GPIO 36 is already wired as the `ext1` deep-sleep wake source. Confirm it reads reliably while the
device is awake, which is a different condition from the deep-sleep wakeup configuration.

**Success:** a press reads LOW, idle reads HIGH.

---

## Track D — OTAManager

### D1. `checkForUpdate` and `UpdateInfo`

New module `src/ota/OTAManager.h/.cpp`. Fetches the manifest over plain HTTP, parses it with
`StaticJsonDocument<1024>`, and compares the manifest version against the compiled-in
`FIRMWARE_VERSION`.

Validate that every one of `version`, `size`, `sha256`, `signature`, `url`, and `min_battery_mv` is
present and non-empty before populating `UpdateInfo`. ArduinoJson silently yields 0 for missing
integers and an empty string for missing strings, so a missing `url` would otherwise flow into
`performUpdate()` unnoticed. Treat any missing field as "no update available".

Inject the transport behind the existing `IHttpClient` interface so this is host-unit-testable the
same way `CurrentConditions` is. New sources must be added to the explicit `add_executable` list in
`tests/unit/CMakeLists.txt` — that list is not globbed.

Depends on A1, A2, A5.

### D2. `performUpdate`

Uses the ESP-IDF native OTA API (`esp_ota_ops.h`) exclusively — it must never be mixed with the
Arduino `Update` library, and the native API is what exposes `esp_ota_abort()` and partition-level
control.

The call order is the safety property:

1. `esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &handle)`
2. per 4KB chunk: `esp_ota_write()` plus SHA-256 accumulate
3. verify the computed SHA-256 against the manifest
4. verify the ECDSA signature over that digest with the embedded public key
5. only if both pass: `esp_ota_end()` → `esp_ota_set_boot_partition()` → `esp_restart()`
6. on either failure: `esp_ota_abort(handle)` and return false, leaving the running partition
   untouched

`esp_ota_abort()` before `esp_ota_end()` is always safe; calling it after is undefined. That is why
`esp_ota_end()` comes only after both checks.

`OTA_SIZE_UNKNOWN` avoids making the unsigned manifest `size` field a second source of truth — byte
count is implicitly verified when the SHA-256 matches. Set `HTTPClient::setTimeout()` to 30s so a
dropped connection cannot hang the device with WiFi active. Render download progress against
`info.size` roughly every 64KB, so a mid-stream dropout is distinguishable from normal operation.

Depends on C1, C2, C3.

### D3. Sketch integration

Insert the OTA check in `Weather_Station_2.ino` after `network->begin()` and before the weather
fetch.

Implementation notes that will otherwise bite:

- `display.printf()` is not part of the Inkplate/Print API — use `snprintf` then `print`.
- `PRIu64` from `<inttypes.h>`, not `%llu`, which is broken on ESP32 Arduino's newlib-nano.
- The font maps are `std::map`, and `.at()` on an unmapped key is a runtime failure. `Roboto_Medium.at(35)` exists; verify any other size against `layout.json` before using it.
- Battery gate reads `InkplateBatteryReader::readVoltage()` at confirmation time and compares against `info.min_battery_mv`. The splash also shows battery, but the confirmation-time reading is the one that matters for flash safety.
- Both failure paths — battery too low, and update failed — fall through to normal weather display.
- New layout fields go through all four files per the layout convention in `CLAUDE.md`: `layout.json`, `cmake/Layout.cmake`, `src/layout.h.in`, and `tools/layout-editor/index.html`.

Depends on D1, D2, C4.

### D4. End-to-end verification

| Case | Expected |
|---|---|
| Confirm path: publish → device detects → button press | Flashes, restarts, boots the new firmware |
| Skip path: device detects → no press → 30s elapses | Falls through to normal weather display |
| Tampered SHA-256 | Rejected; running partition untouched |
| Tampered signature | Rejected; running partition untouched |
| Battery below `min_battery_mv` | Refused with a message; falls through |
| Missing manifest field | Treated as no update available |
| Mid-stream WiFi drop | Times out, returns false, falls through |

Recovery path for a bad image that does ship: reflash over USB. Automatic rollback is explicitly out
of scope.

---

## Track E — Binary size reduction

Independent of OTA, triggered by B3 when headroom erodes. Sequenced by whichever lands first.

- **E1** — #39 and #20 together: generate fonts at build time via `fontconvert` from a declarative
  config, emitting only the sizes referenced by `layout.json` (currently 8 of the 15 sizes compiled
  in). Delete the committed `assets/fonts/*.h`, including three standalone headers no source file
  includes: `Roboto_Medium_150.h`, `Roboto_Light_48.h`, `Roboto_Light_36.h`.
- **E2** — #14: generate `KittyPics` and `Kitties` at build time from the PNGs in `assets/images/`
  (~270KB of PROGMEM from 1.6MB of committed generated source).
- **E3** — Only if E1 and E2 prove insufficient: revisit a LittleFS/PSRAM asset partition, which
  requires re-cutting the partition table.

---

## Verification

| Track | How |
|---|---|
| A | `./build.sh test-host`; run `ota_keygen.py` and `ota_publish.py` on the host, then confirm the manifest at `/ota-firmware/data` parses and the fetched binary's `sha256sum` matches the manifest field |
| B | `./build.sh configure && ./build.sh build && ./build.sh flash`; B1's spike prints `app1`; a normal weather cycle still renders after B2 |
| C | Each spike's serial output against its stated success criterion |
| D | `./build.sh test-host` for D1; D4's matrix on hardware for the rest |
