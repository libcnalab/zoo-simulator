// SPDX-License-Identifier: MIT
#include "zoo/sim/VisitorZoo.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <set>
#include <span>
#include <tuple>
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

constexpr std::array<HabitatPlan, 118> kHabitats{{
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
    // The northern extension's first new habitat: outside the woodland loop, with a long inward
    // gallery and a quiet planted margin against the property wall.
    {"Lynx Ridge", "lynx", 58, 160, 12, 10},
    // A cool, densely planted canopy on the extension's quieter western arc. Its broad public
    // front leaves the picnic glade open while the back trees screen the property boundary.
    {"Red Panda Canopy", "red_panda", -44, 160, 10, 8},
    // The second northern stage ends in an open alpine bowl, beyond the woodland canopy but with
    // two independent approaches from its old crown.
    {"Snow Leopard Heights", "snow_leopard", 0, 196, 14, 10},
    // The two side gardens occupy the wider property. Earlier maps use the habitats above.
    {"Capybara Marsh", "capybara", -79, 84, 8, 10},
    {"Aldabra Meadow", "aldabra_tortoise", 79, 84, 8, 10},
    {"Corn Snake Terrarium", "corn_snake", 78, 123, 2, 3},
    {"Python Terrarium", "black_headed_python", 86, 123, 2, 3},
    {"Woodland Aviary", "mandarin_duck", -83, 127, 6, 6},
    {"Gobi Steppe", "przewalski_horse", 155, 84, 25, 18},
    {"Tibetan Plateau", "kiang", -155, 84, 23, 16},
    // The first enlargement is now a pair of complete visitor districts instead of two
    // paddocks standing alone at the ends of long service walks.
    {"Cheetah Run", "cheetah", 132, 32, 18, 14},
    {"Emu Meadow", "emu", 184, 32, 14, 12},
    {"Wombat Burrows", "common_wombat", 187, 135, 11, 9},
    {"Cassowary Rainforest", "southern_cassowary", 132, 151, 13, 12},
    {"Maned Wolf Pampas", "maned_wolf", 177, 184, 15, 12},
    {"Hippo Lake", "hippopotamus", -132, 32, 20, 16},
    {"Tapir Creek", "south_american_tapir", -184, 35, 14, 12},
    {"Prairie Dog Town", "black_tailed_prairie_dog", -187, 135, 12, 10},
    {"Peccary Brush", "collared_peccary", -132, 153, 14, 12},
    {"Puma Canyon", "puma", -177, 184, 14, 12},
    // The former three rigid rows are now a chain of smaller, irregular landscapes.  Their
    // centres and footprints deliberately vary; winding paths use the open seams between them.
    {"Bison Prairie", "american_bison", -169, 245, 20, 15},
    {"Reindeer Ridge", "reindeer", -102, 252, 18, 14},
    {"Arctic Fox Tundra", "arctic_fox", -37, 238, 13, 11},
    {"Hyena Basin", "spotted_hyena", 39, 254, 17, 13},
    {"Oryx Dunes", "scimitar_horned_oryx", 127, 241, 19, 14},
    {"Anteater Grassland", "giant_anteater", 180, 296, 16, 12},
    {"Lemur Island", "ring_tailed_lemur", 111, 321, 14, 12},
    {"Macaque Cliffs", "barbary_macaque", 28, 306, 16, 13},
    {"Guanaco Range", "guanaco", -60, 326, 19, 14},
    {"Addax Desert", "addax", -154, 310, 18, 13},
    {"Okapi Forest", "okapi", -174, 381, 17, 14},
    {"Red Deer Glade", "red_deer", -92, 394, 18, 14},
    {"Musk Ox Tundra", "musk_ox", 3, 376, 18, 14},
    {"European Bison Wood", "european_bison", 105, 396, 20, 15},
    // South-west: an Australian and island district.  The unequal footprints and staggered
    // centres follow the two bends of its walk instead of a rectangular exhibit grid.
    {"Outback Emu Range", "emu", -584, 51, 17, 12},
    {"Wombat Creek", "common_wombat", -519, 78, 14, 11},
    {"Red Kangaroo Plain", "kangaroo", -449, 51, 18, 13},
    {"Cassowary Gully", "southern_cassowary", -369, 84, 15, 13},
    {"Island Tortoise Garden", "aldabra_tortoise", -289, 56, 14, 11},
    {"Wetland Bird Reach", "mandarin_duck", -569, 151, 13, 12},
    {"Coastal Penguin Rocks", "penguin", -499, 166, 16, 12},
    {"Flamingo Billabong", "flamingo", -419, 184, 18, 13},
    {"Tasman Wombat Wood", "common_wombat", -349, 148, 13, 11},
    {"Forest Cassowary Walk", "southern_cassowary", -279, 176, 16, 13},
    // South-east: open African country. Uneven centres follow a savanna trail, its river
    // boardwalk and two predator links instead of two exact exhibit rows.
    {"Serengeti Giraffe Plain", "giraffe", 289, 50, 21, 16},
    {"Zebra Kopje", "zebra", 359, 34, 19, 14},
    {"Cheetah Grass Run", "cheetah", 429, 69, 17, 12},
    {"Hyena Moon Basin", "spotted_hyena", 509, 36, 16, 13},
    {"Oryx Sandveld", "scimitar_horned_oryx", 584, 58, 18, 13},
    {"Addax Dry Valley", "addax", 279, 158, 17, 12},
    {"Lion Savanna", "lion", 349, 174, 18, 14},
    {"Hippo River Bend", "hippopotamus", 424, 153, 19, 14},
    {"Okapi Forest Edge", "okapi", 504, 176, 16, 13},
    {"Anteater Termite Plain", "giant_anteater", 574, 150, 16, 12},
    // Mid-west: an American prairie trail and an oxbow boardwalk. Alternating centres follow the
    // meanders rather than repeating the two-row continental template.
    {"North Bison Range", "american_bison", -584, 248, 19, 14},
    {"Prairie Dog Colony", "black_tailed_prairie_dog", -519, 267, 13, 10},
    {"Puma Mesa", "puma", -449, 242, 16, 13},
    {"Peccary Scrub", "collared_peccary", -369, 255, 15, 12},
    {"Tapir Rain Creek", "south_american_tapir", -289, 260, 16, 13},
    {"Capybara Oxbow", "capybara", -569, 382, 17, 13},
    {"Maned Wolf Cerrado", "maned_wolf", -499, 365, 16, 12},
    {"Guanaco Foothills", "guanaco", -419, 390, 18, 13},
    {"Anteater Palm Savanna", "giant_anteater", -349, 365, 15, 12},
    {"Puma Forest Pass", "puma", -279, 386, 15, 12},
    // Mid-east: an Asian mountain switchback and monsoon boardwalk. Alternating centres follow
    // the slopes and river bends instead of two parallel exhibit rows.
    {"Kiang High Steppe", "kiang", 289, 250, 18, 13},
    {"Przewalski Valley", "przewalski_horse", 359, 270, 19, 14},
    {"Red Panda Bamboo Wood", "red_panda", 429, 244, 14, 11},
    {"Tiger River Forest", "tiger", 509, 270, 17, 14},
    {"Snow Leopard Pass", "snow_leopard", 584, 250, 15, 12},
    {"Macaque Temple Wood", "barbary_macaque", 279, 382, 16, 13},
    {"Asian Elephant Forest", "elephant", 349, 365, 20, 15},
    {"Rhino Monsoon Plain", "rhino", 424, 392, 18, 14},
    {"Tiger Bamboo Ravine", "tiger", 504, 365, 16, 13},
    {"Red Panda Cedar Hill", "red_panda", 574, 390, 14, 11},
    // North-west: a boreal-to-Arctic journey rather than one oversized tundra paddock.
    {"Reindeer Migration Wood", "reindeer", -584, 475, 18, 13},
    {"Musk Ox Ridge", "musk_ox", -519, 465, 18, 14},
    {"Arctic Fox Fell", "arctic_fox", -449, 475, 13, 11},
    {"Polar Bear Fjord", "polar_bear", -369, 460, 18, 14},
    {"Northern Wolf Taiga", "wolf", -289, 480, 17, 13},
    {"Red Deer Boreal Glade", "red_deer", -569, 575, 18, 13},
    {"European Bison Taiga", "european_bison", -499, 592, 19, 14},
    {"Lynx Spruce Ridge", "lynx", -419, 578, 15, 12},
    {"Brown Bear River", "brown_bear", -349, 595, 17, 14},
    {"Arctic Fox Coast", "arctic_fox", -279, 570, 13, 10},
    // North-centre: a compact European conservation landscape with deliberately small exhibits.
    {"Bison Conservation Wood", "european_bison", -160, 475, 18, 13},
    {"Red Deer Coppice", "red_deer", -95, 465, 17, 13},
    {"Lynx Limestone Hill", "lynx", -25, 475, 14, 11},
    {"Wolf Carpathian Wood", "wolf", 55, 460, 16, 13},
    {"Bear Mountain Stream", "brown_bear", 135, 480, 17, 14},
    {"Tortoise Conservatory", "aldabra_tortoise", -145, 575, 13, 10},
    {"Flamingo Fen", "flamingo", -75, 592, 16, 12},
    {"European Bison Meadow", "european_bison", 5, 578, 18, 13},
    {"Red Deer Riverside", "red_deer", 75, 595, 17, 13},
    {"Lynx Quiet Forest", "lynx", 145, 570, 14, 11},
    // North-east: islands and tropical forest, alternating intimate primate gardens and birds.
    {"Ring-tailed Lemur Island", "ring_tailed_lemur", 289, 478, 15, 12},
    {"Macaque Forest Temple", "barbary_macaque", 359, 463, 16, 13},
    {"Cassowary Palm Forest", "southern_cassowary", 429, 476, 15, 12},
    {"Aldabra Island", "aldabra_tortoise", 509, 461, 15, 11},
    {"Flamingo Mangrove", "flamingo", 584, 481, 17, 13},
    {"Lemur Canopy Walk", "ring_tailed_lemur", 279, 572, 15, 12},
    {"Tapir Tropical River", "south_american_tapir", 349, 593, 16, 13},
    {"Okapi Rainforest", "okapi", 424, 577, 16, 13},
    {"Cassowary Fern Gully", "southern_cassowary", 504, 594, 15, 12},
    {"Tropical Penguin Coast", "penguin", 574, 569, 16, 12},
}};

bool SteppeEquid(std::string_view species) {
    return species == "przewalski_horse" || species == "kiang";
}

bool ExpansionCat(std::string_view species) {
    return species == "cheetah" || species == "puma";
}

bool ExpansionCanid(std::string_view species) {
    return species == "maned_wolf" || species == "arctic_fox" || species == "spotted_hyena";
}

bool ExpansionRatite(std::string_view species) {
    return species == "emu" || species == "southern_cassowary";
}

bool ExpansionHerd(std::string_view species) {
    return species == "american_bison" || species == "reindeer" || species == "scimitar_horned_oryx" ||
           species == "guanaco" || species == "addax" || species == "red_deer" || species == "musk_ox" ||
           species == "european_bison";
}

bool Terrarium(std::string_view species) {
    return species == "corn_snake" || species == "black_headed_python";
}

std::vector<TileCoord> RegionTiles(const Script& script, const HabitatPlan& plan) {
    if (Terrarium(plan.species) || plan.species == "mandarin_duck") {
        return TilesInRect(script.Tile(plan.east - plan.rx, plan.north + plan.rn),
                           script.Tile(plan.east + plan.rx, plan.north - plan.rn));
    }
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
    if (species == "elephant" || species == "rhino" || species == "hippopotamus" ||
        species == "american_bison" || species == "european_bison") {
        return 2;
    }
    if (SteppeEquid(species) || ExpansionHerd(species) || ExpansionRatite(species) || species == "giraffe" ||
        species == "ostrich" || species == "camel" || species == "kangaroo" || species == "okapi") {
        return 1;
    }
    return 0;
}

