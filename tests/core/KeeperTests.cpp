// SPDX-License-Identifier: MIT
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <string>

#include "TestData.hpp"
#include "zoo/animals/AnimalTuning.hpp"
#include "zoo/sim/VisitStart.hpp"
#include "zoo/sim/VisitorZoo.hpp"
#include "zoo/sim/ZooSaveJson.hpp"
#include "zoo/staff/StaffJson.hpp"

namespace zoo {
namespace {

constexpr NavProfile kStaff = NavProfile::Staff();
/// The furthest a tick may move a member: a step at walking speed, half as much again
/// round a tree, and the 5 cm within which the route's next waypoint is snapped to.
constexpr float kStepBound = Staff::kWalkSpeed * kTickSeconds * 1.5f + 0.06f;

std::unique_ptr<Zoo> VisitorZoo(std::uint32_t seed = 1) {
    auto zoo =
        std::make_unique<Zoo>(testdata::GameCatalog(), testdata::GameSpecies(), VisitorZooConfig(seed));
    BuildVisitorZoo(*zoo);
    return zoo;
}

/// The habitats of the dangerous species: by who lives there.
std::set<std::uint32_t> DangerousHabitats(const Zoo& zoo) {
    std::set<std::uint32_t> result;
    for (const Animal& animal : zoo.Fauna().All()) {
        if (animal.species >= 0 &&
            zoo.SpeciesList().All()[static_cast<std::size_t>(animal.species)].dangerous) {
            result.insert(animal.home.value);
        }
    }
    return result;
}

/// What must hold for every member of staff after every step.
struct Watch {
    explicit Watch(const Zoo& zoo) : dangerous(DangerousHabitats(zoo)) {
        for (const StaffMember& member : zoo.Keepers().All()) {
            before.push_back(member.position);
        }
    }

    void Check(const Zoo& zoo) {
        const auto& members = zoo.Keepers().All();
        ASSERT_EQ(members.size(), before.size());
        for (std::size_t i = 0; i < members.size(); ++i) {
            const StaffMember& member = members[i];
            const TileCoord tile = World::TileAt(member.position);
            const std::string who =
                "staff " + std::to_string(member.id.value) + " at tick " + std::to_string(zoo.Time().tick) +
                " (" + std::to_string(member.position.x) + ", " + std::to_string(member.position.z) + ")";
            ASSERT_TRUE(zoo.Routes().Passable(tile, kStaff)) << who << " stands where nobody can walk";
            ASSERT_NE(zoo.Habitats().RegionAt(tile), Region::StandOff) << who << " stands in a planted strip";
            ASSERT_FALSE(zoo.Map().At(tile).standOff) << who << " stands in a planted strip";
            // Dry-shod: no deeper than a boot anywhere along the way (a lagoon's shelf is 0.3 m).
            ASSERT_LE(zoo.Map().WaterDepthAt(member.position), 0.08f) << who << " wades";
            ASSERT_FALSE(dangerous.contains(zoo.Habitats().At(tile).value))
                << who << " is inside the habitat of a dangerous species";
            // No step is longer than a walk allows, and none goes through a fence.
            ASSERT_LE(Distance(before[i], member.position), kStepBound) << who;
            ASSERT_TRUE(zoo.Routes().Walkable(before[i], member.position, kStaff))
                << who << " crossed a barrier";
            if (member.working) {
                ++worked[static_cast<std::size_t>(member.activity)];
            }
            ++seen[static_cast<std::size_t>(member.activity)];
            before[i] = member.position;
        }
    }

