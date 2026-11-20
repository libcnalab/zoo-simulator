// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "zoo/base/Ids.hpp"
#include "zoo/base/Math.hpp"
#include "zoo/base/Money.hpp"
#include "zoo/build/Catalog.hpp"
#include "zoo/build/Enclosures.hpp"
#include "zoo/build/Objects.hpp"
#include "zoo/economy/Finance.hpp"
#include "zoo/world/World.hpp"

namespace zoo {

/// Why something cannot be built. Order is not saved anywhere; extend freely.
enum class BuildError : std::uint8_t {
    None,
    OutOfBounds,        ///< off the map
    NotProperty,        ///< outside the zoo's boundary wall
    Occupied,           ///< a building is in the way
    Blocked,            ///< a tree, rock or other object is in the way
    Water,              ///< water is in the way
    PathInTheWay,       ///< a path is in the way (water, blocking props)
    TooSteep,           ///< the ground is too uneven here
    NeedsEnclosure,     ///< this only makes sense inside an animal enclosure
    NeedsPublicGround,  ///< this must stand where visitors can reach it
    FenceInTheWay,      ///< a fence crosses the footprint or stands before the door
    FenceExists,        ///< the same fence is already there
    NoFence,            ///< there is no fence to remove or to put a gate in
    Perimeter,          ///< the boundary wall cannot be changed
    ThroughBuilding,    ///< a fence cannot run through a building
    UnknownItem,        ///< no such catalog entry, building or prop
    NothingToDo,        ///< everything asked for is already so
    NoMoney,            ///< the zoo cannot afford it
};

/// A sentence for the player: "A building is in the way.", "Not enough money.", ...
/// Empty for None.
std::string_view Describe(BuildError e);

/// What an operation would do (Plan...) or did (the mutating twin).
///
/// Operations on many tiles or edges do what they can: elements that cannot be
/// built are skipped and counted. `error` is None when at least one element is
/// (or was) built; otherwise it is the reason the first element failed, or
/// NoMoney when the elements are fine but the total is not affordable (all or
/// nothing: an operation is never carried out partially for lack of money).
struct BuildPlan {
    BuildError error = BuildError::None;
    /// What the zoo pays, in cents; negative when the operation pays money back.
    Money cost = 0;
    int count = 0;    ///< elements built / changed / removed
    int skipped = 0;  ///< elements left out
    /// The reason the first skipped element was left out (None when nothing was skipped).
    BuildError skippedBecause = BuildError::None;
    /// Set by PlaceBuilding / PlaceProp.
    BuildingId building;
    PropId prop;

