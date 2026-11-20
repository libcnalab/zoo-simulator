// SPDX-License-Identifier: MIT
#include "SightLine.hpp"

#include <cmath>
#include <numbers>
#include <string_view>

#include <gtest/gtest.h>

#include "TestData.hpp"

namespace zoogame {
namespace {

TEST(SightLine, TreeCrownsRatherThanOnlyTheirTrunksBlockAnAutomaticView) {
    const zoo::Catalog& catalog = zoo::testdata::GameCatalog();
    for (const std::string_view id : {"oak", "birch", "pine", "spruce", "acacia", "willow"}) {
        const int index = catalog.PropIndex(id);
        ASSERT_GE(index, 0) << id;
        const zoo::PropDef& tree = catalog.props[static_cast<std::size_t>(index)];
        ASSERT_EQ(tree.role, zoo::PropRole::Tree) << id;
        ASSERT_GT(tree.shadeM2, 0.0f) << id;
        const float crown = std::sqrt(tree.shadeM2 / std::numbers::pi_v<float>);
        EXPECT_FLOAT_EQ(SightBlockingRadius(tree, 1.0f), std::max(tree.radiusM, crown)) << id;
        EXPECT_GT(SightBlockingRadius(tree, 1.0f), tree.radiusM) << id;
        EXPECT_FLOAT_EQ(SightBlockingRadius(tree, 0.7f), 0.7f * std::max(tree.radiusM, crown)) << id;
    }
}

TEST(SightLine, NonTreesKeepTheirSolidGroundRadius) {
    zoo::PropDef bush;
    bush.role = zoo::PropRole::Bush;
    bush.radiusM = 0.6f;
    bush.shadeM2 = 40.0f;
    EXPECT_FLOAT_EQ(SightBlockingRadius(bush, 1.25f), 0.75f);
    EXPECT_FLOAT_EQ(SightBlockingRadius(bush, -1.0f), 0.0f);
}

}  // namespace
}  // namespace zoogame
