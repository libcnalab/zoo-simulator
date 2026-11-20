// SPDX-License-Identifier: MIT
#include <gtest/gtest.h>

#include <algorithm>

#include "TestData.hpp"
#include "zoo/animals/Animals.hpp"

namespace zoo {
namespace {

void AddSquareFence(World& world, const Catalog& catalog, int low, int high) {
    const int fence = catalog.FindFence("high_glass_wall")->kind;
    for (int x = low; x <= high; ++x) {
        world.SetFence({x, low, true}, {static_cast<std::uint8_t>(fence), false, 1.0f});
        world.SetFence({x, high + 1, true}, {static_cast<std::uint8_t>(fence), false, 1.0f});
    }
    for (int z = low; z <= high; ++z) {
        world.SetFence({low, z, false}, {static_cast<std::uint8_t>(fence), false, 1.0f});
        world.SetFence({high + 1, z, false}, {static_cast<std::uint8_t>(fence), false, 1.0f});
    }
}

void AddSmallShelter(World& world, ZooObjects& objects, const Catalog& catalog, TileCoord origin) {
    Building shelter;
    shelter.def = catalog.BuildingIndex("small_shelter");
    shelter.origin = origin;
    const BuildingId id = objects.AddBuilding(shelter);
    ASSERT_TRUE(id);
    const BuildingDef& def = catalog.buildings[static_cast<std::size_t>(shelter.def)];
    for (int z = origin.z; z < origin.z + def.depth; ++z) {
        for (int x = origin.x; x < origin.x + def.width; ++x) {
            world.SetBuilding({x, z}, id);
        }
    }
}

// Two clutches may be due together or in different hours. The first must not consume
// the minimum litter reserved at conception for the second.
TEST(AnimalBirthCapacity, ConcurrentClutchesKeepEachOthersRoom) {
    const Catalog& catalog = testdata::GameCatalog();
    const SpeciesCatalog& species = testdata::GameSpecies();
    const int ostrich = species.IndexOf("ostrich");
    ASSERT_GE(ostrich, 0);
    const SpeciesDef& def = species.All()[static_cast<std::size_t>(ostrich)];
    for (const bool together : {false, true}) {
        for (std::uint64_t seed = 1; seed <= 16; ++seed) {
            SCOPED_TRACE(seed);
            World world(40, 40);
            ZooObjects objects;
            Enclosures enclosures;
            enclosures.Rebuild(world, objects, catalog);
            Navigator nav(world);
            Animals animals(seed);
            Env env{world, objects, enclosures, catalog, species, nav, nullptr, nullptr, {}, {}};
            std::vector<AnimalId> ids;
            const int initial = def.groupMax - 2 * def.litterMin;
            for (int i = 0; i < initial; ++i) {
                ids.push_back(animals.Add(env, ostrich, Sex::Female, 4.0f,
                                          World::TileCenter({10 + i % 8, 10 + i / 8})));
                ASSERT_TRUE(ids.back());
                animals.Find(ids.back())->breeding = false;
                animals.Find(ids.back())->welfare = 1.0f;
            }
            ASSERT_GE(ids.size(), 2U);
            animals.Find(ids[0])->dueTick = kTicksPerGameHour;
            animals.Find(ids[1])->dueTick = (together ? 1 : 2) * kTicksPerGameHour;
            env.time.tick = kTicksPerGameHour;
            animals.Step(env);
            EXPECT_LE(static_cast<int>(animals.All().size()), def.groupMax - (together ? 0 : def.litterMin));
            env.time.tick += kTicksPerGameHour;
            animals.Step(env);
            EXPECT_LE(static_cast<int>(animals.All().size()), def.groupMax);
            EXPECT_FALSE(animals.Find(ids[0])->Pregnant());
            EXPECT_FALSE(animals.Find(ids[1])->Pregnant());
        }
    }
}

TEST(AnimalBirthCapacity, AFullGroupDefersBirthUntilThereIsRoom) {
    const Catalog& catalog = testdata::GameCatalog();
    const SpeciesCatalog& species = testdata::GameSpecies();
    const int ostrich = species.IndexOf("ostrich");
    ASSERT_GE(ostrich, 0);
    const SpeciesDef& def = species.All()[static_cast<std::size_t>(ostrich)];
    World world(40, 40);
    ZooObjects objects;
    Enclosures enclosures;
    enclosures.Rebuild(world, objects, catalog);
    Navigator nav(world);
    Animals animals(3);
    Env env{world, objects, enclosures, catalog, species, nav, nullptr, nullptr, {}, {}};
    std::vector<AnimalId> ids;
    for (int i = 0; i < def.groupMax; ++i) {
        ids.push_back(
            animals.Add(env, ostrich, Sex::Female, 4.0f, World::TileCenter({10 + i % 8, 10 + i / 8})));
        ASSERT_TRUE(ids.back());
        animals.Find(ids.back())->breeding = false;
    }
    animals.Find(ids[0])->dueTick = kTicksPerGameHour;
    env.time.tick = kTicksPerGameHour;
    animals.Step(env);
    EXPECT_TRUE(animals.Find(ids[0])->Pregnant());
    EXPECT_EQ(animals.Births(), 0);
    EXPECT_EQ(static_cast<int>(animals.All().size()), def.groupMax);
    for (int i = 0; i < def.litterMin; ++i) {
        ASSERT_TRUE(animals.Remove(ids.back()));
        ids.pop_back();
    }
    env.time.tick += kTicksPerGameHour;
    animals.Step(env);
    EXPECT_FALSE(animals.Find(ids[0])->Pregnant());
    EXPECT_LE(static_cast<int>(animals.All().size()), def.groupMax);
}

TEST(AnimalBirthCapacity, HabitatCapacityCapsAClutchAndPreventsAnotherConception) {
    const Catalog& catalog = testdata::GameCatalog();
    const SpeciesCatalog& species = testdata::GameSpecies();
    const int ostrich = species.IndexOf("ostrich");
    ASSERT_GE(ostrich, 0);
    World world(50, 50);
    ZooObjects objects;
    AddSquareFence(world, catalog, 8, 40);
    AddSmallShelter(world, objects, catalog, {10, 10});
    Enclosures enclosures;
    enclosures.Rebuild(world, objects, catalog);
    ASSERT_EQ(enclosures.All().size(), 1U);
    ASSERT_EQ(enclosures.All().front().stats.shelterCapacity, 8);
    Navigator nav(world);
    Animals animals(3U);
    std::vector<Event> events;
    Env env{world, objects, enclosures, catalog, species, nav, nullptr, &events, {}, {}};
    std::vector<AnimalId> ids;
    for (int i = 0; i < 7; ++i) {
        const Sex sex = i == 1 ? Sex::Male : Sex::Female;
        ids.push_back(animals.Add(env, ostrich, sex, 4.0f, World::TileCenter({16 + i, 20})));
        ASSERT_TRUE(ids.back());
        animals.Find(ids.back())->breeding = false;
        animals.Find(ids.back())->welfare = 0.9f;
    }
    animals.Find(ids.front())->dueTick = kTicksPerGameHour;
    env.time.tick = kTicksPerGameHour;
    animals.Step(env);
    EXPECT_EQ(animals.All().size(), 8U);
    EXPECT_EQ(animals.Births(), 1);
    EXPECT_FALSE(animals.Find(ids.front())->Pregnant());

    Animal* candidate = animals.Find(ids[2]);
    Animal* male = animals.Find(ids[1]);
    ASSERT_NE(candidate, nullptr);
    ASSERT_NE(male, nullptr);
    candidate->breeding = true;
    male->breeding = true;
    events.clear();
    for (int hour = 2; hour < 24 * 60; ++hour) {
        candidate = animals.Find(ids[2]);
        male = animals.Find(ids[1]);
        ASSERT_NE(candidate, nullptr);
        ASSERT_NE(male, nullptr);
        candidate->welfare = 0.9f;
        candidate->sick = false;
        male->sick = false;
        env.time.tick = static_cast<std::int64_t>(hour) * kTicksPerGameHour;
        animals.Step(env);
    }
    EXPECT_FALSE(animals.Find(ids[2])->Pregnant());
    EXPECT_EQ(std::count_if(events.begin(), events.end(),
                            [](const Event& event) { return event.type == EventType::AnimalPregnant; }),
              0);
}

}  // namespace
}  // namespace zoo
