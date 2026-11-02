# Precipitation Forecast Strip Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the mostly-empty METAR line with a 4-day chance-of-precipitation strip (`Precip:  Today 1%   Tue 0%   Wed 87%❄   Thu 25%`) fetched from the weather.gov gridpoint forecast.

**Architecture:** A new host-testable `PrecipForecast` class (same shape as `CurrentConditions`: `IHttpClient&` injection, ArduinoJson filter) fetches and parses `/gridpoints/{wfo}/{x},{y}/forecast` into up to four `PrecipDay` records. `setup()` in `Weather_Station_2.ino` draws them, putting a snowflake bitmap after snowy days. The bitmap is generated once from DejaVu Sans by a new `tools/glyph_to_bitmap.py`. Network-result classification and log macros move into small shared headers so the two weather.gov clients don't duplicate them.

**Tech Stack:** C++11 (ESP32 Arduino core + host gtest), ArduinoJson 6.21.6, CMake, Python 3 + Pillow + pytest.

**Spec:** `docs/superpowers/specs/2026-11-02-precip-forecast-design.md`

## Global Constraints

- Every code file starts with two `// ABOUTME: ` (or `# ABOUTME: `) lines.
- C++11 only (host tests set `CMAKE_CXX_STANDARD 11`). No `std::make_unique` in `src/`.
- Google style, 120 cols, via `clang-format -i <file>` on new/changed `src/` C++ files. Don't reformat unrelated lines.
- Display label is `Precip:`. Failure text is `Precip forecast unavailable`. Unknown percent is `--%`.
- Layout `font_size` must be one of `12, 14, 17, 20, 24, 29, 35, 42, 50, 60, 72, 87, 104, 125, 150` (`Roboto_*.at()` throws otherwise). The strip uses **42**.
- Gridpoint config key: `WEATHER_FORECAST_GRIDPOINT`, KBFI value `"SEW/125,65"`. Snow test gridpoint `"SEW/166,121"`.
- Snow = daytime `shortForecast` contains `snow`, case-insensitive.
- Test output must be pristine. Run host tests with `./build.sh test-host`, Python with `./build.sh test-python` (both in the container).
- All work happens in the worktree `/home/jesse/Arduino/Weather_Station_2/.worktrees/rain-forecast` on branch `feat/rain-forecast`. Commit messages are conventional commits ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## File Map

| File | Responsibility |
|---|---|
| `src/network/NetworkResult.h` (new) | `classifyNetworkResult(int)` → `NetworkFailure` category |
| `src/network/NetworkLog.h` (new) | `NET_LOG_*` macros: ArduinoLog on device, no-ops on host |
| `src/network/PrecipForecast.h/.cpp` (new) | Fetch + parse the gridpoint forecast into `PrecipDay[4]`; pure helpers for weekday, snow, formatting |
| `src/network/CurrentConditions.h/.cpp` | Uses the two shared headers; METAR (`raw_message`) removed |
| `src/display/Snowflake.h` (new, generated) | 1-bit snowflake bitmap, `Snowflake::w/h/bitmap` |
| `tools/glyph_to_bitmap.py` (new) | Renders one font glyph into a PROGMEM header |
| `Weather_Station_2.ino` | Fetches the forecast and draws the strip |
| `layout.json`, `cmake/Layout.cmake`, `src/layout.h.in`, `tools/layout-editor/index.html` | `metar` → `precip_forecast` |
| `CMakeLists.txt`, `config.cmake.example`, `src/user_settings.h.in` | `WEATHER_FORECAST_GRIDPOINT` plumbing |
| `tests/unit/NetworkResultTest.cpp`, `tests/unit/PrecipForecastTest.cpp` (new), `tests/unit/fixtures/*.json` (new), `tests/unit/CMakeLists.txt`, `tests/unit/CurrentConditionsTest.cpp` | Host tests |
| `tests/python/test_glyph_to_bitmap.py` (new) | Python tests |
| `CLAUDE.md` | Docs for the new endpoint and config |

---

### Task 1: Shared network-result and log helpers

Extract the network-error switch and the log-macro block from `CurrentConditions.cpp` so `PrecipForecast` can reuse them.

**Files:**
- Create: `src/network/NetworkResult.h`, `src/network/NetworkLog.h`, `tests/unit/NetworkResultTest.cpp`
- Modify: `src/network/CurrentConditions.cpp` (lines 5–17 macro block, lines 50–64 switch), `tests/unit/CurrentConditionsTest.cpp`, `tests/unit/CMakeLists.txt`

**Interfaces:**
- Produces: `enum class NetworkFailure { None, Connection, Certificate, Other };` and `inline NetworkFailure classifyNetworkResult(int result);` in `network/NetworkResult.h`. Macros `NET_LOG_ERROR/WARNING/NOTICE/INFO(fmt, ...)` in `network/NetworkLog.h`.

- [ ] **Step 1: Add a characterization test for the cert path in CurrentConditions**

The existing tests don't cover `-5`. Add this after `NetworkErrorReturnsCCNetworkError` in `tests/unit/CurrentConditionsTest.cpp`:

```cpp
TEST(UpdateTest, CertErrorReturnsCCCertError) {
    StubHttpClient http;
    StubClock clock;
    http.next_result = -5;  // NETWORK_CERT_ERROR value

    CurrentConditions cc(http, clock, "KBFI");
    ASSERT_EQ(cc.update(), CURRENT_CONDITIONS_CERT_ERROR);
}

TEST(UpdateTest, UnknownNetworkResultReturnsGeneralError) {
    StubHttpClient http;
    StubClock clock;
    http.next_result = -99;

    CurrentConditions cc(http, clock, "KBFI");
    ASSERT_EQ(cc.update(), CURRENT_CONDITIONS_ERROR);
}
```

- [ ] **Step 2: Run, expect PASS** (it characterizes current behavior before the refactor)

Run: `./build.sh test-host`
Expected: all tests pass, including the two new ones.

- [ ] **Step 3: Write the failing NetworkResult test**

Create `tests/unit/NetworkResultTest.cpp`:

```cpp
// ABOUTME: Host unit tests for classifyNetworkResult().
// ABOUTME: Checks every NetworkError value maps to the right failure category.
#include <gtest/gtest.h>

#include "network/NetworkResult.h"

TEST(ClassifyNetworkResultTest, ZeroIsNone) { ASSERT_EQ(classifyNetworkResult(0), NetworkFailure::None); }

TEST(ClassifyNetworkResultTest, ConnectionErrorsAreConnection) {
    for (int code : {-1, -2, -3, -4}) {
        ASSERT_EQ(classifyNetworkResult(code), NetworkFailure::Connection) << "code " << code;
    }
}

TEST(ClassifyNetworkResultTest, CertErrorIsCertificate) {
    ASSERT_EQ(classifyNetworkResult(-5), NetworkFailure::Certificate);
}

TEST(ClassifyNetworkResultTest, UnknownCodeIsOther) {
    ASSERT_EQ(classifyNetworkResult(-6), NetworkFailure::Other);
    ASSERT_EQ(classifyNetworkResult(7), NetworkFailure::Other);
}
```

Add `NetworkResultTest.cpp` to the `add_executable(unit_tests ...)` list in `tests/unit/CMakeLists.txt`, right after `CurrentConditionsTest.cpp`.

- [ ] **Step 4: Run, expect a build FAIL**

Run: `./build.sh test-host`
Expected: compile error `network/NetworkResult.h: No such file or directory`.

- [ ] **Step 5: Create `src/network/NetworkResult.h`**

```cpp
// ABOUTME: Classifies IHttpClient::get() result codes into failure categories.
// ABOUTME: Shared by the weather.gov clients so each maps failures onto its own error codes.
#ifndef NETWORK_RESULT_H
#define NETWORK_RESULT_H

enum class NetworkFailure { None, Connection, Certificate, Other };

// The values mirror the NetworkError enum in Network.h. That header isn't included here,
// so host tests compile without WiFi headers. Keep the two in sync.
inline NetworkFailure classifyNetworkResult(int result) {
    switch (result) {
        case 0:
            return NetworkFailure::None;
        case -1:  // NETWORK_WIFI_ERROR
        case -2:  // NETWORK_HTTP_ERROR
        case -3:  // NETWORK_TIMEOUT_ERROR
        case -4:  // NETWORK_NO_DATA
            return NetworkFailure::Connection;
        case -5:  // NETWORK_CERT_ERROR
            return NetworkFailure::Certificate;
        default:
            return NetworkFailure::Other;
    }
}

#endif  // NETWORK_RESULT_H
```

