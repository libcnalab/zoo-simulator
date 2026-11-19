// SPDX-License-Identifier: MIT
#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "TestData.hpp"
#include "zoo/sim/VisitStart.hpp"
#include "zoo/sim/VisitorZoo.hpp"

namespace zoo {
namespace {

struct Rgb {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
};

/// A top-down drawing of the built zoo: the plan is judged on it before it is judged in the game.
class PlanImage {
public:
    PlanImage(const Zoo& zoo, int pixelsPerTile)
        : zoo_(zoo),
          scale_(pixelsPerTile),
          min_(zoo.Map().PropertyMin()),
          width_((zoo.Map().PropertyMax().x - min_.x + 1) * pixelsPerTile),
          height_((zoo.Map().PropertyMax().z - min_.z + 1) * pixelsPerTile),
          pixels_(static_cast<std::size_t>(width_ * height_)) {}

    void Draw() {
        Ground();
        Fences();
        Buildings();
        Props();
        Animals();
    }

    bool Save(const std::string& path) const {
        std::ofstream out(path, std::ios::binary);
        if (!out) {
            return false;
        }
        out << "P6\n" << width_ << ' ' << height_ << "\n255\n";
        for (const Rgb& pixel : pixels_) {
            out.put(static_cast<char>(pixel.r))
                .put(static_cast<char>(pixel.g))
                .put(static_cast<char>(pixel.b));
        }
        return static_cast<bool>(out);
    }

private:
    float X(float worldX) const {
        return (worldX / kTileSize - static_cast<float>(min_.x)) * static_cast<float>(scale_);
    }
    float Y(float worldZ) const {
        return (worldZ / kTileSize - static_cast<float>(min_.z)) * static_cast<float>(scale_);
    }
    void Set(int x, int y, Rgb colour) {
        if (x >= 0 && y >= 0 && x < width_ && y < height_) {
            pixels_[static_cast<std::size_t>(y * width_ + x)] = colour;
        }
    }
    void Rect(float x0, float y0, float x1, float y1, Rgb colour) {
        for (int y = static_cast<int>(std::floor(y0)); y < static_cast<int>(std::ceil(y1)); ++y) {
            for (int x = static_cast<int>(std::floor(x0)); x < static_cast<int>(std::ceil(x1)); ++x) {
                Set(x, y, colour);
            }
        }
    }
    void Disc(Vec2 world, float radiusM, Rgb colour, bool ring = false) {
        const float cx = X(world.x);
        const float cy = Y(world.z);
        const float r = std::max(1.5f, radiusM / kTileSize * static_cast<float>(scale_));
        for (int y = static_cast<int>(cy - r - 1.0f); y <= static_cast<int>(cy + r + 1.0f); ++y) {
            for (int x = static_cast<int>(cx - r - 1.0f); x <= static_cast<int>(cx + r + 1.0f); ++x) {
                const float dx = static_cast<float>(x) + 0.5f - cx;
                const float dy = static_cast<float>(y) + 0.5f - cy;
                const float d = std::sqrt(dx * dx + dy * dy);
                if (ring ? std::abs(d - r) <= 0.6f : d <= r) {
                    Set(x, y, colour);
                }
            }
        }
    }

    void Ground() {
        constexpr Rgb terrain[kTerrainCount] = {{150, 190, 120}, {205, 200, 130}, {170, 140, 105},
                                                {230, 215, 165}, {160, 160, 160}, {240, 245, 250},
                                                {120, 150, 100}, {140, 120, 95}};
        constexpr Rgb paths[kPathKindCount] = {
            {0, 0, 0}, {120, 120, 125}, {215, 195, 160}, {190, 170, 170}, {175, 130, 90}};
        const World& map = zoo_.Map();
        for (int z = min_.z; z <= map.PropertyMax().z; ++z) {
            for (int x = min_.x; x <= map.PropertyMax().x; ++x) {
                const TileCoord tile{x, z};
                const Tile& data = map.At(tile);
                Rgb colour = terrain[static_cast<std::size_t>(data.terrain)];
                if (!zoo_.Habitats().At(tile)) {
                    // Public ground is paler, so a habitat's footprint reads at once.
                    colour = {static_cast<std::uint8_t>((colour.r + 255) / 2),
                              static_cast<std::uint8_t>((colour.g + 255) / 2),
                              static_cast<std::uint8_t>((colour.b + 255) / 2)};
                }
                if (data.standOff) {
                    colour = {120, 200, 120};
                }
                if (data.path != PathKind::None) {
                    colour = paths[static_cast<std::size_t>(data.path)];
                }
                if (map.HasWater(tile)) {
                    colour = map.WaterDepthAt(World::TileCenter(tile)) > 1.0f ? Rgb{40, 90, 200}
                                                                              : Rgb{90, 160, 235};
                }
                const float px = static_cast<float>((x - min_.x) * scale_);
                const float py = static_cast<float>((z - min_.z) * scale_);
                Rect(px, py, px + static_cast<float>(scale_), py + static_cast<float>(scale_), colour);
            }
        }
        for (const Enclosure& habitat : zoo_.Habitats().All()) {
            for (const ViewingSpot& spot : habitat.viewing) {
                const Vec2 centre = World::TileCenter(spot.tile);
                Disc(centre, 0.2f, {255, 255, 255});
            }
        }
    }

