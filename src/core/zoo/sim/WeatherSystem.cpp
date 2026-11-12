// SPDX-License-Identifier: MIT
#include "zoo/sim/WeatherSystem.hpp"

#include <algorithm>
#include <array>
#include <cmath>

#include "zoo/base/Math.hpp"

namespace zoo {

namespace {

constexpr int kSkyKinds = 5;  // Clear, Cloudy, Overcast, Rain, Storm

/// Chance of the next spell's sky (columns) given this spell's (rows), in a
/// temperate climate. Rows sum to 1. The sky changes one step at a time, so
/// rain always comes out of cloud and a storm out of an overcast or rainy sky.
constexpr std::array<std::array<float, kSkyKinds>, kSkyKinds> kTransitions = {{
    {0.70f, 0.27f, 0.03f, 0.00f, 0.00f},  // Clear
    {0.30f, 0.42f, 0.22f, 0.06f, 0.00f},  // Cloudy
    {0.00f, 0.28f, 0.37f, 0.30f, 0.05f},  // Overcast
    {0.00f, 0.12f, 0.35f, 0.45f, 0.08f},  // Rain
    {0.00f, 0.00f, 0.25f, 0.60f, 0.15f},  // Storm
}};

constexpr std::array<float, kSkyKinds> kCloudOf = {0.08f, 0.45f, 0.85f, 0.95f, 1.0f};

/// Per tick share of the remaining distance to the target: cloud, rain and wind
/// take about twenty game minutes to follow a change of the sky.
constexpr float kEase = 1.0f / 200.0f;
/// The temperature follows more slowly (about an hour).
constexpr float kTemperatureEase = 1.0f / 600.0f;

float SmoothStep(float edge0, float edge1, float x) {
    const float t = Clamp01((x - edge0) / (edge1 - edge0));
    return t * t * (3.0f - 2.0f * t);
}

}  // namespace

WeatherSystem::WeatherSystem(std::uint64_t seed, const Climate& climate, GameTime start)
    : climate_(climate), rng_(seed, 0x77656174686572ULL) {
    state_.ticksLeft = kTicksPerGameHour * 2;
    state_.cloud = kCloudOf[0] + 0.1f;
    state_.windTarget = state_.wind;
    Evaluate(start, true);
}

float WeatherSystem::SeasonalTemperature(GameTime time) const {
    // Year fraction 0 is 1 March; the warmest days are in mid July (0.375).
    const float season = std::sin(kTwoPi * (time.YearFraction() - 0.125f));
    const float day = std::cos(kTwoPi * (time.Hour() - 15.0f) / 24.0f);
    return climate_.meanC + climate_.seasonalSwingC * season + climate_.diurnalSwingC * day;
}

float WeatherSystem::DayLengthHours(GameTime time) const {
    // The spring equinox (20 March) is 0.055 of a year after 1 March.
    return 12.0f + climate_.dayLengthSwingH * std::sin(kTwoPi * (time.YearFraction() - 0.055f));
}

float WeatherSystem::Daylight(GameTime time) const {
    const float half = DayLengthHours(time) * 0.5f;
    const float fromNoon = std::abs(time.Hour() - 12.0f);
    return 1.0f - SmoothStep(half - 0.75f, half + 0.75f, fromNoon);
}

void WeatherSystem::Decide(GameTime time) {
    const int from = std::clamp(static_cast<int>(state_.sky), 0, kSkyKinds - 1);
    std::array<float, kSkyKinds> chance = kTransitions[static_cast<std::size_t>(from)];
    // Wet climates turn wet more readily; thunderstorms belong to summer.
    const float summer = 0.5f + 0.5f * std::sin(kTwoPi * (time.YearFraction() - 0.125f));
    chance[3] *= climate_.wetness;
    chance[4] *= climate_.wetness * (0.3f + 1.4f * summer);
    float total = 0.0f;
    for (const float c : chance) {
        total += c;
    }
    float pick = rng_.NextFloat() * total;
    int next = from;
    for (int i = 0; i < kSkyKinds; ++i) {
        pick -= chance[static_cast<std::size_t>(i)];
        if (pick < 0.0f) {
            next = i;
            break;
        }
    }
    state_.sky = static_cast<WeatherKind>(next);
    state_.ticksLeft = rng_.RangeInt(kTicksPerGameHour, kTicksPerGameHour * 4);
    // Storms pass quickly.
    if (state_.sky == WeatherKind::Storm) {
        state_.ticksLeft = rng_.RangeInt(kTicksPerGameHour / 2, kTicksPerGameHour * 3 / 2);
    }
    switch (state_.sky) {
        case WeatherKind::Rain:
            state_.rainTarget = rng_.Range(0.25f, 0.7f);
            state_.windTarget = rng_.Range(0.15f, 0.55f);
            break;
        case WeatherKind::Storm:
            state_.rainTarget = rng_.Range(0.85f, 1.0f);
            state_.windTarget = rng_.Range(0.6f, 1.0f);
            break;
        case WeatherKind::Overcast:
            state_.rainTarget = 0.0f;
            state_.windTarget = rng_.Range(0.1f, 0.5f);
            break;
        default:
            state_.rainTarget = 0.0f;
            state_.windTarget = rng_.Range(0.03f, 0.35f);
            break;
    }
    state_.offsetTargetC = rng_.Range(-3.0f, 3.0f);
}

void WeatherSystem::Step(GameTime time) {
    if (--state_.ticksLeft <= 0) {
        Decide(time);
    }
    const float cloudTarget =
        kCloudOf[static_cast<std::size_t>(std::clamp(static_cast<int>(state_.sky), 0, 4))];
    state_.cloud += (cloudTarget - state_.cloud) * kEase;
    // Rain only falls out of a sky that has closed in.
    const float rainTarget = state_.cloud > 0.7f ? state_.rainTarget : 0.0f;
    state_.rain += (rainTarget - state_.rain) * kEase;
    if (state_.rain < 0.004f && rainTarget == 0.0f) {
        state_.rain = 0.0f;
    }
    state_.wind += (state_.windTarget - state_.wind) * kEase;
    state_.offsetC += (state_.offsetTargetC - state_.offsetC) * kTemperatureEase;
    Evaluate(time, true);
}

void WeatherSystem::Evaluate(GameTime time, bool decideSnow) {
    // Cloud flattens the day's swing (cooler afternoons, milder nights) and rain cools.
    const float season = std::sin(kTwoPi * (time.YearFraction() - 0.125f));
    const float day = std::cos(kTwoPi * (time.Hour() - 15.0f) / 24.0f);
    const float temperature = climate_.meanC + climate_.seasonalSwingC * season +
                              climate_.diurnalSwingC * day * (1.0f - 0.6f * state_.cloud) + state_.offsetC -
                              2.5f * state_.rain;
    if (decideSnow) {
        if (temperature < 0.3f) {
            state_.snowing = true;
        } else if (temperature > 1.5f) {
            state_.snowing = false;
        }
    }

    now_.temperatureC = temperature;
    now_.cloud = Clamp01(state_.cloud);
    now_.rain = Clamp01(state_.rain);
    now_.wind = Clamp01(state_.wind);
    now_.daylight = Daylight(time);
    if (now_.rain > 0.03f) {
        now_.kind = state_.snowing      ? WeatherKind::Snow
                    : now_.rain > 0.78f ? WeatherKind::Storm
                                        : WeatherKind::Rain;
    } else if (now_.cloud > 0.68f) {
        now_.kind = WeatherKind::Overcast;
    } else if (now_.cloud > 0.3f) {
        now_.kind = WeatherKind::Cloudy;
    } else {
        now_.kind = WeatherKind::Clear;
    }
}

WeatherState WeatherSystem::State() const {
    WeatherState state = state_;
    state.rngState = rng_.State();
    state.rngStream = rng_.Stream();
    return state;
}

void WeatherSystem::Restore(const WeatherState& state, GameTime time) {
    state_ = state;
    state_.sky = static_cast<WeatherKind>(std::clamp(static_cast<int>(state.sky), 0, kSkyKinds - 1));
    rng_.Restore(state.rngState, state.rngStream);
    state_.rngState = 0;
    state_.rngStream = 0;
    Evaluate(time, false);
}

}  // namespace zoo