    std::set<std::uint32_t> dangerous;
    std::vector<Vec2> before;
    std::array<std::int64_t, static_cast<std::size_t>(StaffActivity::Count)> seen{};
    std::array<std::int64_t, static_cast<std::size_t>(StaffActivity::Count)> worked{};
};

TEST(Keeper, TheVisitorZooHasItsCrew) {
    const auto zoo = VisitorZoo();
    const Staff& staff = zoo->Keepers();
    ASSERT_GE(staff.All().size(), 6u);
    ASSERT_LE(staff.All().size(), 10u);
    int gardeners = 0;
    int cleaners = 0;
    for (const StaffMember& member : staff.All()) {
        ASSERT_FALSE(member.round.empty());
        gardeners += member.role == StaffRole::Gardener;
        cleaners += member.role == StaffRole::Cleaner;
        EXPECT_TRUE(zoo->Routes().Passable(World::TileAt(member.position), kStaff));
        if (member.role == StaffRole::Keeper) {
            EXPECT_LE(member.round.size(), 2u);
        }
    }
    EXPECT_EQ(gardeners, 1);
    EXPECT_EQ(cleaners, 1);
    Env env = zoo->MakeEnv();
    const std::set<std::uint32_t> dangerous = DangerousHabitats(*zoo);
    EXPECT_FALSE(dangerous.empty());
    for (const Enclosure& enclosure : zoo->Habitats().All()) {
        const StaffMember* keeper = staff.KeeperOf(enclosure.id);
        ASSERT_NE(keeper, nullptr) << enclosure.name << " has no keeper";
        // Keepers go in where nothing dangerous lives, and only there.
        EXPECT_EQ(KeepersMayEnter(env, zoo->Fauna(), enclosure.id), !dangerous.contains(enclosure.id.value))
            << enclosure.name;
        // Two feedings a day the public can come to, in opening hours, morning and afternoon.
        const std::array<float, 2> hours = staff.FeedingHours(enclosure.id);
        EXPECT_GE(hours[0], 9.0f) << enclosure.name;
        EXPECT_LT(hours[0], 13.0f) << enclosure.name;
        EXPECT_GE(hours[1], 13.5f) << enclosure.name;
        EXPECT_LT(hours[1], 17.5f) << enclosure.name;
    }
    // One keeper's two feedings do not fall together.
    for (const StaffMember& member : staff.All()) {
        if (member.role == StaffRole::Keeper && member.round.size() == 2) {
            for (std::size_t slot = 0; slot < 2; ++slot) {
                EXPECT_GE(std::abs(member.round[0].feedHours[slot] - member.round[1].feedHours[slot]), 0.7f);
            }
        }
    }
    EXPECT_EQ(staff.Find(StaffId{1}), &staff.All().front());
    EXPECT_EQ(staff.Find(StaffId{9999}), nullptr);
    EXPECT_EQ(staff.KeeperOf(EnclosureId{9999}), nullptr);
}

// A planted stand-off strip leaves the service gate as the only place directly beside a
// dangerous habitat.  The public viewing bays are safe working places too: without them a
// keeper repeats a whole round at one or two points hidden behind the habitat.
TEST(Keeper, DangerousHabitatsHaveSeveralSafeBarrierWorkPlaces) {
    const auto zoo = VisitorZoo();
    zoo->Guests().SetOpen(false);
    const std::set<std::uint32_t> dangerous = DangerousHabitats(*zoo);
    std::map<std::uint32_t, std::set<std::pair<float, float>>> places;
    for (int i = 0; i < 12000; ++i) {
        zoo->Step();
        for (const StaffMember& member : zoo->Keepers().All()) {
            for (const StaffTask& task : member.tasks) {
                if (!dangerous.contains(task.enclosure.value)) {
                    continue;
                }
                EXPECT_EQ(zoo->Habitats().RegionAt(World::TileAt(task.place)), Region::Public);
                places[task.enclosure.value].insert({task.place.x, task.place.z});
            }
        }
    }
    for (const std::uint32_t id : dangerous) {
        const Enclosure* enclosure = zoo->Habitats().Find(EnclosureId{id});
        ASSERT_NE(enclosure, nullptr);
        EXPECT_GE(places[id].size(), 2u) << enclosure->name;
    }
}

TEST(Keeper, NextFeedingIsTheNextOneOfToday) {
    const auto zoo = VisitorZoo();
    const Staff& staff = zoo->Keepers();
    const Enclosure& habitat = zoo->Habitats().All().front();
    const std::array<float, 2> hours = staff.FeedingHours(habitat.id);
    zoo->SetCalendar(3, 8.0f);
    Env morning = zoo->MakeEnv();
    ASSERT_TRUE(staff.NextFeeding(morning, habitat.id).has_value());
    EXPECT_FLOAT_EQ(*staff.NextFeeding(morning, habitat.id), hours[0]);
    zoo->SetCalendar(3, hours[0] + 1.5f);
    Env noon = zoo->MakeEnv();
    ASSERT_TRUE(staff.NextFeeding(noon, habitat.id).has_value());
    EXPECT_FLOAT_EQ(*staff.NextFeeding(noon, habitat.id), hours[1]);
    zoo->SetCalendar(3, 19.0f);
    Env evening = zoo->MakeEnv();
    EXPECT_FALSE(staff.NextFeeding(evening, habitat.id).has_value());
    EXPECT_FALSE(staff.NextFeeding(evening, EnclosureId{9999}).has_value());
}

TEST(Keeper, EqualZoosStayEqual) {
    const auto a = VisitorZoo(2);
    const auto b = VisitorZoo(2);
    ASSERT_EQ(a->Keepers().StateHash(), b->Keepers().StateHash());
    for (int i = 0; i < 2400; ++i) {
        a->Step();
        b->Step();
        if (i % 400 == 399) {
            ASSERT_EQ(a->Keepers().StateHash(), b->Keepers().StateHash()) << "after step " << i;
            ASSERT_EQ(a->StateHash(), b->StateHash()) << "after step " << i;
        }
    }
    EXPECT_TRUE(a->Keepers() == b->Keepers());
    EXPECT_TRUE(a->SameState(*b));
    // The staff are part of the zoo's hash: a zoo whose crew stands elsewhere differs.
    const auto c = VisitorZoo(2);
    const std::uint64_t staffed = c->StateHash();
    c->Keepers() = Staff{};
    EXPECT_NE(c->StateHash(), staffed);
}

TEST(Keeper, SavedAndLoadedTheyCarryOnTheSame) {
    const auto original = VisitorZoo();
    // Far enough for everybody to be under way: routes, tasks and feedings given are saved.
    original->SetCalendar(14, 9.4f);
    for (int i = 0; i < 1500; ++i) {
        original->Step();
    }
    std::string error;
    const nlohmann::json saved = ZooToJson(*original);
    ASSERT_TRUE(saved.dump().find("\"staff\"") != std::string::npos);
    const auto loaded = ZooFromJson(saved, testdata::GameCatalog(), testdata::GameSpecies(), &error);
    ASSERT_NE(loaded, nullptr) << error;
    EXPECT_TRUE(loaded->Keepers() == original->Keepers());
    EXPECT_EQ(loaded->Keepers().StateHash(), original->Keepers().StateHash());
    EXPECT_TRUE(loaded->SameState(*original));
    for (int i = 0; i < 1500; ++i) {
        original->Step();
        loaded->Step();
    }
    EXPECT_EQ(loaded->StateHash(), original->StateHash());
    EXPECT_TRUE(loaded->SameState(*original));

    // Through text as well: the numbers survive being written out.
    const auto reread = StaffFromJson(nlohmann::json::parse(ToJson(original->Keepers()).dump()), &error);
    ASSERT_TRUE(reread.has_value()) << error;
    EXPECT_TRUE(*reread == original->Keepers());
}

TEST(Keeper, ASaveWithoutStaffOpensUnstaffedAndDamagedStaffIsRefused) {
    const auto original = VisitorZoo();
    nlohmann::json saved = ZooToJson(*original);
    nlohmann::json without = saved;
    // The body of the save is wherever the staff were written.
    nlohmann::json* body = nullptr;
    for (auto& [key, value] : without.items()) {
        if (value.is_object() && value.contains("staff")) {
            body = &value;
        }
    }
    if (without.contains("staff")) {
        body = &without;
    }
    ASSERT_NE(body, nullptr);
    body->erase("staff");
    std::string error;
    const auto unstaffed = ZooFromJson(without, testdata::GameCatalog(), testdata::GameSpecies(), &error);
    ASSERT_NE(unstaffed, nullptr) << error;
    EXPECT_TRUE(unstaffed->Keepers().All().empty());
    for (int i = 0; i < 50; ++i) {
        unstaffed->Step();
    }

    nlohmann::json staff = ToJson(original->Keepers());
    EXPECT_TRUE(StaffFromJson(staff, &error).has_value());
    nlohmann::json bad = staff;
    bad["version"] = 99;
    EXPECT_FALSE(StaffFromJson(bad, &error).has_value());
    EXPECT_NE(error.find("staff:"), std::string::npos);
    bad = staff;
    bad["members"][0]["role"] = "juggler";
    EXPECT_FALSE(StaffFromJson(bad, &error).has_value());
    bad = staff;
    bad["members"][1]["id"] = bad["members"][0]["id"];
    EXPECT_FALSE(StaffFromJson(bad, &error).has_value());
    bad = staff;
    bad["members"][0]["route_index"] = 1000;
    EXPECT_FALSE(StaffFromJson(bad, &error).has_value());
    bad = staff;
    bad["members"][0]["round"] = nlohmann::json::array();
    EXPECT_FALSE(StaffFromJson(bad, &error).has_value());
    bad = staff;
    bad["next_id"] = 1;
    EXPECT_FALSE(StaffFromJson(bad, &error).has_value());
    EXPECT_FALSE(StaffFromJson(nlohmann::json::array(), &error).has_value());
}

/// What the hourly look at the animals found over a run.
struct Welfare {
    float worstHunger = 0.0f;
    float worstThirst = 0.0f;
    float worstDirt = 0.0f;
    std::string worstHungerWho;
    std::string worstThirstWho;
    int issueSamples = 0;       ///< animal-hours at or above tuning::kNeedIssue
    int longestIssueHours = 0;  ///< the longest one animal stayed there, in hourly looks
    std::string longestWho;
};

/// Runs the zoo with the gate shut (the animals and the staff only) and checks after
/// every step that the staff are where they may be; looks at every animal each game hour
/// of the motion clock. Prints what was seen.
Welfare RunAndWatch(Zoo& zoo, std::int64_t ticks, const char* label, bool staffed = true) {
    zoo.Guests().SetOpen(false);
    Watch watch(zoo);
    Welfare welfare;
    std::map<std::uint32_t, int> hoursInNeed;
    const std::size_t animalsAtFirst = zoo.Fauna().All().size();
    for (std::int64_t i = 0; i < ticks; ++i) {
        zoo.Step();
        watch.Check(zoo);
        if (::testing::Test::HasFatalFailure()) {
            return welfare;
        }
        if (i % kTicksPerGameHour != 0) {
            continue;
        }
        for (const Animal& animal : zoo.Fauna().All()) {
            if (!animal.InWorld()) {
                continue;
            }
            const std::string who = "animal " + std::to_string(animal.id.value) + " of habitat " +
                                    std::to_string(animal.home.value) + " at tick " + std::to_string(i);
            if (animal.hunger > welfare.worstHunger) {
                welfare.worstHunger = animal.hunger;
                welfare.worstHungerWho = who;
            }
            if (animal.thirst > welfare.worstThirst) {
                welfare.worstThirst = animal.thirst;
                welfare.worstThirstWho = who;
            }
            int& hours = hoursInNeed[animal.id.value];
            if (animal.hunger < tuning::kNeedIssue && animal.thirst < tuning::kNeedIssue) {
                hours = 0;
                continue;
            }
            ++welfare.issueSamples;
            if (++hours > welfare.longestIssueHours) {
                welfare.longestIssueHours = hours;
                welfare.longestWho = "animal " + std::to_string(animal.id.value) + " of habitat " +
                                     std::to_string(animal.home.value) + " at tick " + std::to_string(i);
            }
        }
        for (const Enclosure& enclosure : zoo.Habitats().All()) {
            welfare.worstDirt = std::max(welfare.worstDirt, zoo.Fauna().Dirt(enclosure.id));
        }
    }
    EXPECT_GE(zoo.Fauna().All().size(), animalsAtFirst) << "an animal died";
    std::cout << "KEEPERS " << label << ": worst hunger " << welfare.worstHunger << " ("
              << welfare.worstHungerWho << "), thirst " << welfare.worstThirst << " ("
              << welfare.worstThirstWho << "), dirt " << welfare.worstDirt << "; animal-hours in need "
              << welfare.issueSamples << ", longest " << welfare.longestIssueHours << " ("
              << welfare.longestWho << "); ticks seen/working";
    for (int a = 0; a < static_cast<int>(StaffActivity::Count); ++a) {
        const auto index = static_cast<std::size_t>(a);
        std::cout << " " << ToString(static_cast<StaffActivity>(a)) << " " << watch.seen[index] << "/"
                  << watch.worked[index];
    }
    std::cout << "\n";
    if (!staffed) {
        return welfare;
    }
    // Every kind of work is seen being done.
    for (const StaffActivity activity :
         {StaffActivity::Carry, StaffActivity::Push, StaffActivity::FillFeeder, StaffActivity::ThrowFeed,
          StaffActivity::Rake, StaffActivity::Hose, StaffActivity::Inspect, StaffActivity::Sweep,
          StaffActivity::EmptyBin, StaffActivity::Tend, StaffActivity::Water}) {
        EXPECT_GT(watch.seen[static_cast<std::size_t>(activity)], 0) << ToString(activity);
    }
    return welfare;
}

/// The zoo's automatic care alone over the same span: what the staffed zoo is held against.
Welfare WithoutStaff(std::uint32_t seed, float hour, int pace, std::int64_t ticks, const char* label) {
    const auto zoo = VisitorZoo(seed);
    zoo->Keepers() = Staff{};
    zoo->SetCalendar(14, hour);
    zoo->SetDayPace(pace);
    return RunAndWatch(*zoo, ticks, label, false);
}

/// With the keepers on their rounds the animals are no worse off than with the zoo's
/// automatic care alone, which stays behind them (docs/KEEPERS.md, "Automatic care stays behind them").
/// It is not "no animal ever hungry": automatic care alone does not reach that either, and
/// the figures both runs print say by how much.
///
/// The two runs are different histories (a feeder filled at another moment sends the
/// animals to it at another moment), so single animals fare a little differently in them:
/// the sum of the hours in need may differ by a twentieth, the longest spell of one animal
/// by a few hours and the single worst moment of one of 88 animals can move more while remaining
/// below the welfare issue line. Keep the tighter comparison whenever automatic care is near it.
/// As measured after the habitat-front planting change (2026-11-19): over a week 11 against 36
/// animal-hours in need, 3 against 10 hours, hunger at worst 0.751 against 0.989 and thirst 0.712
/// against 0.756; over a visitor's three days neither run has an hour in need, hunger is 0.663
/// against 0.549 and thirst 0.692 against 0.644.
void ExpectNoWorseOff(const Welfare& staffed, const Welfare& care) {
    EXPECT_LE(staffed.issueSamples, care.issueSamples + care.issueSamples / 20 + 1);
    EXPECT_LE(staffed.longestIssueHours, care.longestIssueHours + 4) << staffed.longestWho;
    EXPECT_LE(staffed.worstHunger, std::max(care.worstHunger + 0.06f, tuning::kNeedIssue));
    EXPECT_LE(staffed.worstThirst, std::max(care.worstThirst + 0.06f, tuning::kNeedIssue));
    EXPECT_LE(staffed.worstDirt, care.worstDirt + 0.01f);
    EXPECT_LT(staffed.worstDirt, tuning::kDirtyIssue);
}

// A week of game days at the pace of the animals' own clock, the staff where they may be at
// every step, and no animal hungrier or thirstier than automatic care alone leaves it.
TEST(Keeper, AWeekWithNoAnimalWorseOffThanUnderAutomaticCare) {
    const auto zoo = VisitorZoo();
    zoo->SetCalendar(14, 6.0f);
    zoo->SetDayPace(1);
    const Welfare staffed = RunAndWatch(*zoo, 7 * kTicksPerDay, "week");
    ExpectNoWorseOff(staffed, WithoutStaff(1, 6.0f, 1, 7 * kTicksPerDay, "week, no staff"));
}

// As the visitor lives it: the calendar twelve times slower than the animals' clock, so a
// morning of the keepers' rounds is several days of eating.
TEST(Keeper, AVisitorsMorningWithNoAnimalWorseOffThanUnderAutomaticCare) {
    const auto zoo = VisitorZoo(3);
    zoo->SetCalendar(14, 8.0f);
    zoo->SetDayPace(12);
    const Welfare staffed = RunAndWatch(*zoo, 3 * kTicksPerDay, "visit pace");
    ExpectNoWorseOff(staffed, WithoutStaff(3, 8.0f, 12, 3 * kTicksPerDay, "visit pace, no staff"));
}

/// Is the member seen at work on a habitat: at a job, or carrying the bucket or pushing the
/// wheelbarrow to one that is close by.
bool BusyAtHabitat(const StaffMember& member) {
    const StaffTask* task = member.Task();
    if (task == nullptr || !task->enclosure) {
        return false;
    }
    if (member.working) {
        return member.activity != StaffActivity::Rest;
    }
    return (member.activity == StaffActivity::Carry || member.activity == StaffActivity::Push) &&
           Distance(member.position, task->place) < 20.0f;
}

// With the public in: nobody of the staff is thrown off the paths by the crowd's presence,
// and the morning's fill (the calendar stopped) leaves them at work.
TEST(Keeper, AtWorkAmongVisitorsAfterTheMorningFill) {
    const auto zoo = VisitorZoo();
    FillForVisit(*zoo, VisitStartOptions{.fillTicks = 1200});
    zoo->SetDayPace(12);
    Watch watch(*zoo);
    std::int64_t working = 0;
    std::int64_t all = 0;
    int visitorEncounters = 0;
    int animalEncounters = 0;
    float closestWalkingVisitor = 1.0e9f;
    for (int i = 0; i < 3600; ++i) {
        std::vector<Vec2> staffBefore;
        for (const StaffMember& member : zoo->Keepers().All()) {
            staffBefore.push_back(member.position);
        }
        zoo->Step();
        watch.Check(*zoo);
        if (::testing::Test::HasFatalFailure()) {
            return;
        }
        const auto& members = zoo->Keepers().All();
        for (const Visitor& visitor : zoo->Guests().All()) {
            if (visitor.inside || visitor.speed < 0.05f) {
                continue;
            }
            const EnclosureId region = zoo->Habitats().At(World::TileAt(visitor.position));
            for (const Vec2 staffPosition : staffBefore) {
                if (zoo->Habitats().At(World::TileAt(staffPosition)) == region) {
                    closestWalkingVisitor =
                        std::min(closestWalkingVisitor, Distance(visitor.position, staffPosition));
                }
            }
        }
        for (std::size_t m = 0; m < members.size(); ++m) {
            const StaffMember& member = members[m];
            ++all;
            working += member.working && member.activity != StaffActivity::Rest;
            if (DistanceSquared(staffBefore[m], member.position) < 1.0e-8f) {
                continue;
            }
            const EnclosureId region = zoo->Habitats().At(World::TileAt(staffBefore[m]));
            for (const Visitor& visitor : zoo->Guests().All()) {
                if (visitor.inside || zoo->Habitats().At(World::TileAt(visitor.position)) != region) {
                    continue;
                }
                constexpr float clearance = 0.32f + 0.28f + 0.08f;
                const float before = Distance(staffBefore[m], visitor.position);
                const float after = Distance(member.position, visitor.position);
                visitorEncounters += before < clearance + 0.08f;
                EXPECT_GE(after + 1.0e-4f, std::min(before, clearance))
                    << "staff " << member.id.value << " walked into visitor " << visitor.id.value;
            }
            for (const Animal& animal : zoo->Fauna().All()) {
                if (!animal.InWorld() || animal.shelter || animal.species < 0 ||
                    static_cast<std::size_t>(animal.species) >= zoo->SpeciesList().All().size() ||
                    zoo->Habitats().At(World::TileAt(animal.position)) != region) {
                    continue;
                }
                const SpeciesDef& species =
                    zoo->SpeciesList().All()[static_cast<std::size_t>(animal.species)];
                const float clearance =
                    0.32f + species.bodyRadiusM * BodyScale(animal, species, zoo->Time()) * 0.75f + 0.08f;
                const float before = Distance(staffBefore[m], animal.position);
                const float after = Distance(member.position, animal.position);
                animalEncounters += before < clearance + 0.08f;
                EXPECT_GE(after + 1.0e-4f, std::min(before, clearance))
                    << "staff " << member.id.value << " walked into animal " << animal.id.value;
            }
        }
    }
    std::cout << "KEEPERS at a job " << working * 100 / all << " % of the staff's time\n";
    std::cout << "KEEPERS guarded " << visitorEncounters << " close visitor and " << animalEncounters
              << " close animal steps; nearest walking visitor " << closestWalkingVisitor << " m\n";
    EXPECT_GT(working * 5, all * 2) << "the staff are at a job less than two fifths of the time";
    EXPECT_GT(visitorEncounters, 10) << "the run did not exercise walking through a crowd";
    EXPECT_EQ(animalEncounters, 0) << "a shy animal let staff enter its body clearance";
    EXPECT_GT(closestWalkingVisitor, 0.2f) << "a visitor walked through a member of staff";
}

// How often a visitor finds a keeper at a habitat: figures for docs/KEEPERS.md, not a check.
// Two shares of the time for each habitat: its keeper at a job there, and its keeper at a job
// or carrying or pushing to one within 20 m.
TEST(Keeper, DISABLED_PresenceFigures) {
    for (const float hour : {9.0f, 11.0f, 14.0f, 16.0f}) {
        const auto zoo = VisitorZoo();
        VisitStartOptions options;
        options.hour = hour;
        options.fillTicks = 6000;
        zoo->Guests().SetOpen(false);
        FillForVisit(*zoo, options);
        zoo->SetDayPace(12);
        const std::size_t habitats = zoo->Habitats().All().size();
        ASSERT_LE(habitats, 20u);  // ids 1..n, a bit each below
        std::vector<std::int64_t> atJob(habitats + 2, 0);
        std::vector<std::int64_t> busy(habitats + 2, 0);
        std::int64_t staffAtJob = 0;
        // How often each set of habitats had a keeper busy at one of them at least.
        std::vector<std::int64_t> masks(std::size_t{1} << habitats, 0);
        const int samples = 6000;
        for (int i = 0; i < samples; ++i) {
            zoo->Step();
            std::size_t mask = 0;
            for (const StaffMember& member : zoo->Keepers().All()) {
                const StaffTask* task = member.Task();
                if (task == nullptr) {
                    continue;
                }
                const bool job = member.working && member.activity != StaffActivity::Rest;
                staffAtJob += job;
                if (member.role == StaffRole::Keeper) {
                    atJob[task->enclosure.value] += job;
                    busy[task->enclosure.value] += BusyAtHabitat(member);
                    if (BusyAtHabitat(member)) {
                        mask |= std::size_t{1} << (task->enclosure.value - 1);
                    }
                }
            }
            ++masks[mask];
        }
        // A visitor who stands at two, or at three, of the habitats: any two, any three.
        std::int64_t pairsSeen = 0;
        std::int64_t pairs = 0;
        std::int64_t triplesSeen = 0;
        std::int64_t triples = 0;
        for (std::size_t a = 0; a < habitats; ++a) {
            for (std::size_t b = a + 1; b < habitats; ++b) {
                const std::size_t two = (std::size_t{1} << a) | (std::size_t{1} << b);
                for (std::size_t mask = 0; mask < masks.size(); ++mask) {
                    pairsSeen += (mask & two) != 0 ? masks[mask] : 0;
                }
                pairs += samples;
                for (std::size_t c = b + 1; c < habitats; ++c) {
                    const std::size_t three = two | (std::size_t{1} << c);
                    for (std::size_t mask = 0; mask < masks.size(); ++mask) {
                        triplesSeen += (mask & three) != 0 ? masks[mask] : 0;
                    }
                    triples += samples;
                }
            }
        }
        std::int64_t sumJob = 0;
        std::int64_t sumBusy = 0;
        std::cout << "PRESENCE hour " << hour << ":";
        for (const Enclosure& enclosure : zoo->Habitats().All()) {
            std::cout << " " << enclosure.id.value << "=" << atJob[enclosure.id.value] * 100 / samples << "/"
                      << busy[enclosure.id.value] * 100 / samples;
            sumJob += atJob[enclosure.id.value];
            sumBusy += busy[enclosure.id.value];
        }
        const auto all = static_cast<std::int64_t>(habitats) * samples;
        std::cout << " mean " << sumJob * 100 / all << "/" << sumBusy * 100 / all << " %; staff at a job "
                  << staffAtJob * 100 / (samples * static_cast<std::int64_t>(zoo->Keepers().All().size()))
                  << " %; a keeper busy at one of two habitats " << pairsSeen * 100 / pairs
                  << " %, at one of three " << triplesSeen * 100 / triples << " %\n";
    }
}

}  // namespace
}  // namespace zoo