- [ ] **Step 6: Create `src/network/NetworkLog.h`**

```cpp
// ABOUTME: Logging macros for network clients: ArduinoLog on device, no-ops in host tests.
// ABOUTME: Lets the weather.gov clients log without pulling Arduino headers into host builds.
#ifndef NETWORK_LOG_H
#define NETWORK_LOG_H

#ifdef ARDUINO
#include <ArduinoLog.h>
#define NET_LOG_ERROR(fmt, ...) Log.errorln(fmt, ##__VA_ARGS__)
#define NET_LOG_WARNING(fmt, ...) Log.warningln(fmt, ##__VA_ARGS__)
#define NET_LOG_NOTICE(fmt, ...) Log.noticeln(fmt, ##__VA_ARGS__)
#define NET_LOG_INFO(fmt, ...) Log.infoln(fmt, ##__VA_ARGS__)
#else
#define NET_LOG_ERROR(fmt, ...)
#define NET_LOG_WARNING(fmt, ...)
#define NET_LOG_NOTICE(fmt, ...)
#define NET_LOG_INFO(fmt, ...)
#endif

#endif  // NETWORK_LOG_H
```

- [ ] **Step 7: Run, expect PASS**

Run: `./build.sh test-host`
Expected: all tests pass.

- [ ] **Step 8: Refactor `CurrentConditions.cpp` to use both headers**

Replace the whole `#ifdef ARDUINO ... #endif` macro block (and its `ArduinoLog.h` include) with:

```cpp
#include "NetworkLog.h"
#include "NetworkResult.h"
```

Rename every `CC_LOG_` to `NET_LOG_`: `sed -i 's/CC_LOG_/NET_LOG_/g' src/network/CurrentConditions.cpp`.

Replace the comment and `switch (networkResult) { ... }` block inside `update()` with:

```cpp
        switch (classifyNetworkResult(networkResult)) {
            case NetworkFailure::Connection:
                lastError = CURRENT_CONDITIONS_NETWORK_ERROR;
                break;
            case NetworkFailure::Certificate:
                lastError = CURRENT_CONDITIONS_CERT_ERROR;
                break;
            default:
                lastError = CURRENT_CONDITIONS_ERROR;
        }
```

- [ ] **Step 9: Run, expect PASS**

Run: `./build.sh test-host`
Expected: all tests pass, including `CertErrorReturnsCCCertError` and `UnknownNetworkResultReturnsGeneralError`. Check: `grep -c CC_LOG_ src/network/CurrentConditions.cpp` prints `0`.

- [ ] **Step 10: Commit**

```bash
git add src/network/NetworkResult.h src/network/NetworkLog.h src/network/CurrentConditions.cpp \
        tests/unit/NetworkResultTest.cpp tests/unit/CurrentConditionsTest.cpp tests/unit/CMakeLists.txt
git commit -m "refactor: share network-result mapping and log macros between weather clients

PrecipForecast (next) needs the same NetworkError mapping and host-safe
logging as CurrentConditions. Pull both into headers instead of copying.
OTAManager has its own copy of the log macros; left alone for now.

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 2: Remove the METAR

**Files:**
- Modify: `src/network/CurrentConditions.h` (lines 29, 46), `src/network/CurrentConditions.cpp` (`rawMessage` filter line, the cache restore/store lines, the `prop_rawMessage` parse lines), `Weather_Station_2.ino` (the `// Raw Message` block, lines 165–169)

**Interfaces:**
- Produces: `CurrentConditions` without `raw_message`. Its other public fields are unchanged.

- [ ] **Step 1: Remove `raw_message` everywhere**

In `CurrentConditions.h`, delete `const char* raw_message = "No data";` and `std::string raw_message;` (inside `m_last_valid`).

In `CurrentConditions.cpp`, delete:
- `filter_properties["rawMessage"] = true;`
- `raw_message = m_last_valid.raw_message.c_str();`
- `m_last_valid.raw_message = raw_message;`
- `const char* prop_rawMessage = properties["rawMessage"] | "No data";`
- `raw_message = prop_rawMessage;`

In `Weather_Station_2.ino`, delete the block:

```cpp
    // Raw Message
    display.setFont(Roboto_Medium.at(LAYOUT_MAIN_METAR_FONT_SIZE));
    display.setCursor(LAYOUT_MAIN_METAR_X, LAYOUT_MAIN_METAR_Y);
    sprintf(buffer, "%s", curr.raw_message);
    display.print(buffer);

```

Leave the `"rawMessage"` keys in the test JSON strings in `CurrentConditionsTest.cpp`. They're realistic API input, and the filter now drops them.

- [ ] **Step 2: Verify nothing references it**

Run: `grep -rn "raw_message\|prop_rawMessage\|\"rawMessage\"\] = " src Weather_Station_2.ino`
Expected: no output.

- [ ] **Step 3: Run host tests and firmware build**

Run: `./build.sh test-host && ./build.sh configure && ./build.sh build`
Expected: tests pass. Firmware compiles and the size check passes.

- [ ] **Step 4: Commit**

```bash
git add src/network/CurrentConditions.h src/network/CurrentConditions.cpp Weather_Station_2.ino
git commit -m "feat: drop METAR line from display

rawMessage is empty about 85% of the time. The bottom strip is being
reused for the precipitation forecast.

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 3: PrecipForecast pure helpers (weekday, snow, formatting)

**Files:**
- Create: `src/network/PrecipForecast.h`, `src/network/PrecipForecast.cpp`, `tests/unit/PrecipForecastTest.cpp`
- Modify: `tests/unit/CMakeLists.txt`

**Interfaces:**
- Produces (in `network/PrecipForecast.h`):

```cpp
struct PrecipDay {
    char label[6];  // "Today", "Tue"
    int percent;    // -1 when unknown
    bool snow;
};
class PrecipForecast {
   public:
    static bool weekdayAbbrev(const char* isoDate, char* out, size_t len);   // "2026-11-03..." -> "Tue"
    static bool mentionsSnow(const char* text);                             // case-insensitive "snow"
    static void formatDay(const PrecipDay& day, char* buf, size_t len);     // "Wed 87%", "Fri --%"
};
```

- [ ] **Step 1: Write failing tests**

Create `tests/unit/PrecipForecastTest.cpp`:

```cpp
// ABOUTME: Host unit tests for PrecipForecast parsing and formatting.
// ABOUTME: Uses StubHttpClient with real captured weather.gov forecast responses.
#include <gtest/gtest.h>

#include <cstring>

#include "StubHttpClient.h"
#include "network/PrecipForecast.h"

// ── weekdayAbbrev() ─────────────────────────────────────────────────────────

static std::string weekdayOf(const char* isoDate) {
    char out[6] = "";
    if (!PrecipForecast::weekdayAbbrev(isoDate, out, sizeof(out))) return "<fail>";
    return out;
}

TEST(WeekdayAbbrevTest, KnownDates) {
    ASSERT_EQ(weekdayOf("1970-01-01"), "Thu");
    ASSERT_EQ(weekdayOf("2000-02-29"), "Tue");
    ASSERT_EQ(weekdayOf("2026-11-02T10:00:00-08:00"), "Mon");
    ASSERT_EQ(weekdayOf("2026-11-03T06:00:00-08:00"), "Tue");
    ASSERT_EQ(weekdayOf("2026-11-26T06:00:00-08:00"), "Thu");  // Thanksgiving 2026
    ASSERT_EQ(weekdayOf("2027-01-03"), "Sun");
    ASSERT_EQ(weekdayOf("2027-01-02"), "Sat");
}

TEST(WeekdayAbbrevTest, MalformedDatesFail) {
    ASSERT_EQ(weekdayOf(""), "<fail>");
    ASSERT_EQ(weekdayOf("garbage"), "<fail>");
    ASSERT_EQ(weekdayOf("2026-13-01"), "<fail>");
    ASSERT_EQ(weekdayOf("2026-11-32"), "<fail>");
    ASSERT_FALSE(PrecipForecast::weekdayAbbrev(nullptr, nullptr, 0));
}

