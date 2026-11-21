// SPDX-License-Identifier: MIT
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "zoo/base/Ids.hpp"
#include "zoo/base/Math.hpp"
#include "zoo/build/Catalog.hpp"
#include "zoo/build/Objects.hpp"
#include "zoo/world/World.hpp"

namespace zoo {

/// What part of the map a tile belongs to.
enum class Region : std::uint8_t {
    Outside,    ///< beyond the boundary wall (the approach road, the surrounding land)
    Public,     ///< property ground visitors could reach from the entrance if it were paved
    Enclosure,  ///< property ground closed off by fences
    StandOff,   ///< explicitly reserved planting, excluded from public ground and habitats
};

/// Depth of water (at a tile's centre) from which a pool counts as deep enough to swim in.
inline constexpr float kDeepWaterM = 0.9f;

/// What an enclosure offers its animals; everything here is derived from the world,
/// the placed objects and the catalog, never saved.
struct EnclosureStats {
    int tiles = 0;
    float areaM2 = 0.0f;  ///< tiles * kTileSize^2
    /// Share of the enclosure's tiles (water tiles included in the denominator, but
    /// counted under `waterShare`, not under their ground cover) per ground cover.
    std::array<float, kTerrainCount> terrainShare{};
    float waterShare = 0.0f;    ///< tiles carrying water / tiles
    bool deepWater = false;     ///< some tile has >= kDeepWaterM of water
    float vegetationM2 = 0.0f;  ///< sum of PropDef::vegetationM2 of its props
    /// min(1, vegetationM2 / dry area); 0 for an enclosure without dry ground.
    float vegetationDensity = 0.0f;
    float shadeM2 = 0.0f;     ///< sum of PropDef::shadeM2
    int shelterCapacity = 0;  ///< sum over its open AnimalShelter buildings
    bool heatedShelter = false;
    bool cooledShelter = false;
    int feeders = 0;
    int troughs = 0;
    /// Sum of PropDef::enrichment of its props (toys count fully, trees and rocks a little).
    float enrichment = 0.0f;

    // -- the barrier: every edge between a tile of the enclosure and a tile outside it --
    int boundaryEdges = 0;
    int gates = 0;
    /// The lowest, weakest and most worn stretch (boundary wall edges count with
    /// kPerimeterHeightM / kPerimeterStrength and their own condition). An
    /// enclosure always has a boundary, so these are meaningful whenever tiles > 0.
    float minFenceHeightM = 0.0f;
    int minFenceStrength = 0;
    float minFenceCondition = 1.0f;

    // -- how visitors see in --
    /// Public path tiles with direct or straight-across-stand-off views of positive visibility.
    int viewingTiles = 0;
    /// Mean ViewingSpot::visibility; 0 without any viewing spots.
    float visibility = 0.0f;

    bool operator==(const EnclosureStats&) const = default;
};

/// A place from which visitors watch an enclosure.
struct ViewingSpot {
    TileCoord tile;           ///< the public path tile
    Dir side = Dir::North;    ///< the side of `tile` the enclosure is on
    float visibility = 0.0f;  ///< containment visibility times outer rail visibility across a strip
    float standOffM = 0.0f;   ///< path tile edge to containment barrier; 0 for a direct view

    bool operator==(const ViewingSpot&) const = default;
};

struct Enclosure {
    EnclosureId id;
    /// The player's name for it ("Lion savanna"); generated as "Enclosure <id>" at first.
    std::string name;
    /// Its first tile in row-major order (smallest z, then smallest x). Identity is
    /// carried across rebuilds through this tile, see Enclosures::Rebuild().
    TileCoord anchor;
    TileCoord min;  ///< inclusive bounding rectangle
    TileCoord max;
    EnclosureStats stats;
    bool walkThrough = false;  ///< at least one boundary gate admits visitors
    /// Sorted by (z, x, side); at most one entry per (tile, side).
    std::vector<ViewingSpot> viewing;
    /// Its gate edges, sorted by (horizontal, z, x): where keepers enter.
    std::vector<EdgeRef> gates;
    /// The boundary edge animals are most likely to break out through: the one with
    /// the lowest (height, strength, condition) in that lexicographic order, ties by
    /// (horizontal, z, x).
    EdgeRef weakestEdge;
    /// Ids ascending.
    std::vector<BuildingId> shelters;  ///< AnimalShelter buildings whose door tile lies in it
    std::vector<PropId> feeders;
    std::vector<PropId> troughs;
    std::vector<PropId> enrichments;  ///< props with role Enrichment

