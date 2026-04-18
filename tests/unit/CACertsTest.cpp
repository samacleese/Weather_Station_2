// ABOUTME: Host unit tests for CACerts certificate lookup.
// ABOUTME: Verifies known-hostname lookups and unknown-hostname null returns.
#include <gtest/gtest.h>
#include "security/CACerts.h"

TEST(CACertsTest, KnownHostReturnsCert) {
    const char* cert = CACerts::getCert("api.weather.gov");
    ASSERT_NE(cert, nullptr);
}

TEST(CACertsTest, UnknownHostReturnsNullptr) {
    const char* cert = CACerts::getCert("notarealhost.example.com");
    ASSERT_EQ(cert, nullptr);
}