// ── mentionsSnow() ──────────────────────────────────────────────────────────

TEST(MentionsSnowTest, DetectsSnowAnyCase) {
    ASSERT_TRUE(PrecipForecast::mentionsSnow("Light Snow Likely"));
    ASSERT_TRUE(PrecipForecast::mentionsSnow("Mostly Sunny then Rain And Snow Likely"));
    ASSERT_TRUE(PrecipForecast::mentionsSnow("SNOW"));
    ASSERT_TRUE(PrecipForecast::mentionsSnow("snow showers"));
}

TEST(MentionsSnowTest, IgnoresNonSnow) {
    ASSERT_FALSE(PrecipForecast::mentionsSnow("Chance Light Rain"));
    ASSERT_FALSE(PrecipForecast::mentionsSnow("Partly Sunny"));
    ASSERT_FALSE(PrecipForecast::mentionsSnow("Sno"));
    ASSERT_FALSE(PrecipForecast::mentionsSnow(""));
    ASSERT_FALSE(PrecipForecast::mentionsSnow(nullptr));
}

// ── formatDay() ─────────────────────────────────────────────────────────────

TEST(FormatDayTest, KnownPercent) {
    PrecipDay day = {"Wed", 87, false};
    char buf[16];
    PrecipForecast::formatDay(day, buf, sizeof(buf));
    ASSERT_STREQ(buf, "Wed 87%");
}

TEST(FormatDayTest, UnknownPercent) {
    PrecipDay day = {"Today", -1, true};
    char buf[16];
    PrecipForecast::formatDay(day, buf, sizeof(buf));
    ASSERT_STREQ(buf, "Today --%");
}
```

Add `PrecipForecastTest.cpp` and `../../src/network/PrecipForecast.cpp` to `add_executable(unit_tests ...)` in `tests/unit/CMakeLists.txt` (test file after `NetworkResultTest.cpp`, source after `../../src/network/CurrentConditions.cpp`).

- [ ] **Step 2: Run, expect a build FAIL**

Run: `./build.sh test-host`
Expected: compile error, `network/PrecipForecast.h: No such file or directory`.

- [ ] **Step 3: Create the header with the helpers only**

`src/network/PrecipForecast.h`:

```cpp
// ABOUTME: Weather.gov gridpoint forecast client for the daily chance of precipitation.
// ABOUTME: Parses the next four daytime periods into labeled percentages with a snow flag.
#ifndef PRECIP_FORECAST_H
#define PRECIP_FORECAST_H

#include <stddef.h>

struct PrecipDay {
    char label[6];  // "Today" or a weekday abbreviation like "Tue"
    int percent;    // chance of precipitation; -1 when the API reports null
    bool snow;      // shortForecast mentions snow
};

class PrecipForecast {
   public:
    // Writes the 3-letter weekday of the YYYY-MM-DD prefix of isoDate. Returns false if the date is malformed.
    static bool weekdayAbbrev(const char* isoDate, char* out, size_t len);
    // True when text contains "snow", ignoring case.
    static bool mentionsSnow(const char* text);
    // Formats one day for the display strip, e.g. "Wed 87%" or "Fri --%".
    static void formatDay(const PrecipDay& day, char* buf, size_t len);
};

#endif  // PRECIP_FORECAST_H
```

- [ ] **Step 4: Implement the helpers**

`src/network/PrecipForecast.cpp`:

```cpp
// ABOUTME: Weather.gov gridpoint forecast client for the daily chance of precipitation.
// ABOUTME: Parses the next four daytime periods into labeled percentages with a snow flag.
#include "PrecipForecast.h"

#include <cctype>
#include <cstdio>

