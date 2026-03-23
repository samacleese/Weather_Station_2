# Fix PR #5: NETWORK_CERT_ERROR Implementation Plan

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fix the broken SSL error detection in PR #5, refactor error codes to a proper enum, and correctly detect cert failures using `WiFiClientSecure::lastError()`.

**Architecture:** Change `Network::get()` to accept `WiFiClientSecure&` (all callers already pass one), enabling direct SSL error inspection. Replace `const int` error constants with a plain `enum NetworkError`. Add a private `isCertError()` method that checks `lastError()` after a failed connection.

**Tech Stack:** C++, Arduino ESP32 (Inkplate 10), ESP32 WiFiClientSecure / mbedTLS, HTTPClient

---

## Background: Why the PR is broken

The PR adds `HTTPC_ERROR_SSL` — this constant **does not exist** in this HTTPClient library. The check silently never fires. The PR also redefines the existing `const int` error codes in a new `enum` inside `Network.cpp` rather than updating `Network.h`, causing a redefinition compile error. `handleSSLError` is not declared in `Network.h` so it won't link.

### How SSL errors actually surface in this library

`WiFiClientSecure::connect()` stores the raw mbedTLS return code in `_lastError` and exposes it via `lastError(char* buf, size_t size)`. However, the cert verify failure path in `ssl_client.cpp` has a bug: it calls `handle_error(0)` (where `ret=0` from a successful handshake), so `_lastError` may be 0 even when cert verification failed. The socket is killed (`stop_ssl_socket()`), so subsequent `https.GET()` will fail — but the failure code will be a generic connection error, not an SSL-specific one.

**The investigation step below is mandatory before implementation.** We need to observe empirically what `httpCode` and `lastError()` actually return when a cert fails, since the code path is non-obvious.

---

## Files

| File | Change |
|---|---|
| `src/network/Network.h` | Replace `const int` with `enum NetworkError`; add `NETWORK_CERT_ERROR`; change `get()` parameter to `WiFiClientSecure&`; declare private `isCertError()` |
| `src/network/Network.cpp` | Update `get()` signature; implement `isCertError()`; add `NETWORK_CERT_ERROR` case to `getErrorString()`; remove "Improved error handling" comment |
| `src/network/CurrentConditions.cpp` | Update `network->get()` call site (already passes `WiFiClientSecure`, just update include if needed) |
| `4-add-explicit-ssl-certificate-error-detection...` branch | Rebase onto master, apply changes, force-push |

---

## Chunk 1: Investigation

### Task 1: Determine what error values surface on cert failure

This is mandatory before writing the detection code. We need to know:
- What does `httpCode` contain after a failed SSL connection?
- What does `client.lastError()` contain?

**Files:** No code changes — observation only.

- [ ] **Step 1: Temporarily break the cert**

In `src/security/CACerts.cpp`, corrupt the `r12_` cert by changing one character in the base64 body (not the header/footer lines). Build and flash.

- [ ] **Step 2: Observe serial output**

Connect serial monitor (115200 baud). Look for:
1. The log line: `[HTTPS] GET... failed, error: <string>` — note the error string
2. Any `log_e` output from the SSL layer mentioning mbedTLS codes
3. The error code integer that flows through to the display

Record the exact `httpCode` value returned by `https.GET()` and whether `client.lastError()` returns non-zero.

Expected candidates based on library source:
- `HTTPC_ERROR_NOT_CONNECTED = -4` (connection appears to succeed then fails on send)
- `HTTPC_ERROR_SEND_HEADER_FAILED = -2` (send to dead socket fails)
- `HTTPC_ERROR_CONNECTION_REFUSED = -1`

- [ ] **Step 3: Add temporary logging to Network.cpp**

Add temporarily (do not commit):
```cpp
// In the httpCode < 0 branch of Network::get():
WiFiClientSecure* secureClient = static_cast<WiFiClientSecure*>(&client);
char sslErrBuf[100];
int sslErr = secureClient->lastError(sslErrBuf, sizeof(sslErrBuf));
Log.error(F("[HTTPS] httpCode=%d, lastError=%d (%s)" CR), httpCode, sslErr, sslErrBuf);
```

This requires temporarily changing the `WiFiClient&` parameter to `WiFiClientSecure&` in the local test build. Don't commit — just observe.

- [ ] **Step 4: Document findings**

