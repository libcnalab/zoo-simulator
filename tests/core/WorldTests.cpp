// SPDX-License-Identifier: MIT
#include "zoo/world/World.hpp"

#include <gtest/gtest.h>

#include <cmath>

namespace zoo {
namespace {

TEST(World, ConstructionClampsDimensionsAndStartsFlat) {
    const World world(2, 600);
    EXPECT_EQ(world.Width(), 8);
    EXPECT_EQ(world.Depth(), 512);
    EXPECT_TRUE(world.IsProperty({7, 511}));
    EXPECT_FALSE(world.HasWater({-1, 0}));
    EXPECT_FLOAT_EQ(world.HeightAt({3.0f, 4.0f}), 0.0f);
    EXPECT_EQ(world.At(world.EntranceTile()).terrain, Terrain::Grass);
}

TEST(World, HeightInterpolationUsesDocumentedDiagonalAndIsContinuous) {
    World world(8, 8);
    world.SetVertexHeight(0, 0, 0.0f);
    world.SetVertexHeight(1, 0, 2.0f);
    world.SetVertexHeight(0, 1, 4.0f);
    world.SetVertexHeight(1, 1, 6.0f);
    EXPECT_FLOAT_EQ(world.HeightAt({0.5f, 0.5f}), 1.5f);
    EXPECT_FLOAT_EQ(world.HeightAt({1.5f, 1.5f}), 4.5f);
    EXPECT_FLOAT_EQ(world.HeightAt({1.0f, 1.0f}), 3.0f);
    EXPECT_FLOAT_EQ(world.HeightAt({-100.0f, -100.0f}), 0.0f);
    EXPECT_NEAR(world.NormalAt({0.5f, 0.5f}).y, 1.0f / std::sqrt(6.0f), 1.0e-5f);
    world.SetVertexHeight(1, 0, 4.0f);
    world.SetVertexHeight(1, 1, 8.0f);
    world.SetVertexHeight(0, 1, 0.0f);
    EXPECT_FLOAT_EQ(world.HeightAt({2.0f, 1.0f}), 6.0f);
}

TEST(World, VertexBoundsAndHeightLimitsAreEnforced) {
    World world(8, 8);
    world.SetVertexHeight(-5, -2, 99.0f);
    EXPECT_FLOAT_EQ(world.VertexHeight(0, 0), World::kMaxHeight);
    world.SetVertexHeight(100, 100, -99.0f);
    EXPECT_FLOAT_EQ(world.VertexHeight(8, 8), World::kMinHeight);
}

TEST(World, BrushesFallOffProtectBordersAndBuildings) {
    World world(16, 16);
    const Vec2 center{8.0f, 8.0f};
    world.RaiseBrush(center, 8.0f, 4.0f);
    EXPECT_GT(world.VertexHeight(4, 4), world.VertexHeight(5, 4));
    EXPECT_FLOAT_EQ(world.VertexHeight(1, 1), 0.0f);
    const float outside = world.VertexHeight(14, 14);
    world.SetBuilding({4, 4}, BuildingId{7});
    const float underBuilding = world.VertexHeight(4, 4);
    world.RaiseBrush({8.0f, 8.0f}, 12.0f, 10.0f);
    EXPECT_FLOAT_EQ(world.VertexHeight(4, 4), underBuilding);
    EXPECT_FLOAT_EQ(world.VertexHeight(14, 14), outside);
    world.FlattenBrush(center, 8.0f, 2.0f, 1.0f);
    EXPECT_NEAR(world.VertexHeight(4, 4), underBuilding, 1.0e-6f);
    world.FlattenTiles({0, 0}, {0, 0}, 3.0f);
    EXPECT_FLOAT_EQ(world.VertexHeight(0, 0), 3.0f);
}

TEST(World, FlattenAndSmoothBrushMoveGradually) {
    World world(16, 16);
    world.RaiseBrush({14.0f, 14.0f}, 8.0f, 8.0f);
    const float centerBefore = world.VertexHeight(7, 7);
    const float edgeBefore = world.VertexHeight(5, 7);
    world.FlattenBrush({14.0f, 14.0f}, 8.0f, 1.0f, 0.5f);
    EXPECT_LT(world.VertexHeight(7, 7), centerBefore);
    EXPECT_GT(world.VertexHeight(5, 7), 0.0f);
    EXPECT_LE(world.VertexHeight(5, 7), edgeBefore);
    world.SetVertexHeight(8, 8, 20.0f);
    const float neighborBefore = world.VertexHeight(7, 8);
    world.SmoothBrush({16.0f, 16.0f}, 6.0f, 1.0f);
    EXPECT_LT(world.VertexHeight(8, 8), 20.0f);
    EXPECT_GT(world.VertexHeight(7, 8), neighborBefore);
}

TEST(World, WaterDigsWithoutRaisingAndCanBeCleared) {
    World world(8, 8);
    world.SetVertexHeight(2, 2, 5.0f);
    world.SetWater({1, 1}, 3.0f, 1.0f);
    EXPECT_FLOAT_EQ(world.VertexHeight(2, 2), 2.0f);
    EXPECT_FLOAT_EQ(world.WaterDepthAt(World::TileCenter({1, 1})), 2.0f);
    EXPECT_FLOAT_EQ(world.WaterDepthAt(World::TileCenter({2, 2})), 0.0f);
    world.SetWater({1, 1}, 8.0f, 0.5f);
    EXPECT_FLOAT_EQ(world.VertexHeight(2, 2), 2.0f);
    world.ClearWater({1, 1});
    EXPECT_FALSE(world.HasWater({1, 1}));
    EXPECT_FLOAT_EQ(world.VertexHeight(2, 2), 2.0f);
}

TEST(World, FenceStorageIsSharedByNeighbourSidesAndBoundsAreSafe) {
    World world(8, 8);
    const Fence fence{3, true, 0.75f};
    world.SetFence(EdgeOf({2, 3}, Dir::East), fence);
    EXPECT_EQ(world.FenceOn({2, 3}, Dir::East), world.FenceOn({3, 3}, Dir::West));
    EXPECT_EQ(world.FenceOn({2, 3}, Dir::East), fence);
    EXPECT_TRUE(world.FenceAt({-1, 0, true}) == Fence{});
    world.SetFence({-1, 0, true}, fence);
    EXPECT_TRUE(world.FenceAt({-1, 0, true}) == Fence{});
    for (const Dir side : {Dir::North, Dir::East, Dir::South, Dir::West}) {
        const TileCoord tile{3, 3};
        const TileCoord neighbor = tile + Step(side);
        const Fence shared{static_cast<std::uint8_t>(static_cast<int>(side) + 4), false, 0.5f};
        world.SetFence(EdgeOf(tile, side), shared);
        EXPECT_EQ(world.FenceOn(tile, side), world.FenceOn(neighbor, Opposite(side)));
    }
}

TEST(World, RevisionsChangeOnlyRelevantChunksAndCaches) {
    World world(32, 32);
    const auto c00 = world.ChunkRevision(0, 0);
    const auto c10 = world.ChunkRevision(1, 0);
    world.SetTerrain({3, 4}, Terrain::Dirt);
    EXPECT_GT(world.ChunkRevision(0, 0), c00);
    EXPECT_EQ(world.ChunkRevision(1, 0), c10);
    const auto after = world.ChunkRevision(0, 0);
    world.SetTerrain({3, 4}, Terrain::Dirt);
    EXPECT_EQ(world.ChunkRevision(0, 0), after);
    world.SetPath({15, 5}, PathKind::Asphalt);
    EXPECT_GT(world.ChunkRevision(1, 0), c10);
    const auto nav = world.NavRevision();
    world.SetBuilding({3, 4}, BuildingId{2});
    EXPECT_GT(world.NavRevision(), nav);
    const auto fences = world.FenceRevision();
    world.SetFence({1, 1, true}, {2, false, 1.0f});
    EXPECT_GT(world.FenceRevision(), fences);
    World corner(32, 32);
    const auto a = corner.ChunkRevision(0, 0);
    const auto b = corner.ChunkRevision(1, 0);
    const auto c = corner.ChunkRevision(0, 1);
    const auto d = corner.ChunkRevision(1, 1);
    corner.SetVertexHeight(16, 16, 1.0f);
    EXPECT_GT(corner.ChunkRevision(0, 0), a);
    EXPECT_GT(corner.ChunkRevision(1, 0), b);
    EXPECT_GT(corner.ChunkRevision(0, 1), c);
    EXPECT_GT(corner.ChunkRevision(1, 1), d);
    const auto afterA = corner.ChunkRevision(0, 0);
    const auto afterB = corner.ChunkRevision(1, 0);
    const auto afterC = corner.ChunkRevision(0, 1);
    const auto afterD = corner.ChunkRevision(1, 1);
    corner.SetTerrain({15, 15}, Terrain::Dirt);
    EXPECT_GT(corner.ChunkRevision(0, 0), afterA);
    EXPECT_GT(corner.ChunkRevision(1, 0), afterB);
    EXPECT_GT(corner.ChunkRevision(0, 1), afterC);
    EXPECT_EQ(corner.ChunkRevision(1, 1), afterD);
}

TEST(World, RaycastFindsTerrainAndHonoursDistanceAndBounds) {
    World world(8, 8);
    const auto down = world.Raycast({4.0f, 10.0f, 4.0f}, {0.0f, -4.0f, 0.0f}, 20.0f);
    ASSERT_TRUE(down.has_value());
    EXPECT_NEAR(down->y, 0.0f, 0.001f);
    EXPECT_FALSE(world.Raycast({4.0f, 10.0f, 4.0f}, {0.0f, -1.0f, 0.0f}, 9.0f));
    EXPECT_FALSE(world.Raycast({4.0f, -1.0f, 4.0f}, {0.0f, 1.0f, 0.0f}, 10.0f));
    EXPECT_FALSE(world.Raycast({-5.0f, 10.0f, 4.0f}, {0.0f, 1.0f, 0.0f}, 20.0f));
    const auto outside = world.Raycast({4.0f, 10.0f, -8.0f}, {0.0f, -1.0f, 1.0f}, 30.0f);
    ASSERT_TRUE(outside.has_value());
    EXPECT_NEAR(outside->y, world.HeightAt({outside->x, outside->z}), 0.01f);
    const auto oblique = world.Raycast({2.0f, 10.0f, 2.0f}, {0.2f, -1.0f, 0.1f}, 20.0f);
    ASSERT_TRUE(oblique.has_value());
    EXPECT_NEAR(oblique->y, world.HeightAt({oblique->x, oblique->z}), 0.01f);
    world.SetVertexHeight(4, 4, 5.0f);
    const auto slope = world.Raycast({6.0f, 10.0f, 6.0f}, {0.0f, -1.0f, 0.0f}, 20.0f);
    ASSERT_TRUE(slope.has_value());
    EXPECT_NEAR(slope->y, world.HeightAt({slope->x, slope->z}), 0.01f);
    const auto occluded = world.Raycast({2.0f, 3.0f, 8.0f}, {1.0f, 0.0f, 0.0f}, 20.0f);
    ASSERT_TRUE(occluded.has_value());
    EXPECT_LT(occluded->x, 8.0f);
}

}  // namespace
}  // namespace zoo
