// SPDX-License-Identifier: MIT
#include "zoo/nav/Nav.hpp"

#include <gtest/gtest.h>

namespace zoo {
namespace {

TEST(Navigator, StraightPathSimplifiesAndSameTileEndsAtDestination) {
    World world(12, 12);
    Navigator navigator(world);
    std::vector<Vec2> path;
    ASSERT_TRUE(navigator.FindPath(World::TileCenter({1, 1}), World::TileCenter({9, 8}), {}, path));
    EXPECT_LE(path.size(), 2u);
    EXPECT_EQ(path.back(), World::TileCenter({9, 8}));
    ASSERT_TRUE(navigator.FindPath({3.1f, 3.2f}, {3.8f, 3.7f}, {}, path));
    ASSERT_EQ(path.size(), 1u);
    EXPECT_EQ(path.front(), (Vec2{3.8f, 3.7f}));
}

TEST(Navigator, GatesAndFenceGapsControlRoutes) {
    World world(12, 12);
    for (int z = 0; z <= 11; ++z) {
        if (z != 6) {
            world.SetFence({5, z, false}, {1, false, 1.0f});
        }
    }
    Navigator navigator(world);
    std::vector<Vec2> path;
    ASSERT_TRUE(navigator.FindPath(World::TileCenter({3, 6}), World::TileCenter({7, 6}), {}, path));
    world.SetFence({5, 6, false}, {1, true, 1.0f});
    EXPECT_FALSE(Navigator(world).FindPath(World::TileCenter({3, 6}), World::TileCenter({7, 6}), {}, path));
    EXPECT_TRUE(Navigator(world).FindPath(World::TileCenter({3, 6}), World::TileCenter({7, 6}),
                                          NavProfile::Staff(), path));
}

TEST(Navigator, RingBlocksAndCacheTracksRevision) {
    World world(10, 10);
    for (int x = 3; x <= 6; ++x) {
        world.SetFence({x, 3, true}, {1, false, 1.0f});
        world.SetFence({x, 7, true}, {1, false, 1.0f});
    }
    for (int z = 3; z <= 6; ++z) {
        world.SetFence({3, z, false}, {1, false, 1.0f});
        world.SetFence({7, z, false}, {1, false, 1.0f});
    }
    Navigator navigator(world);
    EXPECT_FALSE(navigator.Reachable({4, 4}, {1, 1}, {}));
    std::vector<Vec2> path;
    EXPECT_FALSE(navigator.FindPath(World::TileCenter({4, 4}), World::TileCenter({1, 1}), {}, path));
}

TEST(Navigator, BuildingsWaterAndVisitorPathsAreRespected) {
    World world(14, 14);
    for (int x = 0; x < world.Width(); ++x) {
        world.SetPath({x, 6}, PathKind::Asphalt);
        world.SetPath({x, 5}, PathKind::Asphalt);
        world.SetPath({x, 7}, PathKind::Asphalt);
    }
    world.SetBuilding({7, 6}, BuildingId{999});
    Navigator navigator(world);
    std::vector<Vec2> path;
    ASSERT_TRUE(navigator.FindPath(World::TileCenter({1, 6}), World::TileCenter({12, 6}),
                                   NavProfile::Visitor(), path));
    Vec2 previous = World::TileCenter({1, 6});
    for (const Vec2 waypoint : path) {
        for (int i = 0; i <= 20; ++i) {
            const Vec2 sample = Lerp(previous, waypoint, static_cast<float>(i) / 20.0f);
            EXPECT_NE(world.At(World::TileAt(sample)).path, PathKind::None);
        }
        previous = waypoint;
    }
}

TEST(Navigator, DistanceFieldNearestPassableAndFollowPath) {
    World world(8, 8);
    Navigator navigator(world);
    std::vector<float> field;
    const std::array<TileCoord, 1> goals = {{{1, 1}}};
    navigator.DistanceField(goals, {}, field);
    EXPECT_FLOAT_EQ(field[1u * 8u + 1u], 0.0f);
    EXPECT_NEAR(field[1u * 8u + 2u], 1.0f, 0.001f);
    EXPECT_EQ(navigator.NearestPassable({2, 2}, {}, 0), TileCoord({2, 2}));
    const std::array<Vec2, 3> points = {{{1.0f, 0.0f}, {2.0f, 0.0f}, {3.0f, 0.0f}}};
    std::size_t index = 0;
    Vec2 position{};
    EXPECT_TRUE(FollowPath(points, index, position, 3.0f));
    EXPECT_EQ(position, points.back());
    EXPECT_TRUE(FollowPath({}, index, position, 1.0f));
}

TEST(Navigator, EntranceGateAndWaterCostAreProfileSpecific) {
    World world = World::Generate({});
    Navigator navigator(world);
    std::vector<Vec2> path;
    const Vec2 spawn = world.SpawnPoint();
    const Vec2 entrance = World::TileCenter(world.EntranceTile());
    ASSERT_TRUE(navigator.FindPath(spawn, entrance, NavProfile::Visitor(), path));
    NavProfile noEntrance = NavProfile::Visitor();
    noEntrance.useEntrance = false;
    EXPECT_FALSE(navigator.FindPath(spawn, entrance, noEntrance, path));

    World pond(12, 8);
    pond.SetWater({5, 3}, 1.0f, 0.8f);
    pond.SetWater({5, 4}, 1.0f, 0.8f);
    NavProfile wader = NavProfile::Animal(2.0f, false);
    wader.waterCost = 20.0f;
    Navigator pondNavigator(pond);
    ASSERT_TRUE(pondNavigator.FindPath(World::TileCenter({2, 3}), World::TileCenter({9, 3}), wader, path));
    Vec2 previous = World::TileCenter({2, 3});
    bool crossedWater = false;
    for (const Vec2 waypoint : path) {
        for (int sampleIndex = 0; sampleIndex <= 40; ++sampleIndex) {
            const Vec2 sample = Lerp(previous, waypoint, static_cast<float>(sampleIndex) / 40.0f);
            crossedWater = crossedWater || pond.HasWater(World::TileAt(sample));
        }
        previous = waypoint;
    }
    EXPECT_FALSE(crossedWater);
}

TEST(Navigator, VertexTraversalCornerCuttingAndDeterminism) {
    World world(10, 10);
    Navigator navigator(world);
    const Vec2 start = World::TileCenter({1, 1});
    const Vec2 end = World::TileCenter({7, 7});
    std::vector<Vec2> first;
    std::vector<Vec2> second;
    ASSERT_TRUE(navigator.FindPath(start, end, {}, first));
    ASSERT_TRUE(navigator.FindPath(World::TileCenter({8, 1}), World::TileCenter({1, 8}), {}, second));
    ASSERT_TRUE(navigator.FindPath(start, end, {}, second));
    EXPECT_EQ(first, second);
    EXPECT_TRUE(navigator.Walkable(start, end, {}));
    world.SetBuilding({2, 2}, BuildingId{77});
    EXPECT_FALSE(Navigator(world).Walkable(start, end, {}));

    World corner(6, 6);
    corner.SetFence({2, 1, false}, {1, false, 1.0f});
    Navigator cornerNavigator(corner);
    EXPECT_FALSE(cornerNavigator.Walkable(World::TileCenter({1, 1}), World::TileCenter({2, 2}), {}));
}

TEST(Navigator, EndpointOnFenceDoesNotCrossTheUnexaminedFinalEdge) {
    World world(8, 8);
    const Vec2 west = World::TileCenter({2, 3});
    const Vec2 east = World::TileCenter({3, 3});
    const Vec2 edge = {(west.x + east.x) * 0.5f, west.z};
    Navigator open(world);
    EXPECT_TRUE(open.Walkable(west, edge, {}));
    EXPECT_TRUE(open.Walkable(edge, west, {}));

    world.SetFence(EdgeOf({2, 3}, Dir::East), {1, false, 1.0f});
    Navigator closed(world);
    // The exact endpoint belongs to the east tile under World::TileAt, so reaching it from the
    // west crosses the fence at t=1; starting there and heading west crosses it at t=0.
    EXPECT_FALSE(closed.Walkable(west, edge, {}));
    EXPECT_FALSE(closed.Walkable(edge, west, {}));
    EXPECT_TRUE(closed.Walkable(east, edge, {}));
    EXPECT_TRUE(closed.Walkable(edge, east, {}));
}

TEST(Navigator, PerRouteCircularObstaclesAreWalkedAroundDeterministically) {
    World world(12, 9);
    Navigator navigator(world);
    const Vec2 start = World::TileCenter({1, 4});
    const Vec2 end = World::TileCenter({10, 4});
    const std::array<NavObstacle, 1> obstacles = {{{World::TileCenter({5, 4}), 1.3f}}};
    EXPECT_TRUE(navigator.Walkable(start, end, {}));
    EXPECT_FALSE(navigator.Walkable(start, end, {}, obstacles));

    std::vector<Vec2> first;
    std::vector<Vec2> second;
    ASSERT_TRUE(navigator.FindPath(start, end, {}, first, 60000, obstacles));
    ASSERT_TRUE(navigator.FindPath(start, end, {}, second, 60000, obstacles));
    EXPECT_EQ(first, second);
    ASSERT_GE(first.size(), 2u);
    EXPECT_EQ(first.back(), end);
    Vec2 previous = start;
    for (const Vec2 point : first) {
        EXPECT_TRUE(navigator.Walkable(previous, point, {}, obstacles));
        previous = point;
    }
}

TEST(Navigator, ARouteMayLeadOutOfAnObstacleItAlreadyStartsInside) {
    World world(8, 8);
    Navigator navigator(world);
    const Vec2 start = World::TileCenter({2, 2});
    const std::array<NavObstacle, 1> obstacles = {{{start, 1.0f}}};
    EXPECT_TRUE(navigator.Walkable(start, World::TileCenter({4, 2}), {}, obstacles));
    EXPECT_FALSE(navigator.Walkable(World::TileCenter({1, 2}), World::TileCenter({3, 2}), {}, obstacles));
}

TEST(Navigator, FenceCacheInvalidationDistanceFieldAndExpansionLimit) {
    World world(8, 8);
    Navigator navigator(world);
    const TileCoord left{2, 3};
    const TileCoord right{5, 3};
    EXPECT_TRUE(navigator.Reachable(left, right, {}));
    for (int z = 0; z < world.Depth(); ++z) {
        world.SetFence({4, z, false}, {1, false, 1.0f});
    }
    EXPECT_FALSE(navigator.Reachable(left, right, {}));
    std::vector<float> field;
    const std::array<TileCoord, 1> goals = {left};
    navigator.DistanceField(goals, {}, field);
    EXPECT_EQ(field[3u * 8u + 5u], -1.0f);
    std::vector<Vec2> path;
    EXPECT_FALSE(navigator.FindPath(World::TileCenter(left), World::TileCenter(right), {}, path, 1));
}

TEST(Navigator, GeneratedWorldHasVisitorRouteFromSpawn) {
    const World world = World::Generate({});
    Navigator navigator(world);
    std::vector<Vec2> path;
    EXPECT_TRUE(navigator.FindPath(world.SpawnPoint(), World::TileCenter(world.EntranceTile()),
                                   NavProfile::Visitor(), path));
}

}  // namespace
}  // namespace zoo
