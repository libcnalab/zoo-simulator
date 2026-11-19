// SPDX-License-Identifier: MIT
#include <array>

#include <gtest/gtest.h>

#include "StaticScene.hpp"

namespace zoogame {
namespace {

TEST(StaticScene, IgnoresSimulationStateThatHasNoVisualGeometry) {
    zoo::World first(4, 4);
    first.SetFence({1, 1, true}, {.kind = 2, .gate = false, .condition = 1.0f});
    zoo::ZooObjects firstObjects;
    const zoo::BuildingId building =
        firstObjects.AddBuilding({.id = {}, .def = 3, .origin = {1, 1}, .facing = zoo::Dir::East});
    const zoo::PropId prop = firstObjects.AddProp(
        {.id = {}, .def = 7, .position = {3.0f, 5.0f}, .heading = 0.4f, .scale = 1.1f});

    zoo::World second = first;
    zoo::ZooObjects secondObjects = firstObjects;
    std::array<float, 256> wear{};
    std::array<int, 256> fenceCounts{};
    wear[2] = 0.25f;
    second.WearFences(wear, fenceCounts);
    zoo::Building* changedBuilding = secondObjects.FindBuilding(building);
    ASSERT_NE(changedBuilding, nullptr);
    changedBuilding->condition = 0.4f;
    changedBuilding->itemPrice = 123;
    changedBuilding->open = false;
    changedBuilding->servedToday = 8;
    zoo::Prop* changedProp = secondObjects.FindProp(prop);
    ASSERT_NE(changedProp, nullptr);
    changedProp->amount = 17.0f;
    changedProp->food = 2;
    changedProp->filledDay = 4.5f;

    EXPECT_TRUE(SameStaticScene(first, firstObjects, second, secondObjects));
}

TEST(StaticScene, DetectsEveryKindOfGeometryEdit) {
    zoo::World world(4, 4);
    zoo::ZooObjects objects;
    const zoo::BuildingId building =
        objects.AddBuilding({.id = {}, .def = 3, .origin = {1, 1}, .facing = zoo::Dir::East});
    const zoo::PropId prop =
        objects.AddProp({.id = {}, .def = 7, .position = {3.0f, 5.0f}, .heading = 0.4f, .scale = 1.1f});

    zoo::World changedTerrain = world;
    changedTerrain.SetTerrain({0, 0}, zoo::Terrain::Sand);
    EXPECT_FALSE(SameStaticScene(world, objects, changedTerrain, objects));

    zoo::World changedHeight = world;
    changedHeight.SetVertexHeight(2, 2, 1.0f);
    EXPECT_FALSE(SameStaticScene(world, objects, changedHeight, objects));

    zoo::World changedFence = world;
    changedFence.SetFence({1, 1, true}, {.kind = 2});
    EXPECT_FALSE(SameStaticScene(world, objects, changedFence, objects));

    zoo::ZooObjects changedBuilding = objects;
    changedBuilding.FindBuilding(building)->facing = zoo::Dir::North;
    EXPECT_FALSE(SameStaticScene(world, objects, world, changedBuilding));

    zoo::ZooObjects changedProp = objects;
    changedProp.FindProp(prop)->heading += 0.2f;
    EXPECT_FALSE(SameStaticScene(world, objects, world, changedProp));
}

}  // namespace
}  // namespace zoogame
