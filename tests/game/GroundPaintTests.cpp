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

TEST(GroundPaint, BuildingFoundationCrossesTheTileEdgeAndEndsInARoundedApron) {
    zoo::World world(8, 8);
    world.SetBuilding({2, 2}, zoo::BuildingId{1});

    const TilePaintInfo building = PaintInfo(world, {2, 2});
    const TilePaintInfo east = PaintInfo(world, {3, 2});
    for (int sample = 0; sample <= 8; ++sample) {
        const float z = 4.0f + static_cast<float>(sample) * zoo::kTileSize / 8.0f;
        const GroundSample fromBuilding = SampleGround(building, 6.0f, z);
        const GroundSample fromOutside = SampleGround(east, 6.0f, z);
        for (std::size_t channel = 0; channel < fromBuilding.color.size(); ++channel)
            EXPECT_NEAR(fromBuilding.color[channel], fromOutside.color[channel], 1.0e-6f) << "sample " << sample;
        EXPECT_EQ(fromBuilding.detail, fromOutside.detail) << "sample " << sample;
    }

    zoo::World bareWorld(8, 8);
    const TilePaintInfo bareEast = PaintInfo(bareWorld, {3, 2});
    const GroundSample apron = SampleGround(east, 6.1f, 5.0f);
    const GroundSample roundedCorner = SampleGround(PaintInfo(world, {3, 3}), 6.1f, 6.1f);
    const GroundSample beyond = SampleGround(east, 6.3f, 5.0f);
    const GroundSample bareApron = SampleGround(bareEast, 6.1f, 5.0f);
    const GroundSample bareCorner = SampleGround(PaintInfo(bareWorld, {3, 3}), 6.1f, 6.1f);
    const GroundSample bareBeyond = SampleGround(bareEast, 6.3f, 5.0f);

    float apronDifference = 0.0f;
    float cornerDifference = 0.0f;
    for (std::size_t channel = 0; channel < apron.color.size(); ++channel) {
        apronDifference += std::abs(apron.color[channel] - bareApron.color[channel]);
        cornerDifference += std::abs(roundedCorner.color[channel] - bareCorner.color[channel]);
        EXPECT_FLOAT_EQ(beyond.color[channel], bareBeyond.color[channel]);
    }
    EXPECT_GT(apronDifference, 0.05f);
    EXPECT_GT(cornerDifference, 0.01f);
    EXPECT_EQ(apron.detail, GroundDetail::Grain);
    EXPECT_EQ(beyond.detail, bareBeyond.detail);
}

TEST(GroundPaint, DeepPoolBankIsBroadAndReachesItsBedBeforeTheNextTile) {
    zoo::World world(10, 10);
    for (int z = 2; z <= 7; ++z) {
        for (int x = 2; x <= 7; ++x)
            world.SetWater({x, z}, 0.0f, 1.4f);
    }
    const TilePaintInfo bank = PaintInfo(world, {4, 2});
    constexpr float cell = zoo::kTileSize / static_cast<float>(kGroundCells);
    float steepest = 0.0f;
    float narrowestShallowBand = zoo::kTileSize;
    for (int column = 0; column <= kGroundCells; ++column) {
        const float x = 8.0f + static_cast<float>(column) * cell;
        float previous = GroundHeight(bank, x, 4.0f);
        float twentyCentimetreDepthAt = zoo::kTileSize;
        float halfMetreDepthAt = zoo::kTileSize;
        for (int step = 1; step <= kGroundCells; ++step) {
            const float distance = static_cast<float>(step) * cell;
            const float height = GroundHeight(bank, x, 4.0f + distance);
            steepest = std::max(steepest, (previous - height) / cell);
            const float previousDistance = distance - cell;
            if (height <= -0.2f && twentyCentimetreDepthAt == zoo::kTileSize)
                twentyCentimetreDepthAt =
                    previousDistance + cell * (previous + 0.2f) / (previous - height);
            if (height <= -0.5f && halfMetreDepthAt == zoo::kTileSize)
                halfMetreDepthAt = previousDistance + cell * (previous + 0.5f) / (previous - height);
            previous = height;
        }
        narrowestShallowBand =
            std::min(narrowestShallowBand, halfMetreDepthAt - twentyCentimetreDepthAt);
        EXPECT_FLOAT_EQ(previous, -1.4f);
    }
    // The former 0.9 m fall reached 3.19 m/m over these nine mesh columns and
    // narrowed the 20-to-50 cm band to 11 cm. The broader profile is still a
    // purposeful animal-pool bank, but no longer reads as a near-vertical wall.
    EXPECT_LT(steepest, 1.6f);
    EXPECT_GT(narrowestShallowBand, 0.2f);

    // The 25 cm render lattice used to hide a sharper part of the continuous profile. Probe at
    // 12.5 cm as well: increasing the render resolution must not rediscover an engineered wall.
    constexpr int fineCells = kGroundCells * 2;
    constexpr float fineCell = zoo::kTileSize / static_cast<float>(fineCells);
    float fineSteepest = 0.0f;
    for (int column = 0; column <= fineCells; ++column) {
        const float x = 8.0f + static_cast<float>(column) * fineCell;
        float previous = GroundHeight(bank, x, 4.0f);
        for (int step = 1; step <= fineCells; ++step) {
            const float height = GroundHeight(bank, x, 4.0f + static_cast<float>(step) * fineCell);
            fineSteepest = std::max(fineSteepest, (previous - height) / fineCell);
            previous = height;
        }
    }
    EXPECT_LT(fineSteepest, 1.6f);
}

}  // namespace
}  // namespace zoogame
