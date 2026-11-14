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

int StripDepth(const Enclosure& home) {
    if (home.name == "Elephant Valley" || home.name == "Rhino Hollow") {
        return 2;
    }
    if (home.name == "Acacia Plain" || home.name == "Ostrich Rise" || home.name == "Red Dune" ||
        home.name == "Kangaroo Grove") {
        return 1;
    }
    return 0;
}

/// What the hoofstock stands behind: cables for the heavy and the tall, a timber rail for the rest.
std::string_view FrontBarrier(const Enclosure& home) {
    if (home.name == "Elephant Valley" || home.name == "Rhino Hollow") {
        return "cable_barrier";
    }
    return home.name == "Acacia Plain" || home.name == "Ostrich Rise" ? "tall_cable_fence" : "paddock_rail";
}

void ExpectFronts(const Zoo& zoo) {
    for (const Enclosure& home : zoo.Habitats().All()) {
        SCOPED_TRACE(home.name);
        const int depth = StripDepth(home);
        const Dir inwardView = home.min.x < zoo.Map().EntranceTile().x ? Dir::West : Dir::East;
        for (const Dir side : {Dir::North, inwardView}) {
            bool found = false;
            for (const ViewingSpot& spot : home.viewing) {
                if (spot.side == side && spot.standOffM == static_cast<float>(depth) * kTileSize) {
                    found = true;
                }
            }
            EXPECT_TRUE(found) << static_cast<int>(side);
        }
        for (const ViewingSpot& spot : home.viewing) {
            EXPECT_EQ(zoo.Habitats().RegionAt(spot.tile), Region::Public);
            EXPECT_GE(spot.standOffM, static_cast<float>(depth) * kTileSize);
            EXPECT_TRUE(zoo.Routes().Reachable(zoo.Map().EntranceTile(), spot.tile, NavProfile::Visitor()));
            if (depth == 0) {
                continue;
            }
            const auto* rail = zoo.Definitions().FenceByKind(zoo.Map().FenceOn(spot.tile, spot.side).kind);
            ASSERT_NE(rail, nullptr);
            EXPECT_EQ(rail->id, "stand_off_rail");
            TileCoord next = spot.tile + Step(spot.side);
            int crossed = 0;
            while (zoo.Map().InBounds(next) && zoo.Map().At(next).standOff) {
                EXPECT_EQ(zoo.Habitats().RegionAt(next), Region::StandOff);
                EXPECT_FALSE(zoo.Habitats().At(next));
                EXPECT_FALSE(zoo.Routes().Passable(next, NavProfile::Visitor()));
                ++crossed;
                next = next + Step(spot.side);
            }
            EXPECT_EQ(zoo.Habitats().At(next), home.id);
            EXPECT_FLOAT_EQ(spot.standOffM, static_cast<float>(crossed) * kTileSize);
            const auto* barrier =
                zoo.Definitions().FenceByKind(zoo.Map().FenceOn(next, Opposite(spot.side)).kind);
            ASSERT_NE(barrier, nullptr);
            EXPECT_EQ(barrier->id, FrontBarrier(home));
            EXPECT_FLOAT_EQ(spot.visibility, rail->visibility * barrier->visibility);
        }
        if (home.name == "Lion Rock" || home.name == "Tiger Forest" || home.name == "Bear Falls" ||
            home.name == "Polar Sound" || home.name == "Wolf Wood") {
            for (const TileCoord tile : TilesInRect(home.min, home.max)) {
                if (zoo.Habitats().At(tile) != home.id) {
                    continue;
                }
                for (const Dir side : {Dir::North, Dir::East, Dir::South, Dir::West}) {
                    const auto fence = zoo.Map().FenceOn(tile, side);
                    if (!fence.Exists()) {
                        continue;
                    }
                    const auto* def = zoo.Definitions().FenceByKind(fence.kind);
                    ASSERT_NE(def, nullptr);
                    EXPECT_NE(def->id, "steel_bars");
                    EXPECT_TRUE(def->style == FenceStyle::Glass || def->id == "heavy_mesh" ||
                                (home.name == "Wolf Wood" && def->id == "tall_mesh"));
                }
            }
        }
    }
    int marked = 0;
    int planted = 0;
    for (int z = 0; z < zoo.Map().Depth(); ++z) {
        for (int x = 0; x < zoo.Map().Width(); ++x) {
            if (zoo.Map().At({x, z}).standOff) {
                ++marked;
            }
        }
    }
    for (const Prop& prop : zoo.Objects().Props()) {
        if (zoo.Habitats().RegionAt(World::TileAt(prop.position)) == Region::StandOff) {
            ++planted;
            const auto& def = zoo.Definitions().props[static_cast<std::size_t>(prop.def)];
            EXPECT_TRUE(def.role == PropRole::Bush || def.role == PropRole::Rock);
            EXPECT_LE(def.heightM * prop.scale, 1.27f);
        }
    }
    EXPECT_GT(marked, 0);
    EXPECT_EQ(planted, marked);
    std::cout << "STRIPS seed=" << zoo.Config().seed << " tiles=" << marked << " props=" << planted << '\n';
}

