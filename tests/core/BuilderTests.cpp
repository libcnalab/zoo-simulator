// SPDX-License-Identifier: MIT
#include "zoo/build/Builder.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <string>
#include <vector>

namespace zoo {
namespace {

Catalog ShippedCatalog() {
    return Catalog::LoadDirectory(std::string(ZOO_SOURCE_DIR) + "/data/construction");
}

void ExpectSamePlan(const BuildPlan& actual, const BuildPlan& expected) {
    EXPECT_EQ(actual.error, expected.error);
    EXPECT_EQ(actual.cost, expected.cost);
    EXPECT_EQ(actual.count, expected.count);
    EXPECT_EQ(actual.skipped, expected.skipped);
    EXPECT_EQ(actual.skippedBecause, expected.skippedBecause);
    EXPECT_EQ(actual.building, expected.building);
    EXPECT_EQ(actual.prop, expected.prop);
}

int FindProp(const Catalog& catalog, PropRole role, Placement placement) {
    for (std::size_t i = 0; i < catalog.props.size(); ++i) {
        if (catalog.props[i].role == role && catalog.props[i].placement == placement) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

int FindBuilding(const Catalog& catalog, BuildingRole role, Placement placement) {
    for (std::size_t i = 0; i < catalog.buildings.size(); ++i) {
        if (catalog.buildings[i].role == role && catalog.buildings[i].placement == placement) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

TEST(Builder, GeometryHelpersIncludeEndpointsAndAvoidDuplicateEdges) {
    for (std::uint8_t value = static_cast<std::uint8_t>(BuildError::None);
         value <= static_cast<std::uint8_t>(BuildError::NoMoney); ++value) {
        const BuildError error = static_cast<BuildError>(value);
        if (error == BuildError::None) {
            EXPECT_TRUE(Describe(error).empty());
        } else {
            EXPECT_FALSE(Describe(error).empty());
        }
    }
    EXPECT_EQ(TilesInLine({2, 3}, {4, 1}), (std::vector<TileCoord>{{2, 3}, {3, 3}, {4, 3}, {4, 2}, {4, 1}}));
    EXPECT_EQ(TilesInLine({2, 3}, {2, 3}), (std::vector<TileCoord>{{2, 3}}));
    EXPECT_EQ(EdgesAlong({2, 3}, {4, 1}),
              (std::vector<EdgeRef>{{2, 3, true}, {3, 3, true}, {4, 2, false}, {4, 1, false}}));
    EXPECT_TRUE(EdgesAlong({1, 1}, {1, 1}).empty());
    const auto outline = EdgesAround({2, 2}, {3, 3});
    EXPECT_EQ(outline.size(), 8U);
    EXPECT_EQ(outline.front(), (EdgeRef{2, 2, true}));
    EXPECT_EQ(outline[2], (EdgeRef{4, 2, false}));
    EXPECT_EQ(TilesInRect({3, 3}, {2, 2}), (std::vector<TileCoord>{{2, 2}, {3, 2}, {2, 3}, {3, 3}}));
    EXPECT_EQ(EdgesAround({1, 1}, {1, 1}).size(), 4U);
}

TEST(Builder, FencePlanningDeduplicatesAndMutatesExactlyAsPlanned) {
    Catalog catalog = ShippedCatalog();
    World world(16, 16);
    ZooObjects objects;
    Enclosures enclosures;
    enclosures.Rebuild(world, objects, catalog);
    Finance finance(Dollars(5000));
    Builder builder(world, objects, enclosures, catalog, &finance);
    const int kind = catalog.fences.front().kind;
    const EdgeRef edge{5, 5, true};
    const std::array<EdgeRef, 3> edges{edge, edge, {6, 5, true}};
    const BuildPlan plan = builder.PlanFence(edges, kind, true);
    EXPECT_TRUE(plan.Ok());
    EXPECT_EQ(plan.count, 2);
    EXPECT_EQ(plan.skipped, 0);
    EXPECT_EQ(plan.cost, 2 * (catalog.fences.front().cost + catalog.fences.front().gateCost));
    EXPECT_EQ(world.FenceAt(edge), Fence{});
    const BuildPlan placed = builder.PlaceFence(edges, kind, true);
    ExpectSamePlan(placed, plan);
    EXPECT_TRUE(world.FenceAt(edge).gate);
    EXPECT_EQ(finance.Today().amounts[static_cast<std::size_t>(Account::Construction)], plan.cost);
    const Fence worn{static_cast<std::uint8_t>(kind), false, 0.4f};
    world.SetFence(edge, worn);
    const BuildPlan togglePlan = builder.PlanFence(std::span<const EdgeRef>(&edge, 1), kind, true);
    EXPECT_EQ(togglePlan.cost, catalog.fences.front().gateCost);
    ExpectSamePlan(builder.PlaceFence(std::span<const EdgeRef>(&edge, 1), kind, true), togglePlan);
    EXPECT_FLOAT_EQ(world.FenceAt(edge).condition, 0.4f);
    const BuildPlan removePlan = builder.PlanRemoveFence(std::span<const EdgeRef>(&edge, 1));
    EXPECT_TRUE(removePlan.Ok());
    EXPECT_LT(removePlan.cost, 0);
    ExpectSamePlan(builder.RemoveFence(std::span<const EdgeRef>(&edge, 1)), removePlan);
    EXPECT_FALSE(world.FenceAt(edge).Exists());
}

TEST(Builder, FailedAndUnaffordableOperationsLeaveWorldAndObjectsUntouched) {
    Catalog catalog = ShippedCatalog();
    World world(16, 16);
    ZooObjects objects;
    Enclosures enclosures;
    enclosures.Rebuild(world, objects, catalog);
    Finance finance(0);
    Builder builder(world, objects, enclosures, catalog, &finance);
    const World oldWorld = world;
    const ZooObjects oldObjects = objects;
    const std::array<TileCoord, 1> tile{{{5, 5}}};
    const BuildPlan plan = builder.PlanPath(tile, PathKind::Asphalt);
    EXPECT_EQ(plan.error, BuildError::NoMoney);
    ExpectSamePlan(builder.PlacePath(tile, PathKind::Asphalt), plan);
    EXPECT_EQ(world, oldWorld);
    EXPECT_EQ(objects, oldObjects);
    EXPECT_EQ(builder.CheckFence({-1, 0, true}, 1, false), BuildError::OutOfBounds);
    EXPECT_EQ(builder.CheckFence({5, 5, true}, 254, false), BuildError::UnknownItem);
    EXPECT_EQ(builder.CheckPath({16, 2}, PathKind::Asphalt), BuildError::OutOfBounds);
    EXPECT_EQ(builder.CheckWater({-1, 0}), BuildError::OutOfBounds);
    EXPECT_EQ(Describe(BuildError::NoMoney), "There is not enough money.");
    EXPECT_TRUE(Describe(BuildError::None).empty());
}

TEST(Builder, PathsTerrainAndWaterApplyCostAndSurfaceRules) {
    Catalog catalog = ShippedCatalog();
    World world(16, 16);
    ZooObjects objects;
    Enclosures enclosures;
    enclosures.Rebuild(world, objects, catalog);
    Finance finance(Dollars(10000));
    Builder builder(world, objects, enclosures, catalog, &finance);
    const std::array<TileCoord, 2> pathTiles{{{5, 5}, {6, 5}}};
    const BuildPlan pathPlan = builder.PlanPath(pathTiles, PathKind::Asphalt);
    EXPECT_TRUE(pathPlan.Ok());
    ExpectSamePlan(builder.PlacePath(pathTiles, PathKind::Asphalt), pathPlan);
    EXPECT_EQ(world.At({5, 5}).path, PathKind::Asphalt);
    const BuildPlan pathRemoval = builder.PlanPath(pathTiles, PathKind::None);
    EXPECT_TRUE(pathRemoval.Ok());
    ExpectSamePlan(builder.PlacePath(pathTiles, PathKind::None), pathRemoval);
    const std::array<TileCoord, 1> terrainTiles{{{5, 6}}};
    const BuildPlan terrainPlan = builder.PlanTerrain(terrainTiles, Terrain::Dirt);
    ExpectSamePlan(builder.PaintTerrain(terrainTiles, Terrain::Dirt), terrainPlan);
    EXPECT_EQ(world.At({5, 6}).terrain, Terrain::Dirt);
    const std::array<TileCoord, 1> pond{{{7, 5}}};
    const BuildPlan waterPlan = builder.PlanWater(pond, 0.8f);
    EXPECT_TRUE(waterPlan.Ok());
    ExpectSamePlan(builder.PlaceWater(pond, 0.8f), waterPlan);
    EXPECT_NE(world.At({7, 5}).waterLevel, kNoWater);
    const float pondLevel = world.At({7, 5}).waterLevel;
    const std::array<TileCoord, 1> pondExtension{{{8, 5}}};
    const BuildPlan extensionPlan = builder.PlanWater(pondExtension, 1.1f);
    EXPECT_EQ(extensionPlan.cost,
              static_cast<Money>(std::llround(static_cast<double>(Builder::kWaterCostPerTile) * 1.1 / 0.8)));
    ExpectSamePlan(builder.PlaceWater(pondExtension, 1.1f), extensionPlan);
    EXPECT_FLOAT_EQ(world.At({8, 5}).waterLevel, pondLevel);
    const std::array<TileCoord, 2> waterTiles{{{7, 5}, {8, 5}}};
    const BuildPlan removeWater = builder.PlanRemoveWater(waterTiles);
    EXPECT_TRUE(removeWater.Ok());
    ExpectSamePlan(builder.RemoveWater(waterTiles), removeWater);
    EXPECT_FALSE(world.HasWater({7, 5}));
}

TEST(Builder, BuildingsAndPropsPlaceAndRefund) {
    Catalog catalog = ShippedCatalog();
    World world(16, 16);
    ZooObjects objects;
    Enclosures enclosures;
    enclosures.Rebuild(world, objects, catalog);
    Finance finance(Dollars(50000));
    Builder builder(world, objects, enclosures, catalog, &finance);
    int publicBuilding = -1;
    for (std::size_t i = 0; i < catalog.buildings.size(); ++i) {
        if (catalog.buildings[i].placement == Placement::Public) {
            publicBuilding = static_cast<int>(i);
            break;
        }
    }
    ASSERT_GE(publicBuilding, 0);
    const BuildingDef& def = catalog.buildings[static_cast<std::size_t>(publicBuilding)];
    const BuildPlan buildingPlan = builder.PlanBuilding(publicBuilding, {5, 5}, Dir::South);
    ASSERT_TRUE(buildingPlan.Ok()) << Describe(buildingPlan.error);
    const BuildPlan placed = builder.PlaceBuilding(publicBuilding, {5, 5}, Dir::South);
    ExpectSamePlan(placed, buildingPlan);
    ASSERT_TRUE(placed.building);
    EXPECT_EQ(world.At({5, 5}).building, placed.building);
    const BuildPlan demolishPlan = builder.PlanDemolish(placed.building);
    EXPECT_EQ(demolishPlan.cost, -static_cast<Money>(std::floor(0.4 * static_cast<double>(def.cost))));
    ExpectSamePlan(builder.Demolish(placed.building), demolishPlan);
    EXPECT_FALSE(world.At({5, 5}).building);

    int propDef = -1;
    for (std::size_t i = 0; i < catalog.props.size(); ++i) {
        if (catalog.props[i].placement == Placement::Anywhere) {
            propDef = static_cast<int>(i);
            break;
        }
    }
    ASSERT_GE(propDef, 0);
    const Vec2 position{25.0f, 25.0f};
    const BuildPlan propPlan = builder.PlanProp(propDef, position);
    ASSERT_TRUE(propPlan.Ok());
    const BuildPlan prop = builder.PlaceProp(propDef, position, 9.0f, 9.0f);
    ExpectSamePlan(prop, propPlan);
    EXPECT_TRUE(prop.prop);
    EXPECT_EQ(objects.FindProp(prop.prop)->scale, Builder::kMaxPropScale);
    EXPECT_EQ(builder.PropAt(position), prop.prop);
    const PropRole role = catalog.props[static_cast<std::size_t>(propDef)].role;
    const Account expense = role == PropRole::Tree || role == PropRole::Bush || role == PropRole::Rock
                                ? Account::Landscaping
                                : Account::Construction;
    EXPECT_EQ(finance.Today().amounts[static_cast<std::size_t>(expense)], propPlan.cost);
    const BuildPlan removePropPlan = builder.PlanRemoveProp(prop.prop);
    EXPECT_TRUE(removePropPlan.Ok());
    ExpectSamePlan(builder.RemoveProp(prop.prop), removePropPlan);
    EXPECT_GT(finance.Today().amounts[static_cast<std::size_t>(Account::Refunds)], 0);
    EXPECT_FALSE(objects.FindProp(prop.prop));
}

TEST(Builder, BuildingPlacementLevelsAndClearsEveryFacingThenDemolishesFootprint) {
    Catalog catalog = ShippedCatalog();
    World world(16, 16);
    ZooObjects objects;
    Enclosures enclosures;
    enclosures.Rebuild(world, objects, catalog);
    Builder builder(world, objects, enclosures, catalog);
    const int buildingDef = FindBuilding(catalog, BuildingRole::FoodStall, Placement::Public);
    ASSERT_GE(buildingDef, 0);
    const BuildingDef& def = catalog.buildings[static_cast<std::size_t>(buildingDef)];
    const std::array<Dir, 4> facings{Dir::North, Dir::East, Dir::South, Dir::West};
    const std::array<TileCoord, 4> origins{{{5, 8}, {3, 5}, {5, 3}, {8, 5}}};
    for (std::size_t i = 0; i < facings.size(); ++i) {
        const Dir facing = facings[i];
        const TileCoord origin = origins[i];
        const TileCoord size = FootprintSize(def, facing);
        const TileCoord max{origin.x + size.x - 1, origin.z + size.z - 1};
        world.SetVertexHeight(origin.x, origin.z, 0.0f);
        world.SetVertexHeight(max.x + 1, max.z + 1, 1.0f);
        float total = 0.0f;
        int heightCount = 0;
        for (int z = origin.z; z <= max.z + 1; ++z) {
            for (int x = origin.x; x <= max.x + 1; ++x) {
                total += world.VertexHeight(x, z);
                ++heightCount;
            }
        }
        const float mean = total / static_cast<float>(heightCount);
        for (const TileCoord tile : TilesInRect(origin, max)) {
            world.SetPath(tile, PathKind::Asphalt);
        }
        const BuildPlan plan = builder.PlanBuilding(buildingDef, origin, facing);
        ASSERT_TRUE(plan.Ok()) << Describe(plan.error);
        const BuildPlan placed = builder.PlaceBuilding(buildingDef, origin, facing);
        ExpectSamePlan(placed, plan);
        for (const TileCoord tile : TilesInRect(origin, max)) {
            EXPECT_EQ(world.At(tile).building, placed.building);
            EXPECT_EQ(world.At(tile).path, PathKind::None);
        }
        EXPECT_FLOAT_EQ(world.VertexHeight(origin.x, origin.z), mean);
        const BuildPlan demolition = builder.PlanDemolish(placed.building);
        ExpectSamePlan(builder.Demolish(placed.building), demolition);
        for (const TileCoord tile : TilesInRect(origin, max)) {
            EXPECT_FALSE(world.At(tile).building);
        }
    }
}

TEST(Builder, TerraformCostIsRoundedUpAndSandboxBuildsForFree) {
    Catalog catalog = ShippedCatalog();
    World world(16, 16);
    ZooObjects objects;
    Enclosures enclosures;
    enclosures.Rebuild(world, objects, catalog);
    Builder builder(world, objects, enclosures, catalog);
    const BuildPlan plan = builder.PlanTerraform(Builder::Brush::Raise, {16.0f, 16.0f}, 1.0f, 0.1f);
    EXPECT_TRUE(plan.Ok());
    EXPECT_GT(plan.cost, 0);
    ExpectSamePlan(builder.Terraform(Builder::Brush::Raise, {16.0f, 16.0f}, 1.0f, 0.1f), plan);
    EXPECT_GT(world.VertexHeight(8, 8), 0.0f);
}

TEST(Builder, SmallZooConstructionUpdatesEnclosureStatisticsEndToEnd) {
    Catalog catalog = ShippedCatalog();
    World world(16, 16);
    ZooObjects objects;
    Enclosures enclosures;
    enclosures.Rebuild(world, objects, catalog);
    Builder builder(world, objects, enclosures, catalog);

    const int fenceKind = catalog.fences[1].kind;
    const std::vector<EdgeRef> perimeter = EdgesAround({4, 4}, {10, 10});
    ASSERT_TRUE(builder.PlaceFence(perimeter, fenceKind).Ok());
    ASSERT_EQ(enclosures.All().size(), 1U);
    const std::vector<TileCoord> paddock = TilesInRect({4, 4}, {10, 10});
    int savanna = -1;
    for (std::size_t i = 0; i < catalog.terrains.size(); ++i) {
        if (catalog.terrains[i].terrain == Terrain::Savanna) {
            savanna = static_cast<int>(i);
            break;
        }
    }
    ASSERT_GE(savanna, 0);
    EXPECT_TRUE(builder.PaintTerrain(paddock, static_cast<Terrain>(savanna)).Ok());

    const std::array<TileCoord, 1> pond{{{8, 8}}};
    ASSERT_TRUE(builder.PlaceWater(pond, 1.0f).Ok());
    int shelterDef = -1;
    int feederDef = -1;
    int treeDef = -1;
    for (std::size_t i = 0; i < catalog.buildings.size(); ++i) {
        if (catalog.buildings[i].role == BuildingRole::AnimalShelter) {
            shelterDef = static_cast<int>(i);
            break;
        }
    }
    for (std::size_t i = 0; i < catalog.props.size(); ++i) {
        if (feederDef < 0 && catalog.props[i].role == PropRole::Feeder) {
            feederDef = static_cast<int>(i);
        }
        if (treeDef < 0 && catalog.props[i].role == PropRole::Tree) {
            treeDef = static_cast<int>(i);
        }
    }
    ASSERT_GE(shelterDef, 0);
    ASSERT_GE(feederDef, 0);
    ASSERT_GE(treeDef, 0);
    const BuildPlan shelter = builder.PlaceBuilding(shelterDef, {5, 5}, Dir::South);
    ASSERT_TRUE(shelter.Ok()) << Describe(shelter.error);
    const BuildPlan feeder = builder.PlaceProp(feederDef, World::TileCenter({8, 6}));
    ASSERT_TRUE(feeder.Ok()) << Describe(feeder.error);
    const BuildPlan tree = builder.PlaceProp(treeDef, World::TileCenter({9, 8}));
    ASSERT_TRUE(tree.Ok()) << Describe(tree.error);
    const std::array<TileCoord, 1> path{{{3, 7}}};
    ASSERT_TRUE(builder.PlacePath(path, PathKind::Asphalt).Ok());

    ASSERT_EQ(enclosures.All().size(), 1U);
    const Enclosure& enclosure = enclosures.All().front();
    EXPECT_EQ(enclosure.stats.tiles, 49);
    EXPECT_FLOAT_EQ(enclosure.stats.waterShare, 1.0f / 49.0f);
    EXPECT_EQ(enclosure.stats.feeders, 1);
    EXPECT_EQ(enclosure.stats.shelterCapacity,
              catalog.buildings[static_cast<std::size_t>(shelterDef)].capacity);
    EXPECT_GT(enclosure.stats.vegetationM2, 0.0f);
    EXPECT_GT(enclosure.stats.viewingTiles, 0);
    EXPECT_GT(enclosure.stats.visibility, 0.0f);
}

TEST(Builder, HeldFiguresComeUpToDateOnReleaseAndRegionsNeverGoStale) {
    Catalog catalog = ShippedCatalog();
    World world(16, 16);
    ZooObjects objects;
    Enclosures enclosures;
    enclosures.Rebuild(world, objects, catalog);
    Builder builder(world, objects, enclosures, catalog);
    int treeDef = -1;
    for (std::size_t i = 0; i < catalog.props.size(); ++i) {
        if (catalog.props[i].role == PropRole::Tree) {
            treeDef = static_cast<int>(i);
            break;
        }
    }
    ASSERT_GE(treeDef, 0);

    builder.Hold();
    // A fence changes regions: it is seen at once, held or not.
    ASSERT_TRUE(builder.PlaceFence(EdgesAround({4, 4}, {10, 10}), catalog.fences[1].kind).Ok());
    ASSERT_EQ(enclosures.All().size(), 1U);
    EXPECT_TRUE(enclosures.At(TileCoord{7, 7}));
    // A tree changes only the figures: they wait for the release.
    ASSERT_TRUE(builder.PlaceProp(treeDef, World::TileCenter({9, 8})).Ok());
    EXPECT_FLOAT_EQ(enclosures.All().front().stats.vegetationM2, 0.0f);
    builder.Release();
    EXPECT_GT(enclosures.All().front().stats.vegetationM2, 0.0f);

    // After the release every prop counts at once again.
    const float one = enclosures.All().front().stats.vegetationM2;
    ASSERT_TRUE(builder.PlaceProp(treeDef, World::TileCenter({6, 6})).Ok());
    EXPECT_GT(enclosures.All().front().stats.vegetationM2, one);
}

TEST(Builder, FenceChecksReportBoundsPropertyPerimeterBuildingAndDuplicateErrors) {
    Catalog catalog = ShippedCatalog();
    World world(16, 16);
    ZooObjects objects;
    Enclosures enclosures;
    enclosures.Rebuild(world, objects, catalog);
    Builder builder(world, objects, enclosures, catalog);
    const int kind = catalog.fences.front().kind;
    EXPECT_EQ(builder.CheckFence({-1, 0, true}, kind, false), BuildError::OutOfBounds);

    WorldGenParams params;
    params.width = 24;
    params.depth = 24;
    params.margin = 4;
    params.entranceWidth = 2;
    params.relief = 0.0f;
    World generated = World::Generate(params);
    ZooObjects generatedObjects;
    Enclosures generatedEnclosures;
    generatedEnclosures.Rebuild(generated, generatedObjects, catalog);
    Builder generatedBuilder(generated, generatedObjects, generatedEnclosures, catalog);
    EXPECT_EQ(generatedBuilder.CheckFence({0, 1, true}, kind, false), BuildError::NotProperty);

    const EdgeRef perimeter{5, 5, true};
    world.SetFence(perimeter, {kPerimeterFence, false, 1.0f});
    EXPECT_EQ(builder.CheckFence(perimeter, kind, false), BuildError::Perimeter);
    world.SetFence(perimeter, {});
    world.SetBuilding({5, 5}, BuildingId{10});
    world.SetBuilding({6, 5}, BuildingId{10});
    EXPECT_EQ(builder.CheckFence({6, 5, false}, kind, false), BuildError::ThroughBuilding);
    world.SetBuilding({5, 5}, {});
    world.SetBuilding({6, 5}, {});
    EXPECT_EQ(builder.CheckFence(perimeter, 254, false), BuildError::UnknownItem);
    EXPECT_TRUE(builder.PlaceFence(std::span<const EdgeRef>(&perimeter, 1), kind).Ok());
    EXPECT_EQ(builder.CheckFence(perimeter, kind, false), BuildError::FenceExists);
    EXPECT_EQ(builder.PlanRemoveFence(std::span<const EdgeRef>(&perimeter, 1)).count, 1);
    const EdgeRef emptyEdge{4, 4, true};
    EXPECT_EQ(builder.PlanRemoveFence(std::span<const EdgeRef>(&emptyEdge, 1)).error, BuildError::NoFence);
}

TEST(Builder, PathWaterBuildingAndPropChecksReportTheirSpecificBlockers) {
    Catalog catalog = ShippedCatalog();
    World world(16, 16);
    ZooObjects objects;
    Enclosures enclosures;
    enclosures.Rebuild(world, objects, catalog);
    Builder builder(world, objects, enclosures, catalog);
    const int tree = FindProp(catalog, PropRole::Tree, Placement::Anywhere);
    ASSERT_GE(tree, 0);
    const int publicBuilding = FindBuilding(catalog, BuildingRole::FoodStall, Placement::Public);
    const int enclosureBuilding = FindBuilding(catalog, BuildingRole::AnimalShelter, Placement::Enclosure);
    const int publicProp = FindProp(catalog, PropRole::Bench, Placement::Public);
    const int enclosureProp = FindProp(catalog, PropRole::Feeder, Placement::Enclosure);
    ASSERT_GE(publicBuilding, 0);
    ASSERT_GE(enclosureBuilding, 0);
    ASSERT_GE(publicProp, 0);
    ASSERT_GE(enclosureProp, 0);

    EXPECT_EQ(builder.CheckPath({16, 3}, PathKind::Asphalt), BuildError::OutOfBounds);
    EXPECT_EQ(builder.CheckWater({-1, 3}), BuildError::OutOfBounds);
    const TileCoord occupied{3, 3};
    world.SetBuilding(occupied, BuildingId{7});
    EXPECT_EQ(builder.CheckPath(occupied, PathKind::Asphalt), BuildError::Occupied);
    EXPECT_EQ(builder.CheckWater(occupied), BuildError::Occupied);
    world.SetBuilding(occupied, {});

    const TileCoord wet{4, 4};
    world.SetWater(wet, 1.0f, 0.8f);
    EXPECT_EQ(builder.CheckPath(wet, PathKind::Asphalt), BuildError::Water);
    EXPECT_EQ(builder.CheckWater(wet), BuildError::NothingToDo);
    world.ClearWater(wet);

    const TileCoord pathTile{5, 5};
    world.SetPath(pathTile, PathKind::Asphalt);
    EXPECT_EQ(builder.CheckPath(pathTile, PathKind::Asphalt), BuildError::NothingToDo);
    EXPECT_EQ(builder.CheckWater(pathTile), BuildError::PathInTheWay);
    EXPECT_EQ(builder.CheckPath({6, 5}, PathKind::None), BuildError::NothingToDo);
    EXPECT_EQ(builder.CheckPath(pathTile, PathKind::None), BuildError::None);

    world.SetVertexHeight(8, 8, 2.0f);
    EXPECT_EQ(builder.CheckPath({7, 7}, PathKind::Asphalt), BuildError::TooSteep);
    world.SetVertexHeight(8, 8, 0.0f);
    Prop blocking;
    blocking.def = tree;
    blocking.position = World::TileCenter({9, 9});
    objects.AddProp(blocking);
    EXPECT_EQ(builder.CheckPath({9, 9}, PathKind::Asphalt), BuildError::Blocked);
    EXPECT_EQ(builder.CheckWater({9, 9}), BuildError::Blocked);
    EXPECT_EQ(builder.CheckProp(tree, World::TileCenter({9, 9})), BuildError::Blocked);

    EXPECT_EQ(builder.CheckBuilding(-1, {2, 2}, Dir::South), BuildError::UnknownItem);
    EXPECT_EQ(builder.CheckBuilding(publicBuilding, {15, 15}, Dir::South), BuildError::OutOfBounds);
    EXPECT_EQ(builder.CheckBuilding(enclosureBuilding, {2, 2}, Dir::South), BuildError::NeedsEnclosure);
    EXPECT_EQ(builder.CheckBuilding(publicBuilding, {2, 2}, Dir::South), BuildError::None);
    EXPECT_EQ(builder.CheckProp(-1, World::TileCenter({2, 2})), BuildError::UnknownItem);
    EXPECT_EQ(builder.CheckProp(publicProp, World::TileCenter({2, 2})), BuildError::None);

    WorldGenParams params;
    params.width = 24;
    params.depth = 24;
    params.margin = 4;
    params.entranceWidth = 2;
    params.relief = 0.0f;
    World generated = World::Generate(params);
    ZooObjects generatedObjects;
    Enclosures generatedEnclosures;
    generatedEnclosures.Rebuild(generated, generatedObjects, catalog);
    Builder generatedBuilder(generated, generatedObjects, generatedEnclosures, catalog);
    EXPECT_EQ(generatedBuilder.CheckPath({0, 0}, PathKind::Asphalt), BuildError::NotProperty);
    EXPECT_EQ(generatedBuilder.CheckWater({0, 0}), BuildError::NotProperty);
    EXPECT_EQ(generatedBuilder.CheckBuilding(publicBuilding, {0, 0}, Dir::South), BuildError::NotProperty);
    EXPECT_EQ(generatedBuilder.CheckProp(publicProp, World::TileCenter({0, 0})), BuildError::NotProperty);
}

TEST(Builder, BuildingAndPropChecksUseFootprintsAndPlacementRegions) {
    Catalog catalog = ShippedCatalog();
    const int publicBuilding = FindBuilding(catalog, BuildingRole::FoodStall, Placement::Public);
    const int shelter = FindBuilding(catalog, BuildingRole::AnimalShelter, Placement::Enclosure);
    const int tree = FindProp(catalog, PropRole::Tree, Placement::Anywhere);
    const int publicProp = FindProp(catalog, PropRole::Bench, Placement::Public);
    const int feeder = FindProp(catalog, PropRole::Feeder, Placement::Enclosure);
    ASSERT_GE(publicBuilding, 0);
    ASSERT_GE(shelter, 0);
    ASSERT_GE(tree, 0);
    ASSERT_GE(publicProp, 0);
    ASSERT_GE(feeder, 0);

    World world(16, 16);
    ZooObjects objects;
    Enclosures enclosures;
    enclosures.Rebuild(world, objects, catalog);
    Builder builder(world, objects, enclosures, catalog);
    const TileCoord origin{2, 2};
    EXPECT_EQ(builder.CheckBuilding(publicBuilding, origin, Dir::South), BuildError::None);
    world.SetBuilding(origin, BuildingId{20});
    EXPECT_EQ(builder.CheckBuilding(publicBuilding, origin, Dir::South), BuildError::Occupied);
    world.SetBuilding(origin, {});
    world.SetWater(origin, 1.0f, 0.8f);
    EXPECT_EQ(builder.CheckBuilding(publicBuilding, origin, Dir::South), BuildError::Water);
    world.ClearWater(origin);

    Prop blocking;
    blocking.def = tree;
    blocking.position = World::TileCenter(origin);
    const PropId blocker = objects.AddProp(blocking);
    EXPECT_EQ(builder.CheckBuilding(publicBuilding, origin, Dir::South), BuildError::Blocked);
    objects.RemoveProp(blocker);

    const BuildingDef& publicDef = catalog.buildings[static_cast<std::size_t>(publicBuilding)];
    const TileCoord footprint = FootprintSize(publicDef, Dir::South);
    ASSERT_GT(footprint.x, 1);
    world.SetFence(EdgeOf(origin, Dir::East),
                   {static_cast<std::uint8_t>(catalog.fences.front().kind), false, 1.0f});
    EXPECT_EQ(builder.CheckBuilding(publicBuilding, origin, Dir::South), BuildError::FenceInTheWay);
    world.SetFence(EdgeOf(origin, Dir::East), {});
    world.SetVertexHeight(origin.x + 1, origin.z + 1, 3.0f);
    EXPECT_EQ(builder.CheckBuilding(publicBuilding, origin, Dir::South), BuildError::TooSteep);
    world.SetVertexHeight(origin.x + 1, origin.z + 1, 0.0f);

    const Vec2 position = World::TileCenter({12, 12});
    EXPECT_EQ(builder.CheckProp(publicProp, position), BuildError::None);
    EXPECT_EQ(builder.CheckProp(tree, {-1.0f, 1.0f}), BuildError::OutOfBounds);
    world.SetBuilding({12, 12}, BuildingId{21});
    EXPECT_EQ(builder.CheckProp(publicProp, position), BuildError::Occupied);
    world.SetBuilding({12, 12}, {});
    world.SetWater({12, 12}, 1.0f, 0.8f);
    EXPECT_EQ(builder.CheckProp(publicProp, position), BuildError::Water);
    world.ClearWater({12, 12});
    world.SetPath({12, 12}, PathKind::Asphalt);
    EXPECT_EQ(builder.CheckProp(tree, position), BuildError::PathInTheWay);
    world.SetPath({12, 12}, PathKind::None);
    Prop nearby;
    nearby.def = tree;
    nearby.position = position;
    const PropId nearbyId = objects.AddProp(nearby);
    EXPECT_EQ(builder.CheckProp(tree, position), BuildError::Blocked);
    objects.RemoveProp(nearbyId);

    for (const EdgeRef edge : EdgesAround({4, 4}, {10, 10})) {
        world.SetFence(edge, {static_cast<std::uint8_t>(catalog.fences.front().kind), false, 1.0f});
    }
    enclosures.Rebuild(world, objects, catalog);
    EXPECT_EQ(builder.CheckBuilding(publicBuilding, {5, 5}, Dir::South), BuildError::NeedsPublicGround);
    EXPECT_EQ(builder.CheckBuilding(shelter, {5, 5}, Dir::South), BuildError::None);
    EXPECT_EQ(builder.CheckProp(publicProp, World::TileCenter({5, 5})), BuildError::NeedsPublicGround);
    EXPECT_EQ(builder.CheckProp(feeder, World::TileCenter({2, 2})), BuildError::NeedsEnclosure);
}

}  // namespace
}  // namespace zoo
