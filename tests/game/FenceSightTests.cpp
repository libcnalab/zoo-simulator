// SPDX-License-Identifier: MIT
#include "FenceModels.hpp"
#include "ModelSight.hpp"

#include <gtest/gtest.h>

namespace zoogame {
namespace {

TEST(FenceSight, SlopingPaddockRailsHideOnlyWhatTheyCover) {
    FenceLook rail;
    rail.style = zoo::FenceStyle::Rail;
    rail.height = 2.0f;
    ModelBuilder model;
    BuildFencePanel(model, rail, {0.0f, 1.0f, 0.0f}, {0.0f, 2.0f, 2.0f}, false, 19u);
    BuildFencePost(model, rail, {0.0f, 2.0f, 2.0f}, false);
    CnaEngine::Collision::StaticWorld solids(1.0f);
    AddSightSurfaces(solids, model);
    const auto at = [&](float y, float z, float reach) {
        return solids.Raycast({{-2.0f, y, z}, {1.0f, 0.0f, 0.0f}}, reach);
    };
    // Midway up the slope the ground is at 1.5 m, the top rail at another 0.92 * 2 m.
    const auto board = at(3.34f, 1.0f, 3.0f);
    ASSERT_TRUE(board);
    EXPECT_NEAR(board->distance, 1.975f, 0.01f);
    EXPECT_FALSE(at(3.34f, 1.0f, 1.5f));
    EXPECT_FALSE(at(3.1f, 1.0f, 3.0f));
    EXPECT_TRUE(at(3.1f, 2.0f, 3.0f));
}

TEST(FenceSight, ViewingGlassAndWireLetVisitorsLookThroughTheFrame) {
    for (auto style : {zoo::FenceStyle::Glass, zoo::FenceStyle::Mesh}) {
        FenceLook look;
        look.style = style;
        look.height = 2.2f;
        ModelBuilder model;
        BuildFencePanel(model, look, {0.0f, 0.0f, 0.0f}, {2.0f, 0.0f, 0.0f}, false, 20u);
        BuildFencePost(model, look, {0.0f, 0.0f, 0.0f}, false);
        CnaEngine::Collision::StaticWorld solids;
        AddSightSurfaces(solids, model);
        const auto at = [&](float x, float y) {
            return solids.Raycast({{x, y, 2.0f}, {0.0f, 0.0f, -1.0f}}, 3.0f);
        };
        EXPECT_FALSE(at(1.0f, 1.3f));
        EXPECT_TRUE(at(0.0f, 1.3f));
        EXPECT_TRUE(at(1.0f, 2.205f));
    }
}

}  // namespace
}  // namespace zoogame