    constexpr bool Ok() const { return error == BuildError::None; }
};

// ---- geometry helpers for drag gestures ---------------------------------------------

/// The tile edges on the way from grid vertex `a` to grid vertex `b` (vertex
/// coordinates as in World::VertexHeight): first along x at z = a.z, then along z
/// at x = b.x. Empty when a == b. In walking order, no duplicates.
std::vector<EdgeRef> EdgesAlong(TileCoord a, TileCoord b);
/// The outline of the inclusive tile rectangle spanned by two corner tiles (any
/// two opposite corners): north side west to east, east side, south side, west side.
std::vector<EdgeRef> EdgesAround(TileCoord cornerA, TileCoord cornerB);
/// The tiles from `a` to `b`: along x at z = a.z, then along z at x = b.x; both ends included.
std::vector<TileCoord> TilesInLine(TileCoord a, TileCoord b);
/// Every tile of the inclusive rectangle spanned by two corner tiles, row-major.
std::vector<TileCoord> TilesInRect(TileCoord cornerA, TileCoord cornerB);

/// Performs and prices everything the player builds, and is the only code that
/// edits the World and ZooObjects of a running zoo (besides wear and repair).
///
/// Every operation has a const Plan...() twin that changes nothing and returns
/// exactly what the mutating call would return in the same state: the UI shows
/// the plan's cost and error while the player aims. The mutating calls
///  1. do nothing at all unless the plan is Ok();
///  2. book the cost: a positive cost through Finance::TrySpend() in account
///     Construction (fences, paths, buildings, props that are not plants or rocks)
///     or Landscaping (terrain paint, water, terraforming, trees, bushes, rocks);
///     money back through Finance::Earn(Account::Refunds, ...). A net cost of an
///     operation that both charges and refunds (replacing a fence) is booked as
///     its sign says. Without a Finance (nullptr: sandbox mode, tests) everything
///     is free and NoMoney never occurs, but costs are still reported;
///  3. call Enclosures::Rebuild() once.
///
/// Rules shared by all operations: everything must be inside the property
/// (World::IsProperty); nothing touches the boundary wall.
class Builder {
public:
    // ---- prices that are not in the catalog ------------------------------------------
    static constexpr float kFenceRefund = 0.5f;              ///< times cost (+ gate cost) times condition
    static constexpr float kBuildingRefund = 0.4f;           ///< times cost times condition
    static constexpr float kPropRefund = 0.3f;               ///< times cost
    static constexpr Money kWaterCostPerTile = Dollars(40);  ///< at 0.8 m; scales linearly with depth
    static constexpr Money kWaterRemovalPerTile = Dollars(10);
    /// Terraforming: per square metre of brush area and metre of amount (raise /
    /// lower), or per square metre and unit of strength (flatten, smooth).
    static constexpr Money kTerraformPerM3 = 150;
    static constexpr Money kLevelPerM2 = 60;
    static constexpr Money kStandOffPerTile = Dollars(10);
    // ---- limits ---------------------------------------------------------------------
    static constexpr float kMaxPathRelief = 1.2f;  ///< World::TileRelief of a path tile
    /// Height range of the vertices under a new building; within it the footprint
    /// is levelled to its mean height when the building is placed.
    static constexpr float kMaxBuildingRelief = 2.0f;
    static constexpr float kMinWaterDepth = 0.2f;
    static constexpr float kMaxWaterDepth = 3.0f;
    static constexpr float kMinBrushRadius = 1.0f;
    static constexpr float kMaxBrushRadius = 16.0f;
    static constexpr float kMinPropScale = 0.7f;
    static constexpr float kMaxPropScale = 1.3f;

    /// All references must outlive the builder; `finance` may be null.
    Builder(World& world, ZooObjects& objects, Enclosures& enclosures, const Catalog& catalog,
            Finance* finance = nullptr);

    /// For placing many props, painting ground or terraforming in a row (a scenario's planting):
    /// between Hold() and Release() those operations leave the enclosures' figures
    /// (Enclosure::stats) as they were, and Release() brings them up to date in one
    /// rebuild instead of one per operation. Regions never go stale: props and ground cover
    /// do not change them, and every other operation still rebuilds at once.
    void Hold() { held_ = true; }
    void Release();

    // ---- fences ---------------------------------------------------------------------
    /// What stops a fence of catalog kind `kind` (FenceDef::kind) on one edge:
    /// OutOfBounds; NotProperty unless both tiles beside the edge are property;
    /// Perimeter; ThroughBuilding when both tiles belong to the same building;
    /// UnknownItem for a kind not in the catalog; FenceExists when the edge already
    /// carries exactly this kind with this gate flag. An existing fence of another
    /// kind or gate flag is replaced.
    BuildError CheckFence(EdgeRef edge, int kind, bool gate) const;
    /// Builds (or replaces) the fence on every edge that passes CheckFence;
    /// duplicates in `edges` count once. Cost per edge: FenceDef::cost, plus
    /// gateCost with `gate`, minus the refund for a replaced fence (see RemoveFence);
    /// turning an existing fence of the same kind into a gate costs only gateCost,
    /// turning a gate back into fence costs nothing. New fences have condition 1; a
    /// same-kind gate toggle keeps the condition.
    BuildPlan PlanFence(std::span<const EdgeRef> edges, int kind, bool gate = false) const;
    BuildPlan PlaceFence(std::span<const EdgeRef> edges, int kind, bool gate = false);
    /// Removes the player's fences on the edges (NoFence, Perimeter otherwise) and
    /// pays back kFenceRefund * (cost + gateCost if a gate) * condition, rounded
    /// down to a cent, per edge.
    BuildPlan PlanRemoveFence(std::span<const EdgeRef> edges) const;
    BuildPlan RemoveFence(std::span<const EdgeRef> edges);

