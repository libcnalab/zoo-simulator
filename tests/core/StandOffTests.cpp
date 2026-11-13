// SPDX-License-Identifier: MIT
#include <gtest/gtest.h>

#include <iomanip>
#include <iostream>
#include <map>
#include <set>

#include "TestData.hpp"
#include "zoo/sim/VisitStart.hpp"
#include "zoo/sim/VisitorZoo.hpp"
#include "zoo/sim/ZooSaveJson.hpp"

namespace zoo {
namespace {

class StripFixture : public testing::Test {
protected:
    Catalog catalog = testdata::GameCatalog();
    World world{20, 20};
    ZooObjects objects;
    Enclosures enclosures;
    Finance finance{Dollars(10000)};
    Builder builder{world, objects, enclosures, catalog, &finance};

    void SetUp() override { enclosures.Rebuild(world, objects, catalog); }
};

TEST_F(StripFixture, MarkRemoveCostsAndRevisions) {
    const std::vector<TileCoord> tiles{{4, 4}, {4, 4}, {5, 4}};
    const auto planned = builder.PlanStandOff(tiles);
    EXPECT_EQ(planned.count, 2);
    EXPECT_EQ(planned.cost, 2 * Builder::kStandOffPerTile);
    EXPECT_FALSE(world.At({4, 4}).standOff);
    const auto revision = world.NavRevision();
    const auto chunk = world.ChunkRevision(0, 0);
    const auto balance = finance.Balance();
    const auto placed = builder.PlaceStandOff(tiles);
    EXPECT_EQ(placed.cost, planned.cost);
    EXPECT_EQ(placed.count, planned.count);
    EXPECT_EQ(finance.Balance(), balance - planned.cost);
    EXPECT_GT(world.NavRevision(), revision);
    EXPECT_GT(world.ChunkRevision(0, 0), chunk);
    EXPECT_EQ(enclosures.RegionAt({4, 4}), Region::StandOff);
    EXPECT_FALSE(enclosures.At(TileCoord{4, 4}));
    EXPECT_TRUE(enclosures.All().empty());
    EXPECT_EQ(builder.CheckPath({4, 4}, PathKind::Gravel), BuildError::Blocked);
    EXPECT_EQ(builder.CheckWater({4, 4}), BuildError::Blocked);
    const auto removed = builder.PlaceStandOff(tiles, false);
    EXPECT_TRUE(removed.Ok());
    EXPECT_EQ(removed.cost, 0);
    EXPECT_EQ(enclosures.RegionAt({4, 4}), Region::Public);
    EXPECT_EQ(finance.Balance(), balance - planned.cost);
}

TEST_F(StripFixture, RefusesOccupiedGroundAndFunctionalProps) {
    world.SetPath({4, 4}, PathKind::Gravel);
    world.SetBuilding({5, 4}, BuildingId{1});
    world.SetWater({6, 4}, 0.0f);
    EXPECT_EQ(builder.CheckStandOff({4, 4}), BuildError::PathInTheWay);
    EXPECT_EQ(builder.CheckStandOff({5, 4}), BuildError::Occupied);
    EXPECT_EQ(builder.CheckStandOff({6, 4}), BuildError::Water);
    EXPECT_EQ(builder.CheckStandOff({-1, 4}), BuildError::OutOfBounds);
    EXPECT_NE(builder.CheckStandOff(world.EntranceTile()), BuildError::None);
    ASSERT_TRUE(builder.PlaceProp(catalog.PropIndex("bench"), World::TileCenter({7, 4})).Ok());
    EXPECT_EQ(builder.CheckStandOff({7, 4}), BuildError::Blocked);
    ASSERT_TRUE(builder.PlaceStandOff(std::vector<TileCoord>{{10, 10}}).Ok());
    for (const auto name : {"bench", "litter_bin", "hay_rack", "water_trough", "info_board", "ball"}) {
        EXPECT_NE(builder.CheckProp(catalog.PropIndex(name), World::TileCenter({10, 10})), BuildError::None)
            << name;
    }
    for (const auto name : {"savanna_shrub", "sandstone_rock", "flower_bed"}) {
        EXPECT_EQ(builder.CheckProp(catalog.PropIndex(name), World::TileCenter({10, 10})), BuildError::None)
            << name;
    }
    EXPECT_EQ(builder.CheckBuilding(catalog.BuildingIndex("small_shelter"), {10, 10}, Dir::South),
              BuildError::Blocked);
    ASSERT_TRUE(builder.PlaceFence(EdgesAround({12, 12}, {14, 14}), 2).Ok());
    EXPECT_EQ(builder.CheckStandOff({13, 13}), BuildError::NeedsPublicGround);
    EXPECT_EQ(enclosures.All().size(), 1U);  // A gateless pocket is still a real enclosure.
}

TEST_F(StripFixture, UnaffordableMarkDoesNotMutate) {
    Finance empty{0};
    Builder poor(world, objects, enclosures, catalog, &empty);
    const std::vector<TileCoord> tiles{{4, 4}, {5, 4}};
    EXPECT_EQ(poor.PlanStandOff(tiles).error, BuildError::NoMoney);
    EXPECT_EQ(poor.PlaceStandOff(tiles).error, BuildError::NoMoney);
    EXPECT_FALSE(world.At({4, 4}).standOff);
    EXPECT_FALSE(world.At({5, 4}).standOff);
}

TEST_F(StripFixture, StraightViewsAcrossOneAndTwoTiles) {
    for (const int depth : {1, 2}) {
        SCOPED_TRACE(depth);
        World map(20, 20);
        ZooObjects props;
        Enclosures homes;
        homes.Rebuild(map, props, catalog);
        Builder build(map, props, homes, catalog);
        std::vector<TileCoord> strip;
        for (int z = 8; z < 8 + depth; ++z) {
            for (int x = 5; x <= 7; ++x) {
                strip.push_back({x, z});
            }
        }
        ASSERT_TRUE(build.PlaceStandOff(strip).Ok());
        ASSERT_TRUE(build.PlaceFence(EdgesAround({5, 5}, {7, 7}), 3).Ok());
        ASSERT_TRUE(build.PlaceFence(EdgesAround({5, 8}, {7, 7 + depth}), 1).Ok());
        // Restore containment on the shared edge after enclosing the planting pocket.
        ASSERT_TRUE(build.PlaceFence(EdgesAlong({5, 8}, {8, 8}), 3).Ok());
        const TileCoord path{6, 8 + depth};
        ASSERT_TRUE(build.PlacePath(std::vector<TileCoord>{path}, PathKind::Gravel).Ok());
        ASSERT_EQ(homes.All().size(), 1U);
        const auto home = homes.All().front();
        ASSERT_EQ(home.viewing.size(), 1U);
        EXPECT_EQ(home.viewing.front().tile, path);
        EXPECT_EQ(home.viewing.front().side, Dir::North);
        EXPECT_FLOAT_EQ(home.viewing.front().standOffM, static_cast<float>(depth) * kTileSize);
        EXPECT_FLOAT_EQ(home.viewing.front().visibility, 0.82f * 0.75f);
        EXPECT_FLOAT_EQ(home.stats.visibility, 0.82f * 0.75f);
        EXPECT_EQ(home.stats.viewingTiles, 1);
        EXPECT_FLOAT_EQ(home.stats.minFenceHeightM, 3.5f);
        EXPECT_EQ(home.stats.minFenceStrength, 3);
        EXPECT_EQ(map.FenceAt(home.weakestEdge).kind, 3);
        build.PlacePath(std::vector<TileCoord>{path}, PathKind::None);
        EXPECT_TRUE(homes.All().front().viewing.empty());
        build.PlacePath(std::vector<TileCoord>{{8, 8 + depth}}, PathKind::Gravel);
        EXPECT_TRUE(homes.All().front().viewing.empty());  // Diagonal is not a corridor.
        build.PlacePath(std::vector<TileCoord>{path}, PathKind::Gravel);
        build.RemoveFence(std::vector<EdgeRef>{EdgeOf(path, Dir::North)});
        EXPECT_TRUE(homes.All().front().viewing.empty());
    }
}

TEST(StandOff, SaveRoundTripAndLegacySave) {
    ZooConfig config;
    config.world.width = 32;
    config.world.depth = 32;
    config.world.relief = 0.0f;
    Zoo zoo(testdata::GameCatalog(), testdata::GameSpecies(), config);
    const std::vector<TileCoord> strip{{10, 10}, {11, 10}};
    ASSERT_TRUE(zoo.FreeBuilder().PlaceStandOff(strip).Ok());
    auto document = ZooToJson(zoo);
    std::string error;
    const auto loaded = ZooFromJson(document, zoo.Definitions(), zoo.SpeciesList(), &error);
    ASSERT_NE(loaded, nullptr) << error;
    EXPECT_EQ(loaded->Map(), zoo.Map());
    EXPECT_TRUE(loaded->Habitats().All().empty());
    EXPECT_EQ(loaded->Habitats().RegionAt({10, 10}), Region::StandOff);
    document["zoo"].erase("stand_off");
    EXPECT_EQ(ZooFromJson(document, zoo.Definitions(), zoo.SpeciesList(), &error), nullptr);
    document["version"] = 1;
    const auto legacy = ZooFromJson(document, zoo.Definitions(), zoo.SpeciesList(), &error);
    ASSERT_NE(legacy, nullptr) << error;
    EXPECT_FALSE(legacy->Map().At({10, 10}).standOff);
    EXPECT_EQ(legacy->Habitats().RegionAt({10, 10}), Region::Public);
    document["zoo"]["stand_off"] = nlohmann::json::array({{-1, 10}});
    EXPECT_EQ(ZooFromJson(document, zoo.Definitions(), zoo.SpeciesList(), &error), nullptr);
}

TEST(StandOff, CannotReserveAnAnimalsOccupiedHome) {
    ZooConfig config;
    config.sandbox = true;
    config.world.width = 32;
    config.world.depth = 32;
    config.world.relief = 0.0f;
    Zoo zoo(testdata::GameCatalog(), testdata::GameSpecies(), config);
    auto builder = zoo.FreeBuilder();
    ASSERT_TRUE(builder.PlaceFence(EdgesAround({10, 10}, {16, 16}), 2).Ok());
    const auto bought =
        zoo.BuyAnimal(zoo.SpeciesList().IndexOf("meerkat"), Sex::Female, World::TileCenter({13, 13}));
    ASSERT_TRUE(bought.Ok());
    EXPECT_EQ(builder.PlaceStandOff(std::vector<TileCoord>{{13, 13}}).error, BuildError::NeedsPublicGround);
    EXPECT_FALSE(zoo.Map().At({13, 13}).standOff);
    EXPECT_TRUE(zoo.Habitats().At(TileCoord{13, 13}));
}

TEST(StandOff, ReservingGroundClearsExistingLitterOnTheNextStep) {
    ZooConfig config;
    config.world.width = 32;
    config.world.depth = 32;
    config.world.relief = 0.0f;
    Zoo original(testdata::GameCatalog(), testdata::GameSpecies(), config);
    auto document = ZooToJson(original);
    const Vec2 reserved = World::TileCenter({10, 10});
    const Vec2 publicGround = World::TileCenter({12, 12});
    document["zoo"]["visitors"]["litter"] =
        nlohmann::json::array({{reserved.x, reserved.z, 0}, {publicGround.x, publicGround.z, 0}});
    auto zoo = ZooFromJson(document, original.Definitions(), original.SpeciesList());
    ASSERT_NE(zoo, nullptr);
    zoo->Step();
    ASSERT_EQ(zoo->Guests().LitterOnGround().size(), 2U);
    ASSERT_TRUE(zoo->FreeBuilder().PlaceStandOff(std::vector<TileCoord>{{10, 10}}).Ok());
    zoo->Step();
    ASSERT_EQ(zoo->Guests().LitterOnGround().size(), 1U);
    EXPECT_EQ(zoo->Guests().LitterOnGround().front().position, publicGround);
}

TEST(StandOff, DISABLED_RecordHabitatFigures) {
    for (const std::uint64_t seed : {1ULL, 2ULL}) {
        Zoo zoo(testdata::GameCatalog(), testdata::GameSpecies(), VisitorZooConfig(seed));
        const auto report = BuildVisitorZoo(zoo);
        for (const auto& problem : report.problems) {
            std::cout << "PROBLEM " << problem << '\n';
        }
        std::set<int> seen;
        for (const Animal& animal : zoo.Fauna().All()) {
            if (!seen.insert(animal.species).second) {
                continue;
            }
            const auto& def = zoo.SpeciesList().All()[static_cast<std::size_t>(animal.species)];
            const auto& home = *zoo.Habitats().Find(animal.home);
            const auto group = zoo.Fauna().GroupOf(zoo.MakeEnv(), animal);
            const auto welfare = EvaluateHabitat(def, home.stats, group);
            std::cout << std::setprecision(9) << "FIGURE " << seed << ' ' << def.id
                      << " area=" << home.stats.areaM2
                      << " required=" << SpaceNeededM2(def, group.sameSpecies) + group.otherSpeciesSpaceM2
                      << " habitat=" << welfare.overall << " space=" << welfare.space
                      << " terrain=" << welfare.terrain << " vegetation=" << welfare.vegetation
                      << " containment=" << Containment(def, home.stats) << '\n';
        }
        FillForVisit(zoo, {.fillTicks = 600});
        std::map<int, std::pair<float, int>> totals;
        for (const Animal& animal : zoo.Fauna().All()) {
            auto& total = totals[animal.species];
            total.first += animal.welfare;
            ++total.second;
        }
        for (const auto& [index, total] : totals) {
            std::cout << "FILLED " << seed << ' '
                      << zoo.SpeciesList().All()[static_cast<std::size_t>(index)].id << ' '
                      << total.first / static_cast<float>(total.second) << '\n';
        }
    }
}

}  // namespace
}  // namespace zoo
