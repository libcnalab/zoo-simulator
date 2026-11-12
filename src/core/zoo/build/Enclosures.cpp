// SPDX-License-Identifier: MIT
#include "zoo/build/Enclosures.hpp"

#include <algorithm>
#include <tuple>
#include <utility>

namespace zoo {

namespace {

constexpr std::size_t kMaxNameLength = 40;

struct Component {
    TileCoord anchor;
    bool outside = false;
    std::int64_t code = 0;  // filled in by the classification
    int enclosure = -1;     // index into the new enclosure list
};

/// Running sums that become shares once all tiles are counted.
struct Tally {
    std::array<int, kTerrainCount> terrain{};
    int waterTiles = 0;
    float visibilitySum = 0.0f;
    bool hasBoundary = false;
    float weakestHeight = 0.0f;
    int weakestStrength = 0;
    float weakestCondition = 0.0f;
};

struct Barrier {
    float heightM = 0.0f;
    int strength = 0;
    float visibility = 0.0f;
};

Barrier BarrierOf(const Fence& fence, const Catalog& catalog) {
    if (fence.kind == kPerimeterFence) {
        return {kPerimeterHeightM, kPerimeterStrength, 0.0f};
    }
    const FenceDef* def = catalog.FenceByKind(fence.kind);
    if (def == nullptr) {
        return {};
    }
    return {def->heightM, def->strength, def->visibility};
}

auto EdgeKey(const EdgeRef& e) {
    return std::tuple(e.horizontal, e.z, e.x);
}

std::string Truncated(std::string name) {
    if (name.size() > kMaxNameLength) {
        name.resize(kMaxNameLength);
    }
    return name;
}

}  // namespace

void Enclosures::Rebuild(const World& world, const ZooObjects& objects, const Catalog& catalog) {
    const int width = world.Width();
    const int depth = world.Depth();
    const std::size_t count = static_cast<std::size_t>(width) * static_cast<std::size_t>(depth);
    const auto indexOf = [width](TileCoord t) {
        return static_cast<std::size_t>(t.z) * static_cast<std::size_t>(width) +
               static_cast<std::size_t>(t.x);
    };
    constexpr std::array<Dir, 4> kSides = {Dir::North, Dir::East, Dir::South, Dir::West};

    // ---- connected components -------------------------------------------------------
    labels_.assign(count, -1);
    std::vector<Component> components;
    for (int z = 0; z < depth; ++z) {
        for (int x = 0; x < width; ++x) {
            if (labels_[indexOf({x, z})] >= 0) {
                continue;
            }
            const auto label = static_cast<std::int32_t>(components.size());
            Component component;
            component.anchor = {x, z};  // row-major scan: the first tile met is the anchor
            labels_[indexOf({x, z})] = label;
            stack_.clear();
            stack_.push_back(static_cast<std::int32_t>(indexOf({x, z})));
            while (!stack_.empty()) {
                const std::int32_t index = stack_.back();
                stack_.pop_back();
                const TileCoord tile{index % width, index / width};
                if (!world.IsProperty(tile)) {
                    component.outside = true;
                }
                for (const Dir side : kSides) {
                    const TileCoord next = tile + Step(side);
                    if (!world.InBounds(next) || labels_[indexOf(next)] >= 0 ||
                        world.FenceOn(tile, side).Exists()) {
                        continue;
                    }
                    labels_[indexOf(next)] = label;
                    stack_.push_back(static_cast<std::int32_t>(indexOf(next)));
                }
            }
            components.push_back(component);
        }
    }

    // ---- classification and identity ------------------------------------------------
    constexpr std::int64_t kUnclaimed = 0;
    for (Component& component : components) {
        component.code = component.outside ? kOutsideCode : kUnclaimed;
    }
    if (world.InBounds(world.EntranceTile())) {
        Component& entrance = components[static_cast<std::size_t>(labels_[indexOf(world.EntranceTile())])];
        if (!entrance.outside) {
            entrance.code = kPublicCode;
        }
    }
    std::vector<Enclosure> next;
    for (Enclosure& old : enclosures_) {  // ascending id: the lower id wins a merge
        if (!world.InBounds(old.anchor)) {
            continue;
        }
        Component& component = components[static_cast<std::size_t>(labels_[indexOf(old.anchor)])];
        if (component.code != kUnclaimed) {
            continue;
        }
        component.code = static_cast<std::int64_t>(old.id.value);
        Enclosure kept;
        kept.id = old.id;
        kept.name = std::move(old.name);
        next.push_back(std::move(kept));
    }
    for (Component& component : components) {
        if (component.code != kUnclaimed) {
            continue;
        }
        Enclosure fresh;
        fresh.id = EnclosureId{nextId_++};
        fresh.name = "Enclosure " + std::to_string(fresh.id.value);
        component.code = static_cast<std::int64_t>(fresh.id.value);
        next.push_back(std::move(fresh));
    }
    std::sort(next.begin(), next.end(), [](const Enclosure& a, const Enclosure& b) { return a.id < b.id; });
    for (Component& component : components) {
        if (component.code <= 0) {
            continue;
        }
        const EnclosureId id{static_cast<std::uint32_t>(component.code)};
        const auto found = std::lower_bound(
            next.begin(), next.end(), id,
            [](const Enclosure& enclosure, EnclosureId wanted) { return enclosure.id < wanted; });
        if (found == next.end() || found->id != id) {
            continue;
        }
        component.enclosure = static_cast<int>(std::distance(next.begin(), found));
        found->anchor = component.anchor;
        found->min = component.anchor;
        found->max = component.anchor;
    }

    previousCodes_.swap(codes_);
    codes_.resize(count);
    for (std::size_t i = 0; i < count; ++i) {
        codes_[i] = components[static_cast<std::size_t>(labels_[i])].code;
    }
    if (width != width_ || depth != depth_ || codes_ != previousCodes_) {
        ++revision_;
    }
    width_ = width;
    depth_ = depth;

    // ---- statistics: one pass over the tiles ----------------------------------------
    std::vector<Tally> tallies(next.size());
    for (int z = 0; z < depth; ++z) {
        for (int x = 0; x < width; ++x) {
            const TileCoord tile{x, z};
            const std::int32_t label = labels_[indexOf(tile)];
            const int which = components[static_cast<std::size_t>(label)].enclosure;
            if (which < 0) {
                continue;
            }
            Enclosure& enclosure = next[static_cast<std::size_t>(which)];
            Tally& tally = tallies[static_cast<std::size_t>(which)];
            EnclosureStats& stats = enclosure.stats;
            ++stats.tiles;
            enclosure.min = {std::min(enclosure.min.x, x), std::min(enclosure.min.z, z)};
            enclosure.max = {std::max(enclosure.max.x, x), std::max(enclosure.max.z, z)};
            const Tile& data = world.At(tile);
            if (data.waterLevel != kNoWater) {
                ++tally.waterTiles;
                if (world.WaterDepthAt(World::TileCenter(tile)) >= kDeepWaterM) {
                    stats.deepWater = true;
                }
            } else {
                ++tally.terrain[static_cast<std::size_t>(data.terrain)];
            }
            for (const Dir side : kSides) {
                const TileCoord other = tile + Step(side);
                if (world.InBounds(other) && labels_[indexOf(other)] == label) {
                    continue;
                }
                const EdgeRef edge = EdgeOf(tile, side);
                const Fence fence = world.FenceAt(edge);
                const Barrier barrier = BarrierOf(fence, catalog);
                ++stats.boundaryEdges;
                if (fence.gate) {
                    ++stats.gates;
                    enclosure.gates.push_back(edge);
                }
                const auto strength = std::tuple(barrier.heightM, barrier.strength, fence.condition);
                const auto weakest =
                    std::tuple(tally.weakestHeight, tally.weakestStrength, tally.weakestCondition);
                if (!tally.hasBoundary) {
                    stats.minFenceHeightM = barrier.heightM;
                    stats.minFenceStrength = barrier.strength;
                    stats.minFenceCondition = fence.condition;
                }
                if (!tally.hasBoundary || strength < weakest ||
                    (strength == weakest && EdgeKey(edge) < EdgeKey(enclosure.weakestEdge))) {
                    tally.weakestHeight = barrier.heightM;
                    tally.weakestStrength = barrier.strength;
                    tally.weakestCondition = fence.condition;
                    enclosure.weakestEdge = edge;
                }
                tally.hasBoundary = true;
                stats.minFenceHeightM = std::min(stats.minFenceHeightM, barrier.heightM);
                stats.minFenceStrength = std::min(stats.minFenceStrength, barrier.strength);
                stats.minFenceCondition = std::min(stats.minFenceCondition, fence.condition);
                if (barrier.visibility <= 0.0f || !world.InBounds(other)) {
                    continue;
                }
                const Tile& beyond = world.At(other);
                if (codes_[indexOf(other)] == kPublicCode && beyond.path != PathKind::None &&
                    !beyond.building) {
                    enclosure.viewing.push_back({other, Opposite(side), barrier.visibility});
                    tally.visibilitySum += barrier.visibility;
                }
            }
        }
    }

    // ---- props and buildings ---------------------------------------------------------
    const auto enclosureAt = [&](TileCoord tile) -> Enclosure* {
        if (!world.InBounds(tile)) {
            return nullptr;
        }
        const int which = components[static_cast<std::size_t>(labels_[indexOf(tile)])].enclosure;
        return which < 0 ? nullptr : &next[static_cast<std::size_t>(which)];
    };
    for (const Prop& prop : objects.Props()) {
        Enclosure* enclosure = enclosureAt(World::TileAt(prop.position));
        if (enclosure == nullptr || prop.def < 0 || prop.def >= static_cast<int>(catalog.props.size())) {
            continue;
        }
        const PropDef& def = catalog.props[static_cast<std::size_t>(prop.def)];
        EnclosureStats& stats = enclosure->stats;
        stats.vegetationM2 += def.vegetationM2;
        stats.shadeM2 += def.shadeM2;
        stats.enrichment += def.enrichment;
        if (def.role == PropRole::Feeder) {
            ++stats.feeders;
            enclosure->feeders.push_back(prop.id);
        } else if (def.role == PropRole::WaterTrough) {
            ++stats.troughs;
            enclosure->troughs.push_back(prop.id);
        } else if (def.role == PropRole::Enrichment) {
            enclosure->enrichments.push_back(prop.id);
        }
    }
    for (const Building& building : objects.Buildings()) {
        if (building.def < 0 || building.def >= static_cast<int>(catalog.buildings.size())) {
            continue;
        }
        const BuildingDef& def = catalog.buildings[static_cast<std::size_t>(building.def)];
        if (def.role != BuildingRole::AnimalShelter) {
            continue;
        }
        Enclosure* enclosure = enclosureAt(DoorTile(def, building.origin, building.facing));
        if (enclosure == nullptr) {
            continue;
        }
        enclosure->shelters.push_back(building.id);
        if (building.open) {
            enclosure->stats.shelterCapacity += def.capacity;
            enclosure->stats.heatedShelter = enclosure->stats.heatedShelter || def.heated;
            enclosure->stats.cooledShelter = enclosure->stats.cooledShelter || def.cooled;
        }
    }

    // ---- shares and orders -----------------------------------------------------------
    for (std::size_t i = 0; i < next.size(); ++i) {
        Enclosure& enclosure = next[i];
        EnclosureStats& stats = enclosure.stats;
        const Tally& tally = tallies[i];
        const auto tiles = static_cast<float>(stats.tiles);
        stats.areaM2 = tiles * kTileSize * kTileSize;
        for (std::size_t t = 0; t < tally.terrain.size(); ++t) {
            stats.terrainShare[t] = static_cast<float>(tally.terrain[t]) / tiles;
        }
        stats.waterShare = static_cast<float>(tally.waterTiles) / tiles;
        const float dryM2 = static_cast<float>(stats.tiles - tally.waterTiles) * kTileSize * kTileSize;
        stats.vegetationDensity = dryM2 > 0.0f ? std::min(1.0f, stats.vegetationM2 / dryM2) : 0.0f;
        std::sort(enclosure.viewing.begin(), enclosure.viewing.end(),
                  [](const ViewingSpot& a, const ViewingSpot& b) {
                      return std::tuple(a.tile.z, a.tile.x, a.side) < std::tuple(b.tile.z, b.tile.x, b.side);
                  });
        int distinct = 0;
        for (std::size_t v = 0; v < enclosure.viewing.size(); ++v) {
            if (v == 0 || enclosure.viewing[v].tile != enclosure.viewing[v - 1].tile) {
                ++distinct;
            }
        }
        stats.viewingTiles = distinct;
        stats.visibility = enclosure.viewing.empty()
                               ? 0.0f
                               : tally.visibilitySum / static_cast<float>(enclosure.viewing.size());
        std::sort(enclosure.gates.begin(), enclosure.gates.end(),
                  [](const EdgeRef& a, const EdgeRef& b) { return EdgeKey(a) < EdgeKey(b); });
    }
    enclosures_ = std::move(next);
}

Region Enclosures::RegionAt(TileCoord t) const {
    if (t.x < 0 || t.z < 0 || t.x >= width_ || t.z >= depth_) {
        return Region::Outside;
    }
    const std::int64_t code = codes_[static_cast<std::size_t>(t.z) * static_cast<std::size_t>(width_) +
                                     static_cast<std::size_t>(t.x)];
    if (code == kOutsideCode) {
        return Region::Outside;
    }
    return code == kPublicCode ? Region::Public : Region::Enclosure;
}

EnclosureId Enclosures::At(TileCoord t) const {
    if (t.x < 0 || t.z < 0 || t.x >= width_ || t.z >= depth_) {
        return {};
    }
    const std::int64_t code = codes_[static_cast<std::size_t>(t.z) * static_cast<std::size_t>(width_) +
                                     static_cast<std::size_t>(t.x)];
    return code > 0 ? EnclosureId{static_cast<std::uint32_t>(code)} : EnclosureId{};
}

const Enclosure* Enclosures::Find(EnclosureId id) const {
    const auto it = std::lower_bound(enclosures_.begin(), enclosures_.end(), id,
                                     [](const Enclosure& e, EnclosureId wanted) { return e.id < wanted; });
    return it != enclosures_.end() && it->id == id ? &*it : nullptr;
}

bool Enclosures::Rename(EnclosureId id, std::string name) {
    if (name.empty()) {
        return false;
    }
    for (Enclosure& enclosure : enclosures_) {
        if (enclosure.id == id) {
            enclosure.name = Truncated(std::move(name));
            return true;
        }
    }
    return false;
}

bool Enclosures::operator==(const Enclosures& other) const {
    if (nextId_ != other.nextId_ || enclosures_.size() != other.enclosures_.size()) {
        return false;
    }
    for (std::size_t i = 0; i < enclosures_.size(); ++i) {
        const Enclosure& a = enclosures_[i];
        const Enclosure& b = other.enclosures_[i];
        if (a.id != b.id || a.name != b.name || a.anchor != b.anchor) {
            return false;
        }
    }
    return true;
}

}  // namespace zoo
