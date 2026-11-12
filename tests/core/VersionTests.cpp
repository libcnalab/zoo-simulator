// SPDX-License-Identifier: MIT
#include "zoo/base/Version.hpp"

#include <gtest/gtest.h>

#include <string>

TEST(Version, IsNotEmpty) { EXPECT_FALSE(std::string(zoo::VersionString()).empty()); }