std::vector<TileCoord> StripCorners(const Script& script, const HabitatPlan& plan) {
    if (plan.species == "ostrich") {
        // Close three diagonal gaps against Flamingo Lagoon before they become fenced,
        // unnamed pockets, and keep the southern path outside the planted stand-off.
        return {script.Tile(14, 27), script.Tile(20, 28), script.Tile(21, 29), script.Tile(22, 30)};
    }
    return {};
}

std::vector<TileCoord> PublicOutline(const Script& script, const HabitatPlan& plan) {
    auto tiles = Script::Expanded(RegionTiles(script, plan), StandOffDepth(plan.species));
    const auto corners = StripCorners(script, plan);
    tiles.insert(tiles.end(), corners.begin(), corners.end());
    return tiles;
}

void Walks(Zoo& zoo, Script& script, std::span<const HabitatPlan> habitats, bool sideGardens,
           bool steppeGardens, bool northernExpansion, bool continentalExpansion) {
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
    // The larger northern property has a woodland circuit of its own, joined at both ends to the
    // old predator walk. Its top reaches 348 m from the gate while retaining a three-tile planted
    // margin before the boundary wall. The centre spur serves the picnic glade below.
    script.PathAlong({{-12, 141},
                      {-20, 151},
                      {-24, 163},
                      {-18, 170},
                      {0, 174},
                      {22, 170},
                      {38, 162},
                      {40, 150},
                      {34, 138}},
                     3, PathKind::Asphalt);
    script.PathAlong({{0, 174}, {0, 164}, {6, 153}, {10, 141}}, 2, PathKind::Gravel);
    // Join the west gallery of Red Panda Canopy to the woodland circuit. Its own continuous
    // perimeter walk meets this short spur at the eastern viewing front.
    script.PathAlong({{-24, 163}, {-32, 163}}, 3, PathKind::Gravel);
    // Two arms carry the woodland crown into the second-stage land. The habitat's perimeter walk
    // joins them into a loop, so a closed side never strands visitors at the far end.
    script.PathAlong({{-18, 170}, {-17, 178}, {-14, 184}, {0, 196}}, 3, PathKind::Asphalt);
    script.PathAlong({{22, 170}, {18, 178}, {14, 184}, {0, 196}}, 3, PathKind::Asphalt);
    if (sideGardens) {
        // Each side garden connects two existing habitat galleries. The new land can be crossed
        // on a circuit, with a rest stop halfway along the outer walk.
        script.PathAlong({{65, 53}, {78, 53}, {90, 62}, {98, 78}, {98, 92}, {88, 109}, {62, 123}}, 3,
                         PathKind::Gravel);
        script.PathAlong({{-67, 52}, {-80, 53}, {-93, 64}, {-99, 78}, {-96, 94}, {-85, 106}, {-65, 115}}, 3,
                         PathKind::Gravel);
        script.PathAlong({{98, 86}, {90, 86}, {85, 84}}, 2, PathKind::Gravel);
        script.PathAlong({{73, 84}, {68, 86}, {61, 86}}, 2, PathKind::Gravel);
        // The east loop enters the reptile gallery from both ends.
        script.PathAlong({{85, 111}, {82, 114}, {82, 130}, {73, 131}, {62, 123}}, 3, PathKind::Cobblestone);
        script.PathAlong({{-94, 93}, {-96, 109}, {-94, 121}, {-90, 125}, {-76, 125}, {-64, 120}}, 2,
                         PathKind::Gravel);
        // The reed marsh lies between the western rest stop and Penguin Cove.
        script.PathAlong({{-97, 86}, {-90, 86}, {-85, 84}}, 2, PathKind::Boardwalk);
        script.PathAlong({{-73, 84}, {-65, 87}, {-60, 86}}, 2, PathKind::Gravel);
    }
    if (steppeGardens) {
        for (const int side : {-1, 1}) {
            script.PathAlong({{side * 96, 72}, {side * 115, 70}, {side * 136, 70}, {side * 155, 70}}, 3,
                             PathKind::Gravel);
            script.PathAlong({{side * 96, 94}, {side * 119, 101}, {side * 144, 99}, {side * 155, 98}}, 3,
                             PathKind::Gravel);
            // Fill the former empty wing with a continuous outer circuit around five exhibits.
            script.PathAlong({{side * 98, 43},
                              {side * 113, 38},
                              {side * 132, 40},
                              {side * 157, 44},
                              {side * 178, 45},
                              {side * 181, 64}},
                             3, PathKind::Gravel);
            script.PathAlong({{side * 180, 104},
                              {side * 187, 119},
                              {side * 187, 145},
                              {side * 165, 151},
                              {side * 143, 154},
                              {side * 145, 172},
                              {side * 174, 176},
                              {side * 177, 198},
                              {side * 145, 202},
                              {side * 105, 194},
                              {side * 66, 180},
                              {side * 38, 162}},
                             3, PathKind::Gravel);
            script.PathAlong({{side * 155, 99}, {side * 174, 108}, {side * 183, 123}}, 2, PathKind::Gravel);
            script.PathAlong({{side * 132, 42}, {side * 130, 61}, {side * 136, 70}}, 2, PathKind::Gravel);
        }
    }
    if (northernExpansion) {
        // A broad, asymmetric landscape loop replaces the former parallel avenues.  Smaller
        // exhibits sit against its changing bends, while a woodland cross-trail offers shortcuts
        // without turning the district into a machine-made grid.
        script.PathAlong({{-190, 218}, {-150, 211}, {-111, 222}, {-70, 210},  {-25, 222},  {18, 213},
                          {63, 225},   {106, 212},  {151, 224},  {190, 218},  {200, 250},  {188, 280},
                          {204, 310},  {185, 340},  {198, 371},  {178, 404},  {136, 417},  {91, 407},
                          {45, 420},   {-5, 409},   {-50, 419},  {-96, 405},  {-140, 417}, {-180, 402},
                          {-196, 370}, {-183, 338}, {-201, 305}, {-185, 275}, {-198, 245}, {-190, 218}},
                         3, PathKind::Asphalt);
        script.PathAlong({{-174, 349},
                          {-132, 359},
                          {-91, 345},
                          {-48, 358},
                          {-7, 346},
                          {34, 359},
                          {74, 345},
                          {113, 356},
                          {151, 344},
                          {185, 340}},
                         3, PathKind::Gravel);
        script.PathAlong({{-150, 281},
                          {-112, 292},
                          {-73, 280},
                          {-34, 291},
                          {7, 279},
                          {48, 292},
                          {88, 281},
                          {126, 293},
                          {160, 310},
                          {185, 340}},
                         2, PathKind::Gravel);
        // Three independent approaches retain a loop from the established zoo.
        script.PathAlong({{0, 196}, {-6, 207}, {-25, 222}}, 3, PathKind::Asphalt);
        script.PathAlong({{-177, 198}, {-184, 207}, {-190, 218}}, 3, PathKind::Gravel);
        script.PathAlong({{177, 198}, {184, 207}, {190, 218}}, 3, PathKind::Gravel);
        for (const HabitatPlan& plan : habitats.subspan(34, 14)) {
            TileCoord join{plan.east, plan.north < 280 ? 218 : plan.north < 350 ? 350 : 417};
            // Exact vertices on the winding middle walk. A shared nominal northing left the four
            // central rings a few tiles short of the curve even though they looked connected.
            if (plan.north >= 280 && plan.north < 350) {
                switch (plan.east) {
                    case -154:
                        join = {-174, 349};
                        break;
                    case -60:
                        join = {-48, 358};
                        break;
                    case 28:
                        join = {34, 359};
                        break;
                    case 111:
                        join = {113, 356};
                        break;
                    case 180:
                        join = {185, 340};
                        break;
                    default:
                        break;
                }
            }
            const int clearance = plan.rn + StandOffDepth(plan.species) + 2;
            const int frontNorth = join.z < plan.north ? plan.north - clearance : plan.north + clearance;
            const int bend = ((plan.east + plan.north) & 1) == 0 ? -5 : 5;
            script.PathAlong(
                {{plan.east, frontNorth}, {(plan.east + join.x) / 2 + bend, (frontNorth + join.z) / 2}, join},
                2, PathKind::Gravel);
        }
    }
    if (continentalExpansion) {
        // The Australian sheet has its own low gravel trail and wetland boardwalk instead of the
        // continental circuit copied by the other districts. Two diagonal links make useful
        // shorter loops without restoring a straight middle avenue.
        script.PathAlong({{-619, 80},
                          {-584, 70},
                          {-552, 82},
                          {-519, 66},
                          {-484, 79},
                          {-449, 73},
                          {-410, 87},
                          {-369, 68},
                          {-329, 84},
                          {-289, 76},
                          {-254, 90},
                          {-240, 112},
                          {-228, 137}},
                         3, PathKind::Gravel);
        script.PathAlong({{-592, 152},
                          {-569, 132},
                          {-535, 145},
                          {-499, 143},
                          {-455, 125},
                          {-419, 132},
                          {-385, 150},
                          {-349, 145},
                          {-315, 125},
                          {-279, 124},
                          {-250, 132},
                          {-228, 137}},
                         3, PathKind::Boardwalk);
        script.PathAlong({{-619, 80}, {-610, 112}, {-592, 152}}, 3, PathKind::Gravel);
        script.PathAlong({{-519, 66}, {-510, 100}, {-499, 143}}, 2, PathKind::Boardwalk);
        script.PathAlong({{-369, 68}, {-379, 103}, {-385, 150}}, 2, PathKind::Gravel);
        script.PathAlong({{-273, 151}, {-250, 146}, {-228, 137}}, 3, PathKind::Gravel);

        // The south-east African sheet follows a broad savanna trail and a lower river boardwalk.
        // Two narrow stone links serve the predators and divide the long walks into useful loops.
        // The literal end vertices retain the established-zoo and eastern-column connections.
        script.PathAlong({{229, 126},
                          {246, 105},
                          {265, 84},
                          {289, 78},
                          {325, 86},
                          {359, 73},
                          {395, 91},
                          {429, 86},
                          {464, 99},
                          {499, 78},
                          {540, 87},
                          {574, 80},
                          {608, 92},
                          {621, 115},
                          {605, 137},
                          {594, 151}},
                         3, PathKind::Gravel);
        script.PathAlong({{229, 126},
                          {244, 134},
                          {279, 122},
                          {315, 137},
                          {349, 123},
                          {386, 136},
                          {424, 118},
                          {463, 133},
                          {504, 119},
                          {544, 137},
                          {575, 126},
                          {594, 151}},
                         3, PathKind::Boardwalk);
        // Rejoin the west end to the inherited vertical greenway at the sheet seam. Without this
        // short arm the seam is reachable only by walking almost all the way around the district.
        script.PathAlong({{244, 134}, {254, 151}}, 3, PathKind::Boardwalk);
        script.PathAlong({{359, 73}, {367, 99}, {386, 136}}, 2, PathKind::Cobblestone);
        script.PathAlong({{499, 78}, {486, 102}, {463, 133}}, 2, PathKind::Cobblestone);

        // The mid-west American sheet pairs a dry prairie trail with a wetter oxbow boardwalk.
        // Three cross-links create smaller loops, and short arms retain all inherited sheet seams.
        script.PathAlong({{-594, 273},
                          {-584, 286},
                          {-550, 300},
                          {-519, 280},
                          {-486, 295},
                          {-449, 279},
                          {-410, 299},
                          {-369, 282},
                          {-330, 300},
                          {-289, 282},
                          {-260, 296},
                          {-244, 320},
                          {-228, 349}},
                         3, PathKind::Gravel);
        script.PathAlong({{-594, 363},
                          {-569, 342},
                          {-535, 356},
                          {-499, 337},
                          {-460, 354},
                          {-419, 340},
                          {-382, 360},
                          {-349, 342},
                          {-315, 357},
                          {-279, 339},
                          {-250, 348},
                          {-228, 349}},
                         3, PathKind::Boardwalk);
        script.PathAlong({{-519, 280}, {-510, 315}, {-499, 337}}, 2, PathKind::Boardwalk);
        script.PathAlong({{-449, 279}, {-435, 320}, {-419, 340}}, 2, PathKind::Cobblestone);
        script.PathAlong({{-369, 282}, {-365, 320}, {-349, 342}}, 2, PathKind::Gravel);
        script.PathAlong({{-289, 282}, {-279, 272}}, 3, PathKind::Gravel);
        script.PathAlong({{-279, 339}, {-273, 363}}, 3, PathKind::Boardwalk);

        // The mid-east Asian sheet pairs a stone mountain switchback with a monsoon boardwalk.
        // Three cross-links form shorter temple, bamboo and predator loops between them.
        script.PathAlong({{254, 273},
                          {289, 288},
                          {330, 274},
                          {359, 296},
                          {395, 278},
                          {429, 302},
                          {465, 281},
                          {509, 298},
                          {548, 279},
                          {584, 292},
                          {594, 273}},
                         3, PathKind::Cobblestone);
        script.PathAlong({{254, 363},
                          {279, 345},
                          {320, 362},
                          {349, 340},
                          {390, 357},
                          {424, 338},
                          {465, 360},
                          {504, 342},
                          {545, 359},
                          {574, 340},
                          {594, 363}},
                         3, PathKind::Boardwalk);
        script.PathAlong({{229, 318}, {245, 326}, {279, 345}}, 3, PathKind::Gravel);
        script.PathAlong({{359, 296}, {370, 320}, {349, 340}}, 2, PathKind::Cobblestone);
        script.PathAlong({{429, 302}, {424, 338}}, 2, PathKind::Gravel);
        script.PathAlong({{509, 298}, {490, 322}, {504, 342}}, 2, PathKind::Boardwalk);

        // The remaining new sheets are landscape districts with an asymmetric, gently winding
        // circuit. Neighbouring circuits meet at short greenways on the sheet seams.
        const auto districtLoop = [&](int east, int north, PathKind kind) {
            script.PathAlong({{east - 195, north - 26}, {east - 170, north - 42}, {east - 122, north - 34},
                              {east - 72, north - 45},  {east - 18, north - 33},  {east + 34, north - 44},
                              {east + 91, north - 32},  {east + 145, north - 46}, {east + 191, north - 25},
                              {east + 180, north + 3},  {east + 196, north + 31}, {east + 151, north + 45},
                              {east + 96, north + 34},  {east + 43, north + 47},  {east - 9, north + 35},
                              {east - 62, north + 45},  {east - 119, north + 33}, {east - 168, north + 46},
                              {east - 196, north + 21}, {east - 195, north - 26}},
                             3, kind);
            script.PathAlong({{east - 168, north + 2},
                              {east - 115, north - 5},
                              {east - 58, north + 7},
                              {east, north - 4},
                              {east + 55, north + 8},
                              {east + 112, north - 6},
                              {east + 180, north + 3}},
                             2, PathKind::Gravel);
        };
        for (const auto& [east, north, kind] :
             std::array<std::tuple<int, int, PathKind>, 3>{{{-424, 530, PathKind::Gravel},
                                                            {0, 530, PathKind::Asphalt},
                                                            {424, 530, PathKind::Boardwalk}}}) {
            districtLoop(east, north, kind);
        }
        // The remaining north-western middle promenade leaves a one-tile cusp where its shallow
        // bends cross the sheet centre. Keep the public walk at least two tiles wide there.
        script.PathAlong({{-430, 530}, {-420, 530}, {-410, 530}}, 3, PathKind::Gravel);
        // Only the northern row has three new neighbouring districts. In the two established
        // centre sheets the short nominal seam had no centre circuit to meet; the curved gateways
        // below are their real connections.
        script.PathAlong({{-229, 530}, {-214, 535}, {-195, 530}}, 3, PathKind::Gravel);
        script.PathAlong({{195, 530}, {214, 525}, {229, 530}}, 3, PathKind::Gravel);
        // Vertical seams in the outer columns, kept near district edges so they do not divide a
        // habitat.  Both sides are linked, providing a choice of direction around every sheet.
        for (const int east : {-594, 254, 594}) {
            script.PathAlong({{east, 151}, {east + (east < 0 ? 7 : -7), 212}, {east, 273}}, 3,
                             PathKind::Gravel);
            script.PathAlong({{east, 363}, {east + (east < 0 ? -6 : 6), 424}, {east, 485}}, 3,
                             PathKind::Gravel);
        }
        // The west sheets are asymmetric: their inner edge is not the mirror of the outer edge.
        // Meet actual circuit vertices instead of leaving the inner greenway floating nearby.
        script.PathAlong({{-273, 151}, {-267, 212}, {-279, 272}}, 3, PathKind::Gravel);
        script.PathAlong({{-273, 363}, {-267, 424}, {-279, 484}}, 3, PathKind::Gravel);
        // The established centre sheets remain the heart of the plan.  Curved gateways join the
        // new circuits to their existing west, east and northern walks without cutting exhibits.
        // Use literal vertices from both circuits. The former visually close endpoints were up
        // to fifteen tiles apart, leaving the whole western column as a separate route graph.
        script.PathAlong({{-228, 137}, {-211, 154}, {-193, 181}, {-177, 198}}, 3, PathKind::Gravel);
        script.PathAlong({{229, 126}, {211, 154}, {193, 181}, {177, 198}}, 3, PathKind::Gravel);
        script.PathAlong({{-228, 349}, {-207, 345}, {-185, 340}}, 3, PathKind::Gravel);
        script.PathAlong({{229, 318}, {214, 329}, {200, 342}}, 3, PathKind::Gravel);
        script.PathAlong({{-195, 504}, {-205, 466}, {-180, 402}}, 3, PathKind::Gravel);
        script.PathAlong({{195, 504}, {205, 466}, {178, 404}}, 3, PathKind::Gravel);
        // The Australian fronts join named vertices of their two trails. These short branches
        // retain the deliberate bends rather than ending on the removed middle promenade.
        constexpr std::array<TileCoord, 10> kAustralianJoins{{
            {-584, 70},
            {-519, 66},
            {-449, 73},
            {-369, 68},
            {-289, 76},
            {-569, 132},
            {-499, 143},
            {-419, 132},
            {-349, 145},
            {-279, 124},
        }};
        for (std::size_t i = 0; i < kAustralianJoins.size(); ++i) {
            const HabitatPlan& plan = habitats[48 + i];
            const TileCoord join = kAustralianJoins[i];
            const int clearance = plan.rn + StandOffDepth(plan.species) + 2;
            const int frontNorth = join.z < plan.north ? plan.north - clearance : plan.north + clearance;
            const int bend = i % 2 == 0 ? -3 : 3;
            script.PathAlong({{plan.east, frontNorth},
                              {(plan.east + join.x) / 2 + bend, (frontNorth + join.z) / 2},
                              {join.x, join.z}},
                             2, PathKind::Gravel);
        }

        // African exhibit fronts join the named savanna and river vertices. The two predator
        // branches intentionally meet the stone cross-links rather than a generic middle avenue.
        constexpr std::array<TileCoord, 10> kAfricanJoins{{
            {289, 78},
            {359, 73},
            {429, 86},
            {499, 78},
            {574, 80},
            {279, 122},
            {349, 123},
            {424, 118},
            {504, 119},
            {575, 126},
        }};
        for (std::size_t i = 0; i < kAfricanJoins.size(); ++i) {
            const HabitatPlan& plan = habitats[58 + i];
            const TileCoord join = kAfricanJoins[i];
            const int clearance = plan.rn + StandOffDepth(plan.species) + 2;
            const int frontNorth = join.z < plan.north ? plan.north - clearance : plan.north + clearance;
            const int bend = i % 2 == 0 ? 3 : -3;
            script.PathAlong({{plan.east, frontNorth},
                              {(plan.east + join.x) / 2 + bend, (frontNorth + join.z) / 2},
                              {join.x, join.z}},
                             2, PathKind::Gravel);
        }

        constexpr std::array<TileCoord, 10> kAmericanJoins{{
            {-584, 286},
            {-519, 280},
            {-449, 279},
            {-369, 282},
            {-289, 282},
            {-569, 342},
            {-499, 337},
            {-419, 340},
            {-349, 342},
            {-279, 339},
        }};
        for (std::size_t i = 0; i < kAmericanJoins.size(); ++i) {
            const HabitatPlan& plan = habitats[68 + i];
            const TileCoord join = kAmericanJoins[i];
            const int clearance = plan.rn + StandOffDepth(plan.species) + 2;
            const int frontNorth = join.z < plan.north ? plan.north - clearance : plan.north + clearance;
            const int bend = i % 2 == 0 ? -3 : 3;
            script.PathAlong({{plan.east, frontNorth},
                              {(plan.east + join.x) / 2 + bend, (frontNorth + join.z) / 2},
                              {join.x, join.z}},
                             2, PathKind::Gravel);
        }

        constexpr std::array<TileCoord, 10> kAsianJoins{{
            {289, 288},
            {359, 296},
            {429, 302},
            {509, 298},
            {584, 292},
            {279, 345},
            {349, 340},
            {424, 338},
            {504, 342},
            {574, 340},
        }};
        for (std::size_t i = 0; i < kAsianJoins.size(); ++i) {
            const HabitatPlan& plan = habitats[78 + i];
            const TileCoord join = kAsianJoins[i];
            const int clearance = plan.rn + StandOffDepth(plan.species) + 2;
            const int frontNorth = join.z < plan.north ? plan.north - clearance : plan.north + clearance;
            const int bend = i % 2 == 0 ? 3 : -3;
            script.PathAlong({{plan.east, frontNorth},
                              {(plan.east + join.x) / 2 + bend, (frontNorth + join.z) / 2},
                              {join.x, join.z}},
                             2, PathKind::Gravel);
        }

        // Every other exhibit gets a short, slightly bent branch from its near side to the
        // district's middle promenade. These make both lobes reachable where the loop swings away.
        for (const HabitatPlan& plan : habitats.subspan(48)) {
            if (plan.east < -212 && plan.north < 212) {
                continue;  // the Australian sheet was joined to its own trails above
            }
            if (plan.east > 212 && plan.north < 212) {
                continue;  // the African sheet was joined to its own trails above
            }
            if (plan.east < -212 && plan.north >= 212 && plan.north < 424) {
                continue;  // the American sheet was joined to its own trails above
            }
            if (plan.east > 212 && plan.north >= 212 && plan.north < 424) {
                continue;  // the Asian sheet was joined to its own trails above
            }
            const int districtNorth = plan.north < 212 ? 106 : plan.north < 424 ? 318 : 530;
            const int clearance = plan.rn + StandOffDepth(plan.species) + 2;
            const int frontNorth =
                plan.north < districtNorth ? plan.north + clearance : plan.north - clearance;
            const int bend = ((plan.east / 7 + plan.north) & 1) == 0 ? -4 : 4;
            script.PathAlong({{plan.east, frontNorth},
                              {plan.east + bend, (frontNorth + districtNorth) / 2},
                              {plan.east, districtNorth}},
                             2, PathKind::Gravel);
        }
    }
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
    for (const HabitatPlan& plan : habitats) {
        for (const TileCoord tile : PublicOutline(script, plan)) {
            reserved.emplace(tile.z, tile.x);
        }
    }
    for (const HabitatPlan& plan : habitats) {
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
    if (ExpansionCat(species)) {
        const bool forest = species == "puma";
        return {
            forest ? Terrain::ForestFloor : Terrain::Savanna,
            "heavy_mesh",
            "high_glass_wall",
            "medium_shelter",
            "meat_platform",
            forest ? "juniper" : "savanna_shrub",
            forest ? "granite_boulder" : "sandstone_rock",
            {"scratching_post", "climbing_frame", "log_pile", "ball", "fallen_log"},
            forest ? std::vector<std::string_view>{"pine", "birch", "juniper", "juniper", "tall_grass"}
                   : std::vector<std::string_view>{"acacia", "savanna_shrub", "savanna_shrub", "tall_grass"}};
    }
    if (ExpansionCanid(species)) {
        const bool cold = species == "arctic_fox";
        const bool open = species == "spotted_hyena";
        return {cold   ? Terrain::Snow
                : open ? Terrain::Savanna
                       : Terrain::Grass,
                open ? "heavy_mesh" : "tall_mesh",
                "high_glass_wall",
                cold ? "cooled_den" : "medium_shelter",
                "meat_platform",
                cold   ? "juniper"
                : open ? "savanna_shrub"
                       : "tall_grass",
                cold   ? "ice_rock"
                : open ? "sandstone_rock"
                       : "granite_boulder",
                {"log_pile", "fallen_log", "ball", "scratching_post"},
                cold   ? std::vector<std::string_view>{"juniper", "juniper", "pine"}
                : open ? std::vector<std::string_view>{"acacia", "savanna_shrub", "tall_grass"}
                       : std::vector<std::string_view>{"tall_grass", "tall_grass", "juniper", "birch"}};
    }
    if (ExpansionRatite(species)) {
        const bool forest = species == "southern_cassowary";
        return {forest ? Terrain::ForestFloor : Terrain::Grass,
                "tall_mesh",
                "glass_wall",
                "medium_shelter",
                "pellet_fruit_trough",
                forest ? "tropical_fern" : "savanna_shrub",
                forest ? "granite_boulder" : "sandstone_rock",
                {"log_pile", "fallen_log", "termite_mound"},
                forest
                    ? std::vector<std::string_view>{"tropical_fern", "tropical_fern", "palm", "birch"}
                    : std::vector<std::string_view>{"acacia", "savanna_shrub", "tall_grass", "tall_grass"}};
    }
    if (species == "hippopotamus") {
        return {Terrain::Grass,
                "cable_barrier",
                "high_glass_wall",
                "heated_barn",
                "hay_rack",
                "savanna_shrub",
                "sandstone_rock",
                {"log_pile", "ball", "scratching_post"},
                {"tall_grass", "tall_grass", "savanna_shrub", "acacia"}};
    }
    if (species == "south_american_tapir") {
        return {Terrain::ForestFloor,
                "heavy_mesh",
                "glass_wall",
                "heated_barn",
                "pellet_fruit_trough",
                "tropical_fern",
                "granite_boulder",
                {"log_pile", "fallen_log", "scratching_post"},
                {"tropical_fern", "tropical_fern", "palm", "willow", "birch"}};
    }
    if (species == "black_tailed_prairie_dog") {
        return {Terrain::Dirt,
                "wire_mesh",
                "glass_wall",
                "small_shelter",
                "pellet_fruit_trough",
                "tall_grass",
                "sandstone_rock",
                {"termite_mound", "log_pile"},
                {"tall_grass", "tall_grass", "savanna_shrub"}};
    }
    if (species == "collared_peccary" || species == "common_wombat") {
        return {species == "common_wombat" ? Terrain::Grass : Terrain::ForestFloor,
                "tall_mesh",
                "glass_wall",
                "medium_shelter",
                "pellet_fruit_trough",
                "juniper",
                "granite_boulder",
                {"log_pile", "fallen_log", "scratching_post"},
                {"tall_grass", "juniper", "birch", "tropical_fern"}};
    }
    if (species == "giant_anteater") {
        return {Terrain::Grass,
                "tall_mesh",
                "glass_wall",
                "medium_shelter",
                "insect_feeder",
                "tall_grass",
                "sandstone_rock",
                {"termite_mound", "fallen_log", "log_pile"},
                {"tall_grass", "tall_grass", "savanna_shrub", "acacia"}};
    }
    if (species == "ring_tailed_lemur" || species == "barbary_macaque") {
        const bool rocky = species == "barbary_macaque";
        return {rocky ? Terrain::Rock : Terrain::ForestFloor,
                "tall_mesh",
                "high_glass_wall",
                "heated_barn",
                "pellet_fruit_trough",
                rocky ? "juniper" : "tropical_fern",
                "granite_boulder",
                {"climbing_frame", "tyre_swing", "fallen_log", "log_pile"},
                rocky ? std::vector<std::string_view>{"pine", "juniper", "juniper", "birch"}
                      : std::vector<std::string_view>{"tropical_fern", "palm", "birch", "tall_grass"}};
    }
    if (species == "okapi") {
        return {Terrain::ForestFloor,
                "tall_cable_fence",
                "glass_wall",
                "heated_barn",
                "giraffe_browse_feeder",
                "tropical_fern",
                "granite_boulder",
                {"log_pile", "fallen_log", "scratching_post"},
                {"tropical_fern", "tropical_fern", "palm", "birch", "oak"}};
    }
    if (ExpansionHerd(species)) {
        const bool desert = species == "addax" || species == "scimitar_horned_oryx";
        const bool cold = species == "reindeer" || species == "musk_ox";
        const bool forest = species == "red_deer" || species == "european_bison";
        const bool powerful = species == "american_bison" || species == "european_bison";
        return {cold     ? Terrain::Snow
                : desert ? Terrain::Sand
                : forest ? Terrain::ForestFloor
                         : Terrain::Grass,
                powerful               ? "cable_barrier"
                : species == "musk_ox" ? "heavy_mesh"
                                       : "paddock_rail",
                "glass_wall",
                cold ? "cooled_den" : "large_shelter",
                "hay_rack",
                desert ? "desert_scrub" : "juniper",
                cold     ? "ice_rock"
                : desert ? "sandstone_rock"
                         : "granite_boulder",
                {"log_pile", "scratching_post", "fallen_log"},
                desert   ? std::vector<std::string_view>{"desert_scrub", "desert_scrub", "tall_grass"}
                : forest ? std::vector<std::string_view>{"oak", "birch", "juniper", "tall_grass"}
                         : std::vector<std::string_view>{"tall_grass", "juniper", "birch", "pine"}};
    }
    if (SteppeEquid(species)) {
        return {species == "kiang" ? Terrain::Grass : Terrain::Savanna,
                "paddock_rail",
                "paddock_rail",
                "large_shelter",
                "hay_rack",
                "desert_scrub",
                "sandstone_rock",
                {"log_pile", "scratching_post", "fallen_log"},
                {"tall_grass", "juniper", "desert_scrub", "tall_grass", "birch"}};
    }
    if (species == "mandarin_duck") {
        return {Terrain::Grass,
                "aviary_mesh",
                "aviary_mesh",
                "small_shelter",
                "waterfowl_pan",
                "juniper",
                "sandstone_rock",
                {"fallen_log", "log_pile"},
                {"young_birch", "juniper", "young_birch", "tropical_fern"}};
    }
    if (Terrarium(species)) {
        const bool woodland = species == "corn_snake";
        return {woodland ? Terrain::ForestFloor : Terrain::Sand,
                "concrete_wall",
                "glass_wall",
                "reptile_hide",
                "prey_tray",
                "desert_scrub",
                "sandstone_rock",
                {"fallen_log", "sandstone_rock"},
                {woodland ? "tropical_fern" : "desert_scrub"}};
    }
    if (species == "meerkat") {
        return {Terrain::Sand,    "mesh_backed_hedge",
                "glass_wall",     "small_shelter",
                "insect_feeder",  "desert_scrub",
                "sandstone_rock", {"termite_mound", "log_pile", "termite_mound", "termite_mound"},
                {"desert_scrub"}};
    }
    if (species == "aldabra_tortoise") {
        return {Terrain::Grass,
                "mesh_backed_hedge",
                "glass_wall",
                "heated_barn",
                "forage_tray",
                "desert_scrub",
                "sandstone_rock",
                {"fallen_log", "log_pile"},
                {"acacia", "desert_scrub", "tall_grass", "tall_grass"}};
    }
    if (species == "capybara") {
        return {Terrain::Grass,
                "wire_mesh",
                "glass_wall",
                "heated_barn",
                "forage_tray",
                "tall_grass",
                "sandstone_rock",
                {"fallen_log", "log_pile", "ball"},
                {"willow", "birch", "tall_grass", "tall_grass", "tall_grass"}};
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
                 "tall_grass", "baobab"}};
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
                {"acacia", "acacia", "savanna_shrub", "savanna_shrub", "savanna_shrub", "tall_grass"}};
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
                {"acacia", "acacia", "savanna_shrub", "savanna_shrub", "savanna_shrub", "tall_grass"}};
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
    if (species == "lynx") {
        return {Terrain::ForestFloor,
                "tall_mesh",
                "glass_wall",
                "medium_shelter",
                "meat_platform",
                "juniper",
                "granite_boulder",
                {"climbing_frame", "scratching_post", "fallen_log", "log_pile"},
                {"spruce", "spruce", "spruce", "pine", "birch", "birch", "juniper", "juniper", "juniper"}};
    }
    if (species == "red_panda") {
        return {Terrain::ForestFloor,
                "tall_mesh",
                "glass_wall",
                "small_shelter",
                "bamboo_feeder",
                "bamboo_clump",
                "granite_boulder",
                {"climbing_frame", "fallen_log", "log_pile", "tyre_swing"},
                {"bamboo_clump", "bamboo_clump", "bamboo_clump", "bamboo_clump", "birch", "birch", "oak",
                 "tropical_fern", "tropical_fern"}};
    }
    if (species == "snow_leopard") {
        return {Terrain::Rock,
                "heavy_mesh",
                "high_glass_wall",
                "medium_shelter",
                "meat_platform",
                "juniper",
                "granite_boulder",
                {"climbing_frame", "scratching_post", "fallen_log", "log_pile"},
                {"juniper", "juniper", "juniper", "pine", "pine", "spruce"}};
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
                {"pine", "pine", "spruce", "spruce", "oak", "oak", "oak", "birch", "juniper", "juniper",
                 "tall_grass"}};
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
                "dry_moat",
                "large_shelter",
                "hay_rack",
                "savanna_shrub",
                "sandstone_rock",
                {"log_pile", "tyre_swing", "ball", "scratching_post"},
                {"acacia", "acacia", "acacia", "acacia", "savanna_shrub", "savanna_shrub", "savanna_shrub",
                 "tall_grass", "baobab"}};
    }
    if (species == "rhino") {
        return {Terrain::Grass,
                "cable_barrier",
                "dry_moat",
                "large_shelter",
                "hay_rack",
                "savanna_shrub",
                "sandstone_rock",
                {"log_pile", "scratching_post"},
                {"acacia", "acacia", "savanna_shrub", "savanna_shrub", "savanna_shrub", "tall_grass"}};
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
                {"desert_scrub", "desert_scrub", "desert_scrub", "desert_scrub", "tall_grass", "tall_grass",
                 "palm", "palm"}};
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
            {"acacia", "acacia", "savanna_shrub", "savanna_shrub", "savanna_shrub", "tall_grass"}};
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
// species, a pair for tigers, and a mother with cubs for the bears. The first of each family is
// its sire, except in the bear and elephant mother groups where the sire lives elsewhere. The
// youngest are under a quarter of their species' age of maturity and stay under it for a week of
// the calendar and more: they suckle, keep to their mothers and are drawn as juveniles (below half
// of maturity).
// Older daughters are female so that no second adult male grows up into a fight. The kangaroos'
// joeys are the exception: under half a year they would be in the pouch, which the simulation does
// not have, so theirs are young at foot, past the quarter and still drawn young.
std::span<const Resident> Family(std::string_view species) {
    constexpr Sex m = Sex::Male;
    constexpr Sex f = Sex::Female;
    if (species == "cheetah") {
        static constexpr Resident coalition[] = {Adult(m, 6), Adult(m, 5), Adult(m, 4)};
        return coalition;
    }
    if (species == "puma") {
        static constexpr Resident family[] = {Adult(f, 7), Young(f, 0.8f, 0), Young(m, 0.8f, 0)};
        return family;
    }
    if (ExpansionRatite(species)) {
        static constexpr Resident family[] = {Adult(m, 7), Adult(f, 6), Adult(f, 5), Young(f, 0.5f, 1),
                                              Young(m, 0.5f, 1)};
        return family;
    }
    if (species == "maned_wolf") {
        static constexpr Resident family[] = {Adult(m, 6), Adult(f, 5), Young(f, 0.4f, 1), Young(m, 0.4f, 1)};
        return family;
    }
    if (species == "arctic_fox") {
        static constexpr Resident family[] = {Adult(m, 4), Adult(f, 4), Young(f, 0.3f, 1), Young(m, 0.3f, 1),
                                              Young(f, 0.3f, 1)};
        return family;
    }
    if (species == "spotted_hyena") {
        static constexpr Resident clan[] = {Adult(f, 8), Adult(f, 6),       Adult(f, 5),
                                            Adult(m, 5), Young(f, 0.6f, 1), Young(m, 0.6f, 1)};
        return clan;
    }
    if (species == "hippopotamus") {
        static constexpr Resident group[] = {Adult(m, 18), Adult(f, 16), Adult(f, 12), Young(f, 2, 1)};
        return group;
    }
    if (species == "south_american_tapir") {
        static constexpr Resident family[] = {Adult(f, 9), Young(m, 0.8f, 0)};
        return family;
    }
    if (species == "black_tailed_prairie_dog") {
        static constexpr Resident colony[] = {Adult(m, 4),        Adult(f, 4),        Adult(f, 3),
                                              Adult(f, 3),        Young(f, 0.25f, 1), Young(m, 0.25f, 1),
                                              Young(f, 0.25f, 2), Young(m, 0.25f, 2)};
        return colony;
    }
    if (species == "collared_peccary") {
        static constexpr Resident herd[] = {Adult(m, 5), Adult(f, 5),       Adult(f, 4),
                                            Adult(f, 3), Young(f, 0.5f, 1), Young(m, 0.5f, 2)};
        return herd;
    }
    if (species == "common_wombat") {
        static constexpr Resident family[] = {Adult(f, 6), Young(f, 0.5f, 0)};
        return family;
    }
    if (species == "giant_anteater") {
        static constexpr Resident family[] = {Adult(f, 7), Young(m, 0.7f, 0)};
        return family;
    }
    if (species == "ring_tailed_lemur") {
        static constexpr Resident troop[] = {Adult(f, 8), Adult(f, 6),       Adult(f, 5),      Adult(m, 6),
                                             Adult(m, 4), Young(f, 0.5f, 1), Young(m, 0.5f, 2)};
        return troop;
    }
    if (species == "barbary_macaque") {
        static constexpr Resident troop[] = {Adult(m, 10), Adult(m, 7),       Adult(f, 9),      Adult(f, 7),
                                             Adult(f, 5),  Young(f, 0.8f, 2), Young(m, 0.8f, 3)};
        return troop;
    }
    if (species == "okapi") {
        static constexpr Resident family[] = {Adult(f, 8), Young(f, 1, 0)};
        return family;
    }
    if (ExpansionHerd(species)) {
        static constexpr Resident herd[] = {Adult(m, 8), Adult(f, 9), Adult(f, 6), Adult(f, 5),
                                            Young(f, 0.7f, 1)};
        return herd;
    }
    if (SteppeEquid(species)) {
        static constexpr Resident herd[] = {Adult(m, 8), Adult(f, 9), Adult(f, 6), Adult(f, 5),
                                            Young(f, 0.6f, 1)};
        return herd;
    }
    if (species == "mandarin_duck") {
        static constexpr Resident flock[] = {Adult(m, 3), Adult(f, 3), Adult(m, 2), Adult(f, 2)};
        return flock;
    }
    if (Terrarium(species)) {
        static constexpr Resident solitary[] = {Adult(m, 7.0f)};
        return solitary;
    }
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
    if (species == "lynx") {
        // Eurasian lynx are solitary outside the breeding season: the public group is a mother
        // with two still-dependent kittens, not an invented adult pair or pack.
        static constexpr Resident family[] = {Adult(f, 6.0f), Young(f, 0.45f, 0), Young(m, 0.45f, 0)};
        return family;
    }
    if (species == "red_panda") {
        // Red pandas are solitary except while breeding and rearing young: this is a mother with
        // two dependent cubs, not an adult social group.
        static constexpr Resident family[] = {Adult(f, 5.0f), Young(f, 0.2f, 0), Young(m, 0.2f, 0)};
        return family;
    }
    if (species == "aldabra_tortoise") {
        static constexpr Resident adults[] = {Adult(m, 60.0f), Adult(f, 45.0f), Adult(f, 55.0f)};
        return adults;
    }
    if (species == "capybara") {
        static constexpr Resident family[] = {Adult(m, 5.0f),     Adult(f, 4.0f),     Adult(f, 3.0f),
                                              Adult(f, 5.0f),     Young(f, 0.13f, 1), Young(m, 0.13f, 1),
                                              Young(m, 0.13f, 1), Young(f, 0.13f, 1)};
        return family;
    }
    if (species == "snow_leopard") {
        // A solitary mother raises the cubs; their sire has a separate range and is not displayed
        // as an invented permanent pair.
        static constexpr Resident family[] = {Adult(f, 7.0f), Young(f, 0.45f, 0), Young(m, 0.45f, 0)};
        return family;
    }
    if (species == "brown_bear") {
        static constexpr Resident family[] = {Adult(f, 10.0f), Young(f, 0.7f, 0), Young(m, 0.7f, 0)};
        return family;
    }
    if (species == "polar_bear") {
        static constexpr Resident family[] = {Adult(f, 9.0f), Young(f, 0.6f, 0), Young(m, 0.6f, 0)};
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

bool MarginalPlant(const PropDef& kind) {
    return kind.look == "reeds";
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
        (Plant(kind.role) && !MarginalPlant(kind) && !AboveTheBank(zoo.Map(), point, radius + 0.1f)) ||
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

/// Digs the pool and returns its radius in metres.
float HabitatPool(Zoo& zoo, Script& script, const Site& site, const SpeciesDef& species) {
    const HabitatPlan& plan = *site.plan;
    if (Terrarium(plan.species)) {
        // The enclosure has a removable drinking basin and a dry floor.
        return 0.0f;
    }
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
    const auto shore = [&](TileCoord tile) {
        return Interior(zoo, tile, site.id, 1) &&
               (plan.species != "mandarin_duck" || !BesidePath(zoo.Map(), tile, 1));
    };
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
    const bool shallow = dish || plan.species == "aldabra_tortoise";
    const float depth = shallow ? Builder::kMinWaterDepth : species.needsDeepWater ? 1.4f : 0.45f;
    // Nearest first: the heart is dug before the shallows, which take its surface.
    const std::size_t heart = lagoon                       ? pool.size() / 8
                              : plan.species == "capybara" ? pool.size() * 2 / 3
                                                           : pool.size();
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
    const EnclosureStats& initial = zoo.Habitats().Find(site.id)->stats;
    const float dryM2 = initial.areaM2 * (1.0f - initial.waterShare);
    float vegetationM2 = initial.vegetationM2;
    const auto density = [&] { return dryM2 > 0.0f ? std::min(1.0f, vegetationM2 / dryM2) : 0.0f; };
    // First keep every trunk behind the depth of the planted strip, not only massive trees. A
    // slender acacia a metre inside Elephant Valley's cable still put its crown in the view.
    const auto plant = [&](bool setBack, std::span<const std::string_view> flora, bool understory = false,
                           bool denseUnderstory = false) {
        std::vector<PropId> placed;
        // The last pass is chiefly for walk-through grasses and ferns beneath a woodland
        // canopy.  Keeping it tighter lets shy forest species such as okapi reach their cover
        // band without adding another row of trees or closing the public sight lines.
        for (const float spacing : {3.6f, 2.6f, 2.0f, 1.4f}) {
            for (const Bed& bed : beds) {
                if (density() >= wanted) {
                    return placed;
                }
                const std::uint32_t pick =
                    HashU32(static_cast<std::uint32_t>(bed.tile.x * 4099 + bed.tile.z), salt);
                const std::string_view kind = flora[pick % flora.size()];
                const int def = catalog.PropIndex(kind);
                const Vec2 point = World::TileCenter(bed.tile) + Vec2{script.Random().Range(-0.7f, 0.7f),
                                                                      script.Random().Range(-0.7f, 0.7f)};
                const float scale = small        ? script.Random().Range(0.6f, 0.8f)
                                    : understory ? script.Random().Range(0.55f, 0.85f)
                                                 : script.Random().Range(0.8f, 1.25f);
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
                          small             ? 0.4f
                          : tight           ? 0.8f
                          : denseUnderstory ? 0.25f
                          : understory      ? 0.55f
                                            : 1.8f,
                          understory ? spacing * (denseUnderstory ? 0.45f : 0.65f) : spacing)) {
                    continue;
                }
                if (const PropId id = script.Prop(kind, point, heading, scale)) {
                    placed.push_back(id);
                    vegetationM2 += catalog.props[static_cast<std::size_t>(def)].vegetationM2;
                }
            }
        }
        return placed;
    };
    plant(true, style.flora);
    // A small habitat with a long front can run short of cover after every tree is set back. Fill
    // its remaining suitable beds with the style's low layer before considering a closer trunk.
    // Rhino Hollow and Red Dune keep the shrubs from the first pass and add walk-through grass.
    // This preserves the open crown line and is characteristic ground cover for their residents.
    if (density() < target - kCoverBand + kCoverMargin) {
        std::vector<std::string_view> low;
        for (const std::string_view kind : style.flora) {
            const int def = catalog.PropIndex(kind);
            if (def >= 0 && catalog.props[static_cast<std::size_t>(def)].role != PropRole::Tree &&
                !catalog.props[static_cast<std::size_t>(def)].blocksWalking) {
                low.push_back(kind);
            }
        }
        // A future style without grass still gets a chance to use its ordinary low shrubs.
        if (low.empty()) {
            for (const std::string_view kind : style.flora) {
                const int def = catalog.PropIndex(kind);
                if (def >= 0 && catalog.props[static_cast<std::size_t>(def)].role != PropRole::Tree) {
                    low.push_back(kind);
                }
            }
        }
        if (!low.empty()) {
            plant(true, low, true);
        }
        if (density() < target - kCoverBand + kCoverMargin && plan.species == "lynx") {
            // Grasses fit between the trunks at the woodland edge without blocking a sight line.
            constexpr std::array<std::string_view, 1> grasses{"tall_grass"};
            plant(true, grasses);
        }
    }
    // A large estate has enough combinations of seed, water, shelter and furnishings that the
    // ordinary low layer can still run short. Keep every established tree behind the viewing
    // set-back and fill its remaining gaps with walk-through ground cover. The old fallback
    // removed those trees and replanted them closer to the barrier, contradicting that invariant.
    if (density() < target - kCoverBand + kCoverMargin) {
        const std::string_view kind = style.ground == Terrain::ForestFloor ? "forest_fern" : "tall_grass";
        const std::array<std::string_view, 1> groundCover{kind};
        plant(true, groundCover, true, true);
    }
    // Short of the aim is a problem only where the residents would feel it (Animals.cpp: welfare
    // counts a habitat as too bare beyond 0.15 under the species' figure).
    if (density() < target - kCoverBand) {
        script.Problem(std::string(plan.name) + ": planting target not met (" + std::to_string(density()) +
                       " of " + std::to_string(wanted) + ")");
    }
}

// Small reed stands root in the wetlands' sloping, still-dry margins. Their whole catalog circle
// remains outside the water; unlike trees and ordinary bushes they may use ground whose shared
// corner the pool lowered. Hashes spread them around the shore without consuming the sequence used
// to stock the residents.
void ShorelineReeds(Zoo& zoo, Script& script, const Site& site) {
    if (site.plan->species != "flamingo" && site.plan->species != "capybara") {
        return;
    }
    const int def = zoo.Definitions().PropIndex("wetland_reeds");
    if (def < 0) {
        script.Problem("unknown prop wetland_reeds");
        return;
    }
    const std::uint32_t salt = Salt(zoo, *site.plan) ^ 0x7b91e2d5U;
    std::vector<TileCoord> bank;
    for (const TileCoord tile : site.tiles) {
        if (Interior(zoo, tile, site.id, 0) && !zoo.Map().HasWater(tile) && BedReaches(zoo.Map(), tile) &&
            !site.kept.contains({tile.z, tile.x})) {
            bank.push_back(tile);
        }
    }
    std::stable_sort(bank.begin(), bank.end(), [salt](TileCoord a, TileCoord b) {
        const auto rank = [salt](TileCoord tile) {
            return HashU32(static_cast<std::uint32_t>(tile.x * 4099 + tile.z), salt);
        };
        return rank(a) < rank(b);
    });
    const Builder builder = zoo.FreeBuilder();
    int placed = 0;
    for (const TileCoord tile : bank) {
        const std::uint32_t hash =
            HashU32(static_cast<std::uint32_t>(tile.x * 4099 + tile.z), salt ^ 0x53a0c61bU);
        const Vec2 offset{(HashFloat(hash, salt) - 0.5f) * 0.5f,
                          (HashFloat(hash, salt ^ 0xd16ea37bU) - 0.5f) * 0.5f};
        const Vec2 point = World::TileCenter(tile) + offset;
        const float scale = 0.85f + 0.3f * HashFloat(hash, salt ^ 0x8f24b6cdU);
        bool inView = false;
        for (const Vec2 from : site.watched) {
            inView = inView || DistanceToLine(point, from, site.middle) < site.sight;
        }
        if (inView || !Fits(zoo, builder, site, def, point, scale, 0.35f, 1.6f)) {
            continue;
        }
        const float heading = HashFloat(hash, salt ^ 0x386d941fU) * kTwoPi;
        if (script.Prop("wetland_reeds", point, heading, scale) && ++placed == 12) {
            break;
        }
    }
    if (placed < 8) {
        script.Problem(std::string(site.plan->name) + ": too few reed stands (" + std::to_string(placed) +
                       ")");
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
    int minimumTroughs = 1;
    if (const int index = zoo.SpeciesList().IndexOf(plan.species);
        index >= 0 && zoo.SpeciesList().All()[static_cast<std::size_t>(index)].shyness >= 0.6f) {
        // Quiet northern species should never have to pass a keeper or a dominant group-mate to
        // reach their only drink. Two separated water points also keep one blocked route harmless.
        minimumTroughs = 2;
    }
    const int troughs = std::max(minimumTroughs, static_cast<int>(std::ceil(water * 0.75f / 180.0f)));
    const std::string_view drinkingPoint =
        (plan.species == "aldabra_tortoise" || plan.species == "mandarin_duck" || Terrarium(plan.species))
            ? "drinking_basin"
            : "water_trough";
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
        const std::string_view kind = trough ? drinkingPoint : table[next];
        if (!Settle(zoo, script, front, kind, target, site.clear)) {
            // A narrow overlap gives large provisions one more visible row in a water-heavy
            // habitat without changing the usual seeded layout when the strict front has room.
            Site visible = site;
            const float overlap = 0.16f * static_cast<float>(site.plan->rn) * kTileSize;
            std::erase_if(visible.tiles, [&site, overlap](TileCoord tile) {
                return World::TileCenter(tile).z < site.middle.z - overlap;
            });
            Need(zoo, script, visible, kind, target, site.clear);
        }
        next += trough ? 0U : 1U;
    }

    if (plan.species == "aldabra_tortoise" || SteppeEquid(plan.species)) {
        // Residents can drink near the shelter as well as at the main viewing front.
        Need(zoo, script, site, drinkingPoint, Share(site, -0.38f, 0.46f), site.clear);
    }
    const Vec2 rise = Rise(script, plan);
    if (small || plan.species == "mandarin_duck") {
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
    float enrichment = zoo.Habitats().Find(site.id)->stats.enrichment;
    const auto have = [&] { return enrichment; };
    const auto credit = [&](std::string_view kind) {
        const int def = zoo.Definitions().PropIndex(kind);
        if (def >= 0) {
            enrichment += zoo.Definitions().props[static_cast<std::size_t>(def)].enrichment;
        }
    };
    // A log pile made for a bear dwarfs a bird or a meerkat: theirs are the smallest the catalog makes.
    const std::string_view lives = site.plan->species;
    const float size = lives == "meerkat" || lives == "flamingo" || lives == "penguin" ? 0.7f : 1.0f;
    // Where the ground is nearly full the last ones stand closer, and at last along the barrier.
    Site rim = site;
    rim.margin = std::min(site.margin, 1);
    for (int i = 0; i < 30 && (i < 2 || have() < needed); ++i) {
        const std::string_view toy = style.toys[static_cast<std::size_t>(i) % style.toys.size()];
        const Vec2 place = PlayPlace(site, i);
        bool settled = Settle(zoo, script, site, toy, place, site.clear * 0.75f, size) ||
                       Settle(zoo, script, rim, toy, place, 0.4f, size);
        std::string_view settledKind = toy;
        if (!settled) {
            rim.margin = 0;
            settled = Settle(zoo, script, rim, toy, place, 0.4f, size);
            if (!settled) {
                // In the aviary a compact log pile can fit on a bank too narrow for a fallen trunk.
                if (lives == "mandarin_duck" && toy != "log_pile" &&
                    Settle(zoo, script, rim, "log_pile", place, 0.4f, size)) {
                    settled = true;
                    settledKind = "log_pile";
                } else {
                    break;
                }
            }
        }
        if (settled) {
            credit(settledKind);
        }
    }
    if (lives == "capybara" && have() < needed * 0.95f) {
        // Capybara Marsh has little dry ground after its feeding bank, pool and wetland planting.
        // Preserve the global construction RNG (and therefore every later habitat) by replacing a
        // few deterministic low plants in-place instead of running another random placement pass.
        std::vector<PropId> candidates;
        for (const Prop& prop : zoo.Objects().Props()) {
            if (zoo.Habitats().At(World::TileAt(prop.position)) == site.id) {
                const PropDef& def = zoo.Definitions().props[static_cast<std::size_t>(prop.def)];
                if (def.role == PropRole::Bush) {
                    candidates.push_back(prop.id);
                }
            }
        }
        for (const PropId id : candidates) {
            if (have() >= needed * 0.95f) {
                break;
            }
            const Prop* found = zoo.Objects().FindProp(id);
            if (found == nullptr) {
                continue;
            }
            const Prop old = *found;
            const std::string oldKind = zoo.Definitions().props[static_cast<std::size_t>(old.def)].id;
            Builder builder = zoo.FreeBuilder();
            if (!builder.RemoveProp(id).Ok()) {
                continue;
            }
            --script.Report().props;
            if (script.Prop("ball", old.position, old.heading, old.scale, true)) {
                credit("ball");
            } else {
                script.Prop(oldKind, old.position, old.heading, old.scale, true);
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
        // On a wet or densely furnished habitat, random attempts can miss the small usable bank.
        // Do not leave a hole in the family: the habitat tiles provide a deterministic exhaustive
        // fallback without consuming more of the construction RNG.
        if (!id) {
            for (const TileCoord tile : site.tiles) {
                const Vec2 at = World::TileCenter(tile);
                if (free(at)) {
                    id = zoo.Fauna().Add(env, species, resident.sex, resident.age, at);
                    if (id) {
                        break;
                    }
                }
            }
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
            const auto sire = std::ranges::find_if(family, [&zoo](AnimalId relative) {
                const Animal* animal = zoo.Fauna().Find(relative);
                return animal != nullptr && animal->sex == Sex::Male;
            });
            young->father = sire != family.end() ? *sire : AnimalId{};
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

/// Replace only the habitat edges that face the reserved strip. The rear service apron keeps
/// its cable fence and gate, while the public circuit sees an uninterrupted dry moat.
void DryMoatFront(Zoo& zoo, Script& script, const Site& site) {
    std::vector<EdgeRef> edges;
    std::vector<TileCoord> ditch;
    for (const TileCoord tile : PublicOutline(script, *site.plan)) {
        if (zoo.Map().At(tile).standOff) {
            ditch.push_back(tile);
        }
    }
    for (const TileCoord tile : site.tiles) {
        for (const Dir side : {Dir::North, Dir::East, Dir::South, Dir::West}) {
            const TileCoord next = tile + Step(side);
            if (zoo.Map().InBounds(next) && zoo.Map().At(next).standOff) {
                edges.push_back(EdgeOf(tile, side));
            }
        }
    }
    Builder builder = zoo.FreeBuilder();
    const auto painted = builder.PaintTerrain(ditch, Terrain::Rock);
    const auto fenced = builder.PlaceFence(edges, script.FenceKind("dry_moat"));
    if (!painted.Ok() || !fenced.Ok() || fenced.count == 0 || fenced.skipped > 0) {
        script.Problem(std::string(site.plan->name) + ": incomplete dry moat");
    }
}

void HabitatShell(Zoo& zoo, Script& script, const HabitatPlan& plan) {
    Site site;
    site.plan = &plan;
    site.tiles = RegionTiles(script, plan);
    const HabitatStyle style = Style(plan.species);
    script.PaintTiles(site.tiles, style.ground);
    site.id = script.EncloseTiles(site.tiles, style.fence, std::string(plan.name));
    if (!site.id) {
        return;
    }
    const int stripDepth = StandOffDepth(plan.species);
    if (stripDepth > 0) {
        StandOffWithService(zoo, script, site, stripDepth);
        if (style.front == "dry_moat") {
            DryMoatFront(zoo, script, site);
        }
    } else if (Terrarium(plan.species)) {
        script.ViewingFront(site.tiles, style.front, {Dir::South, Dir::East, Dir::West});
    } else {
        script.ViewingFront(site.tiles, style.front, {Dir::South, plan.east < 0 ? Dir::East : Dir::West});
    }
    script.KeeperGate(site.tiles, style.fence);
    if (plan.species == "mandarin_duck") {
        const int north = plan.north - 2;
        Builder builder = zoo.FreeBuilder();
        for (Dir side : {Dir::West, Dir::East}) {
            const int edge = plan.east + (side == Dir::West ? -plan.rx : plan.rx);
            const auto door = EdgeOf(script.Tile(edge, north), side);
            const auto result =
                builder.PlaceFence(std::span(&door, 1), script.FenceKind("aviary_curtain"), true);
            if (!result.Ok())
                script.Problem(std::string(plan.name) + ": cannot place visitor curtain");
        }
        script.Path(plan.east - plan.rx - 1, north, plan.east + plan.rx + 1, north, PathKind::Boardwalk);
        script.Prop("aviary_net", script.Point(plan.east, plan.north));
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
    site.id = zoo.Habitats().At(script.Tile(plan.east, plan.north));
    if (!site.id) {
        script.Problem(std::string(plan.name) + ": shell is missing");
        return;
    }
    if (plan.species == "mandarin_duck") {
        // Two visitor curtains keep the flock inside the mesh. The public walk cuts between the
        // feeding bank and the pond. Birds can cross it anywhere inside their habitat.
        const int north = plan.north - 2;
        for (int east = plan.east - plan.rx; east <= plan.east + plan.rx; ++east) {
            const TileCoord tile = script.Tile(east, north);
            site.kept.emplace(tile.z, tile.x);
        }
    }
    const bool small = plan.rx <= 5;
    if (small || plan.species == "mandarin_duck" || species.waterAreaFraction >= 0.3f) {
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
    // The remaining edits cannot merge or split an enclosure. HabitatPlants and Enrich maintain
    // their own running cover figures, so the caller can commit every habitat's shelter,
    // pool-furnishing and planting work with one statistics rebuild for the whole estate.
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
    ShorelineReeds(zoo, script, site);
    Enrich(zoo, script, site, style);
    for (const std::string_view resident : Residents(plan)) {
        if (!Omitted(options, resident)) {
            Stock(zoo, script, site, resident);
        }
    }
}

void StripPlants(Zoo& zoo, Script& script, std::span<const HabitatPlan> habitats) {
    // Plant after the animal groups so decorating the public edge consumes none of
    // the script random sequence used for residents or habitat vegetation.
    for (const HabitatPlan& plan : habitats) {
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

/// Cut the V-shaped earthwork after its sparse scrub and rocks have been placed on level ground.
/// Only vertices surrounded by four reserved tiles move: habitat and public-path lips therefore
/// retain their exact heights, areas and walking grades. Two strip tiles make the ditch 4 m wide.
void GradeDryMoats(Zoo& zoo, Script& script, std::span<const HabitatPlan> habitats) {
    for (const HabitatPlan& plan : habitats) {
        if (plan.species != "elephant" && plan.species != "rhino") {
            continue;
        }
        std::set<std::pair<int, int>> ditch;
        for (const TileCoord tile : PublicOutline(script, plan)) {
            if (zoo.Map().At(tile).standOff) {
                ditch.emplace(tile.z, tile.x);
            }
        }
        int lowered = 0;
        for (int vz = 1; vz < zoo.Map().Depth(); ++vz) {
            for (int vx = 1; vx < zoo.Map().Width(); ++vx) {
                bool surrounded = true;
                for (int dz = -1; dz <= 0; ++dz) {
                    for (int dx = -1; dx <= 0; ++dx) {
                        surrounded = surrounded && ditch.contains({vz + dz, vx + dx});
                    }
                }
                if (surrounded) {
                    const Vec2 vertex{static_cast<float>(vx) * kTileSize, static_cast<float>(vz) * kTileSize};
                    if (script.Terraform(Builder::Brush::Raise, vertex, 1.0f, -1.8f)) {
                        ++lowered;
                    }
                }
            }
        }
        if (lowered == 0) {
            script.Problem(std::string(plan.name) + ": dry moat has no lowered centre line");
        }
    }
}

// Elephant Valley and Kangaroo Grove nearly touch at one diagonal corner. Recess the public rail
// around the outermost planted tile there, making the shared walk 4 m wide without moving either
// habitat or reducing the one-tile planted buffer that remains before both containment fences.
void WidenElephantKangarooWalk(Zoo& zoo, Script& script) {
    const TileCoord lane = script.Tile(-21, 37);
    Builder builder = zoo.FreeBuilder();
    const std::array<TileCoord, 1> tiles{lane};
    const BuildPlan unmarked = builder.PlaceStandOff(tiles, false);
    if (!unmarked.Ok() || unmarked.skipped > 0) {
        script.Problem("Elephant Valley/Kangaroo Grove: could not recess the public rail");
        return;
    }

    std::vector<EdgeRef> remove;
    std::vector<EdgeRef> add;
    for (const Dir side : {Dir::North, Dir::East, Dir::South, Dir::West}) {
        const EdgeRef edge = EdgeOf(lane, side);
        const Fence fence = zoo.Map().FenceAt(edge);
        const FenceDef* def = zoo.Definitions().FenceByKind(fence.kind);
        const bool rail = def != nullptr && def->id == "stand_off_rail";
        const TileCoord next = lane + Step(side);
        if (zoo.Map().InBounds(next) && zoo.Map().At(next).standOff) {
            if (!fence.Exists()) {
                add.push_back(edge);
            }
        } else if (rail) {
            remove.push_back(edge);
        }
    }
    const BuildPlan removed = builder.RemoveFence(remove);
    const BuildPlan paved = builder.PlacePath(tiles, PathKind::Gravel);
    const BuildPlan railed = builder.PlaceFence(add, script.FenceKind("stand_off_rail"));
    if (!removed.Ok() || !paved.Ok() || !railed.Ok() || removed.skipped > 0 || paved.skipped > 0 ||
        railed.skipped > 0) {
        script.Problem("Elephant Valley/Kangaroo Grove: incomplete widened viewing bay");
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
    auto candidates = NearFirst(TilesInRect({wanted.x - 18, wanted.z - 18}, {wanted.x + 18, wanted.z + 18}),
                                World::TileCenter(wanted));
    // Prefer a door that opens directly onto an already connected walk.  A diagonal avenue can
    // otherwise be geometrically near while the little rectangular apron between it and the door
    // is cut by planting or a habitat stand-off.  Keep the wider search as a fallback for the few
    // old-zoo kiosks that intentionally sit at the end of a short paved forecourt.
    const TileCoord gate = zoo.Map().EntranceTile();
    const auto opensOnConnectedWalk = [&](TileCoord origin) {
        const TileCoord front = FrontTile(def, origin, facing);
        return zoo.Map().InBounds(front) && zoo.Map().At(front).path != PathKind::None &&
               zoo.Routes().Reachable(gate, front, NavProfile::Visitor());
    };
    std::stable_sort(candidates.begin(), candidates.end(), [&](TileCoord a, TileCoord b) {
        return opensOnConnectedWalk(a) && !opensOnConnectedWalk(b);
    });
    for (const TileCoord origin : candidates) {
        bool clear = true;
        const TileCoord far{origin.x + size.x - 1, origin.z + size.z - 1};
        for (const TileCoord tile : TilesInRect(origin, far)) {
            clear = clear && zoo.Map().InBounds(tile) && zoo.Habitats().RegionAt(tile) == Region::Public &&
                    zoo.Map().At(tile).path == PathKind::None;
        }
        // Script::Building levels the chosen footprint before placement.  A candidate whose only
        // fault is the untouched estate's relief is therefore viable; rejecting it here used to
        // leave whole western districts without services even though the automatic levelling
        // would have made the same site legal a moment later.
        const BuildError building = builder.CheckBuilding(index, origin, facing);
        if (!clear || (building != BuildError::None && building != BuildError::TooSteep)) {
            continue;
        }
        const TileCoord front = FrontTile(def, origin, facing);
        if (!zoo.Map().InBounds(front) || zoo.Habitats().RegionAt(front) != Region::Public ||
            (zoo.Map().At(front).path == PathKind::None &&
             builder.CheckPath(front, PathKind::Cobblestone) != BuildError::None)) {
            continue;
        }
        const auto path = zoo.Routes().NearestPassable(front, NavProfile::Visitor(), 8);
        if (!path) {
            continue;
        }
        if (!zoo.Routes().Reachable(gate, *path, NavProfile::Visitor())) {
            continue;
        }
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
        const TileCoord apronLow{std::min(front.x, path->x), std::min(front.z, path->z)};
        const TileCoord apronHigh{std::max(front.x, path->x), std::max(front.z, path->z)};
        std::vector<TileCoord> apron;
        for (const TileCoord tile : TilesInRect(apronLow, apronHigh)) {
            if (zoo.Habitats().RegionAt(tile) == Region::Public &&
                builder.CheckPath(tile, PathKind::Cobblestone) == BuildError::None) {
                apron.push_back(tile);
            }
        }
        script.PathTiles(apron, PathKind::Cobblestone);
        wideAprons.push_back({low, high});
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

std::string_view ParkTree(int east, int north, int variant) {
    if (std::abs(east) > 106) {
        return variant == 0 ? "birch" : "pine";
    }
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
            if (habitat.name != "Corn Snake Terrarium" && habitat.name != "Python Terrarium") {
                ShadeTree(zoo, script, spot);
            }
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

void Facilities(Zoo& zoo, Script& script, bool sideGardens, bool steppeGardens, bool northernExpansion,
                bool continentalExpansion) {
    std::vector<ServiceApron> serviceAprons;
    script.PathTiles(script.Ellipse(13, 74, 10.0f, 4.0f), PathKind::Cobblestone);
    // A second shaded rest place makes the new northern circuit a destination rather than an
    // empty perimeter path. Its two services are placed below before furniture fills the glade.
    script.PathAlong(Script::Arc(0, 164, 9.0f, 5.0f, 0.0f, kTwoPi), 2, PathKind::Cobblestone);
    if (sideGardens) {
        script.PathTiles(script.Ellipse(96, 80, 5.0f, 4.0f), PathKind::Cobblestone);
        script.PathTiles(script.Ellipse(-97, 80, 5.0f, 4.0f), PathKind::Cobblestone);
    }
    if (northernExpansion) {
        // The pavilions below stand on the lawn between paired cross-zoo avenues. Their own
        // cobbled aprons join both sides without paving over the building sites first.
    }
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
    Service(zoo, script, serviceAprons, "restroom", -10, 164, Dir::South);
    Service(zoo, script, serviceAprons, "drinks_stall", 10, 164, Dir::South);
    Service(zoo, script, serviceAprons, "drinks_stall", -30, 167, Dir::South);
    Service(zoo, script, serviceAprons, "restroom", -22, 188, Dir::South);
    Service(zoo, script, serviceAprons, "drinks_stall", 22, 188, Dir::South);
    Service(zoo, script, serviceAprons, "drinks_stall", 66, 123, Dir::West);
    Service(zoo, script, serviceAprons, "drinks_stall", 66, 91, Dir::West);
    Service(zoo, script, serviceAprons, "ice_cream_stand", -67, 84, Dir::East);
    if (steppeGardens) {
        for (const int side : {-1, 1}) {
            Service(zoo, script, serviceAprons, "drinks_stall", side * 122, 75, Dir::South);
            Service(zoo, script, serviceAprons, "restroom", side * 151, 106, Dir::South);
            Service(zoo, script, serviceAprons, "ice_cream_stand", side * 173, 65, Dir::North);
            Service(zoo, script, serviceAprons, "drinks_stall", side * 108, 44,
                    side < 0 ? Dir::East : Dir::West);
            Service(zoo, script, serviceAprons, "restroom", side * 186, 53, Dir::South);
            Service(zoo, script, serviceAprons, "drinks_stall", side * 181, 116, Dir::South);
            Service(zoo, script, serviceAprons, "ice_cream_stand", side * 154, 172, Dir::North);
            Service(zoo, script, serviceAprons, "restroom", side * 176, 202, Dir::South);
        }
    }
    if (continentalExpansion) {
        // Australia's four stops follow actual vertices of its gravel and wetland trails. The
        // generic centre targets belonged to the removed middle avenue and could leave an apron
        // facing an isolated remnant of it.
        Service(zoo, script, serviceAprons, "restroom", -610, 112, Dir::East);
        Service(zoo, script, serviceAprons, "drinks_stall", -510, 100, Dir::South);
        Service(zoo, script, serviceAprons, "ice_cream_stand", -379, 103, Dir::North);
        Service(zoo, script, serviceAprons, "burger_stall", -250, 132, Dir::West);
        // South-east stops sit on literal savanna, predator-link and river vertices.
        Service(zoo, script, serviceAprons, "restroom", 246, 105, Dir::South);
        Service(zoo, script, serviceAprons, "drinks_stall", 367, 99, Dir::North);
        Service(zoo, script, serviceAprons, "ice_cream_stand", 486, 102, Dir::South);
        Service(zoo, script, serviceAprons, "burger_stall", 605, 137, Dir::North);
        // The northern Lion Savanna gallery is a long perimeter walk from the four trail stops.
        Service(zoo, script, serviceAprons, "drinks_stall", 349, 191, Dir::South);
        // American stops follow the prairie, oxbow and cross-link vertices.
        Service(zoo, script, serviceAprons, "restroom", -584, 286, Dir::South);
        Service(zoo, script, serviceAprons, "drinks_stall", -510, 315, Dir::North);
        Service(zoo, script, serviceAprons, "ice_cream_stand", -435, 320, Dir::South);
        Service(zoo, script, serviceAprons, "burger_stall", -250, 348, Dir::North);
        // Long perimeter galleries on opposite sides of the two meanders need their own small stops.
        Service(zoo, script, serviceAprons, "drinks_stall", -369, 239, Dir::North);
        Service(zoo, script, serviceAprons, "restroom", -569, 400, Dir::South);
        // Asian stops follow mountain, temple-crossing and monsoon vertices.
        Service(zoo, script, serviceAprons, "restroom", 254, 273, Dir::South);
        Service(zoo, script, serviceAprons, "drinks_stall", 370, 320, Dir::North);
        Service(zoo, script, serviceAprons, "ice_cream_stand", 490, 322, Dir::South);
        Service(zoo, script, serviceAprons, "burger_stall", 574, 340, Dir::North);
        Service(zoo, script, serviceAprons, "drinks_stall", 229, 318, Dir::East);
        // Remote sides of three large rings otherwise exceed the 360 m service-walk limit.
        Service(zoo, script, serviceAprons, "drinks_stall", 429, 229, Dir::North);
        Service(zoo, script, serviceAprons, "restroom", 424, 410, Dir::South);
        Service(zoo, script, serviceAprons, "ice_cream_stand", 279, 400, Dir::South);
        // Four stops per other sheet follow different bends of each circuit. They are deliberately
        // offset rather than stamped at page centres, and Service snaps each door to a connected
        // walk before accepting the site.
        for (const auto& [east, north] :
             std::array<std::pair<int, int>, 3>{{{-424, 530}, {0, 530}, {424, 530}}}) {
            Service(zoo, script, serviceAprons, "restroom", east - 150, north + 2, Dir::South);
            Service(zoo, script, serviceAprons, "drinks_stall", east - 55, north - 3, Dir::North);
            Service(zoo, script, serviceAprons, "ice_cream_stand", east + 55, north + 3, Dir::South);
            Service(zoo, script, serviceAprons, "burger_stall", east + 150, north - 2, Dir::North);
        }
        // Large exhibit rings can put the opposite viewing side hundreds of metres along the
        // path from a central kiosk. These compact stops sit on the actual habitat branches.
        Service(zoo, script, serviceAprons, "drinks_stall", 359, 56, Dir::South);
        Service(zoo, script, serviceAprons, "drinks_stall", 594, 151, Dir::West);
        Service(zoo, script, serviceAprons, "restroom", 359, 268, Dir::South);
        Service(zoo, script, serviceAprons, "drinks_stall", 594, 363, Dir::West);
        Service(zoo, script, serviceAprons, "ice_cream_stand", -349, 579, Dir::North);
        Service(zoo, script, serviceAprons, "drinks_stall", 75, 580, Dir::North);
        Service(zoo, script, serviceAprons, "drinks_stall", 424, 530, Dir::South);
        Service(zoo, script, serviceAprons, "restroom", 359, 478, Dir::South);
    }
    if (northernExpansion) {
        Service(zoo, script, serviceAprons, "americas_lodge", -127, 280, Dir::South);
        Service(zoo, script, serviceAprons, "tundra_station", 0, 280, Dir::South);
        Service(zoo, script, serviceAprons, "african_plains_pavilion", 127, 280, Dir::South);
        Service(zoo, script, serviceAprons, "forest_discovery_house", -127, 350, Dir::South);
        Service(zoo, script, serviceAprons, "education_centre", 127, 350, Dir::South);
        Service(zoo, script, serviceAprons, "burger_stall", -42, 282, Dir::North);
        Service(zoo, script, serviceAprons, "drinks_stall", 42, 282, Dir::North);
        Service(zoo, script, serviceAprons, "ice_cream_stand", -50, 350, Dir::North);
        Service(zoo, script, serviceAprons, "drinks_stall", 50, 350, Dir::North);
        // Stops follow the bends of the perimeter and two inner paths.  Their spacing is dense
        // enough for visitors but does not reveal the plan-sheet grid in the landscape.
        int stop = 0;
        for (const auto& [east, north] : std::array<std::pair<int, int>, 18>{{{-187, 231},
                                                                              {-130, 214},
                                                                              {-68, 220},
                                                                              {-8, 214},
                                                                              {53, 224},
                                                                              {115, 213},
                                                                              {172, 228},
                                                                              {194, 282},
                                                                              {181, 339},
                                                                              {172, 397},
                                                                              {112, 412},
                                                                              {48, 416},
                                                                              {-18, 411},
                                                                              {-82, 414},
                                                                              {-145, 407},
                                                                              {-189, 365},
                                                                              {-187, 305},
                                                                              {-122, 351}}}) {
            const std::string_view kind = stop % 3 == 0   ? "restroom"
                                          : stop % 3 == 1 ? "drinks_stall"
                                                          : "ice_cream_stand";
            Service(zoo, script, serviceAprons, kind, east, north, Dir::South);
            ++stop;
        }
    }
    if (sideGardens) {
        Service(zoo, script, serviceAprons, "restroom", 92, 82, Dir::East);
        Service(zoo, script, serviceAprons, "drinks_stall", 92, 77, Dir::East);
        Service(zoo, script, serviceAprons, "restroom", -92, 82, Dir::West);
        Service(zoo, script, serviceAprons, "ice_cream_stand", -92, 77, Dir::West);
    }
    WidenServiceAprons(zoo, script, serviceAprons);
    WidenThinWalks(zoo, script);
    TrimTeeth(zoo, script);
    for (int n = 72; n <= 76; n += 4) {
        for (int e = 6; e <= 21; e += 3) {
            PublicProp(zoo, script, "picnic_table", script.Point(e, n));
        }
    }
    for (const int north : {161, 163, 165}) {
        for (const int east : {-5, -3, 3, 5}) {
            PublicProp(zoo, script, "picnic_table", script.Point(east, north));
        }
    }
    for (const int east : {-7, 7}) {
        PublicProp(zoo, script, "bench", script.Point(east, 164), east < 0 ? -kPi * 0.5f : kPi * 0.5f);
        PublicProp(zoo, script, "litter_bin", script.Point(east, 162));
        PublicProp(zoo, script, "lamp", script.Point(east, 166));
    }
    if (sideGardens) {
        for (const int side : {-1, 1}) {
            for (const int north : {78, 82}) {
                PublicProp(zoo, script, "picnic_table", script.Point(side < 0 ? -103 : 102, north));
            }
            PublicProp(zoo, script, "bench", script.Point(side * 101, 80),
                       side < 0 ? -kPi * 0.5f : kPi * 0.5f);
            PublicProp(zoo, script, "litter_bin", script.Point(side * 100, 77));
        }
    }
    if (northernExpansion) {
        for (const int north : {280, 350}) {
            for (const int east : {-133, -129, 129, 133}) {
                PublicProp(zoo, script, "picnic_table", script.Point(east, north));
            }
        }
    }
    PublicProp(zoo, script, "info_board", script.Point(0, 168), kPi);
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
    const TileCoord entrance = map.EntranceTile();
    for (int z = map.PropertyMin().z; z <= map.PropertyMax().z; ++z) {
        for (int x = map.PropertyMin().x; x <= map.PropertyMax().x; ++x) {
            const TileCoord tile{x, z};
            if (map.At(tile).path == PathKind::None || (x + 3 * z) % 13 != 0) {
                continue;
            }
            // The player's opening run uses the centre of the entrance avenue. Explicit lamps
            // already light this forecourt; random path-edge furniture must leave it unobstructed.
            const int northOfGate = entrance.z - z;
            if (std::abs(x - entrance.x) <= 2 && northOfGate >= 0 && northOfGate <= 10) {
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

void GroundDetail(Zoo& zoo, Script& script, std::span<const HabitatPlan> habitats) {
    Builder builder = zoo.FreeBuilder();
    for (const HabitatPlan& plan : habitats) {
        const EnclosureId id = zoo.Habitats().At(script.Tile(plan.east, plan.north));
        if (!id || Terrarium(plan.species) || plan.species == "mandarin_duck") {
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
        if (plan.species == "polar_bear" || plan.species == "penguin" || plan.species == "brown_bear" ||
            plan.species == "snow_leopard") {
            terrain = Terrain::Rock;
        } else if (plan.species == "tiger" || plan.species == "wolf") {
            terrain = Terrain::Grass;
        } else if (plan.species == "kangaroo") {
            terrain = Terrain::Savanna;
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
    // Nine equal visitor-map sheets form a 3 by 3 estate.  Each sheet is the size of one half of
    // the preceding 424 by 424 property: 424 by 212 tiles.  The resulting 1,272 by 636 tile
    // property is exactly 4.5 times its area, with the usual six-tile scenery margin around it.
    config.world.width = 1272 + 2 * config.world.margin;
    config.world.depth = 636 + 2 * config.world.margin;
    config.world.relief = 0.12f;
    config.sandbox = true;
    config.autoCare = true;
    return config;
}

ScenarioReport BuildVisitorZoo(Zoo& zoo, const VisitorZooOptions& options) {
    Script script(zoo);
    const World& map = zoo.Map();
    const TileCoord gate = map.EntranceTile();
    const int besideGate = std::min(gate.x - map.PropertyMin().x, map.PropertyMax().x - gate.x);
    if (besideGate < 70 || gate.z - map.PropertyMin().z < 211) {
        script.Problem("the Visitor Zoo needs 70 tiles beside the entrance and 211 tiles north of it");
        return script.Finish();
    }
    const bool sideGardens = besideGate >= 103;
    const bool steppeGardens = besideGate >= 185;
    const bool northernExpansion = gate.z - map.PropertyMin().z >= 423;
    const bool continentalExpansion = besideGate >= 600 && gate.z - map.PropertyMin().z >= 635;
    const auto habitats = std::span(kHabitats).first(continentalExpansion ? kHabitats.size()
                                                     : northernExpansion  ? 48
                                                     : steppeGardens      ? 34
                                                     : sideGardens        ? 22
                                                                          : 17);
    script.Hold();
    Walks(zoo, script, habitats, sideGardens, steppeGardens, northernExpansion, continentalExpansion);
    for (const HabitatPlan& plan : habitats) {
        HabitatShell(zoo, script, plan);
    }
    script.Release();
    // All habitat regions, gates and viewing fronts are now stable. Updating their statistics
    // scans the whole map, so defer it across the 118 independent furnishing passes.
    script.Hold();
    for (const HabitatPlan& plan : habitats) {
        Habitat(zoo, script, plan, options);
    }
    script.Release();
    // Animals were stocked while enclosure-stat rebuilds were deliberately deferred. Align their
    // opening welfare once with the now-complete shelter, planting and enrichment figures; from
    // here ordinary simulation smoothing owns it again.
    zoo.Fauna().RefreshWelfare(zoo.MakeEnv());
    WidenElephantKangarooWalk(zoo, script);
    // What follows stands outside the habitats and asks about none of their figures.
    script.Hold();
    StripPlants(zoo, script, habitats);
    GradeDryMoats(zoo, script, habitats);
    if (sideGardens) {
        script.Prop("snake_pavilion", script.Point(82, 123));
    }
    Facilities(zoo, script, sideGardens, steppeGardens, northernExpansion, continentalExpansion);
    GroundDetail(zoo, script, habitats);
    Parkland(zoo, script);
    script.Release();
    // Validate access only after every construction phase has finished. Later path and terrain
    // work can join a newly paved district and invalidate the route cache used during Facilities.
    for (const Building& building : zoo.Objects().Buildings()) {
        const BuildingDef& def = zoo.Definitions().buildings[static_cast<std::size_t>(building.def)];
        if (def.placement != Placement::Public) {
            continue;
        }
        const TileCoord front = FrontTile(def, building.origin, building.facing);
        if (!zoo.Routes().Reachable(gate, front, NavProfile::Visitor())) {
            script.Problem(def.id + ": door is unreachable at " + std::to_string(front.x - gate.x) + "," +
                           std::to_string(gate.z - front.z));
        }
    }
    zoo.Guests().SetTicketPrice(0);
    script.Provision();
    // The crew a visitor sees at work (docs/KEEPERS.md). Every keeper gate opens onto a dry
    // service apron joined to the walks (StandOffWithService), so no lane is cut for them.
    zoo.Keepers().Engage(zoo.MakeEnv(), zoo.Fauna());
    return script.Finish();
}
}  // namespace zoo
