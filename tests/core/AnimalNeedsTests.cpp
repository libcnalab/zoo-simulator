// SPDX-License-Identifier: MIT
#include <gtest/gtest.h>

#include <algorithm>
#include <array>

#include "zoo/animals/AnimalTuning.hpp"
#include "zoo/animals/Animals.hpp"
#include "zoo/build/Catalog.hpp"

namespace zoo {
namespace {

TEST(AnimalNeedsTests, HungerAndThirstRiseAtTheirDocumentedDailyRates) {
    World world(40, 40);
    ZooObjects objects;
    Catalog catalog = Catalog::LoadDirectory(std::string(ZOO_SOURCE_DIR) + "/data/construction");
    SpeciesCatalog species = SpeciesCatalog::LoadDirectory(std::string(ZOO_SOURCE_DIR) + "/data/species");
    Enclosures enclosures;
    enclosures.Rebuild(world, objects, catalog);
    Navigator nav(world);
    Animals animals(9U);
    const int lion = species.IndexOf("lion");
    ASSERT_GE(lion, 0);
    ASSERT_TRUE(animals.Add(Env{world, objects, enclosures, catalog, species, nav, nullptr, nullptr, {}, {}},
                            lion, Sex::Female, 5.0f, World::TileCenter({20, 20})));
    Animal* animal = animals.Find(AnimalId{1U});
    ASSERT_NE(animal, nullptr);
    animal->hunger = 0.2f;
    animal->thirst = 0.2f;
    animal->activity = AnimalActivity::Idle;
    animal->activityLeft = kTicksPerDay;
    Env env{world, objects, enclosures, catalog, species, nav, nullptr, nullptr, {}, {}};
    for (int tick = 0; tick < kTicksPerDay; ++tick) {
        env.time.tick = tick;
        animals.Step(env);
    }
    ASSERT_NE(animals.Find(AnimalId{1U}), nullptr);
    EXPECT_NEAR(animals.Find(AnimalId{1U})->hunger, 0.7f, 0.002f);
    EXPECT_NEAR(animals.Find(AnimalId{1U})->thirst, 1.0f, 0.002f);
}

TEST(AnimalNeedsTests, FatigueAndBoredomUseGameHourAndGameDayRates) {
    World world(40, 40);
    ZooObjects objects;
    Catalog catalog = Catalog::LoadDirectory(std::string(ZOO_SOURCE_DIR) + "/data/construction");
    SpeciesCatalog species = SpeciesCatalog::LoadDirectory(std::string(ZOO_SOURCE_DIR) + "/data/species");
    Enclosures enclosures;
    enclosures.Rebuild(world, objects, catalog);
    Navigator nav(world);
    Animals animals(17U);
    const int lion = species.IndexOf("lion");
    ASSERT_GE(lion, 0);
    Env env{world, objects, enclosures, catalog, species, nav, nullptr, nullptr, {}, {}};
    const AnimalId awakeId = animals.Add(env, lion, Sex::Female, 5.0f, World::TileCenter({20, 20}));
    const AnimalId restingId = animals.Add(env, lion, Sex::Female, 5.0f, World::TileCenter({21, 20}));
    const AnimalId sleepingId = animals.Add(env, lion, Sex::Female, 5.0f, World::TileCenter({22, 20}));
    ASSERT_TRUE(awakeId);
    ASSERT_TRUE(restingId);
    ASSERT_TRUE(sleepingId);
    const SpeciesDef& lionDef = species.All()[static_cast<std::size_t>(lion)];
    const float initialFatigue = 0.5f;
    const float initialBoredom = 0.2f;
    for (const AnimalId id : {awakeId, restingId, sleepingId}) {
        Animal* animal = animals.Find(id);
        ASSERT_NE(animal, nullptr);
        animal->fatigue = initialFatigue;
        animal->boredom = initialBoredom;
        animal->activityLeft = 1000;
    }
    animals.Find(awakeId)->activity = AnimalActivity::Idle;
    animals.Find(restingId)->activity = AnimalActivity::Rest;
    animals.Find(sleepingId)->activity = AnimalActivity::Sleep;
    constexpr int elapsedTicks = kTicksPerGameHour / 2;
    for (int tick = 0; tick < elapsedTicks; ++tick) {
        env.time.tick = tick;
        animals.Step(env);
    }
    const float elapsedGameHours = static_cast<float>(elapsedTicks) / static_cast<float>(kTicksPerGameHour);
    const float awakeHours = 24.0f - lionDef.sleepHoursPerDay;
    EXPECT_NEAR(animals.Find(awakeId)->fatigue, initialFatigue + elapsedGameHours / awakeHours, 0.002f);
    EXPECT_NEAR(animals.Find(restingId)->fatigue,
                initialFatigue - 0.5f * elapsedGameHours / lionDef.sleepHoursPerDay, 0.002f);
    EXPECT_NEAR(animals.Find(sleepingId)->fatigue,
                initialFatigue - elapsedGameHours / lionDef.sleepHoursPerDay, 0.002f);
    const float expectedBoredom = initialBoredom + lionDef.playfulness * 1.2f *
                                                       static_cast<float>(elapsedTicks) /
                                                       static_cast<float>(kTicksPerDay);
    EXPECT_NEAR(animals.Find(awakeId)->boredom, expectedBoredom, 0.002f);
    EXPECT_NEAR(animals.Find(restingId)->boredom, expectedBoredom, 0.002f);
    EXPECT_NEAR(animals.Find(sleepingId)->boredom, expectedBoredom, 0.002f);
}

TEST(AnimalNeedsTests, StressMovesTowardTheLooseAnimalTargetAtTheDocumentedRates) {
    World world(40, 40);
    ZooObjects objects;
    Catalog catalog = Catalog::LoadDirectory(std::string(ZOO_SOURCE_DIR) + "/data/construction");
    SpeciesCatalog species = SpeciesCatalog::LoadDirectory(std::string(ZOO_SOURCE_DIR) + "/data/species");
    Enclosures enclosures;
    enclosures.Rebuild(world, objects, catalog);
    Navigator nav(world);
    Animals animals(29U);
    const int lion = species.IndexOf("lion");
    ASSERT_GE(lion, 0);
    Env env{world, objects, enclosures, catalog, species, nav, nullptr, nullptr, {}, {}};
    const AnimalId risingId = animals.Add(env, lion, Sex::Female, 5.0f, World::TileCenter({20, 20}));
    const AnimalId fallingId = animals.Add(env, lion, Sex::Male, 5.0f, World::TileCenter({21, 20}));
    ASSERT_TRUE(risingId);
    ASSERT_TRUE(fallingId);
    Animal* rising = animals.Find(risingId);
    Animal* falling = animals.Find(fallingId);
    ASSERT_NE(rising, nullptr);
    ASSERT_NE(falling, nullptr);
    rising->stress = 0.1f;
    falling->stress = 0.9f;
    rising->activityLeft = 1000;
    falling->activityLeft = 1000;
    constexpr int elapsedTicks = kTicksPerGameHour / 2;
    for (int tick = 0; tick < elapsedTicks; ++tick) {
        env.time.tick = tick;
        animals.Step(env);
    }
    const float elapsedGameDays = static_cast<float>(elapsedTicks) / static_cast<float>(kTicksPerDay);
    EXPECT_NEAR(animals.Find(risingId)->stress, 0.1f + tuning::kStressRisePerDay * elapsedGameDays, 0.002f);
    EXPECT_NEAR(animals.Find(fallingId)->stress, 0.9f - tuning::kStressFallPerDay * elapsedGameDays, 0.002f);
}

TEST(AnimalNeedsTests, DependentCalfNursesAndIndependentHatchlingFeedsForItself) {
    World world(40, 40);
    ZooObjects objects;
    Catalog catalog = Catalog::LoadDirectory(std::string(ZOO_SOURCE_DIR) + "/data/construction");
    SpeciesCatalog species = SpeciesCatalog::LoadDirectory(std::string(ZOO_SOURCE_DIR) + "/data/species");
    const int fence = catalog.FindFence("high_glass_wall")->kind;
    for (int x = 8; x <= 30; ++x) {
        world.SetFence({x, 8, true}, {static_cast<std::uint8_t>(fence), false, 1.0f});
        world.SetFence({x, 31, true}, {static_cast<std::uint8_t>(fence), false, 1.0f});
    }
    for (int z = 8; z <= 30; ++z) {
        world.SetFence({8, z, false}, {static_cast<std::uint8_t>(fence), false, 1.0f});
        world.SetFence({31, z, false}, {static_cast<std::uint8_t>(fence), false, 1.0f});
    }
    Enclosures enclosures;
    enclosures.Rebuild(world, objects, catalog);
    Navigator nav(world);
    std::vector<Event> events;
    Env env{world, objects, enclosures, catalog, species, nav, nullptr, &events, {}, {}};
    for (const std::string_view name : {"zebra", "aldabra_tortoise", "mandarin_duck"}) {
        SCOPED_TRACE(name);
        const int kind = species.IndexOf(name);
        ASSERT_GE(kind, 0);
        Animals animals(23U);
        env.time.tick = 0;
        const AnimalId mother =
            animals.Add(env, kind, Sex::Female, name == "aldabra_tortoise" ? 45.0f : 5.0f, World::TileCenter({19, 20}));
        const AnimalId offspring = animals.Add(env, kind, Sex::Female, 0.1f, World::TileCenter({20, 20}));
        ASSERT_TRUE(mother);
        ASSERT_TRUE(offspring);
        animals.Find(mother)->hunger = 0.2f;
        animals.Find(mother)->activityLeft = kTicksPerDay;
        Animal& young = *animals.Find(offspring);
        young.mother = mother;
        young.hunger = 0.29f;
        young.thirst = 0.29f;
        young.activityLeft = kTicksPerDay;
        for (int tick = 0; tick < kTicksPerDay; ++tick) {
            env.time.tick = tick;
            animals.Step(env);
        }
        const Animal& grown = *animals.Find(offspring);
        EXPECT_EQ(grown.mother, mother);
        if (name != "zebra") {
            EXPECT_GT(grown.hunger, tuning::kJuvenileNeedCap);
            EXPECT_GT(grown.thirst, tuning::kJuvenileNeedCap);
            if (name == "aldabra_tortoise") {
                EXPECT_FALSE(grown.following);
            }
        } else {
            EXPECT_LE(grown.hunger, tuning::kJuvenileNeedCap);
            EXPECT_LE(grown.thirst, tuning::kJuvenileNeedCap);
        }
    }
}

TEST(AnimalNeedsTests, AlertsLatchUntilNeedsFallBelowResetThreshold) {
    World world(40, 40);
    ZooObjects objects;
    Catalog catalog = Catalog::LoadDirectory(std::string(ZOO_SOURCE_DIR) + "/data/construction");
    SpeciesCatalog species = SpeciesCatalog::LoadDirectory(std::string(ZOO_SOURCE_DIR) + "/data/species");
    Enclosures enclosures;
    enclosures.Rebuild(world, objects, catalog);
    Navigator nav(world);
    Animals animals(4U);
    const int lion = species.IndexOf("lion");
    ASSERT_GE(lion, 0);
    ASSERT_TRUE(animals.Add(Env{world, objects, enclosures, catalog, species, nav, nullptr, nullptr, {}, {}},
                            lion, Sex::Female, 5.0f, World::TileCenter({20, 20})));
    Animal* animal = animals.Find(AnimalId{1U});
    ASSERT_NE(animal, nullptr);
    animal->hunger = 0.89999f;
    animal->thirst = 0.89999f;
    std::vector<Event> events;
    Env env{world, objects, enclosures, catalog, species, nav, nullptr, &events, {}, {}};
    env.time.tick = 0;
    animals.Step(env);
    EXPECT_EQ(animals.Find(AnimalId{1U})->alerts & 3U, 3U);
    const auto countEvents = [&events](EventType type) {
        return std::count_if(events.begin(), events.end(),
                             [type](const Event& event) { return event.type == type; });
    };
    EXPECT_EQ(countEvents(EventType::AnimalStarving), 1);
    EXPECT_EQ(countEvents(EventType::AnimalThirsty), 1);
    animals.Find(AnimalId{1U})->hunger = 0.59f;
    animals.Find(AnimalId{1U})->thirst = 0.59f;
    env.time.tick = 1;
    animals.Step(env);
    EXPECT_EQ(animals.Find(AnimalId{1U})->alerts & 3U, 0U);
    animals.Find(AnimalId{1U})->hunger = 0.89999f;
    animals.Find(AnimalId{1U})->thirst = 0.89999f;
    env.time.tick = 2;
    animals.Step(env);
    EXPECT_EQ(animals.Find(AnimalId{1U})->alerts & 3U, 3U);
    EXPECT_EQ(countEvents(EventType::AnimalStarving), 2);
    EXPECT_EQ(countEvents(EventType::AnimalThirsty), 2);
}

TEST(AnimalNeedsTests, FreshSpeciesFoodIsEatenFromAnEnclosureFeeder) {
    World world(40, 40);
    ZooObjects objects;
    Catalog catalog = Catalog::LoadDirectory(std::string(ZOO_SOURCE_DIR) + "/data/construction");
    SpeciesCatalog species = SpeciesCatalog::LoadDirectory(std::string(ZOO_SOURCE_DIR) + "/data/species");
    const int fence = catalog.FindFence("high_glass_wall")->kind;
    for (int x = 8; x <= 30; ++x) {
        world.SetFence({x, 8, true}, {static_cast<std::uint8_t>(fence), false, 1.0f});
        world.SetFence({x, 31, true}, {static_cast<std::uint8_t>(fence), false, 1.0f});
    }
    for (int z = 8; z <= 30; ++z) {
        world.SetFence({8, z, false}, {static_cast<std::uint8_t>(fence), false, 1.0f});
        world.SetFence({31, z, false}, {static_cast<std::uint8_t>(fence), false, 1.0f});
    }
    Prop feeder;
    feeder.def = catalog.PropIndex("meat_platform");
    feeder.position = World::TileCenter({20, 20});
    feeder.amount = 18.0f;
    feeder.food = catalog.FoodIndex("meat");
    objects.AddProp(feeder);
    Enclosures enclosures;
    enclosures.Rebuild(world, objects, catalog);
    Navigator nav(world);
    Animals animals(43U);
    const int lion = species.IndexOf("lion");
    ASSERT_GE(lion, 0);
    Env env{world, objects, enclosures, catalog, species, nav, nullptr, nullptr, {}, {}};
    const AnimalId id = animals.Add(env, lion, Sex::Female, 5.0f, World::TileCenter({14, 14}));
    ASSERT_TRUE(id);
    Animal* animal = animals.Find(id);
    ASSERT_NE(animal, nullptr);
    animal->hunger = 0.2f;
    animal->thirst = 0.0f;
    float maximumHunger = animal->hunger;
    for (int tick = 0; tick < kTicksPerDay; ++tick) {
        env.time.tick = tick;
        animals.Step(env);
        animal = animals.Find(id);
        ASSERT_NE(animal, nullptr);
        maximumHunger = std::max(maximumHunger, animal->hunger);
    }
    const Prop* remaining = objects.FindProp(PropId{1U});
    ASSERT_NE(remaining, nullptr);
    EXPECT_LT(remaining->amount, 18.0f);
    EXPECT_LT(maximumHunger, 0.8f);
}

TEST(AnimalNeedsTests, FourAnimalsTakeDifferentApproachAnglesAroundOneFeeder) {
    World world(40, 40);
    ZooObjects objects;
    Catalog catalog = Catalog::LoadDirectory(std::string(ZOO_SOURCE_DIR) + "/data/construction");
    SpeciesCatalog species = SpeciesCatalog::LoadDirectory(std::string(ZOO_SOURCE_DIR) + "/data/species");
    const int fence = catalog.FindFence("high_glass_wall")->kind;
    for (int x = 8; x <= 30; ++x) {
        world.SetFence({x, 8, true}, {static_cast<std::uint8_t>(fence), false, 1.0f});
        world.SetFence({x, 31, true}, {static_cast<std::uint8_t>(fence), false, 1.0f});
    }
    for (int z = 8; z <= 30; ++z) {
        world.SetFence({8, z, false}, {static_cast<std::uint8_t>(fence), false, 1.0f});
        world.SetFence({31, z, false}, {static_cast<std::uint8_t>(fence), false, 1.0f});
    }
    Prop feeder;
    feeder.def = catalog.PropIndex("meat_platform");
    feeder.position = World::TileCenter({20, 20});
    feeder.amount = 100.0f;
    feeder.food = catalog.FoodIndex("meat");
    const PropId feederId = objects.AddProp(feeder);
    ASSERT_TRUE(feederId);
    Enclosures enclosures;
    enclosures.Rebuild(world, objects, catalog);
    Navigator nav(world);
    Animals animals(49U);
    const int lion = species.IndexOf("lion");
    ASSERT_GE(lion, 0);
    Env env{world, objects, enclosures, catalog, species, nav, nullptr, nullptr, {}, {}};
    std::array<AnimalId, 4> ids{};
    for (std::size_t index = 0; index < ids.size(); ++index) {
        const int x = 14 + static_cast<int>(index % 2U) * 2;
        const int z = 14 + static_cast<int>(index / 2U) * 2;
        ids[index] = animals.Add(env, lion, Sex::Female, 5.0f, World::TileCenter({x, z}));
        ASSERT_TRUE(ids[index]);
        Animal* animal = animals.Find(ids[index]);
        ASSERT_NE(animal, nullptr);
        animal->hunger = 0.5f;
        animal->activityLeft = 0;
    }
    env.time.tick = 0;
    animals.Step(env);
    std::array<Vec2, 4> targets{};
    for (std::size_t index = 0; index < ids.size(); ++index) {
        const Animal* animal = animals.Find(ids[index]);
        ASSERT_NE(animal, nullptr);
        EXPECT_EQ(animal->targetProp, feederId);
        targets[index] = animal->target;
    }
    for (std::size_t left = 0; left < targets.size(); ++left) {
        for (std::size_t right = left + 1; right < targets.size(); ++right) {
            EXPECT_GT(DistanceSquared(targets[left], targets[right]), 1.0f);
        }
    }
}

TEST(AnimalNeedsTests, AWellSuppliedPairStaysBelowUrgentHungerAndConsumesDailyFood) {
    World world(40, 40);
    ZooObjects objects;
    Catalog catalog = Catalog::LoadDirectory(std::string(ZOO_SOURCE_DIR) + "/data/construction");
    SpeciesCatalog species = SpeciesCatalog::LoadDirectory(std::string(ZOO_SOURCE_DIR) + "/data/species");
    const int fence = catalog.FindFence("high_glass_wall")->kind;
    for (int x = 8; x <= 30; ++x) {
        world.SetFence({x, 8, true}, {static_cast<std::uint8_t>(fence), false, 1.0f});
        world.SetFence({x, 31, true}, {static_cast<std::uint8_t>(fence), false, 1.0f});
    }
    for (int z = 8; z <= 30; ++z) {
        world.SetFence({8, z, false}, {static_cast<std::uint8_t>(fence), false, 1.0f});
        world.SetFence({31, z, false}, {static_cast<std::uint8_t>(fence), false, 1.0f});
    }
    Prop feeder;
    feeder.def = catalog.PropIndex("hay_rack");
    feeder.position = World::TileCenter({17, 18});
    feeder.amount = 1000.0f;
    feeder.food = catalog.FoodIndex("hay");
    const PropId firstFeederId = objects.AddProp(feeder);
    feeder.position = World::TileCenter({23, 18});
    const PropId secondFeederId = objects.AddProp(feeder);
    world.SetWater({13, 19}, 1.0f, 0.3f);
    world.SetWater({27, 19}, 1.0f, 0.3f);
    Enclosures enclosures;
    enclosures.Rebuild(world, objects, catalog);
    Navigator nav(world);
    Animals animals(97U);
    const int zebra = species.IndexOf("zebra");
    ASSERT_GE(zebra, 0);
    Env env{world, objects, enclosures, catalog, species, nav, nullptr, nullptr, {}, {}};
    const AnimalId first = animals.Add(env, zebra, Sex::Female, 5.0f, World::TileCenter({13, 18}));
    const AnimalId second = animals.Add(env, zebra, Sex::Female, 5.0f, World::TileCenter({27, 18}));
    ASSERT_TRUE(first);
    ASSERT_TRUE(second);
    for (AnimalId id : {first, second}) {
        Animal* animal = animals.Find(id);
        ASSERT_NE(animal, nullptr);
        animal->hunger = 0.41f;
        animal->thirst = 0.9f;
        animal->activityLeft = 0;
    }
    ASSERT_NE(objects.FindProp(firstFeederId), nullptr);
    ASSERT_NE(objects.FindProp(secondFeederId), nullptr);
    EXPECT_GT(Animals::EdibleKg(env, *objects.FindProp(firstFeederId)), 0.0f);
    EXPECT_GT(Animals::EdibleKg(env, *objects.FindProp(secondFeederId)), 0.0f);
    ASSERT_EQ(enclosures.All().size(), 1U);
    EXPECT_EQ(enclosures.All().front().feeders.size(), 2U);
    float maximumHunger = 0.0f;
    float maximumThirst = 0.0f;
    const std::array<AnimalId, 2> animalIds{first, second};
    std::array<Vec2, 2> previousPositions{};
    std::array<float, 2> dailyDistance{};
    for (std::size_t index = 0; index < animalIds.size(); ++index) {
        previousPositions[index] = animals.Find(animalIds[index])->position;
    }
    for (int tick = 0; tick < 3 * kTicksPerDay; ++tick) {
        env.time.tick = tick;
        animals.Step(env);
        const Animal* firstAnimal = animals.Find(first);
        const Animal* secondAnimal = animals.Find(second);
        ASSERT_NE(firstAnimal, nullptr) << "first animal died at tick " << tick;
        ASSERT_NE(secondAnimal, nullptr) << "second animal died at tick " << tick;
        maximumHunger = std::max({maximumHunger, firstAnimal->hunger, secondAnimal->hunger});
        maximumThirst = std::max({maximumThirst, firstAnimal->thirst, secondAnimal->thirst});
        for (std::size_t index = 0; index < animalIds.size(); ++index) {
            const Animal* animal = animals.Find(animalIds[index]);
            ASSERT_NE(animal, nullptr);
            EXPECT_FALSE(animal->loose);
            EXPECT_EQ(enclosures.At(animal->position), enclosures.All().front().id);
            if (animal->moving) {
                EXPECT_FALSE(animal->route.empty());
            }
            if (animal->targetProp) {
                EXPECT_NE(objects.FindProp(animal->targetProp), nullptr);
            }
            dailyDistance[index] += Distance(animal->position, previousPositions[index]);
            previousPositions[index] = animal->position;
            const SpeciesDef& animalSpecies = species.All()[static_cast<std::size_t>(animal->species)];
            const float bodyRadius = animalSpecies.bodyRadiusM * BodyScale(*animal, animalSpecies, env.time);
            for (const Prop& prop : objects.Props()) {
                if (prop.def < 0 || static_cast<std::size_t>(prop.def) >= catalog.props.size()) {
                    continue;
                }
                const PropDef& definition = catalog.props[static_cast<std::size_t>(prop.def)];
                if (definition.blocksWalking && prop.id != animal->targetProp) {
                    // Between obstacles it is the width of the body that has to fit.
                    const float clearance = definition.radiusM + bodyRadius * tuning::kBodyWidthFraction;
                    EXPECT_GE(DistanceSquared(animal->position, prop.position), clearance * clearance);
                }
            }
        }
        if ((tick + 1) % kTicksPerDay == 0) {
            EXPECT_GT(dailyDistance[0], 0.0f);
            EXPECT_GT(dailyDistance[1], 0.0f);
            dailyDistance.fill(0.0f);
        }
    }
    const Prop* firstRemaining = objects.FindProp(firstFeederId);
    const Prop* secondRemaining = objects.FindProp(secondFeederId);
    ASSERT_NE(firstRemaining, nullptr);
    ASSERT_NE(secondRemaining, nullptr);
    const float consumed = feeder.amount * 2.0f - firstRemaining->amount - secondRemaining->amount;
    EXPECT_LT(maximumHunger, 0.8f);
    EXPECT_LT(maximumThirst, 1.0f);
    // The initial 0.41 hunger adds food on top of the steady daily intake.
    EXPECT_NEAR(consumed, 61.0f, 12.0f);
}

TEST(AnimalNeedsTests, SpoiledFoodIsNotSelectedOrConsumed) {
    World world(40, 40);
    ZooObjects objects;
    Catalog catalog = Catalog::LoadDirectory(std::string(ZOO_SOURCE_DIR) + "/data/construction");
    SpeciesCatalog species = SpeciesCatalog::LoadDirectory(std::string(ZOO_SOURCE_DIR) + "/data/species");
    const int fence = catalog.FindFence("high_glass_wall")->kind;
    for (int x = 8; x <= 30; ++x) {
        world.SetFence({x, 8, true}, {static_cast<std::uint8_t>(fence), false, 1.0f});
        world.SetFence({x, 31, true}, {static_cast<std::uint8_t>(fence), false, 1.0f});
    }
    for (int z = 8; z <= 30; ++z) {
        world.SetFence({8, z, false}, {static_cast<std::uint8_t>(fence), false, 1.0f});
        world.SetFence({31, z, false}, {static_cast<std::uint8_t>(fence), false, 1.0f});
    }
    Prop feeder;
    feeder.def = catalog.PropIndex("meat_platform");
    feeder.position = World::TileCenter({20, 20});
    feeder.amount = 20.0f;
    feeder.food = catalog.FoodIndex("meat");
    feeder.filledDay = 0.0f;
    const PropId feederId = objects.AddProp(feeder);
    Enclosures enclosures;
    enclosures.Rebuild(world, objects, catalog);
    Navigator nav(world);
    Animals animals(105U);
    const int lion = species.IndexOf("lion");
    ASSERT_GE(lion, 0);
    Env env{world, objects, enclosures, catalog, species, nav, nullptr, nullptr, {}, {}};
    const AnimalId id = animals.Add(env, lion, Sex::Female, 5.0f, World::TileCenter({14, 14}));
    ASSERT_TRUE(id);
    Animal* animal = animals.Find(id);
    ASSERT_NE(animal, nullptr);
    animal->hunger = 0.8f;
    animal->thirst = 0.0f;
    animal->activityLeft = 0;
    env.time.tick = 100 * kTicksPerDay;
    animals.Step(env);
    animal = animals.Find(id);
    ASSERT_NE(animal, nullptr);
    EXPECT_NE(animal->targetProp, feederId);
    const Prop* remaining = objects.FindProp(feederId);
    ASSERT_NE(remaining, nullptr);
    EXPECT_FLOAT_EQ(remaining->amount, 20.0f);
}

TEST(AnimalNeedsTests, ThirstCausesDeathWhenItDoesMoreDamageThanHunger) {
    World world(40, 40);
    ZooObjects objects;
    Catalog catalog = Catalog::LoadDirectory(std::string(ZOO_SOURCE_DIR) + "/data/construction");
    SpeciesCatalog species = SpeciesCatalog::LoadDirectory(std::string(ZOO_SOURCE_DIR) + "/data/species");
    Enclosures enclosures;
    enclosures.Rebuild(world, objects, catalog);
    Navigator nav(world);
    Animals animals(101U);
    const int lion = species.IndexOf("lion");
    ASSERT_GE(lion, 0);
    Env env{world, objects, enclosures, catalog, species, nav, nullptr, nullptr, {}, {}};
    const AnimalId id = animals.Add(env, lion, Sex::Female, 5.0f, World::TileCenter({20, 20}));
    ASSERT_TRUE(id);
    Animal* animal = animals.Find(id);
    ASSERT_NE(animal, nullptr);
    animal->hunger = 1.0f;
    animal->thirst = 1.0f;
    animal->health = 0.01f;
    animal->activityLeft = kTicksPerDay;
    std::vector<Event> events;
    env.events = &events;
    for (int tick = 0; tick < 1000 && animals.Find(id) != nullptr; ++tick) {
        env.time.tick = tick;
        animals.Step(env);
    }
    EXPECT_EQ(animals.Find(id), nullptr);
    EXPECT_EQ(animals.Deaths(), 1);
    const auto death = std::find_if(events.begin(), events.end(),
                                    [](const Event& event) { return event.type == EventType::AnimalDied; });
    ASSERT_NE(death, events.end());
    EXPECT_EQ(death->detail, static_cast<int>(DeathCause::Thirst));
}

TEST(AnimalNeedsTests, UrgentThirstUsesOpenWaterWhenThereIsNoTrough) {
    World world(40, 40);
    ZooObjects objects;
    Catalog catalog = Catalog::LoadDirectory(std::string(ZOO_SOURCE_DIR) + "/data/construction");
    SpeciesCatalog species = SpeciesCatalog::LoadDirectory(std::string(ZOO_SOURCE_DIR) + "/data/species");
    const int fence = catalog.FindFence("high_glass_wall")->kind;
    for (int x = 8; x <= 30; ++x) {
        world.SetFence({x, 8, true}, {static_cast<std::uint8_t>(fence), false, 1.0f});
        world.SetFence({x, 31, true}, {static_cast<std::uint8_t>(fence), false, 1.0f});
    }
    for (int z = 8; z <= 30; ++z) {
        world.SetFence({8, z, false}, {static_cast<std::uint8_t>(fence), false, 1.0f});
        world.SetFence({31, z, false}, {static_cast<std::uint8_t>(fence), false, 1.0f});
    }
    world.SetWater({20, 20}, 1.0f, 0.3f);
    Enclosures enclosures;
    enclosures.Rebuild(world, objects, catalog);
    Navigator nav(world);
    Animals animals(51U);
    const int lion = species.IndexOf("lion");
    ASSERT_GE(lion, 0);
    Env env{world, objects, enclosures, catalog, species, nav, nullptr, nullptr, {}, {}};
    const AnimalId id = animals.Add(env, lion, Sex::Female, 5.0f, World::TileCenter({18, 20}));
    ASSERT_TRUE(id);
    Animal* animal = animals.Find(id);
    ASSERT_NE(animal, nullptr);
    animal->hunger = 0.0f;
    animal->thirst = 0.9f;
    animal->activityLeft = 0;
    for (int tick = 0; tick < 1500; ++tick) {
        env.time.tick = tick;
        animals.Step(env);
    }
    animal = animals.Find(id);
    ASSERT_NE(animal, nullptr);
    EXPECT_LT(animal->thirst, 0.9f);
}

TEST(AnimalNeedsTests, UrgentThirstUsesAnEnclosureTrough) {
    World world(40, 40);
    ZooObjects objects;
    Catalog catalog = Catalog::LoadDirectory(std::string(ZOO_SOURCE_DIR) + "/data/construction");
    SpeciesCatalog species = SpeciesCatalog::LoadDirectory(std::string(ZOO_SOURCE_DIR) + "/data/species");
    const int fence = catalog.FindFence("high_glass_wall")->kind;
    for (int x = 8; x <= 30; ++x) {
        world.SetFence({x, 8, true}, {static_cast<std::uint8_t>(fence), false, 1.0f});
        world.SetFence({x, 31, true}, {static_cast<std::uint8_t>(fence), false, 1.0f});
    }
    for (int z = 8; z <= 30; ++z) {
        world.SetFence({8, z, false}, {static_cast<std::uint8_t>(fence), false, 1.0f});
        world.SetFence({31, z, false}, {static_cast<std::uint8_t>(fence), false, 1.0f});
    }
    Prop trough;
    trough.def = catalog.PropIndex("water_trough");
    trough.position = World::TileCenter({20, 20});
    trough.amount = 500.0f;
    const PropId troughId = objects.AddProp(trough);
    Enclosures enclosures;
    enclosures.Rebuild(world, objects, catalog);
    Navigator nav(world);
    Animals animals(52U);
    const int lion = species.IndexOf("lion");
    ASSERT_GE(lion, 0);
    Env env{world, objects, enclosures, catalog, species, nav, nullptr, nullptr, {}, {}};
    const AnimalId id = animals.Add(env, lion, Sex::Female, 5.0f, World::TileCenter({18, 20}));
    ASSERT_TRUE(id);
    Animal* animal = animals.Find(id);
    ASSERT_NE(animal, nullptr);
    animal->hunger = 0.0f;
    animal->thirst = 0.9f;
    animal->activityLeft = 0;
    ASSERT_EQ(enclosures.All().front().troughs.size(), 1U);
    for (int tick = 0; tick < 1500; ++tick) {
        env.time.tick = tick;
        animals.Step(env);
    }
    animal = animals.Find(id);
    ASSERT_NE(animal, nullptr);
    ASSERT_NE(objects.FindProp(troughId), nullptr);
    EXPECT_LT(animal->thirst, 0.9f);
    EXPECT_LT(objects.FindProp(troughId)->amount, 500.0f);
}

TEST(AnimalNeedsTests, ShySpeciesAccumulateMoreCrowdStressThanBoldSpecies) {
    World world(40, 40);
    ZooObjects objects;
    Catalog catalog = Catalog::LoadDirectory(std::string(ZOO_SOURCE_DIR) + "/data/construction");
    SpeciesCatalog species = SpeciesCatalog::LoadDirectory(std::string(ZOO_SOURCE_DIR) + "/data/species");
    const int fence = catalog.FindFence("high_glass_wall")->kind;
    for (int x = 8; x <= 30; ++x) {
        world.SetFence({x, 8, true}, {static_cast<std::uint8_t>(fence), false, 1.0f});
        world.SetFence({x, 31, true}, {static_cast<std::uint8_t>(fence), false, 1.0f});
    }
    for (int z = 8; z <= 30; ++z) {
        world.SetFence({8, z, false}, {static_cast<std::uint8_t>(fence), false, 1.0f});
        world.SetFence({31, z, false}, {static_cast<std::uint8_t>(fence), false, 1.0f});
    }
    Enclosures enclosures;
    enclosures.Rebuild(world, objects, catalog);
    Navigator nav(world);
    Animals animals(81U);
    const int tiger = species.IndexOf("tiger");
    const int zebra = species.IndexOf("zebra");
    ASSERT_GE(tiger, 0);
    ASSERT_GE(zebra, 0);
    EnclosureId enclosure = enclosures.At(World::TileCenter({18, 18}));
    ASSERT_TRUE(enclosure);
    animals.SetViewers(enclosure, 1000);
    Env env{world, objects, enclosures, catalog, species, nav, nullptr, nullptr, {}, {}};
    const AnimalId shy = animals.Add(env, tiger, Sex::Female, 5.0f, World::TileCenter({18, 18}));
    const AnimalId bold = animals.Add(env, zebra, Sex::Female, 5.0f, World::TileCenter({20, 18}));
    ASSERT_TRUE(shy);
    ASSERT_TRUE(bold);
    for (int tick = 0; tick < 3000; ++tick) {
        env.time.tick = tick;
        animals.Step(env);
    }
    ASSERT_NE(animals.Find(shy), nullptr);
    ASSERT_NE(animals.Find(bold), nullptr);
    EXPECT_GT(animals.Find(shy)->stress, animals.Find(bold)->stress);
}

}  // namespace
}  // namespace zoo
