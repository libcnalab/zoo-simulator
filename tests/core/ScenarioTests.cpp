// SPDX-License-Identifier: MIT
#include <gtest/gtest.h>

#include "TestData.hpp"
#include "zoo/sim/Scenario.hpp"

namespace zoo {

namespace {

std::string Joined(const std::vector<std::string>& lines) {
    std::string out;
    for (const std::string& line : lines) {
        out += line + "\n";
    }
    return out;
}

}  // namespace

TEST(ScenarioTests, StarterZooBuildsWithoutProblemsOnTheDefaultMap) {
    Zoo zoo(testdata::GameCatalog(), testdata::GameSpecies());
    const Money before = zoo.Accounts().Balance();
    const ScenarioReport report = BuildStarterZoo(zoo);
    EXPECT_TRUE(report.problems.empty()) << Joined(report.problems);
    EXPECT_EQ(report.enclosures, 3);
    EXPECT_EQ(report.animals, 3 + 5 + 4);
    EXPECT_EQ(report.buildings, 9);
    EXPECT_GT(report.props, 80);
    EXPECT_EQ(zoo.Accounts().Balance(), before) << "the starter zoo is a gift";
    EXPECT_EQ(zoo.AnimalCount(), 12);
}

TEST(ScenarioTests, StarterEnclosuresSuitTheirAnimals) {
    Zoo zoo(testdata::GameCatalog(), testdata::GameSpecies());
    BuildStarterZoo(zoo);
    int enclosures = 0;
    for (const Enclosure& e : zoo.Habitats().All()) {
        if (e.name != "Lion Savanna" && e.name != "Zebra Plain" && e.name != "Wolf Wood") {
            continue;
        }
        ++enclosures;
        EXPECT_GE(e.stats.gates, 1) << e.name << " needs a keeper gate";
        EXPECT_GT(e.stats.viewingTiles, 20) << e.name;
        EXPECT_GE(e.stats.shelterCapacity, 5) << e.name;
        EXPECT_GT(e.stats.waterShare, 0.05f) << e.name;
    }
    EXPECT_EQ(enclosures, 3);
    // Nobody is loose and everybody is content with the place on the first evening.
    for (int i = 0; i < kTicksPerGameHour * 6; ++i) {
        zoo.Step();
    }
    for (const Animal& animal : zoo.Fauna().All()) {
        EXPECT_FALSE(animal.loose) << animal.name;
        EXPECT_GT(animal.welfare, 0.6f) << animal.name << " of species " << animal.species;
    }
}

TEST(ScenarioTests, StarterZooIsTheSameEveryTime) {
    Zoo a(testdata::GameCatalog(), testdata::GameSpecies());
    Zoo b(testdata::GameCatalog(), testdata::GameSpecies());
    BuildStarterZoo(a);
    BuildStarterZoo(b);
    EXPECT_TRUE(a.SameState(b));
    EXPECT_EQ(a.StateHash(), b.StateHash());
}

TEST(ScenarioTests, SmallMapsAreRefusedPolitely) {
    ZooConfig config;
    config.world.width = 48;
    config.world.depth = 48;
    Zoo zoo(testdata::GameCatalog(), testdata::GameSpecies(), config);
    const ScenarioReport report = BuildStarterZoo(zoo);
    EXPECT_FALSE(report.problems.empty());
    EXPECT_EQ(report.animals, 0);
}

}  // namespace zoo