// Recorded on the unmodified layout for both seeds; retain the same space margin,
// habitat welfare, and mean smoothed animal welfare after the 600-step fill.
struct HabitatBaseline {
    float area;
    float habitat;
    float filled;
};
const std::map<std::string, HabitatBaseline> kBefore{
    {"giraffe", {4284.0f, 0.964078784f, 0.949947774f}},
    {"zebra", {4284.0f, 0.978447199f, 0.945476532f}},
    {"ostrich", {1016.0f, 0.970645428f, 0.970760703f}},
    {"lion", {2236.0f, 0.970187545f, 0.938474238f}},
    {"tiger", {1788.0f, 0.976001620f, 0.902444363f}},
    {"wolf", {1788.0f, 0.978674769f, 0.927070796f}},
    {"brown_bear", {2236.0f, 0.969099939f, 0.976925611f}},
    {"polar_bear", {2812.0f, 0.962766051f, 0.976536632f}},
    {"elephant", {5436.0f, 0.952969968f, 0.937010646f}},
    {"rhino", {1692.0f, 0.976396203f, 0.969400227f}},
    {"camel", {1616.0f, 0.985593915f, 0.975913942f}},
    {"kangaroo", {1136.0f, 0.991231322f, 0.978923440f}},
};

void ExpectWelfare(Zoo& zoo, bool filled) {
    std::map<std::string, std::pair<float, int>> totals;
    for (const Animal& animal : zoo.Fauna().All()) {
        const auto& species = zoo.SpeciesList().All()[static_cast<std::size_t>(animal.species)];
        if (!kBefore.contains(species.id)) {
            continue;
        }
        const auto& before = kBefore.at(species.id);
        const auto& home = *zoo.Habitats().Find(animal.home);
        const auto welfare = EvaluateHabitat(species, home.stats, zoo.Fauna().GroupOf(zoo.MakeEnv(), animal));
        EXPECT_FLOAT_EQ(home.stats.areaM2, before.area) << species.id;
        EXPECT_GE(welfare.overall + 0.000001f, before.habitat) << species.id;
        EXPECT_FLOAT_EQ(welfare.space, 1.0f);
        EXPECT_GE(Containment(species, home.stats), 0.99999f);
        auto& total = totals[species.id];
        total.first += animal.welfare;
        ++total.second;
    }
    if (filled) {
        for (const auto& [species, total] : totals) {
            const float mean = total.first / static_cast<float>(total.second);
            EXPECT_GE(mean + 0.000001f, kBefore.at(species).filled) << species;
            std::cout << "WELFARE " << zoo.Config().seed << ' ' << species << ' ' << mean << '\n';
        }
    }
}

