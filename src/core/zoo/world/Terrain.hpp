// SPDX-License-Identifier: MIT
#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

namespace zoo {

/// Ground cover of one tile. The order is part of the save format (names are
/// saved, not numbers, but renderers index arrays with it): append only.
enum class Terrain : std::uint8_t {
    Grass = 0,      ///< temperate lawn / meadow
    Savanna,        ///< dry yellow grass
    Dirt,           ///< bare earth
    Sand,           ///< desert or beach sand
    Rock,           ///< bare stone
    Snow,           ///< snow and ice
    ForestFloor,    ///< leaf litter and moss
    Mud,            ///< wet earth, wallows
    Count
};

inline constexpr int kTerrainCount = static_cast<int>(Terrain::Count);

inline constexpr std::array<std::string_view, kTerrainCount> kTerrainNames = {
    "grass", "savanna", "dirt", "sand", "rock", "snow", "forest_floor", "mud"};

constexpr std::string_view ToString(Terrain t) { return kTerrainNames[static_cast<std::size_t>(t)]; }

constexpr std::optional<Terrain> TerrainFromString(std::string_view name) {
    for (int i = 0; i < kTerrainCount; ++i) {
        if (kTerrainNames[static_cast<std::size_t>(i)] == name) {
            return static_cast<Terrain>(i);
        }
    }
    return std::nullopt;
}

/// Surface of a visitor path on a tile.
enum class PathKind : std::uint8_t { None = 0, Asphalt, Gravel, Cobblestone, Boardwalk, Count };

inline constexpr int kPathKindCount = static_cast<int>(PathKind::Count);

inline constexpr std::array<std::string_view, kPathKindCount> kPathKindNames = {
    "none", "asphalt", "gravel", "cobblestone", "boardwalk"};

constexpr std::string_view ToString(PathKind p) { return kPathKindNames[static_cast<std::size_t>(p)]; }

constexpr std::optional<PathKind> PathKindFromString(std::string_view name) {
    for (int i = 0; i < kPathKindCount; ++i) {
        if (kPathKindNames[static_cast<std::size_t>(i)] == name) {
            return static_cast<PathKind>(i);
        }
    }
    return std::nullopt;
}

}  // namespace zoo