Record the observed `httpCode` and `lastError()` values in a comment at the top of the implementation in Task 3. Restore the cert to the correct value before proceeding.

---

## Chunk 2: Enum refactor and signature change

### Task 2: Replace `const int` with `enum NetworkError` in `Network.h`

**Files:**
- Modify: `src/network/Network.h`

- [ ] **Step 1: Replace the constant block with an enum**

In `src/network/Network.h`, replace lines 12–17:

```cpp
// Network error codes
const int NETWORK_OK = 0;
const int NETWORK_WIFI_ERROR = -1;
const int NETWORK_HTTP_ERROR = -2;
const int NETWORK_TIMEOUT_ERROR = -3;
const int NETWORK_NO_DATA = -4;
```

With:

```cpp
enum NetworkError {
    NETWORK_OK = 0,
    NETWORK_WIFI_ERROR = -1,
    NETWORK_HTTP_ERROR = -2,
    NETWORK_TIMEOUT_ERROR = -3,
    NETWORK_NO_DATA = -4,
    NETWORK_CERT_ERROR = -5,
};
```

- [ ] **Step 2: Update `get()` signature in `Network.h`**

Add `#include <WiFiClientSecure.h>` to the includes block (after `<WiFiClient.h>`).

Change:
```cpp
int get(WiFiClient& client, const String& url, StreamString& stream, int retries = 2, int timeout = 10000);
```
To:
```cpp
int get(WiFiClientSecure& client, const String& url, StreamString& stream, int retries = 2, int timeout = 10000);
```

- [ ] **Step 3: Declare `isCertError()` as a private method in `Network.h`**

In the `private:` section, add:
```cpp
bool isCertError(int httpCode, WiFiClientSecure& client);
```

- [ ] **Step 4: Fix the file header comment**

Change line 1 from:
```cpp
// Network.h - Improved error handling
```
To the required ABOUTME format:
```cpp
// ABOUTME: WiFi connection management and HTTPS request handling for ESP32.
// ABOUTME: Provides error codes and retry logic for network operations.
```

- [ ] **Step 5: Compile check**

```bash
arduino-cli compile --fqbn Croduino_Boards:Inkplate:Inkplate10 .
```

Expected: errors about mismatched `get()` call sites — that's fine, we fix them next.

- [ ] **Step 6: Commit**

```bash
git add src/network/Network.h
git commit -m "refactor: replace const int error codes with NetworkError enum, add NETWORK_CERT_ERROR"
```

---

### Task 3: Update `Network.cpp`

**Files:**
- Modify: `src/network/Network.cpp:1,59,107,158,173`

- [ ] **Step 1: Update the file header comment**

Change line 1:
```cpp
// Network.cpp - Improved error handling
```
To:
```cpp
// ABOUTME: WiFi connection management and HTTPS request handling for ESP32.
// ABOUTME: Provides retry logic, backoff, and SSL certificate error detection.
```

- [ ] **Step 2: Update `get()` signature**

Change line 59:
```cpp
int Network::get(WiFiClient& client, const String& url, StreamString& stream, int retries, int timeout) {
```
To:
```cpp
int Network::get(WiFiClientSecure& client, const String& url, StreamString& stream, int retries, int timeout) {
```

- [ ] **Step 3: Add `isCertError()` implementation**

Using the findings from Task 1, implement cert detection. Add this before `getErrorString()`:

```cpp
bool Network::isCertError(int httpCode, WiFiClientSecure& client) {
    // [Document the observed httpCode and lastError values from Task 1 here]
    // SSL failures surface as <INSERT OBSERVED httpCode> after the socket is
    // killed by cert verification failure in ssl_client.cpp.
    char errBuf[100];
    int sslErr = client.lastError(errBuf, sizeof(errBuf));
    if (sslErr != 0) {
        Log.error(F("[HTTPS] SSL error %d: %s" CR), sslErr, errBuf);
        return true;
    }
    // Fallback: if httpCode matches the observed failure code and no other
    // explanation fits, treat as cert error.
    // [INSERT observed httpCode check here based on Task 1 findings]
    return false;
}
```

> **Note:** Fill in the observed values from Task 1. If `lastError()` reliably returns non-zero, the fallback httpCode check may not be needed.

- [ ] **Step 4: Wire `isCertError()` into the retry loop**

In the `httpCode < 0` branch (around line 107), add SSL check before the generic error log:

