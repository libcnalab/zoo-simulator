// SPDX-License-Identifier: MIT
#include "zoo/world/WorldJson.hpp"

#include <gtest/gtest.h>

#include <string>

namespace zoo {
namespace {

TEST(WorldJson, GeneratedAndEditedWorldRoundTrips) {
    WorldGenParams params;
    params.width = 48;
    params.depth = 48;
    params.seed = 77;
    World world = World::Generate(params);
    world.SetTerrain({4, 4}, Terrain::Rock);
    world.SetPath({4, 4}, PathKind::Gravel);
    world.SetBuilding({5, 5}, BuildingId{42});
    world.SetVertexHeight(7, 8, 2.375f);
    world.SetWater({9, 9}, 1.25f, 0.75f);
    world.SetFence({8, 8, false}, {12, true, 0.625f});
    const auto restored = WorldFromJson(ToJson(world));
    ASSERT_TRUE(restored.has_value());
    EXPECT_TRUE(world == *restored);
}

TEST(WorldJson, Generated128MapStaysCompact) {
    WorldGenParams params;
    const World world = World::Generate(params);
    EXPECT_LT(ToJson(world).dump().size(), 300000u);
}

TEST(WorldJson, MalformedDocumentsFailWithUsefulErrors) {
    const nlohmann::json good = ToJson(World(8, 8));
    std::string error;
    auto expectInvalid = [&error](nlohmann::json invalid) {
        error.clear();
        EXPECT_FALSE(WorldFromJson(invalid, &error).has_value());
        EXPECT_FALSE(error.empty());
    };
    expectInvalid(nullptr);
    nlohmann::json bad = good;
    bad["width"] = 10000;
    expectInvalid(bad);
    bad = good;
    bad["tiles"][0]["terrain"] = "lava";
    expectInvalid(bad);
    bad = good;
    bad["heights_mm"].erase(bad["heights_mm"].begin());
    expectInvalid(bad);
    bad = good;
    bad["property"]["min"] = "wrong";
    expectInvalid(bad);
    bad = good;
    bad["fences_h"] =
        nlohmann::json::array({{{"count", 1}, {"kind", 256}, {"gate", false}, {"condition", 1.0}}});
    expectInvalid(bad);
}

}  // namespace
}  // namespace zoo
