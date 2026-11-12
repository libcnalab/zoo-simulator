// SPDX-License-Identifier: MIT
#include "zoo/build/Enclosures.hpp"
#include "zoo/build/EnclosuresJson.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <string>

namespace zoo {
namespace {

Catalog ShippedCatalog() {
    return Catalog::LoadDirectory(std::string(ZOO_SOURCE_DIR) + "/data/construction");
}

void FenceBox(World& world, TileCoord min, TileCoord max, int kind, bool gate = false) {
    for (int x = min.x; x <= max.x; ++x) {
        world.SetFence({x, min.z, true}, {static_cast<std::uint8_t>(kind), false, 0.8f});
        world.SetFence({x, max.z + 1, true}, {static_cast<std::uint8_t>(kind), gate && x == min.x, 0.9f});
    }
    for (int z = min.z; z <= max.z; ++z) {
        world.SetFence({min.x, z, false}, {static_cast<std::uint8_t>(kind), false, 0.7f});
        world.SetFence({max.x + 1, z, false}, {static_cast<std::uint8_t>(kind), false, 0.6f});
    }
}

TEST(Enclosures, GeneratedRegionsAndEmptyFlatMap) {
    Catalog catalog = ShippedCatalog();
    WorldGenParams params;
    params.width = 32;
    params.depth = 32;
    params.margin = 4;
    params.entranceWidth = 2;
    params.relief = 0.0f;
    World world = World::Generate(params);
    ZooObjects objects;
    Enclosures enclosures;
    enclosures.Rebuild(world, objects, catalog);
    EXPECT_TRUE(enclosures.All().empty());
    EXPECT_EQ(enclosures.RegionAt(world.EntranceTile()), Region::Public);
    EXPECT_EQ(enclosures.RegionAt({0, 0}), Region::Outside);

    World flat(16, 16);
    enclosures.Rebuild(flat, objects, catalog);
    EXPECT_TRUE(enclosures.All().empty());
    EXPECT_EQ(enclosures.RegionAt({8, 8}), Region::Public);
}

TEST(Enclosures, FenceBoxStatisticsAndIdentity) {
    Catalog catalog = ShippedCatalog();
    World world(16, 16);
    ZooObjects objects;
    Enclosures enclosures;
    const int kind = catalog.fences.front().kind;
    FenceBox(world, {5, 5}, {8, 8}, kind, true);
    const TileCoord entrance = world.EntranceTile();
    world.SetPath(entrance, PathKind::Asphalt);
    world.SetTerrain({5, 5}, Terrain::Dirt);
    world.SetWater({6, 5}, 1.0f, 1.2f);
    enclosures.Rebuild(world, objects, catalog);
    ASSERT_EQ(enclosures.All().size(), 1U);
    const Enclosure& enclosure = enclosures.All().front();
    EXPECT_EQ(enclosure.anchor, (TileCoord{5, 5}));
    EXPECT_EQ(enclosure.min, (TileCoord{5, 5}));
    EXPECT_EQ(enclosure.max, (TileCoord{8, 8}));
    EXPECT_EQ(enclosure.stats.tiles, 16);
    EXPECT_FLOAT_EQ(enclosure.stats.areaM2, 64.0f);
    EXPECT_FLOAT_EQ(enclosure.stats.waterShare, 1.0f / 16.0f);
    EXPECT_TRUE(enclosure.stats.deepWater);
    EXPECT_EQ(enclosure.stats.boundaryEdges, 16);
    EXPECT_EQ(enclosure.stats.gates, 1);
    EXPECT_EQ(enclosure.gates.size(), 1U);
    EXPECT_FLOAT_EQ(enclosure.stats.minFenceCondition, 0.6f);
    EXPECT_FLOAT_EQ(enclosure.stats.terrainShare[static_cast<std::size_t>(Terrain::Dirt)], 1.0f / 16.0f);
    const EnclosureId oldId = enclosure.id;
    ASSERT_TRUE(enclosures.Rename(oldId, "Lion home"));
    const std::uint32_t revision = enclosures.Revision();
    enclosures.Rebuild(world, objects, catalog);
    EXPECT_EQ(enclosures.Revision(), revision);
    EXPECT_EQ(enclosures.Find(oldId)->name, "Lion home");

    const nlohmann::json saved = ToJson(enclosures);
    Enclosures restored;
    std::string error;
    ASSERT_TRUE(EnclosuresFromJson(saved, restored, &error)) << error;
    restored.Rebuild(world, objects, catalog);
    ASSERT_EQ(restored.All().size(), 1U);
    EXPECT_EQ(restored.All().front().id, oldId);
    EXPECT_EQ(restored.All().front().name, "Lion home");
    EXPECT_EQ(ToJson(restored), saved);
}

TEST(Enclosures, GapOpensBoxAndDividerIdentitySurvivesSplitAndMerge) {
    Catalog catalog = ShippedCatalog();
    World world(16, 16);
    ZooObjects objects;
    Enclosures enclosures;
    const int kind = catalog.fences.front().kind;
    FenceBox(world, {4, 4}, {10, 10}, kind);
    world.SetFence({6, 4, true}, {});
    enclosures.Rebuild(world, objects, catalog);
    EXPECT_TRUE(enclosures.All().empty());

    world.SetFence({6, 4, true}, {static_cast<std::uint8_t>(kind), false, 1.0f});
    enclosures.Rebuild(world, objects, catalog);
    ASSERT_EQ(enclosures.All().size(), 1U);
    const Enclosure original = enclosures.All().front();
    for (int z = 4; z <= 10; ++z) {
        world.SetFence({7, z, false}, {static_cast<std::uint8_t>(kind), false, 1.0f});
    }
    enclosures.Rebuild(world, objects, catalog);
    ASSERT_EQ(enclosures.All().size(), 2U);
    EXPECT_EQ(enclosures.At(TileCoord{4, 4}), original.id);
    EXPECT_EQ(enclosures.Find(original.id)->name, original.name);
    const EnclosureId second = enclosures.At(TileCoord{8, 4});
    EXPECT_NE(second, original.id);
    for (int z = 4; z <= 10; ++z) {
        world.SetFence({7, z, false}, {});
    }
    enclosures.Rebuild(world, objects, catalog);
    ASSERT_EQ(enclosures.All().size(), 1U);
    EXPECT_EQ(enclosures.All().front().id, original.id);

    world.SetFence({4, 4, true}, {});
    enclosures.Rebuild(world, objects, catalog);
    EXPECT_TRUE(enclosures.All().empty());
    FenceBox(world, {4, 4}, {10, 10}, kind);
    enclosures.Rebuild(world, objects, catalog);
    ASSERT_EQ(enclosures.All().size(), 1U);
    EXPECT_GT(enclosures.All().front().id.value, second.value);
}

TEST(Enclosures, BoundaryWallIsIncludedInWeakestBarrierAndRenameValidation) {
    Catalog catalog = ShippedCatalog();
    WorldGenParams params;
    params.width = 24;
    params.depth = 24;
    params.margin = 3;
    params.entranceWidth = 2;
    params.relief = 0.0f;
    World world = World::Generate(params);
    const TileCoord min = world.PropertyMin();
    const int kind = catalog.fences.front().kind;
    const TileCoord boxMin{min.x + 3, min.z};
    const TileCoord boxMax{min.x + 6, min.z + 3};
    for (int x = boxMin.x; x <= boxMax.x; ++x) {
        world.SetFence({x, boxMax.z + 1, true}, {static_cast<std::uint8_t>(kind), false, 1.0f});
    }
    for (int z = boxMin.z; z <= boxMax.z; ++z) {
        world.SetFence({boxMin.x, z, false}, {static_cast<std::uint8_t>(kind), false, 1.0f});
        world.SetFence({boxMax.x + 1, z, false}, {static_cast<std::uint8_t>(kind), false, 1.0f});
    }
    ZooObjects objects;
    Enclosures values;
    values.Rebuild(world, objects, catalog);
    ASSERT_FALSE(values.All().empty());
    EXPECT_EQ(values.All().front().stats.minFenceHeightM,
              std::min(catalog.fences.front().heightM, kPerimeterHeightM));
    EXPECT_FALSE(values.Rename(EnclosureId{999}, "Unknown"));
    EXPECT_FALSE(values.Rename(values.All().front().id, ""));
}

TEST(Enclosures, ObjectRolesAndPublicViewingStatsUseTheirOwningTiles) {
    Catalog catalog = ShippedCatalog();
    World world(16, 16);
    ZooObjects objects;
    Enclosures enclosures;
    const int fenceKind = catalog.fences[1].kind;
    FenceBox(world, {5, 5}, {9, 9}, fenceKind);
    world.SetPath({4, 7}, PathKind::Asphalt);

    auto addProp = [&](PropRole role, Vec2 position) {
        for (std::size_t i = 0; i < catalog.props.size(); ++i) {
            if (catalog.props[i].role == role) {
                Prop prop;
                prop.def = static_cast<int>(i);
                prop.position = position;
                return objects.AddProp(prop);
            }
        }
        return PropId{};
    };
    const PropId tree = addProp(PropRole::Tree, World::TileCenter({6, 6}));
    const PropId feeder = addProp(PropRole::Feeder, World::TileCenter({7, 6}));
    const PropId trough = addProp(PropRole::WaterTrough, World::TileCenter({8, 6}));
    const PropId enrichment = addProp(PropRole::Enrichment, World::TileCenter({6, 8}));
    ASSERT_TRUE(tree);
    ASSERT_TRUE(feeder);
    ASSERT_TRUE(trough);
    ASSERT_TRUE(enrichment);

    int openShelterDef = -1;
    int closedShelterDef = -1;
    for (std::size_t i = 0; i < catalog.buildings.size(); ++i) {
        if (catalog.buildings[i].role != BuildingRole::AnimalShelter) {
            continue;
        }
        if (openShelterDef < 0 && (catalog.buildings[i].heated || catalog.buildings[i].cooled)) {
            openShelterDef = static_cast<int>(i);
        }
        if (closedShelterDef < 0) {
            closedShelterDef = static_cast<int>(i);
        }
    }
    ASSERT_GE(openShelterDef, 0);
    ASSERT_GE(closedShelterDef, 0);
    Building open;
    open.def = openShelterDef;
    open.origin = {6, 6};
    open.facing = Dir::North;
    open.open = true;
    const BuildingId openId = objects.AddBuilding(open);
    Building closed = open;
    closed.def = closedShelterDef;
    closed.origin = {8, 8};
    closed.open = false;
    const BuildingId closedId = objects.AddBuilding(closed);

    enclosures.Rebuild(world, objects, catalog);
    ASSERT_EQ(enclosures.All().size(), 1U);
    const Enclosure& enclosure = enclosures.All().front();
    EXPECT_EQ(enclosure.feeders, (std::vector<PropId>{feeder}));
    EXPECT_EQ(enclosure.troughs, (std::vector<PropId>{trough}));
    EXPECT_EQ(enclosure.enrichments, (std::vector<PropId>{enrichment}));
    EXPECT_EQ(enclosure.stats.feeders, 1);
    EXPECT_EQ(enclosure.stats.troughs, 1);
    float expectedShade = 0.0f;
    float expectedEnrichment = 0.0f;
    for (const Prop& prop : objects.Props()) {
        const PropDef& propDef = catalog.props[static_cast<std::size_t>(prop.def)];
        expectedShade += propDef.shadeM2;
        expectedEnrichment += propDef.enrichment;
    }
    EXPECT_FLOAT_EQ(enclosure.stats.shadeM2, expectedShade);
    EXPECT_FLOAT_EQ(enclosure.stats.enrichment, expectedEnrichment);
    EXPECT_FLOAT_EQ(
        enclosure.stats.vegetationM2,
        catalog.props[static_cast<std::size_t>(objects.FindProp(tree)->def)].vegetationM2 +
            catalog.props[static_cast<std::size_t>(objects.FindProp(feeder)->def)].vegetationM2 +
            catalog.props[static_cast<std::size_t>(objects.FindProp(trough)->def)].vegetationM2 +
            catalog.props[static_cast<std::size_t>(objects.FindProp(enrichment)->def)].vegetationM2);
    EXPECT_FLOAT_EQ(enclosure.stats.vegetationDensity, std::min(1.0f, enclosure.stats.vegetationM2 / 100.0f));
    const BuildingDef& shelter = catalog.buildings[static_cast<std::size_t>(openShelterDef)];
    EXPECT_EQ(enclosure.stats.shelterCapacity, shelter.capacity);
    EXPECT_EQ(enclosure.stats.heatedShelter, shelter.heated);
    EXPECT_EQ(enclosure.stats.cooledShelter, shelter.cooled);
    EXPECT_EQ(enclosure.shelters, (std::vector<BuildingId>{openId, closedId}));
    EXPECT_GT(enclosure.stats.viewingTiles, 0);
    EXPECT_GT(enclosure.stats.visibility, 0.0f);

    const auto concrete = std::find_if(catalog.fences.begin(), catalog.fences.end(),
                                       [](const FenceDef& fence) { return fence.visibility == 0.0f; });
    ASSERT_NE(concrete, catalog.fences.end());
    world.SetFence({5, 7, false}, {static_cast<std::uint8_t>(concrete->kind), false, 1.0f});
    enclosures.Rebuild(world, objects, catalog);
    ASSERT_EQ(enclosures.All().size(), 1U);
    EXPECT_EQ(enclosures.All().front().stats.viewingTiles, 0);
    EXPECT_FLOAT_EQ(enclosures.All().front().stats.visibility, 0.0f);
}

TEST(Enclosures, RejectsMalformedIdentityJson) {
    Enclosures out;
    std::string error;
    EXPECT_FALSE(EnclosuresFromJson(
        {{"nextId", 2}, {"enclosures", {{"id", 2}, {"name", "x"}, {"anchor", {0, 0}}}}}, out, &error));
    EXPECT_FALSE(error.empty());
    EXPECT_FALSE(EnclosuresFromJson(
        {{"nextId", 2}, {"enclosures", {{"id", 1}, {"name", ""}, {"anchor", {0, 0}}}}}, out));
    EXPECT_FALSE(EnclosuresFromJson(
        {{"nextId", 2}, {"enclosures", {{"id", 1}, {"name", "x"}, {"anchor", {0}}}}}, out));
}

TEST(Enclosures, RestoresFullWidthIdsAcrossRebuild) {
    Catalog catalog = ShippedCatalog();
    World world(8, 8);
    const int kind = catalog.fences.front().kind;
    FenceBox(world, {2, 2}, {5, 5}, kind);
    Enclosures loaded;
    const std::uint32_t largeId = 2'147'483'648U;
    const nlohmann::json json = {
        {"nextId", 2'147'483'649U},
        {"enclosures", {{{"id", largeId}, {"name", "Large id"}, {"anchor", {2, 2}}}}}};
    ASSERT_TRUE(EnclosuresFromJson(json, loaded));
    ZooObjects objects;
    loaded.Rebuild(world, objects, catalog);
    ASSERT_EQ(loaded.All().size(), 1U);
    EXPECT_EQ(loaded.All().front().id.value, largeId);
    EXPECT_EQ(loaded.At(TileCoord{3, 3}).value, largeId);
}

}  // namespace
}  // namespace zoo
