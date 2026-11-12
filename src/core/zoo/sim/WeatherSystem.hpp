// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>

#include "zoo/base/Rng.hpp"
#include "zoo/sim/Calendar.hpp"
#include "zoo/sim/Weather.hpp"

namespace zoo {

/// What the weather of a map is like over the year. The defaults describe a
/// temperate lowland (central Europe): about 1 °C in January, 21 °C in July.
struct Climate {
    float meanC = 11.0f;           ///< mean temperature of the year
    float seasonalSwingC = 10.0f;  ///< warmest month's mean minus the year's mean
    float diurnalSwingC = 5.0f;    ///< afternoon peak minus the day's mean under a clear sky
    float dayLengthSwingH = 4.0f;  ///< midsummer day length minus 12 hours
    float wetness = 1.0f;          ///< scales the chance of the sky turning wet; 0 = never rains

    bool operator==(const Climate&) const = default;
};

/// Everything of the weather that is saved with a game.
struct WeatherState {
    /// The sky's state in the chain Clear .. Storm. Never Snow: snow is rain or a
    /// storm that falls while it freezes (see WeatherSystem::Now).
    WeatherKind sky = WeatherKind::Clear;
    std::int32_t ticksLeft = 0;  ///< until the sky is decided anew
    float cloud = 0.15f;
    float rain = 0.0f;
    float wind = 0.15f;
    float rainTarget = 0.0f;  ///< what `rain` eases towards during this spell
    float windTarget = 0.15f;
    float offsetC = 0.0f;  ///< this spell's departure from the seasonal temperature, eased
    float offsetTargetC = 0.0f;
    bool snowing = false;  ///< with hysteresis around freezing
    std::uint64_t rngState = 0;
    std::uint64_t rngStream = 0;

    bool operator==(const WeatherState&) const = default;
};

/// The weather of a zoo: spells of clear, cloudy, overcast, rainy and stormy sky
/// following each other by chance (a seeded Markov chain decided every one to
/// four game hours), a temperature that follows the season, the hour and the
/// sky, and the daylight of the calendar.
///
/// Deterministic: equal seed, climate and sequence of Step() calls give equal weather.
class WeatherSystem {
public:
    explicit WeatherSystem(std::uint64_t seed = 1, const Climate& climate = {}, GameTime start = {});

    /// Advances by one tick; `time` is the moment the tick begins.
    void Step(GameTime time);

    const WeatherNow& Now() const { return now_; }
    const Climate& ClimateOf() const { return climate_; }

    /// The temperature the season and the hour alone would give (no spell offset, clear sky).
    float SeasonalTemperature(GameTime time) const;
    /// Hours between sunrise and sunset on the day of `time`.
    float DayLengthHours(GameTime time) const;
    /// 0 at night .. 1 by day, with about an hour and a half of dawn and of dusk.
    float Daylight(GameTime time) const;

    WeatherState State() const;
    /// Puts back a saved state; `time` is the moment of the last Step() before it
    /// was saved, so that Now() is exactly what it was.
    void Restore(const WeatherState& state, GameTime time);

private:
    void Decide(GameTime time);
    void Evaluate(GameTime time, bool decideSnow);

    Climate climate_;
    Rng rng_;
    WeatherState state_;
    WeatherNow now_;
};

}  // namespace zoo
