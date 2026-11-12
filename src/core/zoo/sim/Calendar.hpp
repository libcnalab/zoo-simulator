// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>

namespace zoo {

/// Simulation time.
///
/// The simulation advances in fixed ticks of kTickSeconds of "motion time": the
/// time in which bodies move (an animal walking 1 m/s covers 1 m in 20 ticks).
/// The calendar runs kTimeScale times faster than motion, as in every management
/// game: one game day lasts 12 real minutes at normal speed. Slow processes
/// (hunger, ageing, gestation) are defined in calendar time.
inline constexpr int kTicksPerSecond = 20;
inline constexpr float kTickSeconds = 1.0f / static_cast<float>(kTicksPerSecond);
inline constexpr int kTimeScale = 120;
inline constexpr int kGameSecondsPerTick = kTimeScale / kTicksPerSecond;  // 6
inline constexpr int kTicksPerGameHour = 3600 / kGameSecondsPerTick;      // 600
inline constexpr int kTicksPerDay = 24 * kTicksPerGameHour;               // 14400
inline constexpr int kDaysPerMonth = 4;
inline constexpr int kMonthsPerYear = 12;
inline constexpr int kDaysPerYear = kDaysPerMonth * kMonthsPerYear;  // 48
/// Real-world durations of biology (gestation, lifespan, ...) are given in real
/// days or years in the data and converted with these: a game year stands for a
/// real year although it only has kDaysPerYear game days.
inline constexpr float kGameDaysPerRealYear = static_cast<float>(kDaysPerYear);
inline constexpr float kGameDaysPerRealDay = kGameDaysPerRealYear / 365.25f;

enum class Season : std::uint8_t { Spring = 0, Summer, Autumn, Winter };

/// A point in simulation time: ticks since the game started. The game starts on
/// day 0 (1 March of year 1) at kStartHour.
///
/// Two clocks live here. `tick` is the motion clock: it advances by one with every
/// step, and everything a body does is timed by it (how long an animal has been
/// hungry, when a visitor goes home; Days()). The calendar (the date, the hour, the
/// season: what the sun, the weather and the opening hours follow) is the motion
/// clock plus `shift`. In a management game the shift stays 0 and a day passes in
/// twelve minutes; a zoo that is visited on foot holds its calendar back
/// (Zoo::SetDayPace) so that a morning lasts as long as a visit does.
struct GameTime {
    std::int64_t tick = 0;
    /// Calendar ticks minus motion ticks (see above).
    std::int64_t shift = 0;

    static constexpr int kStartHour = 8;
    static constexpr std::int64_t kStartOffset = static_cast<std::int64_t>(kStartHour) * kTicksPerGameHour;

    /// Ticks of the calendar since midnight of day 0.
    constexpr std::int64_t AbsoluteTick() const { return tick + shift + kStartOffset; }
    /// The same of the motion clock: what daily routines that must keep the pace of
    /// the bodies (refilling the feeders) go by.
    constexpr std::int64_t MotionTick() const { return tick + kStartOffset; }
    /// Whole days since the start of the calendar.
    constexpr std::int64_t Day() const { return AbsoluteTick() / kTicksPerDay; }
    /// Time of day in hours, [0, 24).
    constexpr float Hour() const {
        return static_cast<float>(AbsoluteTick() % kTicksPerDay) / static_cast<float>(kTicksPerGameHour);
    }
    constexpr int DayOfMonth() const { return static_cast<int>(Day() % kDaysPerMonth) + 1; }
    /// 0 = March ... 11 = February, so that the game starts in spring.
    constexpr int MonthIndex() const { return static_cast<int>((Day() / kDaysPerMonth) % kMonthsPerYear); }
    /// Calendar month number, 1 = January.
    constexpr int Month() const { return (MonthIndex() + 2) % 12 + 1; }
    constexpr int Year() const { return static_cast<int>(Day() / kDaysPerYear) + 1; }
    constexpr Season SeasonNow() const { return static_cast<Season>(MonthIndex() / 3); }
    /// Fraction of the year elapsed, [0, 1), 0 = 1 March.
    constexpr float YearFraction() const {
        return static_cast<float>(AbsoluteTick() % (static_cast<std::int64_t>(kDaysPerYear) * kTicksPerDay)) /
               static_cast<float>(static_cast<std::int64_t>(kDaysPerYear) * kTicksPerDay);
    }
    /// Game days (fractional) since the game started; the unit of slow processes.
    constexpr double Days() const { return static_cast<double>(tick) / static_cast<double>(kTicksPerDay); }

    constexpr bool operator==(const GameTime&) const = default;
};

/// Game days advanced by one tick.
inline constexpr float kDaysPerTick = 1.0f / static_cast<float>(kTicksPerDay);
/// Game hours advanced by one tick.
inline constexpr float kHoursPerTick = 1.0f / static_cast<float>(kTicksPerGameHour);

}  // namespace zoo
