// SPDX-License-Identifier: MIT
#include <gtest/gtest.h>
#include <algorithm>
#include <nlohmann/json.hpp>
#include <string>
#include "zoo/animals/Species.hpp"
#include "zoo/build/Catalog.hpp"
namespace zoo {
namespace {
Catalog Shipped() {
    return Catalog::LoadDirectory(std::string(ZOO_SOURCE_DIR) + "/data/construction");
}
SpeciesCatalog Species() {
    return SpeciesCatalog::LoadDirectory(std::string(ZOO_SOURCE_DIR) + "/data/species");
}
using Json = nlohmann::json;
Json Entry(std::string_view plural, Json value) {
    return Json{{std::string(plural), Json::array({std::move(value)})}};
}
TEST(CatalogTests, ShippedDataLoadsAndHasCompleteEnumsAndLookups) {
    const Catalog c = Shipped();
    ASSERT_TRUE(c.errors.empty()) << (c.errors.empty() ? "" : c.errors.front());
    EXPECT_EQ(c.fences.size(), 16U);
    EXPECT_EQ(c.foods.size(), 8U);
    EXPECT_GE(c.buildings.size(), 13U);
    EXPECT_GE(c.props.size(), 35U);
    for (int i = 1; i < kPathKindCount; ++i)
        EXPECT_EQ(c.Path(static_cast<PathKind>(i)).kind, static_cast<PathKind>(i));
    for (int i = 0; i < kTerrainCount; ++i)
        EXPECT_EQ(c.TerrainInfo(static_cast<Terrain>(i)).terrain, static_cast<Terrain>(i));
    EXPECT_NE(c.FindFence("wire_mesh"), nullptr);
    EXPECT_EQ(c.FenceByKind(0), nullptr);
    EXPECT_EQ(c.FoodIndex("meat"), 0);
    EXPECT_EQ(c.BuildingIndex("burger_stall"), 5);
    EXPECT_GE(c.PropIndex("oak"), 0);
}
TEST(CatalogTests, HabitatBarrierKindsRemainStable) {
    const Catalog catalog = Shipped();
    int kind = 1;
    for (const auto id :
         {"wooden_rail", "wire_mesh", "tall_mesh", "steel_bars", "glass_wall", "concrete_wall",
          "cable_barrier", "hedge", "high_cable_barrier", "high_glass_wall", "heavy_mesh", "stand_off_rail",
          "paddock_rail", "tall_cable_fence", "dry_moat", "mesh_backed_hedge"}) {
        const FenceDef* fence = catalog.FindFence(id);
        ASSERT_NE(fence, nullptr);
        EXPECT_EQ(fence->kind, kind);
        ++kind;
    }
    const auto& mesh = *catalog.FindFence("heavy_mesh");
    EXPECT_EQ(mesh.style, FenceStyle::Mesh);
    EXPECT_FLOAT_EQ(mesh.heightM, 4.5f);
    EXPECT_EQ(mesh.strength, 4);
    EXPECT_FLOAT_EQ(mesh.visibility, 0.8f);
    EXPECT_GT(mesh.cost, catalog.FindFence("tall_mesh")->cost);
    EXPECT_LT(mesh.cost, catalog.FindFence("steel_bars")->cost);
    const auto& rail = *catalog.FindFence("stand_off_rail");
    EXPECT_EQ(rail.style, FenceStyle::Rail);
    EXPECT_FLOAT_EQ(rail.heightM, 1.1f);
    EXPECT_EQ(rail.strength, 1);
    EXPECT_FLOAT_EQ(rail.visibility, 1.0f);
    const auto& moat = *catalog.FindFence("dry_moat");
    EXPECT_EQ(moat.style, FenceStyle::Moat);
    EXPECT_FLOAT_EQ(moat.heightM, 2.4f);
    EXPECT_EQ(moat.strength, 5);
    EXPECT_FLOAT_EQ(moat.visibility, 1.0f);
    const auto& wildlifeHedge = *catalog.FindFence("mesh_backed_hedge");
    EXPECT_EQ(wildlifeHedge.style, FenceStyle::Hedge);
    EXPECT_FLOAT_EQ(wildlifeHedge.heightM, 1.8f);
    EXPECT_EQ(wildlifeHedge.strength, 2);
    EXPECT_GT(wildlifeHedge.heightM, catalog.FindFence("hedge")->heightM);
}

TEST(CatalogTests, EverySpeciesFoodAndContainmentRequirementIsCovered) {
    const auto c = Shipped();
    const auto species = Species();
    ASSERT_TRUE(species.Errors().empty());
    for (const auto& animal : species.All()) {
        for (const auto& food : animal.foods)
            EXPECT_GE(c.FoodIndex(food), 0) << animal.id << " needs " << food;
        const auto found = std::find_if(c.fences.begin(), c.fences.end(), [&](const FenceDef& f) {
            return f.heightM >= animal.fenceHeightM && f.strength >= animal.fenceStrength;
        });
        EXPECT_NE(found, c.fences.end())
            << animal.id << " needs " << animal.fenceHeightM << "m and strength " << animal.fenceStrength;
    }
}
TEST(CatalogTests, ColorFormsAndEntryErrorsAreHandled) {
    const auto c = Catalog::FromJson(
        R"({"fences":[{"id":"hex","name":"Hex","kind":9,"style":"mesh","heightM":2,"strength":2,"visibility":0.5,"cost":12,"gateCost":4,"upkeepPerDay":1,"wearPerDay":0.01,"color":"#ffffff","postColor":[0.5,0.5,0.5]},{"id":"bad","name":"Bad","kind":10,"style":"mesh","heightM":9,"strength":2,"visibility":0.5,"cost":12,"gateCost":4,"upkeepPerDay":1,"wearPerDay":0.01,"color":[1,1,1],"postColor":[0,0,0]}]})",
        R"({"paths":[]})", R"({"terrains":[]})", R"({"foods":[]})", R"({"buildings":[]})", R"({"props":[]})");
    ASSERT_EQ(c.fences.size(), 1U);
    EXPECT_NEAR(c.fences[0].color[0], 1.0f, 0.001f);
    ASSERT_FALSE(c.errors.empty());
    EXPECT_NE(c.errors[0].find("heightM"), std::string::npos);
}
TEST(CatalogTests, UnknownKeysAndInvalidFeederFoodAreReportedAndSkipped) {
    const auto c = Catalog::FromJson(
        "", "", "",
        R"({"foods":[{"id":"hay","name":"Hay","costPerKg":0.2,"shelfLifeDays":4,"color":[0.3,0.4,0.2]}]})",
        "",
        R"({"props":[{"id":"x","name":"X","role":"feeder","placement":"enclosure","radiusM":1,"heightM":1,"blocksWalking":true,"cost":10,"vegetationM2":0,"shadeM2":0,"biome":"","enrichment":0,"foods":["bad_food"],"capacity":2,"feedHeightM":1,"beauty":0,"look":"x","color":[0,0,0],"color2":[0,0,0],"extra":1}]})");
    EXPECT_TRUE(c.props.empty());
    ASSERT_GE(c.errors.size(), 1U);
}
TEST(CatalogTests, BuildingRulesRejectInvalidFootprintsDoorsAndShopMargins) {
    Json base = {{"id", "stall"},
                 {"name", "Stall"},
                 {"description", "A small stall."},
                 {"role", "food_stall"},
                 {"placement", "public"},
                 {"width", 2},
                 {"depth", 2},
                 {"doorOffset", 0},
                 {"heightM", 3},
                 {"cost", 1000},
                 {"upkeepPerDay", 10},
                 {"capacity", 4},
                 {"heated", false},
                 {"cooled", false},
                 {"item", "Meal"},
                 {"itemPrice", 10},
                 {"itemCost", 4},
                 {"serviceSeconds", 8},
                 {"hungerRelief", 0.4},
                 {"thirstRelief", 0},
                 {"fun", 0.1},
                 {"education", 0},
                 {"look", "kiosk"},
                 {"wallColor", "#ffffff"},
                 {"roofColor", Json::array({0.2, 0.2, 0.2})}};
    const std::array<std::pair<std::string, std::string>, 4> cases = {{
        {"width", "zero width"},
        {"depth", "oversized depth"},
        {"doorOffset", "door outside width"},
        {"itemPrice", "no shop margin"},
    }};
    for (const auto& [field, reason] : cases) {
        Json bad = base;
        if (field == "width") {
            bad[field] = 0;
        } else if (field == "depth") {
            bad[field] = 9;
        } else if (field == "doorOffset") {
            bad[field] = 2;
        } else {
            bad[field] = 4;
        }
        const auto c = Catalog::FromJson("", "", "", "", Entry("buildings", bad).dump(), "");
        EXPECT_TRUE(c.buildings.empty()) << reason;
        ASSERT_FALSE(c.errors.empty()) << reason;
    }
    Json wrongType = base;
    wrongType["heated"] = "false";
    const auto typed = Catalog::FromJson("", "", "", "", Entry("buildings", wrongType).dump(), "");
    EXPECT_TRUE(typed.buildings.empty());
}
TEST(CatalogTests, FeederFoodsAndCapacityRolesAreValidated) {
    const std::string food = Entry("foods", Json{{"id", "hay"},
                                                 {"name", "Hay"},
                                                 {"costPerKg", 0.2},
                                                 {"shelfLifeDays", 5},
                                                 {"color", "#aabbcc"}})
                                 .dump();
    Json feeder = {{"id", "rack"},
                   {"name", "Rack"},
                   {"role", "feeder"},
                   {"placement", "enclosure"},
                   {"radiusM", 0.5},
                   {"heightM", 1.2},
                   {"blocksWalking", true},
                   {"cost", 100},
                   {"vegetationM2", 0},
                   {"shadeM2", 0},
                   {"biome", ""},
                   {"enrichment", 0},
                   {"foods", Json::array({"hay"})},
                   {"capacity", 10},
                   {"feedHeightM", 1},
                   {"beauty", 0},
                   {"look", "hay_rack"},
                   {"color", "#ffffff"},
                   {"color2", "#000000"}};
    const auto valid = Catalog::FromJson("", "", "", food, "", Entry("props", feeder).dump());
    ASSERT_EQ(valid.props.size(), 1U);
    for (const auto& badFoods : {Json::array(), Json::array({"missing"})}) {
        Json bad = feeder;
        bad["foods"] = badFoods;
        const auto c = Catalog::FromJson("", "", "", food, "", Entry("props", bad).dump());
        EXPECT_TRUE(c.props.empty());
        EXPECT_FALSE(c.errors.empty());
    }
    Json noCapacity = feeder;
    noCapacity["capacity"] = 0;
    EXPECT_TRUE(Catalog::FromJson("", "", "", food, "", Entry("props", noCapacity).dump()).props.empty());
    Json trough = feeder;
    trough["id"] = "trough";
    trough["role"] = "water_trough";
    trough["foods"] = Json::array();
    trough["capacity"] = 0;
    EXPECT_TRUE(Catalog::FromJson("", "", "", food, "", Entry("props", trough).dump()).props.empty());
}
}  // namespace
}  // namespace zoo
