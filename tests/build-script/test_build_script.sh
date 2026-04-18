#!/usr/bin/env bash
# ABOUTME: Tests for build.sh pure-logic behavior (no container invocation required)
# ABOUTME: Covers usage output, device guard, and exit codes

set -euo pipefail

SCRIPT="$(cd "$(dirname "$0")/../.." && pwd)/build.sh"
PASS=0
FAIL=0

assert_exit_code() {
    local desc="$1" expected="$2"
    shift 2
    local actual=0
    "$@" >/dev/null 2>&1 || actual=$?
    if [ "$actual" -eq "$expected" ]; then
        echo "PASS: $desc"
        PASS=$((PASS + 1))
    else
        echo "FAIL: $desc (expected exit $expected, got $actual)"
        FAIL=$((FAIL + 1))
    fi
}

assert_output_contains() {
    local desc="$1" pattern="$2"
    shift 2
    local output
    output=$("$@" 2>&1 || true)
    if echo "$output" | grep -qF "$pattern"; then
        echo "PASS: $desc"
        PASS=$((PASS + 1))
    else
        echo "FAIL: $desc (expected output to contain '$pattern')"
        echo "  Got: $output"
        FAIL=$((FAIL + 1))
    fi
}

# no args → usage + exit 1
assert_exit_code       "no args exits 1"       1 "$SCRIPT"
assert_output_contains "no args shows usage"   "Usage" "$SCRIPT"

# unknown subcommand → usage + exit 1
assert_exit_code       "unknown subcommand exits 1"       1 "$SCRIPT" foobar
assert_output_contains "unknown subcommand shows usage" "Usage" "$SCRIPT" foobar

# device guard — nonexistent device → exit 1 with clear message
assert_exit_code       "flash: missing device exits 1"            1 env SERIAL_PORT=/dev/nonexistent "$SCRIPT" flash
assert_output_contains "flash: missing device error message" "Device not found" \
    env SERIAL_PORT=/dev/nonexistent "$SCRIPT" flash

assert_exit_code "monitor: missing device exits 1"             1 env SERIAL_PORT=/dev/nonexistent "$SCRIPT" monitor
assert_exit_code "flash-device-tests: missing device exits 1"  1 env SERIAL_PORT=/dev/nonexistent "$SCRIPT" flash-device-tests

echo ""
echo "Results: $PASS passed, $FAIL failed"
[ "$FAIL" -eq 0 ]