    bool operator==(const Enclosure&) const = default;
};

/// Divides the map into the outside, the public area and the enclosures, and
/// describes each enclosure.
///
/// Regions are the connected components of the tile grid where two edge-adjacent
/// tiles are connected when the edge between them carries no fence at all (a gate
/// is a fence: it separates). Building footprints and water do not separate.
/// Explicit stand-off tiles form separate components classified as StandOff, regardless
/// of fences. They neither connect other regions nor receive enclosure identities.
///  - Every component containing a tile outside the property is Outside.
///  - The component of World::EntranceTile() is Public (unless it is Outside: a
///    map whose entrance is not closed by the boundary wall's gate has no public area).
///  - Every other component is an Enclosure.
///
/// Identity: Rebuild() matches the new components with the previous enclosures by
/// anchor tile. Previous enclosures are visited in ascending id; each claims the
/// component that now contains its old anchor if that component is an Enclosure
/// and is still unclaimed, keeping its id and name. Components left unclaimed get
/// fresh ids (ascending in row-major order of their anchors); previous enclosures
/// that claimed nothing cease to exist. Ids are never reused.
///
/// Only ids, names and anchors are simulation state (saved with the game);
/// everything else is recomputed by Rebuild(), which the Builder calls after
/// every successful edit.
class Enclosures {
public:
    /// Recomputes regions, enclosures and their statistics from scratch. A full
    /// rebuild of a 128 x 128 map with a few thousand props must stay well under a
    /// millisecond or two: it runs after every construction step, also while the
    /// player drags a fence.
    void Rebuild(const World& world, const ZooObjects& objects, const Catalog& catalog);

    /// Region of a tile; Outside for tiles out of bounds or before the first Rebuild().
    Region RegionAt(TileCoord t) const;
    /// The enclosure a tile belongs to; the null id unless RegionAt(t) == Enclosure.
    EnclosureId At(TileCoord t) const;
    EnclosureId At(Vec2 p) const { return At(World::TileAt(p)); }

    /// Ascending id.
    const std::vector<Enclosure>& All() const { return enclosures_; }
    /// nullptr for an unknown (or the null) id.
    const Enclosure* Find(EnclosureId id) const;
    /// False for an unknown id or an empty name (names are trimmed to 40 characters).
    bool Rename(EnclosureId id, std::string name);

    /// Changes whenever a Rebuild() changed the set of regions (any tile's region or
    /// enclosure id), not when only statistics changed.
    std::uint32_t Revision() const { return revision_; }

    /// Compares the saved state: ids, names, anchors and the id counter.
    bool operator==(const Enclosures& other) const;

private:
    friend struct EnclosuresAccess;  // serialisation

    int width_ = 0;
    int depth_ = 0;
    /// Per tile: kOutsideCode, kPublicCode, or the value of the enclosure's id.
    std::vector<std::int64_t> codes_;
    std::vector<Enclosure> enclosures_;  // ascending id
    std::uint32_t nextId_ = 1;
    std::uint32_t revision_ = 1;
    // scratch buffers, kept to avoid allocating on every rebuild
    std::vector<std::int32_t> labels_;
    std::vector<std::int32_t> stack_;
    std::vector<std::int64_t> previousCodes_;

    static constexpr std::int64_t kOutsideCode = -2;
    static constexpr std::int64_t kPublicCode = -1;
    static constexpr std::int64_t kStandOffCode = -3;
};

}  // namespace zoo
