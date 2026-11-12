// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>

namespace zoo {

/// The sky's mood. Names are part of the save format; append only.
enum class WeatherKind : std::uint8_t { Clear, Cloudy, Overcast, Rain, Storm, Snow, Count };

/// The weather at one moment, as the rest of the simulation sees it.
struct WeatherNow {
    WeatherKind kind = WeatherKind::Clear;
    float temperatureC = 18.0f;  ///< air temperature in the open
    float rain = 0.0f;           ///< precipitation intensity, 0 dry .. 1 downpour (snow counts)
    float cloud = 0.2f;          ///< sky cover, 0..1
    float wind = 0.2f;           ///< 0 calm .. 1 gale
    /// 0 in the dark of night .. 1 in full daylight (dawn and dusk in between).
    float daylight = 1.0f;

    bool Snowing() const { return kind == WeatherKind::Snow; }
    bool operator==(const WeatherNow&) const = default;
};

}  // namespace zoo
