// SPDX-License-Identifier: MIT
#include <gtest/gtest.h>

#include "TestData.hpp"
#include "zoo/sim/Scenario.hpp"
#include "zoo/sim/Zoo.hpp"

namespace zoo {

namespace {

void Advance(Zoo& zoo, std::int64_t ticks) {
    for (std::int64_t i = 0; i < ticks; ++i) {
        zoo.Step();
    }
}

int Lion() {
    return testdata::GameSpecies().IndexOf("lion");
}

}  // namespace

TEST(ZooTests, ANewZooIsEmptyAndFunded) {
    ZooConfig config;
    config.name = "Riverside";
    config.startingMoney = Dollars(12345);
    Zoo zoo(testdata::GameCatalog(), testdata::GameSpecies(), config);
    EXPECT_EQ(zoo.Name(), "Riverside");
    EXPECT_EQ(zoo.Accounts().Balance(), Dollars(12345));
    EXPECT_EQ(zoo.AnimalCount(), 0);
    EXPECT_EQ(zoo.Time().tick, 0);
    EXPECT_TRUE(zoo.Notifications().empty());
}

TEST(ZooTests, EqualSeedsStayInStepAndDifferentSeedsPart) {
    ZooConfig config;
    config.seed = 77;
    Zoo a(testdata::GameCatalog(), testdata::GameSpecies(), config);
    Zoo b(testdata::GameCatalog(), testdata::GameSpecies(), config);
    config.seed = 78;
    Zoo c(testdata::GameCatalog(), testdata::GameSpecies(), config);
    BuildStarterZoo(a);
    BuildStarterZoo(b);
    BuildStarterZoo(c);
    for (int hour = 0; hour < 30; ++hour) {
        Advance(a, kTicksPerGameHour);
        Advance(b, kTicksPerGameHour);
        ASSERT_EQ(a.StateHash(), b.StateHash()) << "after hour " << hour;
    }
    EXPECT_TRUE(a.SameState(b));
    Advance(c, static_cast<std::int64_t>(kTicksPerGameHour) * 30);
    EXPECT_NE(a.StateHash(), c.StateHash());
}

TEST(ZooTests, TheDayEndsWithUpkeepWearAndAReport) {
    Zoo zoo(testdata::GameCatalog(), testdata::GameSpecies());
    BuildStarterZoo(zoo);
    const Money upkeep = zoo.DailyUpkeep();
    EXPECT_GT(upkeep, 0);
    float conditionBefore = 1.0f;
    for (const Enclosure& e : zoo.Habitats().All()) {
        conditionBefore = std::min(conditionBefore, e.stats.minFenceCondition);
    }
    EXPECT_FLOAT_EQ(conditionBefore, 1.0f);

    const std::int64_t day = zoo.Time().Day();
    while (zoo.Time().Day() == day) {
        zoo.Step();
    }
    ASSERT_EQ(zoo.Accounts().History().size(), 1U);
    const DayReport& report = zoo.Accounts().History().back();
    EXPECT_EQ(report.day, day);
    EXPECT_EQ(report.amounts[static_cast<std::size_t>(Account::Upkeep)], upkeep);
    EXPECT_EQ(report.balance, zoo.Accounts().Balance());
    for (const Enclosure& e : zoo.Habitats().All()) {
        EXPECT_LT(e.stats.minFenceCondition, 1.0f) << e.name << ": fences wear a little every day";
        EXPECT_GT(e.stats.minFenceCondition, 0.9f) << e.name;
    }
    bool told = false;
    for (const Notification& n : zoo.Notifications()) {
        told = told || n.text.find("closed with") != std::string::npos;
    }
    EXPECT_TRUE(told);
}

TEST(ZooTests, ASandboxZooPaysNothing) {
    ZooConfig config;
    config.sandbox = true;
    Zoo zoo(testdata::GameCatalog(), testdata::GameSpecies(), config);
    BuildStarterZoo(zoo);
    const Money before = zoo.Accounts().Balance();
    const std::vector<EdgeRef> edges = EdgesAround({40, 40}, {44, 44});
    ASSERT_TRUE(zoo.Build().PlaceFence(edges, 1).Ok());
    Advance(zoo, kTicksPerDay + 10);
    EXPECT_EQ(zoo.Accounts().Balance(), before);
}

TEST(ZooTests, BuyingSellingAndMovingAnimals) {
    Zoo zoo(testdata::GameCatalog(), testdata::GameSpecies());
    BuildStarterZoo(zoo);
    EnclosureId lions;
    for (const Enclosure& e : zoo.Habitats().All()) {
        if (e.name == "Lion Savanna") {
            lions = e.id;
        }
    }
    ASSERT_TRUE(lions);
    const Vec2 inside = zoo.Fauna().All().front().position;
    const Money price = zoo.PriceOf(Lion());
    ASSERT_GT(price, 0);
    const Money before = zoo.Accounts().Balance();

    EXPECT_EQ(zoo.BuyAnimal(999, Sex::Female, inside).error, ActionError::UnknownSpecies);
    EXPECT_EQ(zoo.BuyAnimal(Lion(), Sex::Female, {-500.0f, -500.0f}).error, ActionError::BadPlace);
    EXPECT_EQ(zoo.Accounts().Balance(), before) << "a failed order costs nothing";

    const BuyResult bought = zoo.BuyAnimal(Lion(), Sex::Female, inside);
    ASSERT_TRUE(bought.Ok());
    EXPECT_EQ(bought.price, price);
    EXPECT_EQ(zoo.Accounts().Balance(), before - price);
    EXPECT_EQ(zoo.AnimalCount(Lion()), 4);
    const Animal* lioness = zoo.Fauna().Find(bought.animal);
    ASSERT_NE(lioness, nullptr);
    EXPECT_LT(lioness->bornTick, 0) << "bought as an adult";

    const Vec2 other = zoo.Fauna().All()[1].position;
    EXPECT_EQ(zoo.MoveAnimal(bought.animal, other), ActionError::None);
    EXPECT_EQ(zoo.Fauna().Find(bought.animal)->position, other);
    EXPECT_EQ(zoo.MoveAnimal(AnimalId{9999}, other), ActionError::UnknownAnimal);
    EXPECT_EQ(zoo.RecaptureAnimal(bought.animal), ActionError::NotLoose);
    EXPECT_EQ(zoo.TreatAnimal(bought.animal), ActionError::NotSick);

    const Money fetched = zoo.SellAnimal(bought.animal);
    EXPECT_GT(fetched, 0);
    EXPECT_EQ(zoo.Accounts().Balance(), before - price + fetched);
    EXPECT_EQ(zoo.AnimalCount(Lion()), 3);
    EXPECT_EQ(zoo.SellAnimal(bought.animal), -1);
}

TEST(ZooTests, APoorZooCannotBuy) {
    ZooConfig config;
    config.startingMoney = Dollars(100);
    Zoo zoo(testdata::GameCatalog(), testdata::GameSpecies(), config);
    BuildStarterZoo(zoo);
    const Vec2 inside = zoo.Fauna().All().front().position;
    EXPECT_EQ(zoo.BuyAnimal(Lion(), Sex::Male, inside).error, ActionError::NoMoney);
    EXPECT_EQ(zoo.AnimalCount(Lion()), 3);
}

TEST(ZooTests, KeepersKeepAStarterZooAliveForAWeek) {
    Zoo zoo(testdata::GameCatalog(), testdata::GameSpecies());
    BuildStarterZoo(zoo);
    Advance(zoo, static_cast<std::int64_t>(kTicksPerDay) * 7);
    EXPECT_GE(zoo.AnimalCount(), 12) << "nobody died in the first week";
    EXPECT_GT(zoo.Accounts().Lifetime()[static_cast<std::size_t>(Account::AnimalFood)], 0)
        << "the rounds cost food";
    EXPECT_GT(zoo.MeanWelfare(), 0.55f);
    for (const Animal& animal : zoo.Fauna().All()) {
        EXPECT_FALSE(animal.loose) << animal.name;
    }
    for (const Notification& n : zoo.Notifications()) {
        EXPECT_NE(n.severity, Severity::Alarm) << n.text;
    }
}

TEST(ZooTests, WithoutCareAnimalsGoHungryAndTheZooSaysSo) {
    ZooConfig config;
    config.autoCare = false;
    Zoo zoo(testdata::GameCatalog(), testdata::GameSpecies(), config);
    BuildStarterZoo(zoo);
    Advance(zoo, static_cast<std::int64_t>(kTicksPerDay) * 12);
    bool warned = false;
    for (const Notification& n : zoo.Notifications()) {
        warned = warned || n.severity == Severity::Warning || n.severity == Severity::Alarm;
    }
    EXPECT_TRUE(warned);
    EXPECT_LE(static_cast<int>(zoo.Notifications().size()), Zoo::kMaxNotifications);
}

TEST(ZooTests, EventsCanBeDrained) {
    ZooConfig config;
    config.autoCare = false;
    Zoo zoo(testdata::GameCatalog(), testdata::GameSpecies(), config);
    BuildStarterZoo(zoo);
    std::vector<Event> events;
    std::size_t seen = 0;
    for (int day = 0; day < 12; ++day) {
        Advance(zoo, kTicksPerDay);
        events.clear();
        zoo.DrainEvents(events);
        seen += events.size();
    }
    EXPECT_GT(seen, 0U);
    events.clear();
    zoo.DrainEvents(events);
    EXPECT_TRUE(events.empty());
}

}  // namespace zoo
