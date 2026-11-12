// SPDX-License-Identifier: MIT
#include "zoo/sim/Scenario.hpp"

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <string_view>

#include "zoo/base/Rng.hpp"

namespace zoo {

namespace {

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
    Rng& Random() { return rng_; }

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

constexpr float kQuarter = kPi * 0.5f;

}  // namespace

ScenarioReport BuildStarterZoo(Zoo& zoo) {
    Script s(zoo);
    const World& map = zoo.Map();
    const TileCoord entrance = map.EntranceTile();
    // The layout reaches 30 tiles to each side of the gate and 50 into the zoo.
    if (entrance.x - map.PropertyMin().x < 32 || map.PropertyMax().x - entrance.x < 33 ||
        entrance.z - map.PropertyMin().z < 52) {
        s.Problem("the map is too small for the starter zoo");
        return s.Report();
    }

    // ---- ground and paths ---------------------------------------------------------------
    // The built-up part is evened out first: gentle slopes remain, steps do not.
    s.Level(-30, 6, 31, 48, 0.8f);

    s.Path(-1, 6, 2, 46, PathKind::Asphalt);     // the avenue north from the plaza
    s.Path(-28, 23, 29, 24, PathKind::Asphalt);  // the cross walk
    s.Path(-28, 25, -27, 44, PathKind::Gravel);  // west side
    s.Path(28, 25, 29, 44, PathKind::Gravel);    // east side
    s.Path(-28, 45, 29, 46, PathKind::Gravel);   // north walk, closing the round
    s.Path(-28, 6, -27, 22, PathKind::Gravel);   // down the outside of the lions
    s.Path(28, 6, 29, 22, PathKind::Gravel);     // and of the zebras
    s.Path(-26, 6, -2, 7, PathKind::Gravel);     // along the south fences back to the plaza
    s.Path(3, 6, 27, 7, PathKind::Gravel);

    // ---- lions: dry savanna south-west ---------------------------------------------------
    {
        const int e0 = -26, e1 = -2, n0 = 8, n1 = 22;
        s.Paint(e0, n0, e1, n1, Terrain::Savanna);
        s.Patches(e0, n0, e1, n1, Terrain::Dirt, 6, 2);
        s.Patches(e0, n0, e1, n1, Terrain::Rock, 2, 1);
        const EnclosureId id = s.Enclose(e0, n0, e1, n1, "steel_bars", "Lion Savanna");
        s.FenceSide(e1, n0 + 2, e1, n1 - 2, Dir::East, "glass_wall");   // a glass front on the avenue
        s.FenceSide(e0 + 6, n1, e1 - 6, n1, Dir::North, "glass_wall");  // and on the cross walk
        s.FenceSide(e0 + 2, n0, e0 + 2, n0, Dir::South, "steel_bars", true);
        s.Pond(-22, 10, -15, 14, 0.5f);
        s.Building("medium_shelter", -8, 16, Dir::South);
        s.Prop("meat_platform", s.Point(-12, 12), 0.3f);
        s.Prop("meat_platform", s.Point(-19, 18), 1.2f);
        s.Prop("water_trough", s.Point(-6, 11), 0.0f);
        s.Prop("scratching_post", s.Point(-14, 17));
        s.Prop("ball", s.Point(-10, 10));
        s.Prop("log_pile", s.Point(-22, 19), 0.8f);
        s.Prop("fallen_log", s.Point(-16, 20), 0.4f);
        s.Prop("sandstone_rock", s.Point(-5, 20), 1.0f, 1.3f);
        s.Prop("sandstone_rock", s.Point(-7, 19), 2.1f, 1.0f);
        s.Prop("granite_boulder", s.Point(-24, 15), 0.5f, 1.2f);
        s.Plant(id, e0, n0, e1, n1, 0.29f, {"acacia", "acacia", "savanna_shrub", "savanna_shrub", "baobab"});
        s.Animals("lion", id, e0, n0, e1, n1, 1, 2);
    }

    // ---- plains zebras: grassland south-east ---------------------------------------------
    {
        const int e0 = 3, e1 = 27, n0 = 8, n1 = 22;
        s.Paint(e0, n0, e1, n1, Terrain::Savanna);
        s.Patches(e0, n0, e1, n1, Terrain::Grass, 7, 3);
        s.Patches(e0, n0, e1, n1, Terrain::Dirt, 3, 1);
        const EnclosureId id = s.Enclose(e0, n0, e1, n1, "cable_barrier", "Zebra Plain");
        s.FenceSide(e1 - 2, n0, e1 - 2, n0, Dir::South, "cable_barrier", true);
        s.Pond(17, 16, 23, 19, 0.45f);
        s.Building("large_shelter", 6, 17, Dir::South);
        s.Prop("hay_rack", s.Point(8, 11), 0.0f);
        s.Prop("hay_rack", s.Point(20, 11), 1.6f);
        s.Prop("pellet_fruit_trough", s.Point(14, 13), 0.4f);
        s.Prop("water_trough", s.Point(12, 20), kQuarter);
        s.Prop("log_pile", s.Point(24, 12), 0.3f);
        s.Prop("fallen_log", s.Point(15, 9), 1.3f);
        s.Prop("sandstone_rock", s.Point(25, 20), 0.2f, 1.1f);
        s.Plant(id, e0, n0, e1, n1, 0.28f, {"acacia", "acacia", "savanna_shrub", "savanna_shrub"});
        s.Animals("zebra", id, e0, n0, e1, n1, 1, 4);
    }

    // ---- grey wolves: woodland north-west ------------------------------------------------
    {
        const int e0 = -26, e1 = -2, n0 = 25, n1 = 44;
        s.Paint(e0, n0, e1, n1, Terrain::ForestFloor);
        s.Patches(e0, n0, e1, n1, Terrain::Grass, 6, 2);
        s.Patches(e0, n0, e1, n1, Terrain::Rock, 3, 1);
        const EnclosureId id = s.Enclose(e0, n0, e1, n1, "tall_mesh", "Wolf Wood");
        s.FenceSide(e1, n0 + 3, e1, n1 - 3, Dir::East, "glass_wall");
        s.FenceSide(e0 + 2, n0, e0 + 2, n0, Dir::South, "tall_mesh", true);
        s.Pond(-22, 36, -16, 40, 0.5f);
        s.Building("medium_shelter", -9, 38, Dir::South);
        s.Prop("meat_platform", s.Point(-12, 29), 0.5f);
        s.Prop("meat_platform", s.Point(-20, 31), 2.0f);
        s.Prop("water_trough", s.Point(-6, 28), 0.0f);
        s.Prop("log_pile", s.Point(-15, 33), 1.1f);
        s.Prop("fallen_log", s.Point(-8, 33), 0.7f);
        s.Prop("fallen_log", s.Point(-22, 27), 2.4f);
        s.Prop("ball", s.Point(-11, 41));
        s.Prop("granite_boulder", s.Point(-5, 42), 0.0f, 1.3f);
        s.Prop("granite_boulder", s.Point(-7, 41), 1.7f, 0.9f);
        s.Prop("granite_boulder", s.Point(-24, 34), 0.9f, 1.1f);
        s.Plant(id, e0, n0, e1, n1, 0.4f, {"spruce", "spruce", "pine", "birch", "juniper", "juniper"});
        s.Animals("wolf", id, e0, n0, e1, n1, 1, 3);
    }

    // ---- what visitors need --------------------------------------------------------------
    s.Building("gift_shop", 5, 1, Dir::West);    // east of the plaza
    s.Building("staff_room", -5, 1, Dir::East);  // west of it
    s.Building("burger_stall", 5, 25, Dir::South);
    s.Building("drinks_stall", 9, 25, Dir::South);
    s.Building("restroom", 13, 25, Dir::South);
    s.Building("ice_cream_stand", 3, 40, Dir::West);
    for (int e = 18; e <= 24; e += 3) {
        s.Prop("picnic_table", s.Point(e, 26, 0.0f, 0.4f), e % 2 == 0 ? 0.0f : kQuarter);
    }
    s.Prop("litter_bin", s.Point(16, 25, 0.9f, 0.0f));
    s.Prop("litter_bin", s.Point(8, 25, -0.9f, 0.0f));

    // Benches facing the glass fronts, lamps and bins along the avenue.
    for (int n = 10; n <= 44; n += 6) {
        if (n >= 22 && n <= 25) {
            continue;
        }
        s.Prop("lamp", s.Point(-1, n, -0.75f, 0.0f));
        s.Prop("lamp", s.Point(2, n + 3, 0.75f, 0.0f));
    }
    for (const int n : {12, 18, 30, 38}) {
        s.Prop("bench", s.Point(2, n, 0.55f, 0.0f), -kQuarter);  // on the east kerb, looking west
    }
    for (const int n : {15, 34}) {
        s.Prop("litter_bin", s.Point(2, n, 0.7f, 0.0f));
    }
    s.Prop("info_board", s.Point(-1, 15, -0.7f, 0.0f), kQuarter);
    s.Prop("info_board", s.Point(2, 14, 0.7f, 0.0f), -kQuarter);
    s.Prop("info_board", s.Point(-1, 33, -0.7f, 0.0f), kQuarter);
    s.Prop("bench", s.Point(-14, 23, 0.0f, -0.6f), 0.0f);
    s.Prop("bench", s.Point(14, 23, 0.0f, -0.6f), 0.0f);

    // The plaza: flower beds either side of the gate and a fountain where the avenue begins.
    s.Prop("flower_bed", s.Point(-3, 4), 0.0f);
    s.Prop("flower_bed", s.Point(4, 4), 0.0f);
    s.Prop("flower_bed", s.Point(-3, 2), 0.0f);
    s.Prop("flower_bed", s.Point(4, 2), 0.0f);

    // Trees on the lawns: the open north-east quarter, the margins and beyond the round walk.
    s.Park(3, 27, 27, 44, 26, {"oak", "birch", "birch", "willow", "juniper"});
    s.Park(-31, 5, 32, 50, 60, {"oak", "oak", "birch", "willow", "pine", "juniper"});
    s.Park(-45, 0, 46, 60, 90, {"oak", "birch", "pine", "spruce", "willow"});

    s.Provision();
    return s.Report();
}

}  // namespace zoo
