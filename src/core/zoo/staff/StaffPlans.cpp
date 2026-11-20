// SPDX-License-Identifier: MIT
#include <algorithm>
#include <cmath>
#include <limits>

#include "zoo/staff/StaffInternal.hpp"

// Who is engaged, where they go and what they do at each stop. Everything here is a pure
// function of the zoo's state at the moment of planning: no random stream is consumed.

namespace zoo {

namespace staff_detail {

bool HoldsDanger(const Env& env, const Animals& animals, EnclosureId enclosure) {
    if (!enclosure) {
        return false;
    }
    for (const Animal& animal : animals.All()) {
        if (!animal.InWorld() || animal.species < 0 ||
            static_cast<std::size_t>(animal.species) >= env.species.All().size()) {
            continue;
        }
        const SpeciesDef& def = env.species.All()[static_cast<std::size_t>(animal.species)];
        if (def.dangerous && (animal.home == enclosure || env.enclosures.At(animal.position) == enclosure)) {
            return true;
        }
    }
    return false;
}

bool DangerSecuredForStaff(const Env& env, const Animals& animals, EnclosureId enclosure) {
    if (!enclosure || !animals.StaffLockout(enclosure)) {
        return false;
    }
    bool found = false;
    for (const Animal& animal : animals.All()) {
        if (!animal.InWorld() || animal.species < 0 ||
            static_cast<std::size_t>(animal.species) >= env.species.All().size()) {
            continue;
        }
        const SpeciesDef& def = env.species.All()[static_cast<std::size_t>(animal.species)];
        if (!def.dangerous || (animal.home != enclosure && env.enclosures.At(animal.position) != enclosure)) {
            continue;
        }
        found = true;
        if (!animal.shelter) {
            return false;
        }
    }
    return found;
}

bool Reachable(const Env& env, TileCoord tile) {
    return env.nav.Passable(tile, kStaffProfile) &&
           env.nav.Reachable(tile, env.world.EntranceTile(), kStaffProfile);
}

}  // namespace staff_detail

using namespace staff_detail;

namespace {

/// Seconds a job is seen being done.
constexpr float kFillFeederS = 12.0f;
constexpr float kFillTroughS = 14.0f;
constexpr float kThrowFeedS = 9.0f;
constexpr float kScatterS = 15.0f;
constexpr float kRakeS = 34.0f;
constexpr float kDangerousRakeS = 12.0f;
/// The keeper remains outside while the last assigned house gate completes its closing travel.
constexpr float kHoldingGateCloseS = 1.2f;
/// Opening the holding gate: the release occurs halfway through this visible operation.
constexpr float kReleaseGateS = 2.4f;
constexpr float kHoseS = 30.0f;
constexpr float kInspectS = 22.0f;
constexpr float kSweepS = 24.0f;
/// How far from a bin its cleaner picks a litter target, in metres. This overlaps
/// neighbouring bin catchments after accounting for the cleaner's stand beside a bin.
constexpr float kLitterReach = 30.0f;
constexpr float kEmptyBinS = 10.0f;
constexpr float kTendS = 30.0f;
constexpr float kWaterS = 16.0f;
constexpr float kNightRestS = 90.0f;
/// Dirt from which a habitat is raked whatever the hour (0..1).
constexpr float kRakeFromDirt = 0.25f;
/// Dangerous groups are shut in only for a short, necessary clean, not for every round.
/// Starting before the ordinary issue threshold leaves ample time to reach the service gate.
constexpr float kDangerousRakeFromDirt = 0.18f;
/// At most this many feeders and troughs are seen to one by one at a stop, the emptiest
/// first. The rest wait for the feed scattered at a public feeding or for the zoo's
/// automatic care.
constexpr std::size_t kMostFills = 4;
/// What a metre of walking from the job before weighs against a metre nearer the public
/// when the place of the next job is chosen.
constexpr float kNearWeight = 0.4f;
/// How many beds a gardener and how many bins a cleaner has on the round at most.
constexpr std::size_t kMostBeds = 24;
constexpr std::size_t kMostBins = 30;
/// Up to this many habitats the keepers' pairs are the best possible; beyond, the nearest are paired.
constexpr std::size_t kMatchLimit = 14;
/// Two public feedings of one keeper are at least this far apart (calendar hours).
constexpr float kFeedingGap = 0.75f;

std::int32_t Ticks(float seconds) {
    return static_cast<std::int32_t>(seconds * static_cast<float>(kTicksPerSecond));
}

Vec2 Along(Dir d) {
    const TileCoord step = Step(d);
    return {static_cast<float>(step.x), static_cast<float>(step.z)};
}

/// A place to stand and the way to face there.
struct Stand {
    Vec2 place;
    float facing = 0.0f;
};

/// How far the nearest place visitors watch the habitat from is, and where. Without any
/// such place the distance is 0: no spot is nearer the public than another.
struct Front {
    float distance = 0.0f;
    Vec2 point;
    bool any = false;
};
Front NearestView(const Enclosure& enclosure, Vec2 from) {
    Front best;
    for (const ViewingSpot& spot : enclosure.viewing) {
        const Vec2 point = World::TileCenter(spot.tile);
        const float distance = Distance(point, from);
        if (!best.any || distance < best.distance) {
            best = {distance, point, true};
        }
    }
    return best;
}

/// Is the ground here dry, or wet no deeper than a boot. The tiles beside a pool are dry
/// by the map and still slope down into it: the pool's bed is dug at the corners they share.
bool Shallow(const Env& env, Vec2 point) {
    const TileCoord tile = World::TileAt(point);
    const float ground = env.world.HeightAt(point);
    for (int dz = -1; dz <= 1; ++dz) {
        for (int dx = -1; dx <= 1; ++dx) {
            const TileCoord near{tile.x + dx, tile.z + dz};
            if (env.world.HasWater(near) && env.world.At(near).waterLevel - ground > 0.08f) {
                return false;
            }
        }
    }
    return true;
}

/// May a member of staff stand here to work: reachable ground and not in water, inside
/// `inside` when given, on public ground otherwise.
bool Standable(const Env& env, Vec2 point, EnclosureId inside) {
    const TileCoord tile = World::TileAt(point);
    if (!Reachable(env, tile) || !Shallow(env, point)) {
        return false;
    }
    return inside ? env.enclosures.At(tile) == inside : env.enclosures.RegionAt(tile) == Region::Public;
}

/// A place to stand beside `target`, `reach` metres from it or a little more, on the side
/// `wish` points to when that can be had, facing the target.
std::optional<Stand> StandBeside(const Env& env, Vec2 target, Vec2 wish, float reach, EnclosureId inside) {
    static constexpr std::array<float, 8> kTurns = {0.0f,     0.7854f, -0.7854f, 1.5708f,
                                                    -1.5708f, 2.3562f, -2.3562f, 3.1416f};
    static constexpr std::array<float, 4> kFurther = {0.0f, 0.6f, 1.4f, 2.6f};
    const float base = HeadingOf(Normalized(wish));
    for (const float further : kFurther) {
        for (const float turn : kTurns) {
            const Vec2 place = target + HeadingVector(base + turn) * (reach + further);
            if (Standable(env, place, inside)) {
                return Stand{place, HeadingOf(target - place)};
            }
        }
    }
    return std::nullopt;
}

/// Where the habitat can be served from outside: public ground right at its barrier
/// (usually the keeper gate), and the places visitors watch from.  A stand-off strip can
/// leave one service-gate point directly beside the habitat; keep the viewing bays as well
/// so work does not all happen at that one hidden point.
void BarrierStands(const Env& env, const Enclosure& enclosure, std::vector<Stand>& out) {
    out.clear();
    for (int z = enclosure.min.z; z <= enclosure.max.z; ++z) {
        for (int x = enclosure.min.x; x <= enclosure.max.x; ++x) {
            const TileCoord tile{x, z};
            if (env.enclosures.At(tile) != enclosure.id) {
                continue;
            }
            for (int d = 0; d < 4; ++d) {
                const Dir side = static_cast<Dir>(d);
                const TileCoord outside = tile + Step(side);
                if (env.enclosures.RegionAt(outside) != Region::Public || !Reachable(env, outside)) {
                    continue;
                }
                const Vec2 inward = Along(Opposite(side));
                out.push_back({World::TileCenter(outside) + inward * 0.4f, HeadingOf(inward)});
            }
        }
    }
    for (const ViewingSpot& spot : enclosure.viewing) {
        if (Reachable(env, spot.tile)) {
            const Vec2 inward = Along(spot.side);
            out.push_back({World::TileCenter(spot.tile) + inward * 0.4f, HeadingOf(inward)});
        }
    }
}

/// The dry tiles inside a habitat a keeper can walk to, in row-major order.
void OpenTiles(const Env& env, const Enclosure& enclosure, std::vector<TileCoord>& out) {
    out.clear();
    for (int z = enclosure.min.z; z <= enclosure.max.z; ++z) {
        for (int x = enclosure.min.x; x <= enclosure.max.x; ++x) {
            const TileCoord tile{x, z};
            if (env.enclosures.At(tile) == enclosure.id && !env.world.HasWater(tile) &&
                Reachable(env, tile)) {
                out.push_back(tile);
            }
        }
    }
}

bool ClearOfProps(const Env& env, Vec2 point, float margin, std::vector<PropId>& scratch) {
    env.objects.PropsNear(point, 4.0f, scratch);
    for (const PropId id : scratch) {
        const Prop* prop = env.objects.FindProp(id);
        const PropDef& def = env.catalog.props[static_cast<std::size_t>(prop->def)];
        if (def.blocksWalking && Distance(prop->position, point) < def.radiusM * prop->scale + margin) {
            return false;
        }
    }
    return true;
}

std::uint32_t TileSalt(TileCoord tile, std::uint32_t salt) {
    return HashU32(HashU32(static_cast<std::uint32_t>(tile.x), static_cast<std::uint32_t>(tile.z)), salt);
}

/// Metres of walking the place costs after the job before, weighted; nothing for the first job.
float Detour(std::optional<Vec2> after, Vec2 place) {
    return after ? kNearWeight * Distance(*after, place) : 0.0f;
}

/// Open ground to rake: near where the public stands, away from where the animals are
/// now, not far from `after`, a different patch for a different `salt`.
std::optional<Stand> PaddockStand(const Env& env, const Animals& animals, const Enclosure& enclosure,
                                  const std::vector<TileCoord>& open, std::uint32_t salt,
                                  std::optional<Vec2> after) {
    std::vector<Vec2> herd;
    for (const Animal& animal : animals.All()) {
        if (animal.Visible() && env.enclosures.At(animal.position) == enclosure.id) {
            herd.push_back(animal.position);
        }
    }
    struct Scored {
        float score = 0.0f;
        TileCoord tile;
    };
    std::array<Scored, 6> best{};
    std::size_t kept = 0;
    for (const TileCoord tile : open) {
        const Vec2 centre = World::TileCenter(tile);
        if (!Shallow(env, centre)) {
            continue;
        }
        float clear = 14.0f;
        for (const Vec2 at : herd) {
            clear = std::min(clear, Distance(at, centre));
        }
        const float front = NearestView(enclosure, centre).distance;
        const float score = clear - 0.8f * std::max(0.0f, front - 4.0f) - Detour(after, centre) +
                            5.0f * static_cast<float>(TileSalt(tile, salt) >> 8u) * (1.0f / 16777216.0f);
        std::size_t at = kept;
        while (at > 0 && best[at - 1].score < score) {
            --at;
        }
        if (at >= best.size()) {
            continue;
        }
        kept = std::min(kept + 1, best.size());
        for (std::size_t i = kept - 1; i > at; --i) {
            best[i] = best[i - 1];
        }
        best[at] = {score, tile};
    }
    std::vector<PropId> scratch;
    for (std::size_t i = 0; i < kept; ++i) {
        const Vec2 centre = World::TileCenter(best[i].tile);
        if (!ClearOfProps(env, centre, 0.9f, scratch)) {
            continue;
        }
        // Side on to the public: the rake is seen working, not hidden behind the back.
        const Front front = NearestView(enclosure, centre);
        const float facing = front.any ? WrapAngle(HeadingOf(front.point - centre) + 1.2f) : 0.0f;
        return Stand{centre, facing};
    }
    return std::nullopt;
}

/// In front of the door of the habitat's house.
std::optional<Stand> ShelterStand(const Env& env, const Enclosure& enclosure) {
    for (const BuildingId id : enclosure.shelters) {
        const Building* building = env.objects.FindBuilding(id);
        if (building == nullptr) {
            continue;
        }
        const BuildingDef& def = env.catalog.buildings[static_cast<std::size_t>(building->def)];
        const TileCoord front = FrontTile(def, building->origin, building->facing);
        const Vec2 place = World::TileCenter(front);
        if (Standable(env, place, enclosure.id) && !env.world.HasWater(front)) {
            return Stand{place, HeadingOf(Along(Opposite(building->facing)))};
        }
    }
    return std::nullopt;
}

/// At the edge of the habitat's water, facing it; the edge nearest the public first.
std::optional<Stand> WaterStand(const Env& env, const Enclosure& enclosure,
                                const std::vector<TileCoord>& open, std::uint32_t salt,
                                std::optional<Vec2> after) {
    std::optional<Stand> best;
    float bestScore = std::numeric_limits<float>::max();
    for (const TileCoord tile : open) {
        for (int d = 0; d < 4; ++d) {
            const Dir side = static_cast<Dir>(d);
            const TileCoord wet = tile + Step(side);
            if (!env.world.HasWater(wet) || env.enclosures.At(wet) != enclosure.id) {
                continue;
            }
            // As near the water as the bank lets one stand without wading.
            const Vec2 centre = World::TileCenter(tile);
            std::optional<Vec2> bank;
            for (const float towards : {0.35f, 0.05f, -0.25f, -0.55f, -0.85f}) {
                if (Shallow(env, centre + Along(side) * towards)) {
                    bank = centre + Along(side) * towards;
                    break;
                }
            }
            if (!bank) {
                continue;
            }
            const float score = NearestView(enclosure, centre).distance + Detour(after, centre) +
                                6.0f * HashFloat(TileSalt(tile, salt), 3u);
            if (score < bestScore) {
                bestScore = score;
                best = Stand{*bank, HeadingOf(Along(side))};
            }
        }
    }
    return best;
}

/// Inside, at a stretch of the habitat's fence, facing it; a stretch near the public first.
std::optional<Stand> FenceStand(const Env& env, const Enclosure& enclosure,
                                const std::vector<TileCoord>& open, std::uint32_t salt,
                                std::optional<Vec2> after) {
    std::optional<Stand> best;
    float bestScore = std::numeric_limits<float>::max();
    for (const TileCoord tile : open) {
        for (int d = 0; d < 4; ++d) {
            const Dir side = static_cast<Dir>(d);
            const Fence fence = env.world.FenceOn(tile, side);
            if (!fence.Exists() || fence.gate || env.enclosures.At(tile + Step(side)) == enclosure.id) {
                continue;
            }
            const Vec2 centre = World::TileCenter(tile);
            if (!Shallow(env, centre + Along(side) * 0.45f)) {
                continue;
            }
            const float score = NearestView(enclosure, centre).distance + Detour(after, centre) +
                                8.0f * HashFloat(TileSalt(tile, salt), 5u);
            if (score < bestScore) {
                bestScore = score;
                best = Stand{centre + Along(side) * 0.45f, HeadingOf(Along(side))};
            }
        }
    }
    return best;
}

/// One of the places at the barrier, those near the public first, another for another `salt`.
std::optional<Stand> PickBarrier(const Enclosure& enclosure, const std::vector<Stand>& stands,
                                 std::uint32_t salt, std::optional<Vec2> after) {
    std::optional<Stand> best;
    float bestScore = std::numeric_limits<float>::max();
    for (std::size_t i = 0; i < stands.size(); ++i) {
        const float score = NearestView(enclosure, stands[i].place).distance +
                            Detour(after, stands[i].place) +
                            10.0f * HashFloat(static_cast<std::uint32_t>(i), salt);
        if (score < bestScore) {
            bestScore = score;
            best = stands[i];
        }
    }
    return best;
}

std::optional<Stand> NearestStand(const std::vector<Stand>& stands, Vec2 to) {
    std::optional<Stand> best;
    float bestDistance = std::numeric_limits<float>::max();
    for (const Stand& stand : stands) {
        const float distance = DistanceSquared(stand.place, to);
        if (distance < bestDistance) {
            bestDistance = distance;
            best = stand;
        }
    }
    return best;
}

/// Public ground at a keeper gate, facing into the habitat. This is where the lockout
/// is operated both before entry and after the keeper has come back out.
std::optional<Stand> LockoutStand(const Env& env, const Enclosure& enclosure, Vec2 from, Vec2& insidePlace) {
    std::optional<Stand> best;
    float nearest = std::numeric_limits<float>::max();
    for (const EdgeRef gate : enclosure.gates) {
        const TileCoord near{gate.x, gate.z};
        const TileCoord far = gate.horizontal ? TileCoord{gate.x, gate.z - 1} : TileCoord{gate.x - 1, gate.z};
        TileCoord inside{};
        TileCoord outside{};
        if (env.enclosures.At(near) == enclosure.id) {
            inside = near;
            outside = far;
        } else if (env.enclosures.At(far) == enclosure.id) {
            inside = far;
            outside = near;
        } else {
            continue;
        }
        if (env.enclosures.RegionAt(outside) != Region::Public || !Reachable(env, inside) ||
            !Reachable(env, outside)) {
            continue;
        }
        const Vec2 inward = Normalized(World::TileCenter(inside) - World::TileCenter(outside));
        const Vec2 place = World::TileCenter(outside) + inward * 0.4f;
        const float distance = DistanceSquared(from, place);
        if (distance < nearest && Shallow(env, place)) {
            nearest = distance;
            best = Stand{place, HeadingOf(inward)};
            insidePlace = World::TileCenter(inside);
        }
    }
    return best;
}

/// The open tile of the habitat nearest to `target` that no tree or rock stands on: where a
/// keeper serves from inside what cannot be stood beside (a feeder on an island).
std::optional<Stand> NearestOpen(const Env& env, const std::vector<TileCoord>& open, Vec2 target) {
    std::optional<Stand> best;
    float bestDistance = std::numeric_limits<float>::max();
    std::vector<PropId> scratch;
    for (const TileCoord tile : open) {
        const Vec2 centre = World::TileCenter(tile);
        const float distance = DistanceSquared(centre, target);
        if (distance < bestDistance && ClearOfProps(env, centre, 0.6f, scratch)) {
            bestDistance = distance;
            best = Stand{centre, HeadingOf(target - centre)};
        }
    }
    return best;
}

StaffTask Job(StaffActivity work, StaffActivity approach, Vec2 place, float facing, float seconds,
              EnclosureId enclosure = {}, PropId prop = {},
              StaffLockoutAction lockout = StaffLockoutAction::None) {
    return StaffTask{work, approach, place, facing, Ticks(seconds), enclosure, prop, lockout};
}

/// Puts the jobs from `first` on in the order of the shortest hops, starting from `from`.
void OrderByWalk(std::vector<StaffTask>& tasks, std::size_t first, Vec2 from) {
    for (std::size_t i = first; i < tasks.size(); ++i) {
        std::size_t nearest = i;
        for (std::size_t j = i + 1; j < tasks.size(); ++j) {
            if (DistanceSquared(tasks[j].place, from) < DistanceSquared(tasks[nearest].place, from)) {
                nearest = j;
            }
        }
        std::swap(tasks[i], tasks[nearest]);
        from = tasks[i].place;
    }
}

/// The part of the day the routine work goes by (calendar hours).
enum class Shift { Night, Morning, LateMorning, Midday, Afternoon, Evening };
Shift ShiftAt(float hour) {
    if (hour < 7.0f || hour >= 18.5f) {
        return Shift::Night;
    }
    if (hour < 10.5f) {
        return Shift::Morning;
    }
    if (hour < 12.0f) {
        return Shift::LateMorning;
    }
    if (hour < 13.5f) {
        return Shift::Midday;
    }
    return hour < 16.0f ? Shift::Afternoon : Shift::Evening;
}

/// A keeper's jobs at a habitat. `feeding`: this is a public feeding, feed is scattered and
/// every feeder gets its fill.
void PlanHabitat(const Env& env, const Animals& animals, StaffMember& member, EnclosureId id, bool feeding) {
    const Enclosure* found = env.enclosures.Find(id);
    if (found == nullptr) {
        return;
    }
    const Enclosure& enclosure = *found;
    const bool dangerous = HoldsDanger(env, animals, id);
    Vec2 lockoutInside{};
    const std::optional<Stand> lockout =
        dangerous ? LockoutStand(env, enclosure, member.position, lockoutInside) : std::optional<Stand>{};
    const Vec2 lockoutPlace = lockout ? lockout->place : Vec2{};
    const float lockoutFacing = lockout ? lockout->facing : 0.0f;
    const bool lockoutInsideValid =
        lockout && env.enclosures.At(lockoutInside) == id && Standable(env, lockoutInside, id);
    const bool lockoutDue =
        animals.Dirt(id) >= kDangerousRakeFromDirt || (member.stopsDone + id.value) % 3u == 0u;
    const bool enter = dangerous ? lockoutInsideValid && lockoutDue : KeepersMayEnter(env, animals, id);
    const bool serveInside = enter && !dangerous;
    std::vector<Stand> barrier;
    BarrierStands(env, enclosure, barrier);
    std::vector<TileCoord> open;
    if (enter) {
        OpenTiles(env, enclosure, open);
    }
    const std::uint32_t salt = HashU32(member.id.value * 7919u + member.stopsDone, id.value);
    std::vector<StaffTask>& tasks = member.tasks;

    // What the animals need: feed and water where it runs low, the emptiest first.
    struct Fill {
        float full = 0.0f;
        StaffTask task;
    };
    std::vector<Fill> fills;
    for (const PropId feederId : enclosure.feeders) {
        const Prop* feeder = env.objects.FindProp(feederId);
        if (feeder == nullptr || animals.FoodFor(env, *feeder) < 0) {
            continue;
        }
        const PropDef& def = env.catalog.props[static_cast<std::size_t>(feeder->def)];
        const float full = def.capacity > 0.0f ? Animals::EdibleKg(env, *feeder) / def.capacity : 1.0f;
        if (!feeding && full >= Staff::kFeederDue) {
            continue;
        }
        if (serveInside) {
            // Behind the feeder as the public sees it: the keeper faces them.
            const Front front = NearestView(enclosure, feeder->position);
            const Vec2 wish = front.any ? feeder->position - front.point : Vec2{0.0f, -1.0f};
            if (const auto stand =
                    StandBeside(env, feeder->position, wish, def.radiusM * feeder->scale + 0.5f, id)) {
                fills.push_back({full, Job(StaffActivity::FillFeeder, StaffActivity::Carry, stand->place,
                                           stand->facing, kFillFeederS, id, feederId)});
                continue;
            }
        }
        const Stand stand = (serveInside ? NearestOpen(env, open, feeder->position) : std::nullopt)
                                .or_else([&] { return NearestStand(barrier, feeder->position); })
                                .value_or(Stand{member.position, 0.0f});
        fills.push_back({full, Job(StaffActivity::ThrowFeed, StaffActivity::Carry, stand.place,
                                   HeadingOf(feeder->position - stand.place), kThrowFeedS, id, feederId)});
    }
    for (const PropId troughId : enclosure.troughs) {
        const Prop* trough = env.objects.FindProp(troughId);
        if (trough == nullptr) {
            continue;
        }
        const PropDef& def = env.catalog.props[static_cast<std::size_t>(trough->def)];
        const float full = def.capacity > 0.0f ? trough->amount / def.capacity : 1.0f;
        if (full >= Staff::kTroughDue) {
            continue;
        }
        if (serveInside) {
            const Front front = NearestView(enclosure, trough->position);
            const Vec2 wish = front.any ? trough->position - front.point : Vec2{0.0f, -1.0f};
            if (const auto stand =
                    StandBeside(env, trough->position, wish, def.radiusM * trough->scale + 0.5f, id)) {
                fills.push_back({full, Job(StaffActivity::FillTrough, StaffActivity::Carry, stand->place,
                                           stand->facing, kFillTroughS, id, troughId)});
                continue;
            }
        }
        const Stand stand = (serveInside ? NearestOpen(env, open, trough->position) : std::nullopt)
                                .or_else([&] { return NearestStand(barrier, trough->position); })
                                .value_or(Stand{member.position, 0.0f});
        fills.push_back({full, Job(StaffActivity::Hose, StaffActivity::Walk, stand.place,
                                   HeadingOf(trough->position - stand.place), kFillTroughS, id, troughId)});
    }
    if (fills.size() > kMostFills) {
        std::stable_sort(fills.begin(), fills.end(),
                         [](const Fill& a, const Fill& b) { return a.full < b.full; });
        fills.resize(kMostFills);
    }
    for (const Fill& fill : fills) {
        tasks.push_back(fill.task);
    }
    OrderByWalk(tasks, 0, member.position);

    // The place of the job before: the next one is not sought at the other end of the habitat.
    const auto after = [&]() -> std::optional<Vec2> {
        return tasks.empty() ? std::nullopt : std::optional<Vec2>(tasks.back().place);
    };
    const auto rake = [&](std::uint32_t which) {
        std::optional<Stand> stand;
        if (which == 0 && (salt & 1u) != 0) {
            stand = ShelterStand(env, enclosure);
        }
        if (!stand) {
            stand = PaddockStand(env, animals, enclosure, open, salt + which, after());
        }
        if (stand) {
            tasks.push_back(
                Job(StaffActivity::Rake, StaffActivity::Push, stand->place, stand->facing, kRakeS, id));
        }
        return stand.has_value();
    };
    const auto hose = [&] {
        if (const auto stand = WaterStand(env, enclosure, open, salt, after())) {
            tasks.push_back(
                Job(StaffActivity::Hose, StaffActivity::Walk, stand->place, stand->facing, kHoseS, id));
            return true;
        }
        return rake(3);
    };
    const auto inspect = [&](std::uint32_t which) {
        const auto stand = enter ? FenceStand(env, enclosure, open, salt + which, after())
                                 : PickBarrier(enclosure, barrier, salt + which, after());
        if (stand) {
            tasks.push_back(
                Job(StaffActivity::Inspect, StaffActivity::Walk, stand->place, stand->facing, kInspectS, id));
        }
    };
    const auto sweep = [&](std::uint32_t which) {
        if (const auto stand = PickBarrier(enclosure, barrier, salt + 10u + which, after())) {
            // Along the barrier, not at it.
            tasks.push_back(Job(StaffActivity::Sweep, StaffActivity::Walk, stand->place,
                                WrapAngle(stand->facing + 1.5708f), kSweepS, id));
        }
    };
    const auto rest = [&](float seconds) {
        const Stand stand =
            PickBarrier(enclosure, barrier, salt + 20u, after()).value_or(Stand{member.position, 0.0f});
        tasks.push_back(
            Job(StaffActivity::Rest, StaffActivity::Walk, stand.place, stand.facing, seconds, id));
    };

    if (feeding) {
        // Something for the animals to come for, where the public sees it. It tops up every
        // feeder of the habitat (Staff::Apply): a public feeding leaves nobody out.
        const auto stand = serveInside ? PaddockStand(env, animals, enclosure, open, salt + 30u, after())
                                       : PickBarrier(enclosure, barrier, salt, after());
        if (stand) {
            tasks.push_back(Job(StaffActivity::ThrowFeed, StaffActivity::Carry, stand->place, stand->facing,
                                kScatterS, id));
        }
    }

    const std::size_t routine = tasks.size();
    const std::size_t firstInside = tasks.size();
    const Shift shift = ShiftAt(env.time.Hour());
    if (enter && dangerous) {
        // One purposeful job keeps the group lockout short enough that normal feeding and
        // drinking are not displaced by a long sequence or a walk across the habitat.
        tasks.push_back(Job(StaffActivity::Rake, StaffActivity::Push, lockoutInside,
                            WrapAngle(lockoutFacing + 1.2f), kDangerousRakeS, id));
    } else if (enter) {
        switch (shift) {
            case Shift::Morning:
                rake(0);
                rake(1);
                hose();
                inspect(0);
                break;
            case Shift::LateMorning:
                hose();
                rake(1);
                inspect(0);
                rake(2);
                break;
            case Shift::Midday:
                inspect(0);
                rake(1);
                rest(20.0f);
                rake(2);
                break;
            case Shift::Afternoon:
                rake(0);
                hose();
                rake(1);
                inspect(0);
                break;
            case Shift::Evening:
                hose();
                inspect(0);
                rake(1);
                break;
            case Shift::Night:
                inspect(0);
                rest(kNightRestS);
                break;
        }
        const bool rakes = std::any_of(tasks.begin() + static_cast<std::ptrdiff_t>(routine), tasks.end(),
                                       [](const StaffTask& t) { return t.work == StaffActivity::Rake; });
        if (!rakes && animals.Dirt(id) > kRakeFromDirt) {
            rake(4);
        }
    } else {
        switch (shift) {
            case Shift::Morning:
            case Shift::Afternoon:
                inspect(0);
                sweep(0);
                inspect(1);
                sweep(1);
                break;
            case Shift::LateMorning:
                sweep(0);
                inspect(0);
                rest(15.0f);
                inspect(1);
                break;
            case Shift::Midday:
                inspect(0);
                rest(25.0f);
                sweep(0);
                break;
            case Shift::Evening:
                sweep(0);
                inspect(0);
                sweep(1);
                break;
            case Shift::Night:
                inspect(0);
                rest(kNightRestS);
                break;
        }
    }
    if (dangerous && enter && lockout && tasks.size() > firstInside) {
        tasks.insert(tasks.begin() + static_cast<std::ptrdiff_t>(firstInside),
                     Job(StaffActivity::Inspect, StaffActivity::Walk, lockoutPlace, lockoutFacing, 4.0f, id,
                         {}, StaffLockoutAction::Secure));
        tasks.insert(tasks.begin() + static_cast<std::ptrdiff_t>(firstInside + 1),
                     Job(StaffActivity::Inspect, StaffActivity::Walk, lockoutPlace, lockoutFacing,
                         kHoldingGateCloseS, id));
        tasks.push_back(Job(StaffActivity::Inspect, StaffActivity::Walk, lockoutPlace, lockoutFacing,
                            kReleaseGateS, id, {}, StaffLockoutAction::Release));
    }
}

/// A gardener's jobs at a bed.
void PlanBed(const Env& env, StaffMember& member, PropId id) {
    const Prop* bed = env.objects.FindProp(id);
    if (bed == nullptr) {
        return;
    }
    const PropDef& def = env.catalog.props[static_cast<std::size_t>(bed->def)];
    const Vec2 wish = HeadingVector(HashFloat(id.value, 11u) * kTwoPi);
    const auto stand = StandBeside(env, bed->position, wish, def.radiusM * bed->scale + 0.35f, {});
    if (!stand) {
        return;
    }
    if (member.stopsDone % 3u == 0u) {
        member.tasks.push_back(
            Job(StaffActivity::Water, StaffActivity::Carry, stand->place, stand->facing, kWaterS, {}, id));
    }
    member.tasks.push_back(
        Job(StaffActivity::Tend, StaffActivity::Walk, stand->place, stand->facing, kTendS, {}, id));
    if (member.stopsDone % 5u == 4u) {
        member.tasks.push_back(
            Job(StaffActivity::Rest, StaffActivity::Walk, stand->place, stand->facing, 15.0f, {}, id));
    }
}

/// A cleaner's jobs at a bin: the bin, then the litter lying nearest, or the path beside it.
void PlanBin(const Env& env, const Visitors& visitors, StaffMember& member, PropId id) {
    const Prop* bin = env.objects.FindProp(id);
    if (bin == nullptr) {
        return;
    }
    const PropDef& def = env.catalog.props[static_cast<std::size_t>(bin->def)];
    const Vec2 wish = HeadingVector(HashFloat(id.value, 13u) * kTwoPi);
    const auto stand = StandBeside(env, bin->position, wish, def.radiusM * bin->scale + 0.4f, {});
    if (!stand) {
        return;
    }
    member.tasks.push_back(
        Job(StaffActivity::EmptyBin, StaffActivity::Walk, stand->place, stand->facing, kEmptyBinS, {}, id));

    std::optional<Vec2> litter;
    float nearest = kLitterReach * kLitterReach;
    for (const Litter& piece : visitors.LitterOnGround()) {
        const float distance = DistanceSquared(piece.position, bin->position);
        if (distance < nearest && Standable(env, piece.position, {})) {
            nearest = distance;
            litter = piece.position;
        }
    }
    const float along = HashFloat(id.value + member.stopsDone, 17u) * kTwoPi;
    if (litter) {
        member.tasks.push_back(
            Job(StaffActivity::Sweep, StaffActivity::Walk, *litter, along, kSweepS, {}, id));
    } else if (const auto beside = StandBeside(env, bin->position, HeadingVector(along), 2.5f, {})) {
        member.tasks.push_back(Job(StaffActivity::Sweep, StaffActivity::Walk, beside->place,
                                   WrapAngle(beside->facing + 1.5708f), kSweepS, {}, id));
    }
}

// ---- engaging the crew --------------------------------------------------------------------

/// Pairs up the points so that the pairs are as close as can be. `mate[i]` is the partner
/// of i, or -1 for the one left over when the number is odd.
class Pairing {
public:
    explicit Pairing(const std::vector<Vec2>& at)
        : at_(at), mate_(at.size(), kFree), best_(at.size(), -1), odd_(at.size() % 2 == 1) {
        if (at.size() <= kMatchLimit) {
            Search(0, 0.0f, false);
        } else {
            Greedy();
        }
    }
    const std::vector<int>& Mates() const { return best_; }

private:
    static constexpr int kFree = -2;

