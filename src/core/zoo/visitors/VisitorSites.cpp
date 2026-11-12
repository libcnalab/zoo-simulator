// SPDX-License-Identifier: MIT
// What the map offers visitors: where the animals can be watched from, the stalls
// and their queues, the benches, bins and boards. Derived from the world.

#include <algorithm>
#include <cmath>

#include "zoo/visitors/VisitorInternal.hpp"

namespace zoo::visitors_detail {

namespace {

using namespace visitor_tuning;

bool ServesVisitors(BuildingRole role) {
    return role == BuildingRole::FoodStall || role == BuildingRole::DrinkStall ||
           role == BuildingRole::Restroom || role == BuildingRole::GiftShop ||
           role == BuildingRole::EducationCenter;
}

/// Where the queue of a stall stands: a few places straight out from the counter,
/// then along the path, turning where the paving ends.
void LayQueue(const Env& env, int component, Dir facing, ServiceSite& site) {
    const Vec2 out = ToVec(Step(facing));
    const Vec2 right = ToVec(Step(RotatedClockwise(facing)));
    const Vec2 left = right * -1.0f;
    const auto clear = [&](Vec2 from, Vec2 to) {
        return env.nav.ComponentOf(World::TileAt(to), kWalker) == component &&
               env.nav.Walkable(from, to, kWalker);
    };
    Vec2 place = site.door - out * 0.55f;
    Vec2 heading = out;
    for (int i = 0; i < kQueuePlaces; ++i) {
        site.line.push_back(place);
        if (i == 3) {
            heading = right;
        }
        const Vec2 tries[4] = {heading, out, right, left};
        bool moved = false;
        for (const Vec2 direction : tries) {
            if (Dot(direction, heading) < -0.5f) {
                continue;  // never back over the people already standing
            }
            const Vec2 next = place + direction * kQueueSpacingM;
            if (clear(place, next)) {
                place = next;
                heading = direction;
                moved = true;
                break;
            }
        }
        if (!moved) {
            break;
        }
    }
}

}  // namespace

Vec2 SeatPosition(const SeatSite& bench, int seat, float& face) {
    const Vec2 forward = HeadingVector(bench.heading);
    const Vec2 right{forward.z, -forward.x};
    if (bench.seats <= 3) {
        // A bench: side by side, looking the way the bench looks.
        const float along = (static_cast<float>(seat) - 0.5f * static_cast<float>(bench.seats - 1)) * 0.5f;
        face = bench.heading;
        return bench.position + right * along;
    }
    // A table: a row on each side, facing each other.
    const int near = (bench.seats + 1) / 2;
    const bool back = seat < near;
    const int index = back ? seat : seat - near;
    const int count = back ? near : bench.seats - near;
    const float along = (static_cast<float>(index) - 0.5f * static_cast<float>(count - 1)) * 0.55f;
    face = back ? bench.heading : WrapAngle(bench.heading + kPi);
    return bench.position + right * along + forward * (back ? -0.72f : 0.72f);
}

int Sites::ServiceIndex(BuildingId id) const {
    const auto it = std::lower_bound(services.begin(), services.end(), id,
                                     [](const ServiceSite& s, BuildingId value) { return s.id < value; });
    return it != services.end() && it->id == id ? static_cast<int>(it - services.begin()) : -1;
}

int Sites::ViewIndex(EnclosureId id) const {
    const auto it =
        std::lower_bound(views.begin(), views.end(), id,
                         [](const ViewSite& s, EnclosureId value) { return s.enclosure < value; });
    return it != views.end() && it->enclosure == id ? static_cast<int>(it - views.begin()) : -1;
}

void Sites::Refresh(const Env& env) {
    const World& world = env.world;
    if (valid && nav == world.NavRevision() && fences == world.FenceRevision() &&
        objects == env.objects.Revision() && enclosures == env.enclosures.Revision() &&
        width == world.Width() && depth == world.Depth()) {
        return;
    }
    valid = true;
    nav = world.NavRevision();
    fences = world.FenceRevision();
    objects = env.objects.Revision();
    enclosures = env.enclosures.Revision();
    width = world.Width();
    depth = world.Depth();
    views.clear();
    services.clear();
    benches.clear();
    bins.clear();
    boards.clear();
    beauty.assign(static_cast<std::size_t>(width) * static_cast<std::size_t>(depth), 0.0f);

    spawn = world.SpawnPoint();
    entranceTile = world.EntranceTile();
    const int component = env.nav.ComponentOf(entranceTile, kWalker);
    open = component >= 0 && env.nav.ComponentOf(World::TileAt(spawn), kWalker) == component;
    if (!open) {
        return;  // nobody gets in: nothing is of use
    }
    const auto reachable = [&](TileCoord t) { return env.nav.ComponentOf(t, kWalker) == component; };

    for (const Enclosure& enclosure : env.enclosures.All()) {
        ViewSite site;
        site.enclosure = enclosure.id;
        Vec2 sum;
        for (const ViewingSpot& spot : enclosure.viewing) {
            if (spot.visibility < 0.05f || !reachable(spot.tile)) {
                continue;
            }
            site.spots.push_back(spot);
            sum += World::TileCenter(spot.tile);
            site.visibility = std::max(site.visibility, spot.visibility);
        }
        if (!site.spots.empty()) {
            site.centre = sum / static_cast<float>(site.spots.size());
            views.push_back(std::move(site));
        }
    }

    for (const Building& building : env.objects.Buildings()) {
        if (building.def < 0 || building.def >= static_cast<int>(env.catalog.buildings.size())) {
            continue;
        }
        const BuildingDef& def = env.catalog.buildings[static_cast<std::size_t>(building.def)];
        if (!ServesVisitors(def.role)) {
            continue;
        }
        const TileCoord front = FrontTile(def, building.origin, building.facing);
        if (!reachable(front)) {
            continue;
        }
        ServiceSite site;
        site.id = building.id;
        site.def = building.def;
        site.role = def.role;
        site.door = World::TileCenter(front);
        site.face = HeadingOf(ToVec(Step(building.facing)) * -1.0f);
        site.counters = std::max(1, def.capacity / kCapacityPerCounter);
        if (def.role == BuildingRole::FoodStall || def.role == BuildingRole::DrinkStall) {
            LayQueue(env, component, building.facing, site);
        }
        services.push_back(std::move(site));
    }

    for (const Prop& prop : env.objects.Props()) {
        if (prop.def < 0 || prop.def >= static_cast<int>(env.catalog.props.size())) {
            continue;
        }
        const PropDef& def = env.catalog.props[static_cast<std::size_t>(prop.def)];
        if (def.beauty > 0.0f) {
            // Pleasant to be near: fades out over eight metres.
            constexpr float kReach = 8.0f;
            const TileCoord centre = World::TileAt(prop.position);
            const int span = static_cast<int>(kReach / kTileSize) + 1;
            for (int dz = -span; dz <= span; ++dz) {
                for (int dx = -span; dx <= span; ++dx) {
                    const TileCoord t{centre.x + dx, centre.z + dz};
                    if (!InBounds(t)) {
                        continue;
                    }
                    const float distance = Distance(World::TileCenter(t), prop.position);
                    if (distance < kReach) {
                        beauty[TileIndex(t)] += def.beauty * (1.0f - distance / kReach);
                    }
                }
            }
        }
        if (def.role != PropRole::Bench && def.role != PropRole::Bin && def.role != PropRole::InfoBoard) {
            continue;
        }
        const auto tile = env.nav.NearestPassable(World::TileAt(prop.position), kWalker, 2);
        if (!tile || !reachable(*tile)) {
            continue;
        }
        const Vec2 access = World::TileCenter(*tile);
        if (def.role == PropRole::Bench) {
            benches.push_back({prop.id, prop.position, prop.heading,
                               std::clamp(static_cast<int>(def.capacity), 1, 8), access});
            continue;
        }
        // Stand beside it, on the paving.
        Vec2 stand = prop.position + Normalized(access - prop.position, {0.0f, 1.0f}) * (def.radiusM + 0.45f);
        if (World::TileAt(stand) != *tile && !reachable(World::TileAt(stand))) {
            stand = access;
        }
        (def.role == PropRole::Bin ? bins : boards).push_back({prop.id, prop.position, stand, def.capacity});
    }
}

}  // namespace zoo::visitors_detail