// Days since 1970-01-01 for a proleptic Gregorian date (Howard Hinnant's days_from_civil).
static long daysFromCivil(int y, unsigned m, unsigned d) {
    y -= m <= 2;
    const long era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(y - era * 400);
    const unsigned doy = (153 * (m > 2 ? m - 3 : m + 9) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + static_cast<long>(doe) - 719468;
}

bool PrecipForecast::weekdayAbbrev(const char* isoDate, char* out, size_t len) {
    static const char* const names[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    if (isoDate == nullptr || out == nullptr) return false;

    int year, month, day;
    if (sscanf(isoDate, "%4d-%2d-%2d", &year, &month, &day) != 3) return false;
    if (month < 1 || month > 12 || day < 1 || day > 31) return false;

    long days = daysFromCivil(year, static_cast<unsigned>(month), static_cast<unsigned>(day));
    // 1970-01-01 was a Thursday (index 4).
    int weekday = static_cast<int>(days >= -4 ? (days + 4) % 7 : (days + 5) % 7 + 6);
    snprintf(out, len, "%s", names[weekday]);
    return true;
}

bool PrecipForecast::mentionsSnow(const char* text) {
    static const char needle[] = "snow";
    if (text == nullptr) return false;
    for (const char* p = text; *p; ++p) {
        size_t i = 0;
        while (needle[i] && p[i] && std::tolower(static_cast<unsigned char>(p[i])) == needle[i]) ++i;
        if (needle[i] == '\0') return true;
    }
    return false;
}

void PrecipForecast::formatDay(const PrecipDay& day, char* buf, size_t len) {
    if (day.percent < 0) {
        snprintf(buf, len, "%s --%%", day.label);
    } else {
        snprintf(buf, len, "%s %d%%", day.label, day.percent);
    }
}
```

- [ ] **Step 5: Run, expect PASS**

Run: `./build.sh test-host`
Expected: all tests pass. If `snprintf` truncation warnings show up, fix them; the output must be pristine.

- [ ] **Step 6: Commit**

```bash
git add src/network/PrecipForecast.h src/network/PrecipForecast.cpp tests/unit/PrecipForecastTest.cpp tests/unit/CMakeLists.txt
git commit -m "feat: add PrecipForecast day-label, snow, and format helpers

Labels come from startTime instead of the period name, because NWS
uses names like 'This Afternoon' and 'Thanksgiving Day'.

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 4: PrecipForecast fetch and parse

**Files:**
- Create: `tests/unit/fixtures/forecast_kbfi_2026-11-02.json`, `tests/unit/fixtures/forecast_cascades_2026-11-02.json`
- Modify: `src/network/PrecipForecast.h/.cpp`, `tests/unit/PrecipForecastTest.cpp`, `tests/unit/CMakeLists.txt`

**Interfaces:**
- Consumes: `classifyNetworkResult` / `NetworkFailure` (Task 1), `NET_LOG_*` (Task 1), the helpers from Task 3.
- Produces:

```cpp
const int PRECIP_FORECAST_OK = 0;
const int PRECIP_FORECAST_ERROR = -1;
const int PRECIP_FORECAST_JSON_ERROR = -2;
const int PRECIP_FORECAST_NETWORK_ERROR = -3;
const int PRECIP_FORECAST_DATA_MISSING = -4;
const int PRECIP_FORECAST_CERT_ERROR = -5;

class PrecipForecast {
   public:
    static const int MAX_DAYS = 4;
    PrecipForecast(IHttpClient& http, const std::string& gridpoint);  // gridpoint like "SEW/125,65"
    int update();                         // returns PRECIP_FORECAST_*; resets dayCount first
    const char* getErrorString(int errorCode);
    PrecipDay days[MAX_DAYS];
    int dayCount = 0;
    int lastError = PRECIP_FORECAST_OK;
    // + the static helpers from Task 3
};
```

- [ ] **Step 1: Add the real fixtures**

These were captured live on 2026-11-02 during design:

```bash
mkdir -p tests/unit/fixtures
cp /tmp/claude-1000/-home-jesse-Arduino-Weather-Station-2/faac1a7c-e19c-4ad4-a40a-c7fe355f9415/scratchpad/api/forecast.json \
   tests/unit/fixtures/forecast_kbfi_2026-11-02.json
cp /tmp/claude-1000/-home-jesse-Arduino-Weather-Station-2/faac1a7c-e19c-4ad4-a40a-c7fe355f9415/scratchpad/api/mtn_forecast.json \
   tests/unit/fixtures/forecast_cascades_2026-11-02.json
```

If the scratchpad copies are gone, re-capture them. The data will differ, so update the expected values in Step 2 to match:

```bash
curl -s -A 'WeatherStation2' https://api.weather.gov/gridpoints/SEW/125,65/forecast  -o tests/unit/fixtures/forecast_kbfi_<date>.json
curl -s -A 'WeatherStation2' https://api.weather.gov/gridpoints/SEW/166,121/forecast -o tests/unit/fixtures/forecast_cascades_<date>.json
```

Expected first four daytime periods in the 2026-11-02 files:
- KBFI: `2026-11-02` 1% "Mostly Cloudy"; `11-03` 0% "Partly Sunny"; `11-04` 87% "Partly Sunny then Light Rain"; `11-05` 25% "Chance Light Rain"
- Cascades: `11-02` 0% "Partly Sunny"; `11-03` 0% "Partly Sunny"; `11-04` 57% "Mostly Sunny then Rain And Snow Likely"; `11-05` 55% "Light Snow Likely"

In `tests/unit/CMakeLists.txt`, after `target_include_directories(...)`, add:

```cmake
target_compile_definitions(unit_tests PRIVATE
    FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
)
```

- [ ] **Step 2: Write failing update() tests**

Append to `tests/unit/PrecipForecastTest.cpp`, and add `#include <fstream>`, `#include <initializer_list>`, `#include <sstream>`, and `#include <string>` at the top with the other includes:

```cpp
// ── update() ────────────────────────────────────────────────────────────────

static std::string readFixture(const char* name) {
    std::ifstream in(std::string(FIXTURE_DIR) + "/" + name);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

// Builds a minimal forecast body. Each entry: startTime, isDaytime, percent (-1 = null), shortForecast.
struct PeriodSpec {
    const char* start;
    bool daytime;
    int percent;
    const char* shortForecast;
};

static std::string forecastJson(std::initializer_list<PeriodSpec> periods) {
    std::string json = R"({"properties":{"periods":[)";
    bool first = true;
    for (const PeriodSpec& p : periods) {
        if (!first) json += ",";
        first = false;
        json += std::string(R"({"name":"ignored","startTime":")") + p.start + R"(","isDaytime":)" +
                (p.daytime ? "true" : "false") + R"(,"probabilityOfPrecipitation":{"unitCode":"wmoUnit:percent","value":)" +
                (p.percent < 0 ? std::string("null") : std::to_string(p.percent)) + R"(},"shortForecast":")" +
                p.shortForecast + R"("})";
    }
    json += "]}}";
    return json;
}

TEST(PrecipUpdateTest, BuildsGridpointUrl) {
    StubHttpClient http;
    http.next_body = readFixture("forecast_kbfi_2026-11-02.json");
    PrecipForecast forecast(http, "SEW/125,65");
    forecast.update();
    ASSERT_EQ(http.last_url, "https://api.weather.gov/gridpoints/SEW/125,65/forecast");
}

TEST(PrecipUpdateTest, ParsesRealKbfiMorningForecast) {
    StubHttpClient http;
    http.next_body = readFixture("forecast_kbfi_2026-11-02.json");
    ASSERT_FALSE(http.next_body.empty()) << "fixture missing";
    PrecipForecast forecast(http, "SEW/125,65");

    ASSERT_EQ(forecast.update(), PRECIP_FORECAST_OK);
    ASSERT_EQ(forecast.dayCount, 4);
    const char* labels[] = {"Today", "Tue", "Wed", "Thu"};
    int percents[] = {1, 0, 87, 25};
    for (int i = 0; i < 4; i++) {
        EXPECT_STREQ(forecast.days[i].label, labels[i]) << "day " << i;
        EXPECT_EQ(forecast.days[i].percent, percents[i]) << "day " << i;
        EXPECT_FALSE(forecast.days[i].snow) << "day " << i;
    }
}

TEST(PrecipUpdateTest, FlagsSnowInRealCascadesForecast) {
    StubHttpClient http;
    http.next_body = readFixture("forecast_cascades_2026-11-02.json");
    ASSERT_FALSE(http.next_body.empty()) << "fixture missing";
    PrecipForecast forecast(http, "SEW/166,121");

    ASSERT_EQ(forecast.update(), PRECIP_FORECAST_OK);
    ASSERT_EQ(forecast.dayCount, 4);
    bool snow[] = {false, false, true, true};
    int percents[] = {0, 0, 57, 55};
    for (int i = 0; i < 4; i++) {
        EXPECT_EQ(forecast.days[i].snow, snow[i]) << "day " << i;
        EXPECT_EQ(forecast.days[i].percent, percents[i]) << "day " << i;
    }
}

TEST(PrecipUpdateTest, EveningForecastStartsTomorrowAndSkipsNights) {
    StubHttpClient http;
    http.next_body = forecastJson({
        {"2026-11-02T18:00:00-08:00", false, 10, "Rain"},
        {"2026-11-03T06:00:00-08:00", true, 20, "Rain"},
        {"2026-11-03T18:00:00-08:00", false, 90, "Rain"},
        {"2026-11-04T06:00:00-08:00", true, 30, "Rain"},
        {"2026-11-04T18:00:00-08:00", false, 90, "Rain"},
        {"2026-11-05T06:00:00-08:00", true, 40, "Rain"},
        {"2026-11-05T18:00:00-08:00", false, 90, "Rain"},
        {"2026-11-06T06:00:00-08:00", true, -1, "Cloudy"},
        {"2026-11-06T18:00:00-08:00", false, 90, "Rain"},
        {"2026-11-07T06:00:00-08:00", true, 50, "Rain"},
    });
    PrecipForecast forecast(http, "SEW/125,65");

    ASSERT_EQ(forecast.update(), PRECIP_FORECAST_OK);
    ASSERT_EQ(forecast.dayCount, 4);
    EXPECT_STREQ(forecast.days[0].label, "Tue");
    EXPECT_EQ(forecast.days[0].percent, 20);
    EXPECT_STREQ(forecast.days[1].label, "Wed");
    EXPECT_STREQ(forecast.days[2].label, "Thu");
    EXPECT_STREQ(forecast.days[3].label, "Fri");
    EXPECT_EQ(forecast.days[3].percent, -1);  // null PoP
}

TEST(PrecipUpdateTest, HolidayPeriodGetsWeekdayLabel) {
    StubHttpClient http;
    // The period name would be "Thanksgiving Day". The label must come from the date.
    http.next_body = forecastJson({
        {"2026-11-25T18:00:00-08:00", false, 10, "Cloudy"},
        {"2026-11-26T06:00:00-08:00", true, 40, "Chance Rain"},
    });
    PrecipForecast forecast(http, "SEW/125,65");

    ASSERT_EQ(forecast.update(), PRECIP_FORECAST_OK);
    ASSERT_EQ(forecast.dayCount, 1);
    EXPECT_STREQ(forecast.days[0].label, "Thu");
}

TEST(PrecipUpdateTest, MalformedStartTimeGetsPlaceholderLabel) {
    StubHttpClient http;
    http.next_body = forecastJson({
        {"2026-11-02T10:00:00-08:00", true, 5, "Sunny"},
        {"not-a-date", true, 15, "Sunny"},
    });
    PrecipForecast forecast(http, "SEW/125,65");

    ASSERT_EQ(forecast.update(), PRECIP_FORECAST_OK);
    ASSERT_EQ(forecast.dayCount, 2);
    EXPECT_STREQ(forecast.days[0].label, "Today");
    EXPECT_STREQ(forecast.days[1].label, "---");
}

TEST(PrecipUpdateTest, NoDaytimePeriodsIsDataMissing) {
    StubHttpClient http;
    http.next_body = forecastJson({{"2026-11-02T18:00:00-08:00", false, 10, "Rain"}});
    PrecipForecast forecast(http, "SEW/125,65");
    ASSERT_EQ(forecast.update(), PRECIP_FORECAST_DATA_MISSING);
    ASSERT_EQ(forecast.dayCount, 0);
}

TEST(PrecipUpdateTest, MissingPropertiesIsDataMissing) {
    StubHttpClient http;
    http.next_body = R"({"type":"Feature"})";
    PrecipForecast forecast(http, "SEW/125,65");
    ASSERT_EQ(forecast.update(), PRECIP_FORECAST_DATA_MISSING);
}

TEST(PrecipUpdateTest, MalformedJsonIsJsonError) {
    StubHttpClient http;
    http.next_body = "not json at all {{{";
    PrecipForecast forecast(http, "SEW/125,65");
    ASSERT_EQ(forecast.update(), PRECIP_FORECAST_JSON_ERROR);
}

TEST(PrecipUpdateTest, EmptyBodyIsNetworkError) {
    StubHttpClient http;
    http.next_body = "";
    PrecipForecast forecast(http, "SEW/125,65");
    ASSERT_EQ(forecast.update(), PRECIP_FORECAST_NETWORK_ERROR);
}

TEST(PrecipUpdateTest, NetworkResultsMapToErrorCodes) {
    StubHttpClient http;
    PrecipForecast forecast(http, "SEW/125,65");
    http.next_result = -2;
    EXPECT_EQ(forecast.update(), PRECIP_FORECAST_NETWORK_ERROR);
    http.next_result = -5;
    EXPECT_EQ(forecast.update(), PRECIP_FORECAST_CERT_ERROR);
    http.next_result = -99;
    EXPECT_EQ(forecast.update(), PRECIP_FORECAST_ERROR);
    EXPECT_EQ(forecast.lastError, PRECIP_FORECAST_ERROR);
}

TEST(PrecipUpdateTest, FailureAfterSuccessClearsDays) {
    StubHttpClient http;
    http.next_body = readFixture("forecast_kbfi_2026-11-02.json");
    PrecipForecast forecast(http, "SEW/125,65");
    ASSERT_EQ(forecast.update(), PRECIP_FORECAST_OK);

    http.next_result = -2;
    ASSERT_EQ(forecast.update(), PRECIP_FORECAST_NETWORK_ERROR);
    ASSERT_EQ(forecast.dayCount, 0);
}

TEST(PrecipUpdateTest, AllErrorStringsNonNull) {
    StubHttpClient http;
    PrecipForecast forecast(http, "SEW/125,65");
    for (int code : {PRECIP_FORECAST_OK, PRECIP_FORECAST_ERROR, PRECIP_FORECAST_JSON_ERROR,
                     PRECIP_FORECAST_NETWORK_ERROR, PRECIP_FORECAST_DATA_MISSING, PRECIP_FORECAST_CERT_ERROR, -999}) {
        ASSERT_NE(forecast.getErrorString(code), nullptr) << "code " << code;
    }
}
```

- [ ] **Step 3: Run, expect a build FAIL**

Run: `./build.sh test-host`
Expected: compile errors for the missing constructor, `update`, and `PRECIP_FORECAST_*`.

- [ ] **Step 4: Extend the header**

Replace `src/network/PrecipForecast.h` with:

```cpp
// ABOUTME: Weather.gov gridpoint forecast client for the daily chance of precipitation.
// ABOUTME: Parses the next four daytime periods into labeled percentages with a snow flag.
#ifndef PRECIP_FORECAST_H
#define PRECIP_FORECAST_H

#include <ArduinoJson.h>
#include <stddef.h>

#include <string>

#include "IHttpClient.h"

const int PRECIP_FORECAST_OK = 0;
const int PRECIP_FORECAST_ERROR = -1;
const int PRECIP_FORECAST_JSON_ERROR = -2;
const int PRECIP_FORECAST_NETWORK_ERROR = -3;
const int PRECIP_FORECAST_DATA_MISSING = -4;
const int PRECIP_FORECAST_CERT_ERROR = -5;

struct PrecipDay {
    char label[6];  // "Today" or a weekday abbreviation like "Tue"
    int percent;    // chance of precipitation; -1 when the API reports null
    bool snow;      // shortForecast mentions snow
};

class PrecipForecast {
   public:
    static const int MAX_DAYS = 4;

    // gridpoint is "{wfo}/{x},{y}", e.g. "SEW/125,65" (from GET /points/{lat},{lon}).
    PrecipForecast(IHttpClient& http, const std::string& gridpoint);

    // Fetches the forecast and fills days[] with up to MAX_DAYS daytime periods. Returns a PRECIP_FORECAST_* code.
    int update();
    const char* getErrorString(int errorCode);

    // Writes the 3-letter weekday of the YYYY-MM-DD prefix of isoDate. Returns false if the date is malformed.
    static bool weekdayAbbrev(const char* isoDate, char* out, size_t len);
    // True when text contains "snow", ignoring case.
    static bool mentionsSnow(const char* text);
    // Formats one day for the display strip, e.g. "Wed 87%" or "Fri --%".
    static void formatDay(const PrecipDay& day, char* buf, size_t len);

    PrecipDay days[MAX_DAYS];
    int dayCount = 0;
    int lastError = PRECIP_FORECAST_OK;

   private:
    int parse(const std::string& input);

    IHttpClient& m_http;
    std::string m_url;
    DynamicJsonDocument m_doc;
    StaticJsonDocument<256> m_filter;
};

#endif  // PRECIP_FORECAST_H
```

- [ ] **Step 5: Implement constructor, update, parse, getErrorString**

In `src/network/PrecipForecast.cpp`, add `#include <cstring>`, `#include "NetworkLog.h"` and `#include "NetworkResult.h"` after the existing includes. Then add, below the helpers:

```cpp
static const char* url_prefix = "https://api.weather.gov/gridpoints/";
// The filtered forecast (14 periods x 4 fields) needs about 3 KB on the 64-bit host; the ESP32 needs less.
static const size_t json_capacity = 6144;
static const size_t iso_date_length = 10;  // "YYYY-MM-DD"

PrecipForecast::PrecipForecast(IHttpClient& http, const std::string& gridpoint)
    : m_http(http), m_url(std::string(url_prefix) + gridpoint + "/forecast"), m_doc(json_capacity) {
    // ArduinoJson applies the filter for element 0 to every element of the array.
    JsonObject period = m_filter["properties"]["periods"].createNestedObject();
    period["startTime"] = true;
    period["isDaytime"] = true;
    period["probabilityOfPrecipitation"]["value"] = true;
    period["shortForecast"] = true;

    NET_LOG_NOTICE("Set Precip Forecast URL to %s", m_url.c_str());
}

int PrecipForecast::update() {
    dayCount = 0;
    std::string body;

    NET_LOG_NOTICE("Getting Precip Forecast");
    int networkResult = m_http.get(m_url, body);

    if (networkResult != 0) {
        NET_LOG_ERROR("Network error code: %d", networkResult);
        switch (classifyNetworkResult(networkResult)) {
            case NetworkFailure::Connection:
                lastError = PRECIP_FORECAST_NETWORK_ERROR;
                break;
            case NetworkFailure::Certificate:
                lastError = PRECIP_FORECAST_CERT_ERROR;
                break;
            default:
                lastError = PRECIP_FORECAST_ERROR;
        }
        return lastError;
    }

    if (body.empty()) {
        NET_LOG_ERROR("Empty forecast response from server");
        lastError = PRECIP_FORECAST_NETWORK_ERROR;
        return lastError;
    }

    lastError = parse(body);
    return lastError;
}

int PrecipForecast::parse(const std::string& input) {
    m_doc.clear();
    DeserializationError error =
        deserializeJson(m_doc, input.c_str(), input.size(), DeserializationOption::Filter(m_filter));
    if (error) {
        NET_LOG_ERROR("Forecast deserializeJson() failed: %s", error.c_str());
        return PRECIP_FORECAST_JSON_ERROR;
    }

    JsonArray periods = m_doc["properties"]["periods"];
    if (periods.isNull()) {
        NET_LOG_ERROR("Missing properties.periods in forecast response");
        return PRECIP_FORECAST_DATA_MISSING;
    }

    // "Today" is any daytime period that starts on the same date as the first period.
    const char* firstStart = periods[0]["startTime"] | "";
    bool firstHasDate = strlen(firstStart) >= iso_date_length;

    for (JsonObject period : periods) {
        if (dayCount >= MAX_DAYS) break;
        if (!(period["isDaytime"] | false)) continue;

        PrecipDay& day = days[dayCount];
        const char* start = period["startTime"] | "";
        if (firstHasDate && strncmp(start, firstStart, iso_date_length) == 0) {
            snprintf(day.label, sizeof(day.label), "Today");
        } else if (!weekdayAbbrev(start, day.label, sizeof(day.label))) {
            snprintf(day.label, sizeof(day.label), "---");
        }

        JsonVariant pop = period["probabilityOfPrecipitation"]["value"];
        day.percent = pop.isNull() ? -1 : pop.as<int>();
        day.snow = mentionsSnow(period["shortForecast"] | "");
        NET_LOG_INFO("Precip %s: %d%%%s", day.label, day.percent, day.snow ? " (snow)" : "");
        dayCount++;
    }

    if (dayCount == 0) {
        NET_LOG_ERROR("No daytime periods in forecast response");
        return PRECIP_FORECAST_DATA_MISSING;
    }
    return PRECIP_FORECAST_OK;
}

const char* PrecipForecast::getErrorString(int errorCode) {
    switch (errorCode) {
        case PRECIP_FORECAST_OK:            return "No error";
        case PRECIP_FORECAST_ERROR:         return "General error";
        case PRECIP_FORECAST_JSON_ERROR:    return "JSON parsing error";
        case PRECIP_FORECAST_NETWORK_ERROR: return "Network connection error";
        case PRECIP_FORECAST_DATA_MISSING:  return "Required data missing";
        case PRECIP_FORECAST_CERT_ERROR:    return "SSL certificate validation failed - check CA cert or run cert update";
        default:                            return "Unknown error";
    }
}
```

Then run `clang-format -i src/network/PrecipForecast.h src/network/PrecipForecast.cpp`. `CurrentConditions::getErrorString` uses aligned one-line cases; if clang-format splits them, wrap the switch in `// clang-format off` / `// clang-format on` to match.

- [ ] **Step 6: Run, expect PASS**

Run: `./build.sh test-host`
Expected: all tests pass, with no warnings. If the real-fixture tests fail with `deserializeJson() failed` (which shows up as `PRECIP_FORECAST_JSON_ERROR`, the NoMemory case), raise `json_capacity` to 8192 and update its comment. Don't change the tests.

- [ ] **Step 7: Commit**

```bash
git add src/network/PrecipForecast.h src/network/PrecipForecast.cpp tests/unit/PrecipForecastTest.cpp \
        tests/unit/CMakeLists.txt tests/unit/fixtures/
git commit -m "feat: fetch and parse weather.gov gridpoint precipitation forecast

Takes the first four daytime periods from /gridpoints/{wfo}/{x},{y}/forecast.
Fixtures are real 2026-11-02 responses for KBFI (rain) and a Cascades
gridpoint (snow).

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 5: Snowflake bitmap generator

**Files:**
- Create: `tools/glyph_to_bitmap.py`, `tests/python/test_glyph_to_bitmap.py`, `src/display/Snowflake.h` (generated)

**Interfaces:**
- Produces: `src/display/Snowflake.h` with `namespace Snowflake { const int16_t w; const int16_t h; const uint8_t bitmap[] PROGMEM; }`. That's a 1-bit, row-major, MSB-first bitmap with each row padded to a whole byte and 1 = ink, which is the format `Adafruit_GFX::drawBitmap(x, y, bitmap, w, h, color)` expects.
- Python functions: `render_glyph(font, char) -> Image("1")`, `crop_to_ink(image) -> Image`, `pack_rows(image) -> bytes`, `ascii_preview(image) -> str`, `format_header(name, width, height, data, command) -> str`.

- [ ] **Step 1: Write failing Python tests**

`tests/python/test_glyph_to_bitmap.py`:

```python
# ABOUTME: Tests for tools/glyph_to_bitmap.py bit packing, cropping, and header output.
# ABOUTME: Uses synthetic images plus Pillow's bundled default font, so no system fonts are needed.
import pytest
from PIL import Image, ImageFont

from glyph_to_bitmap import ascii_preview, crop_to_ink, format_header, main, pack_rows, render_glyph


def image_from_rows(rows):
    """Builds a mode-"1" image from strings where '#' is ink."""
    img = Image.new("1", (len(rows[0]), len(rows)), 0)
    for y, row in enumerate(rows):
        for x, ch in enumerate(row):
            if ch == "#":
                img.putpixel((x, y), 1)
    return img


def test_pack_rows_is_msb_first():
    img = image_from_rows(["#.......", ".......#"])
    assert pack_rows(img) == bytes([0x80, 0x01])


def test_pack_rows_pads_each_row_to_a_byte():
    img = image_from_rows(["#########", "........#"])
    # 9 px wide -> 2 bytes per row, trailing bits zero
    assert pack_rows(img) == bytes([0xFF, 0x80, 0x00, 0x80])


def test_crop_to_ink_trims_blank_border():
    img = image_from_rows(["....", ".##.", ".#..", "...."])
    cropped = crop_to_ink(img)
    assert cropped.size == (2, 2)
    assert pack_rows(cropped) == bytes([0xC0, 0x80])


def test_crop_to_ink_rejects_blank_image():
    with pytest.raises(ValueError, match="no ink"):
        crop_to_ink(Image.new("1", (4, 4), 0))


def test_ascii_preview():
    img = image_from_rows(["#.", ".#"])
    assert ascii_preview(img) == "#.\n.#"


def test_format_header_layout():
    header = format_header("Snowflake", 9, 2, bytes([0xFF, 0x80, 0x00, 0x80]), "python3 tools/glyph_to_bitmap.py ...")
    assert header.startswith("// ABOUTME: ")
    assert "#ifndef SNOWFLAKE_H" in header
    assert "// Generated by: python3 tools/glyph_to_bitmap.py ..." in header
    assert "namespace Snowflake {" in header
    assert "const int16_t w = 9;" in header
    assert "const int16_t h = 2;" in header
    assert "const uint8_t bitmap[] PROGMEM = {0xff, 0x80, 0x00, 0x80};" in header
    assert header.strip().endswith("#endif  // SNOWFLAKE_H")


def test_main_rejects_missing_font(tmp_path):
    with pytest.raises(SystemExit):
        main(["--font", str(tmp_path / "nope.ttf"), "--char", "x", "--size", "10", "--name", "X",
              "--out", str(tmp_path / "x.h")])
    assert not (tmp_path / "x.h").exists()


def test_main_rejects_multi_character_char(tmp_path):
    with pytest.raises(SystemExit):
        main(["--font", str(tmp_path / "nope.ttf"), "--char", "ab", "--size", "10", "--name", "X",
              "--out", str(tmp_path / "x.h")])


def test_render_glyph_produces_cropped_ink():
    font = ImageFont.load_default(size=40)
    img = render_glyph(font, "H")
    assert img.mode == "1"
    w, h = img.size
    assert 10 < w < 40 and 15 < h < 40
    assert crop_to_ink(img).size == img.size  # already cropped
```

- [ ] **Step 2: Run, expect FAIL**

Run: `./build.sh test-python`
Expected: `ModuleNotFoundError: No module named 'glyph_to_bitmap'`.

- [ ] **Step 3: Implement `tools/glyph_to_bitmap.py`**

```python
# ABOUTME: Renders a single font glyph into a 1-bit PROGMEM C++ header for Adafruit_GFX drawBitmap().
# ABOUTME: Used to make display icons (e.g. the snowflake) that the ASCII-only Roboto GFX fonts lack.
"""
Usage:
    python3 tools/glyph_to_bitmap.py --font /usr/share/fonts/truetype/dejavu/DejaVuSans.ttf \\
        --char ❄ --size 36 --name Snowflake --out src/display/Snowflake.h

Use this when the display needs a symbol the Roboto fonts in assets/fonts/ don't have.
The glyph is rendered, thresholded to 1 bit, and cropped to its ink. Then it's written as:

    namespace <Name> { const int16_t w, h; const uint8_t bitmap[] PROGMEM; }

That's row-major, MSB-first, with each row padded to a byte and 1 = ink, which is the layout
Adafruit_GFX::drawBitmap() expects. An ASCII preview prints to stdout so you can check the
shape without flashing. The exact command is recorded in the header so it can be regenerated.
"""

import argparse
import shlex
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

INK_THRESHOLD = 128


def render_glyph(font, char):
    """Renders char with a Pillow font into a cropped mode-"1" image (1 = ink)."""
    left, top, right, bottom = font.getbbox(char)
    width, height = max(right - left, 1), max(bottom - top, 1)
    gray = Image.new("L", (width, height), 0)
    ImageDraw.Draw(gray).text((-left, -top), char, font=font, fill=255)
    mono = gray.point(lambda v: 1 if v >= INK_THRESHOLD else 0, mode="1")
    return crop_to_ink(mono)


def crop_to_ink(image):
    """Crops a mode-"1" image to the bounding box of its ink pixels."""
    bbox = image.getbbox()
    if bbox is None:
        raise ValueError("glyph has no ink (blank or missing from font?)")
    return image.crop(bbox)


def pack_rows(image):
    """Packs a mode-"1" image row by row, MSB first, each row padded to a whole byte."""
    width, height = image.size
    out = bytearray()
    for y in range(height):
        byte, bits = 0, 0
        for x in range(width):
            byte = (byte << 1) | (1 if image.getpixel((x, y)) else 0)
            bits += 1
            if bits == 8:
                out.append(byte)
                byte, bits = 0, 0
        if bits:
            out.append(byte << (8 - bits))
    return bytes(out)


def ascii_preview(image):
    """Returns the image as rows of '#' (ink) and '.' (blank)."""
    width, height = image.size
    return "\n".join(
        "".join("#" if image.getpixel((x, y)) else "." for x in range(width)) for y in range(height)
    )


def format_header(name, width, height, data, command):
    """Formats the bitmap as a C++ header with a namespace named after the icon."""
    guard = f"{name.upper()}_H"
    hex_bytes = ", ".join(f"0x{b:02x}" for b in data)
    return (
        f"// ABOUTME: {width}x{height} 1-bit {name} icon for Adafruit_GFX drawBitmap(), generated from a font glyph.\n"
        f"// ABOUTME: Regenerate with tools/glyph_to_bitmap.py instead of editing by hand.\n"
        f"// Generated by: {command}\n"
        f"#ifndef {guard}\n"
        f"#define {guard}\n"
        f"\n"
        f"#include <Arduino.h>\n"
        f"\n"
        f"namespace {name} {{\n"
        f"const int16_t w = {width};\n"
        f"const int16_t h = {height};\n"
        f"const uint8_t bitmap[] PROGMEM = {{{hex_bytes}}};\n"
        f"}}  // namespace {name}\n"
        f"\n"
        f"#endif  // {guard}\n"
    )


def main(argv=None):
    parser = argparse.ArgumentParser(description="Render one font glyph into a 1-bit PROGMEM C++ header")
    parser.add_argument("--font", required=True, help="path to a .ttf/.otf containing the glyph")
    parser.add_argument("--char", required=True, help="the single character to render, e.g. ❄")
    parser.add_argument("--size", type=int, required=True, help="font size in pixels passed to FreeType")
    parser.add_argument("--name", required=True, help="C++ namespace for the icon, e.g. Snowflake")
    parser.add_argument("--out", required=True, help="header file to write")
    args = parser.parse_args(argv)

    if len(args.char) != 1:
        parser.error(f"--char must be exactly one character, got {args.char!r}")
    if not Path(args.font).is_file():
        parser.error(f"font not found: {args.font}")

    font = ImageFont.truetype(args.font, args.size)
    try:
        image = render_glyph(font, args.char)
    except ValueError as e:
        print(f"error: U+{ord(args.char):04X} in {args.font}: {e}", file=sys.stderr)
        return 1

    command = "python3 tools/glyph_to_bitmap.py " + " ".join(
        shlex.quote(a) for a in (argv if argv is not None else sys.argv[1:])
    )
    width, height = image.size
    Path(args.out).write_text(format_header(args.name, width, height, pack_rows(image), command))
    print(ascii_preview(image))
    print(f"wrote {args.out}: {width}x{height}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
```

- [ ] **Step 4: Run, expect PASS**

Run: `./build.sh test-python`
Expected: every test passes, including the existing ones.

- [ ] **Step 5: Generate the snowflake (on the host, where DejaVu is installed)**

```bash
python3 tools/glyph_to_bitmap.py --font /usr/share/fonts/truetype/dejavu/DejaVuSans.ttf \
    --char ❄ --size 36 --name Snowflake --out src/display/Snowflake.h
```

Expected: an ASCII preview that is clearly a six-armed snowflake, and `wrote src/display/Snowflake.h: WxH` with H around 28–34 (close to the Roboto 42 cap height of about 30 px). If H is outside that range, adjust `--size` and rerun. If the preview isn't a snowflake, stop and report.

- [ ] **Step 6: Commit**

```bash
git add tools/glyph_to_bitmap.py tests/python/test_glyph_to_bitmap.py src/display/Snowflake.h
git commit -m "feat: add glyph_to_bitmap tool and generated snowflake icon

The Roboto GFX fonts are web-generated and ASCII-only, so the snow
marker is rendered from U+2744 in DejaVu Sans into a drawBitmap() header.

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 6: Config, layout, and display wiring

**Files:**
- Modify: `CMakeLists.txt` (after the `include(config.cmake)` line 10), `config.cmake.example`, `src/user_settings.h.in`, `layout.json`, `cmake/Layout.cmake`, `src/layout.h.in`, `tools/layout-editor/index.html` (lines ~124, 136, 152, 172), `Weather_Station_2.ino`, `CLAUDE.md`
- Local, not committed: `config.cmake` in the worktree

**Interfaces:**
- Consumes: `PrecipForecast`, `PRECIP_FORECAST_OK`, `PrecipForecast::formatDay` (Tasks 3–4), `Snowflake::w/h/bitmap` (Task 5).
- Produces: `WEATHER_FORECAST_GRIDPOINT` define, `LAYOUT_MAIN_PRECIP_FORECAST_X/Y/FONT_SIZE` defines.

- [ ] **Step 1: Config plumbing**

`config.cmake.example`, add after the `WEATHER_STATION_ID` line:

```cmake
# NWS forecast gridpoint "{wfo}/{x},{y}". Look it up once with
#   curl -s https://api.weather.gov/points/{lat},{lon} | grep -E '"(gridId|gridX|gridY)"'
# (station lat/lon: curl -s https://api.weather.gov/stations/{id} -> geometry.coordinates, lon first).
set(WEATHER_FORECAST_GRIDPOINT "XXX/0,0")
```

`src/user_settings.h.in`, add after `WEATHER_STATION_ID`:

```c
#define WEATHER_FORECAST_GRIDPOINT "@WEATHER_FORECAST_GRIDPOINT@"
```

`CMakeLists.txt`, add right after `include("${CMAKE_SOURCE_DIR}/config.cmake")`:

```cmake
if(NOT DEFINED WEATHER_FORECAST_GRIDPOINT)
    message(FATAL_ERROR "WEATHER_FORECAST_GRIDPOINT missing from config.cmake. See config.cmake.example for how to look it up.")
endif()
```

- [ ] **Step 2: Verify the config check fails first, then fix the local config**

Run: `./build.sh configure`
Expected: FAIL with `WEATHER_FORECAST_GRIDPOINT missing from config.cmake`.

Then: `echo 'set(WEATHER_FORECAST_GRIDPOINT "SEW/125,65")' >> config.cmake` (worktree copy, gitignored) and run `./build.sh configure`.
Expected: configure succeeds. `grep GRIDPOINT build/generated/weather_station_2/user_settings.h` shows `"SEW/125,65"`.

- [ ] **Step 3: Rename the layout field in all four places**

`layout.json`: replace the `metar` line with:
```json
      "precip_forecast": { "x": 20,  "y": 780, "font_size": 42  },
```

`cmake/Layout.cmake`: replace the METAR block with:
```cmake
# Main screen — precipitation forecast strip
string(JSON LAYOUT_MAIN_PRECIP_FORECAST_X         GET ${LAYOUT_JSON} screens main precip_forecast x)
string(JSON LAYOUT_MAIN_PRECIP_FORECAST_Y         GET ${LAYOUT_JSON} screens main precip_forecast y)
string(JSON LAYOUT_MAIN_PRECIP_FORECAST_FONT_SIZE GET ${LAYOUT_JSON} screens main precip_forecast font_size)
```

`src/layout.h.in`: replace the METAR block with:
```c
// Main screen — precipitation forecast strip
#define LAYOUT_MAIN_PRECIP_FORECAST_X         @LAYOUT_MAIN_PRECIP_FORECAST_X@
#define LAYOUT_MAIN_PRECIP_FORECAST_Y         @LAYOUT_MAIN_PRECIP_FORECAST_Y@
#define LAYOUT_MAIN_PRECIP_FORECAST_FONT_SIZE @LAYOUT_MAIN_PRECIP_FORECAST_FONT_SIZE@
```

`tools/layout-editor/index.html`, in the four maps:
- `SAMPLE_TEXT`: `metar: 'KBFI 241553Z ...'` → `precip_forecast: 'Precip:   Today 1%   Tue 0%   Wed 87%❄   Thu 25%',`
- `IS_BOLD.main`: `metar: true,` → `precip_forecast: true,`
- `FONT_WEIGHT_LABEL.main`: `metar: 'Roboto_Medium',` → `precip_forecast: 'Roboto_Medium',`
- `DEFAULT_LAYOUT.screens.main`: `metar: { x: 20, y: 750, font_size: 20 },` → `precip_forecast: { x: 20, y: 780, font_size: 42 },`

Run: `grep -rn -i metar layout.json cmake src tools/layout-editor Weather_Station_2.ino`
Expected: no output.

- [ ] **Step 4: Wire into `Weather_Station_2.ino`**

Add includes after `#include "src/network/CurrentConditions.h"`:

```cpp
#include "src/network/PrecipForecast.h"
```

and after `#include "src/display/KittyPics.h"`:

```cpp
#include "src/display/Snowflake.h"
```

Above `void setup()` (after `char buffer[256];`), add:

```cpp
// Horizontal gap between forecast days, and between a percent and its snowflake.
static const int16_t PRECIP_DAY_GAP = 30;
static const int16_t SNOWFLAKE_GAP = 4;
// Color 0 is black in INKPLATE_3BIT mode (matches setTextColor(0, 7) below).
static const uint16_t INK_BLACK = 0;

// Draws "Precip:  Today 1%  Tue 0%  Wed 87%❄ ..." along the bottom of the screen.
static void drawPrecipForecast(const PrecipForecast& forecast, int result) {
    display.setFont(Roboto_Medium.at(LAYOUT_MAIN_PRECIP_FORECAST_FONT_SIZE));
    display.setCursor(LAYOUT_MAIN_PRECIP_FORECAST_X, LAYOUT_MAIN_PRECIP_FORECAST_Y);
    if (result != PRECIP_FORECAST_OK) {
        display.print("Precip forecast unavailable");
        return;
    }

    display.print("Precip:");
    char dayText[16];
    for (int i = 0; i < forecast.dayCount; i++) {
        display.setCursor(display.getCursorX() + PRECIP_DAY_GAP, LAYOUT_MAIN_PRECIP_FORECAST_Y);
        PrecipForecast::formatDay(forecast.days[i], dayText, sizeof(dayText));
        display.print(dayText);
        if (forecast.days[i].snow) {
            // GFX font cursor y is the text baseline; sit the icon on it.
            int16_t x = display.getCursorX() + SNOWFLAKE_GAP;
            display.drawBitmap(x, LAYOUT_MAIN_PRECIP_FORECAST_Y - Snowflake::h, Snowflake::bitmap, Snowflake::w,
                               Snowflake::h, INK_BLACK);
            display.setCursor(x + Snowflake::w, LAYOUT_MAIN_PRECIP_FORECAST_Y);
        }
    }
}
```

In `setup()`, right after `int updateResult = curr.update(3);  // Try up to 3 times`:

```cpp
    PrecipForecast forecast(*network, WEATHER_FORECAST_GRIDPOINT);
    int forecastResult = forecast.update();
```

In the main-screen drawing, between the weather-description block (ends with `display.print(buffer);` after the font-reduce block) and `// Draw kitty`:

```cpp
    drawPrecipForecast(forecast, forecastResult);

```

- [ ] **Step 5: Build**

Run: `./build.sh test-host && ./build.sh test-python && ./build.sh configure && ./build.sh build`
Expected: all tests pass, firmware compiles, and the size check passes. If `drawBitmap` with this signature isn't available on the Inkplate class, stop and report the compiler error. Don't swap in a different drawing API without checking with Jesse.

- [ ] **Step 6: Update CLAUDE.md**

In `## weather.gov API`, change `filters to textDescription, rawMessage, windSpeed, dewpoint, temperature` to `filters to textDescription, windSpeed, dewpoint, temperature`, then append:

```markdown
`PrecipForecast` reads `GET https://api.weather.gov/gridpoints/{wfo}/{x},{y}/forecast` (gridpoint from `WEATHER_FORECAST_GRIDPOINT` in `config.cmake`; look it up via `/points/{lat},{lon}`). It uses the first 4 `isDaytime` periods' `probabilityOfPrecipitation.value` (any precipitation, may be null) and flags snow when `shortForecast` contains "snow". Labels come from `startTime`, because period names can be holidays. Snow test gridpoint: `SEW/166,121` (Cascades).
```

In `## Architecture`, replace the stale `- Station is hardcoded as \`KBFI\` in \`CurrentConditions.cpp\`.` with `- Station (\`WEATHER_STATION_ID\`) and forecast gridpoint (\`WEATHER_FORECAST_GRIDPOINT\`) come from \`config.cmake\`.`

In `## Build, Flash & Serial`, add `WEATHER_FORECAST_GRIDPOINT` to the list of `config.cmake` contents.

In `## Adding Cat Images`'s neighborhood, add a short section:

```markdown
## Display Icons

Symbols missing from the Roboto fonts are 1-bit bitmaps made by `tools/glyph_to_bitmap.py` (see `--help`). The regeneration command is recorded at the top of each generated header, e.g. `src/display/Snowflake.h`.
```

- [ ] **Step 7: Commit**

```bash
git add CMakeLists.txt config.cmake.example src/user_settings.h.in layout.json cmake/Layout.cmake src/layout.h.in \
        tools/layout-editor/index.html Weather_Station_2.ino CLAUDE.md
git status --short   # config.cmake must NOT appear (gitignored)
git commit -m "feat: show 4-day precipitation forecast strip in place of METAR

Adds the required WEATHER_FORECAST_GRIDPOINT setting (configure fails
with a pointer to the example if it's missing) and renames the metar
layout field to precip_forecast at 42pt.

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 7: Hardware verification and layout tuning

Run this with Jesse. It needs the device on `/dev/ttyUSB0`.

**Files:**
- Possibly modify: `layout.json`, `tools/layout-editor/index.html` (`DEFAULT_LAYOUT`), `Weather_Station_2.ino` (`PRECIP_DAY_GAP`)

- [ ] **Step 1: Flash and capture (KBFI)**

Run: `./build.sh flash && python3 tools/serial_capture.py --seconds 40`
Expected: two `[HTTPS] GET... code: 200` lines (observations + forecast), `Precip Today: N%`-style info lines, and no new errors.

- [ ] **Step 2: Jesse checks the panel**

Ask Jesse to confirm that the strip reads `Precip:  <Today|Day> N% ...`, that it fits within 1200 px, and that it doesn't collide with the description text. If not, adjust `x`/`y` in `layout.json` (keep `font_size` in the valid list) or `PRECIP_DAY_GAP`, then `./build.sh configure && ./build.sh flash`, and repeat.

- [ ] **Step 3: Snow check**

Temporarily set `WEATHER_FORECAST_GRIDPOINT` to `"SEW/166,121"` in the worktree `config.cmake`, then `./build.sh configure && ./build.sh flash && python3 tools/serial_capture.py --seconds 40`. Look for `(snow)` in the log on the days the live forecast says snow. Jesse confirms the flakes render, sit on the baseline, and the line still fits. If the live mountain forecast has no snow that day, say so. Don't fake a pass.

- [ ] **Step 4: Restore KBFI**

Set `WEATHER_FORECAST_GRIDPOINT` back to `"SEW/125,65"`, then `./build.sh configure && ./build.sh flash`.

- [ ] **Step 5: Commit any tuning**

If Steps 2–3 changed any committed files:

```bash
git add layout.json tools/layout-editor/index.html Weather_Station_2.ino
git commit -m "fix: tune precip forecast strip position after on-device check

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

- [ ] **Step 6: Remind Jesse**

Jesse's main-checkout `config.cmake` needs `set(WEATHER_FORECAST_GRIDPOINT "SEW/125,65")` before building this branch from there. Configure fails loudly without it.