    void Fences() {
        const World& map = zoo_.Map();
        for (int z = min_.z; z <= map.PropertyMax().z + 1; ++z) {
            for (int x = min_.x; x <= map.PropertyMax().x + 1; ++x) {
                for (const bool horizontal : {true, false}) {
                    const EdgeRef edge{x, z, horizontal};
                    if (!map.EdgeInBounds(edge)) {
                        continue;
                    }
                    const Fence fence = map.FenceAt(edge);
                    if (!fence.Exists()) {
                        continue;
                    }
                    Rgb colour{90, 30, 30};
                    if (const FenceDef* def = zoo_.Definitions().FenceByKind(fence.kind)) {
                        constexpr Rgb styles[] = {{150, 90, 40}, {20, 20, 20},  {60, 60, 90}, {0, 200, 220},
                                                  {110, 60, 60}, {150, 0, 170}, {0, 110, 0}};
                        colour = styles[static_cast<std::size_t>(def->style)];
                    }
                    if (fence.gate) {
                        colour = {255, 0, 0};
                    }
                    const float px = static_cast<float>((x - min_.x) * scale_);
                    const float py = static_cast<float>((z - min_.z) * scale_);
                    if (horizontal) {
                        Rect(px, py - 1.0f, px + static_cast<float>(scale_), py + 1.0f, colour);
                    } else {
                        Rect(px - 1.0f, py, px + 1.0f, py + static_cast<float>(scale_), colour);
                    }
                }
            }
        }
    }

    void Buildings() {
        for (const Building& building : zoo_.Objects().Buildings()) {
            const BuildingDef& def = zoo_.Definitions().buildings[static_cast<std::size_t>(building.def)];
            const TileCoord size = FootprintSize(def, building.facing);
            const float px = static_cast<float>((building.origin.x - min_.x) * scale_);
            const float py = static_cast<float>((building.origin.z - min_.z) * scale_);
            Rect(px + 1.0f, py + 1.0f, px + static_cast<float>(size.x * scale_) - 1.0f,
                 py + static_cast<float>(size.z * scale_) - 1.0f,
                 def.placement == Placement::Public ? Rgb{200, 60, 60} : Rgb{90, 70, 50});
            Disc(World::TileCenter(FrontTile(def, building.origin, building.facing)), 0.5f, {255, 255, 0});
        }
    }

    void Props() {
        for (const Prop& prop : zoo_.Objects().Props()) {
            const PropDef& def = zoo_.Definitions().props[static_cast<std::size_t>(prop.def)];
            switch (def.role) {
                case PropRole::Tree:
                    Disc(prop.position, std::sqrt(def.shadeM2 / kPi) * prop.scale, {20, 110, 40}, true);
                    Disc(prop.position, def.radiusM * prop.scale, {20, 90, 30});
                    break;
                case PropRole::Bush:
                    Disc(prop.position, def.radiusM * prop.scale, {70, 160, 60});
                    break;
                case PropRole::Rock:
                    Disc(prop.position, def.radiusM * prop.scale, {110, 110, 110});
                    break;
                case PropRole::Feeder:
                    Disc(prop.position, def.radiusM, {255, 130, 0});
                    break;
                case PropRole::WaterTrough:
                    Disc(prop.position, def.radiusM, {0, 60, 255});
                    break;
                case PropRole::Enrichment:
                    Disc(prop.position, def.radiusM, {220, 0, 220});
                    break;
                case PropRole::Bench:
                    Disc(prop.position, def.radiusM, {120, 60, 20});
                    break;
                case PropRole::Lamp:
                    Disc(prop.position, 0.5f, {255, 230, 0});
                    break;
                case PropRole::Bin:
                    Disc(prop.position, 0.5f, {0, 0, 0});
                    break;
                case PropRole::InfoBoard:
                    Disc(prop.position, 0.5f, {255, 255, 255});
                    break;
                default:
                    Disc(prop.position, def.radiusM, {255, 150, 190});
                    break;
            }
        }
    }

    void Animals() {
        const GameTime now = zoo_.Time();
        for (const Animal& animal : zoo_.Fauna().All()) {
            const SpeciesDef& species = zoo_.SpeciesList().All()[static_cast<std::size_t>(animal.species)];
            const bool young = AgeYears(animal, now) < species.maturityYears * 0.5f;
            Disc(animal.position, young ? 0.5f : 0.8f,
                 young                     ? Rgb{255, 255, 0}
                 : animal.sex == Sex::Male ? Rgb{200, 0, 0}
                                           : Rgb{255, 90, 60});
        }
    }

