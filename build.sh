#!/usr/bin/env bash
# ABOUTME: Wrapper script for containerized build, flash, and test operations
# ABOUTME: Supports podman and docker; handles SELinux labels and USB device access

set -euo pipefail

SERIAL_PORT="${SERIAL_PORT:-/dev/ttyUSB0}"

if command -v podman >/dev/null 2>&1; then
    RUNNER=podman
    SELINUX_LABEL=":Z"
    USB_GROUP_FLAG=(--group-add keep-groups)
else
    RUNNER=docker
    SELINUX_LABEL=""
    USB_GROUP_FLAG=()
fi

IMAGE="weather-station-builder"

# All args except the last are extra container flags; the last arg is the shell command.
run_in_container() {
    local cmd="${*: -1}"
    local -a extra=("${@:1:$#-1}")
    "$RUNNER" run --rm "${extra[@]}" \
        -v "$(pwd):/project${SELINUX_LABEL}" \
        -w /project \
        "$IMAGE" \
        bash -c "$cmd"
}

require_device() {
    [ -e "${SERIAL_PORT}" ] || { echo "Device not found: ${SERIAL_PORT}"; exit 1; }
}

usage() {
    cat <<EOF
Usage: $0 <subcommand>

Subcommands:
  configure           Run cmake configure for firmware (also covers device test targets)
  build               Compile firmware
  flash               Flash firmware to device
  monitor             Open serial monitor
  test-host           Build and run host unit tests
  build-device-tests  Compile DeviceUnitTests sketch
  flash-device-tests  Flash DeviceUnitTests to device

Environment:
  SERIAL_PORT         Device path (default: /dev/ttyUSB0)
EOF
}

case "${1:-}" in
    configure)
        run_in_container \
            "cmake \
              -DCMAKE_TOOLCHAIN_FILE=/opt/arduino-cmake-toolchain/Arduino-toolchain.cmake \
              -DARDUINO_BOARD_OPTIONS_FILE=cmake/BoardOptions.cmake \
              -B build \
              -G Ninja"
        ;;
    build)
        run_in_container "cmake --build build"
        ;;
    flash)
        require_device
        run_in_container \
            "--device" "${SERIAL_PORT}" "${USB_GROUP_FLAG[@]}" \
            "SERIAL_PORT=${SERIAL_PORT} cmake --build build --target upload-WeatherStation"
        ;;
    monitor)
        require_device
        run_in_container \
            "--device" "${SERIAL_PORT}" "${USB_GROUP_FLAG[@]}" \
            "arduino-cli monitor --port ${SERIAL_PORT} --config baudrate=115200"
        ;;
    test-host)
        run_in_container \
            "cmake -S tests/unit -B build-host -G Ninja && cmake --build build-host && ./build-host/unit_tests"
        ;;
    build-device-tests)
        run_in_container "cmake --build build --target DeviceUnitTests"
        ;;
    flash-device-tests)
        require_device
        run_in_container \
            "--device" "${SERIAL_PORT}" "${USB_GROUP_FLAG[@]}" \
            "SERIAL_PORT=${SERIAL_PORT} cmake --build build --target upload-DeviceUnitTests"
        ;;
    *)
        usage
        exit 1
        ;;
esac
