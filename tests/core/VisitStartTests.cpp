// SPDX-License-Identifier: MIT
#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <iostream>
#include <map>
#include <memory>
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
    const std::int64_t before = options.fillTicks * 4 / 5;
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
// ground of their own, watchers looking at the animals; 2026-11-14, G8: the zoo's plan, families
// with young, new barriers, furnishing and planting; then G5: the keepers, the gardener and
// the cleaner at work, in the state hash; staff keep off water; then animals' waypoints moved
// off blocking props; 2026-11-19: routine places must have two body-wide exits around props;
// land plants keep off the sloping ground dug by pools; trees keep back from habitat fronts; public
// walks are at least two tiles wide wherever the plan has open lawn to widen them onto;
// 2026-11-20: reeds root on the lagoon's dry margin and tall grass grows in open habitats;
// Elephant Valley and Rhino Hollow gain graded dry moats along their public fronts; staff and
// visitors steer around one another, walking staff slide around animals in their region, and
// shy animals retreat from staff who enter a safe habitat; outside work at dangerous habitats
// uses both the service gate and the public viewing bays; the meerkats' back and sides use a
// mesh-backed wildlife hedge instead of exposed wire mesh; every ordinary public walk is at
// least four metres wide; habitat trees keep the full viewing set-back while low cover preserves
// the vegetation welfare margin; dangerous groups use a saved shelter lockout for inside keeper
// work, public feedings service troughs too, and routine care is requested earlier; watchers
// yield to unrelated standing visitors, and the cleaner prioritises current litter rather than
// finishing a stale bin circuit; walking companions receive the full shoulder corridor;
// ordinary animal errands try their next sidestep after three quarters of a second; both bear
// habitats become mother-and-two-cub groups using the dedicated juvenile models; animal route
// searches avoid the blocking trees and rocks of their own habitat, while late-arriving visitors
// keep hard body room from standing people and the player; dangerous groups visibly walk into
// their assigned houses while the keeper waits outside, then leave only after its holding gate
// visibly slides from a safe-side release operation; followers keep a stable route to a moving
// group leader's destination rather than restarting that route every second; the property is
// twenty percent larger and its connected northern woodland circuit has its own rest facilities;
// Lynx Ridge occupies that new land with a mother and two kittens; Red Panda Canopy adds a mother
// and two dependent cubs, a western gallery and its drinks service; keeper rounds remain globally
// shortest after the expansion rather than leaving the two far ends as one 204-metre pair;
// a second 20,480-square-metre northern stage adds Snow Leopard Heights, two connected approaches,
// a mother and two cubs, redundant water points for the three shy northern species, and the
// visitor spread happens after four fifths of the fill so all seventeen fronts get a group;
// dependent northern cubs follow their mothers, and two new garden walks on a map 64 m wider per
// side lead to four picnic tables and a western marsh with a family of eight capybaras; resting
// penguins and flamingos use their upright group pose beside a stationary companion).
void CheckFillHashes(std::int64_t ticks) {
    constexpr std::array<std::array<std::uint64_t, 2>, 3> shortHashes{
        {{9139815037628863271ULL, 2085363316060078965ULL},
         {16001145640700442961ULL, 5644383874109006456ULL},
         {10636984344542905811ULL, 6304807489940044530ULL}}};
    constexpr std::array<std::array<std::uint64_t, 2>, 3> fullHashes{
        {{1054235427821063531ULL, 8043702145645978796ULL},
         {11740243411003087215ULL, 9360760697049082900ULL},
         {11257981172144800077ULL, 1213298953784258099ULL}}};
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

// The crowd setting of a visit: a smaller crowd holds at most that many people, nobody means
// nobody, and sending people home during a visit takes whole groups, the farthest first.
TEST(VisitStart, CrowdSettingLimitsAndSendsWholeGroupsHome) {
    const auto filled = [](int most) {
        auto zoo =
            std::make_unique<Zoo>(testdata::GameCatalog(), testdata::GameSpecies(), VisitorZooConfig(1));
        BuildVisitorZoo(*zoo);
        VisitStartOptions options;
        const float share = static_cast<float>(most) / static_cast<float>(options.pace.most);
        options.pace.arrivals *= share;
        options.pace.most = most;
        options.fillArrivals *= share;
        options.fillTicks = 3000;
        const VisitStartReport report = FillForVisit(*zoo, options);
        EXPECT_LE(report.visitors, most);
        std::cout << "CROWD most=" << most << " visitors=" << report.visitors << '\n';
        return zoo;
    };
    EXPECT_EQ(filled(0)->Guests().Count(), 0);
    const auto few = filled(150);
    EXPECT_GT(few->Guests().Count(), 100) << "a small crowd still fills up to near its size";
    for (int step = 0; step < 600; ++step) {
        few->Step();
        ASSERT_LE(few->Guests().Count(), 150);
    }

    auto zoo = filled(500);
    const std::vector<Visitor> before = zoo->Guests().All();
    const Vec2 player = before.front().position;
    zoo->Guests().SetBystander(player);
    const int gone = zoo->Guests().SendHome(200);
    const std::vector<Visitor>& after = zoo->Guests().All();
    EXPECT_LE(static_cast<int>(after.size()), 200);
    EXPECT_EQ(gone, static_cast<int>(before.size() - after.size()));
    // Whole groups: every group left has as many members as it had.
    std::map<std::uint32_t, int> had, has;
    for (const Visitor& v : before) {
        ++had[v.group];
    }
    for (const Visitor& v : after) {
        ++has[v.group];
    }
    for (const auto& [group, count] : has) {
        EXPECT_EQ(count, had[group]) << "group " << group;
    }
    // The farthest went first: nobody who went was nearer the player than everybody kept.
    float farthestKept = 0.0f;
    for (const Visitor& v : after) {
        farthestKept = std::max(farthestKept, Distance(v.position, player));
    }
    for (const Visitor& v : before) {
        if (has.count(v.group) == 0 && zoo->Guests().LeaderOf(v.id) == nullptr) {
            EXPECT_GE(Distance(v.position, player) + 6.0f, farthestKept) << "visitor " << v.id.value;
        }
    }
    EXPECT_EQ(zoo->Guests().SendHome(0), static_cast<int>(after.size()));
    EXPECT_EQ(zoo->Guests().Count(), 0);
    zoo->Step();
}
}  // namespace
}  // namespace zoo