void ExpectSeparation(const Zoo& zoo) {
    for (const Animal& animal : zoo.Fauna().All()) {
        const auto* home = zoo.Habitats().Find(animal.home);
        ASSERT_NE(home, nullptr);
        const int depth = StripDepth(*home);
        if (depth == 0) {
            continue;
        }
        const TileCoord at = World::TileAt(animal.position);
        for (int z = at.z - depth - 1; z <= at.z + depth + 1; ++z) {
            for (int x = at.x - depth - 1; x <= at.x + depth + 1; ++x) {
                const TileCoord path{x, z};
                if (zoo.Map().InBounds(path) && zoo.Map().At(path).path != PathKind::None &&
                    zoo.Habitats().RegionAt(path) == Region::Public) {
                    EXPECT_GE(DistanceToTiles(animal.position, path, path),
                              static_cast<float>(depth) * kTileSize)
                        << home->name;
                }
            }
        }
    }
    for (const Visitor& guest : zoo.Guests().All()) {
        EXPECT_NE(zoo.Habitats().RegionAt(World::TileAt(guest.position)), Region::StandOff)
            << "visitor " << guest.id.value << " at " << guest.position.x << ',' << guest.position.z
            << " activity " << static_cast<int>(guest.activity) << " goal " << static_cast<int>(guest.goal)
            << " tick " << zoo.Time().tick;
    }
    for (const Litter& litter : zoo.Guests().LitterOnGround()) {
        EXPECT_NE(zoo.Habitats().RegionAt(World::TileAt(litter.position)), Region::StandOff);
    }
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
    ExpectFronts(zoo);
    ExpectWelfare(zoo, false);
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
    if (ticks == 600) {
        ExpectWelfare(zoo, true);
    }
    for (const Enclosure& home : zoo.Habitats().All()) {
        EXPECT_GT(zoo.Guests().Watching(home.id), 0) << home.name;
        bool watchingFront = false;
        for (const Visitor& guest : zoo.Guests().All()) {
            if (guest.activity != VisitorActivity::Watching || guest.targetEnclosure != home.id) {
                continue;
            }
            for (const ViewingSpot& spot : home.viewing) {
                watchingFront =
                    watchingFront || (spot.side == Dir::North && World::TileAt(guest.position) == spot.tile);
            }
        }
        // Straight after the short fill a group still stands at each front (SpreadVisitors
        // set it there); later it is a matter of minutes, checked over the steps below.
        if (ticks == 600) {
            EXPECT_TRUE(watchingFront) << home.name;
        }
        std::cout << "WATCHERS " << zoo.Config().seed << ' ' << home.name << ' '
                  << zoo.Guests().Watching(home.id) << '\n';
    }
    ExpectResidents(zoo);
    ExpectContained(zoo);
    std::map<std::uint32_t, int> streak;
    std::map<std::uint32_t, int> longest;
    std::map<std::string, float> deepest;
    std::set<std::uint32_t> frontWatched;
    for (int step = 0; step <= 2000; ++step) {
        ExpectSeparation(zoo);
        if (step % 50 == 0) {
            for (const Visitor& guest : zoo.Guests().All()) {
                if (guest.activity != VisitorActivity::Watching ||
                    frontWatched.contains(guest.targetEnclosure.value)) {
                    continue;
                }
                const Enclosure* home = zoo.Habitats().Find(guest.targetEnclosure);
                for (const ViewingSpot& spot : home != nullptr ? home->viewing : std::vector<ViewingSpot>{}) {
                    if (spot.side == Dir::North && World::TileAt(guest.position) == spot.tile) {
                        frontWatched.insert(home->id.value);
                        break;
                    }
                }
            }
        }
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
            for (const Animal& animal : zoo.Fauna().All()) {
                ASSERT_EQ(zoo.Habitats().At(animal.position), animal.home)
                    << animal.name << " left containment at visit step " << step;
            }
        }
    }
    for (const Enclosure& home : zoo.Habitats().All()) {
        EXPECT_TRUE(frontWatched.contains(home.id.value))
            << home.name << ": nobody at its front in 2,000 steps";
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

// A diagnostic, not a check: who watches where over an hour of a visit.
TEST_P(VisitorZooHabitat, DISABLED_PrintWatchersOverTime) {
    Zoo zoo(testdata::GameCatalog(), testdata::GameSpecies(), VisitorZooConfig(GetParam()));
    BuildVisitorZoo(zoo);
    FillForVisit(zoo, {.fillTicks = 6000});
    for (const Enclosure& home : zoo.Habitats().All()) {
        std::cout << home.name.substr(0, 7) << '\t';
    }
    std::cout << "people\n";
    for (int round = 0; round <= 12; ++round) {
        for (const Enclosure& home : zoo.Habitats().All()) {
            std::cout << zoo.Guests().Watching(home.id) << '\t';
        }
        std::cout << zoo.Guests().All().size() << '\n';
        if (round == 0 || round == 12) {
            for (const Enclosure& home : zoo.Habitats().All()) {
                int seen = 0;
                int all = 0;
                for (const Animal& animal : zoo.Fauna().All()) {
                    if (animal.home == home.id) {
                        ++all;
                        seen += animal.Visible() ? 1 : 0;
                    }
                }
                std::cout << home.viewing.size() << ':' << seen << '/' << all << '\t';
            }
            std::cout << "spots:visible/animals\n";
            std::array<int, static_cast<std::size_t>(VisitorActivity::Count)> doing{};
            std::array<int, static_cast<std::size_t>(VisitorGoal::Count)> walkingTo{};
            for (const Visitor& v : zoo.Guests().All()) {
                ++doing[static_cast<std::size_t>(v.activity)];
                if (v.activity == VisitorActivity::Walking) {
                    ++walkingTo[static_cast<std::size_t>(v.goal)];
                }
            }
            for (std::size_t k = 0; k < doing.size(); ++k) {
                std::cout << ToString(static_cast<VisitorActivity>(k)) << ' ' << doing[k] << "  ";
            }
            std::cout << "\nwalking to: ";
            for (std::size_t k = 0; k < walkingTo.size(); ++k) {
                std::cout << ToString(static_cast<VisitorGoal>(k)) << ' ' << walkingTo[k] << "  ";
            }
            std::cout << '\n';
        }
        for (int step = 0; step < 500; ++step) {
            zoo.Step();
        }
    }
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
    const std::filesystem::path directory = std::string(ZOO_SOURCE_DIR) + "/build-asan/tests";
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
