// SPDX-License-Identifier: MIT
#include "zoo/sim/ScenarioScript.hpp"

#include <set>
#include <utility>

namespace zoo::scenario {
namespace {
using TileSet = std::set<std::pair<int, int>>;
TileSet UniqueTiles(const std::vector<TileCoord>& tiles) {
    TileSet result;
    for (const TileCoord tile : tiles) {
        result.emplace(tile.z, tile.x);
    }
    return result;
}
}  // namespace

ScenarioReport Script::Finish() {
    report_.habitats.clear();
    for (const Enclosure& enclosure : zoo_.Habitats().All()) {
        report_.habitats.push_back({enclosure.id, enclosure.name, {}});
    }
    for (const Animal& animal : zoo_.Fauna().All()) {
        if (!animal.InWorld() || animal.health <= 0.0f || animal.species < 0 ||
            static_cast<std::size_t>(animal.species) >= zoo_.SpeciesList().All().size()) {
            continue;
        }
        const auto habitat = std::find_if(
            report_.habitats.begin(), report_.habitats.end(),
            [home = animal.home](const ScenarioHabitatReport& item) { return item.enclosure == home; });
        if (habitat == report_.habitats.end()) {
            continue;
        }
        const std::string& species = zoo_.SpeciesList().All()[static_cast<std::size_t>(animal.species)].id;
        auto resident =
            std::find_if(habitat->residents.begin(), habitat->residents.end(),
                         [&species](const ScenarioResidentReport& item) { return item.species == species; });
        if (resident == habitat->residents.end()) {
            habitat->residents.push_back({species, 1});
        } else {
            ++resident->animals;
        }
    }
    for (ScenarioHabitatReport& habitat : report_.habitats) {
        std::sort(habitat.residents.begin(), habitat.residents.end(),
                  [](const ScenarioResidentReport& a, const ScenarioResidentReport& b) {
                      return a.species < b.species;
                  });
    }
    return report_;
}

void Script::PathTiles(const std::vector<TileCoord>& tiles, PathKind kind) {
    std::vector<TileCoord> changed;
    for (const auto& [z, x] : UniqueTiles(tiles)) {
        const TileCoord tile{x, z};
        if (!zoo_.Map().InBounds(tile) || zoo_.Map().At(tile).path != kind) {
            changed.push_back(tile);
        }
    }
    if (changed.empty()) {
        return;
    }
    const BuildPlan plan = build_.PlacePath(changed, kind);
    if (!plan.Ok() || plan.skipped > 0) {
        Problem("walk: " + std::string(Describe(plan.Ok() ? plan.skippedBecause : plan.error)));
    }
}

void Script::PathAlong(const std::vector<TileCoord>& points, int widthTiles, PathKind kind) {
    if (points.size() < 2 || widthTiles < 1) {
        Problem("walk needs two points and a positive width");
        return;
    }
    std::vector<TileCoord> tiles;
    const float radius = static_cast<float>(widthTiles) * 0.5f;
    for (std::size_t i = 1; i < points.size(); ++i) {
        const Vec2 a{static_cast<float>(points[i - 1].x), static_cast<float>(points[i - 1].z)};
        const Vec2 b{static_cast<float>(points[i].x), static_cast<float>(points[i].z)};
        const Vec2 delta = b - a;
        const float length2 = Dot(delta, delta);
        for (int n = std::min(points[i - 1].z, points[i].z) - widthTiles;
             n <= std::max(points[i - 1].z, points[i].z) + widthTiles; ++n) {
            for (int e = std::min(points[i - 1].x, points[i].x) - widthTiles;
                 e <= std::max(points[i - 1].x, points[i].x) + widthTiles; ++e) {
                const Vec2 p{static_cast<float>(e), static_cast<float>(n)};
                const float t = length2 > 0.0f ? std::clamp(Dot(p - a, delta) / length2, 0.0f, 1.0f) : 0.0f;
                if (DistanceSquared(p, a + delta * t) <= radius * radius) {
                    tiles.push_back(Tile(e, n));
                }
            }
        }
    }
    PathTiles(tiles, kind);
}

std::vector<TileCoord> Script::Ellipse(int east, int north, float radiusEast, float radiusNorth) const {
    std::vector<TileCoord> tiles;
    if (radiusEast <= 0.0f || radiusNorth <= 0.0f) {
        return tiles;
    }
    const int re = static_cast<int>(std::ceil(radiusEast));
    const int rn = static_cast<int>(std::ceil(radiusNorth));
    for (int n = -rn; n <= rn; ++n) {
        for (int e = -re; e <= re; ++e) {
            const float de = static_cast<float>(e) / radiusEast;
            const float dn = static_cast<float>(n) / radiusNorth;
            if (de * de + dn * dn <= 1.0f) {
                tiles.push_back(Tile(east + e, north + n));
            }
        }
    }
    return tiles;
}

std::vector<TileCoord> Script::Arc(int east, int north, float radiusEast, float radiusNorth, float start,
                                   float end) {
    std::vector<TileCoord> points;
    const int steps = std::max(
        2, static_cast<int>(std::ceil(std::abs(end - start) * std::max(radiusEast, radiusNorth) * 2.0f)));
    for (int i = 0; i <= steps; ++i) {
        const float angle = start + (end - start) * static_cast<float>(i) / static_cast<float>(steps);
        const TileCoord point{east + static_cast<int>(std::lround(radiusEast * std::cos(angle))),
                              north + static_cast<int>(std::lround(radiusNorth * std::sin(angle)))};
        if (points.empty() || points.back() != point) {
            points.push_back(point);
        }
    }
    return points;
}

std::vector<EdgeRef> Script::Outline(const std::vector<TileCoord>& tiles) {
    const TileSet region = UniqueTiles(tiles);
    std::vector<EdgeRef> edges;
    for (const auto& [z, x] : region) {
        for (const Dir side : {Dir::North, Dir::East, Dir::South, Dir::West}) {
            const TileCoord delta = Step(side);
            if (!region.contains({z + delta.z, x + delta.x})) {
                edges.push_back(EdgeOf({x, z}, side));
            }
        }
    }
    return edges;
}

EnclosureId Script::EncloseTiles(const std::vector<TileCoord>& tiles, std::string_view fence,
                                 std::string name) {
    if (tiles.empty()) {
        Problem(name + ": empty region");
        return {};
    }
    const BuildPlan plan = build_.PlaceFence(Outline(tiles), FenceKind(fence));
    if (!plan.Ok() || plan.skipped > 0) {
        Problem(name + ": " + std::string(Describe(plan.Ok() ? plan.skippedBecause : plan.error)));
        return {};
    }
    const EnclosureId id = zoo_.Habitats().At(tiles.front());
    if (!id) {
        Problem(name + ": no enclosed region");
        return {};
    }
    for (const TileCoord tile : tiles) {
        if (zoo_.Habitats().At(tile) != id || zoo_.Map().At(tile).path != PathKind::None) {
            Problem(name + ": disconnected region or a walk inside the habitat");
            break;
        }
    }
    zoo_.RenameEnclosure(id, std::move(name));
    ++report_.enclosures;
    return id;
}

void Script::ViewingFront(const std::vector<TileCoord>& tiles, std::string_view fence,
                          std::initializer_list<Dir> sides) {
    std::vector<EdgeRef> front;
    for (const EdgeRef edge : Outline(tiles)) {
        if (zoo_.Map().FenceAt(edge).kind == FenceKind(fence)) {
            continue;
        }
        const TileCoord a{edge.x, edge.z};
        const TileCoord b{edge.x - (edge.horizontal ? 0 : 1), edge.z - (edge.horizontal ? 1 : 0)};
        for (const TileCoord tile : {a, b}) {
            if (zoo_.Map().InBounds(tile) && zoo_.Habitats().RegionAt(tile) == Region::Public &&
                zoo_.Map().At(tile).path != PathKind::None) {
                const Dir side = edge.horizontal ? (tile == a ? Dir::South : Dir::North)
                                                 : (tile == a ? Dir::East : Dir::West);
                if (std::find(sides.begin(), sides.end(), side) != sides.end()) {
                    front.push_back(edge);
                }
                break;
            }
        }
    }
    if (!front.empty()) {
        const BuildPlan plan = build_.PlaceFence(front, FenceKind(fence));
        if (!plan.Ok() && plan.error != BuildError::NothingToDo) {
            Problem("viewing front: " + std::string(Describe(plan.error)));
        }
    }
}

std::vector<TileCoord> Script::Expanded(const std::vector<TileCoord>& tiles, int depth) {
    std::set<std::pair<int, int>> coordinates;
    for (const TileCoord tile : tiles) {
        for (int dz = -depth; dz <= depth; ++dz) {
            for (int dx = -depth; dx <= depth; ++dx) {
                coordinates.emplace(tile.z + dz, tile.x + dx);
            }
        }
    }
    std::vector<TileCoord> expanded;
    for (const auto& [z, x] : coordinates) {
        expanded.push_back({x, z});
    }
    return expanded;
}

std::vector<TileCoord> Script::StandOffFront(const std::vector<TileCoord>& tiles, int depth,
                                             const std::vector<TileCoord>& extra) {
    auto expanded = Expanded(tiles, depth);
    expanded.insert(expanded.end(), extra.begin(), extra.end());
    std::set<std::pair<int, int>> inside;
    for (const TileCoord tile : tiles) {
        inside.emplace(tile.z, tile.x);
    }
    std::vector<TileCoord> strip;
    for (const TileCoord tile : expanded) {
        if (!inside.contains({tile.z, tile.x})) {
            strip.push_back(tile);
        }
    }
    const auto mark = build_.PlaceStandOff(strip);
    if (!mark.Ok() || mark.skipped > 0) {
        Problem("stand-off: " + std::string(Describe(mark.Ok() ? mark.skippedBecause : mark.error)));
        return {};
    }
    std::vector<EdgeRef> publicEdges;
    for (const EdgeRef edge : Outline(expanded)) {
        // A neighbouring habitat already has its own containment; never replace it
        // with a public rail where the two planting boundaries meet.
        if (!zoo_.Map().FenceAt(edge).Exists()) {
            publicEdges.push_back(edge);
        }
    }
    const auto rail = build_.PlaceFence(publicEdges, FenceKind("stand_off_rail"));
    if (!rail.Ok() || rail.skipped > 0) {
        Problem("stand-off rail: " + std::string(Describe(rail.Ok() ? rail.skippedBecause : rail.error)));
    }
    return strip;
}

void Script::KeeperGate(const std::vector<TileCoord>& tiles, std::string_view fence) {
    const std::vector<EdgeRef> outline = Outline(tiles);
    if (!outline.empty()) {
        const BuildPlan plan = build_.PlaceFence({outline.data(), 1}, FenceKind(fence), true);
        if (!plan.Ok()) {
            Problem("keeper gate: " + std::string(Describe(plan.error)));
        }
    }
}
}  // namespace zoo::scenario
