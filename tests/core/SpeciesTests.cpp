// SPDX-License-Identifier: MIT
#include "zoo/animals/Species.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <fstream>
#include <functional>
#include <iterator>
#include <string>

namespace zoo {
namespace {
SpeciesCatalog Catalog() {
    return SpeciesCatalog::LoadDirectory(std::string(ZOO_SOURCE_DIR) + "/data/species");
}

std::string ValidJson() {
    return R"({"id":"test","commonName":"Test animal","scientificName":"Testus animalis","family":"Testidae","model":"test","group":"bird","adultMassKg":2,"shoulderHeightM":1,"bodyLengthM":1,"bodyRadiusM":0.2,"walkSpeed":0.5,"runSpeed":1,"canSwim":false,"canClimb":false,"jumpHeightM":0.2,"lifespanYears":10,"maturityYears":2,"gestationDays":30,"litterMin":1,"litterMax":2,"birthIntervalYears":1,"diet":"herbivore","foods":["hay"],"foodPerDayKg":1,"waterPerDayL":1,"activity":"diurnal","sleepHoursPerDay":8,"groupMin":1,"groupMax":4,"maxAdultMales":0,"sociability":0.5,"playfulness":0.5,"shyness":0.5,"aggression":0.2,"dangerous":false,"minEnclosureM2":100,"spacePerExtraAnimalM2":10,"terrainSuitability":{"grass":0.8},"waterAreaFraction":0,"needsDeepWater":false,"vegetationDensity":0.2,"needsShelter":true,"tempMinC":0,"tempMaxC":30,"fenceHeightM":1,"fenceStrength":1,"price":100,"appeal":50,"status":"LC","nativeRange":"Test range","biome":"Test biome","facts":["Fact one.","Fact two.","Fact three."],"sources":["Test source"]})";
}

TEST(SpeciesTests, LoadsAllFilesInNameOrderAndSupportsLookup) {
    const auto catalog = Catalog();
    ASSERT_TRUE(catalog.Errors().empty()) << (catalog.Errors().empty() ? "" : catalog.Errors().front());
    ASSERT_EQ(catalog.All().size(), 52U);
    for (std::size_t i = 0; i < catalog.All().size(); ++i) {
        if (i > 0) {
            EXPECT_LT(catalog.All()[i - 1].id, catalog.All()[i].id);
        }
        EXPECT_FALSE(SpeciesCatalog::Validate(catalog.All()[i]).size());
        EXPECT_EQ(catalog.Find(catalog.All()[i].id), &catalog.All()[i]);
        EXPECT_EQ(catalog.IndexOf(catalog.All()[i].id), static_cast<int>(i));
    }
    EXPECT_EQ(catalog.Find("unknown"), nullptr);
    EXPECT_EQ(catalog.IndexOf("unknown"), -1);
}

TEST(SpeciesTests, RealDataHasCoherentRelativeValues) {
    const auto c = Catalog();
    ASSERT_EQ(c.All().size(), 52U);
    EXPECT_EQ(c.Find("elephant")->adultMassKg, 5000.0f);
    EXPECT_LT(c.Find("meerkat")->adultMassKg, c.Find("penguin")->adultMassKg);
    EXPECT_LT(c.Find("polar_bear")->tempMinC, c.Find("lion")->tempMinC);
    EXPECT_LT(c.Find("meerkat")->fenceStrength, c.Find("penguin")->fenceStrength);
    EXPECT_LT(c.Find("penguin")->fenceStrength, c.Find("zebra")->fenceStrength);
    EXPECT_LT(c.Find("zebra")->fenceStrength, c.Find("lion")->fenceStrength);
    EXPECT_LT(c.Find("lion")->fenceStrength, c.Find("elephant")->fenceStrength);
    EXPECT_EQ(c.Find("dingo")->model, "dingo");
    EXPECT_EQ(c.Find("african_wild_dog")->model, "african_wild_dog");
    EXPECT_EQ(c.Find("dhole")->model, "dhole");
    constexpr std::array<std::string_view, 8> foods = {"meat",          "fish",      "hay",     "browse",
                                                       "grass_pellets", "fruit_veg", "insects", "bamboo"};
    for (const auto& d : c.All()) {
        for (const auto& food : d.foods)
            EXPECT_NE(std::find(foods.begin(), foods.end(), food), foods.end()) << d.id << ": " << food;
        if (d.diet == Diet::Carnivore) {
            EXPECT_TRUE(std::find(d.foods.begin(), d.foods.end(), "meat") != d.foods.end() ||
                        std::find(d.foods.begin(), d.foods.end(), "fish") != d.foods.end());
        }
        if (d.needsDeepWater) {
            EXPECT_GT(d.waterAreaFraction, 0.0f);
        }
    }
}

TEST(SpeciesTests, ParsesValidJsonAndRejectsInvalidDocumentsAtomically) {
    SpeciesCatalog c;
    ASSERT_TRUE(c.AddFromJson(ValidJson(), "inline"));
    ASSERT_EQ(c.All().size(), 1U);
    const std::array<std::string, 6> invalid = {
        "{",
        "{}",
        ValidJson().substr(0, ValidJson().size() - 1) + ",\"unexpected\":1}",
        ValidJson().replace(ValidJson().find("\"adultMassKg\":2"), 16, "\"adultMassKg\":\"2\""),
        ValidJson().replace(ValidJson().find("\"diet\":\"herbivore\""), 19, "\"diet\":\"unknown\""),
        ValidJson().replace(ValidJson().find("\"appeal\":50"), 11, "\"appeal\":101")};
    for (const auto& json : invalid) {
        const auto count = c.All().size();
        EXPECT_FALSE(c.AddFromJson(json, "bad.json"));
        EXPECT_EQ(c.All().size(), count);
    }
    EXPECT_FALSE(c.AddFromJson(ValidJson(), "duplicate"));
    EXPECT_EQ(c.All().size(), 1U);
    EXPECT_GE(c.Errors().size(), invalid.size() + 1U);
}

TEST(SpeciesTests, ValidationReportsIndependentRuleFamilies) {
    auto c = Catalog();
    ASSERT_NE(c.Find("lion"), nullptr);
    using Break = std::pair<std::string, std::function<void(SpeciesDef&)>>;
    const std::array<Break, 11> cases = {{
        {"id", [](SpeciesDef& d) { d.id = "Bad id"; }},
        {"identity", [](SpeciesDef& d) { d.commonName.clear(); }},
        {"positive measurements", [](SpeciesDef& d) { d.adultMassKg = 0; }},
        {"speed order", [](SpeciesDef& d) { d.runSpeed = d.walkSpeed - 1; }},
        {"life history", [](SpeciesDef& d) { d.maturityYears = d.lifespanYears; }},
        {"litter bounds", [](SpeciesDef& d) { d.litterMin = d.litterMax + 1; }},
        {"group bounds", [](SpeciesDef& d) { d.groupMin = d.groupMax + 1; }},
        {"unit interval", [](SpeciesDef& d) { d.sociability = 1.1f; }},
        {"terrain and water",
         [](SpeciesDef& d) {
             d.terrainSuitability.fill(0);
             d.waterAreaFraction = 1.1f;
         }},
        {"temperature", [](SpeciesDef& d) { d.tempMinC = d.tempMaxC; }},
        {"containment and management",
         [](SpeciesDef& d) {
             d.fenceStrength = 6;
             d.price = -1;
             d.appeal = 101;
         }},
    }};
    for (const auto& [label, mutate] : cases) {
        SpeciesDef d = *c.Find("lion");
        mutate(d);
        EXPECT_FALSE(SpeciesCatalog::Validate(d).empty()) << label;
    }
    SpeciesDef noFood = *c.Find("lion");
    noFood.foods.clear();
    EXPECT_FALSE(SpeciesCatalog::Validate(noFood).empty()) << "foods";
    SpeciesDef fewFacts = *c.Find("lion");
    fewFacts.facts.resize(2);
    EXPECT_FALSE(SpeciesCatalog::Validate(fewFacts).empty()) << "facts";
}

TEST(SpeciesTests, EveryEnumHasStableText) {
    constexpr std::array<Diet, 5> diets = {Diet::Carnivore, Diet::Herbivore, Diet::Omnivore, Diet::Piscivore,
                                           Diet::Insectivore};
    constexpr std::array<std::string_view, 5> dietText = {"carnivore", "herbivore", "omnivore", "piscivore",
                                                          "insectivore"};
    for (std::size_t i = 0; i < diets.size(); ++i)
        EXPECT_EQ(ToString(diets[i]), dietText[i]);
    constexpr std::array<ActivityPattern, 4> activities = {
        ActivityPattern::Diurnal, ActivityPattern::Nocturnal, ActivityPattern::Crepuscular,
        ActivityPattern::Cathemeral};
    constexpr std::array<std::string_view, 4> activityText = {"diurnal", "nocturnal", "crepuscular",
                                                              "cathemeral"};
    for (std::size_t i = 0; i < activities.size(); ++i)
        EXPECT_EQ(ToString(activities[i]), activityText[i]);
    constexpr std::array<ConservationStatus, 7> statuses = {
        ConservationStatus::LeastConcern,         ConservationStatus::NearThreatened,
        ConservationStatus::Vulnerable,           ConservationStatus::Endangered,
        ConservationStatus::CriticallyEndangered, ConservationStatus::ExtinctInTheWild,
        ConservationStatus::NotEvaluated};
    constexpr std::array<std::string_view, 7> statusText = {"LC", "NT", "VU", "EN", "CR", "EW", "NE"};
    constexpr std::array<std::string_view, 7> displayText = {
        "Least Concern",         "Near Threatened",     "Vulnerable",   "Endangered",
        "Critically Endangered", "Extinct in the Wild", "Not Evaluated"};
    for (std::size_t i = 0; i < statuses.size(); ++i) {
        EXPECT_EQ(ToString(statuses[i]), statusText[i]);
        EXPECT_EQ(DisplayName(statuses[i]), displayText[i]);
    }
}
}  // namespace
}  // namespace zoo