    void Search(std::size_t from, float cost, bool singleUsed) {
        while (from < at_.size() && mate_[from] != kFree) {
            ++from;
        }
        if (from == at_.size()) {
            if (cost < bestCost_) {
                bestCost_ = cost;
                best_ = mate_;
            }
            return;
        }
        if (cost >= bestCost_) {
            return;
        }
        if (odd_ && !singleUsed) {
            mate_[from] = -1;
            Search(from + 1, cost, true);
            mate_[from] = kFree;
        }
        for (std::size_t other = from + 1; other < at_.size(); ++other) {
            if (mate_[other] != kFree) {
                continue;
            }
            mate_[from] = static_cast<int>(other);
            mate_[other] = static_cast<int>(from);
            Search(from + 1, cost + Distance(at_[from], at_[other]), singleUsed);
            mate_[from] = kFree;
            mate_[other] = kFree;
        }
    }

    void Greedy() {
        for (;;) {
            float nearest = std::numeric_limits<float>::max();
            std::size_t a = at_.size();
            std::size_t b = at_.size();
            for (std::size_t i = 0; i < at_.size(); ++i) {
                for (std::size_t j = i + 1; j < at_.size(); ++j) {
                    if (mate_[i] == kFree && mate_[j] == kFree && Distance(at_[i], at_[j]) < nearest) {
                        nearest = Distance(at_[i], at_[j]);
                        a = i;
                        b = j;
                    }
                }
            }
            if (a == at_.size()) {
                break;
            }
            mate_[a] = static_cast<int>(b);
            mate_[b] = static_cast<int>(a);
        }
        for (int& mate : mate_) {
            mate = mate == kFree ? -1 : mate;
        }
        best_ = mate_;
    }

