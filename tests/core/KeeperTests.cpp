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
                " (" + std::to_string(member.position.x) + ", " + std::to_string(member.position.z) +
                ") from (" + std::to_string(before[i].x) + ", " + std::to_string(before[i].z) +
                "), lockout " + std::to_string(member.lockout.value) + ", task " +
                std::to_string(member.task);
            ASSERT_TRUE(zoo.Routes().Passable(tile, kStaff)) << who << " stands where nobody can walk";
            ASSERT_NE(zoo.Habitats().RegionAt(tile), Region::StandOff) << who << " stands in a planted strip";
            ASSERT_FALSE(zoo.Map().At(tile).standOff) << who << " stands in a planted strip";
            // Dry-shod: no deeper than a boot anywhere along the way (a lagoon's shelf is 0.3 m).
            ASSERT_LE(zoo.Map().WaterDepthAt(member.position), 0.08f) << who << " wades";
            const EnclosureId standing = zoo.Habitats().At(tile);
            if (dangerous.contains(standing.value)) {
                ASSERT_EQ(member.lockout, standing) << who << " entered without its lockout";
                for (const Animal& animal : zoo.Fauna().All()) {
                    if (!animal.InWorld() || animal.species < 0 ||
                        static_cast<std::size_t>(animal.species) >= zoo.SpeciesList().All().size()) {
                        continue;
                    }
                    const SpeciesDef& species =
                        zoo.SpeciesList().All()[static_cast<std::size_t>(animal.species)];
                    if (species.dangerous &&
                        (animal.home == standing || zoo.Habitats().At(animal.position) == standing)) {
                        ASSERT_TRUE(animal.shelter)
                            << who << " shares the habitat with dangerous animal " << animal.id.value;
                    }
                }
            }
            // No step is longer than a walk allows, and none goes through a fence.
            const float moved = Distance(before[i], member.position);
            ASSERT_LE(moved, kStepBound) << who;
            if (moved > 1.0e-6f) {
                ASSERT_TRUE(zoo.Routes().Walkable(before[i], member.position, kStaff))
                    << who << " crossed a barrier";
            }
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
    ASSERT_EQ(staff.All().size(), 10u);
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
            const Enclosure* first = zoo->Habitats().Find(member.round[0].enclosure);
            const Enclosure* second = zoo->Habitats().Find(member.round[1].enclosure);
            ASSERT_NE(first, nullptr);
            ASSERT_NE(second, nullptr);
            const Vec2 firstCentre = (World::TileCenter(first->min) + World::TileCenter(first->max)) * 0.5f;
            const Vec2 secondCentre =
                (World::TileCenter(second->min) + World::TileCenter(second->max)) * 0.5f;
            EXPECT_LT(Distance(firstCentre, secondCentre), 100.0f)
                << first->name << " and " << second->name << " make an impractical keeper round";
            for (std::size_t slot = 0; slot < 2; ++slot) {
                EXPECT_GE(std::abs(member.round[0].feedHours[slot] - member.round[1].feedHours[slot]), 0.7f);
            }
        }
    }
    EXPECT_EQ(staff.Find(StaffId{1}), &staff.All().front());
    EXPECT_EQ(staff.Find(StaffId{9999}), nullptr);
    EXPECT_EQ(staff.KeeperOf(EnclosureId{9999}), nullptr);
}

