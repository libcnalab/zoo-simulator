// SPDX-License-Identifier: MIT
// Visitors in the starter zoo, through the Zoo: who comes, what they do there,
// how they leave.
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "TestData.hpp"
#include "zoo/sim/Scenario.hpp"
#include "zoo/visitors/VisitorTuning.hpp"

namespace zoo {

namespace {

struct DayStats {
    int arrived = 0;
    int left = 0;
    int peak = 0;
    double happinessLeft = 0.0;  ///< mean of the leaving visitors, 0..1
    int bought = 0;
    int donated = 0;
    int littered = 0;
    int scared = 0;
    std::array<int, static_cast<std::size_t>(VisitorThought::Count)> thoughts{};
    std::array<std::int64_t, static_cast<std::size_t>(VisitorActivity::Count)> activityTicks{};
    int offPath = 0;        ///< visitor-ticks on a tile that is not paved (seats are beside the paths)
    std::string astrayWho;  ///< the first few of them
    int astray = 0;         ///< those of them not at a seat and with no paved tile next to them
    std::array<int, static_cast<std::size_t>(VisitorActivity::Count)> offPathBy{};
    /// Visitor-ticks on the way to each kind of goal, and those of them off the paths.
    std::array<std::int64_t, static_cast<std::size_t>(VisitorGoal::Count)> walkTicks{};
    std::array<int, static_cast<std::size_t>(VisitorGoal::Count)> walkOffPath{};
    std::array<int, static_cast<std::size_t>(VisitorGoal::Count)> trips{};
    float longestStill = 0;       ///< the longest a walking visitor made no headway, seconds
    double enclosuresSeen = 0.0;  ///< mean per leaving group
    int groupsLeft = 0;
    double stepMs = 0.0;  ///< wall time of all the steps
};

/// Runs the zoo to the end of the day it is in, watching the visitors.
DayStats RunDay(Zoo& zoo) {
    DayStats stats;
    const std::int64_t day = zoo.Time().Day();
    std::vector<Event> events;
    std::map<std::uint32_t, std::pair<Vec2, int>> walking;  // id -> where it was, ticks without headway
    std::map<std::uint32_t, int> seen;                      // leader -> enclosures seen
    std::map<std::uint32_t, VisitorGoal> goals;             // leader -> what it was after a tick ago
    std::vector<Vec2> seats;
    for (const Prop& prop : zoo.Objects().Props()) {
        if (zoo.Definitions().props[static_cast<std::size_t>(prop.def)].role == PropRole::Bench) {
            seats.push_back(prop.position);
        }
    }
    const auto start = std::chrono::steady_clock::now();
    while (zoo.Time().Day() == day) {
        zoo.Step();
        const Visitors& guests = zoo.Guests();
        stats.peak = std::max(stats.peak, guests.Count());
        for (const Visitor& v : guests.All()) {
            ++stats.activityTicks[static_cast<std::size_t>(v.activity)];
            const bool walks =
                v.activity == VisitorActivity::Walking || v.activity == VisitorActivity::Leaving;
            const bool off = !zoo.Routes().Passable(World::TileAt(v.position), NavProfile::Visitor());
            if (walks) {
                ++stats.walkTicks[static_cast<std::size_t>(v.goal)];
                stats.walkOffPath[static_cast<std::size_t>(v.goal)] += off ? 1 : 0;
                if (v.leader && goals[v.id.value] != v.goal) {
                    ++stats.trips[static_cast<std::size_t>(v.goal)];
                }
            }
            if (v.leader) {
                goals[v.id.value] = v.goal;
            }
            if (!v.inside && v.ticketPaid && off) {
                bool near = false;
                const TileCoord at = World::TileAt(v.position);
                for (int dz = -1; dz <= 1 && !near; ++dz) {
                    for (int dx = -1; dx <= 1 && !near; ++dx) {
                        near = zoo.Routes().Passable({at.x + dx, at.z + dz}, NavProfile::Visitor());
                    }
                }
                // Benches and picnic tables stand on the grass beside the paths.
                const bool seat = std::any_of(seats.begin(), seats.end(), [&v](Vec2 at_seat) {
                    return DistanceSquared(at_seat, v.position) < 3.5f * 3.5f;
                });
                if (!near && !seat) {
                    if (++stats.astray <= 5) {
                        stats.astrayWho +=
                            "    visitor " + std::to_string(v.id.value) + " " +
                            std::string(ToString(v.activity)) + " for " + std::string(ToString(v.goal)) +
                            " after " + std::string(ToString(v.previousGoal)) + " at " +
                            std::to_string(v.position.x) + ", " + std::to_string(v.position.z) + " tick " +
                            std::to_string(zoo.Time().tick) + "\n";
                    }
                }
                ++stats.offPath;
                ++stats.offPathBy[static_cast<std::size_t>(v.activity)];
            }
            if (v.leader) {
                std::vector<std::uint32_t> distinct = v.seenEnclosures;
                std::sort(distinct.begin(), distinct.end());
                distinct.erase(std::unique(distinct.begin(), distinct.end()), distinct.end());
                seen[v.id.value] = static_cast<int>(distinct.size());
            }
            // Somebody at the end of its own route who waits for the rest of its group is not
            // stuck: only those with a way left count.
            if ((v.activity == VisitorActivity::Walking || v.activity == VisitorActivity::Arriving ||
                 v.activity == VisitorActivity::Leaving) &&
                v.routeIndex < v.route.size()) {
                auto& entry = walking.try_emplace(v.id.value, v.position, 0).first->second;
                if (Distance(entry.first, v.position) > 0.5f) {
                    entry = {v.position, 0};
                } else {
                    ++entry.second;
                    stats.longestStill =
                        std::max(stats.longestStill,
                                 static_cast<float>(entry.second) / static_cast<float>(kTicksPerSecond));
                }
            } else {
                walking.erase(v.id.value);
            }
        }
        events.clear();
        zoo.DrainEvents(events);
        for (const Event& e : events) {
            switch (e.type) {
                case EventType::VisitorArrived:
                    ++stats.arrived;
                    break;
                case EventType::VisitorLeft:
                    ++stats.left;
                    stats.happinessLeft += static_cast<double>(e.detail) / 100.0;
                    if (const auto it = seen.find(e.visitor.value); it != seen.end()) {
                        stats.enclosuresSeen += it->second;
                        ++stats.groupsLeft;
                    }
                    break;
                case EventType::VisitorBought:
                    ++stats.bought;
                    break;
                case EventType::VisitorDonated:
                    ++stats.donated;
                    break;
                case EventType::VisitorLittered:
                    ++stats.littered;
                    break;
                case EventType::VisitorScared:
                    ++stats.scared;
                    break;
                case EventType::VisitorThought:
                    if (e.detail > 0 && e.detail < static_cast<int>(VisitorThought::Count)) {
                        ++stats.thoughts[static_cast<std::size_t>(e.detail)];
                    }
                    break;
                default:
                    break;
            }
        }
    }
    stats.stepMs =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    if (stats.left > 0) {
        stats.happinessLeft /= stats.left;
    }
    if (stats.groupsLeft > 0) {
        stats.enclosuresSeen /= stats.groupsLeft;
    }
    return stats;
}

void Print(const char* title, const Zoo& zoo, const DayStats& s) {
    std::printf("[%s] arrived %d, left %d, peak %d, happiness on leaving %.2f, rating %.2f, education %.2f\n",
                title, s.arrived, s.left, s.peak, s.happinessLeft, static_cast<double>(zoo.Guests().Rating()),
                static_cast<double>(zoo.Guests().MeanEducation()));
    std::printf("  enclosures seen per group %.1f, bought %d, donated %d, littered %d, scared %d\n",
                s.enclosuresSeen, s.bought, s.donated, s.littered, s.scared);
    std::printf(
        "  off the paths %d visitor-ticks (%d astray), longest standstill while walking %.1f s, %.0f ms\n",
        s.offPath, s.astray, static_cast<double>(s.longestStill), s.stepMs);
    std::printf("  activity ticks:");
    for (std::size_t i = 0; i < s.activityTicks.size(); ++i) {
        if (s.activityTicks[i] > 0) {
            std::printf(" %s %lld", std::string(ToString(static_cast<VisitorActivity>(i))).c_str(),
                        static_cast<long long>(s.activityTicks[i]));
        }
    }
    std::printf("\n  off the paths:");
    for (std::size_t i = 0; i < s.offPathBy.size(); ++i) {
        if (s.offPathBy[i] > 0) {
            std::printf(" %s %d", std::string(ToString(static_cast<VisitorActivity>(i))).c_str(),
                        s.offPathBy[i]);
        }
    }
    std::printf("\n  on the way to (trips, seconds per trip, share off the paths):");
    for (std::size_t i = 0; i < s.walkTicks.size(); ++i) {
        if (s.walkTicks[i] > 0) {
            std::printf(" %s %d %.0f %.0f%%", std::string(ToString(static_cast<VisitorGoal>(i))).c_str(),
                        s.trips[i],
                        static_cast<double>(s.walkTicks[i]) / kTicksPerSecond / std::max(1, s.trips[i]),
                        100.0 * s.walkOffPath[i] / static_cast<double>(s.walkTicks[i]));
        }
    }
    std::printf("\n  thoughts:");
    for (std::size_t i = 1; i < s.thoughts.size(); ++i) {
        if (s.thoughts[i] > 0) {
            std::printf(" %s %d", std::string(ToString(static_cast<VisitorThought>(i))).c_str(),
                        s.thoughts[i]);
        }
    }
    std::printf("\n  balance %s\n", FormatMoney(zoo.Accounts().Balance()).c_str());
}

}  // namespace

TEST(VisitorDayTests, ADayInTheStarterZoo) {
    Zoo zoo(testdata::GameCatalog(), testdata::GameSpecies());
    BuildStarterZoo(zoo);
    const Money before = zoo.Accounts().Balance();
    const DayStats first = RunDay(zoo);
    Print("day 1", zoo, first);
    const DayStats second = RunDay(zoo);
    Print("day 2", zoo, second);

    EXPECT_GT(first.arrived, 80);
    EXPECT_LT(first.arrived, 500);
    EXPECT_EQ(zoo.Guests().Count(), 0) << "nobody stays the night";
    EXPECT_EQ(first.arrived, first.left);
    EXPECT_GT(first.happinessLeft, 0.55);
    EXPECT_GT(first.enclosuresSeen, 1.5);
    EXPECT_GT(first.bought, first.arrived / 4);
    for (const DayStats* day : {&first, &second}) {
        EXPECT_EQ(day->astray, 0) << "nobody roams the lawns:\n" << day->astrayWho;
        EXPECT_LT(day->offPath, day->arrived * 250) << "and that is where the time off the paving goes";
        EXPECT_LT(day->longestStill, 20.0f);
        EXPECT_EQ(day->arrived, day->left);
        // Getting about is half of a visit, not all of it. (13: since walkers steer, give way
        // and slow down among the people standing at a rail (G4, 2026-11-14), the second day's
        // way to a viewing place takes 129 s instead of 117 s; walking and leaving were 11.7
        // times the watching before, 12.2 times after.)
        const auto ticks = [day](VisitorActivity a) {
            return day->activityTicks[static_cast<std::size_t>(a)];
        };
        EXPECT_GT(ticks(VisitorActivity::Watching) * 13,
                  ticks(VisitorActivity::Walking) + ticks(VisitorActivity::Leaving));
#if !defined(__SANITIZE_ADDRESS__) && !defined(__SANITIZE_THREAD__)
        // A budget for the optimised build: an instrumented one runs several times slower
        // and says nothing about it.
        EXPECT_LT(day->stepMs / kTicksPerDay, 1.0) << "milliseconds a tick for the whole zoo";
#endif
    }
    EXPECT_EQ(first.scared, 0);
    EXPECT_GT(zoo.Accounts().Balance(), before - Dollars(4000)) << "the starter zoo roughly pays its way";
}

// Visitors::Spread: the zoo one walks into in mid-morning has people all over it.
TEST(VisitorDayTests, SpreadSetsTheGroupsDownAllOverThePathsAndTheyCarryOn) {
    const auto filled = [] {
        auto zoo = std::make_unique<Zoo>(testdata::GameCatalog(), testdata::GameSpecies());
        BuildStarterZoo(*zoo);
        zoo->SetCalendar(1, 10.5f);
        zoo->SetDayPace(0);
        for (int tick = 0; tick < 3000; ++tick) {
            zoo->Step();
        }
        return zoo;
    };
    const std::unique_ptr<Zoo> zoo = filled();
    ASSERT_GT(zoo->Guests().Count(), 20);
    const int moved = zoo->SpreadVisitors();
    EXPECT_GT(moved, zoo->Guests().Count() / 3);
    int settled = 0;
    std::set<std::uint32_t> seededViews;
    for (const Visitor& v : zoo->Guests().All()) {
        const bool idle = v.activity == VisitorActivity::Idle && v.goal == VisitorGoal::None;
        const bool viewing = v.activity == VisitorActivity::Watching && v.goal == VisitorGoal::Watch;
        if (!idle && !viewing) {
            continue;
        }
        ++settled;
        const TileCoord tile = World::TileAt(v.position);
        EXPECT_NE(zoo->Map().At(tile).path, PathKind::None) << "a group is set down on a path";
        EXPECT_EQ(zoo->Habitats().RegionAt(tile), Region::Public);
        if (viewing) {
            const Enclosure* home = zoo->Habitats().Find(v.targetEnclosure);
            ASSERT_NE(home, nullptr);
            EXPECT_TRUE(std::any_of(home->viewing.begin(), home->viewing.end(),
                                    [tile](const ViewingSpot& spot) { return spot.tile == tile; }));
            seededViews.insert(home->id.value);
        }
    }
    EXPECT_GE(settled, moved);
    EXPECT_EQ(seededViews.size(), zoo->Habitats().All().size());

    // The same zoo spreads the same way.
    const std::unique_ptr<Zoo> twin = filled();
    twin->SpreadVisitors();
    EXPECT_EQ(twin->StateHash(), zoo->StateHash());

    // And the people carry on from where they were set down: they watch, nobody is lost.
    std::map<std::uint32_t, std::pair<Vec2, int>> still;
    int longest = 0;
    for (int tick = 0; tick < 2400; ++tick) {
        zoo->Step();
        for (const Visitor& v : zoo->Guests().All()) {
            auto& was = still[v.id.value];
            const bool stuck =
                v.activity == VisitorActivity::Walking && DistanceSquared(v.position, was.first) < 0.000001f;
            was.second = stuck ? was.second + 1 : 0;
            longest = std::max(longest, was.second);
            was.first = v.position;
        }
    }
    int watching = 0;
    int lost = 0;
    for (const Visitor& v : zoo->Guests().All()) {
        watching += v.activity == VisitorActivity::Watching ? 1 : 0;
        lost += v.thought == VisitorThought::LostWay ? 1 : 0;
    }
    EXPECT_GT(watching, 0);
    EXPECT_EQ(lost, 0);
    EXPECT_LT(longest, 20 * kTicksPerSecond) << "nobody walks on the spot";
}

}  // namespace zoo