```cpp
} else {
    if (isCertError(httpCode, static_cast<WiFiClientSecure&>(client))) {
        return NETWORK_CERT_ERROR;
    }
    Log.error(F("[HTTPS] GET... failed, error: %s" CR), https.errorToString(httpCode).c_str());
    https.end();
    attempt++;
    continue;
}
```

- [ ] **Step 5: Add `NETWORK_CERT_ERROR` case to `getErrorString()`**

In the switch statement, add before `default`:
```cpp
case NETWORK_CERT_ERROR:
    return "SSL certificate validation failed - check CA cert or run cert update";
```

- [ ] **Step 6: Compile check**

```bash
arduino-cli compile --fqbn Croduino_Boards:Inkplate:Inkplate10 .
```

Expected: error on `CurrentConditions.cpp` call site — fix next.

- [ ] **Step 7: Commit**

```bash
git add src/network/Network.cpp
git commit -m "feat: implement SSL cert error detection via WiFiClientSecure::lastError()"
```

---

### Task 4: Update `CurrentConditions.cpp` call site

**Files:**
- Modify: `src/network/CurrentConditions.cpp:34,45`

- [ ] **Step 1: Verify include is present**

`WiFiClientSecure.h` is already included at line 5 — no change needed.

- [ ] **Step 2: Update the `get()` call**

The call at line 45 already passes `client` which is a `WiFiClientSecure` — the call site itself doesn't change. The signature change in `Network.h` is all that's needed.

- [ ] **Step 3: Full compile check**

```bash
arduino-cli compile --fqbn Croduino_Boards:Inkplate:Inkplate10 .
```

Expected: clean compile.

- [ ] **Step 4: Commit**

```bash
git add src/network/CurrentConditions.cpp
git commit -m "fix: update Network::get() call site to use WiFiClientSecure reference"
```

---

## Chunk 3: Verification and PR

### Task 5: End-to-end verification

**Files:** No changes — device test.

- [ ] **Step 1: Flash and verify normal operation**

Flash the device with the correct `r12_` cert. Confirm weather data loads and no spurious cert errors appear.

- [ ] **Step 2: Test cert error detection**

Corrupt the `r12_` cert again (one character in the base64 body). Build, flash, observe serial output.

Expected:
- `[HTTPS] SSL error <N>: <description>` in serial log
- Device display shows: "SSL certificate validation failed - check CA cert or run cert update"
- Device enters the error sleep cycle (300s)

- [ ] **Step 3: Restore correct cert**

Revert the corruption. Build, flash, verify normal operation resumes.

- [ ] **Step 4: Commit verification note**

```bash
git commit --allow-empty -m "test: verified SSL cert error detection on device with corrupted cert"
```

---

### Task 6: Rebase branch and update PR

**Files:** Git operations on `4-add-explicit-ssl-certificate-error-detection...` branch.

- [ ] **Step 1: Check out the PR branch**

```bash
git fetch origin
git checkout 4-add-explicit-ssl-certificate-error-detection-and-user-message-for-ca-changes
```

- [ ] **Step 2: Rebase onto master**

```bash
git rebase master
```

Expect conflicts in `Network.cpp` (the PR adds the broken enum and `handleSSLError` — discard those hunks in favor of the new implementation).

- [ ] **Step 3: Resolve conflicts**

Keep everything from `master` (our new implementation). The PR's additions to discard:
- The anonymous `enum { NETWORK_OK ... }` block in `Network.cpp` (now in `Network.h` as `NetworkError`)
- The broken `handleSSLError` body (replaced by `isCertError`)

- [ ] **Step 4: Force-push the branch**

```bash
git push --force-with-lease origin 4-add-explicit-ssl-certificate-error-detection-and-user-message-for-ca-changes
```

- [ ] **Step 5: Update the PR description**

```bash
gh pr edit 5 --body "$(cat <<'EOF'
Introduce `NETWORK_CERT_ERROR` for SSL certificate validation failures.

**Changes from original PR:**
- Error codes moved to `enum NetworkError` in `Network.h` (replacing `const int` constants)
- `Network::get()` now takes `WiFiClientSecure&` instead of `WiFiClient&`, enabling SSL error inspection
- SSL detection uses `WiFiClientSecure::lastError()` (mbedTLS error code) rather than the non-existent `HTTPC_ERROR_SSL` constant
- `isCertError()` declared in `Network.h` and implemented in `Network.cpp`
- Verified on device with deliberately corrupted cert
EOF
)"
```