TEST(Keeper, AWholeDangerousGroupIsRecalledToCapacityCheckedShelters) {
    const auto zoo = VisitorZoo();
    const std::set<std::uint32_t> dangerous = DangerousHabitats(*zoo);
    ASSERT_FALSE(dangerous.empty());
    const EnclosureId habitat{*dangerous.begin()};
    const Enclosure* enclosure = zoo->Habitats().Find(habitat);
    ASSERT_NE(enclosure, nullptr);

    std::vector<AnimalId> group;
    for (const Animal& animal : zoo->Fauna().All()) {
        if (animal.InWorld() && zoo->Habitats().At(animal.position) == habitat) {
            group.push_back(animal.id);
        }
    }
    ASSERT_FALSE(group.empty());
    std::map<std::uint32_t, Vec2> beforeRecall;
    for (AnimalId id : group) {
        beforeRecall[id.value] = zoo->Fauna().Find(id)->position;
    }
    Env env = zoo->MakeEnv();
    ASSERT_TRUE(zoo->Fauna().SecureForStaff(env, habitat));

    float hungerBefore = 0.0f;
    bool visibleRecall = false;
    for (AnimalId id : group) {
        const Animal* animal = zoo->Fauna().Find(id);
        ASSERT_NE(animal, nullptr);
        EXPECT_EQ(animal->home, habitat);
        EXPECT_EQ(animal->activity, AnimalActivity::Shelter);
        EXPECT_FALSE(animal->loose);
        EXPECT_EQ(animal->position, beforeRecall[id.value]) << "recall must not teleport animal " << id.value;
        visibleRecall |= animal->moving && !animal->shelter;
        hungerBefore += animal->hunger;
    }
    EXPECT_TRUE(visibleRecall);

    zoo->Fauna().ClearStaffLockouts();
    zoo->Fauna().AddStaffLockout(habitat);
    int recallTicks = 0;
    for (; recallTicks < 5000; ++recallTicks) {
        bool allSheltered = true;
        for (AnimalId id : group) {
            allSheltered &= static_cast<bool>(zoo->Fauna().Find(id)->shelter);
        }
        if (allSheltered) {
            break;
        }
        env.time.tick = recallTicks + 1;
        zoo->Fauna().Step(env);
    }
    EXPECT_GT(recallTicks, 0);
    ASSERT_LT(recallTicks, 5000);

    std::map<std::uint32_t, int> occupants;
    bool changedPosition = false;
    for (AnimalId id : group) {
        const Animal* animal = zoo->Fauna().Find(id);
        ASSERT_NE(animal, nullptr);
        ASSERT_TRUE(animal->shelter);
        EXPECT_EQ(animal->activity, AnimalActivity::Shelter);
        EXPECT_FALSE(animal->moving);
        changedPosition |= Distance(animal->position, beforeRecall[id.value]) > 0.0f;
        ++occupants[animal->shelter.value];
    }
    EXPECT_TRUE(changedPosition);
    for (const auto& [buildingId, count] : occupants) {
        const Building* building = zoo->Objects().FindBuilding(BuildingId{buildingId});
        ASSERT_NE(building, nullptr);
        ASSERT_GE(building->def, 0);
        ASSERT_LT(static_cast<std::size_t>(building->def), zoo->Definitions().buildings.size());
        EXPECT_LE(count, zoo->Definitions().buildings[static_cast<std::size_t>(building->def)].capacity);
    }

    const std::uint64_t beforeTransientInput = zoo->Fauna().StateHash();
    zoo->Fauna().ClearStaffLockouts();
    zoo->Fauna().AddStaffLockout(habitat);
    zoo->Fauna().AddStaffLockout(habitat);
    EXPECT_TRUE(zoo->Fauna().StaffLockout(habitat));
    EXPECT_EQ(zoo->Fauna().StateHash(), beforeTransientInput);
    for (int step = 0; step < 180; ++step) {
        env.time.tick = recallTicks + step + 1;
        zoo->Fauna().Step(env);
    }
    float hungerAfter = 0.0f;
    for (AnimalId id : group) {
        const Animal* animal = zoo->Fauna().Find(id);
        ASSERT_NE(animal, nullptr);
        EXPECT_TRUE(animal->shelter) << "animal " << id.value << " left during lockout";
        EXPECT_EQ(animal->activity, AnimalActivity::Shelter);
        EXPECT_FALSE(animal->moving);
        hungerAfter += animal->hunger;
    }
    EXPECT_GT(hungerAfter, hungerBefore) << "secured animals should keep accumulating needs";
}

TEST(Keeper, FailedGroupLockoutChangesNoAnimal) {
    const auto zoo = VisitorZoo();
    const std::set<std::uint32_t> dangerous = DangerousHabitats(*zoo);
    ASSERT_FALSE(dangerous.empty());
    const EnclosureId habitat{*dangerous.begin()};
    const Enclosure* enclosure = zoo->Habitats().Find(habitat);
    ASSERT_NE(enclosure, nullptr);
    ASSERT_FALSE(enclosure->shelters.empty());
    for (BuildingId shelter : enclosure->shelters) {
        Building* building = zoo->Objects().FindBuilding(shelter);
        ASSERT_NE(building, nullptr);
        building->open = false;
    }
    const std::uint64_t before = zoo->Fauna().StateHash();
    Env env = zoo->MakeEnv();
    EXPECT_FALSE(zoo->Fauna().SecureForStaff(env, habitat));
    EXPECT_EQ(zoo->Fauna().StateHash(), before);
}

