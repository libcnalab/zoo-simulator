// SPDX-License-Identifier: MIT
#include <gtest/gtest.h>

#include <algorithm>
#include <cstdio>
#include <map>

#include "TestData.hpp"
#include "zoo/sim/VisitStart.hpp"
#include "zoo/sim/VisitorZoo.hpp"

namespace zoo {
namespace {

// Animals on their way get somewhere: in the Visitor Zoo, filled for a visit, an animal that
// walks is rarely held up by trees, rocks and its companions. The navigator plans through tile
// centres and knows nothing of props; a waypoint planned onto a rock made a giraffe stand still
// for 43 % of its walking (2026-11-14, before the waypoints were moved off props). Measured
// after: 1.17 % of all moving ticks stalled here; in a longer probe 2.10 % before and 1.49 %
// after, the worst animal 8 % of its own.
TEST(AnimalStall, AnimalsOnTheirWayGetThere) {
    Zoo zoo(testdata::GameCatalog(), testdata::GameSpecies(), VisitorZooConfig(1));
    BuildVisitorZoo(zoo);
    FillForVisit(zoo, VisitStartOptions{}, [](float) {});
    std::map<std::uint32_t, int> stalled;
    std::map<std::uint32_t, int> moving;
    for (int tick = 0; tick < 6000; ++tick) {
        zoo.Step();
        for (const Animal& animal : zoo.Fauna().All()) {
            if (!animal.InWorld() || animal.shelter || !animal.moving) {
                continue;
            }
            ++moving[animal.id.value];
            stalled[animal.id.value] += animal.speed < 0.05f ? 1 : 0;
        }
    }
    int allStalled = 0;
    int allMoving = 0;
    for (const auto& [id, ticks] : moving) {
        allStalled += stalled[id];
        allMoving += ticks;
        if (ticks >= 1000) {
            EXPECT_LT(static_cast<float>(stalled[id]) / static_cast<float>(ticks), 0.15f) << "animal " << id;
        }
    }
    ASSERT_GT(allMoving, 0);
    const float share = static_cast<float>(allStalled) / static_cast<float>(allMoving);
    std::printf("STALL %d of %d moving ticks (%.2f %%)\n", allStalled, allMoving, 100.0 * share);
    EXPECT_LT(share, 0.02f);
}

}  // namespace
}  // namespace zoo
