// SPDX-License-Identifier: MIT
#include <gtest/gtest.h>

#include <array>

#include "zoo/sim/WeatherSystem.hpp"

namespace zoo {

TEST(WeatherTests, AYearStaysWithinSensibleBounds) {
    WeatherSystem weather(7);
    std::array<int, static_cast<std::size_t>(WeatherKind::Count)> ticksOfKind{};
    float coldest = 100.0f;
    float warmest = -100.0f;
    double summer = 0.0;
    double winter = 0.0;
    int summerTicks = 0;
    int winterTicks = 0;
    GameTime time;
    for (std::int64_t i = 0; i < static_cast<std::int64_t>(kTicksPerDay) * kDaysPerYear; ++i, ++time.tick) {
        weather.Step(time);
        const WeatherNow& now = weather.Now();
        ASSERT_GE(now.rain, 0.0f);
        ASSERT_LE(now.rain, 1.0f);
        ASSERT_GE(now.cloud, 0.0f);
        ASSERT_LE(now.cloud, 1.0f);
        ASSERT_GE(now.wind, 0.0f);
        ASSERT_LE(now.wind, 1.0f);
        ASSERT_GE(now.daylight, 0.0f);
        ASSERT_LE(now.daylight, 1.0f);
        ++ticksOfKind[static_cast<std::size_t>(now.kind)];
        coldest = std::min(coldest, now.temperatureC);
        warmest = std::max(warmest, now.temperatureC);
        if (time.SeasonNow() == Season::Summer) {
            summer += now.temperatureC;
            ++summerTicks;
        } else if (time.SeasonNow() == Season::Winter) {
            winter += now.temperatureC;
            ++winterTicks;
        }
        if (now.kind == WeatherKind::Snow) {
            ASSERT_LT(now.temperatureC, 3.0f) << "snow in the warm";
        }
    }
    EXPECT_GT(coldest, -25.0f);
    EXPECT_LT(coldest, 2.0f) << "a temperate winter has frost";
    EXPECT_LT(warmest, 40.0f);
    EXPECT_GT(warmest, 22.0f);
    EXPECT_GT(summer / summerTicks, winter / winterTicks + 10.0);
    // Every kind of sky happens, and fair weather is the usual one.
    const double total = static_cast<double>(kTicksPerDay) * kDaysPerYear;
    for (std::size_t k = 0; k < ticksOfKind.size(); ++k) {
        EXPECT_GT(ticksOfKind[k], 0) << "kind " << k << " never occurred";
    }
    const double fair = (ticksOfKind[0] + ticksOfKind[1]) / total;
    const double wet = (ticksOfKind[3] + ticksOfKind[4] + ticksOfKind[5]) / total;
    EXPECT_GT(fair, 0.45);
    EXPECT_GT(wet, 0.05);
    EXPECT_LT(wet, 0.35);
}

TEST(WeatherTests, NightsAreDarkAndSummerDaysLong) {
    WeatherSystem weather(1);
    GameTime midsummerNoon{static_cast<std::int64_t>(kTicksPerDay) * 16 +
                           4 * kTicksPerGameHour};  // 12:00, mid June
    GameTime midnight{static_cast<std::int64_t>(kTicksPerDay) * 16 + 16 * kTicksPerGameHour};  // 00:00
    EXPECT_FLOAT_EQ(weather.Daylight(midsummerNoon), 1.0f);
    EXPECT_FLOAT_EQ(weather.Daylight(midnight), 0.0f);
    const GameTime midwinter{static_cast<std::int64_t>(kTicksPerDay) * 40};
    EXPECT_GT(weather.DayLengthHours(midsummerNoon), weather.DayLengthHours(midwinter) + 5.0f);
    EXPECT_GT(weather.SeasonalTemperature(midsummerNoon), weather.SeasonalTemperature(midwinter) + 12.0f);
}

TEST(WeatherTests, EqualSeedsGiveEqualWeatherAndDifferentSeedsDoNot) {
    WeatherSystem a(42);
    WeatherSystem b(42);
    WeatherSystem c(43);
    GameTime time;
    bool differs = false;
    for (int i = 0; i < kTicksPerDay * 6; ++i, ++time.tick) {
        a.Step(time);
        b.Step(time);
        c.Step(time);
        ASSERT_EQ(a.Now(), b.Now());
        differs = differs || !(a.Now() == c.Now());
    }
    EXPECT_EQ(a.State(), b.State());
    EXPECT_TRUE(differs);
}

TEST(WeatherTests, RestoreContinuesExactly) {
    WeatherSystem a(9);
    GameTime time;
    for (int i = 0; i < kTicksPerDay * 3 + 777; ++i, ++time.tick) {
        a.Step(time);
    }
    WeatherSystem b(1);
    b.Restore(a.State(), GameTime{time.tick - 1});
    EXPECT_EQ(a.Now(), b.Now());
    for (int i = 0; i < kTicksPerDay * 3; ++i, ++time.tick) {
        a.Step(time);
        b.Step(time);
        ASSERT_EQ(a.Now(), b.Now()) << "tick " << time.tick;
    }
    EXPECT_EQ(a.State(), b.State());
}

TEST(WeatherTests, AClimateShiftsTheTemperature) {
    Climate arctic;
    arctic.meanC = -8.0f;
    Climate tropic;
    tropic.meanC = 26.0f;
    tropic.seasonalSwingC = 2.0f;
    WeatherSystem cold(3, arctic);
    WeatherSystem hot(3, tropic);
    GameTime time;
    for (int i = 0; i < kTicksPerDay; ++i, ++time.tick) {
        cold.Step(time);
        hot.Step(time);
        ASSERT_LT(cold.Now().temperatureC + 20.0f, hot.Now().temperatureC);
    }
}

}  // namespace zoo