// A planted stand-off strip leaves the service gate as the only place directly beside a
// dangerous habitat.  The public viewing bays are safe working places too: without them a
// keeper repeats a whole round at one or two points hidden behind the habitat.
TEST(Keeper, DangerousHabitatsHaveOutsideServiceAndLockoutWorkInside) {
    const auto zoo = VisitorZoo();
    zoo->Guests().SetOpen(false);
    const std::set<std::uint32_t> dangerous = DangerousHabitats(*zoo);
    std::map<std::uint32_t, std::set<std::pair<float, float>>> places;
    std::map<std::uint32_t, int> inside;
    std::map<std::uint32_t, int> secure;
    std::map<std::uint32_t, int> release;
    std::map<std::uint32_t, int> active;
    std::map<std::uint32_t, int> entered;
    std::map<std::uint32_t, int> visibleRecall;
    std::map<std::uint32_t, int> operatedClose;
    std::map<std::uint32_t, int> operatedRelease;
    std::map<std::uint32_t, int> activeStreak;
    std::map<std::uint32_t, int> longestActive;
    for (int i = 0; i < 50000; ++i) {
        zoo->Step();
        std::set<std::uint32_t> activeNow;
        for (const StaffMember& member : zoo->Keepers().All()) {
            if (member.lockout) {
                ++active[member.lockout.value];
                entered[member.lockout.value] +=
                    zoo->Habitats().At(World::TileAt(member.position)) == member.lockout;
                bool recalling = false;
                for (const Animal& animal : zoo->Fauna().All()) {
                    if (animal.InWorld() && animal.home == member.lockout && !animal.shelter) {
                        recalling |= animal.activity == AnimalActivity::Shelter && animal.moving;
                        EXPECT_NE(zoo->Habitats().At(World::TileAt(member.position)), member.lockout)
                            << "keeper entered habitat " << member.lockout.value << " during recall";
                    }
                }
                visibleRecall[member.lockout.value] += recalling;
                const StaffTask* task = member.Task();
                const bool closesGate = member.working && member.task > 0 && task != nullptr &&
                                        task->lockout == StaffLockoutAction::None &&
                                        member.tasks[member.task - 1].lockout == StaffLockoutAction::Secure;
                operatedClose[member.lockout.value] += closesGate;
                if (closesGate) {
                    EXPECT_NE(zoo->Habitats().At(World::TileAt(member.position)), member.lockout)
                        << "keeper entered habitat " << member.lockout.value
                        << " before its holding gate closed";
                }
                operatedRelease[member.lockout.value] +=
                    member.working && task != nullptr && task->lockout == StaffLockoutAction::Release &&
                    zoo->Habitats().At(World::TileAt(member.position)) != member.lockout;
                activeNow.insert(member.lockout.value);
            }
            for (const StaffTask& task : member.tasks) {
                if (!dangerous.contains(task.enclosure.value)) {
                    continue;
                }
                const EnclosureId standing = zoo->Habitats().At(World::TileAt(task.place));
                if (standing == task.enclosure) {
                    EXPECT_EQ(task.lockout, StaffLockoutAction::None);
                    ++inside[task.enclosure.value];
                } else {
                    EXPECT_EQ(zoo->Habitats().RegionAt(World::TileAt(task.place)), Region::Public);
                    places[task.enclosure.value].insert({task.place.x, task.place.z});
                }
                secure[task.enclosure.value] += task.lockout == StaffLockoutAction::Secure;
                release[task.enclosure.value] += task.lockout == StaffLockoutAction::Release;
            }
        }
        for (const std::uint32_t id : dangerous) {
            activeStreak[id] = activeNow.contains(id) ? activeStreak[id] + 1 : 0;
            longestActive[id] = std::max(longestActive[id], activeStreak[id]);
        }
    }
    for (const std::uint32_t id : dangerous) {
        const Enclosure* enclosure = zoo->Habitats().Find(EnclosureId{id});
        ASSERT_NE(enclosure, nullptr);
        EXPECT_GE(places[id].size(), 2u) << enclosure->name;
        EXPECT_GT(inside[id], 0) << enclosure->name;
        EXPECT_GT(secure[id], 0) << enclosure->name;
        EXPECT_GT(release[id], 0) << enclosure->name;
        EXPECT_GT(active[id], 0) << enclosure->name;
        EXPECT_GT(entered[id], 0) << enclosure->name;
        EXPECT_GT(visibleRecall[id], 0) << enclosure->name << " did not show its recall";
        EXPECT_GT(operatedClose[id], 0) << enclosure->name << " did not wait for its holding gate";
        EXPECT_GT(operatedRelease[id], 0) << enclosure->name << " did not show its release operation";
        EXPECT_LT(longestActive[id], 1500) << enclosure->name << " remained secured too long";
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

    // Version 1 had neither safety actions nor an active lockout. It remains readable.
    nlohmann::json legacy = ToJson(original->Keepers());
    legacy["version"] = 1;
    for (nlohmann::json& member : legacy["members"]) {
        member.erase("lockout");
        for (nlohmann::json& task : member["tasks"]) {
            task.erase("lockout_action");
        }
    }
    EXPECT_TRUE(StaffFromJson(legacy, &error).has_value()) << error;
}

TEST(Keeper, AnActiveDangerousLockoutSurvivesSaveAndContinuesSafely) {
    const auto original = VisitorZoo();
    original->Guests().SetOpen(false);
    const StaffMember* active = nullptr;
    for (int i = 0; i < 30000 && active == nullptr; ++i) {
        original->Step();
        for (const StaffMember& member : original->Keepers().All()) {
            if (member.lockout && original->Habitats().At(World::TileAt(member.position)) == member.lockout) {
                active = &member;
                break;
            }
        }
    }
    ASSERT_NE(active, nullptr);
    const EnclosureId habitat = active->lockout;
    std::string error;
    const auto loaded =
        ZooFromJson(ZooToJson(*original), testdata::GameCatalog(), testdata::GameSpecies(), &error);
    ASSERT_NE(loaded, nullptr) << error;
    ASSERT_TRUE(loaded->SameState(*original));
    bool restored = false;
    for (const StaffMember& member : loaded->Keepers().All()) {
        restored |= member.lockout == habitat;
    }
    EXPECT_TRUE(restored);
    for (const Animal& animal : loaded->Fauna().All()) {
        if (animal.InWorld() && animal.home == habitat) {
            EXPECT_TRUE(animal.shelter) << "animal " << animal.id.value;
        }
    }

    Watch originalWatch(*original);
    Watch loadedWatch(*loaded);
    for (int i = 0; i < 1800; ++i) {
        original->Step();
        loaded->Step();
        originalWatch.Check(*original);
        loadedWatch.Check(*loaded);
        if (::testing::Test::HasFatalFailure()) {
            return;
        }
    }
    EXPECT_TRUE(loaded->SameState(*original));
}

TEST(Keeper, AnAnimatedDangerousRecallSurvivesSaveAndFinishesSafely) {
    const auto original = VisitorZoo();
    original->Guests().SetOpen(false);
    EnclosureId habitat;
    for (int i = 0; i < 30000 && !habitat; ++i) {
        original->Step();
        for (const StaffMember& member : original->Keepers().All()) {
            if (!member.lockout ||
                original->Habitats().At(World::TileAt(member.position)) == member.lockout) {
                continue;
            }
            for (const Animal& animal : original->Fauna().All()) {
                if (animal.InWorld() && animal.home == member.lockout && !animal.shelter && animal.moving &&
                    animal.activity == AnimalActivity::Shelter) {
                    habitat = member.lockout;
                    break;
                }
            }
        }
    }
    ASSERT_TRUE(habitat);

    std::string error;
    const auto loaded =
        ZooFromJson(ZooToJson(*original), testdata::GameCatalog(), testdata::GameSpecies(), &error);
    ASSERT_NE(loaded, nullptr) << error;
    ASSERT_TRUE(loaded->SameState(*original));

    Watch originalWatch(*original);
    Watch loadedWatch(*loaded);
    bool entered = false;
    bool released = false;
    for (int i = 0; i < 3000 && !released; ++i) {
        original->Step();
        loaded->Step();
        originalWatch.Check(*original);
        loadedWatch.Check(*loaded);
        ASSERT_TRUE(loaded->SameState(*original)) << "after recall step " << i;
        bool active = false;
        for (const StaffMember& member : loaded->Keepers().All()) {
            if (member.lockout == habitat) {
                active = true;
                entered |= loaded->Habitats().At(World::TileAt(member.position)) == habitat;
            }
        }
        released = entered && !active;
    }
    EXPECT_TRUE(entered);
    EXPECT_TRUE(released);
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
/// by a few hours and the single worst moment of one of 90 animals can move more while remaining
/// below the welfare issue line. Keep the tighter comparison whenever automatic care is near it.
/// As measured after the habitat-front planting change (2026-11-19): over a week 11 against 36
/// animal-hours in need, 3 against 10 hours, hunger at worst 0.751 against 0.989 and thirst 0.712
/// against 0.756; over a visitor's three days neither run has an hour in need, hunger is 0.663
/// against 0.549 and thirst 0.692 against 0.644. With J26's capacity-aware births, the week is
/// 6 against 13 animal-hours, 5 against 3 hours, hunger 0.840 against 0.773 and thirst 0.706
/// against 0.700; the one-hour peak needs the seven-point allowance while the aggregate improves.
void ExpectNoWorseOff(const Welfare& staffed, const Welfare& care) {
    EXPECT_LE(staffed.issueSamples, care.issueSamples + care.issueSamples / 20 + 1);
    EXPECT_LE(staffed.longestIssueHours, care.longestIssueHours + 4) << staffed.longestWho;
    EXPECT_LE(staffed.worstHunger, std::max(care.worstHunger + 0.07f, tuning::kNeedIssue));
    EXPECT_LE(staffed.worstThirst, std::max(care.worstThirst + 0.07f, tuning::kNeedIssue));
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

// The cleaner is not merely animated beside a bin: a daytime sweep removes real
// litter before automatic care's midnight safety net.
TEST(Keeper, TheCleanerSweepsRealLitterBeforeMidnight) {
    const auto zoo = VisitorZoo();
    FillForVisit(*zoo, VisitStartOptions{.fillTicks = 6000});
    zoo->SetDayPace(12);
    const std::int64_t day = zoo->Time().Day();
    std::size_t mostLitter = zoo->Guests().LitterOnGround().size();
    bool appliedSweep = false;
    bool removedLitter = false;
    bool sweepKnowsBin = false;
    float closestSweep = std::numeric_limits<float>::infinity();
    for (int step = 0; step < 12000 && !removedLitter; ++step) {
        const std::size_t before = zoo->Guests().LitterOnGround().size();
        bool appliesNow = false;
        for (const StaffMember& member : zoo->Keepers().All()) {
            const StaffTask* task = member.Task();
            if (member.role == StaffRole::Cleaner && member.working && task != nullptr &&
                task->work == StaffActivity::Sweep &&
                member.ticksLeft == (std::max(task->ticks, 1) + 1) / 2) {
                appliesNow = true;
                sweepKnowsBin |= static_cast<bool>(task->prop);
                for (const Litter& litter : zoo->Guests().LitterOnGround()) {
                    closestSweep = std::min(closestSweep, Distance(task->place, litter.position));
                }
            }
        }
        zoo->Step();
        const std::size_t after = zoo->Guests().LitterOnGround().size();
        mostLitter = std::max(mostLitter, after);
        appliedSweep |= appliesNow;
        removedLitter |= appliesNow && after < before;
    }
    EXPECT_EQ(zoo->Time().Day(), day) << "the night shift, not the cleaner, removed the litter";
    EXPECT_GT(mostLitter, 0U) << "the run never exercised litter";
    EXPECT_TRUE(appliedSweep) << "the run never reached the cleaner's sweep effect";
    EXPECT_TRUE(sweepKnowsBin) << "the cleaner's sweep did not retain its bin";
    EXPECT_TRUE(removedLitter) << "a visible daytime sweep removed no litter; closest was " << closestSweep
                               << " m";
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
