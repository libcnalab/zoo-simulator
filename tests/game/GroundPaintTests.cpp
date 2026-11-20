// SPDX-License-Identifier: MIT
#include <gtest/gtest.h>

#include <cmath>

#include "GroundPaint.hpp"

namespace zoogame {
namespace {

TEST(GroundPaint, FenceWearContinuesRoundASegmentEndAcrossTheNextTile) {
    zoo::World world(8, 8);
    world.SetFence({2, 2, true}, {.kind = 1});

    const TilePaintInfo beforeEnd = PaintInfo(world, {2, 2});
    const TilePaintInfo afterEnd = PaintInfo(world, {3, 2});
    const GroundSample before = SampleGround(beforeEnd, 6.0f, 4.4f);
    const GroundSample after = SampleGround(afterEnd, 6.0f, 4.4f);

    zoo::World unwornWorld(8, 8);
    const GroundSample unworn = SampleGround(PaintInfo(unwornWorld, {3, 2}), 6.0f, 4.4f);
    float wornDifference = 0.0f;
    EXPECT_EQ(before.detail, after.detail);
    for (std::size_t channel = 0; channel < before.color.size(); ++channel)
        EXPECT_FLOAT_EQ(before.color[channel], after.color[channel]);
    for (std::size_t channel = 0; channel < after.color.size(); ++channel)
        wornDifference += std::abs(after.color[channel] - unworn.color[channel]);
    EXPECT_GT(wornDifference, 0.01f);
}

TEST(GroundPaint, AContinuationAffectsTheNeighbouringTileSignature) {
    zoo::World world(8, 8);
    const std::uint32_t before = PaintSignature(world, {3, 2});

    // This is the preceding continuation of the tile's north side, rather than
    // one of its own four edges, but its rounded worn end reaches into the tile.
    world.SetFence({2, 2, true}, {.kind = 1, .gate = true});

    EXPECT_NE(PaintSignature(world, {3, 2}), before);
}

TEST(GroundPaint, FenceEndsTurnsAndGatesStayContinuousAcrossTileSides) {
    zoo::World world(8, 8);
    // A free end, an L turn, a straight join and a gate-to-panel transition.
    world.SetFence({2, 2, true}, {.kind = 1});
    world.SetFence({2, 2, false}, {.kind = 1});
    world.SetFence({2, 3, false}, {.kind = 1, .gate = true});
    world.SetFence({4, 4, true}, {.kind = 1, .gate = true});
    world.SetFence({5, 4, true}, {.kind = 1});

    for (int z = 1; z < world.Depth() - 1; ++z) {
        for (int x = 1; x < world.Width() - 1; ++x) {
            const TilePaintInfo here = PaintInfo(world, {x, z});
            const TilePaintInfo east = PaintInfo(world, {x + 1, z});
            const TilePaintInfo south = PaintInfo(world, {x, z + 1});
            for (int sample = 0; sample <= 8; ++sample) {
                const float along = static_cast<float>(sample) * zoo::kTileSize / 8.0f;
                const float edgeX = static_cast<float>(x + 1) * zoo::kTileSize;
                const float edgeZ = static_cast<float>(z + 1) * zoo::kTileSize;
                const GroundSample fromWest =
                    SampleGround(here, edgeX, static_cast<float>(z) * zoo::kTileSize + along);
                const GroundSample fromEast =
                    SampleGround(east, edgeX, static_cast<float>(z) * zoo::kTileSize + along);
                const GroundSample fromNorth =
                    SampleGround(here, static_cast<float>(x) * zoo::kTileSize + along, edgeZ);
                const GroundSample fromSouth =
                    SampleGround(south, static_cast<float>(x) * zoo::kTileSize + along, edgeZ);
                for (std::size_t channel = 0; channel < fromWest.color.size(); ++channel) {
                    EXPECT_NEAR(fromWest.color[channel], fromEast.color[channel], 1.0e-6f)
                        << "vertical edge at " << x + 1 << "," << z << " sample " << sample;
                    EXPECT_NEAR(fromNorth.color[channel], fromSouth.color[channel], 1.0e-6f)
                        << "horizontal edge at " << x << "," << z + 1 << " sample " << sample;
                }
                EXPECT_EQ(fromWest.detail, fromEast.detail);
                EXPECT_EQ(fromNorth.detail, fromSouth.detail);
            }
        }
    }
}

}  // namespace
}  // namespace zoogame
