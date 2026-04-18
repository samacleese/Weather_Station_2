# Containerized Build Environment Implementation Plan

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Commit a `Containerfile` that fully pins the Weather Station build toolchain (arduino-cli, board support package, all libraries) so any developer can reproduce the exact build environment without fighting Arduino IDE setup.

**Architecture:** A single Ubuntu 24.04 container installs arduino-cli 1.3.1, the Inkplate board support package, all required Arduino libraries, and Python tools. Users mount their project directory (which includes their personal `config.cmake`) and run cmake configure + build inside the container. Flash is done by passing `/dev/ttyUSB0` through to the container.

**Tech Stack:** Ubuntu 24.04, arduino-cli 1.3.1, CMake + Ninja, Podman/Docker

---

## File Map

| File | Action | Responsibility |
|---|---|---|
| `Containerfile` | Create | Pinned build environment definition |
| `docs/README.md` | Modify | Add "Building with the container" section documenting image build, compile, and flash |

---

## Chunk 1: Containerfile + README

### Task 1: Create feature branch

- [ ] **Step 1: Create and check out the feature branch**

  ```bash
  git checkout -b feat/containerized-build
  ```

---

### Task 2: Write the Containerfile

**Files:**
- Create: `Containerfile`

Before writing, verify the board index URL works. The local `~/.arduino15/package_Croduino_Boards_index.json` was placed there manually. Check the Dasduino/Soldered URL from the issue sketch is reachable:

```bash
curl -fsSL --head \
  "https://github.com/SolderedElectronics/Dasduino-Board-Support-Package-Installer/raw/master/package_Dasduino_Boards_index.json" \
  | head -5
```

Expected: HTTP 200 (or 302 redirect). If this URL is dead, fall back to:
`https://github.com/SolderedElectronics/Croduino-Board-Definitions-for-Arduino-IDE/raw/master/package_Croduino_Boards_index.json`
(the board package itself downloads from `github.com/SolderedElectronics/Croduino-Board-Definitions-for-Arduino-IDE`).

- [ ] **Step 2: Create `Containerfile`**

  Pinned versions (from `arduino-cli core list` and `arduino-cli lib list` on the reference machine):
  - arduino-cli: `1.3.1`
  - Board: `Croduino_Boards:Inkplate@1.0.1`
  - ArduinoJson: `6.18.5`
  - ArduinoLog: `1.1.1`
  - InkplateLibrary: `10.2.2`
  - LCBUrl: `1.1.4`

  ```dockerfile
  # ABOUTME: Container build environment for Weather Station firmware.
  # ABOUTME: Pins arduino-cli, board support, and all library versions for reproducible builds.
  FROM ubuntu:24.04

  RUN apt-get update && apt-get install -y --no-install-recommends \
      curl \
      python3 \
      python3-pip \
      cmake \
      ninja-build \
      git \
      clang-format \
      && rm -rf /var/lib/apt/lists/*

  # Pin arduino-cli version
  RUN curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh \
      | BINDIR=/usr/local/bin sh -s 1.3.1

  # Pin board support package
  RUN arduino-cli config add board_manager.additional_urls \
          https://github.com/SolderedElectronics/Dasduino-Board-Support-Package-Installer/raw/master/package_Dasduino_Boards_index.json \
      && arduino-cli core update-index \
      && arduino-cli core install Croduino_Boards:Inkplate@1.0.1

  # Pin libraries
  RUN arduino-cli lib install \
      "ArduinoJson@6.18.5" \
      "ArduinoLog@1.1.1" \
      "InkplateLibrary@10.2.2" \
      "LCBUrl@1.1.4"

  # Python tooling for image codegen (tools/image_converter.py)
  RUN pip3 install --no-cache-dir --break-system-packages Pillow numpy
  ```

---

### Task 3: Verify container builds

- [ ] **Step 3: Build the container image**

  ```bash
  podman build -t weather-station-builder .
  ```

  Expected: build completes without error. Each `RUN` layer should succeed.

  **If `arduino-cli lib install "InkplateLibrary@10.2.2"` fails:** The library may have a different name in the Arduino Library Manager. Try `arduino-cli lib search inkplate` inside a running container to find the correct name. If it's only available via git (not the library registry), replace the lib install line with:
  ```dockerfile
  RUN git clone --depth 1 --branch 10.2.2 \
      https://github.com/SolderedElectronics/Inkplate-Arduino-library.git \
      /root/Arduino/libraries/Inkplate
  ```

  **If the board URL returns 404:** Try the fallback URL mentioned in Task 2.

- [ ] **Step 4: Verify Arduino libraries are discoverable**

  The CMakeLists.txt references libraries as `Inkplate`, `ArduinoJson`, `ArduinoLog`, `LCBUrl`. Arduino-CMake-Toolchain finds them via `library.properties` in `$HOME/Arduino/libraries/`. Verify the installed library names match:

  ```bash
  podman run --rm weather-station-builder \
      bash -c "ls ~/Arduino/libraries/"
  ```

  Expected output lists directories including: `ArduinoJson`, `ArduinoLog`, `LCBUrl`, and one containing `InkplateLibrary` or `Inkplate`.

  The `library.properties` `name=` field must match what CMakeLists.txt requests. Check:
  ```bash
  podman run --rm weather-station-builder \
      bash -c "grep '^name=' ~/Arduino/libraries/InkplateLibrary/library.properties"
  ```
  Expected: `name=Inkplate` (or similar). If the name is `InkplateLibrary`, the CMakeLists.txt will also need updating — but don't change CMakeLists.txt unless the build fails with a "library not found" error; try the build first (Step 5).

