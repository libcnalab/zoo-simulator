// SPDX-License-Identifier: MIT
#include "zoo/sim/VisitorZoo.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <set>
#include <span>
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
    if (plan.rx <= 5) {
        // A sandy mound with a two-tile drinking dish and room between its furnishings.
        return script.Ellipse(plan.east, plan.north, static_cast<float>(plan.rx),
                              static_cast<float>(plan.rn));
    }
    // Two overlapping lobes keep long viewing fronts while avoiding straight rectangular pens.
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

int StandOffDepth(std::string_view species) {
    if (species == "elephant" || species == "rhino") {
        return 2;
    }
    if (species == "giraffe" || species == "ostrich" || species == "camel" || species == "kangaroo") {
        return 1;
    }
    return 0;
}

std::vector<TileCoord> StripCorners(const Script& script, const HabitatPlan& plan) {
    if (plan.species == "ostrich") {
        // Close three one-tile diagonal gaps against Flamingo Lagoon deliberately,
        // before they could become fenced, unnamed pockets in the region model.
        return {script.Tile(20, 28), script.Tile(21, 29), script.Tile(22, 30)};
    }
    return {};
}

std::vector<TileCoord> PublicOutline(const Script& script, const HabitatPlan& plan) {
    auto tiles = Script::Expanded(RegionTiles(script, plan), StandOffDepth(plan.species));
    const auto corners = StripCorners(script, plan);
    tiles.insert(tiles.end(), corners.begin(), corners.end());
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

    // Walks follow the public rail. Reserve the full band first so nearby circuits
    // and later habitat walks cannot pave over another habitat's clearance.
    std::set<std::pair<int, int>> reserved;
    for (const HabitatPlan& plan : kHabitats) {
        for (const TileCoord tile : PublicOutline(script, plan)) {
            reserved.emplace(tile.z, tile.x);
        }
    }
    for (const HabitatPlan& plan : kHabitats) {
        std::vector<TileCoord> bay;
        for (const TileCoord tile : PublicOutline(script, plan)) {
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
    std::string_view bush;
    std::string_view rock;
    /// What the animals are given to do, the first two always, the rest in turn as the group asks.
    std::vector<std::string_view> toys;
    /// What grows inside, each kind repeated by its share of the planting.
    std::vector<std::string_view> flora;
};

// Hoofstock stands behind what a zoo builds for it, not behind chain-link: cables for the tall and
// the heavy (the eye loses them at a few metres), a timber paddock fence for camels and kangaroos.
// Each is a barrier the species' own figures accept (Containment); a dry moat is not in the catalog.
HabitatStyle Style(std::string_view species) {
    if (species == "meerkat") {
        return {Terrain::Sand,    "wire_mesh",
                "glass_wall",     "small_shelter",
                "insect_feeder",  "desert_scrub",
                "sandstone_rock", {"termite_mound", "log_pile", "termite_mound", "termite_mound"},
                {"desert_scrub"}};
    }
    if (species == "flamingo") {
        return {Terrain::Mud,
                "wire_mesh",
                "glass_wall",
                "heated_barn",
                "fish_bucket_stand",
                "tropical_fern",
                "sandstone_rock",
                {"log_pile"},
                // Not willows: their curtain hangs to the ground and was all one saw from the glass.
                // Acacias stand by the soda lakes too, and carry their crown above the eye.
                {"tropical_fern", "tropical_fern", "tropical_fern", "acacia"}};
    }
    if (species == "giraffe") {
        return {Terrain::Savanna,
                "tall_cable_fence",
                "glass_wall",
                "large_shelter",
                "giraffe_browse_feeder",
                "savanna_shrub",
                "sandstone_rock",
                {"termite_mound", "log_pile", "fallen_log"},
                {"acacia", "acacia", "acacia", "acacia", "savanna_shrub", "savanna_shrub", "savanna_shrub",
                 "baobab"}};
    }
    if (species == "ostrich") {
        return {Terrain::Savanna,
                "tall_cable_fence",
                "glass_wall",
                "medium_shelter",
                "pellet_fruit_trough",
                "savanna_shrub",
                "sandstone_rock",
                {"termite_mound", "log_pile", "fallen_log"},
                {"acacia", "acacia", "savanna_shrub", "savanna_shrub", "savanna_shrub"}};
    }
    if (species == "lion") {
        return {Terrain::Savanna,
                "heavy_mesh",
                "glass_wall",
                "medium_shelter",
                "meat_platform",
                "savanna_shrub",
                "sandstone_rock",
                {"scratching_post", "log_pile", "climbing_frame", "ball", "fallen_log"},
                {"acacia", "acacia", "savanna_shrub", "savanna_shrub", "savanna_shrub"}};
    }
    if (species == "tiger") {
        return {Terrain::ForestFloor,
                "heavy_mesh",
                "high_glass_wall",
                "medium_shelter",
                "meat_platform",
                "tropical_fern",
                "granite_boulder",
                {"scratching_post", "log_pile", "ball"},
                {"bamboo_clump", "tropical_fern", "birch", "oak", "oak"}};
    }
    if (species == "wolf") {
        return {Terrain::ForestFloor,
                "tall_mesh",
                "glass_wall",
                "medium_shelter",
                "meat_platform",
                "juniper",
                "granite_boulder",
                {"log_pile", "fallen_log", "ball"},
                {"spruce", "spruce", "spruce", "birch", "birch", "pine", "juniper", "juniper", "juniper"}};
    }
    if (species == "brown_bear") {
        return {Terrain::ForestFloor,
                "heavy_mesh",
                "high_glass_wall",
                "medium_shelter",
                "meat_platform",
                "juniper",
                "granite_boulder",
                {"climbing_frame", "log_pile", "tyre_swing", "ball"},
                // A mixed mountain wood: pines alone, at the cover a bear asks for, stood as a plantation.
                {"pine", "pine", "spruce", "spruce", "oak", "oak", "oak", "birch", "juniper", "juniper"}};
    }
    if (species == "polar_bear") {
        return {Terrain::Snow,
                "heavy_mesh",
                "high_glass_wall",
                "cooled_den",
                "fish_bucket_stand",
                "juniper",
                "ice_rock",
                {"ice_block", "ball", "log_pile"},
                {"juniper", "juniper", "juniper", "juniper", "pine"}};
    }
    if (species == "penguin") {
        return {Terrain::Sand,
                "wire_mesh",
                "glass_wall",
                "medium_shelter",
                "fish_bucket_stand",
                "desert_scrub",
                "ice_rock",
                {"ice_block", "ball", "ice_block", "ice_block", "ice_rock"},
                {"desert_scrub", "desert_scrub", "desert_scrub", "desert_scrub", "palm"}};
    }
    if (species == "elephant") {
        return {Terrain::Savanna,
                "cable_barrier",
                "cable_barrier",
                "large_shelter",
                "hay_rack",
                "savanna_shrub",
                "sandstone_rock",
                {"log_pile", "tyre_swing", "ball", "scratching_post"},
                {"acacia", "acacia", "acacia", "acacia", "savanna_shrub", "savanna_shrub", "savanna_shrub",
                 "baobab"}};
    }
    if (species == "rhino") {
        return {Terrain::Grass,
                "cable_barrier",
                "cable_barrier",
                "large_shelter",
                "hay_rack",
                "savanna_shrub",
                "sandstone_rock",
                {"log_pile", "scratching_post"},
                {"acacia", "acacia", "savanna_shrub", "savanna_shrub", "savanna_shrub"}};
    }
    if (species == "camel") {
        return {Terrain::Sand,
                "paddock_rail",
                "glass_wall",
                "large_shelter",
                "hay_rack",
                "desert_scrub",
                "sandstone_rock",
                {"log_pile", "scratching_post", "fallen_log"},
                {"desert_scrub", "desert_scrub", "desert_scrub", "desert_scrub", "palm", "palm"}};
    }
    return {Terrain::Grass,
            "paddock_rail",
            "glass_wall",
            "medium_shelter",
            "hay_rack",
            "savanna_shrub",
            "sandstone_rock",
            {"log_pile", "fallen_log", "termite_mound"},
            // Wattles are acacias: the nearest the catalog has to an Australian wood.
            {"acacia", "acacia", "savanna_shrub", "savanna_shrub", "savanna_shrub"}};
}

/// One resident: its sex, its age in years on the opening day, and for a young animal the place in
/// the family of its mother (who stands before it in the list).
struct Resident {
    Sex sex;
    float age;
    int mother;
};

constexpr Resident Adult(Sex sex, float age) {
    return {sex, age, -1};
}
constexpr Resident Young(Sex sex, float age, int mother) {
    return {sex, age, mother};
}

// Who lives where, as zoos keep them: a breeding group with this season's young for the social
// species, a pair for the solitary ones. The first of each family is its sire. The youngest are
// under a quarter of their species' age of maturity and stay under it for a week of the calendar
// and more: they suckle, keep to their mothers and are drawn as juveniles (below half of maturity).
// Older daughters are female so that no second adult male grows up into a fight. The kangaroos'
// joeys are the exception: under half a year they would be in the pouch, which the simulation does
// not have, so theirs are young at foot, past the quarter and still drawn young.
std::span<const Resident> Family(std::string_view species) {
    constexpr Sex m = Sex::Male;
    constexpr Sex f = Sex::Female;
    if (species == "meerkat") {
        static constexpr Resident family[] = {Adult(m, 4.0f), Adult(f, 5.0f), Adult(f, 3.0f),
                                              Young(m, 0.15f, 1), Young(f, 0.15f, 1)};
        return family;
    }
    if (species == "flamingo") {
        static constexpr Resident family[] = {Adult(m, 9.0f),  Adult(m, 14.0f),   Adult(m, 7.0f),
                                              Adult(m, 20.0f), Adult(m, 11.0f),   Adult(f, 10.0f),
                                              Adult(f, 16.0f), Adult(f, 6.5f),    Adult(f, 12.0f),
                                              Adult(f, 8.0f),  Young(f, 0.8f, 5), Young(m, 0.9f, 6)};
        return family;
    }
    if (species == "giraffe") {
        static constexpr Resident family[] = {Adult(m, 9.0f), Adult(f, 8.0f), Adult(f, 12.0f),
                                              Young(f, 3.0f, 2), Young(f, 0.8f, 1)};
        return family;
    }
    if (species == "zebra") {
        static constexpr Resident family[] = {Adult(m, 8.0f),  Adult(f, 6.0f),    Adult(f, 9.0f),
                                              Adult(f, 12.0f), Young(f, 1.2f, 3), Young(m, 0.4f, 1)};
        return family;
    }
    if (species == "ostrich") {
        static constexpr Resident family[] = {Adult(m, 7.0f),   Adult(f, 6.0f),    Adult(f, 8.0f),
                                              Adult(f, 5.0f),   Young(m, 0.3f, 1), Young(f, 0.3f, 1),
                                              Young(f, 0.3f, 1)};
        return family;
    }
    if (species == "lion") {
        static constexpr Resident family[] = {Adult(m, 7.0f),    Adult(f, 6.0f),    Adult(f, 9.0f),
                                              Young(f, 2.6f, 2), Young(m, 0.6f, 1), Young(f, 0.6f, 1),
                                              Young(f, 0.6f, 1)};
        return family;
    }
    if (species == "tiger") {
        static constexpr Resident family[] = {Adult(m, 7.0f), Adult(f, 6.0f)};
        return family;
    }
    if (species == "wolf") {
        static constexpr Resident family[] = {Adult(m, 6.0f),     Adult(f, 5.0f),     Young(f, 1.3f, 1),
                                              Young(m, 0.25f, 1), Young(f, 0.25f, 1), Young(m, 0.25f, 1)};
        return family;
    }
    if (species == "brown_bear") {
        static constexpr Resident family[] = {Adult(m, 12.0f), Adult(f, 10.0f)};
        return family;
    }
    if (species == "polar_bear") {
        static constexpr Resident family[] = {Adult(m, 11.0f), Adult(f, 9.0f)};
        return family;
    }
    if (species == "penguin") {
        static constexpr Resident family[] = {
            Adult(m, 5.0f),     Adult(m, 8.0f),     Adult(m, 11.0f),   Adult(m, 6.0f), Adult(m, 14.0f),
            Adult(f, 6.0f),     Adult(f, 9.0f),     Adult(f, 12.0f),   Adult(f, 5.0f), Adult(f, 10.0f),
            Young(f, 0.25f, 5), Young(m, 0.25f, 6), Young(f, 0.25f, 7)};
        return family;
    }
    if (species == "elephant") {
        static constexpr Resident family[] = {Adult(f, 26.0f), Adult(f, 38.0f), Adult(f, 22.0f),
                                              Young(f, 7.0f, 1), Young(m, 1.5f, 2)};
        return family;
    }
    if (species == "rhino") {
        static constexpr Resident family[] = {Adult(m, 14.0f), Adult(f, 16.0f), Adult(f, 10.0f),
                                              Young(f, 1.0f, 1)};
        return family;
    }
    if (species == "camel") {
        static constexpr Resident family[] = {Adult(m, 9.0f), Adult(f, 8.0f), Adult(f, 13.0f),
                                              Young(f, 2.0f, 2), Young(m, 0.7f, 1)};
        return family;
    }
    if (species == "kangaroo") {
        static constexpr Resident family[] = {Adult(m, 6.0f),    Adult(f, 4.0f),    Adult(f, 5.0f),
                                              Adult(f, 7.0f),    Young(f, 1.2f, 3), Young(m, 0.7f, 1),
                                              Young(f, 0.65f, 2)};
        return family;
    }
    return {};
}

/// The mouths a family brings to its feeders: a suckling takes half a share.
float Eaters(const SpeciesDef& species) {
    float eaters = 0.0f;
    for (const Resident& resident : Family(species.id)) {
        eaters += resident.age < species.maturityYears * 0.25f ? 0.5f : 1.0f;
    }
    return eaters;
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

/// What the furnishing of one habitat has to know about it.
struct Site {
    const HabitatPlan* plan = nullptr;
    EnclosureId id;
    std::vector<TileCoord> tiles;
    Vec2 middle;
    /// Tiles a prop keeps from the barrier, and metres it keeps from the next prop.
    int margin = 2;
    float clear = 2.0f;
    // Largest adult's collision width plus one metre; never relaxed to meet a planting target.
    float corridor = 1.0f;
    /// Tiles nothing is set down on, as (z, x): the apron of the shelter's door, the keeper's gate.
    std::set<std::pair<int, int>> kept;
    /// Where the benches will stand: the lines from there to the middle stay open, `sight` metres
    /// to either side.
    std::vector<Vec2> watched;
    float sight = 3.0f;
    /// Every place a visitor looks from, with the depth of planting between it and the barrier: no
    /// trunk stands right before one.
    std::vector<std::pair<Vec2, float>> fronts;
};

/// How far a tree keeps from every viewing place; a massive one (a baobab, a bamboo clump: a metre of
/// trunk and more) as far beyond the planted strip before the barrier, where it filled a quarter of
/// the view from the giraffes' rail. A slender trunk carries its crown above the eye.
constexpr float kTreeFromFrontM = 5.0f;
constexpr float kMassiveTrunkM = 1.0f;
/// Animals.cpp treats vegetation more than this far under the species figure as too bare. Trees
/// are set back from a barrier only while planting retains a small margin inside that welfare band.
constexpr float kCoverBand = 0.15f;
constexpr float kCoverMargin = 0.02f;

/// The places of a habitat that get a bench, the outermost viewing place to the south, to the west
/// and to the east.
std::vector<ViewingSpot> Lookouts(const Enclosure& habitat) {
    std::vector<ViewingSpot> spots;
    const Vec2 centre =
        World::TileCenter({(habitat.min.x + habitat.max.x) / 2, (habitat.min.z + habitat.max.z) / 2});
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
        if (best != nullptr) {
            spots.push_back(*best);
        }
    }
    return spots;
}

/// A point of the habitat by its shares of the half-axes: `across` from the west side (-1) to the
/// east side (1), `back` from the main front in the south (-1) to the back (1).
Vec2 Share(const Site& site, float across, float back) {
    return site.middle + Vec2{across * static_cast<float>(site.plan->rx) * kTileSize,
                              -back * static_cast<float>(site.plan->rn) * kTileSize};
}

/// Where the ground of a habitat rises (GroundDetail) and its rocks crop out.
Vec2 Rise(const Script& script, const HabitatPlan& plan) {
    return plan.rx <= 5 ? script.Point(plan.east - 1, plan.north)
                        : script.Point(plan.east - plan.rx / 2, plan.north - 1);
}

Vec2 PoolCentre(const Script& script, const HabitatPlan& plan) {
    return plan.species == "meerkat" ? script.Point(plan.east + 2, plan.north - 1)
                                     : script.Point(plan.east + plan.rx / 3, plan.north + plan.rn / 5);
}

bool Reaches(Vec2 point, float radius, TileCoord tile) {
    const float x = static_cast<float>(tile.x) * kTileSize;
    const float z = static_cast<float>(tile.z) * kTileSize;
    const float dx = point.x - std::clamp(point.x, x, x + kTileSize);
    const float dz = point.z - std::clamp(point.z, z, z + kTileSize);
    return dx * dx + dz * dz < radius * radius;
}

// The builder asks only about the tile under a prop's centre, so a rock set down beside the pool
// stood half in it and a feeder leaned into the shelter's wall. Here the whole circle has to lie on
// dry, unbuilt ground of the habitat.
bool OnDryGround(const Zoo& zoo, EnclosureId id, Vec2 point, float radius) {
    const TileCoord low = World::TileAt({point.x - radius, point.z - radius});
    const TileCoord high = World::TileAt({point.x + radius, point.z + radius});
    for (const TileCoord tile : TilesInRect(low, high)) {
        if (Reaches(point, radius, tile) &&
            (zoo.Habitats().At(tile) != id || zoo.Map().HasWater(tile) || zoo.Map().At(tile).building)) {
            return false;
        }
    }
    return true;
}

// A pool lowers the four corners of every water tile, which the eight surrounding tiles share.
// Their ground slopes under the water surface even though their tile itself is not marked water.
bool BedReaches(const World& map, TileCoord tile) {
    for (int dz = -1; dz <= 1; ++dz) {
        for (int dx = -1; dx <= 1; ++dx) {
            if (map.HasWater({tile.x + dx, tile.z + dz})) {
                return true;
            }
        }
    }
    return false;
}

/// Whether a land plant of `radius` stands entirely above the bank the pool did not dig into.
/// The catalog has no water plant, so this rule applies to every tree and bush.
bool AboveTheBank(const World& map, Vec2 point, float radius) {
    const TileCoord low = World::TileAt({point.x - radius, point.z - radius});
    const TileCoord high = World::TileAt({point.x + radius, point.z + radius});
    for (const TileCoord tile : TilesInRect(low, high)) {
        if (Reaches(point, radius, tile) && BedReaches(map, tile)) {
            return false;
        }
    }
    return true;
}

float DistanceToLine(Vec2 point, Vec2 from, Vec2 to) {
    const Vec2 along = to - from;
    const float length = along.x * along.x + along.z * along.z;
    const float t =
        length > 0.0f
            ? std::clamp(((point.x - from.x) * along.x + (point.z - from.z) * along.z) / length, 0.0f, 1.0f)
            : 0.0f;
    return Distance(point, from + along * t);
}

bool Plant(PropRole role) {
    return role == PropRole::Tree || role == PropRole::Bush;
}

/// Whether a prop may stand at `point`: on dry ground, off the kept tiles, `clear` metres of open
/// ground between it and whatever stands there already. Between two plants `spacing` counts instead,
/// from trunk to trunk (a bush asks for six tenths of it).
bool Fits(const Zoo& zoo, const Builder& builder, const Site& site, int def, Vec2 point, float scale,
          float clear, float spacing = 0.0f) {
    const Catalog& catalog = zoo.Definitions();
    const PropDef& kind = catalog.props[static_cast<std::size_t>(def)];
    const float radius = kind.radiusM * scale;
    const TileCoord tile = World::TileAt(point);
    if (site.kept.contains({tile.z, tile.x}) || !OnDryGround(zoo, site.id, point, radius + 0.1f) ||
        (Plant(kind.role) && !AboveTheBank(zoo.Map(), point, radius + 0.1f)) ||
        builder.CheckProp(def, point) != BuildError::None) {
        return false;
    }
    const float obstacleRadius = std::max(radius, kind.radiusM);
    for (const auto& [z, x] : site.kept) {
        if (Reaches(point, obstacleRadius + site.corridor * 0.5f, {x, z})) {
            return false;
        }
    }
    std::vector<PropId> near;
    zoo.Objects().PropsNear(point, obstacleRadius + std::max({clear, spacing, site.corridor}) + 2.2f, near);
    for (const PropId id : near) {
        const Prop* other = zoo.Objects().FindProp(id);
        if (other == nullptr) {
            continue;
        }
        const PropDef& otherKind = catalog.props[static_cast<std::size_t>(other->def)];
        const float distance = Distance(point, other->position);
        const float edges = distance - radius - otherKind.radiusM * other->scale;
        // Movement currently uses the unscaled catalog radius. Honour both that collision
        // envelope and the visible scaled prop, including small enrichment items.
        if (kind.blocksWalking && otherKind.blocksWalking &&
            distance < obstacleRadius + otherKind.radiusM * std::max(1.0f, other->scale) + site.corridor) {
            return false;
        }
        if (Plant(kind.role) && Plant(otherKind.role)) {
            const bool trees = kind.role == PropRole::Tree && otherKind.role == PropRole::Tree;
            if (edges < 0.2f || distance < (trees ? spacing : spacing * 0.6f)) {
                return false;
            }
        } else if (edges < clear) {
            return false;
        }
    }
    return true;
}

/// Sets a prop down on the free place nearest to `target`; false when the habitat has none.
bool Settle(Zoo& zoo, Script& script, const Site& site, std::string_view kind, Vec2 target, float clear,
            float scale = 1.0f) {
    const int def = zoo.Definitions().PropIndex(kind);
    if (def < 0) {
        script.Problem("unknown prop " + std::string(kind));
        return false;
    }
    const Builder builder = zoo.FreeBuilder();
    for (const TileCoord tile : NearFirst(site.tiles, target)) {
        if (!Interior(zoo, tile, site.id, site.margin)) {
            continue;
        }
        // Off the tile's centre where there is room, so that the furniture does not line up on the grid.
        const Vec2 centre = World::TileCenter(tile);
        const Vec2 loose =
            centre + Vec2{script.Random().Range(-0.4f, 0.4f), script.Random().Range(-0.4f, 0.4f)};
        for (const Vec2 point : {loose, centre}) {
            if (Fits(zoo, builder, site, def, point, scale, clear)) {
                return static_cast<bool>(
                    script.Prop(kind, point, script.Random().Range(0.0f, kTwoPi), scale));
            }
        }
    }
    return false;
}

void Need(Zoo& zoo, Script& script, const Site& site, std::string_view kind, Vec2 target, float clear,
          float scale = 1.0f) {
    if (!Settle(zoo, script, site, kind, target, clear, scale)) {
        script.Problem(std::string(site.plan->name) + ": no place for " + std::string(kind));
    }
}

/// Puts the shelter at the back, its door to the main front; returns the tile before the door.
std::optional<TileCoord> Shelter(Zoo& zoo, Script& script, const HabitatPlan& plan,
                                 const std::vector<TileCoord>& tiles, EnclosureId id, std::string_view kind) {
    Builder builder = zoo.FreeBuilder();
    const int index = zoo.Definitions().BuildingIndex(kind);
    if (index < 0) {
        script.Problem("unknown building " + std::string(kind));
        return std::nullopt;
    }
    const BuildingDef& def = zoo.Definitions().buildings[static_cast<std::size_t>(index)];
    const Vec2 target = script.Point(plan.east - plan.rx / 2, plan.north + plan.rn / 2);
    for (const TileCoord origin : NearFirst(tiles, target)) {
        const TileCoord far{origin.x + def.width - 1, origin.z + def.depth - 1};
        bool inside = true;
        for (const TileCoord tile : TilesInRect(origin, far)) {
            inside = inside && zoo.Habitats().At(tile) == id;
        }
        const TileCoord front = FrontTile(def, origin, Dir::South);
        if (inside && zoo.Habitats().At(front) == id &&
            builder.CheckBuilding(index, origin, Dir::South) == BuildError::None) {
            const TileCoord gate = zoo.Map().EntranceTile();
            script.Building(kind, origin.x - gate.x, gate.z - far.z, Dir::South);
            return front;
        }
    }
    script.Problem(std::string(plan.name) + ": no room for its shelter");
    return std::nullopt;
}

/// Digs the pool and returns its radius in metres.
float HabitatPool(Zoo& zoo, Script& script, const Site& site, const SpeciesDef& species) {
    const HabitatPlan& plan = *site.plan;
    // A shallow 8 m² drinking hollow satisfies the catalog's water-area requirement;
    // the rest of the meerkat mound stays dry and large enough for clear corridors.
    const bool dish = plan.species == "meerkat";
    // Flamingos stand in their water: a lagoon to wade, with a deep heart for the birds that swim.
    const bool lagoon = plan.species == "flamingo";
    const float share = plan.species == "giraffe" ? 0.09f : species.waterAreaFraction + 0.015f;
    const int count = dish ? 2 : static_cast<int>(std::ceil(static_cast<float>(site.tiles.size()) * share));
    std::vector<TileCoord> pool;
    Builder builder = zoo.FreeBuilder();
    // Keep a dry bank on every side. Digging at the back fence also lowers the shared
    // vertices of the public boardwalk and leaves a wheelchair-steep edge in its paving.
    const auto shore = [&](TileCoord tile) { return Interior(zoo, tile, site.id, 1); };
    for (const TileCoord tile : NearFirst(site.tiles, PoolCentre(script, plan))) {
        // The apron of the shelter's door and the keeper's way in stay dry.
        if (shore(tile) && !site.kept.contains({tile.z, tile.x}) &&
            builder.CheckWater(tile) == BuildError::None) {
            pool.push_back(tile);
            if (static_cast<int>(pool.size()) == count) {
                break;
            }
        }
    }
    const float depth = dish ? Builder::kMinWaterDepth : species.needsDeepWater ? 1.4f : 0.45f;
    // Nearest first: the heart is dug before the shallows, which take its surface.
    const std::size_t heart = lagoon ? pool.size() / 8 : pool.size();
    const std::span<const TileCoord> all(pool);
    BuildPlan result = builder.PlaceWater(all.first(heart), depth);
    bool whole = result.Ok() && result.skipped == 0;
    if (heart < pool.size()) {
        result = builder.PlaceWater(all.subspan(heart), 0.3f);
        whole = whole && result.Ok() && result.skipped == 0;
    }
    if (!whole || static_cast<int>(pool.size()) < count) {
        script.Problem(std::string(plan.name) + ": pool incomplete");
    }
    return std::sqrt(static_cast<float>(pool.size()) * kTileSize * kTileSize / kPi);
}

/// Smooth noise over the tile grid, 0..1, with features some eight metres across: where it is high
/// a grove stands, where it is low a glade.
float Grove(TileCoord tile, std::uint32_t salt) {
    constexpr int cell = 4;
    const auto corner = [salt](int x, int z) {
        return HashFloat(HashU32(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(z)), salt);
    };
    const auto ease = [](float t) { return t * t * (3.0f - 2.0f * t); };
    const int x = tile.x / cell;
    const int z = tile.z / cell;
    const float fx = ease(static_cast<float>(tile.x % cell) / static_cast<float>(cell));
    const float fz = ease(static_cast<float>(tile.z % cell) / static_cast<float>(cell));
    return Lerp(Lerp(corner(x, z), corner(x + 1, z), fx), Lerp(corner(x, z + 1), corner(x + 1, z + 1), fx),
                fz);
}

std::uint32_t Salt(const Zoo& zoo, const HabitatPlan& plan) {
    return HashU32(static_cast<std::uint32_t>(zoo.Config().seed),
                   static_cast<std::uint32_t>((plan.east + 1000) * 4099 + plan.north));
}

// Plants in groves with glades between them, towards the back and the rim, never in the lines from
// the benches to the middle of the habitat: the old planting filled the tiles nearest the back one
// by one, which stood as a block of trunks two metres apart (the pines of Bear Falls).
void HabitatPlants(Zoo& zoo, Script& script, const Site& site, const HabitatStyle& style, float target) {
    // A species is content anywhere within 0.15 of its figure; the lower side keeps the views open.
    const float wanted = std::max(0.03f, target - (target > 0.4f ? 0.12f : 0.08f));
    const HabitatPlan& plan = *site.plan;
    // On the meerkats' mound the scrub stands along the mesh and stays under the eye; beside a
    // lagoon the ferns take the shore.
    const bool small = plan.rx <= 5;
    const bool tight = site.margin < 2;
    const std::uint32_t salt = Salt(zoo, plan);
    struct Bed {
        TileCoord tile;
        float rank;
    };
    std::vector<Bed> beds;
    for (const TileCoord tile : site.tiles) {
        if (!Interior(zoo, tile, site.id, tight ? 0 : 1) || BedReaches(zoo.Map(), tile)) {
            continue;
        }
        const Vec2 centre = World::TileCenter(tile);
        const float across = (centre.x - site.middle.x) / (static_cast<float>(plan.rx) * kTileSize);
        const float back = (site.middle.z - centre.z) / (static_cast<float>(plan.rn) * kTileSize);
        const float rim = Clamp01(std::sqrt(across * across + back * back));
        beds.push_back({tile, 0.5f * Grove(tile, salt) + 0.3f * Clamp01(0.5f + 0.5f * back) + 0.2f * rim});
    }
    std::stable_sort(beds.begin(), beds.end(), [](const Bed& a, const Bed& b) { return a.rank > b.rank; });
    const Builder builder = zoo.FreeBuilder();
    const Catalog& catalog = zoo.Definitions();
    const auto density = [&] { return zoo.Habitats().Find(site.id)->stats.vegetationDensity; };
    // First keep every trunk behind the depth of the planted strip, not only massive trees. A
    // slender acacia a metre inside Elephant Valley's cable still put its crown in the view.
    const auto plant = [&](bool setBack) {
        std::vector<PropId> placed;
        for (const float spacing : {3.6f, 2.6f, 2.0f}) {
            for (const Bed& bed : beds) {
                if (density() >= wanted) {
                    return placed;
                }
                const std::uint32_t pick =
                    HashU32(static_cast<std::uint32_t>(bed.tile.x * 4099 + bed.tile.z), salt);
                const std::string_view kind = style.flora[pick % style.flora.size()];
                const int def = catalog.PropIndex(kind);
                const Vec2 point = World::TileCenter(bed.tile) + Vec2{script.Random().Range(-0.7f, 0.7f),
                                                                      script.Random().Range(-0.7f, 0.7f)};
                const float scale =
                    small ? script.Random().Range(0.6f, 0.8f) : script.Random().Range(0.8f, 1.25f);
                const float heading = script.Random().Range(0.0f, kTwoPi);
                bool open = false;
                for (const Vec2 from : site.watched) {
                    open = open || DistanceToLine(point, from, site.middle) < site.sight;
                }
                if (def >= 0 && catalog.props[static_cast<std::size_t>(def)].role == PropRole::Tree) {
                    const bool massive =
                        catalog.props[static_cast<std::size_t>(def)].radiusM >= kMassiveTrunkM;
                    for (const auto& [from, standOff] : site.fronts) {
                        open = open || Distance(point, from) <
                                           kTreeFromFrontM + (massive || setBack ? standOff : 0.0f);
                    }
                }
                if (def < 0 || open ||
                    !Fits(zoo, builder, site, def, point, scale,
                          small   ? 0.4f
                          : tight ? 0.8f
                                  : 1.8f,
                          spacing)) {
                    continue;
                }
                if (const PropId id = script.Prop(kind, point, heading, scale)) {
                    placed.push_back(id);
                }
            }
        }
        return placed;
    };
    // Where the set-back would cross the residents' welfare band, take this attempt up and plant
    // again with only massive trunks kept behind the strip. This preserves cover in the two small
    // habitats with long fronts (Rhino Hollow and Red Dune) without hard-coding their names.
    const std::vector<PropId> setBack = plant(true);
    if (density() < target - kCoverBand + kCoverMargin) {
        Builder remover = zoo.FreeBuilder();
        for (const PropId id : setBack) {
            if (remover.RemoveProp(id).Ok()) {
                --script.Report().props;
            }
        }
        plant(false);
    }
    // Short of the aim is a problem only where the residents would feel it (Animals.cpp: welfare
    // counts a habitat as too bare beyond 0.15 under the species' figure).
    if (density() < target - kCoverBand) {
        script.Problem(std::string(plan.name) + ": planting target not met (" + std::to_string(density()) +
                       " of " + std::to_string(wanted) + ")");
    }
}

bool Omitted(const VisitorZooOptions& options, std::string_view species) {
    return std::find(options.withoutSpecies.begin(), options.withoutSpecies.end(), species) !=
           options.withoutSpecies.end();
}

/// The species a habitat is furnished for, whether or not this visit shows them.
std::vector<std::string_view> Residents(const HabitatPlan& plan) {
    if (plan.species == "giraffe") {
        return {"giraffe", "zebra"};
    }
    return {plan.species};
}

// Feeders, troughs, rocks and the first things to do. Food and water stand in the front half, where
// the benches look at them, two tiles and more inside the barrier and spread over a fan instead of
// a row; rocks crop out together on the rise and at the pool's edge.
void Furnish(Zoo& zoo, Script& script, const Site& site, const HabitatStyle& style, float poolRadius) {
    const HabitatPlan& plan = *site.plan;
    const bool small = plan.rx <= 5;
    // Auto-care visits at 07:00 and 15:00. Stock at least eighteen hours of food and water so that
    // large herds are not dependent on one small catalog feeder.
    std::vector<std::string_view> table;
    float water = 0.0f;
    for (const std::string_view resident : Residents(plan)) {
        const int index = zoo.SpeciesList().IndexOf(resident);
        if (index < 0) {
            continue;
        }
        const SpeciesDef& species = zoo.SpeciesList().All()[static_cast<std::size_t>(index)];
        const float eaters = Eaters(species);
        water += species.waterPerDayL * eaters;
        // Zebras eat from racks and troughs, not from the giraffes' browse baskets.
        const std::string_view feeder = resident == plan.species ? style.feeder : "hay_rack";
        const int def = zoo.Definitions().PropIndex(feeder);
        if (def < 0) {
            script.Problem("unknown prop " + std::string(feeder));
            continue;
        }
        const float capacity = zoo.Definitions().props[static_cast<std::size_t>(def)].capacity;
        const int feeders = std::max(
            2, static_cast<int>(std::ceil(species.foodPerDayKg * eaters * 0.75f / std::max(1.0f, capacity))));
        table.insert(table.end(), static_cast<std::size_t>(feeders), feeder);
    }
    if (plan.species == "giraffe" || plan.species == "elephant") {
        table.push_back("pellet_fruit_trough");
    }
    const int troughs = std::max(1, static_cast<int>(std::ceil(water * 0.75f / 180.0f)));
    const int places = static_cast<int>(table.size()) + troughs;
    // Where water fills the front (the lagoon), the nearest dry place can lie behind the middle,
    // out of the benches' view: the front half is searched first.
    Site front = site;
    std::erase_if(front.tiles, [&site](TileCoord tile) { return World::TileCenter(tile).z < site.middle.z; });
    std::size_t next = 0;
    for (int i = 0; i < places; ++i) {
        const bool trough = (i + 1) * troughs / places > i * troughs / places;
        const float angle =
            kPi * (1.12f + 0.76f * (static_cast<float>(i) + 0.5f) / static_cast<float>(places));
        const float reach = (i % 2 == 0 ? 0.5f : 0.25f) + script.Random().Range(-0.04f, 0.04f);
        const Vec2 target = Share(site, std::cos(angle) * reach, std::sin(angle) * reach);
        const std::string_view kind = trough ? std::string_view("water_trough") : table[next];
        if (!Settle(zoo, script, front, kind, target, site.clear)) {
            Need(zoo, script, site, kind, target, site.clear);
        }
        next += trough ? 0U : 1U;
    }

    const Vec2 rise = Rise(script, plan);
    if (small) {
        // One outcrop leaves the small mound's drinking hollow and door approaches clear.
        Need(zoo, script, site, style.rock, rise + Vec2{-1.5f, -1.0f}, 0.3f, 0.9f);
    } else if (site.margin < 2) {
        // Where the water takes half the ground, three stones on the beach and not five.
        Need(zoo, script, site, style.rock, rise, 0.25f, 0.95f);
        if (plan.species != "flamingo") {
            Need(zoo, script, site, style.rock, rise + Vec2{2.4f, 0.8f}, 0.25f, 0.75f);
            Need(zoo, script, site, style.rock, PoolCentre(script, plan) + Vec2{-poolRadius - 1.5f, 3.0f},
                 0.25f, 0.8f);
        }
    } else {
        Need(zoo, script, site, style.rock, rise, 0.25f, 1.25f);
        Need(zoo, script, site, style.rock, rise + Vec2{2.9f, 0.9f}, 0.25f, 1.0f);
        Need(zoo, script, site, style.rock, rise + Vec2{1.0f, -2.7f}, 0.25f, 0.8f);
        const Vec2 pool = PoolCentre(script, plan);
        const float rim = poolRadius + 1.5f;
        Need(zoo, script, site, style.rock, pool + Vec2{-rim, rim * 0.4f}, 0.25f, 0.9f);
        Need(zoo, script, site, style.rock, pool + Vec2{rim * 0.5f, rim}, 0.25f, 0.75f);
    }
}

/// Where the i-th thing to do stands: round the middle, each a golden angle on from the last.
Vec2 PlayPlace(const Site& site, int i) {
    const float angle = 0.9f + static_cast<float>(i) * 2.399963f;
    const float reach = 0.3f + 0.12f * static_cast<float>(i % 3);
    return Share(site, std::cos(angle) * reach, std::sin(angle) * reach * 0.8f - 0.05f);
}

// Toys in turn from the species' own list until the group has what its playfulness asks for; plants
// and rocks count a little, so this comes after them.
void Enrich(Zoo& zoo, Script& script, const Site& site, const HabitatStyle& style) {
    float needed = 0.0f;
    for (const std::string_view resident : Residents(*site.plan)) {
        const int index = zoo.SpeciesList().IndexOf(resident);
        if (index >= 0) {
            const SpeciesDef& species = zoo.SpeciesList().All()[static_cast<std::size_t>(index)];
            needed = std::max(
                needed, species.playfulness * (0.5f + 0.25f * static_cast<float>(Family(resident).size())));
        }
    }
    const auto have = [&] { return zoo.Habitats().Find(site.id)->stats.enrichment; };
    // A log pile made for a bear dwarfs a bird or a meerkat: theirs are the smallest the catalog makes.
    const std::string_view lives = site.plan->species;
    const float size = lives == "meerkat" || lives == "flamingo" || lives == "penguin" ? 0.7f : 1.0f;
    // Where the ground is nearly full the last ones stand closer, and at last along the barrier.
    Site rim = site;
    rim.margin = std::min(site.margin, 1);
    for (int i = 0; i < 30 && (i < 2 || have() < needed); ++i) {
        const std::string_view toy = style.toys[static_cast<std::size_t>(i) % style.toys.size()];
        const Vec2 place = PlayPlace(site, i);
        if (!Settle(zoo, script, site, toy, place, site.clear * 0.75f, size) &&
            !Settle(zoo, script, rim, toy, place, 0.4f, size)) {
            rim.margin = 0;
            if (!Settle(zoo, script, rim, toy, place, 0.4f, size)) {
                break;
            }
        }
    }
    if (have() < needed * 0.95f) {
        script.Problem(std::string(site.plan->name) + ": too little to do");
    }
}

// Sets a family down: the adults anywhere on dry ground of their habitat, the young beside their
// mothers and known as theirs, so that they suckle and follow. A mother's last birth is her
// youngest's: she is not in calf again the day the zoo opens.
void Stock(Zoo& zoo, Script& script, const Site& site, std::string_view speciesId) {
    const int species = zoo.SpeciesList().IndexOf(speciesId);
    if (species < 0) {
        script.Problem("unknown species " + std::string(speciesId));
        return;
    }
    const SpeciesDef& def = zoo.SpeciesList().All()[static_cast<std::size_t>(species)];
    const Env env = zoo.MakeEnv();
    Rng& random = script.Random();
    std::vector<PropId> near;
    // Birds of the shallows may be set down standing in them.
    const float wade = def.waterAreaFraction >= 0.4f ? 0.25f * def.shoulderHeightM : 0.0f;
    const auto free = [&](Vec2 point) {
        const TileCoord tile = World::TileAt(point);
        if (zoo.Habitats().At(tile) != site.id ||
            (zoo.Map().HasWater(tile) && zoo.Map().WaterDepthAt(point) > wade)) {
            return false;
        }
        zoo.Objects().PropsNear(point, 3.0f, near);
        for (const PropId id : near) {
            const Prop* prop = zoo.Objects().FindProp(id);
            const PropDef& kind = zoo.Definitions().props[static_cast<std::size_t>(prop->def)];
            if (kind.blocksWalking && Distance(point, prop->position) < kind.radiusM * prop->scale + 0.5f) {
                return false;
            }
        }
        return true;
    };
    std::vector<AnimalId> family;
    for (const Resident& resident : Family(speciesId)) {
        const AnimalId dam =
            resident.mother >= 0 ? family[static_cast<std::size_t>(resident.mother)] : AnimalId{};
        const Animal* mother = dam ? zoo.Fauna().Find(dam) : nullptr;
        // By value: the next animal added moves the others in memory.
        const Vec2 beside = mother != nullptr ? mother->position : Vec2{};
        AnimalId id;
        std::optional<Vec2> wading;
        for (int attempt = 0; attempt < 60 && !id; ++attempt) {
            Vec2 at;
            if (mother != nullptr) {
                const float angle = random.Range(0.0f, kTwoPi);
                const float reach = random.Range(1.0f, 2.2f);
                at = beside + Vec2{std::sin(angle) * reach, std::cos(angle) * reach};
            } else {
                const TileCoord tile = site.tiles[static_cast<std::size_t>(
                    random.RangeInt(0, static_cast<int>(site.tiles.size()) - 1))];
                if (!Interior(zoo, tile, site.id, 1)) {
                    continue;
                }
                at = World::TileCenter(tile) + Vec2{random.Range(-0.6f, 0.6f), random.Range(-0.6f, 0.6f)};
            }
            if (!free(at)) {
                continue;
            }
            if (zoo.Map().HasWater(World::TileAt(at)) && !family.empty() && family.front()) {
                // An animal enters the zoo on dry ground: beside the first of its flock, and
                // from there into the shallows.
                wading = at;
                at = zoo.Fauna().Find(family.front())->position;
            } else {
                wading.reset();
            }
            id = zoo.Fauna().Add(env, species, resident.sex, resident.age, at);
        }
        if (!id && mother != nullptr) {
            id = zoo.Fauna().Add(env, species, resident.sex, resident.age, beside);
        }
        family.push_back(id);
        if (!id) {
            script.Problem("no room to set down a " + def.commonName);
            continue;
        }
        ++script.Report().animals;
        if (wading) {
            Animal* wader = zoo.Fauna().Find(id);
            wader->position = *wading;
            wader->target = *wading;
        }
        if (dam) {
            Animal* young = zoo.Fauna().Find(id);
            young->mother = dam;
            young->father = zoo.Fauna().Find(family.front())->sex == Sex::Male ? family.front() : AnimalId{};
            Animal* parent = zoo.Fauna().Find(dam);
            parent->lastBirthTick = parent->lastBirthTick == 0
                                        ? young->bornTick
                                        : std::max(parent->lastBirthTick, young->bornTick);
        }
    }
}

// A planned, unpaved service apron joins the back walk to the keeper gate. Visitors use
// paved tiles only; staff can cross its dry stone surface. Reserve it before strip planting,
// and leave its mouth open so it remains public ground rather than a spurious enclosure.
void StandOffWithService(Zoo& zoo, Script& script, const Site& site, int depth) {
    const auto expanded = PublicOutline(script, *site.plan);
    const TileCoord inside = site.tiles.front();  // northmost row, same edge as KeeperGate
    std::set<std::pair<int, int>> yard;
    for (int z = inside.z - depth; z < inside.z; ++z) {
        for (int x = inside.x; x <= inside.x + 1; ++x) {
            yard.emplace(z, x);
        }
    }
    std::vector<TileCoord> strip;
    std::vector<TileCoord> apron;
    for (const TileCoord tile : expanded) {
        if (zoo.Habitats().At(tile) == site.id) {
            continue;
        }
        if (yard.contains({tile.z, tile.x})) {
            apron.push_back(tile);
        } else {
            strip.push_back(tile);
        }
    }
    Builder builder = zoo.FreeBuilder();
    const auto marked = builder.PlaceStandOff(strip);
    if (!marked.Ok() || marked.skipped > 0) {
        script.Problem(std::string(site.plan->name) + ": incomplete service strip");
    }
    builder.PaintTerrain(apron, Terrain::Rock);
    std::vector<EdgeRef> rails;
    // Work from what was actually placed, not the expanded outline: an irregular habitat can
    // leave an apron touching the strip inside that outline. Every such public-to-strip edge
    // needs the low rail, while the apron remains open to ordinary public ground.
    for (const TileCoord tile : strip) {
        if (!zoo.Map().At(tile).standOff) {
            continue;
        }
        for (const Dir side : {Dir::North, Dir::East, Dir::South, Dir::West}) {
            const TileCoord next = tile + Step(side);
            if (zoo.Map().InBounds(next) && zoo.Habitats().RegionAt(next) == Region::Public) {
                const EdgeRef edge = EdgeOf(tile, side);
                if (!zoo.Map().FenceAt(edge).Exists()) {
                    rails.push_back(edge);
                }
            }
        }
    }
    const auto fenced = builder.PlaceFence(rails, script.FenceKind("stand_off_rail"));
    if (!fenced.Ok() || fenced.skipped > 0) {
        script.Problem(std::string(site.plan->name) + ": incomplete service rails");
    }
}

void Habitat(Zoo& zoo, Script& script, const HabitatPlan& plan, const VisitorZooOptions& options) {
    Site site;
    site.plan = &plan;
    site.tiles = RegionTiles(script, plan);
    const HabitatStyle style = Style(plan.species);
    const int speciesIndex = zoo.SpeciesList().IndexOf(plan.species);
    if (speciesIndex < 0) {
        script.Problem("unknown species " + std::string(plan.species));
        return;
    }
    const SpeciesDef& species = zoo.SpeciesList().All()[static_cast<std::size_t>(speciesIndex)];
    for (const std::string_view resident : Residents(plan)) {
        const int index = zoo.SpeciesList().IndexOf(resident);
        if (index >= 0) {
            site.corridor =
                std::max(site.corridor,
                         1.0f + 1.2f * zoo.SpeciesList().All()[static_cast<std::size_t>(index)].bodyRadiusM);
        }
    }
    Builder builder = zoo.FreeBuilder();
    builder.PaintTerrain(site.tiles, style.ground);
    site.id = script.EncloseTiles(site.tiles, style.fence, std::string(plan.name));
    if (!site.id) {
        return;
    }
    const int stripDepth = StandOffDepth(plan.species);
    if (stripDepth > 0) {
        // Visitors also use the back circuit: the same clearance protects that walk.
        // Keep every habitat tile and all its provisions, moving the public edge out.
        StandOffWithService(zoo, script, site, stripDepth);
    } else {
        script.ViewingFront(site.tiles, style.front, {Dir::South, plan.east < 0 ? Dir::East : Dir::West});
    }
    script.KeeperGate(site.tiles, style.fence);
    const bool small = plan.rx <= 5;
    if (small || species.waterAreaFraction >= 0.3f) {
        // Little dry ground: on a sand mound or beside a lagoon things stand closer.
        site.margin = 1;
        site.clear = small ? 0.5f : 1.0f;
        site.sight = small ? 2.0f : 3.0f;
    }
    for (const TileCoord tile : site.tiles) {
        site.middle = site.middle + World::TileCenter(tile);
    }
    site.middle = site.middle * (1.0f / static_cast<float>(site.tiles.size()));
    const auto keep = [&site](TileCoord centre, int aside, int ahead) {
        for (const TileCoord tile :
             TilesInRect({centre.x - aside, centre.z}, {centre.x + aside, centre.z + ahead})) {
            site.kept.emplace(tile.z, tile.x);
        }
    };
    if (const auto door = Shelter(zoo, script, plan, site.tiles, site.id, style.shelter)) {
        keep(*door, small ? 0 : 1, small ? 0 : 1);
    }
    for (const EdgeRef& gate : zoo.Habitats().Find(site.id)->gates) {
        for (const TileCoord tile :
             {TileCoord{gate.x, gate.z},
              gate.horizontal ? TileCoord{gate.x, gate.z - 1} : TileCoord{gate.x - 1, gate.z}}) {
            if (zoo.Habitats().At(tile) == site.id) {
                keep({tile.x, tile.z - 1}, 1, 2);
            }
        }
    }
    const float poolRadius = HabitatPool(zoo, script, site, species);
    for (const ViewingSpot& spot : Lookouts(*zoo.Habitats().Find(site.id))) {
        site.watched.push_back(World::TileCenter(spot.tile));
    }
    for (const ViewingSpot& spot : zoo.Habitats().Find(site.id)->viewing) {
        site.fronts.emplace_back(World::TileCenter(spot.tile), spot.standOffM);
    }
    Furnish(zoo, script, site, style, poolRadius);
    HabitatPlants(zoo, script, site, style, species.vegetationDensity);
    Enrich(zoo, script, site, style);
    for (const std::string_view resident : Residents(plan)) {
        if (!Omitted(options, resident)) {
            Stock(zoo, script, site, resident);
        }
    }
}

void StripPlants(Zoo& zoo, Script& script) {
    // Plant after the animal groups so decorating the public edge consumes none of
    // the script random sequence used for residents or habitat vegetation.
    for (const HabitatPlan& plan : kHabitats) {
        const int depth = StandOffDepth(plan.species);
        if (depth == 0) {
            continue;
        }
        const HabitatStyle style = Style(plan.species);
        for (const TileCoord tile : PublicOutline(script, plan)) {
            if (!zoo.Map().At(tile).standOff) {
                continue;
            }
            const Vec2 offset{script.Random().Range(-0.2f, 0.2f), script.Random().Range(-0.2f, 0.2f)};
            // The catalog's minimum bush scale is 0.7: 1.26 m crowns, below an
            // adult visitor's sightline. Sparse small rock accents stand 1.05 m high.
            const bool rock = (tile.x + 3 * tile.z) % 11 == 0;
            const std::string_view kind = rock ? style.rock : style.bush;
            const Vec2 point = World::TileCenter(tile) + (rock ? Vec2{} : offset);
            if (zoo.FreeBuilder().CheckProp(zoo.Definitions().PropIndex(kind), point) == BuildError::None) {
                script.Prop(kind, point, script.Random().Range(0.0f, kTwoPi), 0.7f);
            } else {
                script.Prop(style.bush, point, script.Random().Range(0.0f, kTwoPi), 0.7f);
            }
        }
    }
}

struct ServiceApron {
    TileCoord low;
    TileCoord high;
};

void Service(Zoo& zoo, Script& script, std::vector<ServiceApron>& wideAprons, std::string_view kind, int east,
             int north, Dir facing) {
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
        // Remember a two-tile-wide way straight out of the door, but lay it only after every
        // facility has selected its site. Otherwise an early widened apron displaces a later kiosk.
        const TileCoord ahead = Step(facing);
        TileCoord joins = *path;
        for (int step = 0; step <= 8; ++step) {
            const TileCoord tile{front.x + ahead.x * step, front.z + ahead.z * step};
            if (!zoo.Map().InBounds(tile) || zoo.Habitats().RegionAt(tile) != Region::Public ||
                zoo.Map().At(tile).building) {
                break;
            }
            if (zoo.Map().At(tile).path != PathKind::None) {
                joins = tile;
                break;
            }
            if (builder.CheckPath(tile, PathKind::Cobblestone) != BuildError::None) {
                break;
            }
        }
        TileCoord low{std::min(front.x, joins.x), std::min(front.z, joins.z)};
        TileCoord high{std::max(front.x, joins.x), std::max(front.z, joins.z)};
        const auto widen = [](int& from, int& to, int towards) {
            if (from == to) {
                (towards > 0 ? to : from) += towards;
            }
        };
        if (ahead.x == 0) {
            widen(low.z, high.z, ahead.z);
            widen(low.x, high.x, front.x < far.x ? 1 : -1);
        } else {
            widen(low.x, high.x, ahead.x);
            widen(low.z, high.z, front.z < far.z ? 1 : -1);
        }
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
        wideAprons.push_back({low, high});
        if (!zoo.Routes().Reachable(gate, front, NavProfile::Visitor())) {
            script.Problem(std::string(kind) + ": door is unreachable");
        }
        return;
    }
    script.Problem("no public site for " + std::string(kind) + " at " + std::to_string(east) + "," +
                   std::to_string(north));
}

void WidenServiceAprons(Zoo& zoo, Script& script, const std::vector<ServiceApron>& aprons) {
    Builder builder = zoo.FreeBuilder();
    std::vector<TileCoord> paving;
    for (const ServiceApron& apron : aprons) {
        for (const TileCoord tile : TilesInRect(apron.low, apron.high)) {
            if (zoo.Habitats().RegionAt(tile) == Region::Public &&
                zoo.Map().At(tile).path == PathKind::None && !zoo.Map().At(tile).building &&
                builder.CheckPath(tile, PathKind::Cobblestone) == BuildError::None) {
                paving.push_back(tile);
            }
        }
    }
    script.PathTiles(paving, PathKind::Cobblestone);
}

bool PublicProp(Zoo& zoo, Script& script, std::string_view kind, Vec2 position, float heading = 0.0f,
                float scale = 1.0f) {
    return zoo.FreeBuilder().CheckProp(zoo.Definitions().PropIndex(kind), position) == BuildError::None &&
           script.Prop(kind, position, heading, scale);
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

/// Public lawn a walk may be widened onto: nothing built, planted or standing there, and no
/// barrier or strip beside it, so the paving cannot open a new viewing place.
bool OpenLawn(Zoo& zoo, TileCoord tile) {
    const World& map = zoo.Map();
    if (!map.InBounds(tile) || zoo.Habitats().RegionAt(tile) != Region::Public || map.At(tile).building ||
        map.At(tile).standOff || map.HasWater(tile) ||
        zoo.FreeBuilder().CheckPath(tile, PathKind::Gravel) != BuildError::None) {
        return false;
    }
    for (const Dir side : {Dir::North, Dir::East, Dir::South, Dir::West}) {
        const TileCoord next = tile + Step(side);
        if (!map.InBounds(next) || zoo.Habitats().RegionAt(next) != Region::Public || map.At(next).standOff) {
            return false;
        }
    }
    std::vector<PropId> near;
    zoo.Objects().PropsNear(World::TileCenter(tile), 1.5f, near);
    return near.empty();
}

// A bench in the sun is not sat on for long. A broad crown goes near each viewing bench: on the
// nearest lawn behind it, or, where a paved bay lies there, on an island left in the paving as on
// the picnic lawn (only where paving stays all round it, so that no walk is narrowed). A walk that
// runs between two habitats' planted strips has neither, and its bench stays in the sun: what grows
// on a strip stays below the eye.
void ShadeTree(Zoo& zoo, Script& script, const ViewingSpot& spot) {
    const World& map = zoo.Map();
    const TileCoord step = Step(spot.side);
    const TileCoord gate = map.EntranceTile();
    const auto at = [&](int depth, int aside) {
        return TileCoord{spot.tile.x - step.x * depth + step.z * aside,
                         spot.tile.z - step.z * depth - step.x * aside};
    };
    const auto crown = [&](TileCoord tile) {
        std::string_view kind = ParkTree(tile.x - gate.x, gate.z - tile.z, 1);
        const int def = zoo.Definitions().PropIndex(kind);
        if (def < 0 || zoo.Definitions().props[static_cast<std::size_t>(def)].shadeM2 < 30.0f) {
            kind = gate.z - tile.z > 100 ? "oak" : "acacia";
        }
        return kind;
    };
    // Nearest first, and never farther than a crown throws its shade.
    std::vector<std::pair<int, int>> places;
    for (int depth = 1; depth <= 3; ++depth) {
        for (const int aside : {0, -1, 1, -2, 2, -3, 3}) {
            if (depth * depth + aside * aside <= 13) {
                places.emplace_back(depth, aside);
            }
        }
    }
    std::ranges::stable_sort(places, {}, [](const std::pair<int, int>& place) {
        return place.first * place.first + place.second * place.second;
    });
    for (const auto& [depth, aside] : places) {
        const TileCoord tile = at(depth, aside);
        if (!map.InBounds(tile) || zoo.Habitats().RegionAt(tile) != Region::Public || map.At(tile).building ||
            map.At(tile).standOff || map.HasWater(tile)) {
            continue;
        }
        const Vec2 point = World::TileCenter(tile);
        if (map.At(tile).path != PathKind::None) {
            // Keep two tiles of paving around the tree island. Lay a second row on open lawn where
            // possible; otherwise leave this candidate paved and try another shade-tree place.
            bool paved = depth >= 2 && aside == 0;
            std::vector<TileCoord> ring;
            for (int dz = -2; dz <= 2 && paved; ++dz) {
                for (int dx = -2; dx <= 2 && paved; ++dx) {
                    const TileCoord next{tile.x + dx, tile.z + dz};
                    const bool corner = std::abs(dx) == 2 && std::abs(dz) == 2;
                    if (!map.InBounds(next) || (dx == 0 && dz == 0) || corner) {
                        paved = paved && map.InBounds(next);
                        continue;
                    }
                    if (map.At(next).path != PathKind::None) {
                        continue;
                    }
                    paved = std::abs(dx) == 2 || std::abs(dz) == 2;
                    paved = paved && OpenLawn(zoo, next);
                    ring.push_back(next);
                }
            }
            std::vector<PropId> near;
            zoo.Objects().PropsNear(point, 1.8f, near);
            if (!paved || !near.empty()) {
                continue;
            }
            script.PathTiles(ring, map.At(tile).path);
            script.PathTiles({tile}, PathKind::None);
        }
        if (PublicProp(zoo, script, crown(tile), point, script.Random().Range(0.0f, kTwoPi), 1.15f)) {
            return;
        }
    }
}

void ViewingFurniture(Zoo& zoo, Script& script) {
    // Construction refreshes enclosure storage, so keep a value snapshot of the viewing places.
    const auto habitats = zoo.Habitats().All();
    for (const Enclosure& habitat : habitats) {
        int sideNumber = 0;
        for (const ViewingSpot& spot : Lookouts(habitat)) {
            const Vec2 toward = Direction(spot.side);
            const Vec2 point = World::TileCenter(spot.tile);
            const float heading = std::atan2(toward.x, toward.z);
            const Vec2 along{toward.z, -toward.x};
            script.Prop("bench", point - toward * 0.55f, heading);
            // The lamp stands at the end of the bench, not between the sitter and the animals.
            script.Prop("lamp", point - along * 0.9f - toward * 0.55f);
            if (sideNumber == 0) {
                script.Prop("info_board", point + along * 0.7f, heading + kPi);
            } else {
                script.Prop("litter_bin", point + along * 0.7f, heading);
            }
            ShadeTree(zoo, script, spot);
            ++sideNumber;
        }
    }
}

// Rasterised curves leave single paved teeth at their edges. A paving tile that leads nowhere
// reads as a 2 m stub; tiles beside a barrier, strip or building remain as viewing places or doors.
void TrimTeeth(Zoo& zoo, Script& script) {
    const World& map = zoo.Map();
    const TileCoord gate = map.EntranceTile();
    std::set<std::pair<int, int>> doors;
    for (const Building& building : zoo.Objects().Buildings()) {
        const BuildingDef& def = zoo.Definitions().buildings[static_cast<std::size_t>(building.def)];
        const TileCoord door = FrontTile(def, building.origin, building.facing);
        doors.emplace(door.z, door.x);
    }
    for (int round = 0; round < 3; ++round) {
        std::vector<TileCoord> teeth;
        for (int z = map.PropertyMin().z; z <= map.PropertyMax().z; ++z) {
            for (int x = map.PropertyMin().x; x <= map.PropertyMax().x; ++x) {
                const TileCoord tile{x, z};
                if (map.At(tile).path == PathKind::None || tile == gate || doors.contains({tile.z, tile.x})) {
                    continue;
                }
                int paved = 0;
                bool kept = false;
                for (const Dir side : {Dir::North, Dir::East, Dir::South, Dir::West}) {
                    const TileCoord next = tile + Step(side);
                    if (!map.InBounds(next) || next == gate || map.At(next).standOff ||
                        zoo.Habitats().RegionAt(next) != Region::Public) {
                        kept = true;
                        break;
                    }
                    paved += map.At(next).path != PathKind::None ? 1 : 0;
                }
                if (!kept && paved <= 1) {
                    teeth.push_back(tile);
                }
            }
        }
        if (teeth.empty()) {
            return;
        }
        script.PathTiles(teeth, PathKind::None);
    }
}

// Widen a one-tile corridor along its whole run when a parallel row is unobstructed public lawn.
// OpenLawn rejects strips and habitat edges, so constrained viewing teeth remain where widening
// would create a new front or require moving an enclosure.
void WidenThinWalks(Zoo& zoo, Script& script) {
    const World& map = zoo.Map();
    const auto paved = [&map](TileCoord tile) {
        return map.InBounds(tile) && map.At(tile).path != PathKind::None;
    };
    for (int z = map.PropertyMin().z; z <= map.PropertyMax().z; ++z) {
        for (int x = map.PropertyMin().x; x <= map.PropertyMax().x; ++x) {
            const TileCoord tile{x, z};
            if (!paved(tile) || map.At(tile).path == PathKind::Boardwalk) {
                continue;
            }
            const bool horizontal = paved({x - 1, z}) || paved({x + 1, z});
            const bool vertical = paved({x, z - 1}) || paved({x, z + 1});
            if (horizontal == vertical) {
                continue;
            }
            std::vector<TileCoord> run{tile};
            if (vertical) {
                for (int dz : {-1, 1}) {
                    for (int step = 1;; ++step) {
                        const TileCoord along{x, z + dz * step};
                        if (!paved(along) || paved({along.x - 1, along.z}) || paved({along.x + 1, along.z})) {
                            break;
                        }
                        run.push_back(along);
                    }
                }
            } else {
                for (int dx : {-1, 1}) {
                    for (int step = 1;; ++step) {
                        const TileCoord along{x + dx * step, z};
                        if (!paved(along) || paved({along.x, along.z - 1}) || paved({along.x, along.z + 1})) {
                            break;
                        }
                        run.push_back(along);
                    }
                }
            }
            for (const int side : {-1, 1}) {
                std::vector<TileCoord> row;
                for (const TileCoord along : run) {
                    row.push_back(vertical ? TileCoord{along.x + side, along.z}
                                           : TileCoord{along.x, along.z + side});
                }
                if (std::ranges::all_of(row,
                                        [&zoo](TileCoord candidate) { return OpenLawn(zoo, candidate); })) {
                    script.PathTiles(row, map.At(tile).path);
                    break;
                }
            }
        }
    }
}

void Facilities(Zoo& zoo, Script& script) {
    std::vector<ServiceApron> serviceAprons;
    script.PathTiles(script.Ellipse(13, 74, 9.0f, 4.0f), PathKind::Cobblestone);
    // Trunks stand on unpaved islands; the broad crowns shade the two rows of tables.
    for (int e = 6; e <= 21; e += 3) {
        script.PathTiles({script.Tile(e, 74)}, PathKind::None);
        script.Prop("acacia", script.Point(e, 74));
    }
    TrimTeeth(zoo, script);
    Service(zoo, script, serviceAprons, "gift_shop", 12, 2, Dir::West);
    Service(zoo, script, serviceAprons, "restroom", -15, 2, Dir::East);
    Service(zoo, script, serviceAprons, "drinks_stall", 14, 7, Dir::West);
    Service(zoo, script, serviceAprons, "ice_cream_stand", -16, 8, Dir::East);
    Service(zoo, script, serviceAprons, "education_centre", -20, 15, Dir::South);
    Service(zoo, script, serviceAprons, "burger_stall", 6, 79, Dir::South);
    Service(zoo, script, serviceAprons, "drinks_stall", 11, 80, Dir::South);
    Service(zoo, script, serviceAprons, "ice_cream_stand", 16, 79, Dir::South);
    Service(zoo, script, serviceAprons, "restroom", 21, 78, Dir::South);
    Service(zoo, script, serviceAprons, "burger_stall", 0, 108, Dir::South);
    Service(zoo, script, serviceAprons, "drinks_stall", 5, 110, Dir::South);
    Service(zoo, script, serviceAprons, "restroom", 10, 110, Dir::South);
    Service(zoo, script, serviceAprons, "drinks_stall", -25, 74, Dir::South);
    Service(zoo, script, serviceAprons, "ice_cream_stand", -25, 99, Dir::South);
    Service(zoo, script, serviceAprons, "drinks_stall", 31, 35, Dir::South);
    // The new planting occupies the old narrow kiosk gap beside Elephant Valley.
    // Put its refreshments at the southern Kangaroo/Ostrich approach instead.
    Service(zoo, script, serviceAprons, "ice_cream_stand", -5, 18, Dir::North);
    Service(zoo, script, serviceAprons, "drinks_stall", 31, 94, Dir::East);
    Service(zoo, script, serviceAprons, "ice_cream_stand", 32, 139, Dir::South);
    Service(zoo, script, serviceAprons, "drinks_stall", -4, 56, Dir::South);
    Service(zoo, script, serviceAprons, "drinks_stall", -72, 52, Dir::East);
    Service(zoo, script, serviceAprons, "drinks_stall", 70, 52, Dir::West);
    Service(zoo, script, serviceAprons, "drinks_stall", -70, 115, Dir::East);
    Service(zoo, script, serviceAprons, "ice_cream_stand", -69, 17, Dir::East);
    Service(zoo, script, serviceAprons, "drinks_stall", -10, 144, Dir::South);
    Service(zoo, script, serviceAprons, "drinks_stall", 66, 123, Dir::West);
    Service(zoo, script, serviceAprons, "drinks_stall", 66, 91, Dir::West);
    Service(zoo, script, serviceAprons, "ice_cream_stand", -67, 84, Dir::East);
    WidenServiceAprons(zoo, script, serviceAprons);
    WidenThinWalks(zoo, script);
    TrimTeeth(zoo, script);
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
        // A low dry rise behind the front; the pool on the other lobe supplies the hollow. The
        // meerkats' is the mound their sentries stand on.
        builder.Terraform(Builder::Brush::Raise, Rise(script, plan), plan.rx > 5 ? 5.0f : 4.5f,
                          plan.rx > 5 ? 0.55f : 0.7f);
    }
}

// Lawn trees stand in groves with open grass between them, each at its own spot and size: the old
// planting set one on every third tile each way, which read from the walks as an orchard.
void Parkland(Zoo& zoo, Script& script) {
    const World& map = zoo.Map();
    const TileCoord gate = map.EntranceTile();
    const std::uint32_t salt = HashU32(static_cast<std::uint32_t>(zoo.Config().seed), 0x9a4b1d07u);
    std::vector<PropId> near;
    for (int z = map.PropertyMin().z + 1; z < map.PropertyMax().z; ++z) {
        for (int x = map.PropertyMin().x + 1; x < map.PropertyMax().x; ++x) {
            const TileCoord tile{x, z};
            if (zoo.Habitats().RegionAt(tile) != Region::Public || map.At(tile).building ||
                map.At(tile).standOff || map.HasWater(tile) || map.At(tile).terrain == Terrain::Rock ||
                BesidePath(map, tile, 1)) {
                continue;
            }
            const float grove = Grove(tile, salt);
            if (!script.Random().Chance(grove > 0.62f ? 0.36f : grove > 0.47f ? 0.1f : 0.02f)) {
                continue;
            }
            const int variant = script.Random().RangeInt(0, 2);
            const Vec2 point = World::TileCenter(tile) +
                               Vec2{script.Random().Range(-0.8f, 0.8f), script.Random().Range(-0.8f, 0.8f)};
            const float heading = script.Random().Range(0.0f, kTwoPi);
            const float scale = script.Random().Range(0.85f, 1.25f);
            zoo.Objects().PropsNear(point, 3.4f, near);
            bool crowded = false;
            for (const PropId id : near) {
                const Prop* other = zoo.Objects().FindProp(id);
                crowded = crowded || zoo.Definitions().props[static_cast<std::size_t>(other->def)].role ==
                                         PropRole::Tree;
            }
            if (!crowded) {
                PublicProp(zoo, script, ParkTree(x - gate.x, gate.z - z, variant), point, heading, scale);
            }
        }
    }
    for (int z = map.PropertyMin().z + 1; z < map.PropertyMax().z; ++z) {
        for (int x = map.PropertyMin().x + 1; x < map.PropertyMax().x; ++x) {
            const TileCoord tile{x, z};
            if ((x + 5 * z) % 19 != 0 || zoo.Habitats().RegionAt(tile) != Region::Public ||
                map.At(tile).path != PathKind::None || map.At(tile).building || map.HasWater(tile) ||
                map.At(tile).terrain == Terrain::Rock || !BesidePath(map, tile, 1)) {
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
    StripPlants(zoo, script);
    Facilities(zoo, script);
    GroundDetail(zoo, script);
    Parkland(zoo, script);
    script.Release();
    zoo.Guests().SetTicketPrice(0);
    script.Provision();
    // The crew a visitor sees at work (docs/KEEPERS.md). Every keeper gate opens onto a dry
    // service apron joined to the walks (StandOffWithService), so no lane is cut for them.
    zoo.Keepers().Engage(zoo.MakeEnv(), zoo.Fauna());
    return script.Report();
}
}  // namespace zoo
