// SPDX-License-Identifier: MIT
#include "zoo/sim/Scenario.hpp"

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <string_view>

#include "zoo/sim/ScenarioScript.hpp"

namespace zoo {

namespace {

using scenario::Script;

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
        return s.Finish();
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
    return s.Finish();
}

}  // namespace zoo