---

### Task 4: Verify firmware compiles inside the container

The project requires `config.cmake` at the repo root (it holds WiFi credentials and is gitignored). A minimal stub is enough to test the build:

- [ ] **Step 5: Create a test config stub and run cmake configure**

  ```bash
  # Create a minimal config for build testing (credentials don't matter for compilation)
  cat > /tmp/test-config.cmake <<'EOF'
  set(WIFI_SSID "test")
  set(WIFI_PASSWORD "test")
  set(WEATHER_STATION_ID "KBFI")
  set(BATTERY_LOGGER_URL "http://localhost/path")
  EOF

  podman run --rm \
      -v $(pwd):/project:Z \
      -v /tmp/test-config.cmake:/project/config.cmake:Z \
      -w /project \
      weather-station-builder \
      bash -c "rm -rf build && cmake \
          -DCMAKE_TOOLCHAIN_FILE=cmake/Arduino-CMake-Toolchain/Arduino-toolchain.cmake \
          -DARDUINO_BOARD_OPTIONS_FILE=cmake/BoardOptions.cmake \
          -B build \
          -G Ninja"
  ```

  Expected: cmake configure completes successfully and prints board/toolchain detection info.

- [ ] **Step 6: Run the firmware compile**

  ```bash
  podman run --rm \
      -v $(pwd):/project:Z \
      -v /tmp/test-config.cmake:/project/config.cmake:Z \
      -w /project \
      weather-station-builder \
      cmake --build build
  ```

  Expected: compiles without errors, produces `build/WeatherStation.bin`.

  **If build fails with "Inkplate not found":** The library name mismatch described in Step 4 is the likely cause. Either the `InkplateLibrary` directory needs to be symlinked/renamed in the container, or the git-based install from Step 3's fallback installs to a path CMake can find. Debug by inspecting which path Arduino-CMake-Toolchain is searching:
  ```bash
  podman run --rm -v $(pwd):/project:Z -w /project weather-station-builder \
      cmake -DCMAKE_TOOLCHAIN_FILE=cmake/Arduino-CMake-Toolchain/Arduino-toolchain.cmake \
            -DARDUINO_BOARD_OPTIONS_FILE=cmake/BoardOptions.cmake \
            -B build -G Ninja 2>&1 | grep -i inkplate
  ```

---

### Task 5: Update docs/README.md

**Files:**
- Modify: `docs/README.md`

Add a "Building with the container" section. Place it after the existing "Installation" section (which currently describes the Arduino IDE path — leave that section intact).

- [ ] **Step 7: Add container build section to docs/README.md**

  ```markdown
  ## Container build (recommended)

  A `Containerfile` at the repo root pins all toolchain dependencies for reproducible builds.

  ### Build the image

  ```bash
  # Podman
  podman build -t weather-station-builder .

  # Docker
  docker build -t weather-station-builder .
  ```

  ### Compile firmware

  ```bash
  # Copy config.cmake.example → config.cmake and fill in your credentials first.
  cp config.cmake.example config.cmake
  # edit config.cmake

  # Podman (Linux with SELinux)
  podman run --rm -v $(pwd):/project:Z -w /project weather-station-builder \
      bash -c "cmake \
          -DCMAKE_TOOLCHAIN_FILE=cmake/Arduino-CMake-Toolchain/Arduino-toolchain.cmake \
          -DARDUINO_BOARD_OPTIONS_FILE=cmake/BoardOptions.cmake \
          -B build -G Ninja \
      && cmake --build build"

  # Docker / Podman without SELinux (drop the :Z)
  docker run --rm -v $(pwd):/project -w /project weather-station-builder \
      bash -c "cmake \
          -DCMAKE_TOOLCHAIN_FILE=cmake/Arduino-CMake-Toolchain/Arduino-toolchain.cmake \
          -DARDUINO_BOARD_OPTIONS_FILE=cmake/BoardOptions.cmake \
          -B build -G Ninja \
      && cmake --build build"
  ```

  ### Flash firmware

  The device appears on `/dev/ttyUSB0`. Your host account needs to be in the `dialout` group.

  ```bash
  # Podman
  podman run --rm \
      -v $(pwd):/project:Z \
      --device /dev/ttyUSB0 \
      -w /project \
      weather-station-builder \
      bash -c "SERIAL_PORT=/dev/ttyUSB0 cmake --build build --target upload-WeatherStation"

  # Docker
  docker run --rm \
      -v $(pwd):/project \
      --device /dev/ttyUSB0 \
      -w /project \
      weather-station-builder \
      bash -c "SERIAL_PORT=/dev/ttyUSB0 cmake --build build --target upload-WeatherStation"
  ```
  ```

---

### Task 6: Commit

- [ ] **Step 8: Stage and commit**

  ```bash
  git add Containerfile docs/README.md
  git status  # verify only these two files are staged
  git commit -m "feat: add Containerfile with pinned build toolchain

  Pins arduino-cli 1.3.1, Croduino_Boards:Inkplate 1.0.1, and all
  Arduino library versions so the build environment is fully reproducible
  without requiring a local Arduino IDE installation.

  Closes #15"
  ```
