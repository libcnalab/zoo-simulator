// SPDX-License-Identifier: MIT
#include <gtest/gtest.h>

#include <chrono>
#include <iostream>
#include <map>
#include <set>

#include "TestData.hpp"
#include "zoo/sim/VisitorZoo.hpp"

namespace zoo {
namespace {
void ExpectNoProblems(const ScenarioReport& report) {
    for (const std::string& problem : report.problems) {
        ADD_FAILURE() << problem;
    }
}
void ExpectConnected(const Zoo& zoo) {
    const World& map = zoo.Map();
    int disconnected = 0;
    TileCoord first;
    for (int z = 0; z < map.Depth(); ++z) {
        for (int x = 0; x < map.Width(); ++x) {
            if (map.At({x, z}).path != PathKind::None &&
                !zoo.Routes().Reachable(map.EntranceTile(), {x, z}, NavProfile::Visitor())) {
                if (disconnected == 0) {
                    first = {x, z};
                }
                ++disconnected;
            }
        }
    }
    EXPECT_EQ(disconnected, 0) << "first disconnected path tile=" << first.x << ',' << first.z;
}
void ExpectPublicAccess(const Zoo& zoo) {
    std::vector<TileCoord> doors;
    for (const Building& building : zoo.Objects().Buildings()) {
        const BuildingDef& def = zoo.Definitions().buildings[static_cast<std::size_t>(building.def)];
        if (def.placement == Placement::Public) {
            doors.push_back(FrontTile(def, building.origin, building.facing));
            EXPECT_TRUE(zoo.Routes().Reachable(zoo.Map().EntranceTile(),
                                               FrontTile(def, building.origin, building.facing),
                                               NavProfile::Visitor()))
                << def.id;
        }
    }
    for (const Prop& prop : zoo.Objects().Props()) {
        const PropDef& def = zoo.Definitions().props[static_cast<std::size_t>(prop.def)];
        if (def.role != PropRole::Bench && def.role != PropRole::Lamp && def.role != PropRole::Bin &&
            def.role != PropRole::InfoBoard && def.id != "snake_pavilion") {
            EXPECT_EQ(zoo.Map().At(World::TileAt(prop.position)).path, PathKind::None) << def.id;
        }
    }
    std::vector<float> distances;
    zoo.Routes().DistanceField(doors, NavProfile::Visitor(), distances);
    float longest = 0.0f;
    TileCoord farthest;
    for (int z = 0; z < zoo.Map().Depth(); ++z) {
        for (int x = 0; x < zoo.Map().Width(); ++x) {
            const TileCoord tile{x, z};
            if (zoo.Map().IsProperty(tile) && zoo.Map().At(tile).path != PathKind::None) {
                const float metres =
                    distances[static_cast<std::size_t>(z * zoo.Map().Width() + x)] * kTileSize;
                if (metres > longest) {
                    longest = metres;
                    farthest = tile;
                }
            }
        }
    }
    std::cout << "Farthest walk to a facility: " << longest << " m at " << farthest.x << ',' << farthest.z
              << '\n';
    const int propertyDepth = zoo.Map().PropertyMax().z - zoo.Map().PropertyMin().z + 1;
    // A nine-sheet estate allows a 360 m (roughly four-and-a-half-minute) walk to the nearest
    // public service. Tighter limits encouraged empty service plazas instead of habitat land.
    const float maximumWalk = propertyDepth > 424 ? 360.0f : propertyDepth > 212 ? 170.0f : 85.0f;
    if (longest > maximumWalk) {
        for (const Enclosure& habitat : zoo.Habitats().All()) {
            float farthestView = 0.0f;
            for (const ViewingSpot& spot : habitat.viewing) {
                const auto index = static_cast<std::size_t>(spot.tile.z * zoo.Map().Width() + spot.tile.x);
                farthestView = std::max(farthestView, distances[index] * kTileSize);
            }
            std::cout << habitat.name << " farthest facility=" << farthestView << '\n';
        }
    }
    EXPECT_LE(longest, maximumWalk);
    const Vec2 entrance = World::TileCenter(zoo.Map().EntranceTile());
    float firstFence = 1.0e9f;
    for (const Enclosure& enclosure : zoo.Habitats().All()) {
        if (enclosure.name != "Meerkat Lookout") {
            continue;
        }
        for (const ViewingSpot& spot : enclosure.viewing) {
            const TileCoord step = Step(spot.side);
            const Vec2 fence =
                World::TileCenter(spot.tile) + Vec2{static_cast<float>(step.x), static_cast<float>(step.z)};
            firstFence = std::min(firstFence, Distance(fence, entrance));
        }
    }
    EXPECT_LE(firstFence, 45.0f);
}

void PrintPlan(const Zoo& zoo) {
    const World& map = zoo.Map();
    std::vector<char> marks(static_cast<std::size_t>(map.Width() * map.Depth()), '.');
    for (const Prop& prop : zoo.Objects().Props()) {
        const TileCoord tile = World::TileAt(prop.position);
        const PropRole role = zoo.Definitions().props[static_cast<std::size_t>(prop.def)].role;
        marks[static_cast<std::size_t>(tile.z * map.Width() + tile.x)] = role == PropRole::Tree   ? 't'
                                                                         : role == PropRole::Rock ? 'o'
                                                                                                  : '*';
    }
    // One character per 2x2 tiles: buildings, water, paths, props, fences, then habitat ids.
    for (int z = map.PropertyMin().z; z <= map.PropertyMax().z; z += 2) {
        for (int x = map.PropertyMin().x; x <= map.PropertyMax().x; x += 2) {
            char mark = '.';
            int priority = 0;
            for (int dz = 0; dz < 2; ++dz) {
                for (int dx = 0; dx < 2; ++dx) {
                    const TileCoord tile{x + dx, z + dz};
                    if (!map.InBounds(tile)) {
                        continue;
                    }
                    const auto& data = map.At(tile);
                    const EnclosureId enclosure = zoo.Habitats().At(tile);
                    char next = '.';
                    int rank = 0;
                    if (enclosure) {
                        next = static_cast<char>('A' + enclosure.value - 1);
                        rank = 1;
                    }
                    for (const Dir side : {Dir::North, Dir::South, Dir::East, Dir::West}) {
                        if (map.FenceOn(tile, side).Exists()) {
                            next = 'f';
                            rank = 2;
                        }
                    }
                    const char object = marks[static_cast<std::size_t>(tile.z * map.Width() + tile.x)];
                    if (object != '.') {
                        next = object;
                        rank = 3;
                    }
                    if (data.path != PathKind::None) {
                        constexpr char kinds[] = ".=+:~";
                        next = kinds[static_cast<int>(data.path)];
                        rank = 4;
                    }
                    if (map.HasWater(tile)) {
                        next = 'w';
                        rank = 5;
                    }
                    if (data.building) {
                        next = '#';
                        rank = 6;
                    }
                    if (rank > priority) {
                        mark = next;
                        priority = rank;
                    }
                }
            }
            std::cout << mark;
        }
        std::cout << '\n';
    }
    std::cout << "props=" << zoo.Objects().Props().size() << " buildings=" << zoo.Objects().Buildings().size()
              << " animals=" << zoo.AnimalCount() << '\n';
    for (const Enclosure& enclosure : zoo.Habitats().All()) {
        std::cout << enclosure.id.value << " " << enclosure.name << " area=" << enclosure.stats.areaM2
                  << " water=" << enclosure.stats.waterShare
                  << " plants=" << enclosure.stats.vegetationDensity
                  << " views=" << enclosure.stats.viewingTiles
                  << " shelter=" << enclosure.stats.shelterCapacity << " gates=" << enclosure.stats.gates
                  << " fence=" << enclosure.stats.minFenceHeightM << '/' << enclosure.stats.minFenceStrength;
        std::map<std::string, std::pair<int, int>> groups;
        for (const Animal& animal : zoo.Fauna().All()) {
            if (animal.home == enclosure.id) {
                auto& group = groups[zoo.SpeciesList().All()[static_cast<std::size_t>(animal.species)].id];
                if (animal.sex == Sex::Male) {
                    ++group.first;
                } else {
                    ++group.second;
                }
            }
        }
        for (const auto& [name, group] : groups) {
            std::cout << ' ' << name << '=' << group.first << "M+" << group.second << 'F';
        }
        std::cout << '\n';
    }
}
}  // namespace
TEST(VisitorZooTests, CompleteZoo) {
    Zoo zoo(testdata::GameCatalog(), testdata::GameSpecies(), VisitorZooConfig());
    EXPECT_EQ(zoo.Map().Width(), 1284);
    EXPECT_EQ(zoo.Map().Depth(), 648);
    ExpectNoProblems(BuildVisitorZoo(zoo));
    ExpectConnected(zoo);
    ExpectPublicAccess(zoo);
    EXPECT_EQ(zoo.Habitats().All().size(), 121U);
    std::set<int> species;
    const Env env = zoo.MakeEnv();
    for (const Animal& animal : zoo.Fauna().All()) {
        species.insert(animal.species);
        const Enclosure* enclosure = zoo.Habitats().Find(zoo.Habitats().At(animal.position));
        ASSERT_NE(enclosure, nullptr);
        const SpeciesDef& def = zoo.SpeciesList().All()[static_cast<std::size_t>(animal.species)];
        const GroupInfo group = zoo.Fauna().GroupOf(env, animal);
        const WelfareReport welfare = EvaluateHabitat(def, enclosure->stats, group);
        EXPECT_GE(group.sameSpecies, def.groupMin) << def.id;
        EXPECT_LE(group.sameSpecies, def.groupMax) << def.id;
        if (def.maxAdultMales > 0) {
            EXPECT_LE(group.adultMales, def.maxAdultMales) << def.id;
        }
        EXPECT_GE(welfare.space, 0.99f) << def.id;
        EXPECT_GE(welfare.water, 0.99f) << def.id;
        EXPECT_GE(welfare.terrain, 0.8f) << def.id;
        EXPECT_GE(welfare.vegetation, 0.95f) << def.id;
        EXPECT_GE(welfare.enrichment, 0.95f) << def.id;
        EXPECT_GE(Containment(def, enclosure->stats), 0.99f) << def.id;
        EXPECT_GE(enclosure->stats.shelterCapacity, group.allAnimals) << def.id;
    }
    EXPECT_EQ(species.size(), 52U);
    PrintPlan(zoo);
    for (const Enclosure& enclosure : zoo.Habitats().All()) {
        EXPECT_GE(enclosure.stats.gates, 1) << enclosure.name;
        EXPECT_GT(enclosure.stats.viewingTiles, 12) << enclosure.name;
    }
    ASSERT_FALSE(HasFailure()) << "static layout checks must pass before the long welfare run";
    std::cout << std::flush;
    for (int tick = 0; tick < kTicksPerGameHour * 6; ++tick) {
        zoo.Step();
        if ((tick + 1) % kTicksPerGameHour == 0) {
            std::cout << "Welfare check: hour " << zoo.Time().Hour() << std::endl;
        }
    }
    for (const Animal& animal : zoo.Fauna().All()) {
        EXPECT_FALSE(animal.loose) << animal.name;
        EXPECT_GT(animal.welfare, 0.6f)
            << zoo.SpeciesList().All()[static_cast<std::size_t>(animal.species)].id << ' ' << animal.name;
    }
}

TEST(VisitorZooTests, SpeciesCanBeOmittedWithoutRemovingTheirHabitat) {
    Zoo zoo(testdata::GameCatalog(), testdata::GameSpecies(), VisitorZooConfig());
    ExpectNoProblems(BuildVisitorZoo(zoo, {{"flamingo"}}));
    EXPECT_EQ(zoo.AnimalCount(zoo.SpeciesList().IndexOf("flamingo")), 0);
    EXPECT_EQ(zoo.Habitats().All().size(), 121U);
    bool lagoon = false;
    for (const Enclosure& enclosure : zoo.Habitats().All()) {
        if (enclosure.name == "Flamingo Lagoon") {
            lagoon = true;
            EXPECT_GT(enclosure.stats.waterShare, 0.5f);
            EXPECT_GT(enclosure.stats.feeders, 0);
            EXPECT_GE(enclosure.stats.shelterCapacity, 12);
        }
    }
    EXPECT_TRUE(lagoon);
}

TEST(VisitorZooTests, EqualSeedsMatchAndOtherSeedsBuild) {
    Zoo first(testdata::GameCatalog(), testdata::GameSpecies(), VisitorZooConfig());
    Zoo second(testdata::GameCatalog(), testdata::GameSpecies(), VisitorZooConfig());
    ExpectNoProblems(BuildVisitorZoo(first));
    ExpectNoProblems(BuildVisitorZoo(second));
    EXPECT_TRUE(first.SameState(second));
    EXPECT_EQ(first.StateHash(), second.StateHash());
    for (const std::uint64_t seed : {2U, 3U, 7U}) {
        Zoo other(testdata::GameCatalog(), testdata::GameSpecies(), VisitorZooConfig(seed));
        ExpectNoProblems(BuildVisitorZoo(other));
        ExpectConnected(other);
        EXPECT_NE(first.StateHash(), other.StateHash());
    }
}

TEST(VisitorZooTests, PreviousMapWidthRetainsTheNorthernCircuit) {
    ZooConfig config = VisitorZooConfig();
    config.world.width = 160;
    Zoo zoo(testdata::GameCatalog(), testdata::GameSpecies(), config);
    ExpectNoProblems(BuildVisitorZoo(zoo));
    ExpectConnected(zoo);
    EXPECT_EQ(zoo.Habitats().All().size(), 17U);
    EXPECT_EQ(zoo.Fauna().All().size(), 99U);
}

TEST(VisitorZooTests, NineSheetPropertyKeepsTheEstablishedExpansionStocked) {
    ZooConfig previous = VisitorZooConfig();
    previous.world.width = 224;
    previous.world.depth = 224;
    Zoo old(testdata::GameCatalog(), testdata::GameSpecies(), previous);
    ExpectNoProblems(BuildVisitorZoo(old));
    ASSERT_EQ(old.Habitats().All().size(), 22U);

    Zoo expanded(testdata::GameCatalog(), testdata::GameSpecies(), VisitorZooConfig());
    ExpectNoProblems(BuildVisitorZoo(expanded));
    const World& map = expanded.Map();
    const int oldWidth = old.Map().PropertyMax().x - old.Map().PropertyMin().x + 1;
    const int oldDepth = old.Map().PropertyMax().z - old.Map().PropertyMin().z + 1;
    EXPECT_EQ(map.PropertyMax().x - map.PropertyMin().x + 1, 6 * oldWidth);
    EXPECT_EQ(map.PropertyMax().z - map.PropertyMin().z + 1, 3 * oldDepth);
    const int shift = map.EntranceTile().x - old.Map().EntranceTile().x;
    for (const std::string id : {"przewalski_horse", "kiang"}) {
        const int species = expanded.SpeciesList().IndexOf(id);
        // The continental estate adds one second five-animal conservation group for each equid.
        ASSERT_EQ(expanded.AnimalCount(species), 10) << id;
        std::set<EnclosureId> homes;
        bool establishedRangeStocked = false;
        for (const Animal& animal : expanded.Fauna().All()) {
            if (animal.species != species) {
                continue;
            }
            const Enclosure* home = expanded.Habitats().Find(animal.home);
            ASSERT_NE(home, nullptr);
            homes.insert(animal.home);
            if (id == "kiang") {
                establishedRangeStocked =
                    establishedRangeStocked || home->max.x < old.Map().PropertyMin().x + shift;
            } else {
                establishedRangeStocked =
                    establishedRangeStocked || home->min.x > old.Map().PropertyMax().x + shift;
            }
            ASSERT_FALSE(home->viewing.empty());
            EXPECT_TRUE(expanded.Routes().Reachable(map.EntranceTile(), home->viewing.front().tile,
                                                    NavProfile::Visitor()));
        }
        EXPECT_EQ(homes.size(), 2U) << id;
        EXPECT_TRUE(establishedRangeStocked) << id;
    }
}

TEST(VisitorZooTests, SmallMapsReportTheirLimits) {
    ZooConfig config = VisitorZooConfig();
    config.world.width = 48;
    config.world.depth = 48;
    Zoo zoo(testdata::GameCatalog(), testdata::GameSpecies(), config);
    const ScenarioReport report = BuildVisitorZoo(zoo);
    EXPECT_FALSE(report.problems.empty());
    EXPECT_EQ(report.animals, 0);
}

// The full-day diagnostic is expensive under ASan; keep the crowd acceptance checks visible.
TEST(VisitorZooTests, DISABLED_MorningAndCrowdFigures) {
    Zoo zoo(testdata::GameCatalog(), testdata::GameSpecies(), VisitorZooConfig());
    const auto started = std::chrono::steady_clock::now();
    ExpectNoProblems(BuildVisitorZoo(zoo));
    ASSERT_FALSE(HasFailure());
    const auto built = std::chrono::steady_clock::now();
    std::cout << "Crowd build seconds=" << std::chrono::duration<double>(built - started).count()
              << std::endl;
    std::map<std::uint32_t, std::pair<Vec2, int>> walking;
    int longestStill = 0;
    for (int tick = 1; tick <= kTicksPerGameHour * 8; ++tick) {
        zoo.Step();
        for (const Visitor& visitor : zoo.Guests().All()) {
            auto& previous = walking[visitor.id.value];
            if (visitor.activity == VisitorActivity::Walking &&
                DistanceSquared(visitor.position, previous.first) < 0.000001f) {
                ++previous.second;
                longestStill = std::max(longestStill, previous.second);
            } else {
                previous.second = 0;
            }
            previous.first = visitor.position;
        }
        if (tick != kTicksPerGameHour * 3 && tick != kTicksPerGameHour * 5 && tick != kTicksPerGameHour * 8) {
            continue;
        }
        int inside = 0;
        int resting = 0;
        std::set<std::uint32_t> watched;
        for (const Visitor& visitor : zoo.Guests().All()) {
            if (visitor.ticketPaid &&
                zoo.Habitats().RegionAt(World::TileAt(visitor.position)) == Region::Public) {
                ++inside;
            }
            if (visitor.activity == VisitorActivity::Watching) {
                watched.insert(visitor.targetEnclosure.value);
            }
            if (visitor.activity == VisitorActivity::Eating || visitor.activity == VisitorActivity::Sitting) {
                ++resting;
            }
        }
        std::map<std::string, int> doing;
        std::map<std::uint32_t, int> watchers;
        for (const Visitor& visitor : zoo.Guests().All()) {
            ++doing[std::string(ToString(visitor.activity)) + "/" + std::string(ToString(visitor.goal))];
            if (visitor.activity == VisitorActivity::Watching) {
                ++watchers[visitor.targetEnclosure.value];
            }
        }
        for (const auto& [what, count] : doing) {
            std::cout << "  " << what << "=" << count;
        }
        std::cout << "\n  watchers by enclosure:";
        for (const auto& [id, count] : watchers) {
            std::cout << " " << id << ":" << count;
        }
        std::cout << std::endl;
        std::cout << "hour=" << zoo.Time().Hour() << " visitors=" << zoo.Guests().Count()
                  << " inside=" << inside << " watched=" << watched.size() << " sitting/eating=" << resting
                  << " mean_happiness=" << zoo.Guests().MeanHappiness() << " rating=" << zoo.Guests().Rating()
                  << std::endl;
        if (tick == kTicksPerGameHour * 3) {
            EXPECT_GE(inside, 60);
            EXPECT_GE(watched.size(), 5U);
            EXPECT_GT(resting, 0);
        }
    }
    EXPECT_LE(longestStill, kTicksPerGameHour);
    const auto finished = std::chrono::steady_clock::now();
    std::cout << "Build seconds=" << std::chrono::duration<double>(built - started).count()
              << " simulation seconds=" << std::chrono::duration<double>(finished - built).count()
              << " longest Walking standstill ticks=" << longestStill << std::endl;
}

TEST(VisitorZooTests, DISABLED_PrintPlan) {
    Zoo zoo(testdata::GameCatalog(), testdata::GameSpecies(), VisitorZooConfig());
    ExpectNoProblems(BuildVisitorZoo(zoo));
    ExpectConnected(zoo);
    ExpectPublicAccess(zoo);
    PrintPlan(zoo);
}
}  // namespace zoo
