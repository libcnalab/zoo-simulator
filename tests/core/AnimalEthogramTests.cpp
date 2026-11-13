// SPDX-License-Identifier: MIT
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <set>

#include "zoo/animals/AnimalInternal.hpp"
#include "zoo/animals/AnimalsJson.hpp"

namespace zoo {
namespace {

// A kept habitat: four adult females, food/water renewed hourly, mild weather.
// The same fixture is used before and after behaviour changes; needs are never reset.
struct HabitatDay {
    World world{40, 40};
    ZooObjects objects;
    Catalog catalog = Catalog::LoadDirectory(std::string(ZOO_SOURCE_DIR) + "/data/construction");
    SpeciesCatalog species = SpeciesCatalog::LoadDirectory(std::string(ZOO_SOURCE_DIR) + "/data/species");
    Enclosures enclosures;
    Navigator nav{world};
    Animals animals{831U};
    Env env{world, objects, enclosures, catalog, species, nav, nullptr, nullptr, {}, {}};
    std::vector<PropId> supplies;
    int speciesIndex;
    int population = 4;

    explicit HabitatDay(std::string_view name, bool legacy = false) : speciesIndex(species.IndexOf(name)) {
        if (legacy) {
            SpeciesDef legacyDef = species.All()[static_cast<std::size_t>(speciesIndex)];
            legacyDef.id = "legacy_" + legacyDef.id;
            EXPECT_TRUE(species.Add(legacyDef));
            speciesIndex = species.IndexOf(legacyDef.id);
        }
        const SpeciesDef& def = species.All()[static_cast<std::size_t>(speciesIndex)];
        population = std::min(4, def.groupMax);
        const auto best = std::max_element(def.terrainSuitability.begin(), def.terrainSuitability.end());
        const auto terrain = static_cast<Terrain>(best - def.terrainSuitability.begin());
        const auto fence = static_cast<std::uint8_t>(catalog.FindFence("high_cable_barrier")->kind);
        for (int x = 8; x <= 30; ++x) {
            world.SetFence({x, 8, true}, {fence, false, 1.0f});
            world.SetFence({x, 31, true}, {fence, false, 1.0f});
        }
        for (int z = 8; z <= 30; ++z) {
            world.SetFence({8, z, false}, {fence, false, 1.0f});
            world.SetFence({31, z, false}, {fence, false, 1.0f});
            for (int x = 8; x <= 30; ++x) {
                world.SetTerrain({x, z}, terrain);
            }
        }
        for (int z = 20; z <= 26; ++z) {
            for (int x = 23; x <= 28; ++x) {
                const bool shore = x == 23 || x == 28 || z == 20 || z == 26;
                const float depth = name == "flamingo" ? 0.15f : (shore ? 0.2f : 1.5f);
                world.SetWater({x, z}, depth, depth);
            }
        }
        for (int index = 0; index < 4; ++index) {
            Prop feeder;
            for (std::size_t propIndex = 0; propIndex < catalog.props.size(); ++propIndex) {
                const auto& prop = catalog.props[propIndex];
                if (prop.role == PropRole::Feeder &&
                    std::find(prop.foods.begin(), prop.foods.end(), def.foods.front()) != prop.foods.end()) {
                    feeder.def = static_cast<int>(propIndex);
                    break;
                }
            }
            feeder.position = World::TileCenter({12 + index * 4, 12});
            feeder.food = catalog.FoodIndex(def.foods.front());
            supplies.push_back(objects.AddProp(feeder));
            Prop trough;
            trough.def = catalog.PropIndex("water_trough");
            trough.position = World::TileCenter({12 + index * 4, 28});
            supplies.push_back(objects.AddProp(trough));
        }
        Prop tree;
        tree.def = catalog.PropIndex("acacia");
        tree.position = World::TileCenter({18, 23});
        objects.AddProp(tree);
        enclosures.Rebuild(world, objects, catalog);
        env.weather.temperatureC = (def.tempMinC + def.tempMaxC) * 0.5f;
        for (int index = 0; index < population; ++index) {
            const AnimalId id = animals.Add(env, speciesIndex, Sex::Female, def.maturityYears + 1.0f,
                                            World::TileCenter({14 + index * 3, 17}));
            EXPECT_TRUE(id);
            if (Animal* animal = animals.Find(id)) {
                animal->breeding = false;
            }
        }
        Supply();
    }

