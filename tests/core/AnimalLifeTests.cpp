// SPDX-License-Identifier: MIT
#include <gtest/gtest.h>

#include <algorithm>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include "TestData.hpp"
#include "zoo/animals/AnimalTuning.hpp"
#include "zoo/animals/Animals.hpp"
#include "zoo/animals/AnimalsJson.hpp"
#include "zoo/build/Catalog.hpp"
#include "zoo/nav/Nav.hpp"
#include "zoo/world/World.hpp"

namespace zoo {
namespace {

void AddSquareFence(World& world, const Catalog& catalog, int low, int high) {
    const int fence = catalog.fences.back().kind;
    for (int x = low; x <= high; ++x) {
        world.SetFence({x, low, true}, {static_cast<std::uint8_t>(fence), false, 1.0f});
        world.SetFence({x, high + 1, true}, {static_cast<std::uint8_t>(fence), false, 1.0f});
    }
    for (int z = low; z <= high; ++z) {
        world.SetFence({low, z, false}, {static_cast<std::uint8_t>(fence), false, 1.0f});
        world.SetFence({high + 1, z, false}, {static_cast<std::uint8_t>(fence), false, 1.0f});
    }
}

int CountEvents(const std::vector<Event>& events, EventType type) {
    return static_cast<int>(std::count_if(events.begin(), events.end(),
                                          [type](const Event& event) { return event.type == type; }));
}

TEST(AnimalLifeTests, NeglectedAnimalFallsIllAndTreatedAnimalRecovers) {
    const Catalog& catalog = testdata::GameCatalog();
    const SpeciesCatalog& species = testdata::GameSpecies();
    const int lion = species.IndexOf("lion");
    ASSERT_GE(lion, 0);
    int illSeeds = 0;
    for (std::uint64_t seed = 1; seed <= 20; ++seed) {
        World world(40, 40);
        ZooObjects objects;
        Enclosures enclosures;
        enclosures.Rebuild(world, objects, catalog);
        Navigator nav(world);
        Animals animals(seed);
        std::vector<Event> events;
        Env env{world, objects, enclosures, catalog, species, nav, nullptr, &events, {}, {}};
        const AnimalId id = animals.Add(env, lion, Sex::Female, 5.0f, World::TileCenter({20, 20}));
        ASSERT_TRUE(id);
        Animal* animal = animals.Find(id);
        ASSERT_NE(animal, nullptr);
        animal->hunger = 1.0f;
        animal->thirst = 1.0f;
        animal->welfare = 0.0f;
        animal->activity = AnimalActivity::Idle;
        for (int hour = 0; hour < 24 * 21 && !animal->sick; ++hour) {
            env.time.tick = static_cast<std::int64_t>(hour) * kTicksPerGameHour;
            animals.Step(env);
            animal = animals.Find(id);
            ASSERT_NE(animal, nullptr);
        }
        if (animal->sick) {
            ++illSeeds;
            EXPECT_EQ(CountEvents(events, EventType::AnimalSick), 1);
            EXPECT_TRUE(animals.Treat(id));
            const std::int64_t treatedAt = env.time.tick;
            for (int hour = 1; hour <= 24 * 2; ++hour) {
                env.time.tick = treatedAt + static_cast<std::int64_t>(hour) * kTicksPerGameHour;
                animals.Step(env);
            }
            EXPECT_FALSE(animals.Find(id)->sick)
                << "treated tick=" << animals.Find(id)->treatedTick << " current=" << env.time.tick;
            EXPECT_EQ(CountEvents(events, EventType::AnimalRecovered), 1);
        }
    }
    EXPECT_GE(illSeeds, 10);
}

TEST(AnimalLifeTests, HealthyPairConceivesAndBirthsAYoungWithRecordedParents) {
    const Catalog& catalog = testdata::GameCatalog();
    const SpeciesCatalog& species = testdata::GameSpecies();
    const int lion = species.IndexOf("lion");
    ASSERT_GE(lion, 0);
    World world(50, 50);
    ZooObjects objects;
    AddSquareFence(world, catalog, 8, 40);
    Enclosures enclosures;
    enclosures.Rebuild(world, objects, catalog);
    Navigator nav(world);
    Animals animals(3U);
    std::vector<Event> events;
    Env env{world, objects, enclosures, catalog, species, nav, nullptr, &events, {}, {}};
    const AnimalId motherId = animals.Add(env, lion, Sex::Female, 5.0f, World::TileCenter({20, 20}));
    const AnimalId fatherId = animals.Add(env, lion, Sex::Male, 5.0f, World::TileCenter({21, 20}));
    ASSERT_TRUE(motherId);
    ASSERT_TRUE(fatherId);
    animals.Find(motherId)->welfare = 0.9f;
    animals.Find(fatherId)->welfare = 0.9f;
    for (int hour = 0; hour < 24 * 30 && !animals.Find(motherId)->Pregnant(); ++hour) {
        env.time.tick = static_cast<std::int64_t>(hour) * kTicksPerGameHour;
        animals.Step(env);
    }
    Animal* mother = animals.Find(motherId);
    ASSERT_NE(mother, nullptr);
    ASSERT_TRUE(mother->Pregnant());
    EXPECT_EQ(mother->mate, fatherId);
    const std::int64_t dueTick = mother->dueTick;
    env.time.tick = ((dueTick + kTicksPerGameHour - 1) / kTicksPerGameHour) * kTicksPerGameHour;
    animals.Step(env);
    std::vector<const Animal*> young;
    for (const Animal& animal : animals.All()) {
        if (animal.mother == motherId) {
            young.push_back(&animal);
        }
    }
    ASSERT_FALSE(young.empty());
    const SpeciesDef& lionDef = species.All()[static_cast<std::size_t>(lion)];
    ASSERT_GE(young.size(), static_cast<std::size_t>(lionDef.litterMin));
    ASSERT_LE(young.size(), static_cast<std::size_t>(lionDef.litterMax));
    for (const Animal* baby : young) {
        EXPECT_EQ(AgeYears(*baby, env.time), 0.0f);
        EXPECT_EQ(baby->mother, motherId);
        EXPECT_EQ(baby->father, fatherId);
        EXPECT_EQ(baby->home, mother->home);
        EXPECT_NE(baby->id, motherId);
        EXPECT_NE(baby->id, fatherId);
    }
    EXPECT_EQ(animals.Births(), static_cast<int>(young.size()));
    EXPECT_EQ(CountEvents(events, EventType::AnimalPregnant), 1);
    EXPECT_EQ(CountEvents(events, EventType::AnimalBorn), animals.Births());
}

TEST(AnimalLifeTests, LifeStateRoundTripPreservesFutureEventsAndHash) {
    const Catalog& catalog = testdata::GameCatalog();
    const SpeciesCatalog& species = testdata::GameSpecies();
    World world(40, 40);
    ZooObjects objects;
    Enclosures enclosures;
    enclosures.Rebuild(world, objects, catalog);
    Navigator nav(world);
    Animals first(71U);
    std::vector<Event> firstEvents;
    Env firstEnv{world, objects, enclosures, catalog, species, nav, nullptr, &firstEvents, {}, {}};
    const int zebra = species.IndexOf("zebra");
    ASSERT_GE(zebra, 0);
    const AnimalId id = first.Add(firstEnv, zebra, Sex::Female, 4.0f, World::TileCenter({20, 20}));
    ASSERT_TRUE(id);
    Animal* animal = first.Find(id);
    ASSERT_NE(animal, nullptr);
    animal->sick = true;
    animal->treated = true;
    animal->sickSinceTick = 100;
    animal->treatedTick = 200;
    const nlohmann::json saved = ToJson(first, species);
    std::string error;
    nlohmann::json legacy = saved;
    for (nlohmann::json& encoded : legacy["animals"]) {
        encoded.erase("sick_since_tick");
        encoded.erase("treated_tick");
    }
    const auto oldLoaded = AnimalsFromJson(legacy, species, &error);
    ASSERT_TRUE(oldLoaded.has_value()) << error;
    EXPECT_EQ(oldLoaded->All().front().sickSinceTick, 0);
    EXPECT_EQ(oldLoaded->All().front().treatedTick, 0);
    auto restored = AnimalsFromJson(saved, species, &error);
    ASSERT_TRUE(restored.has_value()) << error;
    EXPECT_EQ(first.StateHash(), restored->StateHash());
    std::vector<Event> restoredEvents;
    Env restoredEnv{world, objects, enclosures, catalog, species, nav, nullptr, &restoredEvents, {}, {}};
    for (int hour = 0; hour < 24 * 3; ++hour) {
        const std::int64_t tick = static_cast<std::int64_t>(hour) * kTicksPerGameHour;
        firstEnv.time.tick = tick;
        restoredEnv.time.tick = tick;
        first.Step(firstEnv);
        restored->Step(restoredEnv);
    }
    EXPECT_EQ(first.StateHash(), restored->StateHash());
    EXPECT_EQ(firstEvents, restoredEvents);
}

TEST(AnimalLifeTests, SurplusAdultMalesFightAndWithinLimitStayCalm) {
    const Catalog& catalog = testdata::GameCatalog();
    const SpeciesCatalog& species = testdata::GameSpecies();
    const int lion = species.IndexOf("lion");
    ASSERT_GE(lion, 0);
    ASSERT_GT(species.All()[static_cast<std::size_t>(lion)].maxAdultMales, 0);
    World world(50, 50);
    ZooObjects objects;
    AddSquareFence(world, catalog, 8, 40);
    Enclosures enclosures;
    enclosures.Rebuild(world, objects, catalog);
    Navigator nav(world);
    Animals animals(12U);
    std::vector<Event> events;
    Env env{world, objects, enclosures, catalog, species, nav, nullptr, &events, {}, {}};
    const AnimalId first = animals.Add(env, lion, Sex::Male, 5.0f, World::TileCenter({19, 20}));
    const AnimalId second = animals.Add(env, lion, Sex::Male, 5.0f, World::TileCenter({20, 20}));
    const AnimalId third = animals.Add(env, lion, Sex::Male, 5.0f, World::TileCenter({21, 20}));
    ASSERT_TRUE(first);
    ASSERT_TRUE(second);
    ASSERT_TRUE(third);
    for (int hour = 0; hour < 24 * 21 && CountEvents(events, EventType::AnimalFight) == 0; ++hour) {
        env.time.tick = static_cast<std::int64_t>(hour) * kTicksPerGameHour;
        animals.Step(env);
    }
    EXPECT_GE(CountEvents(events, EventType::AnimalFight), 1);
    const int males = species.All()[static_cast<std::size_t>(lion)].maxAdultMales;
    Animals withinLimit(12U);
    std::vector<Event> calmEvents;
    Env calmEnv{world, objects, enclosures, catalog, species, nav, nullptr, &calmEvents, {}, {}};
    for (int index = 0; index < males; ++index) {
        const AnimalId id =
            withinLimit.Add(calmEnv, lion, Sex::Male, 5.0f, World::TileCenter({19 + index, 25}));
        ASSERT_TRUE(id);
    }
    for (int hour = 0; hour < 24 * 7; ++hour) {
        calmEnv.time.tick = static_cast<std::int64_t>(hour) * kTicksPerGameHour;
        withinLimit.Step(calmEnv);
    }
    EXPECT_EQ(CountEvents(calmEvents, EventType::AnimalFight), 0);
}

TEST(AnimalLifeTests, HungryOmnivoreKillsEligiblePreyButNotItsOwnSpecies) {
    const Catalog& catalog = testdata::GameCatalog();
    const SpeciesCatalog& species = testdata::GameSpecies();
    const int predator = species.IndexOf("brown_bear");
    const int zebra = species.IndexOf("zebra");
    ASSERT_GE(predator, 0);
    ASSERT_GE(zebra, 0);
    World world(50, 50);
    ZooObjects objects;
    AddSquareFence(world, catalog, 8, 40);
    Enclosures enclosures;
    enclosures.Rebuild(world, objects, catalog);
    Navigator nav(world);
    Animals animals(33U);
    std::vector<Event> events;
    Env env{world, objects, enclosures, catalog, species, nav, nullptr, &events, {}, {}};
    const AnimalId predatorId = animals.Add(env, predator, Sex::Male, 5.0f, World::TileCenter({20, 20}));
    const AnimalId rivalId = animals.Add(env, predator, Sex::Female, 5.0f, World::TileCenter({21, 20}));
    const AnimalId preyId = animals.Add(env, zebra, Sex::Female, 5.0f, World::TileCenter({22, 20}));
    ASSERT_TRUE(predatorId);
    ASSERT_TRUE(rivalId);
    ASSERT_TRUE(preyId);
    animals.Find(predatorId)->hunger = 1.0f;
    animals.Find(predatorId)->welfare = 0.9f;
    for (int hour = 0; hour < 24 * 7 && animals.Find(preyId) != nullptr; ++hour) {
        env.time.tick = static_cast<std::int64_t>(hour) * kTicksPerGameHour;
        animals.Step(env);
    }
    EXPECT_EQ(animals.Find(preyId), nullptr);
    EXPECT_NE(animals.Find(rivalId), nullptr);
    EXPECT_EQ(animals.Deaths(), 1);
    EXPECT_EQ(CountEvents(events, EventType::AnimalKilled), 1);
    const auto death = std::find_if(events.begin(), events.end(), [preyId](const Event& event) {
        return event.type == EventType::AnimalDied && event.animal == preyId;
    });
    ASSERT_NE(death, events.end());
    EXPECT_EQ(death->detail, static_cast<int>(DeathCause::Killed));
}

TEST(AnimalLifeTests, CratedMotherWaitsForBirthAndRemovedFatherIdIsPreserved) {
    const Catalog& catalog = testdata::GameCatalog();
    const SpeciesCatalog& species = testdata::GameSpecies();
    const int lion = species.IndexOf("lion");
    ASSERT_GE(lion, 0);
    World world(50, 50);
    ZooObjects objects;
    AddSquareFence(world, catalog, 8, 40);
    Enclosures enclosures;
    enclosures.Rebuild(world, objects, catalog);
    Navigator nav(world);
    Animals animals(91U);
    std::vector<Event> events;
    Env env{world, objects, enclosures, catalog, species, nav, nullptr, &events, {}, {}};
    const Vec2 position = World::TileCenter({20, 20});
    const AnimalId motherId = animals.Add(env, lion, Sex::Female, 5.0f, position);
    const AnimalId fatherId = animals.Add(env, lion, Sex::Male, 5.0f, World::TileCenter({21, 20}));
    ASSERT_TRUE(motherId);
    ASSERT_TRUE(fatherId);
    Animal* mother = animals.Find(motherId);
    ASSERT_NE(mother, nullptr);
    mother->welfare = 0.9f;
    mother->mate = fatherId;
    mother->dueTick = kTicksPerDay;
    ASSERT_TRUE(animals.Remove(fatherId));
    ASSERT_EQ(animals.Find(motherId)->mate, fatherId);
    ASSERT_TRUE(animals.Crate(motherId));
    env.time.tick = kTicksPerDay;
    animals.Step(env);
    EXPECT_TRUE(animals.Find(motherId)->Pregnant());
    ASSERT_TRUE(animals.Uncrate(env, motherId, position));
    env.time.tick = 2 * kTicksPerDay;
    animals.Step(env);
    int offspring = 0;
    for (const Animal& animal : animals.All()) {
        if (animal.mother == motherId) {
            ++offspring;
            EXPECT_EQ(animal.father, fatherId);
        }
    }
    EXPECT_GT(offspring, 0);
}

TEST(AnimalLifeTests, DeadMotherLosesPendingLitter) {
    const Catalog& catalog = testdata::GameCatalog();
    const SpeciesCatalog& species = testdata::GameSpecies();
    const int lion = species.IndexOf("lion");
    ASSERT_GE(lion, 0);
    World world(50, 50);
    ZooObjects objects;
    AddSquareFence(world, catalog, 8, 40);
    Enclosures enclosures;
    enclosures.Rebuild(world, objects, catalog);
    Navigator nav(world);
    Animals animals(57U);
    std::vector<Event> events;
    Env env{world, objects, enclosures, catalog, species, nav, nullptr, &events, {}, {}};
    const AnimalId motherId = animals.Add(env, lion, Sex::Female, 5.0f, World::TileCenter({20, 20}));
    ASSERT_TRUE(motherId);
    Animal* mother = animals.Find(motherId);
    ASSERT_NE(mother, nullptr);
    mother->dueTick = kTicksPerGameHour;
    mother->health = 0.0f;
    env.time.tick = kTicksPerGameHour;
    animals.Step(env);
    EXPECT_EQ(animals.Find(motherId), nullptr);
    EXPECT_EQ(animals.Births(), 0);
    EXPECT_EQ(animals.Deaths(), 1);
    EXPECT_EQ(CountEvents(events, EventType::AnimalBorn), 0);
}

TEST(AnimalLifeTests, UntreatedIllnessContinuesToDamageHealth) {
    const Catalog& catalog = testdata::GameCatalog();
    const SpeciesCatalog& species = testdata::GameSpecies();
    const int lion = species.IndexOf("lion");
    ASSERT_GE(lion, 0);
    World world(40, 40);
    ZooObjects objects;
    Enclosures enclosures;
    enclosures.Rebuild(world, objects, catalog);
    Navigator nav(world);
    Animals animals(65U);
    Env env{world, objects, enclosures, catalog, species, nav, nullptr, nullptr, {}, {}};
    const AnimalId id = animals.Add(env, lion, Sex::Female, 5.0f, World::TileCenter({20, 20}));
    ASSERT_TRUE(id);
    Animal* animal = animals.Find(id);
    ASSERT_NE(animal, nullptr);
    animal->sick = true;
    animal->sickSinceTick = 1;
    animal->hunger = 0.0f;
    animal->thirst = 0.0f;
    animal->health = 1.0f;
    for (int tick = 0; tick < kTicksPerDay; ++tick) {
        env.time.tick = tick;
        animals.Step(env);
    }
    ASSERT_NE(animals.Find(id), nullptr);
    EXPECT_NEAR(animals.Find(id)->health, 1.0f - tuning::kHealthSickPerDay, 0.002f);
}

TEST(AnimalLifeTests, MixedPopulationIsDeterministicAcrossMidweekJsonRoundTrip) {
    const Catalog& catalog = testdata::GameCatalog();
    const SpeciesCatalog& species = testdata::GameSpecies();
    const int lion = species.IndexOf("lion");
    const int zebra = species.IndexOf("zebra");
    const int bear = species.IndexOf("brown_bear");
    ASSERT_GE(lion, 0);
    ASSERT_GE(zebra, 0);
    ASSERT_GE(bear, 0);
    World world(50, 50);
    ZooObjects objects;
    AddSquareFence(world, catalog, 8, 40);
    Enclosures enclosures;
    enclosures.Rebuild(world, objects, catalog);
    Navigator nav(world);
    Animals original(107U);
    Animals parallel(107U);
    std::vector<Event> originalEvents;
    std::vector<Event> parallelEvents;
    Env originalEnv{world, objects, enclosures, catalog, species, nav, nullptr, &originalEvents, {}, {}};
    Env parallelEnv{world, objects, enclosures, catalog, species, nav, nullptr, &parallelEvents, {}, {}};
    const auto addPopulation = [&original, &parallel, &originalEnv, &parallelEnv, lion, zebra, bear]() {
        for (const auto& [speciesIndex, sex, tile] : {std::tuple{lion, Sex::Female, TileCoord{18, 20}},
                                                      std::tuple{lion, Sex::Male, TileCoord{19, 20}},
                                                      std::tuple{zebra, Sex::Female, TileCoord{22, 20}},
                                                      std::tuple{zebra, Sex::Male, TileCoord{23, 20}},
                                                      std::tuple{bear, Sex::Female, TileCoord{25, 20}}}) {
            const Vec2 position = World::TileCenter(tile);
            const AnimalId firstId = original.Add(originalEnv, speciesIndex, sex, 5.0f, position);
            const AnimalId secondId = parallel.Add(parallelEnv, speciesIndex, sex, 5.0f, position);
            EXPECT_TRUE(firstId);
            EXPECT_EQ(firstId, secondId);
            original.Find(firstId)->welfare = 0.9f;
            parallel.Find(secondId)->welfare = 0.9f;
        }
    };
    addPopulation();
    std::optional<Animals> restored;
    std::vector<Event> restoredEvents;
    std::size_t restoredEventStart = 0;
    Env restoredEnv{world, objects, enclosures, catalog, species, nav, nullptr, &restoredEvents, {}, {}};
    for (int hour = 0; hour < 24 * 21; ++hour) {
        const std::int64_t tick = static_cast<std::int64_t>(hour) * kTicksPerGameHour;
        originalEnv.time.tick = tick;
        parallelEnv.time.tick = tick;
        original.Step(originalEnv);
        parallel.Step(parallelEnv);
        if (hour == 24 * 10) {
            restoredEventStart = originalEvents.size();
            std::string error;
            restored = AnimalsFromJson(ToJson(original, species), species, &error);
            ASSERT_TRUE(restored.has_value()) << error;
            EXPECT_EQ(original.StateHash(), restored->StateHash());
        }
        if (restored.has_value() && hour > 24 * 10) {
            restoredEnv.time.tick = tick;
            restored->Step(restoredEnv);
        }
    }
    ASSERT_TRUE(restored.has_value());
    EXPECT_EQ(original.StateHash(), parallel.StateHash());
    EXPECT_EQ(original.StateHash(), restored->StateHash());
    EXPECT_EQ(originalEvents, parallelEvents);
    EXPECT_EQ(std::vector<Event>(originalEvents.begin() + static_cast<std::ptrdiff_t>(restoredEventStart),
                                 originalEvents.end()),
              restoredEvents);
}

}  // namespace
}  // namespace zoo
