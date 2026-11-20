// SPDX-License-Identifier: MIT
#pragma once

// Internal construction helpers shared by the built-in scenarios.
#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <string_view>

#include "zoo/base/Rng.hpp"
#include "zoo/sim/Scenario.hpp"

namespace zoo::scenario {

/// Works in "entrance coordinates": `east` tiles east of the entrance gate's
/// centre tile and `north` tiles into the zoo from the gate.
class Script {
public:
    explicit Script(Zoo& zoo)
        : zoo_(zoo),
          build_(zoo.FreeBuilder()),
          origin_(zoo.Map().EntranceTile()),
          rng_(zoo.Config().seed, 0x5ce0a210) {}

    ScenarioReport& Report() { return report_; }
    /// See Builder::Hold(): for stretches that place much and read no habitat's figures.
    void Hold() { build_.Hold(); }
    void Release() { build_.Release(); }
    Rng& Random() { return rng_; }

    bool Terraform(Builder::Brush brush, Vec2 center, float radius, float amount) {
        const BuildPlan plan = build_.Terraform(brush, center, radius, amount);
        if (!plan.Ok()) {
            Problem("terraform: " + std::string(Describe(plan.error)));
            return false;
        }
        return true;
    }

    TileCoord Tile(int east, int north) const { return {origin_.x + east, origin_.z - north}; }
    /// Centre of a tile, plus an offset in metres (east, north).
    Vec2 Point(int east, int north, float dEast = 0.0f, float dNorth = 0.0f) const {
        const Vec2 c = World::TileCenter(Tile(east, north));
        return {c.x + dEast, c.z - dNorth};
    }

    void Problem(std::string text) { report_.problems.push_back(std::move(text)); }

    /// Evens the ground out over a rectangle of tiles (plus a margin) so that
    /// paths and buildings fit: a few passes of the smoothing brush.
    void Level(int east0, int north0, int east1, int north1, float strength = 1.0f) {
        const Vec2 a = Point(east0, north0);
        const Vec2 b = Point(east1, north1);
        const float step = 10.0f;
        for (int pass = 0; pass < 3; ++pass) {
            for (float z = std::min(a.z, b.z); z <= std::max(a.z, b.z) + 0.1f; z += step) {
                for (float x = std::min(a.x, b.x); x <= std::max(a.x, b.x) + 0.1f; x += step) {
                    build_.Terraform(Builder::Brush::Smooth, {x, z}, 12.0f, strength);
                }
            }
        }
    }

    void Path(int east0, int north0, int east1, int north1, PathKind kind) {
        const std::vector<TileCoord> tiles = TilesInRect(Tile(east0, north0), Tile(east1, north1));
        const BuildPlan plan = build_.PlacePath(tiles, kind);
        if (plan.skipped > 0 && plan.skippedBecause != BuildError::NothingToDo) {
            Problem("path: " + std::to_string(plan.skipped) + " tiles skipped (" +
                    std::string(Describe(plan.skippedBecause)) + ")");
        }
    }

    /// Points use entrance coordinates (x east, z north), widths are in tiles.
    void PathAlong(const std::vector<TileCoord>& points, int widthTiles, PathKind kind);
    void PathTiles(const std::vector<TileCoord>& tiles, PathKind kind);
    /// Rasterised ellipse in world tile coordinates, suitable for unions of regions.
    std::vector<TileCoord> Ellipse(int east, int north, float radiusEast, float radiusNorth) const;
    /// An elliptical arc in entrance coordinates, angles in radians from east towards north.
    static std::vector<TileCoord> Arc(int east, int north, float radiusEast, float radiusNorth, float start,
                                      float end);
    static std::vector<EdgeRef> Outline(const std::vector<TileCoord>& tiles);
    EnclosureId EncloseTiles(const std::vector<TileCoord>& tiles, std::string_view fence, std::string name);
    /// Swap only boundary edges with a public path immediately outside them.
    void ViewingFront(const std::vector<TileCoord>& tiles, std::string_view fence,
                      std::initializer_list<Dir> sides = {Dir::North, Dir::East, Dir::South, Dir::West});
    /// Square dilation preserves the minimum clearance even at raster corners.
    static std::vector<TileCoord> Expanded(const std::vector<TileCoord>& tiles, int depth);
    /// Reserves the band outside an existing habitat and encloses it in a public rail.
    /// Returns the marked tiles for deterministic planting after animals have been placed.
    std::vector<TileCoord> StandOffFront(const std::vector<TileCoord>& tiles, int depth,
                                         const std::vector<TileCoord>& extra = {});
    void KeeperGate(const std::vector<TileCoord>& tiles, std::string_view fence);

    void Paint(int east0, int north0, int east1, int north1, Terrain terrain) {
        build_.PaintTerrain(TilesInRect(Tile(east0, north0), Tile(east1, north1)), terrain);
    }

