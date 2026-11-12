// SPDX-License-Identifier: MIT
#include "zoo/world/World.hpp"

#include <gtest/gtest.h>

namespace zoo {
namespace {

TEST(WorldGen, SameParametersAreIdenticalAndSeedsChangeRelief) {
    WorldGenParams params;
    params.width = 48;
    params.depth = 40;
    params.seed = 123;
    const World a = World::Generate(params);
    const World b = World::Generate(params);
    EXPECT_TRUE(a == b);
    params.seed = 124;
    const World c = World::Generate(params);
    EXPECT_FALSE(a == c);
}

TEST(WorldGen, PropertyWallHasOnlyItsEntranceGate) {
    WorldGenParams params;
    params.width = 48;
    params.depth = 40;
    params.margin = 5;
    params.entranceWidth = 6;
    const World world = World::Generate(params);
    int gates = 0;
    for (int z = 0; z < world.Depth(); ++z)
        for (int x = 0; x < world.Width(); ++x) {
            const TileCoord t{x, z};
            if (!world.IsProperty(t))
                continue;
            for (Dir dir : {Dir::North, Dir::East, Dir::South, Dir::West}) {
                const TileCoord neighbor = t + Step(dir);
                if (world.IsProperty(neighbor))
                    continue;
                const Fence fence = world.FenceOn(t, dir);
                EXPECT_EQ(fence.kind, kPerimeterFence);
                if (fence.gate) {
                    ++gates;
                    EXPECT_EQ(dir, Dir::South);
                    EXPECT_EQ(t.z, world.PropertyMax().z);
                }
            }
        }
    EXPECT_EQ(gates, params.entranceWidth);
}

TEST(WorldGen, PlazaAndApproachArePavedAndSpawnIsAtSouthEdge) {
    WorldGenParams params;
    params.width = 40;
    params.depth = 40;
    params.margin = 5;
    params.entranceWidth = 4;
    const World world = World::Generate(params);
    const TileCoord approach = world.ApproachTile();
    EXPECT_EQ(world.At(approach).path, PathKind::Asphalt);
    EXPECT_EQ(world.At({approach.x, world.Depth() - 1}).path, PathKind::Asphalt);
    EXPECT_EQ(world.At(world.EntranceTile()).path, PathKind::Asphalt);
    EXPECT_FLOAT_EQ(world.SpawnPoint().z, (static_cast<float>(world.Depth()) - 0.5f) * kTileSize);
}

TEST(WorldGen, ReliefZeroIsFlatAndGeneratedReliefIsBuildable) {
    WorldGenParams params;
    params.width = 64;
    params.depth = 64;
    params.relief = 0.0f;
    const World flat = World::Generate(params);
    for (int z = 0; z <= flat.Depth(); ++z)
        for (int x = 0; x <= flat.Width(); ++x)
            EXPECT_FLOAT_EQ(flat.VertexHeight(x, z), 0.0f);
    for (std::uint64_t seed = 1; seed <= 5; ++seed) {
        params.seed = seed;
        params.relief = 1.0f;
        const World rolling = World::Generate(params);
        int buildable = 0;
        int total = 0;
        for (int z = rolling.PropertyMin().z; z <= rolling.PropertyMax().z; ++z)
            for (int x = rolling.PropertyMin().x; x <= rolling.PropertyMax().x; ++x) {
                ++total;
                if (rolling.TileRelief({x, z}) < 0.5f)
                    ++buildable;
            }
        EXPECT_GE(static_cast<float>(buildable) / static_cast<float>(total), 0.95f) << "seed " << seed;
    }
}

}  // namespace
}  // namespace zoo
