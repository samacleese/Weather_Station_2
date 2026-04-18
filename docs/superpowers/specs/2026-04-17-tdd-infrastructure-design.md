# TDD Infrastructure Design

## Context

The Weather Station 2 codebase grew from a hobbyist example sketch and lacks test coverage.
Adding features (e.g. a 24-hour rainfall graph) is risky without tests, and the code is
tightly coupled to hardware, making logic bugs hard to catch before flashing. This design
introduces a hybrid testing approach: GoogleTest for pure C++ logic on the host, AUnit for
hardware integration on the Inkplate device. A minimal interface refactor unlocks testing
of the most complex logic (`CurrentConditions::update()`) without over-engineering.

---

## Phase 0: Dependency Setup

**Goal:** Wire in test libraries as git submodules before writing any tests.

### Submodules

| Library | URL | Path |
|---------|-----|------|
| GoogleTest | https://github.com/google/googletest | `libs/googletest` |
| AUnit | https://github.com/bxparks/AUnit | `libs/AUnit` |

### CMake Integration

- **Host tests:** A separate CMake configuration (`build-host/`) is used — no Arduino
  toolchain. The `if(NOT CMAKE_CROSSCOMPILING)` guard that pulls in GoogleTest and
  `tests/unit/` lives in `tests/CMakeLists.txt` (not the root), so the root file is
  unchanged and the device build is unaffected.
- **Device tests:** AUnit is declared as a dependency via `target_link_arduino_libraries`
  in each device test sketch's `CMakeLists.txt`, pointing at `libs/AUnit`.

### Deliverable

`cmake -B build-host && cmake --build build-host --target tests` compiles and runs an
empty GoogleTest suite. Existing `cmake -B build` firmware build is unaffected.

---

## Phase 1: Pure Logic Tests

**Goal:** Test all logic that requires zero hardware, with minimal source changes.

### Source Changes

| Location | Change |
|----------|--------|
| `CACerts::getCert()` | Parameter changes from `const String&` to `const std::string&`; map key type changes from `std::map<String, …>` to `std::map<std::string, …>`. Return type is already `const char*` — no change. |
| `CurrentConditions::validateData()` | Move from `private` to `public` to enable direct testing. No other changes — the method has no `String` usage (operates only on `int` members). |
| `Kitties.h` | Add portability guard: `#ifndef RTC_DATA_ATTR` / `#define RTC_DATA_ATTR` / `#endif`. `RTC_DATA_ATTR` is an ESP32 macro absent on the host; the guard makes `Kitties` compile in both environments. Behavior on device is unchanged. |

Call sites in `Weather_Station_2.cpp` get a thin `String(result.c_str())` conversion where
`getCert()` is called. No other files change.

### Host Tests (`tests/unit/`)

| File | Tests |
|------|-------|
| `CACertsTest.cpp` | Known hostname returns the cert (`const char*`); unknown hostname returns `nullptr` |
| `KittiesTest.cpp` | Index cycles through 0–4 via modulo after 5 calls; after 256 calls (uint8_t overflow) the cycle still works correctly |
| `CurrentConditionsTest.cpp` | `validateData()` passes valid data; rejects out-of-range temperature; rejects out-of-range dew point; rejects dew point more than 5°C above temperature; rejects negative or excessive wind speed; `getErrorString()` maps all seven error code constants to non-empty strings |

### Device Sketch (`tests/device/unit/`)

Same assertions compiled with AUnit + `aunit/contrib/gtest.h` adapter.

**Constraint:** The AUnit gtest adapter supports only `ASSERT_*` macros. `EXPECT_*` macros
and `TEST_F()` are not available. All tests in both host and device suites must be written
using `ASSERT_*` and plain `TEST()` blocks to ensure the same test code compiles on both
platforms.

Serial output monitored manually for PASS/FAIL.

### Deliverable

`cmake --build build-host --target tests` passes. Device sketch flashes and reports all
green over serial.

---

## Phase 2: Minimal Interface Refactor

**Goal:** Make `CurrentConditions::update()` fully testable on the host via two minimal
interfaces. No other abstractions introduced (YAGNI).

### Interfaces

**`IClock`** — abstracts `millis()` for cache timeout logic:
```cpp
class IClock {
public:
    virtual ~IClock() = default;
    virtual unsigned long millis() const = 0;
};
```

**`IHttpClient`** — abstracts the HTTP GET operation:
```cpp
class IHttpClient {
public:
    virtual ~IHttpClient() = default;
    virtual int get(const std::string& url, std::string& body) = 0;
};
```

### Concrete Implementations

| Interface | Production Implementation |
|-----------|--------------------------|
| `IClock` | `SystemClock` — calls `::millis()` |
| `IHttpClient` | `Network` — adds a new `get(const std::string&, std::string&)` override implementing the interface. The existing `get(WiFiClientSecure&, …)` signature is unchanged and used internally. |

**Cert lookup migration:** `CACerts::getCert()` is currently called in
`CurrentConditions::update()`. When `Network` implements `IHttpClient`, the cert lookup
moves inside `Network`'s `IHttpClient::get()` implementation, since `WiFiClientSecure` is
only visible there. `CurrentConditions` no longer references `CACerts` directly.

### Refactor

- `CurrentConditions` constructor signature changes to take `IHttpClient&` and `IClock&`
  instead of `shared_ptr<Network>`
- `m_network` member type changes from `std::shared_ptr<Network>` to `IHttpClient&`
  (non-owning reference). This makes `CurrentConditions` non-copyable and
  non-move-assignable — acceptable given the embedded lifecycle (all objects constructed
  in `setup()`, nothing is copied)
- All `millis()` call sites inside `CurrentConditions` (lines 67 and 103) replaced with
  `m_clock.millis()`
- `Weather_Station_2.cpp` constructs a `SystemClock` instance and passes it alongside
  `Network` to `CurrentConditions`

### New Tests Unlocked (`CurrentConditionsTest.cpp`)

Note: the cache timeout is 1,800,000 ms (30 minutes). Clock mocks must advance by more
than this value to trigger a re-fetch.

- Cache returns stale data when clock has not advanced past 1,800,000 ms
- Cache expires and triggers re-fetch when clock advances past 1,800,000 ms
- `update()` parses and validates a canned JSON response correctly
- `update()` handles network errors and HTTP error codes
- `update()` rejects malformed JSON

### Deliverable

`CurrentConditions` fully covered on host. `Network` hardware behavior remains
device-only.

---

## Phase 3: Rainfall Graph (Placeholder)

Out of scope for this spec. Gets its own brainstorm and design after Phases 0–2 are
merged. The clean foundation (testable `CurrentConditions`, injectable interfaces) enables
the graph feature to be built test-first from day one.

Anticipated scope: fetch 24-hour precipitation forecast from `api.weather.gov`, render a
graph on the Inkplate display.

---

## Verification

| Phase | How to verify |
|-------|---------------|
| 0 | `cmake -B build-host && cmake --build build-host --target tests` runs empty suite; `cmake -B build` firmware build unchanged |
| 1 | Host test suite passes; device unit sketch reports all PASS over serial |
| 2 | Host test suite passes including `CurrentConditions::update()` tests; firmware boots and functions normally |
