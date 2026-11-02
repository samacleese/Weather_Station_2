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