    // ---- paths ----------------------------------------------------------------------
    /// What stops a path of `kind` on a tile: OutOfBounds, NotProperty, Occupied
    /// (building), Water, TooSteep (relief above kMaxPathRelief), Blocked (the circle
    /// of a prop with blocksWalking overlaps the tile, or a stand-off reservation), NothingToDo when the tile
    /// already has this kind. For PathKind::None (removal) only OutOfBounds,
    /// NotProperty and NothingToDo apply.
    BuildError CheckPath(TileCoord tile, PathKind kind) const;
    /// Lays the path on every tile that passes (replacing another kind at full
    /// price), PathDef::cost per tile. Removal (PathKind::None) is free and pays
    /// nothing back.
    BuildPlan PlanPath(std::span<const TileCoord> tiles, PathKind kind) const;
    BuildPlan PlacePath(std::span<const TileCoord> tiles, PathKind kind);

    /// Reserves dry public ground for planting. Rejects paths, buildings, the entrance,
    /// any enclosure (so no animal can lose its ground), and functional props. Scenery
    /// is allowed. Removal clears only the reservation, freely, as path removal does;
    /// fences and scenery are separate operations. Duplicate tiles count once.
    BuildError CheckStandOff(TileCoord tile, bool marked = true) const;
    BuildPlan PlanStandOff(std::span<const TileCoord> tiles, bool marked = true) const;
    BuildPlan PlaceStandOff(std::span<const TileCoord> tiles, bool marked = true);

    // ---- ground cover ---------------------------------------------------------------
    /// Repaints the tiles (OutOfBounds, NotProperty; tiles that already have this
    /// cover are skipped with NothingToDo); TerrainDef::cost per repainted tile.
    BuildPlan PlanTerrain(std::span<const TileCoord> tiles, Terrain terrain) const;
    BuildPlan PaintTerrain(std::span<const TileCoord> tiles, Terrain terrain);

    // ---- water ----------------------------------------------------------------------
    /// What stops water on a tile: OutOfBounds, NotProperty, Occupied, PathInTheWay,
    /// Blocked (a prop's centre lies on the tile, or a blocking prop's circle
    /// overlaps it, or a stand-off reservation), NothingToDo when it already has water.
    BuildError CheckWater(TileCoord tile) const;
    /// Floods the tiles that pass. The surface level is that of the lowest water
    /// already on one of the tiles or on a tile edge-adjacent to one of them, so a
    /// pond grows at its own level; without such water it is 5 cm below the lowest
    /// corner of the passing tiles. Each tile is dug to `depth` (clamped to
    /// kMinWaterDepth..kMaxWaterDepth) below the surface (World::SetWater). Cost per
    /// tile: kWaterCostPerTile * depth / 0.8, rounded to a cent.
    BuildPlan PlanWater(std::span<const TileCoord> tiles, float depth) const;
    BuildPlan PlaceWater(std::span<const TileCoord> tiles, float depth);
    /// Drains the tiles that have water (NothingToDo otherwise), kWaterRemovalPerTile
    /// each; the dug-out ground stays.
    BuildPlan PlanRemoveWater(std::span<const TileCoord> tiles) const;
    BuildPlan RemoveWater(std::span<const TileCoord> tiles);

    // ---- terraforming ---------------------------------------------------------------
    enum class Brush : std::uint8_t {
        Raise,    ///< `amount` metres at the centre, negative lowers
        Flatten,  ///< towards the ground height at the centre, `amount` = strength 0..1
        Smooth,   ///< `amount` = strength 0..1
    };
    /// One stroke of a brush (the UI repeats strokes while the button is held).
    /// The centre must be on property ground (OutOfBounds, NotProperty); the radius
    /// is clamped to kMinBrushRadius..kMaxBrushRadius; an amount of 0 is NothingToDo.
    /// Cost: Raise: kTerraformPerM3 * pi r^2 * |amount| / 3; Flatten and Smooth:
    /// kLevelPerM2 * pi r^2 * amount; rounded up to a cent. The stroke uses the
    /// World brushes (which leave buildings alone); afterwards every water tile in
    /// reach whose four corners all lie at or above its water level is drained.
    BuildPlan PlanTerraform(Brush brush, Vec2 center, float radius, float amount) const;
    BuildPlan Terraform(Brush brush, Vec2 center, float radius, float amount);

