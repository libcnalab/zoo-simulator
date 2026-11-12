// SPDX-License-Identifier: MIT
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "zoo/base/Money.hpp"
#include "zoo/world/Terrain.hpp"

namespace zoo {

/// Linear RGB, 0..1.
using Color3 = std::array<float, 3>;

// ---- fences ---------------------------------------------------------------------------

/// How a fence is built and drawn.
enum class FenceStyle : std::uint8_t {
    Rail,       ///< wooden posts and horizontal rails
    Mesh,       ///< posts with wire mesh
    Bars,       ///< vertical steel bars
    Glass,      ///< glass panels between posts
    Wall,       ///< solid wall
    Cable,      ///< heavy posts with steel cables
    Hedge,      ///< clipped hedge (decoration)
    Count
};
std::string_view ToString(FenceStyle v);    // "rail", "mesh", "bars", "glass", "wall", "cable", "hedge"

struct FenceDef {
    std::string id;             ///< "wire_mesh"
    std::string name;           ///< "Wire mesh fence"
    /// The value stored in World::Fence::kind, 1..254. Part of the save format: never
    /// renumber a published fence.
    int kind = 0;
    FenceStyle style = FenceStyle::Mesh;
    float heightM = 2.0f;
    int strength = 1;           ///< 1 (light) .. 5 (elephant-proof), see SpeciesDef::fenceStrength
    /// How well visitors see animals through it, 0 (solid wall) .. 1 (glass).
    float visibility = 0.8f;
    Money cost = 0;             ///< per tile edge (2 m)
    Money gateCost = 0;         ///< added for an edge that carries a gate
    Money upkeepPerDay = 0;     ///< per edge and game day
    /// Condition lost per game day (1 = new, 0 = collapsed); mechanics repair it.
    float wearPerDay = 0.002f;
    Color3 color{0.5f, 0.5f, 0.5f};
    Color3 postColor{0.3f, 0.3f, 0.3f};
};

// ---- paths and ground -----------------------------------------------------------------

struct PathDef {
    PathKind kind = PathKind::None;
    std::string name;           ///< "Asphalt path"
    Money cost = 0;             ///< per tile
    /// Walking speed factor of visitors on it (1 = normal).
    float speed = 1.0f;
    /// How pleasant visitors find it, -1..1.
    float appeal = 0.0f;
};

struct TerrainDef {
    Terrain terrain = Terrain::Grass;
    std::string name;           ///< "Savanna grass"
    Money cost = 0;             ///< per tile repainted
};

// ---- food -----------------------------------------------------------------------------

/// Animal food (SpeciesDef::foods names these ids).
struct FoodDef {
    std::string id;             ///< "meat"
    std::string name;           ///< "Meat"
    Money costPerKg = 0;
    /// Game days after which it spoils in a feeder.
    float shelfLifeDays = 1.0f;
    Color3 color{0.5f, 0.3f, 0.2f};
};

// ---- buildings ------------------------------------------------------------------------

/// What a building is for. Names are part of the save format; append only.
enum class BuildingRole : std::uint8_t {
    AnimalShelter,      ///< inside an enclosure: animals sleep and hide in it
    FoodStall,
    DrinkStall,
    Restroom,
    GiftShop,
    StaffRoom,          ///< staff rest here; extends how many staff the zoo can employ
    VetClinic,
    EducationCenter,    ///< visitors learn about the zoo's species
    ResearchCenter,     ///< conservation projects
    Count
};
std::string_view ToString(BuildingRole v);  // "animal_shelter", "food_stall", ...

/// Where something may stand.
enum class Placement : std::uint8_t {
    Anywhere,       ///< any property ground
    Enclosure,      ///< only inside an animal enclosure
    Public,         ///< only on public ground (reachable by visitors, outside enclosures)
};
std::string_view ToString(Placement v);     // "anywhere", "enclosure", "public"

struct BuildingDef {
    std::string id;             ///< "burger_stall"
    std::string name;           ///< "Burger stall"
    std::string description;    ///< one sentence for the build menu
    BuildingRole role = BuildingRole::FoodStall;
    Placement placement = Placement::Public;
    /// Footprint in tiles when the building faces south: `width` along x, `depth`
    /// along z, the door in the south side on the tile `doorOffset` from the west end.
    int width = 2;
    int depth = 2;
    int doorOffset = 0;
    float heightM = 3.0f;       ///< eaves height
    Money cost = 0;
    Money upkeepPerDay = 0;
    /// Animals sheltered / visitors inside or served at a time / staff based here.
    int capacity = 4;
    bool heated = false;        ///< shelters: keeps animals warm below their comfort range
    bool cooled = false;        ///< shelters: keeps animals cool above their comfort range

    // -- what a stall or shop sells (role FoodStall, DrinkStall, GiftShop) --
    std::string item;           ///< "Burger"; empty when nothing is sold
    Money itemPrice = 0;        ///< default selling price (the player may change it)
    Money itemCost = 0;         ///< what one item costs the zoo
    float serviceSeconds = 8.0f;    ///< motion seconds to serve one visitor
    float hungerRelief = 0.0f;  ///< how much of a visitor's hunger one item removes, 0..1
    float thirstRelief = 0.0f;
    float fun = 0.0f;           ///< happiness one item or one visit gives, 0..1
    float education = 0.0f;     ///< what a visit teaches, 0..1 (EducationCenter)

