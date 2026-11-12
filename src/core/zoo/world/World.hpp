// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "zoo/base/Ids.hpp"
#include "zoo/base/Math.hpp"
#include "zoo/world/Terrain.hpp"

namespace zoo {

/// Edge length of one tile in metres.
inline constexpr float kTileSize = 2.0f;
/// Renderers and caches partition the map into square chunks of this many tiles.
inline constexpr int kChunkTiles = 16;
/// Tile::waterLevel of a tile without water.
inline constexpr float kNoWater = -1.0e9f;
/// Fence::kind of the zoo's boundary wall: placed by the generator, never by the player.
inline constexpr std::uint8_t kPerimeterFence = 255;

struct Tile {
    Terrain terrain = Terrain::Grass;
    PathKind path = PathKind::None;
    /// The building whose footprint covers this tile, if any.
    BuildingId building;
    /// Height (world y) of the water surface on this tile, or kNoWater.
    float waterLevel = kNoWater;
};

/// A fence segment on one tile edge. `kind` 0 means "no fence"; other values index
/// the fence catalog (construction module), kPerimeterFence is the boundary wall.
struct Fence {
    std::uint8_t kind = 0;
    bool gate = false;
    /// 1 = new, 0 = collapsed. Worn fences let animals escape.
    float condition = 1.0f;

    constexpr bool Exists() const { return kind != 0; }
    constexpr bool operator==(const Fence&) const = default;
};

/// One tile edge, in normal form.
/// horizontal == true : the edge along x on the north side of tile (x, z);
///                      0 <= x < Width(), 0 <= z <= Depth().
/// horizontal == false: the edge along z on the west side of tile (x, z);
///                      0 <= x <= Width(), 0 <= z < Depth().
struct EdgeRef {
    int x = 0;
    int z = 0;
    bool horizontal = true;

    constexpr bool operator==(const EdgeRef&) const = default;
};

/// The edge of `tile` on side `d`, in normal form.
constexpr EdgeRef EdgeOf(TileCoord tile, Dir d) {
    switch (d) {
        case Dir::North:
            return {tile.x, tile.z, true};
        case Dir::South:
            return {tile.x, tile.z + 1, true};
        case Dir::West:
            return {tile.x, tile.z, false};
        case Dir::East:
            return {tile.x + 1, tile.z, false};
    }
    return {};
}

struct WorldGenParams {
    int width = 128;  ///< tiles along x
    int depth = 128;  ///< tiles along z
    std::uint64_t seed = 1;
    /// Scale of the generated relief; 0 gives a perfectly flat map (tests, tutorials).
    float relief = 1.0f;
    /// Tiles between the map border and the zoo's boundary wall.
    int margin = 6;
    /// Width of the entrance gate in tiles (in the south wall, centred).
    int entranceWidth = 4;
};

/// The terrain and everything that lives on the tile grid: heights (one per grid
/// vertex), ground cover, paths, water and fences.
///
/// Coordinates: tile (x, z) covers the square [x, x+1) x [z, z+1) times kTileSize
/// on the ground plane; the map's corner is the origin; north is -z. Height
/// vertex (vx, vz) sits at (vx, vz) * kTileSize, 0 <= vx <= Width().
///
/// A tile is rendered and walked as two triangles split along the diagonal from
/// its (x, z) corner to its (x+1, z+1) corner; HeightAt() interpolates exactly
/// on those triangles so that feet, fences and the mesh agree.
class World {
public:
    /// A flat map of the given size at height 0, all grass, no boundary wall.
    World(int width, int depth);

    /// A generated map: gentle seeded relief, the boundary wall (kPerimeterFence
    /// edges around the property) with an entrance gate in the middle of the
    /// south wall, an approach road outside it and a small paved entrance plaza
    /// inside. Deterministic for equal parameters.
    static World Generate(const WorldGenParams& params);

    int Width() const { return width_; }
    int Depth() const { return depth_; }
    float SizeX() const { return static_cast<float>(width_) * kTileSize; }
    float SizeZ() const { return static_cast<float>(depth_) * kTileSize; }

    bool InBounds(TileCoord t) const { return t.x >= 0 && t.z >= 0 && t.x < width_ && t.z < depth_; }
    /// True for tiles inside the boundary wall (where the player may build). On a
    /// map without a property rectangle every in-bounds tile is property.
    bool IsProperty(TileCoord t) const;
    /// Inclusive tile rectangle of the property.
    TileCoord PropertyMin() const { return propertyMin_; }
    TileCoord PropertyMax() const { return propertyMax_; }
    /// First property tile behind the middle of the entrance gate (visitors appear
    /// on the approach road and walk in through here), and the tile outside it.
    TileCoord EntranceTile() const { return entrance_; }
    TileCoord ApproachTile() const { return {entrance_.x, entrance_.z + 1}; }
    /// Where visitors enter and leave the map (the far end of the approach road).
    Vec2 SpawnPoint() const;

