// SPDX-License-Identifier: MIT
#include <gtest/gtest.h>

#include "TestData.hpp"
#include "zoo/animals/Animals.hpp"

namespace zoo {
namespace {

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

}  // namespace
}  // namespace zoo
