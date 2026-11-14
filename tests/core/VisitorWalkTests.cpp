// SPDX-License-Identifier: MIT
// How the visitors carry themselves: they walk without snapping round or jolting, a group
// walks as people and not as one body, watchers look at the animals, people who stand keep
// their own ground, sitters sit on their seats facing out, and somebody who stands on the
// path (the player) is walked around.
#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

#include "TestData.hpp"
#include "zoo/sim/Scenario.hpp"
#include "zoo/visitors/VisitorTuning.hpp"

namespace zoo {

namespace {

using namespace visitor_tuning;

bool Travelling(const Visitor& v) {
    return v.activity == VisitorActivity::Walking || v.activity == VisitorActivity::Arriving ||
           v.activity == VisitorActivity::Leaving || v.activity == VisitorActivity::Fleeing;
}

void Advance(Zoo& zoo, std::int64_t ticks) {
    for (std::int64_t i = 0; i < ticks; ++i) {
        zoo.Step();
    }
}

void AdvanceToHour(Zoo& zoo, float hour) {
    while (zoo.Time().Hour() < hour) {
        zoo.Step();
    }
}

std::unique_ptr<Zoo> StarterZoo(std::uint64_t seed = 1) {
    ZooConfig config;
    config.seed = seed;
    auto zoo = std::make_unique<Zoo>(testdata::GameCatalog(), testdata::GameSpecies(), config);
    BuildStarterZoo(*zoo);
    return zoo;
}

/// What a visitor was a tick ago.
struct Before {
    Vec2 position;
    float heading = 0.0f;
    float speed = 0.0f;
    VisitorActivity activity = VisitorActivity::Idle;
};

/// The straight line across the fence from where `v` watches, or false when it stands at none.
bool AcrossTheFence(const Zoo& zoo, const Visitor& v, float& across) {
    const Enclosure* enclosure = zoo.Habitats().Find(v.targetEnclosure);
    if (enclosure == nullptr) {
        return false;
    }
    const TileCoord tile = World::TileAt(v.position);
    const auto spot = std::find_if(enclosure->viewing.begin(), enclosure->viewing.end(),
                                   [tile](const ViewingSpot& s) { return s.tile == tile; });
    if (spot == enclosure->viewing.end()) {
        return false;
    }
    across = HeadingOf(Vec2{static_cast<float>(Step(spot->side).x), static_cast<float>(Step(spot->side).z)});
    return true;
}

}  // namespace

TEST(VisitorWalkTests, WalkersNeitherSnapRoundNorJolt) {
    const std::unique_ptr<Zoo> zoo = StarterZoo();
    AdvanceToHour(*zoo, 11.0f);
    std::unordered_map<std::uint32_t, Before> before;
    // The fastest anybody turns: a walker at a standstill; and the hardest anybody brakes or starts.
    const float turnLimit = (kTurnWalking + kTurnOnTheSpot) * kTickSeconds + 1e-3f;
    const float fastest = kWalkSpeed + kWalkSpeedSpread;
    float worstTurn = 0.0f;
    float worstStep = 0.0f;
    float worstGain = 0.0f;
    std::int64_t walked = 0;
    std::int64_t slid = 0;
    for (int tick = 0; tick < 1200; ++tick) {
        zoo->Step();
        for (const Visitor& v : zoo->Guests().All()) {
            const auto was = before.find(v.id.value);
            const bool walking = v.activity == VisitorActivity::Walking || v.activity == VisitorActivity::Leaving;
            if (was != before.end() && walking && was->second.activity == v.activity) {
                const Before& b = was->second;
                const float turned = std::abs(WrapAngle(v.heading - b.heading));
                const float step = Distance(v.position, b.position);
                worstTurn = std::max(worstTurn, turned);
                worstStep = std::max(worstStep, step);
                worstGain = std::max(worstGain, v.speed - b.speed);
                EXPECT_LE(turned, turnLimit) << "visitor " << v.id.value << " snapped round at tick "
                                             << zoo->Time().tick;
                // Nobody covers more ground in a tick than a quick adult's stride allows (a child
                // catching up with its people runs a little).
                EXPECT_LE(step, 1.5f * fastest * kTickSeconds + 0.02f)
                    << "visitor " << v.id.value << " jumped at tick " << zoo->Time().tick;
                // Feet and ground agree: what it says its speed is, is what it covered.
                ++walked;
                if (std::abs(step - v.speed * kTickSeconds) > 0.03f) {
                    ++slid;
                }
            }
            before[v.id.value] = Before{v.position, v.heading, v.speed, v.activity};
        }
    }
    ASSERT_GT(walked, 20000);
    EXPECT_LE(worstGain, kGainSpeed * kTickSeconds + 1e-3f) << "somebody was at full stride in one step";
    EXPECT_GT(worstTurn, 0.0f);
    EXPECT_GT(worstStep, 0.0f);
    EXPECT_LT(static_cast<double>(slid), 0.01 * static_cast<double>(walked))
        << slid << " of " << walked << " steps were not the length of the walker's speed";
}

TEST(VisitorWalkTests, AWalkingGroupIsPeopleNotOneBody) {
    const std::unique_ptr<Zoo> zoo = StarterZoo();
    AdvanceToHour(*zoo, 11.0f);
    std::int64_t pairs = 0;
    std::int64_t merged = 0;
    std::int64_t members = 0;
    std::int64_t strayed = 0;
    for (int step = 0; step < 120; ++step) {
        Advance(*zoo, 10);
        const std::vector<Visitor>& all = zoo->Guests().All();
        for (std::size_t i = 0; i < all.size(); ++i) {
            const Visitor& a = all[i];
            if (a.activity != VisitorActivity::Walking) {
                continue;
            }
            for (std::size_t k = i + 1; k < all.size() && all[k].group == a.group; ++k) {
                if (all[k].activity != VisitorActivity::Walking) {
                    continue;
                }
                ++pairs;
                if (Distance(a.position, all[k].position) < 0.3f) {
                    ++merged;
                }
            }
            if (!a.leader) {
                const Visitor* lead = zoo->Guests().LeaderOf(a.id);
                ASSERT_NE(lead, nullptr);
                if (lead->activity == VisitorActivity::Walking) {
                    const float away = Distance(a.position, lead->position);
                    ++members;
                    if (away > 4.0f) {
                        ++strayed;
                    }
                }
            }
        }
    }
    ASSERT_GT(pairs, 2000);
    // People of a group used to walk on one line to one point and so inside each other; in
    // rows, two abreast, only those who squeeze past something come that close for a moment.
    EXPECT_LT(static_cast<double>(merged), 0.03 * static_cast<double>(pairs))
        << merged << " of " << pairs << " pairs of a walking group were inside each other";
    EXPECT_LT(static_cast<double>(strayed), 0.05 * static_cast<double>(members))
        << strayed << " of " << members << " walked more than four metres from their leader";
}

TEST(VisitorWalkTests, WatchersLookAtTheAnimalsNotStraightAhead) {
    const std::unique_ptr<Zoo> zoo = StarterZoo();
    AdvanceToHour(*zoo, 12.0f);
    int checked = 0;
    int atAnAnimal = 0;
    int offTheLine = 0;
    for (int step = 0; step < 40; ++step) {
        Advance(*zoo, 30);
        for (const Visitor& v : zoo->Guests().All()) {
            float across = 0.0f;
            if (v.activity != VisitorActivity::Watching || !AcrossTheFence(*zoo, v, across)) {
                continue;
            }
            ++checked;
            // Never turned away from the fence.
            EXPECT_LE(std::abs(WrapAngle(v.face - across)), kAimSpread + 1e-3f)
                << "visitor " << v.id.value << " looks away from the fence";
            if (std::abs(WrapAngle(v.face - across)) > 0.05f) {
                ++offTheLine;
            }
            for (const Animal& animal : zoo->Fauna().All()) {
                if (animal.home != v.targetEnclosure) {
                    continue;
                }
                if (std::abs(WrapAngle(HeadingOf(animal.position - v.position) - v.face)) < 0.12f) {
                    ++atAnAnimal;
                    break;
                }
            }
        }
    }
    ASSERT_GT(checked, 200);
    EXPECT_GT(atAnAnimal * 2, checked) << "of " << checked << " watchers " << atAnAnimal << " looked at an animal";
    EXPECT_GT(offTheLine * 3, checked) << "watchers stare straight across the fence";
}

TEST(VisitorWalkTests, PeopleWhoStandKeepTheirOwnGround) {
    const std::unique_ptr<Zoo> zoo = StarterZoo();
    AdvanceToHour(*zoo, 11.0f);
    std::int64_t standing = 0;
    std::int64_t pairs = 0;
    for (int step = 0; step < 150; ++step) {
        Advance(*zoo, 40);
        const std::vector<Visitor>& all = zoo->Guests().All();
        const auto stands = [](const Visitor& v) {
            return !v.seat && v.speed == 0.0f &&
                   (v.activity == VisitorActivity::Eating || v.activity == VisitorActivity::Reading ||
                    v.activity == VisitorActivity::Watching || v.activity == VisitorActivity::Queueing);
        };
        for (std::size_t i = 0; i < all.size(); ++i) {
            if (!stands(all[i])) {
                continue;
            }
            ++standing;
            for (std::size_t k = i + 1; k < all.size(); ++k) {
                if (!stands(all[k]) || Distance(all[i].position, all[k].position) > 2.0f) {
                    continue;
                }
                ++pairs;
                if (Distance(all[i].position, all[k].position) < 0.3f) {
                    ADD_FAILURE() << all[i].id.value << " (" << ToString(all[i].activity) << ") and "
                                  << all[k].id.value << " (" << ToString(all[k].activity) << ") stand "
                                  << Distance(all[i].position, all[k].position) << " m apart at tick "
                                  << zoo->Time().tick;
                }
            }
        }
    }
    EXPECT_GT(standing, 1000);
}

TEST(VisitorWalkTests, ASitterSitsOnItsSeatAndFacesOut) {
    const std::unique_ptr<Zoo> zoo = StarterZoo();
    AdvanceToHour(*zoo, 11.0f);
    int onBenches = 0;
    int atTables = 0;
    for (int step = 0; step < 150; ++step) {
        Advance(*zoo, 40);
        const std::vector<Visitor>& all = zoo->Guests().All();
        for (std::size_t i = 0; i < all.size(); ++i) {
            const Visitor& v = all[i];
            const bool sits = v.activity == VisitorActivity::Sitting ||
                              (v.activity == VisitorActivity::Eating && static_cast<bool>(v.seat));
            if (!sits || !v.seat) {
                continue;
            }
            const Prop* prop = zoo->Objects().FindProp(v.seat);
            ASSERT_NE(prop, nullptr) << "visitor " << v.id.value << " sits on nothing";
            const Vec2 forward = HeadingVector(prop->heading);
            const Vec2 right{forward.z, -forward.x};
            const Vec2 offset = v.position - prop->position;
            const int seats = static_cast<int>(
                zoo->Definitions().props[static_cast<std::size_t>(prop->def)].capacity);
            // On the seat, not beside it.
            EXPECT_EQ(v.speed, 0.0f);
            EXPECT_LT(Distance(v.position, v.target), 1e-3f) << "visitor " << v.id.value << " sits off its seat";
            if (seats <= 3) {
                // A bench: on its line, looking the way the bench looks.
                ++onBenches;
                EXPECT_LT(std::abs(Dot(offset, forward)), 0.02f);
                EXPECT_LT(std::abs(Dot(offset, right)), 0.25f * static_cast<float>(seats) + 0.02f);
                EXPECT_LT(std::abs(WrapAngle(v.face - prop->heading)), 1e-3f);
            } else {
                // A table: on one of its two rows, looking across it.
                ++atTables;
                EXPECT_NEAR(std::abs(Dot(offset, forward)), 0.72f, 0.02f);
                EXPECT_LT(Dot(HeadingVector(v.face), offset), 0.0f)
                    << "visitor " << v.id.value << " sits with its back to the table";
            }
            // It has turned to face out by the time it has sat a moment (5 rad/s).
            if (std::abs(WrapAngle(v.heading - v.face)) > 1e-3f) {
                continue;
            }
            // Its neighbours on the same seat sit beside it, not on its lap.
            for (std::size_t k = i + 1; k < all.size(); ++k) {
                const Visitor& other = all[k];
                if (other.seat == v.seat && other.speed == 0.0f && !Travelling(other)) {
                    EXPECT_GT(Distance(v.position, other.position), 0.45f)
                        << v.id.value << " and " << other.id.value << " share a seat";
                }
            }
        }
    }
    EXPECT_GT(onBenches, 20);
    EXPECT_GT(atTables, 20);
}

TEST(VisitorWalkTests, SomebodyStandingOnThePathIsWalkedAround) {
    // The same zoo twice: in one the player stands where a walker is about to pass.
    const std::unique_ptr<Zoo> open = StarterZoo(3);
    const std::unique_ptr<Zoo> blocked = StarterZoo(3);
    AdvanceToHour(*open, 11.0f);
    AdvanceToHour(*blocked, 11.0f);
    ASSERT_EQ(open->StateHash(), blocked->StateHash());
    // Somebody who walks alone, at full stride and with a long way still to go.
    const Visitor* chosen = nullptr;
    for (const Visitor& v : open->Guests().All()) {
        const std::vector<Visitor>& all = open->Guests().All();
        const std::size_t index = static_cast<std::size_t>(&v - all.data());
        const bool alone = v.leader && (index + 1 == all.size() || all[index + 1].group != v.group);
        if (alone && v.activity == VisitorActivity::Walking && v.speed > 1.0f &&
            Distance(v.position, v.target) > 12.0f) {
            chosen = &v;
            break;
        }
    }
    ASSERT_NE(chosen, nullptr);
    const VisitorId id = chosen->id;
    const auto find = [id](const Zoo& zoo) -> const Visitor* {
        for (const Visitor& v : zoo.Guests().All()) {
            if (v.id == id) {
                return &v;
            }
        }
        return nullptr;
    };
    // Where it is three seconds from now when nobody is in its way.
    std::vector<Vec2> track;
    for (int tick = 0; tick < 120; ++tick) {
        open->Step();
        const Visitor* v = find(*open);
        ASSERT_NE(v, nullptr);
        track.push_back(v->position);
    }
    const Vec2 spot = track[60];
    float nearestOpen = 1e9f;
    for (const Vec2& p : track) {
        nearestOpen = std::min(nearestOpen, Distance(p, spot));
    }
    ASSERT_LT(nearestOpen, 0.01f);
    blocked->Guests().SetBystander(spot);
    float nearest = 1e9f;
    float everybody = 1e9f;
    for (int tick = 0; tick < 120; ++tick) {
        blocked->Step();
        const Visitor* v = find(*blocked);
        ASSERT_NE(v, nullptr);
        nearest = std::min(nearest, Distance(v->position, spot));
        for (const Visitor& other : blocked->Guests().All()) {
            if (Travelling(other)) {
                everybody = std::min(everybody, Distance(other.position, spot));
            }
        }
    }
    EXPECT_GT(nearest, 0.35f) << "the walker went through the player";
    EXPECT_GT(everybody, 0.3f) << "somebody went through the player";
    // It did not stop for good either: it got past.
    const Visitor* after = find(*blocked);
    ASSERT_NE(after, nullptr);
    EXPECT_GT(Distance(after->position, track[0]), 0.6f * Distance(track.back(), track[0]));
    // Who stands there is no part of the zoo: taken away, nothing remembers it.
    blocked->Guests().ClearBystander();
}

TEST(VisitorWalkTests, ChildrenAndOldPeopleWalkSlower) {
    const std::unique_ptr<Zoo> zoo = StarterZoo();
    AdvanceToHour(*zoo, 12.0f);
    int adults = 0;
    int old = 0;
    for (const Visitor& v : zoo->Guests().All()) {
        const float pace = PaceOf(v);
        if (v.child) {
            EXPECT_FALSE(Elderly(v));
            EXPECT_FLOAT_EQ(pace, kChildSpeed);
            continue;
        }
        ++adults;
        if (Elderly(v)) {
            ++old;
            EXPECT_LE(pace, (kWalkSpeed + kWalkSpeedSpread) * kElderlyPace + 1e-4f);
            EXPECT_LT(pace, kWalkSpeed);
        } else {
            EXPECT_GE(pace, kWalkSpeed);
            EXPECT_LE(pace, kWalkSpeed + kWalkSpeedSpread + 1e-4f);
        }
    }
    ASSERT_GT(adults, 60);
    EXPECT_GT(old, adults / 25);
    EXPECT_LT(old, adults / 4);
    EXPECT_LT(kChildSpeed, kWalkSpeed);
}

}  // namespace zoo