    // -- look (the renderer builds the model from these) --
    std::string look;           ///< "hut", "barn", "kiosk", "house", "pavilion", "cave"
    Color3 wallColor{0.8f, 0.75f, 0.65f};
    Color3 roofColor{0.5f, 0.25f, 0.2f};
};

// ---- props ----------------------------------------------------------------------------

/// What a freely placed object is for. Names are part of the save format; append only.
enum class PropRole : std::uint8_t {
    Tree,
    Bush,
    Rock,
    Feeder,         ///< holds animal food; keepers fill it
    WaterTrough,    ///< holds drinking water; keepers fill it
    Enrichment,     ///< something for animals to play with, climb or scratch
    Bench,
    Bin,            ///< litter bin
    Lamp,
    InfoBoard,      ///< education board about the species of the nearest enclosure
    Decoration,     ///< flower beds, statues, fountains
    Count
};
std::string_view ToString(PropRole v);      // "tree", "bush", "rock", "feeder", "water_trough", ...

struct PropDef {
    std::string id;             ///< "acacia"
    std::string name;           ///< "Umbrella acacia"
    PropRole role = PropRole::Tree;
    Placement placement = Placement::Anywhere;
    /// Radius of the circle it occupies on the ground; other blocking props keep
    /// this far away and walkers go around it.
    float radiusM = 0.5f;
    float heightM = 1.0f;
    bool blocksWalking = true;
    Money cost = 0;
    /// Plant cover it adds to its surroundings, in m2 (enclosure vegetation density).
    float vegetationM2 = 0.0f;
    /// Shade it casts, in m2.
    float shadeM2 = 0.0f;
    /// Biome tag for the build menu and for habitat scoring:
    /// "savanna", "temperate", "boreal", "desert", "tropical", "polar" or "" (neutral).
    std::string biome;
    /// Enrichment value for animals, 0..1 (Enrichment props, and logs, rocks, trees a little).
    float enrichment = 0.0f;
    /// Feeder: food ids it can hold. Empty for everything else.
    std::vector<std::string> foods;
    /// Feeder: kg of food; WaterTrough: litres; Bin: litter items; Bench: seats.
    float capacity = 0.0f;
    /// Feeder: height of the food above the ground (a giraffe's browse hangs high).
    float feedHeightM = 0.0f;
    /// Happiness a visitor gets from being near it, 0..1 (decoration, flowers).
    float beauty = 0.0f;

    // -- look --
    std::string look;           ///< "oak", "pine", "acacia", "palm", "boulder", "hay_rack", "bench", ...
    Color3 color{0.3f, 0.5f, 0.2f};
    Color3 color2{0.4f, 0.3f, 0.2f};
};

// ---- the catalog ----------------------------------------------------------------------

/// Everything the player can build, loaded from data/construction/*.json
/// (fences.json, paths.json, terrain.json, foods.json, buildings.json,
/// props.json). Indices into these vectors are stable for the lifetime of a
/// catalog and are what Building::def and Prop::def store; saved games store
/// the string ids.
struct Catalog {
    std::vector<FenceDef> fences;       ///< ordered by kind
    std::array<PathDef, kPathKindCount> paths{};
    std::array<TerrainDef, kTerrainCount> terrains{};
    std::vector<FoodDef> foods;
    std::vector<BuildingDef> buildings;
    std::vector<PropDef> props;
    std::vector<std::string> errors;

    /// Loads the six files from `directory`. Entries that fail validation are skipped
    /// and reported in `errors` ("fences.json: wire_mesh: height must be within 0.3..8"),
    /// as are unknown keys, duplicate ids, duplicate fence kinds, missing files and
    /// feeders that name unknown foods.
    static Catalog LoadDirectory(const std::string& directory);
    /// The same from JSON text per file, for tests; an empty text leaves that part empty.
    static Catalog FromJson(std::string_view fences, std::string_view paths, std::string_view terrains,
                            std::string_view foods, std::string_view buildings, std::string_view props);

    /// nullptr when `kind` is 0, kPerimeterFence or unknown.
    const FenceDef* FenceByKind(int kind) const;
    const FenceDef* FindFence(std::string_view id) const;
    /// Index in the vector or -1.
    int FoodIndex(std::string_view id) const;
    int BuildingIndex(std::string_view id) const;
    int PropIndex(std::string_view id) const;
    const PathDef& Path(PathKind kind) const { return paths[static_cast<std::size_t>(kind)]; }
    const TerrainDef& TerrainInfo(Terrain t) const { return terrains[static_cast<std::size_t>(t)]; }
};

/// Height, strength and visibility of the boundary wall (kPerimeterFence), which is
/// not in the catalog: it holds every species and visitors cannot see through it.
inline constexpr float kPerimeterHeightM = 4.0f;
inline constexpr int kPerimeterStrength = 5;

}  // namespace zoo
