// SPDX-License-Identifier: MIT
#include "BuildingModels.hpp"

#include <gtest/gtest.h>

namespace zoogame {
namespace {

TEST(BuildingModels, PavilionBlocksItsWallsButAllowsSightBetweenItsPosts) {
    using Microsoft::Xna::Framework::Ray;
    zoo::BuildingDef def;
    def.id = "education_centre";
    def.look = "pavilion";
    def.width = 4;
    def.depth = 3;
    def.heightM = 4.0f;
    def.doorOffset = 1;
    const BuildingSight sight(def);

    // The same line crosses both side walls down low and both open sides at eye height.
    const Ray low({-7.0f, 0.8f, 0.5f}, {1.0f, 0.0f, 0.0f});
    const auto wall = sight.Distance(low, 15.0f);
    ASSERT_TRUE(wall);
    EXPECT_NEAR(*wall, 3.3f, 0.02f);
    EXPECT_FALSE(sight.Distance(low, 3.0f));
    EXPECT_FALSE(sight.Distance(Ray({-7.0f, 2.0f, 0.5f}, {1.0f, 0.0f, 0.0f}), 15.0f));
    EXPECT_TRUE(sight.Distance(Ray({-7.0f, 2.0f, 1.0f}, {1.0f, 0.0f, 0.0f}), 15.0f));
    const Ray rear({0.0f, 2.0f, -7.0f}, {0.0f, 0.0f, 1.0f});
    const auto back = sight.Distance(rear, 15.0f);
    ASSERT_TRUE(back);
    EXPECT_NEAR(*back, 4.3f, 0.02f);
}

}  // namespace
}  // namespace zoogame
