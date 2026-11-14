// SPDX-License-Identifier: MIT
#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <iostream>
#include <vector>

#include "TestData.hpp"
#include "zoo/sim/VisitStart.hpp"
#include "zoo/sim/VisitorZoo.hpp"

namespace zoo {
namespace {

TEST(VisitStart, MatchesGameSequence) {
    Zoo original(testdata::GameCatalog(), testdata::GameSpecies(), VisitorZooConfig(1));
    Zoo extracted(testdata::GameCatalog(), testdata::GameSpecies(), VisitorZooConfig(1));
    BuildVisitorZoo(original);
    BuildVisitorZoo(extracted);
    // A short fill keeps the equivalence check affordable under ASan.
    const VisitStartOptions options{.fillTicks = 601};
    original.Guests().SetPace(options.pace);
    original.SetCalendar(options.day, options.hour);
    original.SetDayPace(0);
    const std::int64_t before = options.fillTicks * 3 / 5;
    VisitorPace filling = options.pace;
    filling.arrivals = options.fillArrivals;
    original.Guests().SetPace(filling);
    for (std::int64_t i = 0; i < before; ++i) {
        original.Step();
    }
    const int spread = original.SpreadVisitors();
    original.Guests().SetPace(options.pace);
    for (std::int64_t i = before; i < options.fillTicks; ++i) {
        original.Step();
    }
    std::vector<float> progress;
    const auto report = FillForVisit(extracted, options, [&](float share) { progress.push_back(share); });
    EXPECT_EQ(original.StateHash(), extracted.StateHash());
    EXPECT_EQ(report.spread, spread);
    EXPECT_EQ(report.visitors, extracted.Guests().Count());
    EXPECT_EQ(report.ticks, options.fillTicks);
    EXPECT_EQ(extracted.Config().dayPace, 0);
    ASSERT_EQ(progress.size(), 3u);
    EXPECT_FLOAT_EQ(progress[0], 250.0f / 601.0f);
    EXPECT_FLOAT_EQ(progress[1], 500.0f / 601.0f);
    EXPECT_FLOAT_EQ(progress[2], 1.0f);
}

// Baselines were first recorded before any optimisation, at
// e1472bd0f07b252142d4efd1698525c9cbd1dd0d (plus the equivalent FillForVisit extraction), and
// are recorded again whenever the visitors' behaviour or the zoo itself is changed on purpose
// (2026-11-13: a group takes a bench only if at least half of it can sit; then the habitat
// fronts: planted stand-off strips, heavy mesh, visitors spread along the fronts; those who
// stand by a bench stay on ground they can walk to; habitats chosen by lot, a slower visit;
// then the animals' species routines: ethogram profiles, places chosen in the habitat;
// then G4: walking by steering with a pace of one's own, groups abreast, standing people on
// ground of their own, watchers looking at the animals).
void CheckFillHashes(std::int64_t ticks) {
    constexpr std::array<std::array<std::uint64_t, 2>, 3> shortHashes{
        {{1085343459346552820ULL, 2423916536734381269ULL},
         {3824713083525200886ULL, 11385678439516204360ULL},
         {1746387860539287630ULL, 1144367776643202936ULL}}};
    constexpr std::array<std::array<std::uint64_t, 2>, 3> fullHashes{
        {{12721221650076589925ULL, 13563154251682554642ULL},
         {15436198954380397714ULL, 14050102984353408064ULL},
         {1975919063374669059ULL, 10526684425400415800ULL}}};
    for (const std::uint64_t seed : {1ULL, 2ULL, 3ULL}) {
        Zoo zoo(testdata::GameCatalog(), testdata::GameSpecies(), VisitorZooConfig(seed));
        BuildVisitorZoo(zoo);
        FillForVisit(zoo, {.fillTicks = ticks});
        const auto& expected = (ticks == 600 ? shortHashes : fullHashes)[static_cast<std::size_t>(seed - 1)];
        const auto filled = zoo.StateHash();
        EXPECT_EQ(filled, expected[0]) << "seed=" << seed;
        for (int i = 0; i < 500; ++i) {
            zoo.Step();
        }
        EXPECT_EQ(zoo.StateHash(), expected[1]) << "seed=" << seed;
        std::cout << "HASH ticks=" << ticks << " seed=" << seed << " fill=" << filled
                  << " later=" << zoo.StateHash() << std::endl;
    }
}

TEST(VisitStart, FillHashesShort) {
    CheckFillHashes(600);
}

TEST(VisitStart, DISABLED_FillHashesFull) {
    CheckFillHashes(6000);
}

TEST(VisitStart, DISABLED_FillTiming) {
    // Internal timing needs temporary instrumentation: keep clocks out of the simulation.
    // These are reference measurements, not estimates of the live runs printed below.
    std::cout << "Reference profile seconds, before -> after (docs/VISIT_START.md): "
                 "animals 7.83423 -> 1.03482; visitors 0.491296 -> 0.427011 "
                 "(routes 0.200208 -> 0.178394, movement 0.138335 -> 0.125675, "
                 "decisions excluding routes 0.005661 -> 0.004452); weather 0 -> 0; "
                 "rest 0.004820 -> 0.003793.\n";
    for (int run = 0; run < 3; ++run) {
        const auto began = std::chrono::steady_clock::now();
        Zoo zoo(testdata::GameCatalog(), testdata::GameSpecies(), VisitorZooConfig(1));
        BuildVisitorZoo(zoo);
        const auto built = std::chrono::steady_clock::now();
        const auto report = FillForVisit(zoo);
        const auto filled = std::chrono::steady_clock::now();
        std::cout << "TIMING run=" << run + 1
                  << " build=" << std::chrono::duration<double>(built - began).count()
                  << " fill=" << std::chrono::duration<double>(filled - built).count()
                  << " visitors=" << report.visitors << " animals=" << zoo.AnimalCount()
                  << " hash=" << zoo.StateHash() << std::endl;
    }
}

TEST(VisitStart, EmptyFillStillSetsCalendarAndPace) {
    for (const std::int64_t ticks : {0, -1}) {
        Zoo zoo(testdata::GameCatalog(), testdata::GameSpecies(), VisitorZooConfig(2));
        int calls = 0;
        const auto report =
            FillForVisit(zoo, {.day = 17, .hour = 11.0f, .fillTicks = ticks}, [&](float share) {
                ++calls;
                EXPECT_FLOAT_EQ(share, 1.0f);
            });
        EXPECT_EQ(report.ticks, 0);
        EXPECT_EQ(report.visitors, 0);
        EXPECT_EQ(report.spread, 0);
        EXPECT_EQ(zoo.Time().Day(), 17);
        EXPECT_FLOAT_EQ(zoo.Time().Hour(), 11.0f);
        EXPECT_EQ(zoo.Config().dayPace, 0);
        EXPECT_EQ(calls, 1);
    }
}
}  // namespace
}  // namespace zoo
