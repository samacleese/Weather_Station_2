// ABOUTME: Implements the WAKE-button hold detection used to confirm an OTA install.
// ABOUTME: See UpdateConfirmation.h for the confirmation rules.
#include "UpdateConfirmation.h"

#include <cmath>

UpdateConfirmation::Result UpdateConfirmation::sample(bool pressed, unsigned long nowMs) {
    if (!m_started) {
        m_started = true;
        m_windowStartMs = nowMs;
    }

    if (!pressed) {
        m_seenRelease = true;
        m_holding = false;
    } else if (m_seenRelease) {
        if (!m_holding) {
            m_holding = true;
            m_holdStartMs = nowMs;
        } else if (nowMs - m_holdStartMs >= HOLD_MS) {
            return CONFIRMED;
        }
    }

    if (nowMs - m_windowStartMs >= WINDOW_MS) return TIMED_OUT;
    return WAITING;
}

bool batteryAllowsInstall(float volts, int minBatteryMv) { return std::lround(volts * 1000.0f) >= minBatteryMv; }
