// SPDX-License-Identifier: MIT
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <iostream>
#include <string>
#include <vector>

#include "zoo/animals/AnimalsJson.hpp"

namespace zoo {
namespace {

void AddFenceRectangle(World& world, int fence, int minX, int minZ, int maxX, int maxZ) {
    for (int x = minX; x <= maxX; ++x) {
        world.SetFence({x, minZ, true}, {static_cast<std::uint8_t>(fence), false, 1.0f});
        world.SetFence({x, maxZ + 1, true}, {static_cast<std::uint8_t>(fence), false, 1.0f});
    }
    for (int z = minZ; z <= maxZ; ++z) {
        world.SetFence({minX, z, false}, {static_cast<std::uint8_t>(fence), false, 1.0f});
        world.SetFence({maxX + 1, z, false}, {static_cast<std::uint8_t>(fence), false, 1.0f});
    }
}

TEST(AnimalDeterminismTests, IdenticalRunsAndSavedContinuationHaveIdenticalHashes) {
    World worldA(40, 40);
    World worldB(40, 40);
    ZooObjects objectsA;
    ZooObjects objectsB;
    Catalog catalogA = Catalog::LoadDirectory(std::string(ZOO_SOURCE_DIR) + "/data/construction");
    Catalog catalogB = Catalog::LoadDirectory(std::string(ZOO_SOURCE_DIR) + "/data/construction");
    SpeciesCatalog speciesA = SpeciesCatalog::LoadDirectory(std::string(ZOO_SOURCE_DIR) + "/data/species");
    SpeciesCatalog speciesB = SpeciesCatalog::LoadDirectory(std::string(ZOO_SOURCE_DIR) + "/data/species");
    Enclosures enclosuresA;
    Enclosures enclosuresB;
    const int fence = catalogA.FindFence("high_glass_wall")->kind;
    AddFenceRectangle(worldA, fence, 2, 2, 12, 12);
    AddFenceRectangle(worldA, fence, 15, 2, 25, 12);
    AddFenceRectangle(worldA, fence, 28, 2, 38, 12);
    AddFenceRectangle(worldB, fence, 2, 2, 12, 12);
    AddFenceRectangle(worldB, fence, 15, 2, 25, 12);
    AddFenceRectangle(worldB, fence, 28, 2, 38, 12);
    enclosuresA.Rebuild(worldA, objectsA, catalogA);
    enclosuresB.Rebuild(worldB, objectsB, catalogB);
    Navigator navA(worldA);
    Navigator navB(worldB);
    Animals first(90210U);
    Animals second(90210U);
    const std::array<std::string, 3> speciesIds{"lion", "zebra", "tiger"};
    std::array<int, 3> speciesIndicesA{};
    std::array<int, 3> speciesIndicesB{};
    for (std::size_t index = 0; index < speciesIds.size(); ++index) {
        speciesIndicesA[index] = speciesA.IndexOf(speciesIds[index]);
        speciesIndicesB[index] = speciesB.IndexOf(speciesIds[index]);
        ASSERT_GE(speciesIndicesA[index], 0);
        ASSERT_GE(speciesIndicesB[index], 0);
    }
    std::vector<Event> eventsA;
    std::vector<Event> eventsB;
    Env envA{worldA, objectsA, enclosuresA, catalogA, speciesA, navA, nullptr, &eventsA, {}, {}};
    Env envB{worldB, objectsB, enclosuresB, catalogB, speciesB, navB, nullptr, &eventsB, {}, {}};
    const std::array<int, 3> enclosureMinX{3, 16, 29};
    for (int animalIndex = 0; animalIndex < 10; ++animalIndex) {
        const int enclosureIndex = animalIndex % 3;
        const int x = enclosureMinX[static_cast<std::size_t>(enclosureIndex)] + animalIndex / 3;
        const int z = 4 + (animalIndex % 3) * 2;
        const std::size_t speciesIndex = static_cast<std::size_t>(animalIndex % 3);
        const Sex sex = animalIndex % 2 == 0 ? Sex::Female : Sex::Male;
        const float age = 3.0f + static_cast<float>(animalIndex % 5);
        ASSERT_TRUE(first.Add(envA, speciesIndicesA[speciesIndex], sex, age, World::TileCenter({x, z})));
        ASSERT_TRUE(second.Add(envB, speciesIndicesB[speciesIndex], sex, age, World::TileCenter({x, z})));
    }
    std::optional<Animals> loaded;
    std::uint64_t savedHash = 0;
    std::size_t savedEventCount = 0;
    std::vector<std::uint64_t> continuationHashes;
    for (int tick = 0; tick < 2 * kTicksPerDay; ++tick) {
        envA.time.tick = tick;
        envB.time.tick = tick;
        first.Step(envA);
        second.Step(envB);
        if ((tick + 1) % kTicksPerGameHour == 0) {
            EXPECT_EQ(first.StateHash(), second.StateHash())
                << "game hour " << (tick + 1) / kTicksPerGameHour;
        }
        if (tick >= kTicksPerDay && (tick + 1) % kTicksPerGameHour == 0) {
            continuationHashes.push_back(first.StateHash());
        }
        if (tick == kTicksPerDay - 1) {
            std::string error;
            loaded = AnimalsFromJson(ToJson(first, speciesA), speciesA, &error);
            ASSERT_TRUE(loaded.has_value()) << error;
            savedHash = first.StateHash();
            savedEventCount = eventsA.size();
            EXPECT_EQ(loaded->StateHash(), savedHash);
        }
    }
    EXPECT_EQ(eventsA, eventsB);
    EXPECT_TRUE(std::any_of(eventsA.begin(), eventsA.end(),
                            [](const Event& event) { return event.type == EventType::AnimalCalled; }));
    ASSERT_TRUE(loaded.has_value());
    Env loadedEnv{worldA, objectsA, enclosuresA, catalogA, speciesA, navA, nullptr, nullptr, {}, {}};
    std::vector<Event> loadedEvents;
    loadedEnv.events = &loadedEvents;
    std::size_t hashIndex = 0;
    for (int tick = kTicksPerDay; tick < 2 * kTicksPerDay; ++tick) {
        loadedEnv.time.tick = tick;
        loaded->Step(loadedEnv);
        if ((tick + 1) % kTicksPerGameHour == 0) {
            ASSERT_LT(hashIndex, continuationHashes.size());
            EXPECT_EQ(loaded->StateHash(), continuationHashes[hashIndex]) << "saved at hash " << savedHash;
            ++hashIndex;
        }
    }
    EXPECT_EQ(hashIndex, continuationHashes.size());
    const std::vector<Event> expectedContinuationEvents(
        eventsA.begin() + static_cast<std::ptrdiff_t>(savedEventCount), eventsA.end());
    EXPECT_EQ(loadedEvents, expectedContinuationEvents);
}

TEST(AnimalDeterminismTests, DISABLED_StepPerformance) {
    World world(40, 40);
    ZooObjects objects;
    Catalog catalog = Catalog::LoadDirectory(std::string(ZOO_SOURCE_DIR) + "/data/construction");
    SpeciesCatalog species = SpeciesCatalog::LoadDirectory(std::string(ZOO_SOURCE_DIR) + "/data/species");
    const int fence = catalog.FindFence("high_glass_wall")->kind;
    for (int row = 0; row < 4; ++row) {
        for (int column = 0; column < 5; ++column) {
            const int minX = 1 + column * 7;
            const int minZ = 1 + row * 9;
            const int maxX = minX + 4;
            const int maxZ = minZ + 5;
            for (int x = minX; x <= maxX; ++x) {
                world.SetFence({x, minZ, true}, {static_cast<std::uint8_t>(fence), false, 1.0f});
                world.SetFence({x, maxZ + 1, true}, {static_cast<std::uint8_t>(fence), false, 1.0f});
            }
            for (int z = minZ; z <= maxZ; ++z) {
                world.SetFence({minX, z, false}, {static_cast<std::uint8_t>(fence), false, 1.0f});
                world.SetFence({maxX + 1, z, false}, {static_cast<std::uint8_t>(fence), false, 1.0f});
            }
        }
    }
    Enclosures enclosures;
    enclosures.Rebuild(world, objects, catalog);
    Navigator nav(world);
    Animals animals(71U);
    const int lion = species.IndexOf("lion");
    ASSERT_GE(lion, 0);
    Env env{world, objects, enclosures, catalog, species, nav, nullptr, nullptr, {}, {}};
    for (int enclosureIndex = 0; enclosureIndex < 20; ++enclosureIndex) {
        const int row = enclosureIndex / 5;
        const int column = enclosureIndex % 5;
        const int minX = 1 + column * 7;
        const int minZ = 1 + row * 9;
        for (int animalIndex = 0; animalIndex < 15; ++animalIndex) {
            const int x = minX + 1 + animalIndex % 4;
            const int z = minZ + 1 + animalIndex / 4;
            ASSERT_TRUE(animals.Add(env, lion, Sex::Female, 5.0f, World::TileCenter({x, z})));
        }
    }
    const auto start = std::chrono::steady_clock::now();
    for (int tick = 0; tick < kTicksPerDay; ++tick) {
        env.time.tick = tick;
        animals.Step(env);
    }
    const auto end = std::chrono::steady_clock::now();
    const double elapsedMicroseconds = std::chrono::duration<double, std::micro>(end - start).count();
    std::cout << "DISABLED_StepPerformance mean us/step: "
              << elapsedMicroseconds / static_cast<double>(kTicksPerDay) << '\n';
}

}  // namespace
}  // namespace zoo
