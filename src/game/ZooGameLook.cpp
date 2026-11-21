// SPDX-License-Identifier: MIT
// What a visitor meets on the walk: the animal the eyes rest on, the boards at the
// enclosures (E), the map of the zoo (M).
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <optional>

#include "Hud.hpp"
#include "SightLine.hpp"
#include "ZooGame.hpp"
#include "zoo/visitors/VisitorTuning.hpp"

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

/// A calendar hour as a clock shows it: 9.5 -> "9:30".
std::string ClockText(float hour) {
    const int minutes = static_cast<int>(std::lround(hour * 60.0f));
    return Format("%d:%02d", minutes / 60, minutes % 60);
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
            if (Automation().Options().Automated()) {
                std::string names;
                for (const VisitMap::Place& place : visit_.map.places)
                    names += (names.empty() ? "" : "; ") + place.name;
                std::printf("zoo: map: %zu places: %s\n", visit_.map.places.size(), names.c_str());
            }
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
    // Through binoculars the eyes reach across the zoo and are as precise as the view is narrow.
    const float magnified = Magnification();
    const float reach = std::min(kLookReach * magnified, 300.0f);
    for (const zoo::Animal& animal : zoo_->Fauna().All()) {
        if (!animal.Visible() || animal.species < 0)
            continue;
        const zoo::SpeciesDef& def = species[static_cast<std::size_t>(animal.species)];
        const Vector3 body(animal.position.x,
                           zoo_->Map().HeightAt(animal.position) + 0.55f * def.shoulderHeightM,
                           animal.position.z);
        const Vector3 to = body - eye;
        const float along = to.X * gaze.X + to.Y * gaze.Y + to.Z * gaze.Z;
        if (along < 0.8f || along > reach)
            continue;
        const float aside =
            std::sqrt(std::max(0.0f, to.X * to.X + to.Y * to.Y + to.Z * to.Z - along * along));
        // A far animal is a small target: the eyes are allowed a little more there.
        const float size = std::max(0.45f, 0.5f * def.bodyLengthM) + 0.2f + 0.012f * along / magnified;
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
    float nearest = 8.0f;
    for (const zoo::Enclosure& enclosure : zoo_->Habitats().All()) {
        const float distance = DistanceToTiles(board.position, enclosure.min, enclosure.max);
        if (distance >= nearest)
            continue;
        const bool livedIn = std::ranges::any_of(
            zoo_->Fauna().All(), [&](const zoo::Animal& a) { return a.InWorld() && a.home == enclosure.id; });
        if (livedIn) {
            nearest = distance;
            home = &enclosure;
        }
    }
    if (home == nullptr) {
        BoardText text;
        text.title = zoo_->Config().name;
        text.latin = "Welcome to the zoo";
        text.rows.emplace_back(
            "Open today", ClockText(zoo::visitor_tuning::kOpeningHour) + " - " +
                              ClockText(zoo::visitor_tuning::kClosingHour));
        text.rows.emplace_back("Last admission", ClockText(zoo::visitor_tuning::kLastAdmissionHour));
        const std::size_t animals = std::ranges::count_if(
            zoo_->Fauna().All(), [](const zoo::Animal& animal) { return animal.InWorld(); });
        text.rows.emplace_back(
            "Animals today",
            Format("%zu in %zu habitats", animals, zoo_->Habitats().All().size()));
        text.facts = {"Follow the paths to the animal habitats.",
                      "Press M to see the visitor map.",
                      "Press E beside a habitat board to learn about its animals."};
        visit_.board = std::move(text);
        visit_.window = VisitWindow::Board;
        boardRead_ = board.position;
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
    text.rows.emplace_back("Lifespan",
                           Format("about %.0f years in human care", static_cast<double>(def.lifespanYears)));
    text.rows.emplace_back("Conservation", std::string(zoo::DisplayName(def.status)));
    // When the keeper feeds where the public can watch (zoo::Staff, docs/KEEPERS.md).
    const std::array<float, 2> feeding = zoo_->Keepers().FeedingHours(home->id);
    if (feeding[0] >= 0.0f && feeding[1] >= 0.0f) {
        std::string when = ClockText(feeding[0]) + " and " + ClockText(feeding[1]);
        if (const std::optional<float> next = zoo_->Keepers().NextFeeding(zoo_->MakeEnv(), home->id))
            when += ", next at " + ClockText(*next);
        else
            when += ", none left today";
        text.rows.emplace_back("Keeper feeds", std::move(when));
    }
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
    // "habitat:1:giraffe" looks at a giraffe of the second habitat; "habitat:giraffe" at one
    // wherever giraffes live, which does not depend on which habitats have somebody outdoors.
    std::string_view only;
    bool bySpecies = false;
    if (colon != std::string_view::npos) {
        const std::string_view rest = what.substr(colon + 1);
        wanted = std::atoi(std::string(rest).c_str());
        if (const std::size_t second = rest.find(':'); second != std::string_view::npos) {
            only = rest.substr(second + 1);
        } else if (kind == "habitat" && !rest.empty() && (rest[0] < '0' || rest[0] > '9')) {
            only = rest;
            bySpecies = true;
        }
    }
    const auto centre = [](zoo::TileCoord t) {
        return zoo::Vec2{(static_cast<float>(t.x) + 0.5f) * zoo::kTileSize,
                         (static_cast<float>(t.z) + 0.5f) * zoo::kTileSize};
    };
    if (kind == "habitat") {
        for (const zoo::Enclosure& enclosure : zoo_->Habitats().All()) {
            // The spot at the fence and the animal that are nearest to each other, where a
            // visitor would in fact stand: at the glass rather than at the mesh, and with no
            // trunk, rock or shelter between them and the animal.
            const auto blocked = [&](zoo::Vec2 from, zoo::Vec2 to) {
                const float sx = to.x - from.x;
                const float sz = to.z - from.z;
                const float length2 = std::max(sx * sx + sz * sz, 1.0e-4f);
                const int samples = std::max(
                    1, static_cast<int>(std::ceil(std::sqrt(length2) / (0.25f * zoo::kTileSize))));
                for (int sample = 1; sample < samples; ++sample) {
                    const float along = static_cast<float>(sample) / static_cast<float>(samples);
                    const zoo::TileCoord tile = zoo::World::TileAt(
                        {from.x + along * sx, from.z + along * sz});
                    if (zoo_->Map().InBounds(tile) && zoo_->Map().At(tile).building)
                        return true;
                }
                for (const zoo::Prop& prop : zoo_->Objects().Props()) {
                    if (prop.def < 0)
                        continue;
                    const zoo::PropDef& def = zoo_->Definitions().props[static_cast<std::size_t>(prop.def)];
                    if (def.heightM * prop.scale < 1.0f)
                        continue;
                    // The planting of a stand-off is kept below the eye: one looks over it.
                    if (def.heightM * prop.scale < 1.5f &&
                        zoo_->Map().At(zoo::World::TileAt(prop.position)).standOff)
                        continue;
                    const float along = std::clamp(
                        ((prop.position.x - from.x) * sx + (prop.position.z - from.z) * sz) / length2, 0.0f,
                        0.92f);
                    const float px = from.x + along * sx - prop.position.x;
                    const float pz = from.z + along * sz - prop.position.z;
                    const float reach = SightBlockingRadius(def, prop.scale) + 0.6f;
                    if (px * px + pz * pz < reach * reach)
                        return true;
                }
                return false;
            };
            // How many fences the line of sight passes: one is the fence one stands at; more
            // means looking through the next bay of a stepped boundary, all mesh and posts.
            const auto fences = [&](zoo::Vec2 from, zoo::Vec2 to) {
                const float sx = to.x - from.x;
                const float sz = to.z - from.z;
                const int steps = std::max(1, static_cast<int>(std::sqrt(sx * sx + sz * sz) / 0.2f));
                int crossed = 0;
                zoo::TileCoord tile = zoo::World::TileAt(from);
                for (int step = 1; step <= steps; ++step) {
                    const float along = static_cast<float>(step) / static_cast<float>(steps);
                    const zoo::TileCoord next =
                        zoo::World::TileAt({from.x + along * sx, from.z + along * sz});
                    for (const zoo::Dir side :
                         {zoo::Dir::North, zoo::Dir::East, zoo::Dir::South, zoo::Dir::West}) {
                        const zoo::TileCoord way = zoo::Step(side);
                        const bool moved = (way.x != 0 && next.x - tile.x == way.x) ||
                                           (way.z != 0 && next.z - tile.z == way.z);
                        // The rail before a planted stand-off is leant on, not looked through.
                        const bool rail = zoo_->Map().InBounds(tile) && zoo_->Map().InBounds(next) &&
                                          !zoo_->Map().At(tile).standOff && zoo_->Map().At(next).standOff;
                        if (moved && !rail && zoo_->Map().InBounds(tile) &&
                            zoo_->Map().FenceOn(tile, side).Exists())
                            ++crossed;
                    }
                    tile = next;
                }
                return crossed;
            };
            float best = 1.0e9f;
            bool populated = false;
            Pose pose;
            for (const zoo::Animal& animal : zoo_->Fauna().All()) {
                if (!animal.Visible() || !(animal.home == enclosure.id))
                    continue;
                populated = !enclosure.viewing.empty();
                if (!only.empty() &&
                    (animal.species < 0 ||
                     zoo_->SpeciesList().All()[static_cast<std::size_t>(animal.species)].id != only))
                    continue;
                for (const zoo::ViewingSpot& spot : enclosure.viewing) {
                    // A transparent keeper gate is technically a view, but not a public
                    // viewing front: its close mesh fills the picture and often frames the
                    // service side of a shelter. Find the containment barrier beyond any
                    // planted stand-off and leave service gates to the keepers.
                    const zoo::TileCoord toward = zoo::Step(spot.side);
                    zoo::TileCoord inside = spot.tile + toward;
                    while (zoo_->Map().InBounds(inside) && zoo_->Map().At(inside).standOff)
                        inside = inside + toward;
                    if (!zoo_->Map().InBounds(inside))
                        continue;
                    const zoo::Fence barrier = zoo_->Map().FenceOn(inside, zoo::Opposite(spot.side));
                    const zoo::FenceDef* barrierDef = zoo_->Definitions().FenceByKind(barrier.kind);
                    if (barrier.gate || barrierDef == nullptr)
                        continue;
                    // Prefer the front a visitor facility is designed around, then choose the
                    // best visible animal along that class of barrier. A nearby animal at the
                    // service mesh must not outweigh glass or a dry moat farther round the walk.
                    float front = 12.0f;
                    if (barrierDef->style == zoo::FenceStyle::Glass ||
                        barrierDef->style == zoo::FenceStyle::Moat)
                        front = 1.0f;
                    else if (barrierDef->style == zoo::FenceStyle::Rail ||
                             barrierDef->style == zoo::FenceStyle::Cable)
                        front = 1.5f;
                    else if (barrierDef->style == zoo::FenceStyle::Hedge)
                        front = 30.0f;
                    // Devalue an end panel where the service barrier fills one side of the
                    // frame, but do not let that preference outweigh an unobstructed animal.
                    const zoo::Dir barrierSide = zoo::Opposite(spot.side);
                    const bool horizontal = barrierSide == zoo::Dir::North ||
                                            barrierSide == zoo::Dir::South;
                    int span = 0;
                    for (const zoo::Dir along : horizontal
                                                   ? std::array{zoo::Dir::East, zoo::Dir::West}
                                                   : std::array{zoo::Dir::North, zoo::Dir::South}) {
                        const zoo::TileCoord beside = inside + zoo::Step(along);
                        if (!zoo_->Map().InBounds(beside) ||
                            !(zoo_->Habitats().At(beside) == enclosure.id))
                            continue;
                        const zoo::Fence neighbour = zoo_->Map().FenceOn(beside, barrierSide);
                        const zoo::FenceDef* neighbourDef =
                            zoo_->Definitions().FenceByKind(neighbour.kind);
                        span += !neighbour.gate && neighbourDef != nullptr &&
                                        neighbourDef->style == barrierDef->style
                                    ? 1
                                    : 0;
                    }
                    const auto worse = [&](float score) { return score >= best; };
                    // Where somebody stands to look: a short step back from the fence (0.8 m),
                    // not with the nose between its bars.
                    const zoo::Vec2 middle = centre(spot.tile);
                    // At a rail before a stand-off one comes right up to it.
                    const float forward = spot.standOffM > 0.0f ? 0.5f : 0.2f;
                    const zoo::Vec2 at{middle.x + forward * static_cast<float>(toward.x),
                                       middle.z + forward * static_cast<float>(toward.z)};
                    if (!ClearOfPlayerProps(*zoo_, at))
                        continue;
                    const float dx = animal.position.x - at.x;
                    const float dz = animal.position.z - at.z;
                    const float sight = std::max(spot.visibility, 0.2f);
                    // Some six metres off is where an animal fills the view without the eye
                    // being on top of it.
                    const float off = std::sqrt(dx * dx + dz * dz) - 6.0f;
                    float score = (off * off + 1.0f) / (sight * sight * sight * sight);
                    score *= front * (span == 2 ? 1.0f : span == 1 ? 2.0f : 4.0f);
                    if (worse(score))
                        continue;
                    // Through the fence rather than along it: looking along it, the eye is
                    // full of posts and rails and the animal is a strip between them.
                    const float distanceNow = std::sqrt(dx * dx + dz * dz);
                    const float square =
                        (dx * static_cast<float>(toward.x) + dz * static_cast<float>(toward.z)) /
                        std::max(distanceNow, 0.01f);
                    if (square < 0.9f)
                        score *= 1.0f + 16.0f * (0.9f - square);
                    if (worse(score))
                        continue;
                    if (blocked(at, {animal.position.x, animal.position.z}))
                        score *= 16.0f;
                    if (worse(score))
                        continue;
                    if (fences(at, {animal.position.x, animal.position.z}) > 1)
                        score *= 40.0f;
                    // Not behind somebody's head.
                    for (const zoo::Visitor& visitor : zoo_->Guests().All()) {
                        const float vx = visitor.position.x - at.x;
                        const float vz = visitor.position.z - at.z;
                        const float ahead = (vx * dx + vz * dz) / std::max(distanceNow, 0.01f);
                        const float aside2 = vx * vx + vz * vz - ahead * ahead;
                        if (ahead > -0.3f && ahead < 2.5f && aside2 < 0.8f * 0.8f) {
                            score *= 30.0f;
                            break;
                        }
                    }
                    if (!worse(score)) {
                        best = score;
                        // At the middle of the animal: down at a wolf lying at the fence, level
                        // at one far off, up at a giraffe.
                        const float distance = std::sqrt(dx * dx + dz * dz);
                        float middleOfIt = 0.45f;
                        if (animal.species >= 0)
                            middleOfIt = 0.55f * zoo_->SpeciesList()
                                                     .All()[static_cast<std::size_t>(animal.species)]
                                                     .shoulderHeightM;
                        const float down = std::atan2(1.65f - middleOfIt, std::max(distance, 0.5f));
                        pose = Pose{at.x, at.z, std::atan2(dx, dz), std::clamp(down, -0.5f, 0.9f)};
                    }
                }
            }
            if (bySpecies) {
                if (best < 1.0e9f)
                    return pose;
                continue;
            }
            // Habitats count by who lives in them, whichever species is asked for.
            if (populated && wanted-- == 0)
                return best < 1.0e9f ? std::optional<Pose>(pose) : std::nullopt;
        }
    } else if (kind == "board") {
        for (const zoo::Prop& prop : zoo_->Objects().Props()) {
            if (prop.def < 0 ||
                zoo_->Definitions().props[static_cast<std::size_t>(prop.def)].role !=
                    zoo::PropRole::InfoBoard ||
                wanted-- != 0)
                continue;
            // From the path side when there is one.
            zoo::Vec2 at{prop.position.x, prop.position.z + 1.7f};
            for (int step = 0; step < 8; ++step) {
                const float angle = 0.785398f * static_cast<float>(step);
                const zoo::Vec2 tried{prop.position.x + 1.7f * std::sin(angle),
                                      prop.position.z + 1.7f * std::cos(angle)};
                const zoo::TileCoord tile = zoo::World::TileAt(tried);
                if (zoo_->Map().InBounds(tile) && zoo_->Map().At(tile).path != zoo::PathKind::None) {
                    at = tried;
                    break;
                }
            }
            return Pose{at.x, at.z, std::atan2(prop.position.x - at.x, prop.position.z - at.z), 0.2f};
        }
    } else if (kind == "keeper") {
        // "keeper:3" looks at the member of staff with that id, "keeper:0" at the first one
        // who is at a job; "keeper:3:rake" only if that is what they are doing. From the
        // nearest piece of path that is a few metres away: where a visitor would see them from.
        for (const zoo::StaffMember& member : zoo_->Keepers().All()) {
            const zoo::StaffTask* task = member.Task();
            const bool atJob = member.working && task != nullptr && task->work != zoo::StaffActivity::Rest;
            if (wanted == 0 ? !atJob : member.id.value != static_cast<std::uint32_t>(wanted))
                continue;
            if (!only.empty() && (task == nullptr || zoo::ToString(member.working ? task->work : member.activity) != only))
                continue;
            constexpr float kNearest = 4.5f;
            constexpr int kReach = 9;  // tiles
            const zoo::TileCoord middle = zoo::World::TileAt(member.position);
            zoo::Vec2 at{member.position.x, member.position.z - 6.0f};
            float best = 1.0e9f;
            for (int dz = -kReach; dz <= kReach; ++dz) {
                for (int dx = -kReach; dx <= kReach; ++dx) {
                    const zoo::TileCoord tile{middle.x + dx, middle.z + dz};
                    if (!zoo_->Map().InBounds(tile) || zoo_->Map().At(tile).path == zoo::PathKind::None)
                        continue;
                    const zoo::Vec2 from = centre(tile);
                    const float distance = std::hypot(from.x - member.position.x, from.z - member.position.z);
                    if (distance >= kNearest && distance < best) {
                        best = distance;
                        at = from;
                    }
                }
            }
            const float dx = member.position.x - at.x;
            const float dz = member.position.z - at.z;
            std::fprintf(stderr, "zoo: keeper %u (%s) %s, %.1f m away at %.1f, %.1f\n", member.id.value,
                         std::string(zoo::DisplayName(member.role)).c_str(),
                         std::string(zoo::Describe(member.working && task != nullptr ? task->work : member.activity))
                             .c_str(),
                         static_cast<double>(std::hypot(dx, dz)), static_cast<double>(member.position.x),
                         static_cast<double>(member.position.z));
            return Pose{at.x, at.z, std::atan2(dx, dz), std::clamp(std::atan2(0.75f, std::hypot(dx, dz)), 0.0f, 0.3f)};
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
                map.shapes.push_back({static_cast<float>(x) * zoo::kTileSize,
                                      static_cast<float>(z) * zoo::kTileSize,
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
    map.shapes.push_back({(static_cast<float>(gate.x) - 0.5f) * zoo::kTileSize,
                          static_cast<float>(gate.z) * zoo::kTileSize, 2.0f * zoo::kTileSize, zoo::kTileSize,
                          Kind::Entrance});

    // The names on the plan: who lives in each enclosure, what each house for visitors is.
    const auto& species = zoo_->SpeciesList().All();
    for (const zoo::Enclosure& enclosure : zoo_->Habitats().All()) {
        std::map<int, int> living;
        for (const zoo::Animal& animal : zoo_->Fauna().All())
            if (animal.InWorld() && animal.home == enclosure.id && animal.species >= 0)
                ++living[animal.species];
        // A habitat with a name of its own ("Lion Rock") shows it, also while it stands empty;
        // one that was only numbered shows who lives in it.
        std::string text = enclosure.name.starts_with("Enclosure ") ? std::string() : enclosure.name;
        if (text.empty()) {
            for (const auto& [index, count] : living) {
                if (!text.empty())
                    text += ", ";
                text += species[static_cast<std::size_t>(index)].commonName;
            }
        }
        if (text.empty())
            continue;
        map.labels.push_back(
            {0.5f * static_cast<float>(enclosure.min.x + enclosure.max.x + 1) * zoo::kTileSize,
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
        const float x =
            (static_cast<float>(building.origin.x) + 0.5f * static_cast<float>(size.x)) * zoo::kTileSize;
        const float z =
            (static_cast<float>(building.origin.z) + 0.5f * static_cast<float>(size.z)) * zoo::kTileSize;
        // A row of stalls is named once, not stall by stall.
        const bool named = std::ranges::any_of(map.labels, [&](const VisitMap::Label& other) {
            return !other.habitat && other.text == def.name && std::hypot(other.x - x, other.z - z) < 70.0f;
        });
        if (!named)
            map.labels.push_back({x, z, def.name, false});
    }
    map.labels.push_back({(static_cast<float>(gate.x) + 0.5f) * zoo::kTileSize,
                          (static_cast<float>(gate.z) - 1.2f) * zoo::kTileSize, "Entrance", false});
    AddMapPlaces(map);
    visit_.map = std::move(map);
}

void ZooGame::AddMapPlaces(VisitMap& map) const {
    // The paved tile nearest to `tile` within `reach` tiles, nearest first.
    const auto paved = [&](zoo::TileCoord tile, int reach) -> std::optional<zoo::TileCoord> {
        std::optional<zoo::TileCoord> best;
        int distance = reach * reach + 1;
        for (int dz = -reach; dz <= reach; ++dz)
            for (int dx = -reach; dx <= reach; ++dx) {
                const zoo::TileCoord next{tile.x + dx, tile.z + dz};
                if (dx * dx + dz * dz < distance && CanLandOnPath(*zoo_, next)) {
                    distance = dx * dx + dz * dz;
                    best = next;
                }
            }
        return best;
    };

    const Pose gate = EntrancePose();
    if (const auto entrance = paved(zoo::World::TileAt({gate.x, gate.z}), 6)) {
        const auto at = zoo::World::TileCenter(*entrance);
        map.places.push_back({at.x, at.z, gate.yaw, "Entrance", false});
    }

    // A habitat: at its main front, the viewing place farthest to the south (the side the walks
    // come from), looking straight across the barrier; a place where people stand at the glass
    // gives way to a free one nearby, so that one does not arrive nose to nose with a stranger.
    std::map<std::pair<int, int>, int> crowd;
    for (const zoo::Visitor& guest : zoo_->Guests().All()) {
        const zoo::TileCoord tile = zoo::World::TileAt(guest.position);
        ++crowd[{tile.x, tile.z}];
    }
    const auto people = [&](zoo::TileCoord tile) {
        int count = 0;
        for (int dz = -1; dz <= 1; ++dz)
            for (int dx = -1; dx <= 1; ++dx)
                if (const auto it = crowd.find({tile.x + dx, tile.z + dz}); it != crowd.end())
                    count += it->second;
        return count;
    };
    for (const zoo::Enclosure& enclosure : zoo_->Habitats().All()) {
        if (enclosure.viewing.empty() || enclosure.name.starts_with("Enclosure "))
            continue;
        const zoo::Vec2 centre{
            0.5f * static_cast<float>(enclosure.min.x + enclosure.max.x + 1) * zoo::kTileSize,
            0.5f * static_cast<float>(enclosure.min.z + enclosure.max.z + 1) * zoo::kTileSize};
        const zoo::ViewingSpot* best = nullptr;
        float score = 1.0e9f;
        for (const zoo::ViewingSpot& spot : enclosure.viewing) {
            const zoo::Vec2 at = zoo::World::TileCenter(spot.tile);
            const float next = std::hypot(at.x - centre.x, at.z - (centre.z + 100.0f)) +
                               15.0f * static_cast<float>(people(spot.tile));
            if (next < score && CanLandOnPath(*zoo_, spot.tile)) {
                score = next;
                best = &spot;
            }
        }
        if (best == nullptr)
            continue;
        const zoo::TileCoord toward = zoo::Step(best->side);
        const zoo::Vec2 at = zoo::World::TileCenter(best->tile);
        map.places.push_back({at.x, at.z,
                              std::atan2(static_cast<float>(toward.x), static_cast<float>(toward.z)),
                              enclosure.name, true});
    }
    const std::size_t habitats = map.places.size();

    // Houses for visitors that stand together (a food court, the plaza) are one place, named by
    // the most notable of them; a lone drinks or ice-cream kiosk along a walk is not a place to go
    // to. One stands on the walk a few steps before the first door, looking at its house.
    struct Group {
        VisitMap::Place place;
        int stalls = 0;
        bool restroom = false, shop = false, education = false;
    };
    std::vector<Group> groups;
    for (const zoo::Building& building : zoo_->Objects().Buildings()) {
        if (building.def < 0)
            continue;
        const zoo::BuildingDef& def = zoo_->Definitions().buildings[static_cast<std::size_t>(building.def)];
        using Role = zoo::BuildingRole;
        if (def.role != Role::FoodStall && def.role != Role::DrinkStall && def.role != Role::Restroom &&
            def.role != Role::GiftShop && def.role != Role::EducationCenter)
            continue;
        // A few steps back from the counter, out of the queue, with the house in view.
        const zoo::TileCoord out = zoo::Step(building.facing);
        const zoo::TileCoord front = zoo::FrontTile(def, building.origin, building.facing);
        const auto stand = paved({front.x + 3 * out.x, front.z + 3 * out.z}, 6);
        if (!stand)
            continue;
        const zoo::Vec2 at = zoo::World::TileCenter(*stand);
        auto group = std::ranges::find_if(groups, [&](const Group& other) {
            return std::hypot(other.place.x - at.x, other.place.z - at.z) < 30.0f;
        });
        if (group == groups.end()) {
            const zoo::TileCoord size = zoo::FootprintSize(def, building.facing);
            const float x =
                (static_cast<float>(building.origin.x) + 0.5f * static_cast<float>(size.x)) * zoo::kTileSize;
            const float z =
                (static_cast<float>(building.origin.z) + 0.5f * static_cast<float>(size.z)) * zoo::kTileSize;
            groups.push_back({{at.x, at.z, std::atan2(x - at.x, z - at.z), def.name, false}});
            group = std::prev(groups.end());
        }
        group->stalls += def.role == Role::FoodStall || def.role == Role::DrinkStall ? 1 : 0;
        group->restroom = group->restroom || def.role == Role::Restroom;
        group->shop = group->shop || def.role == Role::GiftShop;
        group->education = group->education || def.role == Role::EducationCenter;
    }
    for (Group& group : groups) {
        if (group.education)
            group.place.name = "Education centre";
        else if (group.shop)
            group.place.name = "Gift shop";
        else if (group.stalls >= 2)
            group.place.name = "Food court";
        else if (group.restroom)
            group.place.name = "Visitor restroom";
        else
            continue;
        map.places.push_back(std::move(group.place));
    }
    // Use the fixed habitat centres. Viewing places move as visitors gather at the glass.
    std::map<std::string, int> named;
    for (std::size_t i = habitats; i < map.places.size(); ++i)
        ++named[map.places[i].name];
    for (std::size_t i = habitats; i < map.places.size(); ++i) {
        VisitMap::Place& place = map.places[i];
        if (named[place.name] < 2)
            continue;
        const VisitMap::Label* near = nullptr;
        for (const auto& label : map.labels)
            if (label.habitat && (near == nullptr || std::hypot(label.x - place.x, label.z - place.z) <
                                                       std::hypot(near->x - place.x, near->z - place.z)))
                near = &label;
        if (near != nullptr)
            place.name += " by " + near->text;
    }
}

}  // namespace zoogame