    // ---- tiles ------------------------------------------------------------------------
    /// `t` must be in bounds.
    const Tile& At(TileCoord t) const { return tiles_[Index(t)]; }
    void SetTerrain(TileCoord t, Terrain terrain);
    void SetPath(TileCoord t, PathKind path);
    void SetBuilding(TileCoord t, BuildingId building);
    /// Puts water on a tile with its surface at `level` and digs the tile's four
    /// corner vertices down to at least `depth` metres below the surface.
    void SetWater(TileCoord t, float level, float depth = 0.8f);
    /// Removes the water; the dug-out ground stays as it is.
    void ClearWater(TileCoord t);
    bool HasWater(TileCoord t) const { return InBounds(t) && At(t).waterLevel != kNoWater; }
    /// Depth of the water above the ground at a point, 0 where it is dry.
    float WaterDepthAt(Vec2 p) const;

    static TileCoord TileAt(Vec2 p) {
        return {static_cast<int>(std::floor(p.x / kTileSize)), static_cast<int>(std::floor(p.z / kTileSize))};
    }
    static Vec2 TileCenter(TileCoord t) {
        return {(static_cast<float>(t.x) + 0.5f) * kTileSize, (static_cast<float>(t.z) + 0.5f) * kTileSize};
    }

    // ---- heights ----------------------------------------------------------------------
    /// Vertex coordinates are clamped to the grid.
    float VertexHeight(int vx, int vz) const;
    void SetVertexHeight(int vx, int vz, float height);
    /// Ground height at a point (clamped to the map), see the class comment.
    float HeightAt(Vec2 p) const;
    /// Unit normal of the ground triangle under a point (y > 0).
    Vec3 NormalAt(Vec2 p) const;
    /// Mean of the four corner heights.
    float TileHeight(TileCoord t) const;
    /// Largest height difference between the tile's corners.
    float TileRelief(TileCoord t) const;

    /// Terraforming brushes; `center` and `radius` in metres, smooth falloff to the
    /// rim. Heights stay within [kMinHeight, kMaxHeight]. Vertices of the outermost
    /// map border ring and vertices under buildings are left alone.
    void RaiseBrush(Vec2 center, float radius, float amount);
    void FlattenBrush(Vec2 center, float radius, float targetHeight, float strength);
    void SmoothBrush(Vec2 center, float radius, float strength);
    /// Sets every corner of the tiles in the inclusive rectangle to `height`.
    void FlattenTiles(TileCoord min, TileCoord max, float height);

    static constexpr float kMinHeight = -12.0f;
    static constexpr float kMaxHeight = 40.0f;

    // ---- fences -----------------------------------------------------------------------
    bool EdgeInBounds(EdgeRef e) const;
    /// A default (non-existing) fence for an edge without one or out of bounds.
    Fence FenceAt(EdgeRef e) const;
    /// `e` must be in bounds. A default Fence removes the fence.
    void SetFence(EdgeRef e, Fence fence);
    Fence FenceOn(TileCoord t, Dir side) const { return FenceAt(EdgeOf(t, side)); }

    // ---- queries ----------------------------------------------------------------------
    /// First hit of a ray with the ground (not the water), within `maxDistance`;
    /// accurate to about a centimetre. `direction` need not be normalised.
    std::optional<Vec3> Raycast(Vec3 origin, Vec3 direction, float maxDistance) const;

    // ---- change tracking (for renderers and caches) ----------------------------------
    /// Chunk (cx, cz) covers tiles [cx*kChunkTiles, (cx+1)*kChunkTiles). The revision
    /// of a chunk changes whenever anything that affects its appearance changes:
    /// heights of its vertices (including the shared border vertices), terrain,
    /// paths or water of its tiles or of tiles directly adjacent to it.
    int ChunksX() const { return (width_ + kChunkTiles - 1) / kChunkTiles; }
    int ChunksZ() const { return (depth_ + kChunkTiles - 1) / kChunkTiles; }
    std::uint32_t ChunkRevision(int cx, int cz) const;
    /// Changes with every fence edit.
    std::uint32_t FenceRevision() const { return fenceRevision_; }
    /// Changes with every edit that can affect walkability: paths, fences, water,
    /// buildings, heights.
    std::uint32_t NavRevision() const { return navRevision_; }
    /// Changes with every edit of any kind (heights, tiles, fences).
    std::uint32_t Revision() const { return revision_; }

    bool operator==(const World& other) const;

private:
    friend struct WorldAccess;  // serialisation (WorldJson.cpp) and the generator

    std::size_t Index(TileCoord t) const {
        return static_cast<std::size_t>(t.z) * static_cast<std::size_t>(width_) +
               static_cast<std::size_t>(t.x);
    }
    void TouchTile(TileCoord t);
    void TouchVertex(int vx, int vz);

    int width_ = 0;
    int depth_ = 0;
    TileCoord propertyMin_{};
    TileCoord propertyMax_{};
    TileCoord entrance_{};
    std::vector<Tile> tiles_;              // width * depth
    std::vector<float> heights_;           // (width + 1) * (depth + 1)
    std::vector<Fence> fencesHorizontal_;  // width * (depth + 1)
    std::vector<Fence> fencesVertical_;    // (width + 1) * depth
    std::vector<std::uint32_t> chunkRevisions_;
    std::uint32_t fenceRevision_ = 1;
    std::uint32_t navRevision_ = 1;
    std::uint32_t revision_ = 1;
};

}  // namespace zoo
