// SPDX-License-Identifier: MIT
#include <gtest/gtest.h>
#include <algorithm>
#include <string>
#include "zoo/base/Rng.hpp"
#include "zoo/build/Objects.hpp"
#include "zoo/build/ObjectsJson.hpp"
#include "zoo/world/World.hpp"
namespace zoo {
namespace {
Catalog Load() {
    return Catalog::LoadDirectory(std::string(ZOO_SOURCE_DIR) + "/data/construction");
}
TEST(ObjectsTests, IdsIncreaseLookupsRemovalAndRevision) {
    ZooObjects o;
    Building b;
    const auto r = o.Revision();
    const auto a = o.AddBuilding(b), d = o.AddBuilding(b);
    EXPECT_EQ(a.value, 1U);
    EXPECT_EQ(d.value, 2U);
    EXPECT_EQ(o.Revision(), r + 2U);
    const auto beforeRemove = o.Revision();
    EXPECT_TRUE(o.RemoveBuilding(a));
    EXPECT_EQ(o.Revision(), beforeRemove + 1U);
    EXPECT_FALSE(o.RemoveBuilding(a));
    EXPECT_EQ(o.AddBuilding(b).value, 3U);
    for (int i = 0; i < 300; ++i) {
        Prop p;
        p.position = {static_cast<float>(i), 0};
        o.AddProp(p);
    }
    for (std::uint32_t i = 1; i <= 300; ++i)
        EXPECT_NE(o.FindProp(PropId{i}), nullptr);
    for (std::uint32_t i = 1; i <= 300; i += 2)
        EXPECT_TRUE(o.RemoveProp(PropId{i}));
    for (std::uint32_t i = 1; i <= 300; ++i)
        EXPECT_EQ(o.FindProp(PropId{i}) != nullptr, i % 2 == 0);
    EXPECT_EQ(o.AddProp(Prop{}).value, 301U);
}
TEST(ObjectsTests, SpatialQueriesMatchBruteForceWithNegativeCoordinates) {
    ZooObjects o;
    Rng rng(12345);
    for (int i = 0; i < 500; ++i) {
        Prop p;
        p.position = {rng.Range(-300, 300), rng.Range(-300, 300)};
        o.AddProp(p);
    }
    std::vector<PropId> actual, expected;
    for (int q = 0; q < 30; ++q) {
        const Vec2 center{rng.Range(-250, 250), rng.Range(-250, 250)};
        const float radius = rng.Range(1, 80);
        o.PropsNear(center, radius, actual);
        expected.clear();
        for (const auto& p : o.Props())
            if (DistanceSquared(center, p.position) <= radius * radius)
                expected.push_back(p.id);
        EXPECT_EQ(actual, expected);
    }
    o.PropsInTiles({-10, -8}, {10, 9}, actual);
    expected.clear();
    for (const auto& p : o.Props())
        if (p.position.x >= -10 * kTileSize && p.position.x < 11 * kTileSize && p.position.z >= -8 * kTileSize &&
            p.position.z < 10 * kTileSize)
            expected.push_back(p.id);
    EXPECT_EQ(actual, expected);
}
TEST(ObjectsTests, SpatialQueriesStayCurrentAcrossIncrementalEdits) {
    ZooObjects objects;
    Prop far;
    far.position = {40.0f, 40.0f};
    const PropId farId = objects.AddProp(far);
    std::vector<PropId> found;
    objects.PropsNear({0.0f, 0.0f}, 5.0f, found);  // materialise the bucket cache
    EXPECT_TRUE(found.empty());

    Prop near;
    near.position = {2.0f, 1.0f};
    const PropId nearId = objects.AddProp(near);
    objects.PropsNear({0.0f, 0.0f}, 5.0f, found);
    EXPECT_EQ(found, std::vector<PropId>{nearId});

    // Buildings advance the shared object revision but do not alter prop buckets.
    const BuildingId building = objects.AddBuilding(Building{});
    objects.PropsNear({0.0f, 0.0f}, 5.0f, found);
    EXPECT_EQ(found, std::vector<PropId>{nearId});
    EXPECT_TRUE(objects.RemoveBuilding(building));
    objects.PropsNear({0.0f, 0.0f}, 5.0f, found);
    EXPECT_EQ(found, std::vector<PropId>{nearId});

    // Removing a prop shifts vector indices and therefore deliberately takes the rebuild path.
    EXPECT_TRUE(objects.RemoveProp(farId));
    objects.PropsNear({0.0f, 0.0f}, 5.0f, found);
    EXPECT_EQ(found, std::vector<PropId>{nearId});
}
TEST(ObjectsTests, FootprintAndDoorRotationsAreConsistent) {
    BuildingDef d;
    d.width = 3;
    d.depth = 2;
    d.doorOffset = 1;
    const TileCoord origin{10, 20};
    const std::array<Dir, 4> dirs = {Dir::South, Dir::West, Dir::North, Dir::East};
    const std::array<TileCoord, 4> sizes = {TileCoord{3, 2}, TileCoord{2, 3}, TileCoord{3, 2},
                                            TileCoord{2, 3}};
    for (std::size_t i = 0; i < dirs.size(); ++i) {
        EXPECT_EQ(FootprintSize(d, dirs[i]), sizes[i]);
        const auto door = DoorTile(d, origin, dirs[i]);
        EXPECT_GE(door.x, origin.x);
        EXPECT_LT(door.x, origin.x + sizes[i].x);
        EXPECT_GE(door.z, origin.z);
        EXPECT_LT(door.z, origin.z + sizes[i].z);
        EXPECT_EQ(FrontTile(d, origin, dirs[i]), door + Step(dirs[i]));
    }
}
TEST(ObjectsTests, JsonRoundTripUsesCatalogIdsAndRejectsBrokenReferences) {
    const auto c = Load();
    ZooObjects o;
    Building b;
    b.def = c.BuildingIndex("burger_stall");
    b.origin = {2, 3};
    b.itemPrice = Dollars(14);
    o.AddBuilding(b);
    Prop p;
    p.def = c.PropIndex("hay_rack");
    p.position = {-4.5f, 8};
    p.food = c.FoodIndex("hay");
    p.amount = 5;
    o.AddProp(p);
    const auto json = ToJson(o, c);
    EXPECT_EQ(json["buildings"][0]["def"], "burger_stall");
    EXPECT_EQ(json["props"][0]["def"], "hay_rack");
    EXPECT_EQ(json["props"][0]["food"], "hay");
    std::string error;
    const auto loaded = ObjectsFromJson(json, c, &error);
    ASSERT_TRUE(loaded.has_value()) << error;
    EXPECT_EQ(*loaded, o);
    auto broken = json;
    broken["props"][0]["food"] = "no_such_food";
    EXPECT_FALSE(ObjectsFromJson(broken, c, &error));
    EXPECT_NE(error.find("unknown food"), std::string::npos);
    broken = json;
    broken["nextProp"] = 1;
    EXPECT_FALSE(ObjectsFromJson(broken, c, &error));
    broken = json;
    broken["props"][0]["id"] = 0;
    EXPECT_FALSE(ObjectsFromJson(broken, c, &error));
    Prop second = p;
    o.AddProp(second);
    auto ordered = ToJson(o, c);
    ordered["props"][1]["id"] = ordered["props"][0]["id"];
    EXPECT_FALSE(ObjectsFromJson(ordered, c, &error));
    ordered["props"][1]["id"] = 1;
    ordered["props"][0]["id"] = 2;
    EXPECT_FALSE(ObjectsFromJson(ordered, c, &error));
    ordered = ToJson(o, c);
    ordered["props"][0]["def"] = "missing_prop";
    EXPECT_FALSE(ObjectsFromJson(ordered, c, &error));
}
}  // namespace
}  // namespace zoo