    /// Scatters round patches of another ground cover over a rectangle.
    void Patches(int east0, int north0, int east1, int north1, Terrain terrain, int count, int radius) {
        for (int i = 0; i < count; ++i) {
            const int ce = rng_.RangeInt(east0, east1);
            const int cn = rng_.RangeInt(north0, north1);
            const int r = rng_.RangeInt(1, radius);
            std::vector<TileCoord> tiles;
            for (int n = cn - r; n <= cn + r; ++n) {
                for (int e = ce - r; e <= ce + r; ++e) {
                    if (e < east0 || e > east1 || n < north0 || n > north1) {
                        continue;
                    }
                    if ((e - ce) * (e - ce) + (n - cn) * (n - cn) <= r * r) {
                        tiles.push_back(Tile(e, n));
                    }
                }
            }
            build_.PaintTerrain(tiles, terrain);
        }
    }

    int FenceKind(std::string_view id) const {
        const FenceDef* def = zoo_.Definitions().FindFence(id);
        return def != nullptr ? def->kind : 0;
    }

    /// Fences a rectangle of tiles; returns the enclosure it made.
    EnclosureId Enclose(int east0, int north0, int east1, int north1, std::string_view fence,
                        std::string name) {
        const int kind = FenceKind(fence);
        const BuildPlan plan = build_.PlaceFence(EdgesAround(Tile(east0, north0), Tile(east1, north1)), kind);
        if (!plan.Ok() || plan.skipped > 0) {
            Problem(name + ": fence incomplete (" +
                    std::string(Describe(plan.Ok() ? plan.skippedBecause : plan.error)) + ")");
        }
        const EnclosureId id = zoo_.Habitats().At(Tile((east0 + east1) / 2, (north0 + north1) / 2));
        if (!id) {
            Problem(name + ": no enclosure came of the fence");
            return {};
        }
        zoo_.RenameEnclosure(id, std::move(name));
        ++report_.enclosures;
        return id;
    }

    /// Replaces the fence on one side of a tile run (for a glass front or a gate).
    void FenceSide(int east0, int north0, int east1, int north1, Dir side, std::string_view fence,
                   bool gate = false) {
        std::vector<EdgeRef> edges;
        for (const TileCoord t : TilesInRect(Tile(east0, north0), Tile(east1, north1))) {
            edges.push_back(EdgeOf(t, side));
        }
        const BuildPlan plan = build_.PlaceFence(edges, FenceKind(fence), gate);
        if (!plan.Ok()) {
            Problem(std::string(fence) + ": " + std::string(Describe(plan.error)));
        }
    }

    void Pond(int east0, int north0, int east1, int north1, float depth) {
        const TileCoord a = Tile(east0, north0);
        const TileCoord b = Tile(east1, north1);
        // A pond needs level banks.
        const Vec2 centre = Point((east0 + east1) / 2, (north0 + north1) / 2);
        const float radius = std::min(
            16.0f, static_cast<float>(std::max(std::abs(east1 - east0), std::abs(north1 - north0))) + 3.0f);
        build_.Terraform(Builder::Brush::Flatten, centre, radius, 1.0f);
        std::vector<TileCoord> tiles;
        // Rounded corners.
        const int minX = std::min(a.x, b.x), maxX = std::max(a.x, b.x);
        const int minZ = std::min(a.z, b.z), maxZ = std::max(a.z, b.z);
        for (int z = minZ; z <= maxZ; ++z) {
            for (int x = minX; x <= maxX; ++x) {
                const bool cornerX = x == minX || x == maxX;
                const bool cornerZ = z == minZ || z == maxZ;
                if (cornerX && cornerZ && maxX - minX >= 2 && maxZ - minZ >= 2) {
                    continue;
                }
                tiles.push_back({x, z});
            }
        }
        const BuildPlan plan = build_.PlaceWater(tiles, depth);
        if (!plan.Ok()) {
            Problem("pond: " + std::string(Describe(plan.error)));
        }
    }

    BuildingId Building(std::string_view id, int east, int north, Dir facing) {
        const int def = zoo_.Definitions().BuildingIndex(id);
        if (def < 0) {
            Problem("unknown building " + std::string(id));
            return {};
        }
        const BuildingDef& info = zoo_.Definitions().buildings[static_cast<std::size_t>(def)];
        const TileCoord size = FootprintSize(info, facing);
        // (east, north) names the footprint's south-west tile.
        const TileCoord origin = Tile(east, north + size.z - 1);
        const Vec2 centre = {World::TileCenter(origin).x + static_cast<float>(size.x - 1),
                             World::TileCenter(origin).z + static_cast<float>(size.z - 1)};
        build_.Terraform(Builder::Brush::Flatten, centre, static_cast<float>(std::max(size.x, size.z)) + 3.0f,
                         1.0f);
        const BuildPlan plan = build_.PlaceBuilding(def, origin, facing);
        if (!plan.Ok()) {
            Problem(std::string(id) + ": " + std::string(Describe(plan.error)));
            return {};
        }
        ++report_.buildings;
        return plan.building;
    }

    PropId Prop(std::string_view id, Vec2 position, float heading = 0.0f, float scale = 1.0f,
                bool quiet = false) {
        const int def = zoo_.Definitions().PropIndex(id);
        if (def < 0) {
            Problem("unknown prop " + std::string(id));
            return {};
        }
        const BuildPlan plan = build_.PlaceProp(def, position, heading, scale);
        if (!plan.Ok()) {
            if (!quiet) {
                Problem(std::string(id) + ": " + std::string(Describe(plan.error)));
            }
            return {};
        }
        ++report_.props;
        return plan.prop;
    }

