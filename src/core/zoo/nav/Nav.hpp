// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "zoo/base/Math.hpp"
#include "zoo/world/World.hpp"

namespace zoo {

/// What a walker may walk on and through.
struct NavProfile {
    /// Walks only on tiles with a path (visitors).
    bool pathsOnly = false;
    /// May pass gates in ordinary fences (staff). Nobody passes a fence without a gate.
    bool useGates = false;
    /// May pass the gate of the boundary wall (kPerimeterFence with gate: the zoo entrance).
    bool useEntrance = false;
    /// May leave the property through the entrance (visitors and staff arriving and
    /// leaving). Without it tiles outside the property are impassable.
    bool leaveProperty = false;
    /// Deepest water it enters, in metres at the tile centre; 0 = stays dry.
    float maxWaterDepth = 0.0f;
    /// Never enters a tile that holds water, however shallow it is at the centre: a shore
    /// tile's centre may be above the waterline while its pool side is ankle-deep.
    bool keepOffWater = false;
    /// Steepest tile it walks (World::TileRelief, metres across one tile).
    float maxRelief = 1.5f;
    /// Cost factor of a tile without a path relative to a paved one (>= 1). Staff
    /// and visitors prefer paths; animals do not care (1).
    float offPathCost = 1.0f;
    /// Cost factor of a tile with water (>= 1): most animals go around a pool.
    float waterCost = 1.0f;

    constexpr bool operator==(const NavProfile&) const = default;

    /// Visitors: paths only, through the entrance, never into water.
    static constexpr NavProfile Visitor() {
        return {.pathsOnly = true, .useEntrance = true, .leaveProperty = true, .maxRelief = 1.2f};
    }
    /// Staff: anywhere dry (no tile that holds water: keepers do not wade through a
    /// lagoon's shelf or a pond's edge), through gates, preferring paths.
    static constexpr NavProfile Staff() {
        return {.useGates = true,
                .useEntrance = true,
                .leaveProperty = true,
                .keepOffWater = true,
                .offPathCost = 3.0f};
    }
    /// An animal: anywhere fences let it, wading up to `wadeDepth` (swimmers: any depth).
    static constexpr NavProfile Animal(float wadeDepth, bool canSwim) {
        return {.maxWaterDepth = canSwim ? 1.0e6f : wadeDepth,
                .maxRelief = 2.5f,
                .waterCost = canSwim ? 1.5f : 3.0f};
    }
};

/// A circular solid supplied for one route. The World owns permanent tile and edge
/// obstructions; callers use these for movable or freely placed scenery without
/// putting simulation objects into the generic navigator.
struct NavObstacle {
    Vec2 position;
    float radius = 0.0f;
};

/// Pathfinding on the tile grid of a World.
///
/// Walkers move between the centres of neighbouring tiles: the four axis
/// neighbours, and diagonally when both axis neighbours involved are passable
/// and none of the four edges around the shared corner is blocked (no corner
/// cutting past fence ends or buildings).
///
/// A tile is passable for a profile when it is in bounds, carries no building,
/// satisfies pathsOnly / leaveProperty (outside the property only path tiles are
/// ever passable: the approach road), its water depth at the centre is within
/// maxWaterDepth (keepOffWater: it holds no water at all) and its relief within maxRelief. An edge is
/// passable when it has no fence, or the fence is a gate the profile may use.
///
/// The navigator keeps scratch buffers and caches keyed by World::NavRevision();
/// it holds no simulation state (nothing to save) and its results are a pure
/// function of the world and the arguments.
class Navigator {
public:
    explicit Navigator(const World& world);

    bool Passable(TileCoord t, const NavProfile& profile) const;
    /// May the walker step from `from` to its neighbour on side `d` (both tiles passable, edge open)?
    bool CanStep(TileCoord from, Dir d, const NavProfile& profile) const;

    /// A* from the tile under `from` to the tile under `to`. On success fills `out`
    /// with the waypoints to walk in order, in metres: tile centres, simplified so
    /// that consecutive waypoints are joined by straight walkable segments (see
    /// Walkable()), not including the start position and ending with `to` itself.
    /// Returns false (and clears `out`) when there is no route, when either end is
    /// impassable, or when the search exceeds `maxExpansions` nodes.
    /// Step cost: distance in tiles (1 or sqrt 2) times the mean of the two tiles'
    /// cost factors. Ties are broken deterministically.
    bool FindPath(Vec2 from, Vec2 to, const NavProfile& profile, std::vector<Vec2>& out,
                  int maxExpansions = 60000, std::span<const NavObstacle> obstacles = {}) const;

    /// Can the walker go from `a` to `b` in a straight line: every tile the segment
    /// passes through is passable and it crosses no blocked edge, including when either
    /// endpoint lies exactly on that edge (a segment through a grid vertex counts as touching
    /// all four tiles around it).
    bool Walkable(Vec2 a, Vec2 b, const NavProfile& profile,
                  std::span<const NavObstacle> obstacles = {}) const;

    /// Length in metres of the route FindPath would return, or a negative value.
    float PathLength(Vec2 from, Vec2 to, const NavProfile& profile) const;

    /// The passable tile nearest to `around` (by straight tile distance, ties in
    /// row-major order) within `maxRadius` tiles, if any.
    std::optional<TileCoord> NearestPassable(TileCoord around, const NavProfile& profile,
                                             int maxRadius) const;

    /// Dijkstra from a set of goal tiles over the whole map: out[z * Width() + x] is
    /// the walking cost in tiles to the nearest goal, or a negative value where no
    /// goal is reachable. Used for "nearest restroom" style choices by many walkers.
    void DistanceField(std::span<const TileCoord> goals, const NavProfile& profile,
                       std::vector<float>& out) const;

    /// Connected component of each tile for a profile (cached per NavRevision):
    /// equal non-negative labels mean mutually reachable, -1 means impassable.
    int ComponentOf(TileCoord t, const NavProfile& profile) const;
    bool Reachable(TileCoord a, TileCoord b, const NavProfile& profile) const {
        const int ca = ComponentOf(a, profile);
        return ca >= 0 && ca == ComponentOf(b, profile);
    }

private:
    struct OpenNode {
        std::int32_t index = 0;
        float g = 0.0f;
        float h = 0.0f;
    };
    struct Scratch;
    struct ComponentCache {
        NavProfile profile;
        std::uint32_t revision = 0;
        std::vector<int> labels;
    };

    const World* world_;
    mutable std::vector<float> cost_;
    mutable std::vector<std::int32_t> parent_;
    mutable std::vector<std::uint32_t> stamp_;
    mutable std::vector<OpenNode> open_;
    mutable std::uint32_t generation_ = 0;
    mutable std::vector<ComponentCache> components_;
};

/// Advances a walker along a route: moves `position` towards waypoints[index]
/// by `distance` metres, stepping to following waypoints as they are reached
/// (within `arriveRadius`). Returns true when the last waypoint was reached.
/// `index` is advanced in place.
bool FollowPath(std::span<const Vec2> waypoints, std::size_t& index, Vec2& position, float distance,
                float arriveRadius = 0.05f);

}  // namespace zoo
