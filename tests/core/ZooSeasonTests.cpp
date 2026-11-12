// SPDX-License-Identifier: MIT
// The starter zoo left to itself for a season: what its animals live through.
// Slow, so disabled; run with
//   build/tests/zoo_core_tests --gtest_also_run_disabled_tests --gtest_filter='*Season*'
#include <gtest/gtest.h>

#include <array>
#include <cstdio>
#include <vector>

#include "TestData.hpp"
#include "zoo/sim/Scenario.hpp"

namespace zoo {

TEST(ZooSeasonTests, DISABLED_ASeasonInTheStarterZoo) {
    Zoo zoo(testdata::GameCatalog(), testdata::GameSpecies());
    BuildStarterZoo(zoo);
    const int before = zoo.AnimalCount();
    std::vector<Event> events;
    int born = 0;
    int died = 0;
    int sick = 0;
    int recovered = 0;
    int fights = 0;
    int kills = 0;
    int pregnant = 0;
    for (int period = 0; period < 9; ++period) {
        for (std::int64_t tick = 0; tick < static_cast<std::int64_t>(kTicksPerDay) * 10; ++tick) {
            zoo.Step();
            events.clear();
            zoo.DrainEvents(events);
            for (const Event& e : events) {
                born += e.type == EventType::AnimalBorn;
                died += e.type == EventType::AnimalDied;
                sick += e.type == EventType::AnimalSick;
                recovered += e.type == EventType::AnimalRecovered;
                fights += e.type == EventType::AnimalFight;
                kills += e.type == EventType::AnimalKilled;
                pregnant += e.type == EventType::AnimalPregnant;
            }
        }
        std::printf(
            "[day %3d] animals %d, welfare %.2f, pregnant %d, born %d, died %d, sick %d, recovered %d, "
            "fights %d, kills %d, rating %.2f, balance %s\n",
            (period + 1) * 10, zoo.AnimalCount(), static_cast<double>(zoo.MeanWelfare()), pregnant, born,
            died, sick, recovered, fights, kills, static_cast<double>(zoo.Guests().Rating()),
            FormatMoney(zoo.Accounts().Balance()).c_str());
    }
    EXPECT_GE(zoo.AnimalCount(), before - 2) << "a kept zoo does not die out";
    EXPECT_LE(zoo.AnimalCount(), before * 3) << "nor does it overflow";
    EXPECT_EQ(kills, 0) << "every species has its own enclosure";
    EXPECT_LE(sick, before * 3);
}

}  // namespace zoo