    /// Plants until the enclosure's vegetation reaches `density`, picking from
    /// `kinds` (weighted by repetition), away from the fence.
    void Plant(EnclosureId enclosure, int east0, int north0, int east1, int north1, float density,
               std::initializer_list<std::string_view> kinds) {
        for (int attempt = 0; attempt < 400; ++attempt) {
            const Enclosure* e = zoo_.Habitats().Find(enclosure);
            if (e == nullptr || e->stats.vegetationDensity >= density) {
                return;
            }
            const std::string_view kind =
                *(kinds.begin() + rng_.RangeInt(0, static_cast<int>(kinds.size()) - 1));
            const Vec2 at = Point(rng_.RangeInt(east0 + 1, east1 - 1), rng_.RangeInt(north0 + 1, north1 - 1),
                                  rng_.Range(-0.8f, 0.8f), rng_.Range(-0.8f, 0.8f));
            Prop(kind, at, rng_.Range(0.0f, kTwoPi), rng_.Range(0.8f, 1.25f), true);
        }
        Problem("planting: the vegetation target was not reached");
    }

    /// Trees and bushes on the public lawns: random spots that are free.
    void Park(int east0, int north0, int east1, int north1, int count,
              std::initializer_list<std::string_view> kinds) {
        for (int i = 0; i < count; ++i) {
            const int e = rng_.RangeInt(east0, east1);
            const int n = rng_.RangeInt(north0, north1);
            const std::string_view kind =
                *(kinds.begin() + rng_.RangeInt(0, static_cast<int>(kinds.size()) - 1));
            const float heading = rng_.Range(0.0f, kTwoPi);
            const float scale = rng_.Range(0.8f, 1.3f);
            const float de = rng_.Range(-0.7f, 0.7f);
            const float dn = rng_.Range(-0.7f, 0.7f);
            const TileCoord t = Tile(e, n);
            if (zoo_.Habitats().RegionAt(t) != Region::Public || zoo_.Map().At(t).path != PathKind::None) {
                continue;
            }
            // Keep the lawn beside paths open: no trunk within a tile of one.
            bool nearPath = false;
            for (int dz = -1; dz <= 1 && !nearPath; ++dz) {
                for (int dx = -1; dx <= 1; ++dx) {
                    const TileCoord o{t.x + dx, t.z + dz};
                    if (zoo_.Map().InBounds(o) && zoo_.Map().At(o).path != PathKind::None) {
                        nearPath = true;
                        break;
                    }
                }
            }
            if (!nearPath) {
                Prop(kind, Point(e, n, de, dn), heading, scale, true);
            }
        }
    }

    void Animals(std::string_view speciesId, EnclosureId enclosure, int east0, int north0, int east1,
                 int north1, int males, int females) {
        const int species = zoo_.SpeciesList().IndexOf(speciesId);
        if (species < 0) {
            Problem("unknown species " + std::string(speciesId));
            return;
        }
        const SpeciesDef& def = zoo_.SpeciesList().All()[static_cast<std::size_t>(species)];
        const Env env = zoo_.MakeEnv();
        for (int i = 0; i < males + females; ++i) {
            const Sex sex = i < males ? Sex::Male : Sex::Female;
            const float age = def.maturityYears * rng_.Range(1.2f, 2.2f) + 0.5f;
            AnimalId id;
            for (int attempt = 0; attempt < 40 && !id; ++attempt) {
                const Vec2 at =
                    Point(rng_.RangeInt(east0 + 1, east1 - 1), rng_.RangeInt(north0 + 1, north1 - 1));
                if (zoo_.Habitats().At(at) != enclosure || zoo_.Map().HasWater(World::TileAt(at))) {
                    continue;
                }
                id = zoo_.Fauna().Add(env, species, sex, age, at);
            }
            if (id) {
                ++report_.animals;
            } else {
                Problem("no room to set down a " + def.commonName);
            }
        }
    }

    /// Fills every feeder and trough, free of charge.
    void Provision() {
        Env env = zoo_.MakeEnv();
        env.finance = nullptr;
        std::vector<PropId> feeders;
        std::vector<PropId> troughs;
        for (const zoo::Prop& prop : zoo_.Objects().Props()) {
            const PropRole role = zoo_.Definitions().props[static_cast<std::size_t>(prop.def)].role;
            if (role == PropRole::Feeder) {
                feeders.push_back(prop.id);
            } else if (role == PropRole::WaterTrough) {
                troughs.push_back(prop.id);
            }
        }
        for (const PropId id : feeders) {
            zoo_.Fauna().RefillFeeder(env, id);
        }
        for (const PropId id : troughs) {
            zoo_.Fauna().RefillTrough(env, id);
        }
    }

private:
    Zoo& zoo_;
    Builder build_;
    TileCoord origin_;
    Rng rng_;
    ScenarioReport report_;
};

}  // namespace zoo::scenario