    const Zoo& zoo_;
    int scale_;
    TileCoord min_;
    int width_;
    int height_;
    std::vector<Rgb> pixels_;
};

// Not a check: writes the plan as a picture (ZOO_PLAN_IMAGE, or build-probe/plan.ppm in the source tree).
TEST(VisitorZooPlanTests, DISABLED_WritePlanImage) {
    Zoo zoo(testdata::GameCatalog(), testdata::GameSpecies(), VisitorZooConfig());
    const ScenarioReport report = BuildVisitorZoo(zoo);
    for (const std::string& problem : report.problems) {
        ADD_FAILURE() << problem;
    }
    PlanImage image(zoo, 8);
    image.Draw();
    const char* wanted = std::getenv("ZOO_PLAN_IMAGE");
    const std::string path =
        wanted != nullptr ? std::string(wanted) : std::string(ZOO_SOURCE_DIR) + "/build-probe/plan.ppm";
    ASSERT_TRUE(image.Save(path)) << path;
    std::cout << "PLAN " << path << '\n';
}

/// One resident as the plan names it: sex, age in years on the opening day, whether a mother of
/// the same group is named for it.
struct Named {
    Sex sex;
    float age;
    bool young;
};
constexpr Named Grown(Sex sex, float age) {
    return {sex, age, false};
}
constexpr Named Child(Sex sex, float age) {
    return {sex, age, true};
}
constexpr Sex kMale = Sex::Male;
constexpr Sex kFemale = Sex::Female;

// The families of Greenhill Zoo, written out a second time on purpose: a change of the roster in
// VisitorZoo.cpp has to be made here as well, and in docs/VISITOR_ZOO.md.
const std::map<std::string, std::vector<Named>> kFamilies{
    {"meerkat",
     {Grown(kMale, 4.0f), Grown(kFemale, 5.0f), Grown(kFemale, 3.0f), Child(kMale, 0.15f),
      Child(kFemale, 0.15f)}},
    {"flamingo",
     {Grown(kMale, 9.0f), Grown(kMale, 14.0f), Grown(kMale, 7.0f), Grown(kMale, 20.0f), Grown(kMale, 11.0f),
      Grown(kFemale, 10.0f), Grown(kFemale, 16.0f), Grown(kFemale, 6.5f), Grown(kFemale, 12.0f),
      Grown(kFemale, 8.0f), Child(kFemale, 0.8f), Child(kMale, 0.9f)}},
    {"giraffe",
     {Grown(kMale, 9.0f), Grown(kFemale, 8.0f), Grown(kFemale, 12.0f), Child(kFemale, 3.0f),
      Child(kFemale, 0.8f)}},
    {"zebra",
     {Grown(kMale, 8.0f), Grown(kFemale, 6.0f), Grown(kFemale, 9.0f), Grown(kFemale, 12.0f),
      Child(kFemale, 1.2f), Child(kMale, 0.4f)}},
    {"ostrich",
     {Grown(kMale, 7.0f), Grown(kFemale, 6.0f), Grown(kFemale, 8.0f), Grown(kFemale, 5.0f),
      Child(kMale, 0.3f), Child(kFemale, 0.3f), Child(kFemale, 0.3f)}},
    {"lion",
     {Grown(kMale, 7.0f), Grown(kFemale, 6.0f), Grown(kFemale, 9.0f), Child(kFemale, 2.6f),
      Child(kMale, 0.6f), Child(kFemale, 0.6f), Child(kFemale, 0.6f)}},
    {"tiger", {Grown(kMale, 7.0f), Grown(kFemale, 6.0f)}},
    {"wolf",
     {Grown(kMale, 6.0f), Grown(kFemale, 5.0f), Child(kFemale, 1.3f), Child(kMale, 0.25f),
      Child(kFemale, 0.25f), Child(kMale, 0.25f)}},
    {"brown_bear", {Grown(kMale, 12.0f), Grown(kFemale, 10.0f)}},
    {"polar_bear", {Grown(kMale, 11.0f), Grown(kFemale, 9.0f)}},
    {"penguin",
     {Grown(kMale, 5.0f), Grown(kMale, 8.0f), Grown(kMale, 11.0f), Grown(kMale, 6.0f), Grown(kMale, 14.0f),
      Grown(kFemale, 6.0f), Grown(kFemale, 9.0f), Grown(kFemale, 12.0f), Grown(kFemale, 5.0f),
      Grown(kFemale, 10.0f), Child(kFemale, 0.25f), Child(kMale, 0.25f), Child(kFemale, 0.25f)}},
    {"elephant",
     {Grown(kFemale, 26.0f), Grown(kFemale, 38.0f), Grown(kFemale, 22.0f), Child(kFemale, 7.0f),
      Child(kMale, 1.5f)}},
    {"rhino", {Grown(kMale, 14.0f), Grown(kFemale, 16.0f), Grown(kFemale, 10.0f), Child(kFemale, 1.0f)}},
    {"camel",
     {Grown(kMale, 9.0f), Grown(kFemale, 8.0f), Grown(kFemale, 13.0f), Child(kFemale, 2.0f),
      Child(kMale, 0.7f)}},
    {"kangaroo",
     {Grown(kMale, 6.0f), Grown(kFemale, 4.0f), Grown(kFemale, 5.0f), Grown(kFemale, 7.0f),
      Child(kFemale, 1.2f), Child(kMale, 0.7f), Child(kFemale, 0.65f)}},
};

// What stands between the hoofstock and the visitor, by habitat; nothing of it is chain-link.
const std::map<std::string, std::string> kBarriers{
    {"Acacia Plain", "tall_cable_fence"}, {"Ostrich Rise", "tall_cable_fence"},
    {"Elephant Valley", "cable_barrier"}, {"Rhino Hollow", "cable_barrier"},
    {"Red Dune", "paddock_rail"},         {"Kangaroo Grove", "paddock_rail"},
};

const SpeciesDef& SpeciesOf(const Zoo& zoo, const Animal& animal) {
    return zoo.SpeciesList().All()[static_cast<std::size_t>(animal.species)];
}

const PropDef& KindOf(const Zoo& zoo, const Prop& prop) {
    return zoo.Definitions().props[static_cast<std::size_t>(prop.def)];
}

bool IsPlant(const PropDef& kind) {
    return kind.role == PropRole::Tree || kind.role == PropRole::Bush;
}

/// The animals of one species in the order they were set down.
std::vector<const Animal*> Group(const Zoo& zoo, const std::string& species) {
    std::vector<const Animal*> group;
    for (const Animal& animal : zoo.Fauna().All()) {
        if (SpeciesOf(zoo, animal).id == species) {
            group.push_back(&animal);
        }
    }
    return group;
}

/// Whether the circle of `radius` round `point` reaches into the tile.
bool Touches(Vec2 point, float radius, TileCoord tile) {
    const float x = static_cast<float>(tile.x) * kTileSize;
    const float z = static_cast<float>(tile.z) * kTileSize;
    const float dx = point.x - std::clamp(point.x, x, x + kTileSize);
    const float dz = point.z - std::clamp(point.z, z, z + kTileSize);
    return dx * dx + dz * dz < radius * radius;
}

float ToSegment(Vec2 point, Vec2 from, Vec2 to) {
    const Vec2 along = to - from;
    const float length = along.x * along.x + along.z * along.z;
    const float t =
        length > 0.0f
            ? std::clamp(((point.x - from.x) * along.x + (point.z - from.z) * along.z) / length, 0.0f, 1.0f)
            : 0.0f;
    return Distance(point, from + along * t);
}

Vec2 Middle(const Zoo& zoo, const Enclosure& home) {
    Vec2 sum{};
    int count = 0;
    for (const TileCoord tile : TilesInRect(home.min, home.max)) {
        if (zoo.Habitats().At(tile) == home.id) {
            sum = sum + World::TileCenter(tile);
            ++count;
        }
    }
    return sum * (1.0f / static_cast<float>(std::max(1, count)));
}

/// The benches that look into a habitat: the bench nearest to its outermost viewing place to the
/// south, to the west and to the east.
std::vector<const Prop*> ViewingBenches(const Zoo& zoo, const Enclosure& home) {
    std::vector<const Prop*> benches;
    const Vec2 centre = World::TileCenter({(home.min.x + home.max.x) / 2, (home.min.z + home.max.z) / 2});
    for (const Vec2 direction : {Vec2{0.0f, 1.0f}, Vec2{-1.0f, 0.0f}, Vec2{1.0f, 0.0f}}) {
        const ViewingSpot* outermost = nullptr;
        float distance = 1.0e9f;
        for (const ViewingSpot& spot : home.viewing) {
            const float next = DistanceSquared(World::TileCenter(spot.tile), centre + direction * 100.0f);
            if (next < distance) {
                outermost = &spot;
                distance = next;
            }
        }
        if (outermost == nullptr) {
            continue;
        }
        const Prop* bench = nullptr;
        float nearest = 1.6f;
        for (const Prop& prop : zoo.Objects().Props()) {
            const float away = Distance(prop.position, World::TileCenter(outermost->tile));
            if (KindOf(zoo, prop).role == PropRole::Bench && away < nearest) {
                bench = &prop;
                nearest = away;
            }
        }
        benches.push_back(bench);
    }
    return benches;
}

/// Metres from a point inside a habitat to its barrier.
float ToBarrier(const Zoo& zoo, const Enclosure& home, Vec2 point) {
    float nearest = 1.0e9f;
    for (const TileCoord tile : TilesInRect(home.min, home.max)) {
        if (zoo.Habitats().At(tile) != home.id) {
            continue;
        }
        const float x = static_cast<float>(tile.x) * kTileSize;
        const float z = static_cast<float>(tile.z) * kTileSize;
        if (zoo.Habitats().At(TileCoord{tile.x, tile.z - 1}) != home.id) {
            nearest = std::min(nearest, ToSegment(point, {x, z}, {x + kTileSize, z}));
        }
        if (zoo.Habitats().At(TileCoord{tile.x, tile.z + 1}) != home.id) {
            nearest = std::min(nearest, ToSegment(point, {x, z + kTileSize}, {x + kTileSize, z + kTileSize}));
        }
        if (zoo.Habitats().At(TileCoord{tile.x - 1, tile.z}) != home.id) {
            nearest = std::min(nearest, ToSegment(point, {x, z}, {x, z + kTileSize}));
        }
        if (zoo.Habitats().At(TileCoord{tile.x + 1, tile.z}) != home.id) {
            nearest = std::min(nearest, ToSegment(point, {x + kTileSize, z}, {x + kTileSize, z + kTileSize}));
        }
    }
    return nearest;
}

void ExpectFamilies(const Zoo& zoo, float aged) {
    std::size_t expected = 0;
    for (const auto& [species, family] : kFamilies) {
        SCOPED_TRACE(species);
        const std::vector<const Animal*> group = Group(zoo, species);
        expected += family.size();
        ASSERT_EQ(group.size(), family.size());
        const SpeciesDef& def = SpeciesOf(zoo, *group.front());
        for (std::size_t i = 0; i < family.size(); ++i) {
            const Animal& animal = *group[i];
            const Named& named = family[i];
            EXPECT_EQ(animal.sex, named.sex) << i;
            EXPECT_NEAR(AgeYears(animal, zoo.Time()), named.age + aged, 0.01f) << i;
            EXPECT_EQ(animal.home, group.front()->home) << i;
            EXPECT_GT(animal.health, 0.9f) << animal.name;
            if (!named.young) {
                EXPECT_TRUE(IsAdult(animal, def, zoo.Time())) << i;
                EXPECT_FALSE(animal.mother) << i;
                continue;
            }
            const Animal* mother = zoo.Fauna().Find(animal.mother);
            const Animal* father = zoo.Fauna().Find(animal.father);
            ASSERT_NE(mother, nullptr) << i;
            EXPECT_EQ(mother->sex, Sex::Female);
            EXPECT_EQ(mother->species, animal.species);
            EXPECT_EQ(mother->home, animal.home);
            EXPECT_TRUE(IsAdult(*mother, def, zoo.Time()));
            if (species == "elephant") {
                EXPECT_EQ(father, nullptr);  // cow group; the sire is not resident here
            } else {
                ASSERT_NE(father, nullptr) << i;
                EXPECT_EQ(father, group.front());
                EXPECT_EQ(father->sex, Sex::Male);
            }
        }
    }
    EXPECT_EQ(zoo.Fauna().All().size(), expected);
}

class VisitorZooPlan : public testing::TestWithParam<std::uint64_t> {
protected:
    VisitorZooPlan() : zoo_(testdata::GameCatalog(), testdata::GameSpecies(), VisitorZooConfig(GetParam())) {
        for (const std::string& problem : BuildVisitorZoo(zoo_).problems) {
            ADD_FAILURE() << problem;
        }
    }
    Zoo zoo_;
};

TEST_P(VisitorZooPlan, FamiliesWithTheirYoungOnTheOpeningDay) {
    ExpectFamilies(zoo_, 0.0f);
    // The young of the year are small enough to be drawn as young and to need their mothers.
    int sucklings = 0;
    for (const auto& [species, family] : kFamilies) {
        const std::vector<const Animal*> group = Group(zoo_, species);
        const SpeciesDef& def = SpeciesOf(zoo_, *group.front());
        int youngest = 0;
        for (const Animal* animal : group) {
            youngest += AgeYears(*animal, zoo_.Time()) < def.maturityYears * 0.25f ? 1 : 0;
        }
        sucklings += youngest;
        const bool pair = species == "tiger" || species == "brown_bear" || species == "polar_bear";
        // A joey under half a year lives in the pouch, which the simulation has not: the zoo's
        // joeys are young at foot, past their first quarter of maturity.
        const bool pouch = species == "kangaroo";
        EXPECT_EQ(youngest > 0, !pair && !pouch) << species;
        int grownMales = 0;
        for (const Animal* animal : group) {
            grownMales += animal->sex == Sex::Male && IsAdult(*animal, def, zoo_.Time()) ? 1 : 0;
        }
        EXPECT_TRUE(def.maxAdultMales == 0 || grownMales <= def.maxAdultMales) << species;
        EXPECT_GE(static_cast<int>(group.size()), def.groupMin) << species;
        EXPECT_LE(static_cast<int>(group.size()), def.groupMax) << species;
    }
    EXPECT_EQ(sucklings, 21);
}

TEST_P(VisitorZooPlan, FamiliesAreWholeWhenTheVisitBegins) {
    const GameTime before = zoo_.Time();
    FillForVisit(zoo_);
    EXPECT_EQ(zoo_.Fauna().Deaths(), 0);
    ExpectFamilies(zoo_, static_cast<float>(zoo_.Time().tick - before.tick) /
                             (kGameDaysPerRealYear * static_cast<float>(kTicksPerDay)));
}

TEST_P(VisitorZooPlan, NothingStandsInWaterOrInAnotherThing) {
    const auto& props = zoo_.Objects().Props();
    for (const Prop& prop : props) {
        const PropDef& kind = KindOf(zoo_, prop);
        const EnclosureId home = zoo_.Habitats().At(World::TileAt(prop.position));
        const float radius = kind.radiusM * prop.scale;
        const TileCoord low = World::TileAt({prop.position.x - radius, prop.position.z - radius});
        const TileCoord high = World::TileAt({prop.position.x + radius, prop.position.z + radius});
        for (const TileCoord tile : TilesInRect(low, high)) {
            if (!Touches(prop.position, radius, tile)) {
                continue;
            }
            EXPECT_FALSE(zoo_.Map().HasWater(tile))
                << kind.id << " in water at " << prop.position.x << ',' << prop.position.z;
            if (home) {
                EXPECT_FALSE(zoo_.Map().At(tile).building) << kind.id << " in a building";
                EXPECT_EQ(zoo_.Habitats().At(tile), home) << kind.id << " through the barrier";
            }
        }
        if (!home) {
            continue;
        }
        for (const Prop& other : props) {
            if (other.id.value <= prop.id.value ||
                zoo_.Habitats().At(World::TileAt(other.position)) != home) {
                continue;
            }
            const PropDef& otherKind = KindOf(zoo_, other);
            EXPECT_GT(Distance(prop.position, other.position), radius + otherKind.radiusM * other.scale)
                << kind.id << " in " << otherKind.id << " at " << prop.position.x << ',' << prop.position.z;
        }
    }
}

// A water tile's bed lowers the four corners which it shares with the eight surrounding tiles.
// The game draws water over that sloping ground, and the catalog has no water plant: no tree or
// bush may reach a tile whose ground the pool has dug into.
TEST_P(VisitorZooPlan, NoLandPlantInOrBesideWater) {
    const World& map = zoo_.Map();
    const auto dug = [&map](TileCoord tile) {
        for (int dz = -1; dz <= 1; ++dz) {
            for (int dx = -1; dx <= 1; ++dx) {
                if (map.HasWater({tile.x + dx, tile.z + dz})) {
                    return true;
                }
            }
        }
        return false;
    };
    int beside = 0;
    for (const Prop& prop : zoo_.Objects().Props()) {
        const PropDef& kind = KindOf(zoo_, prop);
        if (!IsPlant(kind)) {
            continue;
        }
        const float radius = kind.radiusM * prop.scale;
        const TileCoord low = World::TileAt({prop.position.x - radius, prop.position.z - radius});
        const TileCoord high = World::TileAt({prop.position.x + radius, prop.position.z + radius});
        for (const TileCoord tile : TilesInRect(low, high)) {
            EXPECT_FALSE(Touches(prop.position, radius, tile) && dug(tile))
                << kind.id << " at " << prop.position.x << ',' << prop.position.z << " reaches " << tile.x
                << ',' << tile.z;
        }
        const TileCoord at = World::TileAt(prop.position);
        const bool bank =
            dug({at.x - 1, at.z}) || dug({at.x + 1, at.z}) || dug({at.x, at.z - 1}) || dug({at.x, at.z + 1});
        beside += bank ? 1 : 0;
    }
    EXPECT_GT(beside, 0) << "the plan should retain planting on dry banks near water";
}

TEST_P(VisitorZooPlan, NoPlantOnAPathOrBeforeABench) {
    for (const Prop& prop : zoo_.Objects().Props()) {
        const PropDef& kind = KindOf(zoo_, prop);
        if (IsPlant(kind) || kind.role == PropRole::Rock) {
            EXPECT_EQ(zoo_.Map().At(World::TileAt(prop.position)).path, PathKind::None)
                << kind.id << " on a path at " << prop.position.x << ',' << prop.position.z;
        }
    }
    for (const Enclosure& home : zoo_.Habitats().All()) {
        SCOPED_TRACE(home.name);
        const Vec2 middle = Middle(zoo_, home);
        const std::vector<const Prop*> benches = ViewingBenches(zoo_, home);
        ASSERT_EQ(benches.size(), 3U);
        // Three metres to either side in a large habitat, two on the meerkats' mound; the bench
        // stands half a metre off the middle of its tile.
        const float open = home.stats.areaM2 < 300.0f ? 1.4f : 2.4f;
        // No trunk right before any place at the barrier, benched or not, nor within the planted
        // strip's depth behind it. Rhino Hollow and Red Dune would become too bare for their
        // residents if every slender tree kept that extra distance; massive trunks still do.
        const bool needsFrontCover = home.name == "Rhino Hollow" || home.name == "Red Dune";
        for (const ViewingSpot& spot : home.viewing) {
            for (const Prop& prop : zoo_.Objects().Props()) {
                const PropDef& kind = KindOf(zoo_, prop);
                if (kind.role == PropRole::Tree &&
                    zoo_.Habitats().At(World::TileAt(prop.position)) == home.id) {
                    const bool setBack = kind.radiusM >= 1.0f || !needsFrontCover;
                    EXPECT_GE(Distance(prop.position, World::TileCenter(spot.tile)),
                              5.0f + (setBack ? spot.standOffM : 0.0f))
                        << KindOf(zoo_, prop).id << " at " << prop.position.x << ',' << prop.position.z;
                }
            }
        }
        for (const Prop* bench : benches) {
            ASSERT_NE(bench, nullptr) << "a viewing place without its bench";
            for (const Prop& prop : zoo_.Objects().Props()) {
                if (IsPlant(KindOf(zoo_, prop)) &&
                    zoo_.Habitats().At(World::TileAt(prop.position)) == home.id) {
                    EXPECT_GE(ToSegment(prop.position, bench->position, middle), open)
                        << KindOf(zoo_, prop).id << " at " << prop.position.x << ',' << prop.position.z;
                }
            }
        }
    }
}

TEST_P(VisitorZooPlan, BenchesStandInShade) {
    int benches = 0;
    std::vector<std::string> sunny;
    for (const Enclosure& home : zoo_.Habitats().All()) {
        for (const Prop* bench : ViewingBenches(zoo_, home)) {
            ASSERT_NE(bench, nullptr) << home.name;
            float nearest = 1.0e9f;
            for (const Prop& prop : zoo_.Objects().Props()) {
                if (KindOf(zoo_, prop).role == PropRole::Tree &&
                    !zoo_.Habitats().At(World::TileAt(prop.position))) {
                    nearest = std::min(nearest, Distance(prop.position, bench->position));
                }
            }
            ++benches;
            if (nearest > 8.0f) {
                sunny.push_back(home.name);
            }
        }
    }
    EXPECT_EQ(benches, 42);
    // Three walks run between the planted strips of two habitats and have no lawn for a tree; a
    // strip's own planting stays below the eye (VisitorZooHabitatTests). Their benches are the
    // exception, by name, so that a fourth is noticed.
    EXPECT_EQ(sunny, (std::vector<std::string>{"Ostrich Rise", "Elephant Valley", "Kangaroo Grove"}));
}

TEST_P(VisitorZooPlan, HoofstockStandsBehindCablesAndRails) {
    for (const Enclosure& home : zoo_.Habitats().All()) {
        SCOPED_TRACE(home.name);
        for (const Animal& animal : zoo_.Fauna().All()) {
            if (animal.home == home.id) {
                EXPECT_GE(Containment(SpeciesOf(zoo_, animal), home.stats), 0.99999f)
                    << SpeciesOf(zoo_, animal).id;
            }
        }
        if (!kBarriers.contains(home.name)) {
            continue;
        }
        int edges = 0;
        for (const TileCoord tile : TilesInRect(home.min, home.max)) {
            if (zoo_.Habitats().At(tile) != home.id) {
                continue;
            }
            for (const Dir side : {Dir::North, Dir::East, Dir::South, Dir::West}) {
                const Fence fence = zoo_.Map().FenceOn(tile, side);
                if (!fence.Exists()) {
                    continue;
                }
                const FenceDef* def = zoo_.Definitions().FenceByKind(fence.kind);
                ASSERT_NE(def, nullptr);
                EXPECT_EQ(def->id, kBarriers.at(home.name));
                EXPECT_NE(def->style, FenceStyle::Mesh);
                ++edges;
            }
        }
        EXPECT_GT(edges, 40);
    }
}

TEST_P(VisitorZooPlan, FoodAndWaterStandInsideAndInView) {
    for (const Enclosure& home : zoo_.Habitats().All()) {
        SCOPED_TRACE(home.name);
        const Vec2 middle = Middle(zoo_, home);
        // Where the water takes half the ground, and on the meerkats' mound, a tile from the
        // barrier; elsewhere two.
        const bool tight = home.stats.areaM2 < 300.0f || home.stats.waterShare > 0.3f;
        const float depth = static_cast<float>(home.max.z - home.min.z + 1) * kTileSize;
        int seen = 0;
        int all = 0;
        std::string behind;
        for (const Prop& prop : zoo_.Objects().Props()) {
            const PropDef& kind = KindOf(zoo_, prop);
            if ((kind.role != PropRole::Feeder && kind.role != PropRole::WaterTrough) ||
                zoo_.Habitats().At(World::TileAt(prop.position)) != home.id) {
                continue;
            }
            EXPECT_GE(ToBarrier(zoo_, home, prop.position), tight ? 1.5f : 3.5f)
                << kind.id << " at " << prop.position.x << ',' << prop.position.z;
            ++all;
            // In the half of the habitat before the main front, give or take a tenth of its depth.
            if (prop.position.z > middle.z - 0.1f * depth) {
                ++seen;
            } else {
                behind += ' ' + kind.id + " at " + std::to_string(prop.position.x) + ',' +
                          std::to_string(prop.position.z);
            }
        }
        EXPECT_GE(all, 3);
        EXPECT_GE(seen * 4, all * 3) << seen << " of " << all << "; behind the middle:" << behind
                                     << " (middle " << middle.x << ',' << middle.z << ")";
    }
}

TEST_P(VisitorZooPlan, MeerkatsHaveADishAndTheLagoonIsShallow) {
    for (const Enclosure& home : zoo_.Habitats().All()) {
        if (home.name != "Meerkat Lookout" && home.name != "Flamingo Lagoon") {
            continue;
        }
        int water = 0;
        int shallow = 0;
        for (const TileCoord tile : TilesInRect(home.min, home.max)) {
            if (zoo_.Habitats().At(tile) == home.id && zoo_.Map().HasWater(tile)) {
                ++water;
                shallow += zoo_.Map().WaterDepthAt(World::TileCenter(tile)) <= 0.36f ? 1 : 0;
            }
        }
        if (home.name == "Meerkat Lookout") {
            EXPECT_EQ(water, 2);
            EXPECT_EQ(shallow, 2);
            EXPECT_LE(home.stats.areaM2, 300.0f);
        } else {
            EXPECT_GE(shallow * 3, water * 2) << shallow << " of " << water;
            EXPECT_TRUE(home.stats.deepWater);
        }
    }
}

TEST_P(VisitorZooPlan, TreesStandApartInGroves) {
    std::vector<const Prop*> trees;
    for (const Prop& prop : zoo_.Objects().Props()) {
        if (KindOf(zoo_, prop).role == PropRole::Tree) {
            trees.push_back(&prop);
        }
    }
    int crowded = 0;
    for (const Prop* tree : trees) {
        int near = 0;
        for (const Prop* other : trees) {
            if (other == tree) {
                continue;
            }
            const float distance = Distance(tree->position, other->position);
            EXPECT_GE(distance, 1.99f)
                << KindOf(zoo_, *tree).id << " at " << tree->position.x << ',' << tree->position.z;
            near += distance < 6.0f ? 1 : 0;
        }
        crowded = std::max(crowded, near);
    }
    // The old block of pines had fourteen and more trunks within six metres of one another.
    EXPECT_LE(crowded, 9);
    std::cout << "TREES seed=" << GetParam() << " count=" << trees.size()
              << " mostWithinSixMetres=" << crowded << '\n';
}

TEST_P(VisitorZooPlan, EveryKeeperGateHasADryConnectedApproach) {
    const auto profile = NavProfile::Staff();
    for (const Enclosure& home : zoo_.Habitats().All()) {
        SCOPED_TRACE(home.name);
        ASSERT_FALSE(home.gates.empty());
        for (const EdgeRef gate : home.gates) {
            const TileCoord a{gate.x, gate.z};
            const TileCoord b =
                gate.horizontal ? TileCoord{gate.x, gate.z - 1} : TileCoord{gate.x - 1, gate.z};
            const TileCoord outside = zoo_.Habitats().At(a) == home.id ? b : a;
            EXPECT_NE(zoo_.Habitats().At(outside), home.id);
            EXPECT_FALSE(zoo_.Map().At(outside).standOff);
            EXPECT_FALSE(zoo_.Map().HasWater(outside));
            EXPECT_TRUE(zoo_.Routes().Passable(outside, profile));
            EXPECT_TRUE(zoo_.Routes().Reachable(zoo_.Map().EntranceTile(), outside, profile));
            for (const Prop& prop : zoo_.Objects().Props()) {
                const PropDef& kind = KindOf(zoo_, prop);
                if (kind.blocksWalking) {
                    EXPECT_GE(Distance(prop.position, World::TileCenter(outside)),
                              kind.radiusM * std::max(1.0f, prop.scale) + 0.4f)
                        << kind.id;
                }
            }
        }
    }
}

TEST_P(VisitorZooPlan, BlockingPropsLeaveAnAdultWidthPlusOneMetre) {
    for (const Enclosure& home : zoo_.Habitats().All()) {
        SCOPED_TRACE(home.name);
        float gap = 1.0f;
        for (const Animal& animal : zoo_.Fauna().All()) {
            if (animal.home == home.id) {
                gap = std::max(gap, 1.0f + 1.2f * SpeciesOf(zoo_, animal).bodyRadiusM);
            }
        }
        std::vector<const Prop*> blockers;
        for (const Prop& prop : zoo_.Objects().Props()) {
            if (zoo_.Habitats().At(World::TileAt(prop.position)) == home.id &&
                KindOf(zoo_, prop).blocksWalking) {
                blockers.push_back(&prop);
            }
        }
        for (std::size_t i = 0; i < blockers.size(); ++i) {
            const Prop& a = *blockers[i];
            for (std::size_t j = i + 1; j < blockers.size(); ++j) {
                const Prop& b = *blockers[j];
                const float edges = Distance(a.position, b.position) -
                                    KindOf(zoo_, a).radiusM * std::max(1.0f, a.scale) -
                                    KindOf(zoo_, b).radiusM * std::max(1.0f, b.scale);
                EXPECT_GE(edges + 0.0001f, gap) << KindOf(zoo_, a).id << " / " << KindOf(zoo_, b).id;
            }
        }
    }
}

TEST_P(VisitorZooPlan, FlamingosUseTheShallowShelfDuringAVisit) {
    FillForVisit(zoo_);
    int waders = 0;
    for (int tick = 0; tick < 3000; ++tick) {
        zoo_.Step();
        if (tick % 100 != 0) {
            continue;
        }
        int shallow = 0;
        for (const Animal* bird : Group(zoo_, "flamingo")) {
            const float depth = zoo_.Map().WaterDepthAt(bird->position);
            shallow += depth > 0.05f && depth <= 0.36f ? 1 : 0;
        }
        waders = std::max(waders, shallow);
    }
    EXPECT_GE(waders, 6) << "at least half the flock should use its shelf";
}

TEST(VisitorZooPlanTests, DISABLED_MovementAndFamilyDistances) {
    Zoo zoo(testdata::GameCatalog(), testdata::GameSpecies(), VisitorZooConfig());
    ASSERT_TRUE(BuildVisitorZoo(zoo).problems.empty());
    FillForVisit(zoo);
    struct Counts {
        int moving = 0;
        int stalled = 0;
        float motherDistance = 0.0f;
        int young = 0;
    };
    std::map<std::string, Counts> counts;
    for (int tick = 0; tick < 6000; ++tick) {
        zoo.Step();
        for (const Animal& animal : zoo.Fauna().All()) {
            const auto& species = SpeciesOf(zoo, animal);
            auto& sample = counts[species.id];
            if (animal.moving) {
                ++sample.moving;
                sample.stalled += animal.speed < 0.05f ? 1 : 0;
            }
            if (AgeYears(animal, zoo.Time()) < species.maturityYears * 0.25f) {
                if (const Animal* mother = zoo.Fauna().Find(animal.mother)) {
                    sample.motherDistance += Distance(animal.position, mother->position);
                    ++sample.young;
                }
            }
        }
    }
    for (const auto& [species, sample] : counts) {
        std::cout << "MOVEMENT " << species << " moving=" << sample.moving << " stalled=" << sample.stalled
                  << " meanMotherDistance="
                  << sample.motherDistance / static_cast<float>(std::max(1, sample.young)) << '\n';
    }
}

INSTANTIATE_TEST_SUITE_P(Seeds, VisitorZooPlan, testing::Values(1ULL, 2ULL));

// Slopes include every paved tile. A tile whose axial runs are both one tile lies on a walk 2 m
// wide, under the 2.5 to 3 m of a zoo's side path.
TEST_P(VisitorZooPlan, WalksAreWideAndLevel) {
    const World& map = zoo_.Map();
    const auto paved = [&](int x, int z) {
        return map.InBounds({x, z}) && map.At({x, z}).path != PathKind::None;
    };
    const auto run = [&](TileCoord tile, int dx, int dz) {
        int length = 1;
        for (int step = 1; paved(tile.x + dx * step, tile.z + dz * step); ++step) {
            ++length;
        }
        for (int step = 1; paved(tile.x - dx * step, tile.z - dz * step); ++step) {
            ++length;
        }
        return length;
    };
    std::map<int, int> widths;
    std::vector<TileCoord> narrowTiles;
    int narrow = 0;
    float steepest = 0.0f;
    for (int z = 0; z < map.Depth(); ++z) {
        for (int x = 0; x < map.Width(); ++x) {
            if (!paved(x, z)) {
                continue;
            }
            const TileCoord tile{x, z};
            const int width = std::min(run(tile, 1, 0), run(tile, 0, 1));
            ++widths[std::min(width, 6)];
            if (width < 2 && map.At(tile).path != PathKind::Boardwalk) {
                ++narrow;
                narrowTiles.push_back(tile);
            }
            steepest = std::max(steepest, map.TileRelief(tile) / kTileSize);
            EXPECT_LE(map.TileRelief(tile) / kTileSize, 0.05f) << x << ',' << z;
        }
    }
    std::cout << "WALKS seed=" << GetParam() << " steepest=" << steepest << " narrow=" << narrow
              << " tiles by width";
    for (const auto& [width, count] : widths) {
        std::cout << ' ' << width * 2 << "m:" << count;
    }
    std::cout << " narrow at";
    for (const TileCoord tile : narrowTiles) {
        std::cout << ' ' << tile.x << ',' << tile.z;
    }
    std::cout << std::endl;
    EXPECT_LE(steepest, 0.05f);
    // One-tile ways to kiosk doors, paving around bench islands and dead-end raster teeth are all
    // under a zoo side path's useful width. A few constrained viewing or between-habitat tips may
    // remain where widening would open a new habitat front or require moving a whole enclosure.
    EXPECT_LE(narrow, 3);
}

// A week on the calendar with the gates shut: nobody dies, the young are still young and still fed.
TEST(VisitorZooPlanTests, TheYoungAreStillYoungAndFedAfterAWeek) {
    Zoo zoo(testdata::GameCatalog(), testdata::GameSpecies(), VisitorZooConfig());
    BuildVisitorZoo(zoo);
    VisitorPace nobody;
    nobody.arrivals = 0.0f;
    zoo.Guests().SetPace(nobody);
    for (int tick = 0; tick < 7 * kTicksPerDay; ++tick) {
        zoo.Step();
    }
    EXPECT_EQ(zoo.Fauna().Deaths(), 0);
    const float aged = 7.0f / kGameDaysPerRealYear;
    for (const auto& [species, family] : kFamilies) {
        SCOPED_TRACE(species);
        const std::vector<const Animal*> group = Group(zoo, species);
        ASSERT_GE(group.size(), family.size());
        const SpeciesDef& def = SpeciesOf(zoo, *group.front());
        for (std::size_t i = 0; i < family.size(); ++i) {
            const Animal& animal = *group[i];
            EXPECT_EQ(animal.sex, family[i].sex) << i;
            EXPECT_NEAR(AgeYears(animal, zoo.Time()), family[i].age + aged, 0.01f) << i;
            EXPECT_GT(animal.health, 0.8f) << animal.name;
            EXPECT_LT(animal.hunger, 0.9f) << animal.name;
            if (!family[i].young) {
                continue;
            }
            const Animal* mother = zoo.Fauna().Find(animal.mother);
            ASSERT_NE(mother, nullptr) << i;
            EXPECT_EQ(mother->home, animal.home);
            // Who was a suckling on the opening day still is one; who was drawn young still is.
            if (family[i].age < def.maturityYears * 0.25f) {
                EXPECT_LT(AgeYears(animal, zoo.Time()), def.maturityYears * 0.25f) << i;
            }
            if (family[i].age < def.maturityYears * 0.5f) {
                EXPECT_LT(AgeYears(animal, zoo.Time()), def.maturityYears * 0.5f) << i;
            }
        }
    }
}

}  // namespace
}  // namespace zoo