    const std::vector<Vec2>& at_;
    std::vector<int> mate_;
    std::vector<int> best_;
    float bestCost_ = std::numeric_limits<float>::max();
    bool odd_;
};

/// When the public can come and watch: twice a day, by what the animals eat.
std::array<float, 2> FeedingHoursOf(Diet diet) {
    switch (diet) {
        case Diet::Carnivore:
            return {11.0f, 15.0f};
        case Diet::Piscivore:
            return {11.5f, 15.5f};
        case Diet::Herbivore:
            return {9.5f, 14.0f};
        case Diet::Omnivore:
            return {10.0f, 14.5f};
        case Diet::Insectivore:
            return {10.5f, 16.0f};
    }
    return {-1.0f, -1.0f};
}

/// The species most of the habitat's animals belong to, or -1 for an empty one.
int SpeciesOf(const Env& env, const Animals& animals, EnclosureId enclosure) {
    std::vector<int> count(env.species.All().size(), 0);
    int most = -1;
    for (const Animal& animal : animals.All()) {
        if (animal.home != enclosure || animal.species < 0) {
            continue;
        }
        const auto index = static_cast<std::size_t>(animal.species);
        ++count[index];
        if (most < 0 || count[index] > count[static_cast<std::size_t>(most)]) {
            most = animal.species;
        }
    }
    return most;
}

struct PropStop {
    PropId prop;
    Vec2 place;
};

/// Orders the stops as a walk of short hops from `from`, then keeps at most `most` of
/// them, evenly spread along that walk.
void RoundOf(std::vector<PropStop>& stops, Vec2 from, std::size_t most) {
    for (std::size_t i = 0; i < stops.size(); ++i) {
        std::size_t nearest = i;
        for (std::size_t j = i + 1; j < stops.size(); ++j) {
            if (DistanceSquared(stops[j].place, from) < DistanceSquared(stops[nearest].place, from)) {
                nearest = j;
            }
        }
        std::swap(stops[i], stops[nearest]);
        from = stops[i].place;
    }
    if (stops.size() > most) {
        std::vector<PropStop> kept;
        for (std::size_t i = 0; i < most; ++i) {
            kept.push_back(stops[i * stops.size() / most]);
        }
        stops = std::move(kept);
    }
}

}  // namespace

void staff_detail::RefreshSweep(const Env& env, const Visitors& visitors, StaffMember& member) {
    if (member.task >= member.tasks.size()) {
        return;
    }
    StaffTask& task = member.tasks[member.task];
    const Prop* bin = task.prop ? env.objects.FindProp(task.prop) : nullptr;
    if (task.work != StaffActivity::Sweep || bin == nullptr) {
        return;
    }

    float nearest = kLitterReach * kLitterReach;
    std::optional<Vec2> best;
    for (const Litter& piece : visitors.LitterOnGround()) {
        const float distance = DistanceSquared(piece.position, bin->position);
        if (distance < nearest && Standable(env, piece.position, {})) {
            nearest = distance;
            best = piece.position;
        }
    }
    if (best) {
        task.place = *best;
    }
}

bool KeepersMayEnter(const Env& env, const Animals& animals, EnclosureId enclosure) {
    const Enclosure* found = env.enclosures.Find(enclosure);
    if (found == nullptr || HoldsDanger(env, animals, enclosure)) {
        return false;
    }
    for (const EdgeRef gate : found->gates) {
        // The two tiles a gate stands between: one of them is the habitat's.
        const TileCoord near{gate.x, gate.z};
        const TileCoord far = gate.horizontal ? TileCoord{gate.x, gate.z - 1} : TileCoord{gate.x - 1, gate.z};
        for (const TileCoord tile : {near, far}) {
            if (env.enclosures.At(tile) == enclosure && Reachable(env, tile)) {
                return true;
            }
        }
    }
    return false;
}

void Staff::Engage(const Env& env, const Animals& animals) {
    members_.clear();
    const Vec2 entrance = World::TileCenter(env.world.EntranceTile());
    const auto add = [this](StaffRole role, Vec2 position, float heading) -> StaffMember& {
        StaffMember member;
        member.id = StaffId{nextId_++};
        member.role = role;
        member.look = HashU32(member.id.value, 0x57aff);
        member.position = position;
        member.heading = heading;
        members_.push_back(std::move(member));
        return members_.back();
    };

    // Keepers: two habitats each, neighbours.
    struct Habitat {
        EnclosureId id;
        std::vector<Stand> barrier;
    };
    std::vector<Habitat> habitats;
    std::vector<Vec2> centres;
    for (const Enclosure& enclosure : env.enclosures.All()) {
        Habitat habitat{enclosure.id, {}};
        BarrierStands(env, enclosure, habitat.barrier);
        if (habitat.barrier.empty()) {
            continue;  // nowhere to work from
        }
        centres.push_back((World::TileCenter(enclosure.min) + World::TileCenter(enclosure.max)) * 0.5f);
        habitats.push_back(std::move(habitat));
    }
    const Pairing pairing(centres);
    for (std::size_t i = 0; i < habitats.size(); ++i) {
        const int mate = pairing.Mates()[i];
        if (mate >= 0 && static_cast<std::size_t>(mate) < i) {
            continue;  // engaged with its partner
        }
        const Stand start = habitats[i].barrier.front();
        StaffMember& keeper = add(StaffRole::Keeper, start.place, start.facing);
        for (const std::size_t index : {i, mate >= 0 ? static_cast<std::size_t>(mate) : i}) {
            if (!keeper.round.empty() && keeper.round.back().enclosure == habitats[index].id) {
                continue;
            }
            StaffStop stop;
            stop.enclosure = habitats[index].id;
            if (const int species = SpeciesOf(env, animals, stop.enclosure); species >= 0) {
                stop.feedHours = FeedingHoursOf(env.species.All()[static_cast<std::size_t>(species)].diet);
                for (std::size_t slot = 0; slot < stop.feedHours.size(); ++slot) {
                    for (const StaffStop& earlier : keeper.round) {
                        if (earlier.feedHours[slot] >= 0.0f &&
                            std::abs(earlier.feedHours[slot] - stop.feedHours[slot]) < kFeedingGap) {
                            stop.feedHours[slot] = earlier.feedHours[slot] + kFeedingGap;
                        }
                    }
                }
            }
            keeper.round.push_back(stop);
        }
        // The first plan moves on to the stop after this one: the first of the round.
        keeper.stop = static_cast<std::uint32_t>(keeper.round.size() - 1);
    }

    // A gardener for the flower beds, a cleaner for the bins: those they can get to.
    std::vector<PropStop> beds;
    std::vector<PropStop> bins;
    for (const Prop& prop : env.objects.Props()) {
        const PropDef& def = env.catalog.props[static_cast<std::size_t>(prop.def)];
        const bool bed = def.role == PropRole::Decoration && def.vegetationM2 > 0.0f;
        if (!bed && def.role != PropRole::Bin) {
            continue;
        }
        if (const auto stand =
                StandBeside(env, prop.position, {0.0f, 1.0f}, def.radiusM * prop.scale + 0.4f, {})) {
            (bed ? beds : bins).push_back({prop.id, stand->place});
        }
    }
    const auto engage = [&](StaffRole role, std::vector<PropStop>& stops, std::size_t most) {
        if (stops.empty()) {
            return;
        }
        RoundOf(stops, entrance, most);
        StaffMember& member = add(role, stops.front().place, 0.0f);
        for (const PropStop& at : stops) {
            StaffStop stop;
            stop.prop = at.prop;
            member.round.push_back(stop);
        }
        member.stop = static_cast<std::uint32_t>(member.round.size() - 1);
    };
    engage(StaffRole::Gardener, beds, kMostBeds);
    engage(StaffRole::Cleaner, bins, kMostBins);
}

void Staff::PlanStop(const Env& env, const Animals& animals, const Visitors& visitors,
                     StaffMember& member) const {
    member.tasks.clear();
    member.task = 0;
    member.working = false;
    member.ticksLeft = 0;
    member.route.clear();
    member.routeIndex = 0;

    const std::size_t stops = member.round.size();
    std::size_t next = (member.stop + 1) % stops;
    bool feeding = false;
    if (member.role == StaffRole::Keeper) {
        // A public feeding that is due comes before the order of the round.
        const float hour = env.time.Hour();
        const std::int64_t today = env.time.Day();
        for (std::size_t i = 0; i < stops && !feeding; ++i) {
            StaffStop& stop = member.round[(member.stop + 1 + i) % stops];
            for (std::size_t slot = 0; slot < stop.feedHours.size(); ++slot) {
                const float at = stop.feedHours[slot];
                if (at >= 0.0f && stop.fedDay[slot] != today && hour >= at - kFeedingEarly &&
                    hour < at + kFeedingLate) {
                    stop.fedDay[slot] = today;
                    next = (member.stop + 1 + i) % stops;
                    feeding = true;
                    break;
                }
            }
        }
    } else if (member.role == StaffRole::Cleaner) {
        // Fresh litter should not wait for a complete circuit of a large zoo. Prefer the
        // next bin which has a piece within the same 25 m catchment used by PlanBin.
        float shortestWalk = std::numeric_limits<float>::infinity();
        for (std::size_t i = 0; i < stops; ++i) {
            const Prop* bin = env.objects.FindProp(member.round[i].prop);
            if (bin == nullptr) {
                continue;
            }
            const bool dirty = std::any_of(
                visitors.LitterOnGround().begin(), visitors.LitterOnGround().end(), [&](const Litter& piece) {
                    return DistanceSquared(piece.position, bin->position) < kLitterReach * kLitterReach;
                });
            const float walk = DistanceSquared(member.position, bin->position);
            if (dirty && walk < shortestWalk) {
                shortestWalk = walk;
                next = i;
            }
        }
    }
    member.stop = static_cast<std::uint32_t>(next);
    const StaffStop& stop = member.round[next];
    switch (member.role) {
        case StaffRole::Keeper:
            PlanHabitat(env, animals, member, stop.enclosure, feeding);
            break;
        case StaffRole::Gardener:
            PlanBed(env, member, stop.prop);
            break;
        case StaffRole::Cleaner:
            PlanBin(env, visitors, member, stop.prop);
            break;
        case StaffRole::Count:
            break;
    }
    ++member.stopsDone;
    if (member.tasks.empty()) {
        // Nothing to do here (the habitat or the prop is gone): a pause, then on.
        member.tasks.push_back(
            Job(StaffActivity::Rest, StaffActivity::Walk, member.position, member.heading, 30.0f));
    }
}

}  // namespace zoo
