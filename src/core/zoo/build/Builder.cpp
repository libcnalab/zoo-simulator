// SPDX-License-Identifier: MIT
#include "zoo/build/Builder.hpp"
#include "zoo/build/ObjectsJson.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <tuple>
#include <utility>
#include <vector>

namespace zoo {
namespace {

bool IsEdgeValid(const World& world, EdgeRef edge) {
    return world.EdgeInBounds(edge);
}

std::pair<TileCoord, TileCoord> TilesBeside(EdgeRef edge) {
    if (edge.horizontal) {
        return {{edge.x, edge.z - 1}, {edge.x, edge.z}};
    }
    return {{edge.x - 1, edge.z}, {edge.x, edge.z}};
}

bool CircleOverlapsTile(Vec2 center, float radius, TileCoord tile) {
    const float left = static_cast<float>(tile.x) * kTileSize;
    const float top = static_cast<float>(tile.z) * kTileSize;
    const float closestX = std::clamp(center.x, left, left + kTileSize);
    const float closestZ = std::clamp(center.z, top, top + kTileSize);
    return DistanceSquared(center, {closestX, closestZ}) < radius * radius;
}

bool CircleOverlapsRect(Vec2 center, float radius, TileCoord min, TileCoord max) {
    const float left = static_cast<float>(min.x) * kTileSize;
    const float top = static_cast<float>(min.z) * kTileSize;
    const float right = static_cast<float>(max.x + 1) * kTileSize;
    const float bottom = static_cast<float>(max.z + 1) * kTileSize;
    const float closestX = std::clamp(center.x, left, right);
    const float closestZ = std::clamp(center.z, top, bottom);
    return DistanceSquared(center, {closestX, closestZ}) < radius * radius;
}

Money RoundedMoney(double amount) {
    return static_cast<Money>(std::llround(amount));
}

Money RoundedUpMoney(double amount) {
    return static_cast<Money>(std::ceil(amount - 1.0e-9));
}

Money RefundMoney(double amount) {
    return static_cast<Money>(std::floor(amount + 1.0e-9));
}

void RecordSkipped(BuildPlan& plan, BuildError error) {
    ++plan.skipped;
    if (plan.skippedBecause == BuildError::None) {
        plan.skippedBecause = error;
    }
}

bool FinishPlan(BuildPlan& plan, Finance* finance) {
    if (plan.count == 0) {
        plan.error = plan.skippedBecause == BuildError::None ? BuildError::NothingToDo : plan.skippedBecause;
        return false;
    }
    if (plan.cost > 0 && finance != nullptr && !finance->CanAfford(plan.cost)) {
        plan.error = BuildError::NoMoney;
        return false;
    }
    return true;
}

bool Book(Finance* finance, Money cost, Account expense) {
    if (finance == nullptr) {
        return true;
    }
    if (cost > 0) {
        return finance->TrySpend(expense, cost);
    }
    if (cost < 0) {
        finance->Earn(Account::Refunds, -cost);
    }
    return true;
}

bool IsLandscaping(PropRole role) {
    return role == PropRole::Tree || role == PropRole::Bush || role == PropRole::Rock;
}

float RectMeanHeight(const World& world, TileCoord min, TileCoord max) {
    float total = 0.0f;
    int count = 0;
    for (int z = min.z; z <= max.z + 1; ++z) {
        for (int x = min.x; x <= max.x + 1; ++x) {
            total += world.VertexHeight(x, z);
            ++count;
        }
    }
    return total / static_cast<float>(count);
}

bool StandOffScenery(const PropDef& def) {
    return def.role == PropRole::Tree || def.role == PropRole::Bush || def.role == PropRole::Rock ||
           def.role == PropRole::Decoration;
}

BuildError PlacementError(Placement placement, Region region) {
    if (placement == Placement::Enclosure && region != Region::Enclosure) {
        return BuildError::NeedsEnclosure;
    }
    if (placement == Placement::Public && region != Region::Public) {
        return BuildError::NeedsPublicGround;
    }
    return BuildError::None;
}

}  // namespace

std::string_view Describe(BuildError error) {
    switch (error) {
        case BuildError::None:
            return {};
        case BuildError::OutOfBounds:
            return "That is off the map.";
        case BuildError::NotProperty:
            return "Build inside the zoo boundary.";
        case BuildError::Occupied:
            return "A building is in the way.";
        case BuildError::Blocked:
            return "An object is in the way.";
        case BuildError::Water:
            return "Water is in the way.";
        case BuildError::PathInTheWay:
            return "A path or object is in the way.";
        case BuildError::TooSteep:
            return "The ground is too uneven.";
        case BuildError::NeedsEnclosure:
            return "This belongs inside an animal enclosure.";
        case BuildError::NeedsPublicGround:
            return "This belongs on visitor ground.";
        case BuildError::FenceInTheWay:
            return "A fence crosses this spot.";
        case BuildError::FenceExists:
            return "That fence is already there.";
        case BuildError::NoFence:
            return "There is no fence there.";
        case BuildError::Perimeter:
            return "The boundary wall cannot be changed.";
        case BuildError::ThroughBuilding:
            return "A building is in the way.";
        case BuildError::UnknownItem:
            return "That item is not available.";
        case BuildError::NothingToDo:
            return "There is nothing to change.";
        case BuildError::NoMoney:
            return "There is not enough money.";
    }
    return "That cannot be built.";
}

std::vector<EdgeRef> EdgesAlong(TileCoord a, TileCoord b) {
    std::vector<EdgeRef> result;
    int x = a.x;
    const int xStep = b.x >= a.x ? 1 : -1;
    while (x != b.x) {
        const int nextX = x + xStep;
        result.push_back({std::min(x, nextX), a.z, true});
        x = nextX;
    }
    int z = a.z;
    const int zStep = b.z >= a.z ? 1 : -1;
    while (z != b.z) {
        const int nextZ = z + zStep;
        result.push_back({b.x, std::min(z, nextZ), false});
        z = nextZ;
    }
    return result;
}

std::vector<EdgeRef> EdgesAround(TileCoord a, TileCoord b) {
    const int minX = std::min(a.x, b.x);
    const int maxX = std::max(a.x, b.x);
    const int minZ = std::min(a.z, b.z);
    const int maxZ = std::max(a.z, b.z);
    std::vector<EdgeRef> result;
    for (int x = minX; x <= maxX; ++x) {
        result.push_back({x, minZ, true});
    }
    for (int z = minZ; z <= maxZ; ++z) {
        result.push_back({maxX + 1, z, false});
    }
    for (int x = maxX; x >= minX; --x) {
        result.push_back({x, maxZ + 1, true});
    }
    for (int z = maxZ; z >= minZ; --z) {
        result.push_back({minX, z, false});
    }
    return result;
}

std::vector<TileCoord> TilesInLine(TileCoord a, TileCoord b) {
    std::vector<TileCoord> result;
    int x = a.x;
    const int xStep = b.x >= a.x ? 1 : -1;
    while (true) {
        result.push_back({x, a.z});
        if (x == b.x) {
            break;
        }
        x += xStep;
    }
    int z = a.z;
    const int zStep = b.z >= a.z ? 1 : -1;
    while (z != b.z) {
        z += zStep;
        result.push_back({b.x, z});
    }
    return result;
}

std::vector<TileCoord> TilesInRect(TileCoord a, TileCoord b) {
    const int minX = std::min(a.x, b.x);
    const int maxX = std::max(a.x, b.x);
    const int minZ = std::min(a.z, b.z);
    const int maxZ = std::max(a.z, b.z);
    std::vector<TileCoord> result;
    for (int z = minZ; z <= maxZ; ++z) {
        for (int x = minX; x <= maxX; ++x) {
            result.push_back({x, z});
        }
    }
    return result;
}

Builder::Builder(World& world, ZooObjects& objects, Enclosures& enclosures, const Catalog& catalog,
                 Finance* finance)
    : world_(world), objects_(objects), enclosures_(enclosures), catalog_(catalog), finance_(finance) {
    for (const PropDef& prop : catalog_.props) {
        largestPropRadius_ = std::max(largestPropRadius_, prop.radiusM);
    }
}

BuildError Builder::CheckFence(EdgeRef edge, int kind, bool gate) const {
    if (!IsEdgeValid(world_, edge)) {
        return BuildError::OutOfBounds;
    }
    const auto [first, second] = TilesBeside(edge);
    if (!world_.InBounds(first) || !world_.InBounds(second)) {
        return BuildError::OutOfBounds;
    }
    if (!world_.IsProperty(first) || !world_.IsProperty(second)) {
        return BuildError::NotProperty;
    }
    const Fence existing = world_.FenceAt(edge);
    if (existing.kind == kPerimeterFence) {
        return BuildError::Perimeter;
    }
    if (world_.At(first).building && world_.At(first).building == world_.At(second).building) {
        return BuildError::ThroughBuilding;
    }
    if (catalog_.FenceByKind(kind) == nullptr) {
        return BuildError::UnknownItem;
    }
    if (existing.kind == static_cast<std::uint8_t>(kind) && existing.gate == gate) {
        return BuildError::FenceExists;
    }
    return BuildError::None;
}

BuildPlan Builder::PlanFence(std::span<const EdgeRef> edges, int kind, bool gate) const {
    return const_cast<Builder*>(this)->DoFence(edges, kind, gate, false);
}
BuildPlan Builder::PlaceFence(std::span<const EdgeRef> edges, int kind, bool gate) {
    return DoFence(edges, kind, gate, true);
}

void Builder::RefreshFigures() {
    if (held_) {
        stale_ = true;
        return;
    }
    enclosures_.Rebuild(world_, objects_, catalog_);
}

void Builder::Release() {
    held_ = false;
    if (std::exchange(stale_, false)) {
        enclosures_.Rebuild(world_, objects_, catalog_);
    }
}

BuildPlan Builder::DoFence(std::span<const EdgeRef> edges, int kind, bool gate, bool apply) {
    BuildPlan plan;
    const FenceDef* def = catalog_.FenceByKind(kind);
    std::vector<EdgeRef> accepted;
    std::vector<Fence> replacements;
    for (const EdgeRef edge : edges) {
        if (std::find(accepted.begin(), accepted.end(), edge) != accepted.end()) {
            continue;
        }
        const BuildError error = CheckFence(edge, kind, gate);
        if (error != BuildError::None) {
            RecordSkipped(plan, error);
            continue;
        }
        const Fence existing = world_.FenceAt(edge);
        Money delta = def->cost + (gate ? def->gateCost : 0);
        if (existing.Exists()) {
            const FenceDef* oldDef = catalog_.FenceByKind(existing.kind);
            if (existing.kind == static_cast<std::uint8_t>(kind)) {
                delta = gate ? def->gateCost : 0;
            } else if (oldDef != nullptr) {
                delta -=
                    RefundMoney(static_cast<double>(Builder::kFenceRefund) *
                                static_cast<double>(oldDef->cost + (existing.gate ? oldDef->gateCost : 0)) *
                                static_cast<double>(existing.condition));
            }
        }
        plan.cost += delta;
        ++plan.count;
        accepted.push_back(edge);
        replacements.push_back(existing);
    }
    if (!FinishPlan(plan, finance_)) {
        return plan;
    }
    if (apply) {
        if (!Book(finance_, plan.cost, Account::Construction)) {
            plan.error = BuildError::NoMoney;
            return plan;
        }
        for (std::size_t i = 0; i < accepted.size(); ++i) {
            const Fence previous = replacements[i];
            const bool toggle = previous.kind == static_cast<std::uint8_t>(kind);
            world_.SetFence(accepted[i],
                            {static_cast<std::uint8_t>(kind), gate, toggle ? previous.condition : 1.0f,
                             gate && def->id == "aviary_curtain"});
        }
        enclosures_.Rebuild(world_, objects_, catalog_);
    }
    return plan;
}

BuildPlan Builder::PlanRemoveFence(std::span<const EdgeRef> edges) const {
    return const_cast<Builder*>(this)->DoRemoveFence(edges, false);
}
BuildPlan Builder::RemoveFence(std::span<const EdgeRef> edges) {
    return DoRemoveFence(edges, true);
}

BuildPlan Builder::DoRemoveFence(std::span<const EdgeRef> edges, bool apply) {
    BuildPlan plan;
    std::vector<EdgeRef> accepted;
    for (const EdgeRef edge : edges) {
        if (std::find(accepted.begin(), accepted.end(), edge) != accepted.end()) {
            continue;
        }
        if (!world_.EdgeInBounds(edge)) {
            RecordSkipped(plan, BuildError::OutOfBounds);
            continue;
        }
        const Fence fence = world_.FenceAt(edge);
        if (!fence.Exists()) {
            RecordSkipped(plan, BuildError::NoFence);
            continue;
        }
        if (fence.kind == kPerimeterFence) {
            RecordSkipped(plan, BuildError::Perimeter);
            continue;
        }
        const FenceDef* def = catalog_.FenceByKind(fence.kind);
        if (def != nullptr) {
            plan.cost -= RefundMoney(static_cast<double>(kFenceRefund) *
                                     static_cast<double>(def->cost + (fence.gate ? def->gateCost : 0)) *
                                     static_cast<double>(fence.condition));
        }
        ++plan.count;
        accepted.push_back(edge);
    }
    if (!FinishPlan(plan, finance_)) {
        return plan;
    }
    if (apply) {
        Book(finance_, plan.cost, Account::Construction);
        for (const EdgeRef edge : accepted) {
            world_.SetFence(edge, {});
        }
        enclosures_.Rebuild(world_, objects_, catalog_);
    }
    return plan;
}

BuildError Builder::CheckPath(TileCoord tile, PathKind kind) const {
    if (!world_.InBounds(tile)) {
        return BuildError::OutOfBounds;
    }
    if (!world_.IsProperty(tile)) {
        return BuildError::NotProperty;
    }
    if (world_.At(tile).path == kind) {
        return BuildError::NothingToDo;
    }
    if (kind == PathKind::None) {
        return BuildError::None;
    }
    const Tile& data = world_.At(tile);
    if (data.standOff) {
        return BuildError::Blocked;
    }
    if (data.building) {
        return BuildError::Occupied;
    }
    if (data.waterLevel != kNoWater) {
        return BuildError::Water;
    }
    if (world_.TileRelief(tile) > kMaxPathRelief) {
        return BuildError::TooSteep;
    }
    std::vector<PropId> nearby;
    objects_.PropsNear(World::TileCenter(tile), largestPropRadius_ + 1.414214f, nearby);
    for (const PropId id : nearby) {
        const Prop* prop = objects_.FindProp(id);
        if (prop == nullptr || prop->def < 0 || prop->def >= static_cast<int>(catalog_.props.size())) {
            continue;
        }
        const PropDef& def = catalog_.props[static_cast<std::size_t>(prop->def)];
        if (def.blocksWalking && CircleOverlapsTile(prop->position, def.radiusM, tile)) {
            return BuildError::Blocked;
        }
    }
    return BuildError::None;
}

BuildPlan Builder::PlanPath(std::span<const TileCoord> tiles, PathKind kind) const {
    return const_cast<Builder*>(this)->DoPath(tiles, kind, false);
}
BuildPlan Builder::PlacePath(std::span<const TileCoord> tiles, PathKind kind) {
    return DoPath(tiles, kind, true);
}
BuildPlan Builder::DoPath(std::span<const TileCoord> tiles, PathKind kind, bool apply) {
    BuildPlan plan;
    std::vector<TileCoord> accepted;
    for (const TileCoord tile : tiles) {
        if (std::find(accepted.begin(), accepted.end(), tile) != accepted.end()) {
            continue;
        }
        const BuildError error = CheckPath(tile, kind);
        if (error != BuildError::None) {
            RecordSkipped(plan, error);
            continue;
        }
        if (kind != PathKind::None) {
            plan.cost += catalog_.Path(kind).cost;
        }
        ++plan.count;
        accepted.push_back(tile);
    }
    if (!FinishPlan(plan, finance_)) {
        return plan;
    }
    if (apply) {
        Book(finance_, plan.cost, Account::Construction);
        for (const TileCoord tile : accepted) {
            world_.SetPath(tile, kind);
        }
        enclosures_.Rebuild(world_, objects_, catalog_);
    }
    return plan;
}

BuildError Builder::CheckStandOff(TileCoord tile, bool marked) const {
    if (!world_.InBounds(tile)) {
        return BuildError::OutOfBounds;
    }
    if (!world_.IsProperty(tile)) {
        return BuildError::NotProperty;
    }
    const Tile& data = world_.At(tile);
    if (data.standOff == marked) {
        return BuildError::NothingToDo;
    }
    if (!marked) {
        return BuildError::None;
    }
    if (data.building) {
        return BuildError::Occupied;
    }
    if (data.path != PathKind::None) {
        return BuildError::PathInTheWay;
    }
    if (world_.HasWater(tile)) {
        return BuildError::Water;
    }
    if (tile == world_.EntranceTile() || enclosures_.RegionAt(tile) != Region::Public) {
        return BuildError::NeedsPublicGround;
    }
    std::vector<PropId> props;
    objects_.PropsInTiles(tile, tile, props);
    for (const PropId id : props) {
        const Prop* prop = objects_.FindProp(id);
        if (prop != nullptr && !StandOffScenery(catalog_.props[static_cast<std::size_t>(prop->def)])) {
            return BuildError::Blocked;
        }
    }
    return BuildError::None;
}

BuildPlan Builder::PlanStandOff(std::span<const TileCoord> tiles, bool marked) const {
    return const_cast<Builder*>(this)->DoStandOff(tiles, marked, false);
}

BuildPlan Builder::PlaceStandOff(std::span<const TileCoord> tiles, bool marked) {
    return DoStandOff(tiles, marked, true);
}

BuildPlan Builder::DoStandOff(std::span<const TileCoord> tiles, bool marked, bool apply) {
    BuildPlan plan;
    std::vector<TileCoord> accepted;
    for (const TileCoord tile : tiles) {
        if (std::find(accepted.begin(), accepted.end(), tile) != accepted.end()) {
            continue;
        }
        const BuildError error = CheckStandOff(tile, marked);
        if (error != BuildError::None) {
            RecordSkipped(plan, error);
            continue;
        }
        accepted.push_back(tile);
        ++plan.count;
        if (marked) {
            plan.cost += kStandOffPerTile;
        }
    }
    if (!FinishPlan(plan, finance_)) {
        return plan;
    }
    if (apply) {
        Book(finance_, plan.cost, Account::Landscaping);
        for (const TileCoord tile : accepted) {
            world_.SetStandOff(tile, marked);
        }
        enclosures_.Rebuild(world_, objects_, catalog_);
    }
    return plan;
}

BuildPlan Builder::PlanTerrain(std::span<const TileCoord> tiles, Terrain terrain) const {
    return const_cast<Builder*>(this)->DoTerrain(tiles, terrain, false);
}
BuildPlan Builder::PaintTerrain(std::span<const TileCoord> tiles, Terrain terrain) {
    return DoTerrain(tiles, terrain, true);
}
BuildPlan Builder::DoTerrain(std::span<const TileCoord> tiles, Terrain terrain, bool apply) {
    BuildPlan plan;
    std::vector<TileCoord> accepted;
    for (const TileCoord tile : tiles) {
        if (std::find(accepted.begin(), accepted.end(), tile) != accepted.end()) {
            continue;
        }
        BuildError error = BuildError::None;
        if (!world_.InBounds(tile)) {
            error = BuildError::OutOfBounds;
        } else if (!world_.IsProperty(tile)) {
            error = BuildError::NotProperty;
        } else if (world_.At(tile).terrain == terrain) {
            error = BuildError::NothingToDo;
        }
        if (error != BuildError::None) {
            RecordSkipped(plan, error);
            continue;
        }
        plan.cost += catalog_.TerrainInfo(terrain).cost;
        ++plan.count;
        accepted.push_back(tile);
    }
    if (!FinishPlan(plan, finance_)) {
        return plan;
    }
    if (apply) {
        Book(finance_, plan.cost, Account::Landscaping);
        for (const TileCoord tile : accepted) {
            world_.SetTerrain(tile, terrain);
        }
        RefreshFigures();
    }
    return plan;
}

BuildError Builder::CheckWater(TileCoord tile) const {
    if (!world_.InBounds(tile)) {
        return BuildError::OutOfBounds;
    }
    if (!world_.IsProperty(tile)) {
        return BuildError::NotProperty;
    }
    const Tile& data = world_.At(tile);
    if (data.waterLevel != kNoWater) {
        return BuildError::NothingToDo;
    }
    if (data.standOff) {
        return BuildError::Blocked;
    }
    if (data.building) {
        return BuildError::Occupied;
    }
    if (data.path != PathKind::None) {
        return BuildError::PathInTheWay;
    }
    std::vector<PropId> nearby;
    objects_.PropsNear(World::TileCenter(tile), largestPropRadius_ + 1.414214f, nearby);
    for (const PropId id : nearby) {
        const Prop* prop = objects_.FindProp(id);
        if (prop == nullptr || prop->def < 0 || prop->def >= static_cast<int>(catalog_.props.size())) {
            continue;
        }
        const PropDef& def = catalog_.props[static_cast<std::size_t>(prop->def)];
        if (World::TileAt(prop->position) == tile ||
            (def.blocksWalking && CircleOverlapsTile(prop->position, def.radiusM, tile))) {
            return BuildError::Blocked;
        }
    }
    return BuildError::None;
}

BuildPlan Builder::PlanWater(std::span<const TileCoord> tiles, float depth) const {
    return const_cast<Builder*>(this)->DoWater(tiles, depth, false);
}
BuildPlan Builder::PlaceWater(std::span<const TileCoord> tiles, float depth) {
    return DoWater(tiles, depth, true);
}
BuildPlan Builder::DoWater(std::span<const TileCoord> tiles, float depth, bool apply) {
    BuildPlan plan;
    if (!std::isfinite(depth)) {
        plan.error = BuildError::NothingToDo;
        return plan;
    }
    const float waterDepth = std::clamp(depth, kMinWaterDepth, kMaxWaterDepth);
    std::vector<TileCoord> accepted;
    for (const TileCoord tile : tiles) {
        if (std::find(accepted.begin(), accepted.end(), tile) != accepted.end()) {
            continue;
        }
        const BuildError error = CheckWater(tile);
        if (error != BuildError::None) {
            RecordSkipped(plan, error);
            continue;
        }
        plan.cost +=
            RoundedMoney(static_cast<double>(kWaterCostPerTile) * static_cast<double>(waterDepth) / 0.8);
        ++plan.count;
        accepted.push_back(tile);
    }
    if (!FinishPlan(plan, finance_)) {
        return plan;
    }
    float surface = std::numeric_limits<float>::infinity();
    for (const TileCoord tile : accepted) {
        for (int dz = -1; dz <= 1; ++dz) {
            for (int dx = -1; dx <= 1; ++dx) {
                if (std::abs(dx) + std::abs(dz) > 1) {
                    continue;
                }
                const TileCoord neighbour{tile.x + dx, tile.z + dz};
                if (world_.HasWater(neighbour)) {
                    surface = std::min(surface, world_.At(neighbour).waterLevel);
                }
            }
        }
    }
    if (!std::isfinite(surface)) {
        float lowest = std::numeric_limits<float>::infinity();
        for (const TileCoord tile : accepted) {
            for (int dz = 0; dz <= 1; ++dz) {
                for (int dx = 0; dx <= 1; ++dx) {
                    lowest = std::min(lowest, world_.VertexHeight(tile.x + dx, tile.z + dz));
                }
            }
        }
        surface = lowest - 0.05f;
    }
    if (apply) {
        Book(finance_, plan.cost, Account::Landscaping);
        for (const TileCoord tile : accepted) {
            world_.SetWater(tile, surface, waterDepth);
        }
        enclosures_.Rebuild(world_, objects_, catalog_);
    }
    return plan;
}

BuildPlan Builder::PlanRemoveWater(std::span<const TileCoord> tiles) const {
    return const_cast<Builder*>(this)->DoRemoveWater(tiles, false);
}
BuildPlan Builder::RemoveWater(std::span<const TileCoord> tiles) {
    return DoRemoveWater(tiles, true);
}
BuildPlan Builder::DoRemoveWater(std::span<const TileCoord> tiles, bool apply) {
    BuildPlan plan;
    std::vector<TileCoord> accepted;
    for (const TileCoord tile : tiles) {
        if (std::find(accepted.begin(), accepted.end(), tile) != accepted.end()) {
            continue;
        }
        BuildError error = BuildError::None;
        if (!world_.InBounds(tile)) {
            error = BuildError::OutOfBounds;
        } else if (!world_.IsProperty(tile)) {
            error = BuildError::NotProperty;
        } else if (!world_.HasWater(tile)) {
            error = BuildError::NothingToDo;
        }
        if (error != BuildError::None) {
            RecordSkipped(plan, error);
            continue;
        }
        plan.cost += kWaterRemovalPerTile;
        ++plan.count;
        accepted.push_back(tile);
    }
    if (!FinishPlan(plan, finance_)) {
        return plan;
    }
    if (apply) {
        Book(finance_, plan.cost, Account::Landscaping);
        for (const TileCoord tile : accepted) {
            world_.ClearWater(tile);
        }
        enclosures_.Rebuild(world_, objects_, catalog_);
    }
    return plan;
}

BuildPlan Builder::PlanTerraform(Brush brush, Vec2 center, float radius, float amount) const {
    return const_cast<Builder*>(this)->DoTerraform(brush, center, radius, amount, false);
}
BuildPlan Builder::Terraform(Brush brush, Vec2 center, float radius, float amount) {
    return DoTerraform(brush, center, radius, amount, true);
}
BuildPlan Builder::DoTerraform(Brush brush, Vec2 center, float radius, float amount, bool apply) {
    BuildPlan plan;
    if (!std::isfinite(center.x) || !std::isfinite(center.z) || center.x < 0.0f || center.z < 0.0f ||
        center.x >= world_.SizeX() || center.z >= world_.SizeZ()) {
        plan.error = BuildError::OutOfBounds;
        return plan;
    }
    if (!std::isfinite(radius) || !std::isfinite(amount)) {
        plan.error = BuildError::NothingToDo;
        return plan;
    }
    const TileCoord centerTile = World::TileAt(center);
    if (!world_.InBounds(centerTile)) {
        plan.error = BuildError::OutOfBounds;
        return plan;
    }
    if (!world_.IsProperty(centerTile)) {
        plan.error = BuildError::NotProperty;
        return plan;
    }
    const float strokeAmount = brush == Brush::Raise ? amount : Clamp01(amount);
    if (strokeAmount == 0.0f) {
        plan.error = BuildError::NothingToDo;
        return plan;
    }
    const float brushRadius = std::clamp(radius, kMinBrushRadius, kMaxBrushRadius);
    const double area = static_cast<double>(kPi) * static_cast<double>(brushRadius) * brushRadius;
    double rawCost = 0.0;
    if (brush == Brush::Raise) {
        rawCost =
            static_cast<double>(kTerraformPerM3) * area * std::abs(static_cast<double>(strokeAmount)) / 3.0;
    } else {
        rawCost = static_cast<double>(kLevelPerM2) * area * static_cast<double>(strokeAmount);
    }
    plan.cost = RoundedUpMoney(rawCost);
    plan.count = 1;
    if (!FinishPlan(plan, finance_)) {
        return plan;
    }
    if (apply) {
        Book(finance_, plan.cost, Account::Landscaping);
        if (brush == Brush::Raise) {
            world_.RaiseBrush(center, brushRadius, strokeAmount);
        } else if (brush == Brush::Flatten) {
            world_.FlattenBrush(center, brushRadius, world_.HeightAt(center), strokeAmount);
        } else {
            world_.SmoothBrush(center, brushRadius, strokeAmount);
        }
        const int minX =
            std::max(0, static_cast<int>(std::floor(center.x / kTileSize - brushRadius / kTileSize)));
        const int maxX = std::min(
            world_.Width() - 1, static_cast<int>(std::ceil(center.x / kTileSize + brushRadius / kTileSize)));
        const int minZ =
            std::max(0, static_cast<int>(std::floor(center.z / kTileSize - brushRadius / kTileSize)));
        const int maxZ = std::min(
            world_.Depth() - 1, static_cast<int>(std::ceil(center.z / kTileSize + brushRadius / kTileSize)));
        for (int z = minZ; z <= maxZ; ++z) {
            for (int x = minX; x <= maxX; ++x) {
                const TileCoord tile{x, z};
                if (!world_.HasWater(tile) || !CircleOverlapsTile(center, brushRadius, tile)) {
                    continue;
                }
                const float level = world_.At(tile).waterLevel;
                bool drained = true;
                for (int dz = 0; dz <= 1; ++dz) {
                    for (int dx = 0; dx <= 1; ++dx) {
                        if (world_.VertexHeight(x + dx, z + dz) < level) {
                            drained = false;
                        }
                    }
                }
                if (drained) {
                    world_.ClearWater(tile);
                }
            }
        }
        RefreshFigures();
    }
    return plan;
}

BuildError Builder::CheckBuilding(int defIndex, TileCoord origin, Dir facing) const {
    if (defIndex < 0 || defIndex >= static_cast<int>(catalog_.buildings.size())) {
        return BuildError::UnknownItem;
    }
    const BuildingDef& def = catalog_.buildings[static_cast<std::size_t>(defIndex)];
    const TileCoord size = FootprintSize(def, facing);
    const TileCoord max{origin.x + size.x - 1, origin.z + size.z - 1};
    const TileCoord front = FrontTile(def, origin, facing);
    for (const TileCoord tile : {origin, max, front}) {
        if (!world_.InBounds(tile)) {
            return BuildError::OutOfBounds;
        }
    }
    for (int z = origin.z; z <= max.z; ++z) {
        for (int x = origin.x; x <= max.x; ++x) {
            if (!world_.IsProperty({x, z})) {
                return BuildError::NotProperty;
            }
        }
    }
    if (!world_.IsProperty(front)) {
        return BuildError::NotProperty;
    }
    std::vector<TileCoord> footprint = TilesInRect(origin, max);
    footprint.push_back(front);
    for (const TileCoord tile : footprint) {
        if (world_.At(tile).standOff) {
            return BuildError::Blocked;
        }
        if (world_.At(tile).building) {
            return BuildError::Occupied;
        }
    }
    for (const TileCoord tile : footprint) {
        if (world_.HasWater(tile)) {
            return BuildError::Water;
        }
    }
    std::vector<PropId> propIds;
    objects_.PropsInTiles(origin, max, propIds);
    const Vec2 center{(static_cast<float>(origin.x + max.x + 1) * kTileSize) / 2.0f,
                      (static_cast<float>(origin.z + max.z + 1) * kTileSize) / 2.0f};
    const float halfDiagonal = std::sqrt(std::pow(static_cast<float>(size.x) * kTileSize / 2.0f, 2.0f) +
                                         std::pow(static_cast<float>(size.z) * kTileSize / 2.0f, 2.0f));
    std::vector<PropId> nearIds;
    objects_.PropsNear(center, halfDiagonal + largestPropRadius_, nearIds);
    for (const PropId id : nearIds) {
        if (std::find(propIds.begin(), propIds.end(), id) != propIds.end()) {
            return BuildError::Blocked;
        }
        const Prop* prop = objects_.FindProp(id);
        if (prop == nullptr || prop->def < 0 || prop->def >= static_cast<int>(catalog_.props.size())) {
            continue;
        }
        const PropDef& propDef = catalog_.props[static_cast<std::size_t>(prop->def)];
        if (propDef.blocksWalking && CircleOverlapsRect(prop->position, propDef.radiusM, origin, max)) {
            return BuildError::Blocked;
        }
    }
    for (int z = origin.z; z <= max.z; ++z) {
        for (int x = origin.x; x <= max.x; ++x) {
            const TileCoord tile{x, z};
            for (const Dir side : {Dir::East, Dir::South}) {
                const TileCoord other = tile + Step(side);
                if (other.x <= max.x && other.z <= max.z && world_.FenceOn(tile, side).Exists()) {
                    return BuildError::FenceInTheWay;
                }
            }
        }
    }
    if (world_.FenceOn(DoorTile(def, origin, facing), facing).Exists()) {
        return BuildError::FenceInTheWay;
    }
    float minimum = std::numeric_limits<float>::infinity();
    float maximum = -std::numeric_limits<float>::infinity();
    for (int z = origin.z; z <= max.z + 1; ++z) {
        for (int x = origin.x; x <= max.x + 1; ++x) {
            const float height = world_.VertexHeight(x, z);
            minimum = std::min(minimum, height);
            maximum = std::max(maximum, height);
        }
    }
    if (maximum - minimum > kMaxBuildingRelief) {
        return BuildError::TooSteep;
    }
    return PlacementError(def.placement, enclosures_.RegionAt(front));
}

BuildPlan Builder::PlanBuilding(int def, TileCoord origin, Dir facing) const {
    return const_cast<Builder*>(this)->DoBuilding(def, origin, facing, false);
}
BuildPlan Builder::PlaceBuilding(int def, TileCoord origin, Dir facing) {
    return DoBuilding(def, origin, facing, true);
}
BuildPlan Builder::DoBuilding(int defIndex, TileCoord origin, Dir facing, bool apply) {
    BuildPlan plan;
    const BuildError error = CheckBuilding(defIndex, origin, facing);
    if (error != BuildError::None) {
        plan.error = error;
        return plan;
    }
    const BuildingDef& def = catalog_.buildings[static_cast<std::size_t>(defIndex)];
    plan.cost = def.cost;
    plan.count = 1;
    plan.building = BuildingId{ObjectsAccess::NextBuilding(objects_)};
    if (!FinishPlan(plan, finance_)) {
        return plan;
    }
    if (apply) {
        Book(finance_, plan.cost, Account::Construction);
        const TileCoord size = FootprintSize(def, facing);
        const TileCoord max{origin.x + size.x - 1, origin.z + size.z - 1};
        const float mean = RectMeanHeight(world_, origin, max);
        world_.FlattenTiles(origin, max, mean);
        for (const TileCoord tile : TilesInRect(origin, max)) {
            world_.SetPath(tile, PathKind::None);
        }
        Building building;
        building.def = defIndex;
        building.origin = origin;
        building.facing = facing;
        building.condition = 1.0f;
        building.itemPrice = def.itemPrice;
        plan.building = objects_.AddBuilding(building);
        for (const TileCoord tile : TilesInRect(origin, max)) {
            world_.SetBuilding(tile, plan.building);
        }
        enclosures_.Rebuild(world_, objects_, catalog_);
    }
    return plan;
}

BuildPlan Builder::PlanDemolish(BuildingId id) const {
    return const_cast<Builder*>(this)->DoDemolish(id, false);
}
BuildPlan Builder::Demolish(BuildingId id) {
    return DoDemolish(id, true);
}
BuildPlan Builder::DoDemolish(BuildingId id, bool apply) {
    BuildPlan plan;
    const Building* building = objects_.FindBuilding(id);
    if (building == nullptr || building->def < 0 ||
        building->def >= static_cast<int>(catalog_.buildings.size())) {
        plan.error = BuildError::UnknownItem;
        return plan;
    }
    const Building copy = *building;
    const BuildingDef& def = catalog_.buildings[static_cast<std::size_t>(copy.def)];
    plan.cost = -RefundMoney(static_cast<double>(kBuildingRefund) * static_cast<double>(def.cost) *
                             static_cast<double>(copy.condition));
    plan.count = 1;
    if (!FinishPlan(plan, finance_)) {
        return plan;
    }
    if (apply) {
        Book(finance_, plan.cost, Account::Construction);
        const TileCoord size = FootprintSize(def, copy.facing);
        for (const TileCoord tile :
             TilesInRect(copy.origin, {copy.origin.x + size.x - 1, copy.origin.z + size.z - 1})) {
            world_.SetBuilding(tile, {});
        }
        objects_.RemoveBuilding(id);
        enclosures_.Rebuild(world_, objects_, catalog_);
    }
    return plan;
}

BuildError Builder::CheckProp(int defIndex, Vec2 position) const {
    if (defIndex < 0 || defIndex >= static_cast<int>(catalog_.props.size())) {
        return BuildError::UnknownItem;
    }
    const PropDef& def = catalog_.props[static_cast<std::size_t>(defIndex)];
    if (!std::isfinite(position.x) || !std::isfinite(position.z) || position.x < 0.0f || position.z < 0.0f ||
        position.x >= world_.SizeX() || position.z >= world_.SizeZ()) {
        return BuildError::OutOfBounds;
    }
    const TileCoord tile = World::TileAt(position);
    if (!world_.InBounds(tile)) {
        return BuildError::OutOfBounds;
    }
    if (!world_.IsProperty(tile)) {
        return BuildError::NotProperty;
    }
    if (world_.At(tile).standOff && !StandOffScenery(def)) {
        return BuildError::Blocked;
    }
    if (world_.At(tile).building) {
        return BuildError::Occupied;
    }
    if (world_.HasWater(tile)) {
        return BuildError::Water;
    }
    if (def.blocksWalking) {
        const int minX = std::max(0, static_cast<int>(std::floor((position.x - def.radiusM) / kTileSize)));
        const int maxX = std::min(world_.Width() - 1,
                                  static_cast<int>(std::floor((position.x + def.radiusM) / kTileSize)));
        const int minZ = std::max(0, static_cast<int>(std::floor((position.z - def.radiusM) / kTileSize)));
        const int maxZ = std::min(world_.Depth() - 1,
                                  static_cast<int>(std::floor((position.z + def.radiusM) / kTileSize)));
        for (int z = minZ; z <= maxZ; ++z) {
            for (int x = minX; x <= maxX; ++x) {
                const TileCoord pathTile{x, z};
                if (world_.At(pathTile).path != PathKind::None &&
                    CircleOverlapsTile(position, def.radiusM, pathTile)) {
                    return BuildError::PathInTheWay;
                }
            }
        }
    }
    std::vector<PropId> nearby;
    objects_.PropsNear(position, def.radiusM + largestPropRadius_ + 0.3f, nearby);
    for (const PropId id : nearby) {
        const Prop* prop = objects_.FindProp(id);
        if (prop == nullptr || prop->def < 0 || prop->def >= static_cast<int>(catalog_.props.size())) {
            continue;
        }
        const PropDef& other = catalog_.props[static_cast<std::size_t>(prop->def)];
        const float minimum = def.blocksWalking && other.blocksWalking ? def.radiusM + other.radiusM : 0.3f;
        if (DistanceSquared(position, prop->position) < minimum * minimum) {
            return BuildError::Blocked;
        }
    }
    if (world_.At(tile).standOff) {
        return BuildError::None;  // Decorative scenery is also allowed off public ground.
    }
    return PlacementError(def.placement, enclosures_.RegionAt(tile));
}

BuildPlan Builder::PlanProp(int def, Vec2 position) const {
    return const_cast<Builder*>(this)->DoProp(def, position, 0.0f, 1.0f, false);
}
BuildPlan Builder::PlaceProp(int def, Vec2 position, float heading, float scale) {
    return DoProp(def, position, heading, scale, true);
}
BuildPlan Builder::DoProp(int defIndex, Vec2 position, float heading, float scale, bool apply) {
    BuildPlan plan;
    const BuildError error = CheckProp(defIndex, position);
    if (error != BuildError::None) {
        plan.error = error;
        return plan;
    }
    const PropDef& def = catalog_.props[static_cast<std::size_t>(defIndex)];
    plan.cost = def.cost;
    plan.count = 1;
    plan.prop = PropId{ObjectsAccess::NextProp(objects_)};
    if (!FinishPlan(plan, finance_)) {
        return plan;
    }
    if (apply) {
        const Account account = IsLandscaping(def.role) ? Account::Landscaping : Account::Construction;
        Book(finance_, plan.cost, account);
        Prop prop;
        prop.def = defIndex;
        prop.position = position;
        prop.heading = WrapAngle(heading);
        prop.scale = std::clamp(scale, kMinPropScale, kMaxPropScale);
        plan.prop = objects_.AddProp(prop);
        RefreshFigures();
    }
    return plan;
}

BuildPlan Builder::PlanRemoveProp(PropId id) const {
    return const_cast<Builder*>(this)->DoRemoveProp(id, false);
}
BuildPlan Builder::RemoveProp(PropId id) {
    return DoRemoveProp(id, true);
}
BuildPlan Builder::DoRemoveProp(PropId id, bool apply) {
    BuildPlan plan;
    const Prop* prop = objects_.FindProp(id);
    if (prop == nullptr || prop->def < 0 || prop->def >= static_cast<int>(catalog_.props.size())) {
        plan.error = BuildError::UnknownItem;
        return plan;
    }
    const Prop copy = *prop;
    const PropDef& def = catalog_.props[static_cast<std::size_t>(copy.def)];
    plan.cost = -RefundMoney(static_cast<double>(kPropRefund) * static_cast<double>(def.cost));
    plan.count = 1;
    if (!FinishPlan(plan, finance_)) {
        return plan;
    }
    if (apply) {
        Book(finance_, plan.cost, Account::Construction);
        objects_.RemoveProp(id);
        enclosures_.Rebuild(world_, objects_, catalog_);
    }
    return plan;
}

PropId Builder::PropAt(Vec2 point, float tolerance) const {
    const float search = std::max(0.0f, largestPropRadius_ + tolerance);
    std::vector<PropId> nearby;
    objects_.PropsNear(point, search, nearby);
    PropId best;
    float bestDistance = std::numeric_limits<float>::infinity();
    for (const PropId id : nearby) {
        const Prop* prop = objects_.FindProp(id);
        if (prop == nullptr || prop->def < 0 || prop->def >= static_cast<int>(catalog_.props.size())) {
            continue;
        }
        const float distance = DistanceSquared(point, prop->position);
        const float reach = catalog_.props[static_cast<std::size_t>(prop->def)].radiusM + tolerance;
        if (reach < 0.0f || distance > reach * reach) {
            continue;
        }
        if (distance < bestDistance || (distance == bestDistance && (!best || id < best))) {
            best = id;
            bestDistance = distance;
        }
    }
    return best;
}

}  // namespace zoo
