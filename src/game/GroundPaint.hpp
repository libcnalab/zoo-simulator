// SPDX-License-Identifier: MIT
#pragma once

#include <array>
#include <cstdint>

#include "Common.hpp"
#include "zoo/world/World.hpp"

namespace zoogame {

/// Plain grass repeats after this many metres in both directions, so that the
/// land beyond the map (one repeating texture) continues the map's own grass
/// without a visible join.
inline constexpr float kGrassPeriodM = 64.0f;

/// Linear colour of plain grass at a world position.
zoo::Color3 GrassColor(float x, float z);

/// Which fine pattern lies over the baked ground colour close to the eye
/// (GroundDetail.hpp). The baked colour holds 6 cm a texel; what is finer than
/// that (stones, blades, the joints of paving) comes from the detail layer.
/// The kinds differ in pattern and in tone: a layer lightens and darkens about
/// the ground's own colour, so pale gravel, dark earth and asphalt each need
/// their own.
enum class GroundDetail : std::uint8_t {
    None,    ///< snow, timber, the bed of a pool
    Grain,   ///< pale and stony: gravel, rock, concrete
    Sand,    ///< the same, finer
    Soil,    ///< dark and crumbly: earth, mulch, mud, the forest's floor, setts
    Tar,     ///< asphalt
    Turf,    ///< a lawn's blades
    Straw,   ///< dry grass
    Paving,  ///< slabs: their joints and worn edges
};
inline constexpr int kGroundDetailCount = 8;

/// One fence segment near a tile, in world metres. The ground wear is the distance
/// to this finite segment rather than to the whole tile side, so a free end is round
/// and joined segments blend through a corner.
struct GroundFence {
    zoo::Vec2 from;
    zoo::Vec2 to;
    bool gate = false;
};

/// Side of one paving slab (PathKind::Cobblestone is laid as slabs in a running
/// bond). The baked colour and the detail layer's joints share this grid.
inline constexpr float kPavingSlabM = 0.5f;

/// What the ground painter needs to know about one tile: mostly the tile and its
/// eight neighbours (index (dz + 1) * 3 + (dx + 1)); the corners' slopes look one
/// vertex further. Everything the painter reads is in here, so painting a tile
/// never touches the world.
struct TilePaintInfo {
    zoo::TileCoord tile;
    /// Highest water level among the tile and its eight neighbours, or kNoWater.
    float waterLevel = zoo::kNoWater;
    std::array<zoo::Terrain, 9> terrain{};
    std::array<zoo::PathKind, 9> path{};
    std::array<bool, 9> building{};
    std::array<bool, 9> standOff{};
    /// The tile's corner heights as they are drawn (DrawnVertexHeight), the world's
    /// own ones, and whether a corner belongs to a water tile; index dz * 2 + dx.
    std::array<float, 4> height{};
    std::array<float, 4> bed{};
    /// Of a bank tile: kBankLipM above the water at each corner (or the world's
    /// height where that is higher), where the bank begins to fall to the bed.
    std::array<float, 4> rim{};
    std::array<bool, 4> dug{};
    /// Of a water tile: bit `Dir`, the tile across that side is dry land; bit
    /// `4 + corner`, a dry tile touches that corner.
    std::uint8_t dryEdges = 0;
    /// The steepest slope met at each corner (rise over run, the steeper of the two
    /// tiles on either side, along each axis). Shared by every tile at that corner,
    /// so bare rock on a steep bank fades out across the next tile instead of
    /// ending at the tile's side.
    std::array<float, 4> slope{};
    /// Slowly varying lawn tones at the corners (0..1), interpolated inside.
    std::array<float, 4> lush{};
    std::array<float, 4> rough{};
    /// The tile's four sides and their immediate continuations: enough to cover the
    /// widest 1.5 m gate wear without stopping it at a tile boundary.
    std::array<GroundFence, 12> fences{};
    std::uint8_t fenceCount = 0;
    /// 0 at the map's rim, 1 well inside (at the tile's corners).
    std::array<float, 4> inland{};
    bool mixedTerrain = false;
    bool paved = false;    ///< a path or a building somewhere in the nine tiles
    bool mulched = false;  ///< a planted stand-off somewhere in the nine tiles
    bool steep = false;
    /// One terrain, nothing else: the painter takes its short way.
    bool plain = false;
    /// A water tile at the edge of its pool: the bank runs down from its dry sides
    /// to the bed inside it, and the waterline wanders across it (GroundHeight).
    bool bank = false;
};

TilePaintInfo PaintInfo(const zoo::World& world, zoo::TileCoord tile);

/// What the ground looks like at one point.
struct GroundSample {
    zoo::Color3 color{};
    GroundDetail detail = GroundDetail::None;
};

/// Linear colour of the ground at a world position inside `info.tile` (a point
/// a few centimetres outside is answered as if it were inside): terrain with
/// irregular borders, paths with rounded outlines, kerbs and worn verges,
/// foundations, bare rock on steep slopes, wet banks and lake beds.
GroundSample SampleGround(const TilePaintInfo& info, float x, float z);

/// How high above its water the ground is drawn where dry land meets a pool.
inline constexpr float kBankLipM = 0.05f;

/// Height of a vertex of the tile grid as it is drawn. World::SetWater digs all four
/// corners of a water tile down to the bed, and with them the corners of the dry
/// tiles beside it, so in the world a dry tile at a pool slopes down to the bed and
/// much of it lies under the water. Drawn, a vertex that both water and dry land
/// touch lies kBankLipM above the water instead: dry land stays dry, and the bank
/// runs down to the bed inside the water tile (GroundHeight). Every other vertex is
/// the world's.
float DrawnVertexHeight(const zoo::World& world, int vx, int vz);

/// Height of the drawn ground at a world position inside `info.tile`: the drawn
/// corners' two triangles, and in a bank tile the bank running down from the dry
/// sides to the bed along a wandering line. Along a tile's side it depends only on
/// what both tiles at that side know, so neighbours join without a step.
float GroundHeight(const TilePaintInfo& info, float x, float z);

/// Cells a side of a bank tile's mesh (25 cm each); DrawnGroundHeight follows the same cells.
inline constexpr int kGroundCells = 8;

/// Height of the ground as it is drawn at a world position: on the triangles of
/// TerrainRenderer's mesh, so a foot set there neither sinks into a bank nor floats
/// above it. Away from water it is World::HeightAt; on a tile at a pool it differs
/// from that by up to the pool's depth (DrawnVertexHeight). Points outside the map
/// are clamped onto its edge.
///
/// For everything the game draws standing on the ground (animals, visitors, keepers,
/// props, the player's collision), once per actor per frame: it reads the world only
/// (nine tiles' water, their corner heights), allocates nothing, keeps no cache and
/// is safe on any thread. The simulation's own heights stay World::HeightAt.
float DrawnGroundHeight(const zoo::World& world, zoo::Vec2 at);

/// A value that changes whenever anything PaintInfo reads for a tile changes.
std::uint32_t PaintSignature(const zoo::World& world, zoo::TileCoord tile);

}  // namespace zoogame
