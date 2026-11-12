// SPDX-License-Identifier: MIT
#pragma once

#include "Common.hpp"
#include "zoo/world/World.hpp"

namespace zoogame {

/// Plain grass repeats after this many metres in both directions, so that the
/// land beyond the map (one repeating texture) continues the map's own grass
/// without a visible join.
inline constexpr float kGrassPeriodM = 64.0f;

/// Linear colour of plain grass at a world position.
zoo::Color3 GrassColor(float x, float z);

/// What the ground painter needs to know about one tile besides the world.
struct TilePaintInfo {
    zoo::TileCoord tile;
    /// Highest water level among the tile and its eight neighbours, or kNoWater.
    float waterLevel = zoo::kNoWater;
};

TilePaintInfo PaintInfo(const zoo::World& world, zoo::TileCoord tile);

/// Linear colour of the ground at a world position inside `info.tile`: terrain
/// with soft, irregular borders, crisp paths, foundations, bare rock on steep
/// slopes, wet shores and lake beds.
zoo::Color3 GroundColor(const zoo::World& world, const TilePaintInfo& info, float x, float z);

/// A value that changes whenever anything GroundColor reads for a tile changes.
std::uint32_t PaintSignature(const zoo::World& world, zoo::TileCoord tile);

}  // namespace zoogame
