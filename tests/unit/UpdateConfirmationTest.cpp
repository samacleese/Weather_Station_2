// ABOUTME: Host unit tests for UpdateConfirmation's WAKE-button hold detection and the OTA battery gate.
// ABOUTME: Samples arrive at the sketch's 100 ms poll interval with explicit timestamps, so no real clock is needed.
#include <gtest/gtest.h>

#include "ota/UpdateConfirmation.h"

static const unsigned long POLL_MS = 100;

// Feeds the same button state every POLL_MS from startMs through endMs inclusive.
// Stops early and returns the first non-WAITING result; otherwise returns WAITING.
static UpdateConfirmation::Result feed(UpdateConfirmation& confirmation,
                                       bool pressed,
                                       unsigned long startMs,
                                       unsigned long endMs) {
    for (unsigned long t = startMs; t <= endMs; t += POLL_MS) {
        UpdateConfirmation::Result result = confirmation.sample(pressed, t);
        if (result != UpdateConfirmation::WAITING) return result;
    }
    return UpdateConfirmation::WAITING;
}

TEST(UpdateConfirmationTest, NoPressTimesOutAtThirtySeconds) {
    UpdateConfirmation confirmation;
    EXPECT_EQ(UpdateConfirmation::WAITING, feed(confirmation, false, 0, 29900));
    EXPECT_EQ(UpdateConfirmation::TIMED_OUT, confirmation.sample(false, 30000));
}

TEST(UpdateConfirmationTest, TwoSecondHoldAfterReleaseConfirms) {
    UpdateConfirmation confirmation;
    EXPECT_EQ(UpdateConfirmation::WAITING, confirmation.sample(false, 0));
    // Hold starts at 100 ms, so 2000 ms of hold is reached at 2100 ms.
    EXPECT_EQ(UpdateConfirmation::WAITING, feed(confirmation, true, 100, 2000));
    EXPECT_EQ(UpdateConfirmation::CONFIRMED, confirmation.sample(true, 2100));
}

TEST(UpdateConfirmationTest, ReleaseRestartsTheHold) {
    UpdateConfirmation confirmation;
    confirmation.sample(false, 0);
    EXPECT_EQ(UpdateConfirmation::WAITING, feed(confirmation, true, 100, 1900));
    EXPECT_EQ(UpdateConfirmation::WAITING, confirmation.sample(false, 2000));
    // A new hold from 2100 ms needs until 4100 ms; the earlier 1.8 s does not carry over.
    EXPECT_EQ(UpdateConfirmation::WAITING, feed(confirmation, true, 2100, 4000));
    EXPECT_EQ(UpdateConfirmation::CONFIRMED, confirmation.sample(true, 4100));
}

TEST(UpdateConfirmationTest, RepeatedShortPressesTimeOut) {
    UpdateConfirmation confirmation;
    // 500 ms pressed, 500 ms released, for the whole window.
    for (unsigned long t = 0; t < 30000; t += POLL_MS) {
        bool pressed = (t / 500) % 2 == 1;
        ASSERT_EQ(UpdateConfirmation::WAITING, confirmation.sample(pressed, t)) << "at " << t << " ms";
    }
    EXPECT_EQ(UpdateConfirmation::TIMED_OUT, confirmation.sample(false, 30000));
}

TEST(UpdateConfirmationTest, HoldInProgressWhenPromptAppearsDoesNotCount) {
    UpdateConfirmation confirmation;
    // Still holding WAKE from waking the device: five seconds of LOW never confirms.
    EXPECT_EQ(UpdateConfirmation::WAITING, feed(confirmation, true, 0, 5000));
    EXPECT_EQ(UpdateConfirmation::WAITING, confirmation.sample(false, 5100));
    EXPECT_EQ(UpdateConfirmation::WAITING, feed(confirmation, true, 5200, 7100));
    EXPECT_EQ(UpdateConfirmation::CONFIRMED, confirmation.sample(true, 7200));
}

TEST(UpdateConfirmationTest, StuckButtonTimesOut) {
    UpdateConfirmation confirmation;
    EXPECT_EQ(UpdateConfirmation::WAITING, feed(confirmation, true, 0, 29900));
    EXPECT_EQ(UpdateConfirmation::TIMED_OUT, confirmation.sample(true, 30000));
}

TEST(UpdateConfirmationTest, HoldUnfinishedAtWindowEndTimesOut) {
    UpdateConfirmation confirmation;
    EXPECT_EQ(UpdateConfirmation::WAITING, feed(confirmation, false, 0, 28500));
    EXPECT_EQ(UpdateConfirmation::WAITING, feed(confirmation, true, 28600, 29900));
    EXPECT_EQ(UpdateConfirmation::TIMED_OUT, confirmation.sample(true, 30000));
}

TEST(UpdateConfirmationTest, WindowStartsAtFirstSample) {
    UpdateConfirmation confirmation;
    EXPECT_EQ(UpdateConfirmation::WAITING, feed(confirmation, false, 50000, 79900));
    EXPECT_EQ(UpdateConfirmation::TIMED_OUT, confirmation.sample(false, 80000));
}
