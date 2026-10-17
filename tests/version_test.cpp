#include "diskpilot/version.hpp"
#include <gtest/gtest.h>

TEST(Version, ReportsInitialScaffoldVersion) {
    EXPECT_EQ(diskpilot::version(), "0.1.0");
}
