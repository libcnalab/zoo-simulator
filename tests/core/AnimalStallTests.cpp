// SPDX-License-Identifier: MIT
#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <map>

#include "TestData.hpp"
#include "zoo/sim/VisitStart.hpp"
#include "zoo/sim/VisitorZoo.hpp"

namespace zoo {
namespace {

// Tile navigation cannot see blocking props. After waypoints themselves were moved off props,
// seed 1 still spent 1.49 % of its moving ticks stalled; routine places with two clear exits and
// trying an ordinary-activity sidestep after three quarters of a second reduce the current zoo
// from 0.81 % to 0.66 % and prevent a long-lived pocket from hiding in the total.
TEST(AnimalMovementTests, VisitorZooAnimalsDoNotRemainStalledInPropPockets) {
    Zoo zoo(testdata::GameCatalog(), testdata::GameSpecies(), VisitorZooConfig(1));
    BuildVisitorZoo(zoo);
    FillForVisit(zoo, VisitStartOptions{}, [](float) {});

    constexpr int kWarmupTicks = 6000;
    for (int tick = 0; tick < kWarmupTicks; ++tick) {
        zoo.Step();
    }

    constexpr int kMeasuredTicks = 6000;
    std::map<std::uint32_t, int> stalledRun;
    std::map<std::uint32_t, int> longestStall;
    int stalledTicks = 0;
    int movingTicks = 0;
    for (int tick = 0; tick < kMeasuredTicks; ++tick) {
        zoo.Step();
        for (const Animal& animal : zoo.Fauna().All()) {
            if (!animal.InWorld() || animal.shelter) {
                continue;
            }
            const bool moving = animal.moving;
            const bool stalled = moving && animal.speed < 0.05f;
            stalledTicks += stalled ? 1 : 0;
            movingTicks += moving ? 1 : 0;
            stalledRun[animal.id.value] = stalled ? stalledRun[animal.id.value] + 1 : 0;
            longestStall[animal.id.value] =
                std::max(longestStall[animal.id.value], stalledRun[animal.id.value]);
        }
    }

    ASSERT_GT(movingTicks, 0);
    std::cout << "STALL " << stalledTicks << " of " << movingTicks << " moving ticks ("
              << 100.0 * static_cast<double>(stalledTicks) / static_cast<double>(movingTicks) << " %)\n";
    EXPECT_LT(stalledTicks * 400, movingTicks * 3)
        << stalledTicks << " stalled ticks of " << movingTicks << " moving ticks";
    for (const auto& [id, ticks] : longestStall) {
        EXPECT_LT(ticks, 120) << "animal " << id;
    }
}

}  // namespace
}  // namespace zoo
