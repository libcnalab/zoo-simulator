// SPDX-License-Identifier: MIT
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <vector>

#include "zoo/animals/AnimalTuning.hpp"
#include "zoo/animals/Animals.hpp"
#include "zoo/build/Builder.hpp"

namespace zoo {
namespace {

TEST(AnimalMovementTests, WanderMovesWithinItsEnclosureWithoutSnappingHeading) {
    World world(40, 40);
    ZooObjects objects;
    Catalog catalog = Catalog::LoadDirectory(std::string(ZOO_SOURCE_DIR) + "/data/construction");
    SpeciesCatalog species = SpeciesCatalog::LoadDirectory(std::string(ZOO_SOURCE_DIR) + "/data/species");
    const int fence = catalog.FindFence("high_glass_wall")->kind;
    for (int x = 8; x <= 30; ++x) {
        world.SetFence({x, 8, true}, {static_cast<std::uint8_t>(fence), false, 1.0f});
        world.SetFence({x, 31, true}, {static_cast<std::uint8_t>(fence), false, 1.0f});
    }
    for (int z = 8; z <= 30; ++z) {
        world.SetFence({8, z, false}, {static_cast<std::uint8_t>(fence), false, 1.0f});
        world.SetFence({31, z, false}, {static_cast<std::uint8_t>(fence), false, 1.0f});
    }
    Enclosures enclosures;
    enclosures.Rebuild(world, objects, catalog);
    Navigator nav(world);
    Animals animals(19U);
    const int lion = species.IndexOf("lion");
    ASSERT_GE(lion, 0);
    const AnimalId id =
        animals.Add(Env{world, objects, enclosures, catalog, species, nav, nullptr, nullptr, {}, {}}, lion,
                    Sex::Female, 5.0f, World::TileCenter({18, 18}));
    ASSERT_TRUE(id);
    Animal* animal = animals.Find(id);
    ASSERT_NE(animal, nullptr);
    animal->activityLeft = 0;
    animal->hunger = 0.0f;
    animal->thirst = 0.0f;
    Env env{world, objects, enclosures, catalog, species, nav, nullptr, nullptr, {}, {}};
    Vec2 previous = animal->position;
    float previousHeading = animal->heading;
    float previousSpeed = animal->speed;
    bool moved = false;
    for (int tick = 0; tick < 3000; ++tick) {
        env.time.tick = tick;
        animals.Step(env);
        animal = animals.Find(id);
        ASSERT_NE(animal, nullptr);
        EXPECT_EQ(enclosures.At(animal->position), enclosures.All().front().id);
        EXPECT_LE(std::abs(WrapAngle(animal->heading - previousHeading)), 0.151f);
        const float speedDelta = std::abs(animal->speed - previousSpeed);
        const float accelerationLimit =
            (animal->speed < previousSpeed ? tuning::kBrakeAcceleration : tuning::kWalkAcceleration) *
            tuning::kMotionSecondsPerTick;
        EXPECT_LE(speedDelta, accelerationLimit + 0.01f)
            << "tick " << tick << " position " << animal->position.x << ", " << animal->position.z
            << " speed " << previousSpeed << " to " << animal->speed << " moving " << animal->moving;
        const float covered = Length(animal->position - previous);
        EXPECT_NEAR(covered, animal->speed * tuning::kMotionSecondsPerTick, 0.001f);
        if (animal->position != previous) {
            moved = true;
        }
        previous = animal->position;
        previousHeading = animal->heading;
        previousSpeed = animal->speed;
    }
    EXPECT_TRUE(moved);
}

TEST(AnimalMovementTests, APlannedRouteGoesAroundBlockingHabitatScenery) {
    World world(22, 14);
    ZooObjects objects;
    const Catalog catalog = Catalog::LoadDirectory(std::string(ZOO_SOURCE_DIR) + "/data/construction");
    const SpeciesCatalog species =
        SpeciesCatalog::LoadDirectory(std::string(ZOO_SOURCE_DIR) + "/data/species");
    const int fence = catalog.FindFence("high_glass_wall")->kind;
    for (int x = 1; x <= 20; ++x) {
        world.SetFence({x, 1, true}, {static_cast<std::uint8_t>(fence), false, 1.0f});
        world.SetFence({x, 12, true}, {static_cast<std::uint8_t>(fence), false, 1.0f});
    }
    for (int z = 1; z <= 11; ++z) {
        world.SetFence({1, z, false}, {static_cast<std::uint8_t>(fence), false, 1.0f});
        world.SetFence({21, z, false}, {static_cast<std::uint8_t>(fence), false, 1.0f});
    }
    const int oak = catalog.PropIndex("oak");
    ASSERT_GE(oak, 0);
    const Vec2 tree = World::TileCenter({10, 6});
    objects.AddProp({.id = {}, .def = oak, .position = tree});
    Enclosures enclosures;
    enclosures.Rebuild(world, objects, catalog);
    Navigator nav(world);
    Animals animals(31U);
    Env env{world, objects, enclosures, catalog, species, nav, nullptr, nullptr, {}, {}};
    const int lion = species.IndexOf("lion");
    const Vec2 start = World::TileCenter({3, 6});
    const Vec2 target = World::TileCenter({17, 6});
    const AnimalId id = animals.Add(env, lion, Sex::Female, 5.0f, start);
    ASSERT_TRUE(id);
    Animal* animal = animals.Find(id);
    animal->activity = AnimalActivity::Wander;
    animal->activityLeft = 1000;
    animal->moving = true;
    animal->target = target;
    animal->hunger = 0.0f;
    animal->thirst = 0.0f;

    animals.Step(env);
    animal = animals.Find(id);
    ASSERT_NE(animal, nullptr);
    ASSERT_GE(animal->route.size(), 2u);
    const SpeciesDef& lionDef = species.All()[static_cast<std::size_t>(lion)];
    const float clearance = catalog.props[static_cast<std::size_t>(oak)].radiusM +
                            lionDef.bodyRadiusM * tuning::kBodyWidthFraction + tuning::kObstacleClearanceM;
    const std::array<NavObstacle, 1> obstacle = {{{tree, clearance}}};
    const NavProfile profile = NavProfile::Animal(0.0f, lionDef.canSwim);
    Vec2 previous = animal->position;
    for (std::size_t i = animal->routeIndex; i < animal->route.size(); ++i) {
        EXPECT_TRUE(nav.Walkable(previous, animal->route[i], profile, obstacle));
        previous = animal->route[i];
    }
    EXPECT_EQ(animal->route.back(), target);
}

TEST(AnimalMovementTests, ShyAnimalsRetreatFromAVisibleKeeperButBoldAnimalsDoNot) {
    World world(40, 40);
    ZooObjects objects;
    Catalog catalog = Catalog::LoadDirectory(std::string(ZOO_SOURCE_DIR) + "/data/construction");
    SpeciesCatalog species = SpeciesCatalog::LoadDirectory(std::string(ZOO_SOURCE_DIR) + "/data/species");
    const int fence = catalog.FindFence("high_glass_wall")->kind;
    for (int x = 8; x <= 30; ++x) {
        world.SetFence({x, 8, true}, {static_cast<std::uint8_t>(fence), false, 1.0f});
        world.SetFence({x, 31, true}, {static_cast<std::uint8_t>(fence), false, 1.0f});
    }
    for (int z = 8; z <= 30; ++z) {
        world.SetFence({8, z, false}, {static_cast<std::uint8_t>(fence), false, 1.0f});
        world.SetFence({31, z, false}, {static_cast<std::uint8_t>(fence), false, 1.0f});
    }
    Enclosures enclosures;
    enclosures.Rebuild(world, objects, catalog);
    Navigator nav(world);
    Animals animals(101U);
    Env env{world, objects, enclosures, catalog, species, nav, nullptr, nullptr, {}, {}};
    const AnimalId shy =
        animals.Add(env, species.IndexOf("zebra"), Sex::Female, 5.0f, World::TileCenter({18, 18}));
    const AnimalId bold =
        animals.Add(env, species.IndexOf("flamingo"), Sex::Female, 5.0f, World::TileCenter({24, 24}));
    ASSERT_TRUE(shy);
    ASSERT_TRUE(bold);
    const EnclosureId enclosure = enclosures.At(animals.Find(shy)->position);
    ASSERT_TRUE(enclosure);
    const Vec2 shyKeeper = animals.Find(shy)->position + Vec2{-1.0f, 0.0f};
    const Vec2 boldKeeper = animals.Find(bold)->position + Vec2{-1.0f, 0.0f};
    animals.SetKeeper(enclosure, shyKeeper);
    animals.SetKeeper(enclosure, boldKeeper);
    for (const AnimalId id : {shy, bold}) {
        Animal* animal = animals.Find(id);
        animal->activity = AnimalActivity::Rest;
        animal->activityLeft = 10000;
        animal->hunger = 0.1f;
        animal->thirst = 0.1f;
    }
    const Vec2 boldStart = animals.Find(bold)->position;
    for (int tick = 0; tick < 120; ++tick) {
        env.time.tick = tick;
        animals.Step(env);
    }
    EXPECT_GT(Distance(animals.Find(shy)->position, shyKeeper), 3.0f);
    EXPECT_EQ(enclosures.At(animals.Find(shy)->position), enclosure);
    EXPECT_EQ(animals.Find(shy)->activity, AnimalActivity::Wander);
    EXPECT_EQ(animals.Find(bold)->position, boldStart);
    EXPECT_EQ(animals.Find(bold)->activity, AnimalActivity::Rest);
}

TEST(AnimalMovementTests, RoundedFenceEndpointCannotBecomeOrdinaryWalkingGround) {
    const Catalog catalog = Catalog::LoadDirectory(std::string(ZOO_SOURCE_DIR) + "/data/construction");
    const SpeciesCatalog species =
        SpeciesCatalog::LoadDirectory(std::string(ZOO_SOURCE_DIR) + "/data/species");
    for (const bool strip : {false, true}) {
        SCOPED_TRACE(strip);
        World world(20, 20);
        ZooObjects objects;
        for (const EdgeRef edge : EdgesAround({5, 5}, {7, 7})) {
            world.SetFence(edge, {5, false, 1.0f});
        }
        world.SetStandOff({6, 8}, strip);
        Enclosures homes;
        homes.Rebuild(world, objects, catalog);
        Navigator nav(world);
        Env env{world, objects, homes, catalog, species, nav, nullptr, nullptr, {}, {}};
        Animals animals(2);
        const auto id = animals.Add(env, species.IndexOf("flamingo"), Sex::Female, 5.0f, {13.25f, 15.999f});
        ASSERT_TRUE(id);
        Animal* animal = animals.Find(id);
        const EnclosureId home = homes.At(animal->position);
        ASSERT_TRUE(home);
        animal->activity = AnimalActivity::Wander;
        animal->activityLeft = 1000;
        animal->hunger = 0.0f;
        animal->thirst = 0.0f;
        animal->moving = true;
        animal->speed = 1.0f;
        // A rounded route endpoint lies exactly on the south fence, whose tile
        // ownership is already outside. Navigation must not grant an escape.
        animal->target = {13.25f, 16.0f};
        animal->route = {animal->target};
        animal->routeIndex = 0;
        const Animal before = *animal;
        animals.Step(env);
        EXPECT_EQ(homes.At(animals.Find(id)->position), home);
        EXPECT_FALSE(animals.Find(id)->loose);
        // The reservation must not alter the existing intentional escape behaviour.
        *animals.Find(id) = before;
        animals.Find(id)->activity = AnimalActivity::BreakOut;
        animals.Step(env);
        EXPECT_FALSE(homes.At(animals.Find(id)->position));
    }
}

TEST(AnimalMovementTests, WeakFenceEscapeCanBeRecaptured) {
    World world(40, 40);
    ZooObjects objects;
    Catalog catalog = Catalog::LoadDirectory(std::string(ZOO_SOURCE_DIR) + "/data/construction");
    SpeciesCatalog species = SpeciesCatalog::LoadDirectory(std::string(ZOO_SOURCE_DIR) + "/data/species");
    const int fence = catalog.fences.front().kind;
    for (int x = 8; x <= 30; ++x) {
        world.SetFence({x, 8, true}, {static_cast<std::uint8_t>(fence), false, 1.0f});
        world.SetFence({x, 31, true}, {static_cast<std::uint8_t>(fence), false, 1.0f});
    }
    for (int z = 8; z <= 30; ++z) {
        world.SetFence({8, z, false}, {static_cast<std::uint8_t>(fence), false, 1.0f});
        world.SetFence({31, z, false}, {static_cast<std::uint8_t>(fence), false, 1.0f});
    }
    Enclosures enclosures;
    enclosures.Rebuild(world, objects, catalog);
    Navigator nav(world);
    Animals animals(17U);
    const int lion = species.IndexOf("lion");
    ASSERT_GE(lion, 0);
    Env env{world, objects, enclosures, catalog, species, nav, nullptr, nullptr, {}, {}};
    const AnimalId id = animals.Add(env, lion, Sex::Female, 5.0f, World::TileCenter({18, 18}));
    ASSERT_TRUE(id);
    std::vector<Event> events;
    env.events = &events;
    bool escaped = false;
    for (int tick = 0; tick < 10 * kTicksPerDay && !escaped; ++tick) {
        env.time.tick = tick;
        animals.Step(env);
        const Animal* animal = animals.Find(id);
        escaped = animal != nullptr && animal->loose;
    }
    ASSERT_TRUE(escaped);
    EXPECT_EQ(animals.Escapes(), 1);
    EXPECT_TRUE(std::any_of(events.begin(), events.end(),
                            [](const Event& event) { return event.type == EventType::AnimalEscaped; }));
    ASSERT_TRUE(animals.Recapture(env, id));
    EXPECT_FALSE(animals.Find(id)->loose);
}

TEST(AnimalMovementTests, AdequateFencePreventsHourlyEscapeAttemptsForTenDays) {
    World world(40, 40);
    ZooObjects objects;
    Catalog catalog = Catalog::LoadDirectory(std::string(ZOO_SOURCE_DIR) + "/data/construction");
    SpeciesCatalog species = SpeciesCatalog::LoadDirectory(std::string(ZOO_SOURCE_DIR) + "/data/species");
    const int fence = catalog.FindFence("high_glass_wall")->kind;
    for (int x = 8; x <= 30; ++x) {
        world.SetFence({x, 8, true}, {static_cast<std::uint8_t>(fence), false, 1.0f});
        world.SetFence({x, 31, true}, {static_cast<std::uint8_t>(fence), false, 1.0f});
    }
    for (int z = 8; z <= 30; ++z) {
        world.SetFence({8, z, false}, {static_cast<std::uint8_t>(fence), false, 1.0f});
        world.SetFence({31, z, false}, {static_cast<std::uint8_t>(fence), false, 1.0f});
    }
    Enclosures enclosures;
    enclosures.Rebuild(world, objects, catalog);
    Navigator nav(world);
    Animals animals(79U);
    const int lion = species.IndexOf("lion");
    ASSERT_GE(lion, 0);
    Env env{world, objects, enclosures, catalog, species, nav, nullptr, nullptr, {}, {}};
    const AnimalId id = animals.Add(env, lion, Sex::Female, 5.0f, World::TileCenter({18, 18}));
    ASSERT_TRUE(id);
    std::vector<Event> events;
    env.events = &events;
    const int hourlyChecks = 10 * 24;
    for (int hour = 0; hour < hourlyChecks; ++hour) {
        env.time.tick = static_cast<std::int64_t>(hour) * kTicksPerGameHour +
                        (kTicksPerGameHour - static_cast<int>(id.value % kTicksPerGameHour));
        animals.Step(env);
        const Animal* animal = animals.Find(id);
        ASSERT_NE(animal, nullptr);
        EXPECT_FALSE(animal->loose);
    }
    EXPECT_EQ(animals.Escapes(), 0);
    EXPECT_TRUE(std::none_of(events.begin(), events.end(),
                             [](const Event& event) { return event.type == EventType::AnimalEscaped; }));
}

TEST(AnimalMovementTests, ALooseAnimalStopsBeingLooseAfterItIsFencedInAgain) {
    World world(40, 40);
    ZooObjects objects;
    Catalog catalog = Catalog::LoadDirectory(std::string(ZOO_SOURCE_DIR) + "/data/construction");
    SpeciesCatalog species = SpeciesCatalog::LoadDirectory(std::string(ZOO_SOURCE_DIR) + "/data/species");
    const int weakFence = catalog.fences.front().kind;
    for (int x = 8; x <= 30; ++x) {
        world.SetFence({x, 8, true}, {static_cast<std::uint8_t>(weakFence), false, 1.0f});
        world.SetFence({x, 31, true}, {static_cast<std::uint8_t>(weakFence), false, 1.0f});
    }
    for (int z = 8; z <= 30; ++z) {
        world.SetFence({8, z, false}, {static_cast<std::uint8_t>(weakFence), false, 1.0f});
        world.SetFence({31, z, false}, {static_cast<std::uint8_t>(weakFence), false, 1.0f});
    }
    Enclosures enclosures;
    enclosures.Rebuild(world, objects, catalog);
    Navigator nav(world);
    Animals animals(83U);
    const int lion = species.IndexOf("lion");
    ASSERT_GE(lion, 0);
    Env env{world, objects, enclosures, catalog, species, nav, nullptr, nullptr, {}, {}};
    const AnimalId id = animals.Add(env, lion, Sex::Female, 5.0f, World::TileCenter({18, 18}));
    ASSERT_TRUE(id);
    Animal* animal = animals.Find(id);
    ASSERT_NE(animal, nullptr);
    animal->hunger = 0.0f;
    animal->thirst = 0.0f;
    for (int tick = 0; tick < 10 * kTicksPerDay && !animal->loose; ++tick) {
        env.time.tick = tick;
        animals.Step(env);
        animal = animals.Find(id);
        ASSERT_NE(animal, nullptr);
    }
    ASSERT_TRUE(animal->loose);
    const TileCoord occupied = World::TileAt(animal->position);
    const int minX = std::max(0, occupied.x - 1);
    const int maxX = std::min(world.Width() - 1, occupied.x + 1);
    const int minZ = std::max(0, occupied.z - 1);
    const int maxZ = std::min(world.Depth() - 1, occupied.z + 1);
    const int strongFence = catalog.FindFence("high_glass_wall")->kind;
    for (int x = minX; x <= maxX; ++x) {
        world.SetFence({x, minZ, true}, {static_cast<std::uint8_t>(strongFence), false, 1.0f});
        world.SetFence({x, maxZ + 1, true}, {static_cast<std::uint8_t>(strongFence), false, 1.0f});
    }
    for (int z = minZ; z <= maxZ; ++z) {
        world.SetFence({minX, z, false}, {static_cast<std::uint8_t>(strongFence), false, 1.0f});
        world.SetFence({maxX + 1, z, false}, {static_cast<std::uint8_t>(strongFence), false, 1.0f});
    }
    enclosures.Rebuild(world, objects, catalog);
    animal->activity = AnimalActivity::Idle;
    animal->activityLeft = 1000;
    animal->moving = false;
    animal->speed = 0.0f;
    animal->targetProp = {};
    animal->route.clear();
    animal->routeIndex = 0;
    env.time.tick += 1;
    animals.Step(env);
    animal = animals.Find(id);
    ASSERT_NE(animal, nullptr);
    EXPECT_NE(enclosures.At(animal->position), EnclosureId{});
    EXPECT_FALSE(animal->loose);
}

}  // namespace
}  // namespace zoo
