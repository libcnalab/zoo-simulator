// SPDX-License-Identifier: MIT
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <map>
#include <set>

#include "TestData.hpp"
#include "zoo/animals/AnimalTuning.hpp"
#include "zoo/sim/VisitStart.hpp"
#include "zoo/sim/VisitorZoo.hpp"
#include "zoo/sim/ZooSave.hpp"

namespace zoo {
namespace {
// ScenarioReport contains totals, not a per-habitat roster. These are the scenario's promises,
// independent of the animals actually created (so a missing whole group cannot escape detection).
const std::map<std::string, std::vector<std::string>> kResidents{{"Meerkat Lookout", {"meerkat"}},
                                                                 {"Flamingo Lagoon", {"flamingo"}},
                                                                 {"Acacia Plain", {"giraffe", "zebra"}},
                                                                 {"Ostrich Rise", {"ostrich"}},
                                                                 {"Lion Rock", {"lion"}},
                                                                 {"Tiger Forest", {"tiger"}},
                                                                 {"Wolf Wood", {"wolf"}},
                                                                 {"Bear Falls", {"brown_bear"}},
                                                                 {"Polar Sound", {"polar_bear"}},
                                                                 {"Penguin Cove", {"penguin"}},
                                                                 {"Elephant Valley", {"elephant"}},
                                                                 {"Rhino Hollow", {"rhino"}},
                                                                 {"Red Dune", {"camel"}},
                                                                 {"Kangaroo Grove", {"kangaroo"}}};

void ExpectResidents(const Zoo& zoo, bool omitPenguin = false) {
    ASSERT_EQ(zoo.Habitats().All().size(), kResidents.size());
    for (const Enclosure& home : zoo.Habitats().All()) {
        ASSERT_TRUE(kResidents.contains(home.name)) << home.name;
        for (const std::string& species : kResidents.at(home.name)) {
            const int index = zoo.SpeciesList().IndexOf(species);
            int count = 0;
            for (const Animal& animal : zoo.Fauna().All()) {
                if (animal.InWorld() && animal.health > 0.0f && animal.species == index &&
                    animal.home == home.id && zoo.Habitats().At(animal.position) == home.id) {
                    ++count;
                }
            }
            if (omitPenguin && species == "penguin") {
                EXPECT_EQ(count, 0);
            } else {
                EXPECT_GT(count, 0) << home.name << '/' << species;
            }
        }
    }
}

void ExpectContained(const Zoo& zoo, bool checkProps = false) {
    for (const Animal& animal : zoo.Fauna().All()) {
        SCOPED_TRACE(animal.name);
        ASSERT_TRUE(animal.InWorld());
        EXPECT_FALSE(animal.loose);
        EXPECT_GT(animal.health, 0.0f);
        EXPECT_TRUE(animal.home);
        EXPECT_EQ(zoo.Habitats().At(animal.position), animal.home);
        const SpeciesDef& species = zoo.SpeciesList().All()[static_cast<std::size_t>(animal.species)];
        const Enclosure* home = zoo.Habitats().Find(animal.home);
        ASSERT_NE(home, nullptr);
        ASSERT_TRUE(kResidents.contains(home->name));
        const auto& expected = kResidents.at(home->name);
        EXPECT_NE(std::find(expected.begin(), expected.end(), species.id), expected.end());
        if (animal.shelter) {
            // Sheltered animals are intentionally inside the shelter building.
            continue;
        }
        const float wade = tuning::kWadingShoulderFraction * species.shoulderHeightM *
                           BodyScale(animal, species, zoo.Time());
        EXPECT_TRUE(
            zoo.Routes().Passable(World::TileAt(animal.position), NavProfile::Animal(wade, species.canSwim)));
        if (!species.canSwim) {
            EXPECT_LE(zoo.Map().WaterDepthAt(animal.position), wade);
        }
        for (const Prop& prop : zoo.Objects().Props()) {
            const PropDef& def = zoo.Definitions().props[static_cast<std::size_t>(prop.def)];
            if (checkProps && def.blocksWalking) {
                EXPECT_GE(Distance(animal.position, prop.position), def.radiusM)
                    << species.id << " inside " << def.id;
            }
        }
    }
}

float DistanceToTiles(Vec2 p, TileCoord min, TileCoord max) {
    const float x0 = static_cast<float>(min.x) * kTileSize;
    const float z0 = static_cast<float>(min.z) * kTileSize;
    const float x1 = static_cast<float>(max.x + 1) * kTileSize;
    const float z1 = static_cast<float>(max.z + 1) * kTileSize;
    const float dx = std::max({x0 - p.x, 0.0f, p.x - x1});
    const float dz = std::max({z0 - p.z, 0.0f, p.z - z1});
    return std::sqrt(dx * dx + dz * dz);
}

std::map<int, int> Living(const Zoo& zoo, EnclosureId home) {
    std::map<int, int> counts;
    for (const Animal& animal : zoo.Fauna().All()) {
        if (animal.InWorld() && animal.home == home && animal.species >= 0) {
            ++counts[animal.species];
        }
    }
    return counts;
}

const Enclosure* BoardHome(const Zoo& zoo, const Prop& board) {
    const Enclosure* home = nullptr;
    float nearest = 30.0f;
    for (const Enclosure& enclosure : zoo.Habitats().All()) {
        const float distance = DistanceToTiles(board.position, enclosure.min, enclosure.max);
        if (distance < nearest && !Living(zoo, enclosure.id).empty()) {
            nearest = distance;
            home = &enclosure;
        }
    }
    return home;
}

bool ReadableFromPath(const Zoo& zoo, const Prop& board) {
    const TileCoord at = World::TileAt(board.position);
    for (int z = at.z - 2; z <= at.z + 2; ++z) {
        for (int x = at.x - 2; x <= at.x + 2; ++x) {
            const TileCoord tile{x, z};
            if (!zoo.Routes().Passable(tile, NavProfile::Visitor())) {
                continue;
            }
            const Vec2 here = World::TileCenter(tile);
            const float distance = Distance(here, board.position);
            if (distance > 2.8f) {
                continue;
            }
            // Turn directly towards the board, then use UpdateEncounters' nearest/gaze rule.
            const Vec2 gaze = distance > 0.0f ? (board.position - here) / distance : Vec2{1.0f, 0.0f};
            std::vector<PropId> near;
            zoo.Objects().PropsNear(here, 2.8f, near);
            float best = 2.8f;
            PropId chosen;
            for (const PropId id : near) {
                const Prop* candidate = zoo.Objects().FindProp(id);
                if (candidate == nullptr ||
                    zoo.Definitions().props[static_cast<std::size_t>(candidate->def)].role !=
                        PropRole::InfoBoard) {
                    continue;
                }
                const Vec2 delta = candidate->position - here;
                const float length = Length(delta);
                if (length > best ||
                    (length > 0.6f && (delta.x * gaze.x + delta.z * gaze.z) / length < 0.35f)) {
                    continue;
                }
                best = length;
                chosen = id;
            }
            if (chosen == board.id) {
                return true;
            }
        }
    }
    return false;
}

class VisitorZooHabitat : public testing::TestWithParam<std::uint64_t> {};

TEST_P(VisitorZooHabitat, ResidentsAndOmittedPenguins) {
    for (const bool omitted : {false, true}) {
        Zoo zoo(testdata::GameCatalog(), testdata::GameSpecies(), VisitorZooConfig(GetParam()));
        const auto report =
            BuildVisitorZoo(zoo, omitted ? VisitorZooOptions{{"penguin"}} : VisitorZooOptions{});
        EXPECT_TRUE(report.problems.empty());
        EXPECT_EQ(report.enclosures, 14);
        EXPECT_EQ(report.animals, zoo.AnimalCount());
        ExpectResidents(zoo, omitted);
        ExpectContained(zoo);
        // 600 steps keep normal ASan checks short; the full morning is exercised separately.
        FillForVisit(zoo, {.fillTicks = 600});
        ExpectResidents(zoo, omitted);
        ExpectContained(zoo);
    }
}

TEST_P(VisitorZooHabitat, EveryHabitatHasItsOwnReadableBoard) {
    Zoo zoo(testdata::GameCatalog(), testdata::GameSpecies(), VisitorZooConfig(GetParam()));
    BuildVisitorZoo(zoo);
    std::map<std::uint32_t, std::set<std::uint32_t>> boards;
    for (const Prop& board : zoo.Objects().Props()) {
        if (zoo.Definitions().props[static_cast<std::size_t>(board.def)].role != PropRole::InfoBoard) {
            continue;
        }
        EXPECT_FALSE(zoo.Habitats().At(board.position));
        const Enclosure* home = BoardHome(zoo, board);
        if (home == nullptr || !ReadableFromPath(zoo, board)) {
            continue;
        }
        const auto living = Living(zoo, home->id);
        int chosen = living.begin()->first;
        for (const auto& [species, count] : living) {
            if (count > living.at(chosen)) {
                chosen = species;
            }
        }
        const auto& expected = kResidents.at(home->name);
        const auto& def = zoo.SpeciesList().All()[static_cast<std::size_t>(chosen)];
        EXPECT_NE(std::find(expected.begin(), expected.end(), def.id), expected.end());
        boards[home->id.value].insert(board.id.value);
    }
    for (const Enclosure& home : zoo.Habitats().All()) {
        EXPECT_FALSE(boards[home.id.value].empty()) << home.name;
        for (const Enclosure& other : zoo.Habitats().All()) {
            if (home.id != other.id) {
                EXPECT_NE(boards[home.id.value], boards[other.id.value]);
            }
        }
        for (const auto& [index, count] : Living(zoo, home.id)) {
            EXPECT_GT(count, 0);
            const SpeciesDef& def = zoo.SpeciesList().All()[static_cast<std::size_t>(index)];
            EXPECT_FALSE(def.facts.empty()) << def.id;
            for (const auto& fact : def.facts) {
                EXPECT_FALSE(fact.empty()) << def.id;
            }
            EXPECT_FALSE(def.nativeRange.empty()) << def.id;
            EXPECT_FALSE(ToString(def.diet).empty()) << def.id;
            EXPECT_FALSE(DisplayName(def.status).empty()) << def.id;
        }
    }
}

void CheckVisiting(Zoo& zoo, std::int64_t ticks) {
    FillForVisit(zoo, {.fillTicks = ticks});
    ExpectResidents(zoo);
    ExpectContained(zoo);
    std::map<std::uint32_t, int> streak;
    std::map<std::uint32_t, int> longest;
    std::map<std::string, float> deepest;
    for (int step = 0; step <= 2000; ++step) {
        for (const Enclosure& home : zoo.Habitats().All()) {
            bool visible = false;
            for (const Animal& animal : zoo.Fauna().All()) {
                visible = visible || (animal.home == home.id && animal.Visible());
            }
            if (step == 0) {
                EXPECT_TRUE(visible) << home.name;
            }
            int& consecutive = streak[home.id.value];
            consecutive = visible ? 0 : consecutive + 1;
            longest[home.id.value] = std::max(longest[home.id.value], consecutive);
        }
        // Nobody that cannot swim stands in water over its wading depth (a meerkat was seen
        // strolling across its pool, 2026-11-13).
        for (const Animal& animal : zoo.Fauna().All()) {
            if (!animal.InWorld() || animal.shelter || animal.species < 0) {
                continue;
            }
            const SpeciesDef& def = zoo.SpeciesList().All()[static_cast<std::size_t>(animal.species)];
            if (def.canSwim) {
                continue;
            }
            const float depth = zoo.Map().WaterDepthAt(animal.position);
            float& worst = deepest[def.id];
            worst = std::max(worst, depth - 0.6f * def.shoulderHeightM);
        }
        if (step < 2000) {
            zoo.Step();
        }
    }
    for (const auto& [species, over] : deepest) {
        EXPECT_LE(over, 0.05f) << species << " stood " << over << " m deeper than it wades";
    }
    for (const Enclosure& home : zoo.Habitats().All()) {
        std::cout << "seed=" << zoo.Config().seed << " fill=" << ticks << ' ' << home.name
                  << " longest invisible=" << longest[home.id.value] << '\n';
        EXPECT_LE(longest[home.id.value], 600) << home.name;
    }
    ExpectResidents(zoo);
    ExpectContained(zoo, true);
}

TEST_P(VisitorZooHabitat, VisibleAndContainedDuringVisit) {
    Zoo zoo(testdata::GameCatalog(), testdata::GameSpecies(), VisitorZooConfig(GetParam()));
    BuildVisitorZoo(zoo);
    // Full 6000-step fills are too expensive for routine ASan tests.
    CheckVisiting(zoo, 600);
}

TEST_P(VisitorZooHabitat, DISABLED_VisibleAndContainedFull) {
    Zoo zoo(testdata::GameCatalog(), testdata::GameSpecies(), VisitorZooConfig(GetParam()));
    BuildVisitorZoo(zoo);
    CheckVisiting(zoo, 6000);
}

void ExpectHashes(const Zoo& original, const Zoo& loaded) {
    EXPECT_EQ(original.Fauna().StateHash(), loaded.Fauna().StateHash()) << "animals";
    EXPECT_EQ(original.Guests().StateHash(), loaded.Guests().StateHash()) << "visitors";
    EXPECT_EQ(original.Sky().State(), loaded.Sky().State()) << "weather";
    EXPECT_EQ(original.Time(), loaded.Time()) << "calendar";
    EXPECT_EQ(original.Accounts(), loaded.Accounts()) << "finance";
    EXPECT_EQ(original.Objects(), loaded.Objects()) << "objects";
    EXPECT_EQ(original.Habitats(), loaded.Habitats()) << "enclosures";
    EXPECT_EQ(original.StateHash(), loaded.StateHash()) << "whole zoo";
}

// The pace of a visit is a setting, not state (Visitors::Pace): a saved zoo does not carry it,
// and whoever loads a visit sets it again, as ZooGame::LoadVisit does through PrepareVisit.
// With that, the loaded zoo goes on exactly as the one that was saved.
TEST_P(VisitorZooHabitat, SaveLoadAtVisitingTime) {
    Zoo zoo(testdata::GameCatalog(), testdata::GameSpecies(), VisitorZooConfig(GetParam()));
    BuildVisitorZoo(zoo);
    FillForVisit(zoo, {.fillTicks = 600});
    const std::filesystem::path directory = std::string(ZOO_SOURCE_DIR) + "/build-probe/tests";
    std::filesystem::create_directories(directory);
    const auto file = directory / ("visit-start-" + std::to_string(GetParam()) + ".json");
    std::string error;
    ASSERT_TRUE(SaveZoo(zoo, file.string(), &error)) << error;
    const auto loaded = LoadZoo(file.string(), zoo.Definitions(), zoo.SpeciesList(), &error);
    ASSERT_NE(loaded, nullptr) << error;
    SCOPED_TRACE("after load / after 500 further steps");
    ExpectHashes(zoo, *loaded);
    EXPECT_FLOAT_EQ(loaded->Guests().Pace().linger, VisitorPace{}.linger);
    loaded->Guests().SetPace(zoo.Guests().Pace());
    for (int i = 0; i < 500; ++i) {
        zoo.Step();
        loaded->Step();
    }
    ExpectHashes(zoo, *loaded);
    std::filesystem::remove(file);
}

INSTANTIATE_TEST_SUITE_P(Seeds, VisitorZooHabitat, testing::Values(1ULL, 2ULL));
}  // namespace
}  // namespace zoo
