// SPDX-License-Identifier: MIT
#include <gtest/gtest.h>

#include "zoo/animals/AnimalTuning.hpp"
#include "zoo/animals/Animals.hpp"

namespace zoo {
namespace {

TEST(AnimalBehaviourTests, DecisionsChooseActivitiesAndKeepNeedPriority) {
    World world(40, 40);
    ZooObjects objects;
    Catalog catalog = Catalog::LoadDirectory(std::string(ZOO_SOURCE_DIR) + "/data/construction");
    SpeciesCatalog species = SpeciesCatalog::LoadDirectory(std::string(ZOO_SOURCE_DIR) + "/data/species");
    const int fence = catalog.fences.back().kind;
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
    Animals animals(31U);
    const int lion = species.IndexOf("lion");
    ASSERT_GE(lion, 0);
    const AnimalId id =
        animals.Add(Env{world, objects, enclosures, catalog, species, nav, nullptr, nullptr, {}, {}}, lion,
                    Sex::Female, 5.0f, World::TileCenter({20, 20}));
    ASSERT_TRUE(id);
    Animal* animal = animals.Find(id);
    ASSERT_NE(animal, nullptr);
    animal->hunger = 0.0f;
    animal->thirst = 0.0f;
    animal->fatigue = 0.0f;
    animal->activityLeft = 0;
    Env env{world, objects, enclosures, catalog, species, nav, nullptr, nullptr, {}, {}};
    bool sawWander = false;
    bool sawRest = false;
    bool sawSleep = false;
    for (int tick = 0; tick < 5000; ++tick) {
        env.time.tick = tick;
        animals.Step(env);
        animal = animals.Find(id);
        ASSERT_NE(animal, nullptr);
        sawWander = sawWander || animal->activity == AnimalActivity::Wander;
        sawRest = sawRest || animal->activity == AnimalActivity::Rest;
        sawSleep = sawSleep || animal->activity == AnimalActivity::Sleep;
    }
    EXPECT_TRUE(sawWander);
    EXPECT_TRUE(sawRest || sawSleep);
}

TEST(AnimalBehaviourTests, RainMovesAnAnimalIntoAnOpenShelter) {
    World world(40, 40);
    ZooObjects objects;
    Catalog catalog = Catalog::LoadDirectory(std::string(ZOO_SOURCE_DIR) + "/data/construction");
    SpeciesCatalog species = SpeciesCatalog::LoadDirectory(std::string(ZOO_SOURCE_DIR) + "/data/species");
    const int fence = catalog.fences.back().kind;
    for (int x = 8; x <= 30; ++x) {
        world.SetFence({x, 8, true}, {static_cast<std::uint8_t>(fence), false, 1.0f});
        world.SetFence({x, 31, true}, {static_cast<std::uint8_t>(fence), false, 1.0f});
    }
    for (int z = 8; z <= 30; ++z) {
        world.SetFence({8, z, false}, {static_cast<std::uint8_t>(fence), false, 1.0f});
        world.SetFence({31, z, false}, {static_cast<std::uint8_t>(fence), false, 1.0f});
    }
    Building shelter;
    shelter.def = catalog.BuildingIndex("small_shelter");
    shelter.origin = {18, 18};
    shelter.facing = Dir::South;
    const BuildingId shelterId = objects.AddBuilding(shelter);
    ASSERT_TRUE(shelterId);
    for (int z = 18; z < 20; ++z) {
        for (int x = 18; x < 20; ++x) {
            world.SetBuilding({x, z}, shelterId);
        }
    }
    Enclosures enclosures;
    enclosures.Rebuild(world, objects, catalog);
    Navigator nav(world);
    Animals animals(811U);
    const int lion = species.IndexOf("lion");
    ASSERT_GE(lion, 0);
    const AnimalId id =
        animals.Add(Env{world, objects, enclosures, catalog, species, nav, nullptr, nullptr, {}, {}}, lion,
                    Sex::Female, 5.0f, World::TileCenter({14, 14}));
    ASSERT_TRUE(id);
    Animal* animal = animals.Find(id);
    ASSERT_NE(animal, nullptr);
    animal->hunger = 0.0f;
    animal->thirst = 0.0f;
    animal->activityLeft = 0;
    Env env{world, objects, enclosures, catalog, species, nav, nullptr, nullptr, {}, {}};
    env.weather.rain = 0.8f;
    for (int tick = 0; tick < 1200 && !animal->shelter; ++tick) {
        env.time.tick = tick;
        animals.Step(env);
        animal = animals.Find(id);
        ASSERT_NE(animal, nullptr);
    }
    EXPECT_EQ(animal->shelter, shelterId);
    ASSERT_TRUE(objects.RemoveBuilding(shelterId));
    for (int z = 18; z < 20; ++z) {
        for (int x = 18; x < 20; ++x) {
            world.SetBuilding({x, z}, {});
        }
    }
    enclosures.Rebuild(world, objects, catalog);
    env.time.tick = 1200;
    animals.Step(env);
    animal = animals.Find(id);
    ASSERT_NE(animal, nullptr);
    EXPECT_FALSE(animal->shelter);
}

TEST(AnimalBehaviourTests, ExtremeHeatUsesShadeBesideATreeWhenThereIsNoCooledShelter) {
    World world(40, 40);
    ZooObjects objects;
    Catalog catalog = Catalog::LoadDirectory(std::string(ZOO_SOURCE_DIR) + "/data/construction");
    SpeciesCatalog species = SpeciesCatalog::LoadDirectory(std::string(ZOO_SOURCE_DIR) + "/data/species");
    const int fence = catalog.fences.back().kind;
    for (int x = 8; x <= 30; ++x) {
        world.SetFence({x, 8, true}, {static_cast<std::uint8_t>(fence), false, 1.0f});
        world.SetFence({x, 31, true}, {static_cast<std::uint8_t>(fence), false, 1.0f});
    }
    for (int z = 8; z <= 30; ++z) {
        world.SetFence({8, z, false}, {static_cast<std::uint8_t>(fence), false, 1.0f});
        world.SetFence({31, z, false}, {static_cast<std::uint8_t>(fence), false, 1.0f});
    }
    Prop tree;
    tree.def = catalog.PropIndex("oak");
    tree.position = World::TileCenter({24, 20});
    const PropId treeId = objects.AddProp(tree);
    ASSERT_TRUE(treeId);
    Enclosures enclosures;
    enclosures.Rebuild(world, objects, catalog);
    Navigator nav(world);
    Animals animals(117U);
    const int lion = species.IndexOf("lion");
    ASSERT_GE(lion, 0);
    Env env{world, objects, enclosures, catalog, species, nav, nullptr, nullptr, {}, {}};
    const AnimalId id = animals.Add(env, lion, Sex::Female, 5.0f, World::TileCenter({18, 20}));
    ASSERT_TRUE(id);
    Animal* animal = animals.Find(id);
    ASSERT_NE(animal, nullptr);
    animal->hunger = 0.0f;
    animal->thirst = 0.0f;
    animal->activityLeft = 0;
    env.weather.temperatureC = 44.0f;
    env.time.tick = 0;
    animals.Step(env);
    animal = animals.Find(id);
    ASSERT_NE(animal, nullptr);
    EXPECT_EQ(animal->activity, AnimalActivity::Shelter);
    EXPECT_NE(animal->target, animal->position);
    EXPECT_GT(DistanceSquared(animal->target, tree.position), 1.0f);
}

TEST(AnimalBehaviourTests, HighStressStartsAPaceLoopWithAPathBackToItsStart) {
    World world(40, 40);
    ZooObjects objects;
    Catalog catalog = Catalog::LoadDirectory(std::string(ZOO_SOURCE_DIR) + "/data/construction");
    SpeciesCatalog species = SpeciesCatalog::LoadDirectory(std::string(ZOO_SOURCE_DIR) + "/data/species");
    const int fence = catalog.fences.back().kind;
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
    Animals animals(131U);
    const int lion = species.IndexOf("lion");
    ASSERT_GE(lion, 0);
    Env env{world, objects, enclosures, catalog, species, nav, nullptr, nullptr, {}, {}};
    const AnimalId id = animals.Add(env, lion, Sex::Female, 5.0f, World::TileCenter({18, 18}));
    ASSERT_TRUE(id);
    Animal* animal = animals.Find(id);
    ASSERT_NE(animal, nullptr);
    animal->stress = 0.95f;
    animal->hunger = 0.0f;
    animal->thirst = 0.0f;
    animal->activityLeft = 0;
    bool sawPacingLoop = false;
    for (int tick = 0; tick < 8 * kTicksPerGameHour && !sawPacingLoop; ++tick) {
        env.time.tick = tick;
        animals.Step(env);
        animal = animals.Find(id);
        ASSERT_NE(animal, nullptr);
        sawPacingLoop = animal->activity == AnimalActivity::Pace && animal->route.size() == 3;
    }
    EXPECT_TRUE(sawPacingLoop);
}

TEST(AnimalBehaviourTests, DiurnalAnimalSleepsMostlyAtNightForItsDailySleepNeed) {
    World world(40, 40);
    ZooObjects objects;
    Catalog catalog = Catalog::LoadDirectory(std::string(ZOO_SOURCE_DIR) + "/data/construction");
    SpeciesCatalog species = SpeciesCatalog::LoadDirectory(std::string(ZOO_SOURCE_DIR) + "/data/species");
    const int fence = catalog.fences.back().kind;
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
    trough.position = World::TileCenter({24, 24});
    trough.amount = 180.0f;
    objects.AddProp(trough);
    Enclosures enclosures;
    enclosures.Rebuild(world, objects, catalog);
    Navigator nav(world);
    Animals animals(321U);
    const int zebra = species.IndexOf("zebra");
    ASSERT_GE(zebra, 0);
    Env env{world, objects, enclosures, catalog, species, nav, nullptr, nullptr, {}, {}};
    const AnimalId id = animals.Add(env, zebra, Sex::Female, 5.0f, World::TileCenter({14, 14}));
    ASSERT_TRUE(id);
    Animal* animal = animals.Find(id);
    ASSERT_NE(animal, nullptr);
    animal->hunger = 0.0f;
    animal->thirst = 0.0f;
    animal->fatigue = 0.2f;
    animal->activityLeft = 0;
    int asleepTicks = 0;
    int asleepNightTicks = 0;
    for (int tick = 0; tick < kTicksPerDay; ++tick) {
        env.time.tick = tick;
        animals.Step(env);
        animal = animals.Find(id);
        ASSERT_NE(animal, nullptr);
        if (animal->activity == AnimalActivity::Sleep) {
            ++asleepTicks;
            const float hour = env.time.Hour();
            if (hour < tuning::kNightEndHour || hour >= tuning::kNightBeginHour) {
                ++asleepNightTicks;
            }
        }
    }
    const float sleptHours = static_cast<float>(asleepTicks) / static_cast<float>(kTicksPerGameHour);
    EXPECT_NEAR(sleptHours, species.All()[static_cast<std::size_t>(zebra)].sleepHoursPerDay, 3.0f);
    EXPECT_GT(asleepNightTicks, asleepTicks / 2);
}

}  // namespace
}  // namespace zoo