    // ---- buildings ------------------------------------------------------------------
    /// Checks a building of Catalog::buildings[def] with its footprint's north-west
    /// tile at `origin` (see FootprintSize, DoorTile, FrontTile), in this order:
    /// UnknownItem; OutOfBounds / NotProperty (footprint and front tile); Occupied
    /// (a footprint tile or the front tile belongs to a building); Water (the same
    /// tiles); Blocked (a prop's centre inside the footprint, or a blocking prop's
    /// circle overlapping it, or a stand-off tile in the footprint/front); FenceInTheWay (a fence on an edge
    /// between two footprint tiles, or between the door tile and the front tile); TooSteep (range of the
    /// footprint's vertex heights above kMaxBuildingRelief); NeedsEnclosure / NeedsPublicGround by
    /// BuildingDef::placement against the region of the front tile.
    BuildError CheckBuilding(int def, TileCoord origin, Dir facing) const;
    /// Places it: levels the footprint to the mean of its vertex heights, removes
    /// paths under it, marks the tiles (World::SetBuilding) and stores a Building
    /// with condition 1, open, itemPrice = BuildingDef::itemPrice. Cost BuildingDef::cost.
    BuildPlan PlanBuilding(int def, TileCoord origin, Dir facing) const;
    BuildPlan PlaceBuilding(int def, TileCoord origin, Dir facing);
    /// Removes a building (UnknownItem) and pays back kBuildingRefund * cost * condition.
    BuildPlan PlanDemolish(BuildingId id) const;
    BuildPlan Demolish(BuildingId id);

    // ---- props ----------------------------------------------------------------------
    /// Checks a prop of Catalog::props[def] centred at `position`: UnknownItem;
    /// OutOfBounds / NotProperty (the tile under the centre); Occupied; Water;
    /// PathInTheWay (a prop with blocksWalking whose circle overlaps a path tile);
    /// Blocked (closer to another prop than the sum of the radii when both block
    /// walking, otherwise closer than 0.3 m); NeedsEnclosure / NeedsPublicGround by
    /// PropDef::placement against the region of the tile under the centre. Stand-off
    /// tiles allow only trees, bushes, rocks and Decoration props, regardless of
    /// their placement tag; functional props are Blocked.
    BuildError CheckProp(int def, Vec2 position) const;
    /// Places it with `scale` clamped to kMinPropScale..kMaxPropScale and the
    /// heading wrapped; feeders, troughs and bins start empty. Cost PropDef::cost.
    BuildPlan PlanProp(int def, Vec2 position) const;
    BuildPlan PlaceProp(int def, Vec2 position, float heading = 0.0f, float scale = 1.0f);
    /// Removes a prop (UnknownItem) and pays back kPropRefund * cost.
    BuildPlan PlanRemoveProp(PropId id) const;
    BuildPlan RemoveProp(PropId id);

    // ---- picking --------------------------------------------------------------------
    /// The prop under a point on the ground: the one whose centre is nearest among
    /// those within radius + `tolerance`; the null id when there is none.
    PropId PropAt(Vec2 point, float tolerance = 0.3f) const;

    const Catalog& Definitions() const { return catalog_; }

private:
    BuildPlan DoFence(std::span<const EdgeRef> edges, int kind, bool gate, bool apply);
    BuildPlan DoRemoveFence(std::span<const EdgeRef> edges, bool apply);
    BuildPlan DoPath(std::span<const TileCoord> tiles, PathKind kind, bool apply);
    BuildPlan DoStandOff(std::span<const TileCoord> tiles, bool marked, bool apply);
    BuildPlan DoTerrain(std::span<const TileCoord> tiles, Terrain terrain, bool apply);
    BuildPlan DoWater(std::span<const TileCoord> tiles, float depth, bool apply);
    BuildPlan DoRemoveWater(std::span<const TileCoord> tiles, bool apply);
    BuildPlan DoTerraform(Brush brush, Vec2 center, float radius, float amount, bool apply);
    BuildPlan DoBuilding(int def, TileCoord origin, Dir facing, bool apply);
    BuildPlan DoDemolish(BuildingId id, bool apply);
    BuildPlan DoProp(int def, Vec2 position, float heading, float scale, bool apply);
    BuildPlan DoRemoveProp(PropId id, bool apply);
    /// The enclosures' figures after an operation that cannot change a region.
    void RefreshFigures();

    World& world_;
    ZooObjects& objects_;
    Enclosures& enclosures_;
    const Catalog& catalog_;
    Finance* finance_ = nullptr;
    float largestPropRadius_ = 0.0f;
    bool held_ = false;
    bool stale_ = false;
};

}  // namespace zoo
