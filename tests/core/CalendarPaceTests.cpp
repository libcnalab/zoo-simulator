// SPDX-License-Identifier: MIT
#include <gtest/gtest.h>

#include "TestData.hpp"
#include "zoo/sim/Scenario.hpp"
#include "zoo/sim/Zoo.hpp"
#include "zoo/sim/ZooSaveJson.hpp"

namespace zoo {

namespace {

void Advance(Zoo& zoo, std::int64_t ticks) {
    for (std::int64_t i = 0; i < ticks; ++i) {
        zoo.Step();
    }
}

std::unique_ptr<Zoo> StarterZoo(std::uint64_t seed) {
    ZooConfig config;
    config.seed = seed;
    auto zoo = std::make_unique<Zoo>(testdata::GameCatalog(), testdata::GameSpecies(), config);
    BuildStarterZoo(*zoo);
    return zoo;
}

}  // namespace

TEST(CalendarPaceTests, TheCalendarOfAManagementGameIsItsMotionClock) {
    const GameTime start;
    EXPECT_EQ(start.AbsoluteTick(), start.MotionTick());
    EXPECT_FLOAT_EQ(start.Hour(), 8.0f);
    const std::unique_ptr<Zoo> zoo = StarterZoo(3);
    Advance(*zoo, kTicksPerGameHour * 3);
    EXPECT_EQ(zoo->Time().shift, 0);
    EXPECT_FLOAT_EQ(zoo->Time().Hour(), 11.0f);
}

TEST(CalendarPaceTests, TheCalendarCanBeSetWithoutAnybodyAgeing) {
    const std::unique_ptr<Zoo> zoo = StarterZoo(3);
    Advance(*zoo, 500);
    const std::int64_t tick = zoo->Time().tick;
    const double days = zoo->Time().Days();
    zoo->SetCalendar(14, 10.5f);
    EXPECT_EQ(zoo->Time().tick, tick);
    EXPECT_DOUBLE_EQ(zoo->Time().Days(), days);
    EXPECT_EQ(zoo->Time().Day(), 14);
    EXPECT_NEAR(zoo->Time().Hour(), 10.5f, 0.01f);
    EXPECT_EQ(zoo->Time().SeasonNow(), Season::Summer);
}

TEST(CalendarPaceTests, AStoppedCalendarStandsWhileTheZooLives) {
    const std::unique_ptr<Zoo> zoo = StarterZoo(4);
    zoo->SetCalendar(14, 10.5f);
    zoo->SetDayPace(0);
    const float hour = zoo->Time().Hour();
    // Three hours of the motion clock: long enough for people to have come in.
    Advance(*zoo, kTicksPerGameHour * 3);
    EXPECT_FLOAT_EQ(zoo->Time().Hour(), hour);
    EXPECT_EQ(zoo->Time().Day(), 14);
    EXPECT_EQ(zoo->Time().tick, kTicksPerGameHour * 3);
    EXPECT_GT(zoo->Guests().Count(), 20) << "the gate is open at half past ten";
}

TEST(CalendarPaceTests, ASlowCalendarMovesOneTickInSoManySteps) {
    const std::unique_ptr<Zoo> zoo = StarterZoo(4);
    zoo->SetCalendar(14, 10.0f);
    zoo->SetDayPace(12);
    Advance(*zoo, kTicksPerGameHour * 12);
    EXPECT_NEAR(zoo->Time().Hour(), 11.0f, 0.01f);
    Advance(*zoo, kTicksPerGameHour * 6);
    EXPECT_NEAR(zoo->Time().Hour(), 11.5f, 0.01f);
}

TEST(CalendarPaceTests, TheFeedersAreFilledAtThePaceOfTheAnimals) {
    // A day and a half of the motion clock with the calendar standing: the animals
    // have eaten for that long and must not have gone hungry for the keepers.
    const std::unique_ptr<Zoo> zoo = StarterZoo(5);
    zoo->SetCalendar(14, 10.5f);
    zoo->SetDayPace(0);
    Advance(*zoo, kTicksPerDay * 3 / 2);
    ASSERT_GT(zoo->AnimalCount(), 0);
    for (const Animal& animal : zoo->Fauna().All()) {
        EXPECT_LT(animal.hunger, 0.9f) << "animal " << animal.id.value;
    }
}

TEST(CalendarPaceTests, ASlowZooIsDeterministicAndSurvivesASave) {
    const std::unique_ptr<Zoo> a = StarterZoo(6);
    const std::unique_ptr<Zoo> b = StarterZoo(6);
    for (Zoo* zoo : {a.get(), b.get()}) {
        zoo->SetCalendar(14, 10.5f);
        zoo->SetDayPace(0);
        Advance(*zoo, 2500);
        zoo->SetDayPace(12);
        Advance(*zoo, 1237);
    }
    EXPECT_EQ(a->StateHash(), b->StateHash());

    std::string error;
    const std::unique_ptr<Zoo> loaded =
        ZooFromJson(ZooToJson(*a), testdata::GameCatalog(), testdata::GameSpecies(), &error);
    ASSERT_NE(loaded, nullptr) << error;
    EXPECT_EQ(a->Time(), loaded->Time());
    EXPECT_EQ(a->Config().dayPace, 12);
    EXPECT_EQ(loaded->Config().dayPace, 12);
    EXPECT_EQ(a->Weather(), loaded->Weather());
    EXPECT_EQ(a->StateHash(), loaded->StateHash());
    Advance(*a, 900);
    Advance(*loaded, 900);
    EXPECT_EQ(a->StateHash(), loaded->StateHash());
    EXPECT_EQ(a->Weather(), loaded->Weather());
}

TEST(CalendarPaceTests, AZooSavedBeforeItsFirstStepKeepsItsSky) {
    const std::unique_ptr<Zoo> zoo = StarterZoo(6);
    zoo->SetCalendar(14, 10.5f);
    std::string error;
    const std::unique_ptr<Zoo> loaded =
        ZooFromJson(ZooToJson(*zoo), testdata::GameCatalog(), testdata::GameSpecies(), &error);
    ASSERT_NE(loaded, nullptr) << error;
    EXPECT_EQ(zoo->Weather(), loaded->Weather());
}

}  // namespace zoo
