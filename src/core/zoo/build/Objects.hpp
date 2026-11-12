// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <map>
#include <utility>
#include <vector>

#include "zoo/base/Ids.hpp"
#include "zoo/base/Math.hpp"
#include "zoo/base/Money.hpp"
#include "zoo/build/Catalog.hpp"

namespace zoo {

/// A placed building. Its footprint tiles carry its id in World::Tile::building.
struct Building {
    BuildingId id;
    int def = -1;             ///< index into Catalog::buildings
    TileCoord origin;         ///< north-west tile of the (rotated) footprint
    Dir facing = Dir::South;  ///< the side its door is on
    float condition = 1.0f;   ///< 1 = new; worn buildings cost appeal until repaired
    Money itemPrice = 0;      ///< current selling price (stalls and shops)
    bool open = true;         ///< closed buildings serve nobody
    int servedToday = 0;      ///< visitors served since the day began (statistics)
    int servedTotal = 0;

    bool operator==(const Building&) const = default;
};

/// Size of a footprint along x and z for a facing: (width, depth) when the door is
/// on the south or north side, (depth, width) otherwise.
constexpr TileCoord FootprintSize(const BuildingDef& def, Dir facing) {
    return (facing == Dir::South || facing == Dir::North) ? TileCoord{def.width, def.depth}
                                                          : TileCoord{def.depth, def.width};
}

/// The footprint tile that carries the door. The layouts for the four facings are
/// rotations of the south-facing one about the vertical axis.
constexpr TileCoord DoorTile(const BuildingDef& def, TileCoord origin, Dir facing) {
    switch (facing) {
        case Dir::South:
            return {origin.x + def.doorOffset, origin.z + def.depth - 1};
        case Dir::West:
            return {origin.x, origin.z + def.doorOffset};
        case Dir::North:
            return {origin.x + def.width - 1 - def.doorOffset, origin.z};
        case Dir::East:
            return {origin.x + def.depth - 1, origin.z + def.width - 1 - def.doorOffset};
    }
    return origin;
}

/// The tile outside the door: where visitors queue and animals enter.
constexpr TileCoord FrontTile(const BuildingDef& def, TileCoord origin, Dir facing) {
    return DoorTile(def, origin, facing) + Step(facing);
}

/// A freely placed object: a tree, a rock, a feeder, a bench...
struct Prop {
    PropId id;
    int def = -1;  ///< index into Catalog::props
    Vec2 position;
    float heading = 0.0f;
    float scale = 1.0f;  ///< visual variation of plants and rocks, 0.7..1.3
    /// Feeder: kg of food in it; WaterTrough: litres; Bin: litter items in it.
    float amount = 0.0f;
    /// Feeder: index into Catalog::foods of what it holds, -1 when empty.
    int food = -1;
    /// Feeder: game day (GameTime::Days) on which the food was put in (it spoils).
    float filledDay = 0.0f;

    bool operator==(const Prop&) const = default;
};

/// The buildings and props of a zoo. Ids are handed out in increasing order and
/// never reused; both lists are kept sorted by id, so iteration order is
/// deterministic and lookups are binary searches.
class ZooObjects {
public:
    /// Stores the building under a fresh id (the `id` field of the argument is
    /// ignored) and returns it. Marking the footprint in the World is the
    /// Builder's job.
    BuildingId AddBuilding(Building building);
    bool RemoveBuilding(BuildingId id);
    Building* FindBuilding(BuildingId id);
    const Building* FindBuilding(BuildingId id) const;
    const std::vector<Building>& Buildings() const { return buildings_; }

    PropId AddProp(Prop prop);
    bool RemoveProp(PropId id);
    Prop* FindProp(PropId id);
    const Prop* FindProp(PropId id) const;
    const std::vector<Prop>& Props() const { return props_; }

    /// Ids of the props whose centre lies within `radius` of `center`, ascending.
    /// Backed by a grid of buckets rebuilt lazily after edits: cheap enough to call
    /// for every animal and visitor each second with thousands of trees placed.
    void PropsNear(Vec2 center, float radius, std::vector<PropId>& out) const;
    /// The props whose centre lies inside the tile rectangle (inclusive), ascending.
    void PropsInTiles(TileCoord min, TileCoord max, std::vector<PropId>& out) const;

    /// Changes whenever a building or prop is added, removed or moved (not when
    /// amounts, prices or conditions change).
    std::uint32_t Revision() const { return revision_; }

    /// Compares the stored objects and the id counters (not caches).
    bool operator==(const ZooObjects& other) const;

private:
    friend struct ObjectsAccess;  // serialisation

    void RebuildBuckets() const;

    std::vector<Building> buildings_;
    std::vector<Prop> props_;
    std::uint32_t nextBuilding_ = 1;
    std::uint32_t nextProp_ = 1;
    std::uint32_t revision_ = 1;

    static constexpr float kBucketSize = 16.0f;  ///< metres
    mutable std::map<std::pair<int, int>, std::vector<std::uint32_t>> buckets_;
    mutable std::uint32_t bucketsRevision_ = 0;
};

}  // namespace zoo