    void Supply() {
        for (PropId id : supplies) {
            Prop* prop = objects.FindProp(id);
            prop->amount = 10000.0f;
            prop->filledDay = static_cast<float>(env.time.Days());
        }
        for (const Enclosure& enclosure : enclosures.All()) {
            animals.Clean(enclosure.id, 1.0f);
        }
    }
};

struct DayMetrics {
    std::array<double, static_cast<std::size_t>(AnimalActivity::Count)> budget{};
    double moving = 0.0;
    double active = 0.0;
    double sentinel = 0.0;
    double daylight = 0.0;
    double spread = 0.0;
    double neighbour = 0.0;
    double tiles = 0.0;
    double changesPerHour = 0.0;
    double motionlessSeconds = 0.0;
    double meanHunger = 0.0;
    double meanStress = 0.0;
    double criticalNeeds = 0.0;
    double hunger = 0.0;
    double stress = 0.0;
    double health = 0.0;
    int sick = 0;
};

DayMetrics Measure(HabitatDay& habitat, int days = 1) {
    DayMetrics result;
    std::set<std::pair<int, int>> visited;
    std::array<Vec2, 4> previous{};
    std::array<float, 4> heading{};
    std::array<int, 4> still{};
    std::array<AnimalActivity, 4> activity{};
    double samples = 0.0;
    for (int tick = 0; tick < days * kTicksPerDay; ++tick) {
        habitat.env.time.tick = tick;
        if (tick % kTicksPerGameHour == 0) {
            habitat.Supply();
        }
        habitat.animals.Step(habitat.env);
        if (habitat.env.time.Hour() >= 6.0f && habitat.env.time.Hour() < 20.0f) {
            ++result.daylight;
            for (const Animal& member : habitat.animals.All()) {
                if (member.activity == AnimalActivity::Sentinel && !member.moving) {
                    ++result.sentinel;
                    break;
                }
            }
        }
        Vec2 center{};
        for (const Animal& animal : habitat.animals.All()) {
            center = center + animal.position * (1.0f / static_cast<float>(habitat.population));
        }
        std::size_t index = 0;
        for (const Animal& animal : habitat.animals.All()) {
            ++result.budget[static_cast<std::size_t>(animal.activity)];
            result.meanHunger += animal.hunger;
            result.meanStress += animal.stress;
            result.criticalNeeds += animal.hunger > 0.75f || animal.thirst > 0.75f ? 1.0 : 0.0;
            result.moving += animal.moving ? 1.0 : 0.0;
            result.active += animal.moving || animal_internal::FeedingActivity(animal.activity) ? 1.0 : 0.0;
            result.spread += Length(animal.position - center);
            float nearest = 1000.0f;
            for (const Animal& other : habitat.animals.All()) {
                if (other.id != animal.id) {
                    nearest = std::min(nearest, Length(animal.position - other.position));
                }
            }
            result.neighbour += nearest;
            const TileCoord tile = World::TileAt(animal.position);
            visited.emplace(tile.x, tile.z);
            if (tick > 0 && animal.activity != activity[index]) {
                result.changesPerHour += 1.0;
            }
            if (animal.activity != AnimalActivity::Sleep && !animal.shelter &&
                DistanceSquared(previous[index], animal.position) < 0.000001f &&
                std::abs(heading[index] - animal.heading) < 0.0001f) {
                ++still[index];
                result.motionlessSeconds = std::max(result.motionlessSeconds, still[index] * 0.05);
            } else {
                still[index] = 0;
            }
            previous[index] = animal.position;
            heading[index] = animal.heading;
            activity[index] = animal.activity;
            ++index;
            ++samples;
        }
    }
    for (double& share : result.budget) {
        share /= samples;
    }
    result.meanHunger /= samples;
    result.meanStress /= samples;
    result.criticalNeeds /= samples;
    result.moving /= samples;
    result.active /= samples;
    result.sentinel /= std::max(1.0, result.daylight);
    result.spread /= samples;
    result.neighbour /= samples;
    result.tiles = static_cast<double>(visited.size()) / (23.0 * 23.0);
    result.changesPerHour /= static_cast<double>(habitat.population) * days * 24.0;
    for (const Animal& animal : habitat.animals.All()) {
        result.hunger += animal.hunger / static_cast<double>(habitat.population);
        result.stress += animal.stress / static_cast<double>(habitat.population);
        result.health += animal.health / static_cast<double>(habitat.population);
        result.sick += animal.sick ? 1 : 0;
    }
    return result;
}

void PrintMetrics(std::string_view name, const DayMetrics& metrics) {
    std::cout << "BUDGET " << name << std::fixed << std::setprecision(3);
    for (std::size_t index = 0; index < metrics.budget.size(); ++index) {
        if (metrics.budget[index] > 0.0) {
            std::cout << ' ' << ToString(static_cast<AnimalActivity>(index)) << '=' << metrics.budget[index];
        }
    }
    std::cout << " active=" << metrics.active << " posted=" << metrics.sentinel
              << " moving=" << metrics.moving << " still=" << metrics.motionlessSeconds
              << " spread=" << metrics.spread << " neighbour=" << metrics.neighbour
              << " tiles=" << metrics.tiles << " changes/h=" << metrics.changesPerHour
              << " mean_hunger=" << metrics.meanHunger << " mean_stress=" << metrics.meanStress
              << " critical=" << metrics.criticalNeeds << " hunger=" << metrics.hunger
              << " stress=" << metrics.stress << " health=" << metrics.health << " sick=" << metrics.sick
              << std::endl;
}

TEST(AnimalWelfareTests, ChangingRoutineDoesNotRerollIllness) {
    HabitatDay current("lion");
    HabitatDay legacy("lion", true);
    bool sawIllness = false;
    for (int hour = 0; hour < 24 * 30; ++hour) {
        for (HabitatDay* habitat : {&current, &legacy}) {
            habitat->env.time.tick = hour * kTicksPerGameHour;
            for (const Animal& member : habitat->animals.All()) {
                Animal* animal = habitat->animals.Find(member.id);
                animal->welfare = 0.1f;
                animal->hunger = 0.8f;
                animal->thirst = 0.8f;
            }
            habitat->animals.Step(habitat->env);
        }
        ASSERT_EQ(current.animals.All().size(), legacy.animals.All().size());
        for (std::size_t index = 0; index < current.animals.All().size(); ++index) {
            const Animal& first = current.animals.All()[index];
            const Animal& second = legacy.animals.All()[index];
            EXPECT_EQ(first.sick, second.sick);
            EXPECT_EQ(first.sickSinceTick, second.sickSinceTick);
            sawIllness = sawIllness || first.sick;
        }
    }
    EXPECT_TRUE(sawIllness);
}

TEST(AnimalWelfareTests, ForagingDoesNotRaiseAlreadySatisfiedHunger) {
    HabitatDay habitat("zebra");
    Animal* animal = habitat.animals.Find(AnimalId{1U});
    animal->hunger = 0.05f;
    animal->thirst = 0.0f;
    animal->activity = AnimalActivity::Forage;
    animal->moving = false;
    animal->activityLeft = 100;
    habitat.animals.Step(habitat.env);
    EXPECT_LT(habitat.animals.Find(AnimalId{1U})->hunger, 0.051f);
}

TEST(AnimalWelfareTests, SameSpeciesCarnivoresDoNotCausePredatorStress) {
    HabitatDay habitat("lion");
    for (const Animal& member : habitat.animals.All()) {
        habitat.animals.Find(member.id)->welfare = 0.9f;
        habitat.animals.Find(member.id)->stress = 0.0f;
    }
    for (int tick = 0; tick < kTicksPerGameHour / 2; ++tick) {
        habitat.env.time.tick = tick;
        habitat.animals.Step(habitat.env);
    }
    for (const Animal& animal : habitat.animals.All()) {
        EXPECT_LT(animal.stress, 0.07f);
    }
}

TEST(AnimalEthogramTests, SpeciesBudgetsCohesionAndAwakeMotion) {
    for (const std::string_view name : {"lion", "elephant", "flamingo", "meerkat"}) {
        HabitatDay habitat(name);
        const DayMetrics metrics = Measure(habitat);
        PrintMetrics(name, metrics);
        EXPECT_EQ(habitat.animals.Deaths(), 0);
        EXPECT_EQ(habitat.animals.Escapes(), 0);
        const auto* profile = animal_internal::ProfileFor(
            habitat.species.All()[static_cast<std::size_t>(habitat.speciesIndex)]);
        ASSERT_NE(profile, nullptr);
        EXPECT_LE(metrics.motionlessSeconds, profile->awakeStillSeconds + 0.1) << name;
        EXPECT_GT(metrics.tiles, 0.01) << name;
        EXPECT_LT(metrics.changesPerHour, 12.0) << name;
        if (name == "lion") {
            EXPECT_GT(metrics.budget[static_cast<std::size_t>(AnimalActivity::Rest)] +
                          metrics.budget[static_cast<std::size_t>(AnimalActivity::Sleep)],
                      0.55);
        } else if (name == "elephant") {
            EXPECT_GT(metrics.active, 0.50);
        } else if (name == "flamingo") {
            EXPECT_LT(metrics.neighbour, 5.0);
        } else if (name == "meerkat") {
            EXPECT_GT(metrics.sentinel, 0.80);
        }
    }
}

TEST(AnimalEthogramTests, RoutineStateAndEveryAppendedActivityRoundTripAndAffectHash) {
    HabitatDay habitat("wolf");
    Animal* animal = habitat.animals.Find(AnimalId{1U});
    ASSERT_NE(animal, nullptr);
    animal->nextNeedCheck = 321;
    animal->answeredCallTick = 123;
    animal->lastCallTick = 127;
    animal->attentionHeading = 1.2f;
    animal->attentionLeft = 27;
    animal->stillTicks = 33;
    animal->following = AnimalId{2U};
    for (int index = static_cast<int>(AnimalActivity::Browse);
         index < static_cast<int>(AnimalActivity::Count); ++index) {
        animal->activity = static_cast<AnimalActivity>(index);
        const auto encoded = ToJson(habitat.animals, habitat.species);
        std::string error;
        const auto restored = AnimalsFromJson(encoded, habitat.species, &error);
        ASSERT_TRUE(restored) << error;
        EXPECT_EQ(habitat.animals, *restored);
        EXPECT_EQ(habitat.animals.StateHash(), restored->StateHash());
    }
    const auto original = ToJson(habitat.animals, habitat.species);
    for (const char* field : {"next_need_check", "answered_call_tick", "last_call_tick", "attention_heading",
                              "attention_left", "still_ticks", "following"}) {
        auto changed = original;
        changed["animals"][0][field] = 0U;
        const auto restored = AnimalsFromJson(changed, habitat.species);
        ASSERT_TRUE(restored) << field;
        EXPECT_NE(habitat.animals.StateHash(), restored->StateHash()) << field;
    }
    auto old = original;
    for (auto& entry : old["animals"]) {
        for (const char* field : {"next_need_check", "answered_call_tick", "last_call_tick",
                                  "attention_heading", "attention_left", "still_ticks", "following"}) {
            entry.erase(field);
        }
        entry["activity"] = "idle";
    }
    const auto restored = AnimalsFromJson(old, habitat.species);
    ASSERT_TRUE(restored);
    EXPECT_EQ(restored->All().front().lastCallTick, -1);
    EXPECT_FALSE(restored->All().front().following);
}

TEST(AnimalEthogramTests, HabitatWaterShadeAndHighGroundAreUsedWithoutUnsafeTargets) {
    for (const std::string_view name : {"flamingo", "polar_bear", "lion", "meerkat"}) {
        HabitatDay habitat(name);
        const auto& species = habitat.species.All()[static_cast<std::size_t>(habitat.speciesIndex)];
        const auto* profile = animal_internal::ProfileFor(species);
        ASSERT_NE(profile, nullptr);
        Animal& animal = *habitat.animals.Find(AnimalId{1U});
        std::vector<std::size_t> members;
        for (int index = 0; index < habitat.population; ++index) {
            members.push_back(static_cast<std::size_t>(index));
        }
        std::vector<PropId> scratch;
        AnimalActivity activity = name == "flamingo"     ? AnimalActivity::FilterFeed
                                  : name == "polar_bear" ? AnimalActivity::Bathe
                                  : name == "meerkat"    ? AnimalActivity::Sentinel
                                                         : AnimalActivity::Rest;
        habitat.env.time.shift = 5 * kTicksPerGameHour;
        if (name == "meerkat") {
            for (int z = 14; z <= 19; ++z) {
                for (int x = 14; x <= 19; ++x) {
                    habitat.world.SetVertexHeight(x, z, 4.0f);
                }
            }
            animal.position = World::TileCenter({20, 16});
        }
        const Vec2 target = animal_internal::RoutinePlace(animal, species, *profile, activity,
                                                          habitat.enclosures.All().front(), habitat.env,
                                                          habitat.animals.All(), members, scratch);
        EXPECT_EQ(habitat.enclosures.At(target), animal.home);
        if (name == "flamingo" || name == "polar_bear") {
            EXPECT_GT(habitat.world.WaterDepthAt(target), 0.0f) << name;
        } else {
            EXPECT_FALSE(habitat.world.HasWater(World::TileAt(target)));
        }
        if (name == "meerkat") {
            EXPECT_GT(habitat.world.HeightAt(target), 3.0f);
        }
        if (name == "lion") {
            EXPECT_LT(Length(target - World::TileCenter({18, 23})), 5.0f);
        }
    }
}

TEST(AnimalEthogramTests, NonSwimmerRoutesAroundEvenShallowWater) {
    HabitatDay habitat("kangaroo");
    Animal* animal = habitat.animals.Find(AnimalId{1U});
    animal->position = World::TileCenter({22, 20});
    animal->target = World::TileCenter({29, 20});
    animal->activity = AnimalActivity::Wander;
    animal->moving = true;
    animal->activityLeft = 20 * kTicksPerSecond;
    animal->hunger = 0.0f;
    animal->thirst = 0.0f;
    bool arrived = false;
    for (int tick = 0; tick < 45 * kTicksPerSecond; ++tick) {
        habitat.env.time.tick = tick;
        habitat.animals.Step(habitat.env);
        animal = habitat.animals.Find(AnimalId{1U});
        ASSERT_NE(animal, nullptr);
        EXPECT_FALSE(habitat.world.HasWater(World::TileAt(animal->position)));
        if (!animal->moving) {
            arrived = true;
            break;
        }
    }
    EXPECT_TRUE(arrived);
    EXPECT_LT(Length(animal->position - World::TileCenter({29, 20})), 1.0f);
}

TEST(AnimalEthogramTests, CallsAreAnsweredOnceAndJuvenileFollowsLivingMother) {
    HabitatDay habitat("wolf");
    habitat.env.time.shift = 10 * kTicksPerGameHour;
    std::vector<Event> events;
    habitat.env.events = &events;
    for (int tick = 0; tick < 12 * kTicksPerSecond; ++tick) {
        habitat.env.time.tick = tick;
        habitat.animals.Step(habitat.env);
    }
    std::array<int, 4> calls{};
    for (const Event& event : events) {
        if (event.type == EventType::AnimalCalled) {
            ++calls[event.animal.value - 1U];
        }
    }
    for (int count : calls) {
        EXPECT_EQ(count, 1);
    }
    Animal* child = habitat.animals.Find(AnimalId{4U});
    child->mother = AnimalId{1U};
    child->bornTick = habitat.env.time.tick;
    child->position = World::TileCenter({27, 16});
    child->activity = AnimalActivity::Idle;
    child->moving = false;
    child->activityLeft = 0;
    const float before = Length(child->position - habitat.animals.Find(child->mother)->position);
    for (int tick = 0; tick < 30 * kTicksPerSecond; ++tick) {
        ++habitat.env.time.tick;
        habitat.animals.Step(habitat.env);
    }
    child = habitat.animals.Find(AnimalId{4U});
    EXPECT_EQ(child->following, child->mother);
    EXPECT_LT(Length(child->position - habitat.animals.Find(child->mother)->position), before);
    const AnimalId mother = child->mother;
    EXPECT_TRUE(habitat.animals.Crate(mother));
    for (int tick = 0; tick < 2 * kTicksPerSecond; ++tick) {
        ++habitat.env.time.tick;
        habitat.animals.Step(habitat.env);
    }
    EXPECT_FALSE(habitat.animals.Find(AnimalId{4U})->following);
    EXPECT_TRUE(habitat.animals.Remove(mother));
    EXPECT_FALSE(habitat.animals.Find(AnimalId{4U})->following);
}

TEST(AnimalEthogramTests, DISABLED_MeasureAllSpecies) {
    const auto species = SpeciesCatalog::LoadDirectory(std::string(ZOO_SOURCE_DIR) + "/data/species");
    for (const SpeciesDef& def : species.All()) {
        HabitatDay habitat(def.id);
        const DayMetrics metrics = Measure(habitat);
        PrintMetrics(def.id, metrics);
        const auto* profile = animal_internal::ProfileFor(def);
        ASSERT_NE(profile, nullptr);
        EXPECT_LE(metrics.motionlessSeconds, profile->awakeStillSeconds + 0.1) << def.id;
        EXPECT_EQ(habitat.animals.Deaths(), 0);
    }
}

TEST(AnimalEthogramTests, DISABLED_MeasureLegacy) {
    const auto species = SpeciesCatalog::LoadDirectory(std::string(ZOO_SOURCE_DIR) + "/data/species");
    for (const SpeciesDef& def : species.All()) {
        HabitatDay habitat(def.id, true);
        PrintMetrics(def.id, Measure(habitat));
        EXPECT_EQ(habitat.animals.Deaths(), 0);
    }
}

void CheckKeptWeek(std::string_view speciesName) {
    HabitatDay legacy(speciesName, true);
    const DayMetrics before = Measure(legacy, 7);
    PrintMetrics("legacy_" + std::string(speciesName), before);
    HabitatDay habitat(speciesName);
    const DayMetrics after = Measure(habitat, 7);
    PrintMetrics(speciesName, after);
    EXPECT_EQ(habitat.animals.Deaths(), 0) << speciesName;
    EXPECT_EQ(habitat.animals.Escapes(), 0) << speciesName;
    EXPECT_LE(after.meanHunger, before.meanHunger + 0.05) << speciesName;
    EXPECT_LE(after.meanStress, before.meanStress + 0.05) << speciesName;
    EXPECT_LE(after.criticalNeeds, before.criticalNeeds + 0.01) << speciesName;
    EXPECT_GE(after.health, before.health - 0.01) << speciesName;
    EXPECT_LE(after.sick, before.sick) << speciesName;
    EXPECT_LE(after.hunger, 0.65) << speciesName;
    EXPECT_LE(after.stress, 0.4) << speciesName;
}

TEST(AnimalEthogramTests, DISABLED_MeerkatWeek) {
    CheckKeptWeek("meerkat");
}

TEST(AnimalEthogramTests, DISABLED_MeasureKeptWeek) {
    const auto species = SpeciesCatalog::LoadDirectory(std::string(ZOO_SOURCE_DIR) + "/data/species");
    for (const SpeciesDef& def : species.All()) {
        CheckKeptWeek(def.id);
    }
}

}  // namespace
}  // namespace zoo
