// SPDX-License-Identifier: MIT
#include "zoo/sim/VisitorZoo.hpp"

#include <algorithm>
#include <array>
#include <set>
#include <utility>

#include "zoo/sim/ScenarioScript.hpp"

namespace zoo {
namespace {
using scenario::Script;

Vec2 Direction(Dir side) {
    const TileCoord step = Step(side);
    return {static_cast<float>(step.x), static_cast<float>(step.z)};
}

struct HabitatPlan {
    std::string_view name;
    std::string_view species;
    int east;
    int north;
    int rx;
    int rn;
};

constexpr std::array<HabitatPlan, 14> kHabitats{{
    {"Meerkat Lookout", "meerkat", 10, 18, 5, 4},
    {"Flamingo Lagoon", "flamingo", 28, 24, 9, 7},
    {"Acacia Plain", "giraffe", 48, 53, 18, 19},
    {"Ostrich Rise", "ostrich", 13, 37, 9, 9},
    {"Lion Rock", "lion", 47, 91, 15, 12},
    {"Tiger Forest", "tiger", 50, 123, 12, 12},
    {"Wolf Wood", "wolf", 20, 124, 12, 12},
    {"Bear Falls", "brown_bear", -12, 124, 15, 12},
    {"Polar Sound", "polar_bear", -49, 115, 16, 14},
    {"Penguin Cove", "penguin", -49, 85, 12, 7},
    {"Elephant Valley", "elephant", -44, 52, 22, 20},
    {"Rhino Hollow", "rhino", -49, 16, 15, 9},
    {"Red Dune", "camel", -13, 83, 11, 12},
    {"Kangaroo Grove", "kangaroo", -11, 32, 10, 9},
}};

std::vector<TileCoord> RegionTiles(const Script& script, const HabitatPlan& plan) {
    // Two overlapping lobes keep long viewing fronts while avoiding straight rectangular pens.
    if (plan.rx <= 5) {
        return script.Ellipse(plan.east, plan.north, static_cast<float>(plan.rx),
                              static_cast<float>(plan.rn));
    }
    std::vector<TileCoord> tiles =
        script.Ellipse(plan.east - plan.rx / 5, plan.north, static_cast<float>(plan.rx - plan.rx / 5),
                       static_cast<float>(plan.rn));
    const auto lobe =
        script.Ellipse(plan.east + plan.rx / 3, plan.north - plan.rn / 5,
                       static_cast<float>(plan.rx - plan.rx / 3), static_cast<float>(plan.rn - plan.rn / 5));
    tiles.insert(tiles.end(), lobe.begin(), lobe.end());
    std::sort(tiles.begin(), tiles.end(),
              [](TileCoord a, TileCoord b) { return std::pair(a.z, a.x) < std::pair(b.z, b.x); });
    tiles.erase(std::unique(tiles.begin(), tiles.end()), tiles.end());
    return tiles;
}

void Walks(Zoo& zoo, Script& script) {
    // The plaza leaves a small planting island for its fountain; the two arms meet beyond it.
    script.Path(-10, 0, -3, 13, PathKind::Cobblestone);
    script.Path(3, 0, 10, 13, PathKind::Cobblestone);
    script.Path(-2, 0, 2, 4, PathKind::Cobblestone);
    script.Path(-2, 10, 2, 13, PathKind::Cobblestone);
    script.PathAlong(
        {{0, 11},   {12, 8},   {25, 12},  {40, 16},   {42, 30},   {27, 39},   {27, 68},  {28, 77},
         {29, 103}, {34, 109}, {34, 138}, {-12, 141}, {-29, 132}, {-29, 105}, {-29, 92}, {-32, 73},
         {-20, 70}, {-18, 49}, {-26, 37}, {-29, 29},  {-30, 12},  {-10, 8},   {0, 11}},
        3, PathKind::Asphalt);
    script.PathAlong({{-18, 49}, {-6, 46}, {0, 38}, {16, 36}, {27, 39}}, 2, PathKind::Gravel);
    script.PathAlong({{-20, 70}, {-3, 69}, {11, 74}, {28, 77}}, 2, PathKind::Gravel);
    script.PathAlong({{-29, 105}, {-8, 103}, {11, 106}, {29, 103}}, 2, PathKind::Gravel);
    script.PathAlong({{-18, 55}, {-7, 53}, {8, 55}, {27, 58}}, 2, PathKind::Gravel);
    script.PathAlong({{-43, 74}, {-37, 75}, {-31, 73}}, 2, PathKind::Gravel);
    script.PathAlong({{-13, 97}, {-10, 100}, {-8, 103}}, 2, PathKind::Gravel);
    script.PathAlong({{0, 105}, {-3, 108}, {-3, 111}}, 2, PathKind::Gravel);
    script.PathAlong({{10, 24}, {12, 26}, {17, 27}}, 2, PathKind::Gravel);
    script.PathAlong({{-37, 85}, {-29, 86}}, 2, PathKind::Boardwalk);
    script.PathAlong(Script::Arc(0, 7, 13.0f, 8.0f, 0.0f, kPi), 2, PathKind::Cobblestone);

    // Widened viewing walks follow the raster boundary itself, so every bay touches its fence.
    std::set<std::pair<int, int>> reserved;
    for (const HabitatPlan& plan : kHabitats) {
        for (const TileCoord tile : RegionTiles(script, plan)) {
            reserved.emplace(tile.z, tile.x);
        }
    }
    for (const HabitatPlan& plan : kHabitats) {
        std::vector<TileCoord> bay;
        for (const TileCoord tile : RegionTiles(script, plan)) {
            for (int dz = -2; dz <= 2; ++dz) {
                for (int dx = -2; dx <= 2; ++dx) {
                    const TileCoord other{tile.x + dx, tile.z + dz};
                    if (!reserved.contains({other.z, other.x})) {
                        bay.push_back(other);
                    }
                }
            }
        }
        // Eight-metre-deep bays widen the southern and inward-facing galleries.
        for (const TileCoord centre :
             {script.Tile(plan.east, plan.north - plan.rn),
              script.Tile(plan.east + (plan.east < 0 ? plan.rx : -plan.rx), plan.north)}) {
            for (const TileCoord tile :
                 TilesInRect({centre.x - 4, centre.z - 4}, {centre.x + 4, centre.z + 4})) {
                if (!reserved.contains({tile.z, tile.x})) {
                    bay.push_back(tile);
                }
            }
        }
        script.PathTiles(bay, plan.species == "flamingo" || plan.species == "penguin" ? PathKind::Boardwalk
                                                                                      : PathKind::Gravel);
    }
    // Clip the centre-line sketch against the habitat footprints. Their continuous
    // perimeter walks carry the visitor around any clipped corner.
    std::vector<TileCoord> interior;
    for (const auto& [z, x] : reserved) {
        interior.push_back({x, z});
    }
    zoo.FreeBuilder().PlacePath(interior, PathKind::None);
}
struct HabitatStyle {
    Terrain ground;
    std::string_view fence;
    std::string_view front;
    std::string_view shelter;
    std::string_view feeder;
    std::string_view tree;
    std::string_view bush;
    std::string_view rock;
    std::string_view toy;
    int males;
    int females;
};

HabitatStyle Style(std::string_view species) {
    if (species == "meerkat") {
        return {Terrain::Sand,
                "wire_mesh",
                "glass_wall",
                "small_shelter",
                "insect_feeder",
                "palm",
                "desert_scrub",
                "sandstone_rock",
                "termite_mound",
                1,
                5};
    }
    if (species == "flamingo") {
        return {Terrain::Mud, "wire_mesh", "glass_wall", "heated_barn", "fish_bucket_stand",
                // Not willows: their curtain hangs to the ground and was all one saw from the glass.
                // Acacias stand by the soda lakes too, and carry their crown above the eye.
                "acacia", "tropical_fern", "sandstone_rock", "ball", 3, 9};
    }
    if (species == "giraffe") {
        return {Terrain::Savanna,
                "tall_mesh",
                "glass_wall",
                "large_shelter",
                "giraffe_browse_feeder",
                "acacia",
                "savanna_shrub",
                "sandstone_rock",
                "termite_mound",
                1,
                3};
    }
    if (species == "ostrich") {
        return {Terrain::Savanna,
                "tall_mesh",
                "glass_wall",
                "medium_shelter",
                "pellet_fruit_trough",
                "acacia",
                "savanna_shrub",
                "sandstone_rock",
                "termite_mound",
                1,
                3};
    }
    if (species == "lion") {
        return {Terrain::Savanna,
                "steel_bars",
                "glass_wall",
                "medium_shelter",
                "meat_platform",
                "acacia",
                "savanna_shrub",
                "sandstone_rock",
                "scratching_post",
                1,
                2};
    }
    if (species == "tiger") {
        return {Terrain::ForestFloor,
                "steel_bars",
                "high_glass_wall",
                "medium_shelter",
                "meat_platform",
                "bamboo_clump",
                "tropical_fern",
                "granite_boulder",
                "scratching_post",
                1,
                1};
    }
    if (species == "wolf") {
        return {Terrain::ForestFloor,
                "tall_mesh",
                "glass_wall",
                "medium_shelter",
                "meat_platform",
                "spruce",
                "juniper",
                "granite_boulder",
                "log_pile",
                1,
                4};
    }
    if (species == "brown_bear") {
        return {Terrain::ForestFloor,
                "steel_bars",
                "high_glass_wall",
                "medium_shelter",
                "meat_platform",
                "pine",
                "juniper",
                "granite_boulder",
                "climbing_frame",
                1,
                1};
    }
    if (species == "polar_bear") {
        return {Terrain::Snow,
                "steel_bars",
                "high_glass_wall",
                "cooled_den",
                "fish_bucket_stand",
                "pine",
                "juniper",
                "ice_rock",
                "ice_block",
                1,
                1};
    }
    if (species == "penguin") {
        return {Terrain::Sand,
                "wire_mesh",
                "glass_wall",
                "medium_shelter",
                "fish_bucket_stand",
                "palm",
                "desert_scrub",
                "ice_rock",
                "ice_block",
                4,
                8};
    }
    if (species == "elephant") {
        return {Terrain::Savanna,
                "cable_barrier",
                "cable_barrier",
                "large_shelter",
                "hay_rack",
                "acacia",
                "savanna_shrub",
                "sandstone_rock",
                "log_pile",
                1,
                3};
    }
    if (species == "rhino") {
        return {Terrain::Grass,
                "cable_barrier",
                "cable_barrier",
                "large_shelter",
                "hay_rack",
                "acacia",
                "savanna_shrub",
                "sandstone_rock",
                "log_pile",
                1,
                2};
    }
    if (species == "camel") {
        return {Terrain::Sand,
                "tall_mesh",
                "glass_wall",
                "large_shelter",
                "hay_rack",
                "palm",
                "desert_scrub",
                "sandstone_rock",
                "ball",
                1,
                3};
    }
    return {Terrain::Grass,
            "tall_mesh",
            "glass_wall",
            "medium_shelter",
            "hay_rack",
            "acacia",
            "savanna_shrub",
            "sandstone_rock",
            "ball",
            1,
            4};
}

std::vector<TileCoord> NearFirst(const std::vector<TileCoord>& tiles, Vec2 target) {
    std::vector<TileCoord> candidates = tiles;
    std::stable_sort(candidates.begin(), candidates.end(), [target](TileCoord a, TileCoord b) {
        return DistanceSquared(World::TileCenter(a), target) < DistanceSquared(World::TileCenter(b), target);
    });
    return candidates;
}

bool Interior(const Zoo& zoo, TileCoord tile, EnclosureId id, int margin) {
    for (int dz = -margin; dz <= margin; ++dz) {
        for (int dx = -margin; dx <= margin; ++dx) {
            if (zoo.Habitats().At(TileCoord{tile.x + dx, tile.z + dz}) != id) {
                return false;
            }
        }
    }
    return true;
}

void InsideProp(Zoo& zoo, Script& script, const std::vector<TileCoord>& tiles, std::string_view kind,
                Vec2 target) {
    Builder builder = zoo.FreeBuilder();
    const int def = zoo.Definitions().PropIndex(kind);
    for (const TileCoord tile : NearFirst(tiles, target)) {
        const Vec2 point = World::TileCenter(tile);
        if (builder.CheckProp(def, point) == BuildError::None) {
            script.Prop(kind, point, script.Random().Range(0.0f, kTwoPi));
            return;
        }
    }
    script.Problem("no dry place for " + std::string(kind));
}

void Shelter(Zoo& zoo, Script& script, const HabitatPlan& plan, const std::vector<TileCoord>& tiles,
             EnclosureId id, std::string_view kind) {
    Builder builder = zoo.FreeBuilder();
    const int index = zoo.Definitions().BuildingIndex(kind);
    if (index < 0) {
        script.Problem("unknown building " + std::string(kind));
        return;
    }
    const BuildingDef& def = zoo.Definitions().buildings[static_cast<std::size_t>(index)];
    const Vec2 target = script.Point(plan.east - plan.rx / 2, plan.north + plan.rn / 2);
    for (const TileCoord origin : NearFirst(tiles, target)) {
        const TileCoord far{origin.x + def.width - 1, origin.z + def.depth - 1};
        bool inside = true;
        for (const TileCoord tile : TilesInRect(origin, far)) {
            inside = inside && zoo.Habitats().At(tile) == id;
        }
        if (inside && zoo.Habitats().At(FrontTile(def, origin, Dir::South)) == id &&
            builder.CheckBuilding(index, origin, Dir::South) == BuildError::None) {
            const TileCoord gate = zoo.Map().EntranceTile();
            script.Building(kind, origin.x - gate.x, gate.z - far.z, Dir::South);
            return;
        }
    }
    script.Problem(std::string(plan.name) + ": no room for its shelter");
}

void HabitatPool(Zoo& zoo, Script& script, const HabitatPlan& plan, const std::vector<TileCoord>& tiles,
                 EnclosureId id, const SpeciesDef& species) {
    const float share = plan.species == "giraffe" ? 0.09f : species.waterAreaFraction + 0.015f;
    const int count = static_cast<int>(std::ceil(static_cast<float>(tiles.size()) * share));
    const Vec2 centre = script.Point(plan.east + plan.rx / 3, plan.north + plan.rn / 5);
    std::vector<TileCoord> pool;
    Builder builder = zoo.FreeBuilder();
    for (const TileCoord tile : NearFirst(tiles, centre)) {
        if (Interior(zoo, tile, id, 1) && builder.CheckWater(tile) == BuildError::None) {
            pool.push_back(tile);
            if (static_cast<int>(pool.size()) == count) {
                break;
            }
        }
    }
    const BuildPlan result = builder.PlaceWater(pool, species.needsDeepWater ? 1.4f : 0.45f);
    if (!result.Ok() || result.skipped > 0 || static_cast<int>(pool.size()) < count) {
        script.Problem(std::string(plan.name) + ": pool incomplete");
    }
}

void HabitatPlants(Zoo& zoo, Script& script, const HabitatPlan& plan, const HabitatStyle& style,
                   const std::vector<TileCoord>& tiles, EnclosureId id, float density) {
    Builder builder = zoo.FreeBuilder();
    const Vec2 back = script.Point(plan.east - plan.rx / 3, plan.north + plan.rn);
    const auto candidates = NearFirst(tiles, back);
    int planted = 0;
    for (const TileCoord tile : candidates) {
        if (zoo.Habitats().Find(id)->stats.vegetationDensity >= density) {
            return;
        }
        if (!Interior(zoo, tile, id, plan.rx <= 5 ? 1 : 2)) {
            continue;
        }
        std::string_view kind = planted % 3 == 0 ? style.bush : style.tree;
        if (plan.species == "wolf" && planted % 4 == 0) {
            kind = "birch";
        } else if (plan.species == "wolf" && planted % 7 == 0) {
            kind = "pine";
        }
        if ((plan.species == "giraffe" || plan.species == "elephant") && planted % 8 == 0) {
            kind = "baobab";
        }
        const Vec2 point = World::TileCenter(tile);
        if (builder.CheckProp(zoo.Definitions().PropIndex(kind), point) == BuildError::None) {
            script.Prop(kind, point, script.Random().Range(0.0f, kTwoPi), script.Random().Range(0.8f, 1.2f));
            ++planted;
        }
    }
    if (zoo.Habitats().Find(id)->stats.vegetationDensity + 0.05f < density) {
        script.Problem(std::string(plan.name) + ": planting target not met");
    }
}

bool Omitted(const VisitorZooOptions& options, std::string_view species) {
    return std::find(options.withoutSpecies.begin(), options.withoutSpecies.end(), species) !=
           options.withoutSpecies.end();
}

void Habitat(Zoo& zoo, Script& script, const HabitatPlan& plan, const VisitorZooOptions& options) {
    const auto tiles = RegionTiles(script, plan);
    const HabitatStyle style = Style(plan.species);
    const int speciesIndex = zoo.SpeciesList().IndexOf(plan.species);
    if (speciesIndex < 0) {
        script.Problem("unknown species " + std::string(plan.species));
        return;
    }
    const SpeciesDef& species = zoo.SpeciesList().All()[static_cast<std::size_t>(speciesIndex)];
    Builder builder = zoo.FreeBuilder();
    builder.PaintTerrain(tiles, style.ground);
    const EnclosureId id = script.EncloseTiles(tiles, style.fence, std::string(plan.name));
    if (!id) {
        return;
    }
    script.ViewingFront(tiles, style.front, {Dir::South, plan.east < 0 ? Dir::East : Dir::West});
    script.KeeperGate(tiles, style.fence);
    Shelter(zoo, script, plan, tiles, id, style.shelter);
    HabitatPool(zoo, script, plan, tiles, id, species);
    const int e = plan.east;
    const int n = plan.north;
    InsideProp(zoo, script, tiles, style.feeder, script.Point(e - plan.rx / 3, n - plan.rn / 3));
    InsideProp(zoo, script, tiles, style.feeder, script.Point(e + plan.rx / 3, n - plan.rn / 2));
    InsideProp(zoo, script, tiles, "water_trough", script.Point(e, n - plan.rn / 2));
    InsideProp(zoo, script, tiles, style.toy, script.Point(e - plan.rx / 2, n));
    InsideProp(zoo, script, tiles, "ball", script.Point(e, n - plan.rn / 3));
    if (plan.rx > 5) {
        InsideProp(zoo, script, tiles, "fallen_log", script.Point(e - plan.rx / 2, n + 2));
        InsideProp(zoo, script, tiles, "log_pile", script.Point(e + plan.rx / 2, n - 2));
    }
    for (int i = 0; i < (plan.rx > 5 ? 5 : 2); ++i) {
        InsideProp(zoo, script, tiles, style.rock,
                   script.Point(e + plan.rx / 2 + i % 2, n + plan.rn / 2 - i));
    }
    if (plan.species == "giraffe" || plan.species == "elephant") {
        InsideProp(zoo, script, tiles, "hay_rack", script.Point(e, n - plan.rn / 2 - 2));
        InsideProp(zoo, script, tiles, "pellet_fruit_trough", script.Point(e - 3, n));
    }
    // Auto-care visits at 07:00 and 15:00. Stock at least eighteen hours of food
    // and water so large herds are not dependent on one small catalog feeder.
    const float groupSize = static_cast<float>(style.males + style.females);
    const int feederIndex = zoo.Definitions().PropIndex(style.feeder);
    if (feederIndex >= 0) {
        const float capacity = zoo.Definitions().props[static_cast<std::size_t>(feederIndex)].capacity;
        const int feeders = static_cast<int>(std::ceil(species.foodPerDayKg * groupSize * 0.75f / capacity));
        for (int i = 2; i < feeders; ++i) {
            InsideProp(zoo, script, tiles, style.feeder,
                       script.Point(e - plan.rx / 2 + i * 2, n - plan.rn / 2));
        }
    }
    const float waterNeeded = species.waterPerDayL * groupSize + (plan.species == "giraffe" ? 150.0f : 0.0f);
    const int troughs = static_cast<int>(std::ceil(waterNeeded * 0.75f / 180.0f));
    for (int i = 1; i < troughs; ++i) {
        InsideProp(zoo, script, tiles, "water_trough", script.Point(e + i * 3, n - plan.rn / 3));
    }
    HabitatPlants(zoo, script, plan, style, tiles, id, species.vegetationDensity);
    const float enrichment =
        species.playfulness * (0.5f + 0.25f * static_cast<float>(style.males + style.females));
    for (int i = 0; i < 12 && zoo.Habitats().Find(id)->stats.enrichment < enrichment; ++i) {
        InsideProp(zoo, script, tiles, style.toy, script.Point(e - 1 + i % 3, n - plan.rn / 2 + i / 3));
    }
    if (!Omitted(options, plan.species)) {
        script.Animals(plan.species, id, e - plan.rx, n - plan.rn, e + plan.rx, n + plan.rn, style.males,
                       style.females);
    }
    if (plan.species == "giraffe" && !Omitted(options, "zebra")) {
        script.Animals("zebra", id, e - plan.rx, n - plan.rn, e + plan.rx, n + plan.rn, 1, 5);
    }
}

void Service(Zoo& zoo, Script& script, std::string_view kind, int east, int north, Dir facing) {
    Builder builder = zoo.FreeBuilder();
    const int index = zoo.Definitions().BuildingIndex(kind);
    if (index < 0) {
        script.Problem("unknown building " + std::string(kind));
        return;
    }
    const BuildingDef& def = zoo.Definitions().buildings[static_cast<std::size_t>(index)];
    const TileCoord size = FootprintSize(def, facing);
    const TileCoord wanted = script.Tile(east, north + size.z - 1);
    const auto candidates = NearFirst(TilesInRect({wanted.x - 5, wanted.z - 5}, {wanted.x + 5, wanted.z + 5}),
                                      World::TileCenter(wanted));
    for (const TileCoord origin : candidates) {
        bool clear = true;
        const TileCoord far{origin.x + size.x - 1, origin.z + size.z - 1};
        for (const TileCoord tile : TilesInRect(origin, far)) {
            clear = clear && zoo.Map().InBounds(tile) && zoo.Habitats().RegionAt(tile) == Region::Public &&
                    zoo.Map().At(tile).path == PathKind::None;
        }
        if (!clear || builder.CheckBuilding(index, origin, facing) != BuildError::None) {
            continue;
        }
        const TileCoord front = FrontTile(def, origin, facing);
        const auto path = zoo.Routes().NearestPassable(front, NavProfile::Visitor(), 8);
        if (!path) {
            continue;
        }
        const TileCoord gate = zoo.Map().EntranceTile();
        if (!script.Building(kind, origin.x - gate.x, gate.z - far.z, facing)) {
            return;
        }
        std::vector<TileCoord> apron;
        for (const TileCoord tile : TilesInRect(front, *path)) {
            if (zoo.Habitats().RegionAt(tile) == Region::Public &&
                builder.CheckPath(tile, PathKind::Cobblestone) == BuildError::None) {
                apron.push_back(tile);
            }
        }
        script.PathTiles(apron, PathKind::Cobblestone);
        if (!zoo.Routes().Reachable(gate, front, NavProfile::Visitor())) {
            script.Problem(std::string(kind) + ": door is unreachable");
        }
        return;
    }
    script.Problem("no public site for " + std::string(kind));
}

void PublicProp(Zoo& zoo, Script& script, std::string_view kind, Vec2 position, float heading = 0.0f) {
    if (zoo.FreeBuilder().CheckProp(zoo.Definitions().PropIndex(kind), position) == BuildError::None) {
        script.Prop(kind, position, heading);
    }
}

void ViewingFurniture(Zoo& zoo, Script& script) {
    // Construction refreshes enclosure storage, so keep a value snapshot of the viewing places.
    const auto habitats = zoo.Habitats().All();
    for (const Enclosure& habitat : habitats) {
        const Vec2 centre =
            World::TileCenter({(habitat.min.x + habitat.max.x) / 2, (habitat.min.z + habitat.max.z) / 2});
        int sideNumber = 0;
        for (const Vec2 direction : {Vec2{0.0f, 1.0f}, Vec2{-1.0f, 0.0f}, Vec2{1.0f, 0.0f}}) {
            const Vec2 target = centre + direction * 100.0f;
            const ViewingSpot* best = nullptr;
            float distance = 1.0e9f;
            for (const ViewingSpot& spot : habitat.viewing) {
                const float next = DistanceSquared(World::TileCenter(spot.tile), target);
                if (next < distance) {
                    best = &spot;
                    distance = next;
                }
            }
            if (best == nullptr) {
                continue;
            }
            const Vec2 toward = Direction(best->side);
            const Vec2 point = World::TileCenter(best->tile);
            const float heading = std::atan2(toward.x, toward.z);
            script.Prop("bench", point - toward * 0.55f, heading);
            script.Prop("lamp", point + toward * 0.65f);
            const Vec2 along{toward.z, -toward.x};
            if (sideNumber == 0) {
                script.Prop("info_board", point + along * 0.7f, heading + kPi);
            } else {
                script.Prop("litter_bin", point + along * 0.7f, heading);
            }
            ++sideNumber;
        }
    }
}

void Facilities(Zoo& zoo, Script& script) {
    script.PathTiles(script.Ellipse(13, 74, 9.0f, 4.0f), PathKind::Cobblestone);
    // Trunks stand on unpaved islands; the broad crowns shade the two rows of tables.
    for (int e = 6; e <= 21; e += 3) {
        script.PathTiles({script.Tile(e, 74)}, PathKind::None);
        script.Prop("acacia", script.Point(e, 74));
    }
    Service(zoo, script, "gift_shop", 12, 2, Dir::West);
    Service(zoo, script, "restroom", -15, 2, Dir::East);
    Service(zoo, script, "drinks_stall", 14, 7, Dir::West);
    Service(zoo, script, "ice_cream_stand", -16, 8, Dir::East);
    Service(zoo, script, "education_centre", -20, 15, Dir::South);
    Service(zoo, script, "burger_stall", 6, 79, Dir::South);
    Service(zoo, script, "drinks_stall", 11, 80, Dir::South);
    Service(zoo, script, "ice_cream_stand", 16, 79, Dir::South);
    Service(zoo, script, "restroom", 21, 78, Dir::South);
    Service(zoo, script, "burger_stall", 0, 108, Dir::South);
    Service(zoo, script, "drinks_stall", 5, 110, Dir::South);
    Service(zoo, script, "restroom", 10, 110, Dir::South);
    Service(zoo, script, "drinks_stall", -25, 74, Dir::South);
    Service(zoo, script, "ice_cream_stand", -25, 99, Dir::South);
    Service(zoo, script, "drinks_stall", 31, 35, Dir::South);
    Service(zoo, script, "ice_cream_stand", -24, 45, Dir::East);
    Service(zoo, script, "drinks_stall", 31, 94, Dir::East);
    Service(zoo, script, "ice_cream_stand", 32, 139, Dir::South);
    Service(zoo, script, "drinks_stall", -4, 56, Dir::South);
    Service(zoo, script, "drinks_stall", -72, 52, Dir::East);
    Service(zoo, script, "drinks_stall", 70, 52, Dir::West);
    Service(zoo, script, "drinks_stall", -70, 115, Dir::East);
    Service(zoo, script, "ice_cream_stand", -69, 17, Dir::East);
    Service(zoo, script, "drinks_stall", -10, 144, Dir::South);
    Service(zoo, script, "drinks_stall", 66, 123, Dir::West);
    Service(zoo, script, "drinks_stall", 66, 91, Dir::West);
    Service(zoo, script, "ice_cream_stand", -67, 84, Dir::East);
    for (int n = 72; n <= 76; n += 4) {
        for (int e = 6; e <= 21; e += 3) {
            PublicProp(zoo, script, "picnic_table", script.Point(e, n));
        }
    }
    script.Prop("fountain", script.Point(0, 7));
    script.Prop("statue", script.Point(0, 9));
    script.Prop("fountain", script.Point(23, 71));
    script.Prop("statue", script.Point(3, 101));
    for (const int east : {-1, 1}) {
        script.Prop("flower_bed", script.Point(east, 8));
        script.Prop("flower_bed", script.Point(east, 9));
    }
    for (const int east : {-3, 3}) {
        for (const int north : {6, 9}) {
            script.Prop("bench", script.Point(east, north), east < 0 ? kPi * 0.5f : -kPi * 0.5f);
        }
    }
    for (const int east : {-9, 9}) {
        script.Prop("info_board", script.Point(east, 3));
        script.Prop("litter_bin", script.Point(east, 8));
        script.Prop("lamp", script.Point(east, 4));
        script.Prop("lamp", script.Point(east, 11));
    }
    ViewingFurniture(zoo, script);
    int lamps = 0;
    const World& map = zoo.Map();
    for (int z = map.PropertyMin().z; z <= map.PropertyMax().z; ++z) {
        for (int x = map.PropertyMin().x; x <= map.PropertyMax().x; ++x) {
            const TileCoord tile{x, z};
            if (map.At(tile).path == PathKind::None || (x + 3 * z) % 13 != 0) {
                continue;
            }
            for (const Dir side : {Dir::North, Dir::East, Dir::South, Dir::West}) {
                const TileCoord next = tile + Step(side);
                if (map.InBounds(next) && zoo.Habitats().RegionAt(next) == Region::Public &&
                    map.At(next).path == PathKind::None && !map.At(next).building) {
                    const Vec2 point = World::TileCenter(tile) + Direction(side) * 0.75f;
                    PublicProp(zoo, script, "lamp", point);
                    ++lamps;
                    if (lamps % 3 == 0) {
                        PublicProp(zoo, script, "litter_bin", World::TileCenter(tile));
                    }
                    break;
                }
            }
        }
    }
}

void GroundDetail(Zoo& zoo, Script& script) {
    Builder builder = zoo.FreeBuilder();
    for (const HabitatPlan& plan : kHabitats) {
        const EnclosureId id = zoo.Habitats().At(script.Tile(plan.east, plan.north));
        if (!id) {
            continue;
        }
        std::vector<TileCoord> patch;
        for (const TileCoord tile :
             script.Ellipse(plan.east - plan.rx / 3, plan.north - plan.rn / 3,
                            static_cast<float>(plan.rx) * (plan.species == "elephant" ? 0.3f : 0.45f),
                            static_cast<float>(plan.rn) * 0.35f)) {
            if (zoo.Habitats().At(tile) == id) {
                patch.push_back(tile);
            }
        }
        Terrain terrain = Terrain::Dirt;
        if (plan.species == "polar_bear" || plan.species == "penguin" || plan.species == "brown_bear") {
            terrain = Terrain::Rock;
        } else if (plan.species == "tiger" || plan.species == "wolf") {
            terrain = Terrain::Grass;
        } else if (plan.species == "flamingo" || plan.species == "elephant") {
            terrain = Terrain::Sand;
        }
        builder.PaintTerrain(patch, terrain);
        if (plan.species == "elephant") {
            builder.PaintTerrain(script.Ellipse(plan.east, plan.north - plan.rn / 2, 6.0f, 4.0f),
                                 Terrain::Dirt);
        }
        if (plan.rx > 5) {
            // A low dry rise behind the front; the pool on the other lobe supplies the hollow.
            builder.Terraform(Builder::Brush::Raise, script.Point(plan.east - plan.rx / 2, plan.north - 1),
                              5.0f, 0.55f);
        }
    }
}

bool BesidePath(const World& map, TileCoord tile, int radius) {
    for (int dz = -radius; dz <= radius; ++dz) {
        for (int dx = -radius; dx <= radius; ++dx) {
            const TileCoord next{tile.x + dx, tile.z + dz};
            if (map.InBounds(next) && map.At(next).path != PathKind::None) {
                return true;
            }
        }
    }
    return false;
}

std::string_view ParkTree(int east, int north, int variant) {
    if (east > 30 && north > 105) {
        return variant == 0 ? "bamboo_clump" : "birch";
    }
    if (north > 100) {
        constexpr std::array<std::string_view, 3> trees{"spruce", "pine", "birch"};
        return trees[static_cast<std::size_t>(variant)];
    }
    if ((east > 15 && north < 35) || (east < -32 && north > 75)) {
        return variant == 0 ? "birch" : "willow";
    }
    if (north > 65 && east > -28 && east < 0) {
        return "palm";
    }
    if (north > 20 || std::abs(east) > 28) {
        return variant == 0 ? "baobab" : "acacia";
    }
    return variant == 0 ? "birch" : "oak";
}

void Parkland(Zoo& zoo, Script& script) {
    const World& map = zoo.Map();
    const TileCoord gate = map.EntranceTile();
    for (int z = map.PropertyMin().z + 1; z < map.PropertyMax().z; z += 3) {
        for (int x = map.PropertyMin().x + 1; x < map.PropertyMax().x; x += 3) {
            const TileCoord tile{x, z};
            if (zoo.Habitats().RegionAt(tile) != Region::Public || map.At(tile).building ||
                map.HasWater(tile) || BesidePath(map, tile, 1)) {
                continue;
            }
            const int variant = script.Random().RangeInt(0, 2);
            const Vec2 offset{script.Random().Range(-0.45f, 0.45f), script.Random().Range(-0.45f, 0.45f)};
            PublicProp(zoo, script, ParkTree(x - gate.x, gate.z - z, variant),
                       World::TileCenter(tile) + offset, script.Random().Range(0.0f, kTwoPi));
        }
    }
    for (int z = map.PropertyMin().z + 1; z < map.PropertyMax().z; ++z) {
        for (int x = map.PropertyMin().x + 1; x < map.PropertyMax().x; ++x) {
            const TileCoord tile{x, z};
            if ((x + 5 * z) % 19 != 0 || zoo.Habitats().RegionAt(tile) != Region::Public ||
                map.At(tile).path != PathKind::None || map.At(tile).building || map.HasWater(tile) ||
                !BesidePath(map, tile, 1)) {
                continue;
            }
            const std::string_view kind = (x + z) % 3 == 0 ? "flower_bed" : "juniper";
            PublicProp(zoo, script, kind, World::TileCenter(tile));
        }
    }
}

}  // namespace

ZooConfig VisitorZooConfig(std::uint64_t seed) {
    ZooConfig config;
    config.name = "Greenhill Zoo";
    config.seed = seed;
    config.world.seed = seed;
    config.world.width = 160;
    config.world.depth = 160;
    config.world.relief = 0.12f;
    config.sandbox = true;
    config.autoCare = true;
    return config;
}

ScenarioReport BuildVisitorZoo(Zoo& zoo, const VisitorZooOptions& options) {
    Script script(zoo);
    const World& map = zoo.Map();
    const TileCoord gate = map.EntranceTile();
    if (gate.x - map.PropertyMin().x < 70 || map.PropertyMax().x - gate.x < 70 ||
        gate.z - map.PropertyMin().z < 145) {
        script.Problem("the Visitor Zoo needs 70 tiles beside the entrance and 145 tiles north of it");
        return script.Report();
    }
    Walks(zoo, script);
    for (const HabitatPlan& plan : kHabitats) {
        Habitat(zoo, script, plan, options);
    }
    // What follows stands outside the habitats and asks about none of their figures.
    script.Hold();
    Facilities(zoo, script);
    GroundDetail(zoo, script);
    Parkland(zoo, script);
    script.Release();
    zoo.Guests().SetTicketPrice(0);
    script.Provision();
    return script.Report();
}
}  // namespace zoo
