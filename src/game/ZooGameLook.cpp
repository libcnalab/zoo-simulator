// SPDX-License-Identifier: MIT
// What a visitor meets on the walk: the animal the eyes rest on, the boards at the
// enclosures (E), the map of the zoo (M).
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <map>

#include "Hud.hpp"
#include "ZooGame.hpp"

namespace zoogame {

namespace {

/// The eyes pick out an animal this far away at most.
constexpr float kLookReach = 70.0f;
/// A board is read from this near, and left behind from this far.
constexpr float kBoardReach = 2.8f;
constexpr float kBoardLeave = 5.0f;

std::string Capital(std::string text) {
    if (!text.empty() && text[0] >= 'a' && text[0] <= 'z')
        text[0] = static_cast<char>(text[0] - 'a' + 'A');
    return text;
}

std::string AgeText(float years) {
    if (years < 1.0f) {
        const int months = std::max(1, static_cast<int>(std::lround(years * 12.0f)));
        return Format(months == 1 ? "%d month old" : "%d months old", months);
    }
    const int whole = static_cast<int>(years);
    return Format(whole == 1 ? "%d year old" : "%d years old", whole);
}

std::string DietText(zoo::Diet diet) {
    switch (diet) {
        case zoo::Diet::Carnivore:
            return "Meat eater";
        case zoo::Diet::Herbivore:
            return "Plant eater";
        case zoo::Diet::Omnivore:
            return "Eats plants and animals";
        case zoo::Diet::Piscivore:
            return "Fish eater";
        case zoo::Diet::Insectivore:
            return "Insect eater";
    }
    return {};
}

std::string MassText(float kg) {
    if (kg >= 1000.0f)
        return Format("%.1f t", static_cast<double>(kg / 1000.0f));
    if (kg >= 10.0f)
        return Format("%.0f kg", static_cast<double>(kg));
    return Format("%.1f kg", static_cast<double>(kg));
}

/// Metres from a point to the rectangle of tiles min..max.
float DistanceToTiles(zoo::Vec2 p, zoo::TileCoord min, zoo::TileCoord max) {
    const float x0 = static_cast<float>(min.x) * zoo::kTileSize;
    const float z0 = static_cast<float>(min.z) * zoo::kTileSize;
    const float x1 = static_cast<float>(max.x + 1) * zoo::kTileSize;
    const float z1 = static_cast<float>(max.z + 1) * zoo::kTileSize;
    const float dx = std::max({x0 - p.x, 0.0f, p.x - x1});
    const float dz = std::max({z0 - p.z, 0.0f, p.z - z1});
    return std::sqrt(dx * dx + dz * dz);
}

/// "Zuri", "Zuri and Tau", "Zuri, Tau and Nala", "Zuri, Tau, Nala and 4 more".
std::string Names(const std::vector<std::string>& names) {
    constexpr std::size_t kMost = 5;
    std::string out;
    const std::size_t shown = std::min(names.size(), kMost);
    for (std::size_t i = 0; i < shown; ++i) {
        if (i > 0)
            out += i + 1 == shown && names.size() <= kMost ? " and " : ", ";
        out += names[i];
    }
    if (names.size() > kMost)
        out += Format(" and %zu more", names.size() - kMost);
    return out;
}

}  // namespace

void ZooGame::UpdateEncounters(float seconds) {
    LookAtAnimals(seconds);

    // The board in reach: the nearest one the eyes are turned towards.
    boardInReach_ = {};
    const zoo::Vec2 here = walker_.Position();
    const Vector3 gaze = walker_.Gaze();
    std::vector<zoo::PropId> near;
    zoo_->Objects().PropsNear(here, kBoardReach, near);
    float best = kBoardReach;
    for (const zoo::PropId id : near) {
        const zoo::Prop* prop = zoo_->Objects().FindProp(id);
        if (prop == nullptr || prop->def < 0 ||
            zoo_->Definitions().props[static_cast<std::size_t>(prop->def)].role != zoo::PropRole::InfoBoard)
            continue;
        const float dx = prop->position.x - here.x;
        const float dz = prop->position.z - here.z;
        const float distance = std::sqrt(dx * dx + dz * dz);
        if (distance > best || (distance > 0.6f && (dx * gaze.X + dz * gaze.Z) / distance < 0.35f))
            continue;
        best = distance;
        boardInReach_ = id;
    }

    const bool keys = controls_.KeyboardFree();
    if (visit_.window == VisitWindow::Board) {
        const float dx = boardRead_.x - here.x;
        const float dz = boardRead_.z - here.z;
        if ((keys && controls_.Pressed(Keys::E)) || dx * dx + dz * dz > kBoardLeave * kBoardLeave)
            visit_.window = VisitWindow::None;
    } else if (keys && controls_.Pressed(Keys::E) && boardInReach_) {
        if (const zoo::Prop* prop = zoo_->Objects().FindProp(boardInReach_))
            OpenBoard(*prop);
    }
    if (keys && controls_.Pressed(Keys::M)) {
        if (visit_.window == VisitWindow::Map) {
            visit_.window = VisitWindow::None;
        } else {
            BuildVisitMap();
            visit_.window = VisitWindow::Map;
        }
    }
    visit_.map.you = Vector2(here.x, here.z);
    visit_.map.yaw = walker_.Yaw();
    visit_.prompt = boardInReach_ && visit_.window == VisitWindow::None ? "Read the board" : "";
}

void ZooGame::LookAtAnimals(float seconds) {
    const Vector3 eye = walker_.Eye();
    const Vector3 gaze = walker_.Gaze();
    const auto& species = zoo_->SpeciesList().All();
    const zoo::Animal* found = nullptr;
    float bestMiss = 1.0f;
    for (const zoo::Animal& animal : zoo_->Fauna().All()) {
        if (!animal.Visible() || animal.species < 0)
            continue;
        const zoo::SpeciesDef& def = species[static_cast<std::size_t>(animal.species)];
        const Vector3 body(animal.position.x, zoo_->Map().HeightAt(animal.position) + 0.55f * def.shoulderHeightM,
                           animal.position.z);
        const Vector3 to = body - eye;
        const float along = to.X * gaze.X + to.Y * gaze.Y + to.Z * gaze.Z;
        if (along < 0.8f || along > kLookReach)
            continue;
        const float aside = std::sqrt(std::max(0.0f, to.X * to.X + to.Y * to.Y + to.Z * to.Z - along * along));
        // A far animal is a small target: the eyes are allowed a little more there.
        const float size = std::max(0.45f, 0.5f * def.bodyLengthM) + 0.2f + 0.012f * along;
        const float miss = aside / size;
        if (miss < bestMiss) {
            bestMiss = miss;
            found = &animal;
        }
    }

    AnimalCard& card = visit_.card;
    if (found != nullptr) {
        const zoo::SpeciesDef& def = species[static_cast<std::size_t>(found->species)];
        if (!(lookedAt_ == found->id) && card.shown > 0.0f) {
            // Another animal: the card of the one before goes first.
            card.shown = std::max(0.0f, card.shown - 6.0f * seconds);
            return;
        }
        lookedAt_ = found->id;
        card.name = found->name;
        card.species = def.commonName;
        card.latin = def.scientificName;
        card.about = Capital(std::string(zoo::ToString(found->sex))) + ", " +
                     AgeText(zoo::AgeYears(*found, zoo_->Time()));
        card.doing = ActivityText(*found);
        card.shown = std::min(1.0f, card.shown + 3.0f * seconds);
    } else {
        card.shown = std::max(0.0f, card.shown - 2.5f * seconds);
        if (card.shown <= 0.0f)
            lookedAt_ = {};
    }
}

void ZooGame::OpenBoard(const zoo::Prop& board) {
    // The board speaks of the enclosure it stands at: the nearest one that is lived in.
    const zoo::Enclosure* home = nullptr;
    float nearest = 30.0f;
    for (const zoo::Enclosure& enclosure : zoo_->Habitats().All()) {
        const float distance = DistanceToTiles(board.position, enclosure.min, enclosure.max);
        if (distance >= nearest)
            continue;
        const bool livedIn = std::ranges::any_of(zoo_->Fauna().All(), [&](const zoo::Animal& a) {
            return a.InWorld() && a.home == enclosure.id;
        });
        if (livedIn) {
            nearest = distance;
            home = &enclosure;
        }
    }
    if (home == nullptr) {
        visit_.Flash("This board is still blank", 2.5f);
        return;
    }
    // Who lives there, by species; the board is about the most numerous one.
    std::map<int, std::vector<std::string>> living;
    for (const zoo::Animal& animal : zoo_->Fauna().All())
        if (animal.InWorld() && animal.home == home->id && animal.species >= 0)
            living[animal.species].push_back(animal.name);
    int chosen = living.begin()->first;
    for (const auto& [index, names] : living)
        if (names.size() > living[chosen].size())
            chosen = index;
    const auto& species = zoo_->SpeciesList().All();
    const zoo::SpeciesDef& def = species[static_cast<std::size_t>(chosen)];

    BoardText text;
    text.title = def.commonName;
    text.latin = def.scientificName;
    if (!def.nativeRange.empty())
        text.rows.emplace_back("Native range", def.nativeRange);
    if (!def.biome.empty())
        text.rows.emplace_back("Habitat", def.biome);
    text.rows.emplace_back("Diet", DietText(def.diet));
    text.rows.emplace_back("Adult weight", "about " + MassText(def.adultMassKg));
    text.rows.emplace_back("Height", Format("about %.1f m", static_cast<double>(def.shoulderHeightM)));
    text.rows.emplace_back("Lifespan", Format("about %.0f years in human care", static_cast<double>(def.lifespanYears)));
    text.rows.emplace_back("Conservation", std::string(zoo::DisplayName(def.status)));
    text.facts = def.facts;
    text.residents = "Living here: " + Names(living[chosen]);
    for (const auto& [index, names] : living) {
        if (index == chosen)
            continue;
        text.others += text.others.empty() ? "Also here: " : ", ";
        text.others += species[static_cast<std::size_t>(index)].commonName;
    }
    visit_.board = std::move(text);
    visit_.window = VisitWindow::Board;
    boardRead_ = board.position;
}

std::optional<ZooGame::Pose> ZooGame::StandPose(std::string_view what) const {
    const std::size_t colon = what.find(':');
    const std::string_view kind = what.substr(0, colon);
    int wanted = 0;
    if (colon != std::string_view::npos)
        wanted = std::atoi(std::string(what.substr(colon + 1)).c_str());
    const auto centre = [](zoo::TileCoord t) {
        return zoo::Vec2{(static_cast<float>(t.x) + 0.5f) * zoo::kTileSize,
                         (static_cast<float>(t.z) + 0.5f) * zoo::kTileSize};
    };
    if (kind == "habitat") {
        for (const zoo::Enclosure& enclosure : zoo_->Habitats().All()) {
            // The spot at the fence and the animal that are nearest to each other.
            float best = 1.0e9f;
            Pose pose;
            for (const zoo::Animal& animal : zoo_->Fauna().All()) {
                if (!animal.Visible() || !(animal.home == enclosure.id))
                    continue;
                for (const zoo::ViewingSpot& spot : enclosure.viewing) {
                    const zoo::Vec2 at = centre(spot.tile);
                    const float dx = animal.position.x - at.x;
                    const float dz = animal.position.z - at.z;
                    if (dx * dx + dz * dz < best) {
                        best = dx * dx + dz * dz;
                        pose = Pose{at.x, at.z, std::atan2(dx, dz), 0.06f};
                    }
                }
            }
            if (best < 1.0e9f && wanted-- == 0)
                return pose;
        }
    } else if (kind == "board") {
        for (const zoo::Prop& prop : zoo_->Objects().Props()) {
            if (prop.def < 0 ||
                zoo_->Definitions().props[static_cast<std::size_t>(prop.def)].role != zoo::PropRole::InfoBoard ||
                wanted-- != 0)
                continue;
            // From the path side when there is one.
            zoo::Vec2 at{prop.position.x, prop.position.z + 1.7f};
            for (int step = 0; step < 8; ++step) {
                const float angle = 0.785398f * static_cast<float>(step);
                const zoo::Vec2 tried{prop.position.x + 1.7f * std::sin(angle), prop.position.z + 1.7f * std::cos(angle)};
                const zoo::TileCoord tile = zoo::World::TileAt(tried);
                if (zoo_->Map().InBounds(tile) && zoo_->Map().At(tile).path != zoo::PathKind::None) {
                    at = tried;
                    break;
                }
            }
            return Pose{at.x, at.z, std::atan2(prop.position.x - at.x, prop.position.z - at.z), 0.2f};
        }
    }
    return std::nullopt;
}

void ZooGame::BuildVisitMap() {
    using Kind = VisitMap::Kind;
    const zoo::World& world = zoo_->Map();
    VisitMap map;
    const zoo::TileCoord min = world.PropertyMin();
    const zoo::TileCoord max = world.PropertyMax();
    map.minX = static_cast<float>(min.x) * zoo::kTileSize;
    map.minZ = static_cast<float>(min.z) * zoo::kTileSize;
    map.maxX = static_cast<float>(max.x + 1) * zoo::kTileSize;
    map.maxZ = static_cast<float>(max.z + 1) * zoo::kTileSize;

    // Row by row, runs of tiles of one kind become one rectangle.
    const auto kindOf = [&](zoo::TileCoord t) {
        const zoo::Tile& tile = world.At(t);
        if (tile.building)
            return Kind::Building;
        if (tile.path != zoo::PathKind::None)
            return Kind::Path;
        if (world.HasWater(t))
            return Kind::Water;
        if (zoo_->Habitats().At(t))
            return Kind::Habitat;
        return Kind::Ground;
    };
    for (int z = min.z; z <= max.z; ++z) {
        int x = min.x;
        while (x <= max.x) {
            const Kind kind = kindOf({x, z});
            int end = x + 1;
            while (end <= max.x && kindOf({end, z}) == kind)
                ++end;
            if (kind != Kind::Ground)
                map.shapes.push_back({static_cast<float>(x) * zoo::kTileSize, static_cast<float>(z) * zoo::kTileSize,
                                      static_cast<float>(end - x) * zoo::kTileSize, zoo::kTileSize, kind});
            x = end;
        }
    }
    // The plan shows what has been built, with some green about it, not the whole estate.
    if (!map.shapes.empty()) {
        float x0 = map.maxX, z0 = map.maxZ, x1 = map.minX, z1 = map.minZ;
        for (const VisitMap::Shape& shape : map.shapes) {
            x0 = std::min(x0, shape.x);
            z0 = std::min(z0, shape.z);
            x1 = std::max(x1, shape.x + shape.w);
            z1 = std::max(z1, shape.z + shape.d);
        }
        constexpr float kMargin = 5.0f * zoo::kTileSize;
        map.minX = std::max(map.minX, x0 - kMargin);
        map.minZ = std::max(map.minZ, z0 - kMargin);
        map.maxX = std::min(map.maxX, x1 + kMargin);
        map.maxZ = std::min(map.maxZ, z1 + kMargin);
    }
    map.shapes.insert(map.shapes.begin(),
                      {map.minX, map.minZ, map.maxX - map.minX, map.maxZ - map.minZ, Kind::Ground});
    const zoo::TileCoord gate = world.EntranceTile();
    map.shapes.push_back({(static_cast<float>(gate.x) - 0.5f) * zoo::kTileSize, static_cast<float>(gate.z) * zoo::kTileSize,
                          2.0f * zoo::kTileSize, zoo::kTileSize, Kind::Entrance});

    // The names on the plan: who lives in each enclosure, what each house for visitors is.
    const auto& species = zoo_->SpeciesList().All();
    for (const zoo::Enclosure& enclosure : zoo_->Habitats().All()) {
        std::map<int, int> living;
        for (const zoo::Animal& animal : zoo_->Fauna().All())
            if (animal.InWorld() && animal.home == enclosure.id && animal.species >= 0)
                ++living[animal.species];
        if (living.empty())
            continue;
        std::string text;
        for (const auto& [index, count] : living) {
            if (!text.empty())
                text += ", ";
            text += species[static_cast<std::size_t>(index)].commonName;
        }
        map.labels.push_back({0.5f * static_cast<float>(enclosure.min.x + enclosure.max.x + 1) * zoo::kTileSize,
                              0.5f * static_cast<float>(enclosure.min.z + enclosure.max.z + 1) * zoo::kTileSize,
                              std::move(text), true});
    }
    for (const zoo::Building& building : zoo_->Objects().Buildings()) {
        if (building.def < 0)
            continue;
        const zoo::BuildingDef& def = zoo_->Definitions().buildings[static_cast<std::size_t>(building.def)];
        if (def.role == zoo::BuildingRole::AnimalShelter || def.role == zoo::BuildingRole::StaffRoom ||
            def.role == zoo::BuildingRole::VetClinic || def.role == zoo::BuildingRole::ResearchCenter)
            continue;
        const zoo::TileCoord size = zoo::FootprintSize(def, building.facing);
        map.labels.push_back({(static_cast<float>(building.origin.x) + 0.5f * static_cast<float>(size.x)) * zoo::kTileSize,
                              (static_cast<float>(building.origin.z) + 0.5f * static_cast<float>(size.z)) * zoo::kTileSize,
                              def.name, false});
    }
    map.labels.push_back({(static_cast<float>(gate.x) + 0.5f) * zoo::kTileSize,
                          (static_cast<float>(gate.z) - 1.2f) * zoo::kTileSize, "Entrance", false});
    visit_.map = std::move(map);
}

}  // namespace zoogame
