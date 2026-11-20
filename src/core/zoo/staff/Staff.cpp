// SPDX-License-Identifier: MIT
#include "zoo/staff/Staff.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <span>

#include "zoo/animals/AnimalTuning.hpp"
#include "zoo/staff/StaffInternal.hpp"

namespace zoo {

using namespace staff_detail;

namespace {

constexpr std::array<std::string_view, static_cast<std::size_t>(StaffRole::Count)> kRoleNames = {
    "keeper", "gardener", "cleaner"};
constexpr std::array<std::string_view, static_cast<std::size_t>(StaffRole::Count)> kRoleTitles = {
    "Keeper", "Gardener", "Cleaner"};
constexpr std::array<std::string_view, static_cast<std::size_t>(StaffActivity::Count)> kActivityNames = {
    "walk", "carry",   "push",  "fill_feeder", "fill_trough", "throw_feed", "rake",
    "hose", "inspect", "sweep", "empty_bin",   "tend",        "water",      "rest"};
constexpr std::array<std::string_view, static_cast<std::size_t>(StaffActivity::Count)> kActivityWords = {
    "on the way",
    "carries a bucket",
    "pushes the wheelbarrow",
    "fills the feeder",
    "fills the trough",
    "throws the feed",
    "rakes",
    "hoses down",
    "checks the fence",
    "sweeps the path",
    "empties the bin",
    "tends the flower bed",
    "waters the flowers",
    "takes a breather"};
constexpr std::array<std::string_view, static_cast<std::size_t>(StaffLockoutAction::Count)> kLockoutNames = {
    "none", "secure", "release"};

/// How fast a member turns to face the work or the way, in radians a second.
constexpr float kTurnRate = 4.0f;
/// What one job of raking takes off an enclosure's dirt (0..1).
constexpr float kRakeCleans = 0.5f;
/// How far around the broom the litter goes, in metres.
constexpr float kSweepReach = 5.0f;
/// How far a walker keeps from the rim of a tree, a rock or a feeder.
constexpr float kPropClearance = 0.3f;
/// A uniform is narrower than the clearance a visitor gives another adult.
constexpr float kStaffRadius = 0.32f;
constexpr float kVisitorRadius = 0.28f;
constexpr float kBodyClearance = 0.08f;
/// A shy animal gets one further body-clearance of reaction room while it retreats from staff.
constexpr float kShyAnimalYieldMargin = 0.08f;

struct Hasher {
    std::uint64_t value = 0xcbf29ce484222325ULL;

    void Add(std::uint64_t v) {
        for (int i = 0; i < 8; ++i) {
            value ^= (v >> (i * 8)) & 0xffu;
            value *= 0x100000001b3ULL;
        }
    }
    void Add(float v) { Add(static_cast<std::uint64_t>(std::bit_cast<std::uint32_t>(v))); }
    void Add(std::int64_t v) { Add(static_cast<std::uint64_t>(v)); }
    void Add(int v) { Add(static_cast<std::uint64_t>(static_cast<std::int64_t>(v))); }
    void Add(std::uint32_t v) { Add(static_cast<std::uint64_t>(v)); }
    void Add(bool v) { Add(static_cast<std::uint64_t>(v ? 1 : 0)); }
    void Add(Vec2 v) {
        Add(v.x);
        Add(v.z);
    }
};

float SpeedOf(StaffActivity approach) {
    switch (approach) {
        case StaffActivity::Carry:
            return Staff::kCarrySpeed;
        case StaffActivity::Push:
            return Staff::kPushSpeed;
        default:
            return Staff::kWalkSpeed;
    }
}

void NextTask(StaffMember& member) {
    ++member.task;
    member.working = false;
    member.ticksLeft = 0;
    member.route.clear();
    member.routeIndex = 0;
    member.speed = 0.0f;
}

/// Leads the walker round a tree, a rock or a feeder that `to` lies within and that is not
/// where they are going: along its rim, a tick's `step` further on the side the goal lies.
/// The result may be a place they must not stand on: the caller checks.
Vec2 AroundProps(const Env& env, Vec2 to, Vec2 goal, float step, std::vector<PropId>& scratch) {
    env.objects.PropsNear(to, 3.0f, scratch);
    for (const PropId id : scratch) {
        const Prop* prop = env.objects.FindProp(id);
        const PropDef& def = env.catalog.props[static_cast<std::size_t>(prop->def)];
        if (!def.blocksWalking) {
            continue;
        }
        const float keep = def.radiusM * prop->scale + kPropClearance;
        const Vec2 off = to - prop->position;
        if (LengthSquared(off) >= keep * keep || DistanceSquared(goal, prop->position) < keep * keep) {
            continue;
        }
        // Radially out alone would send who walks straight at the trunk back to where they
        // stood, for ever; so the rim is followed, always the same way round for one goal.
        const Vec2 radial = Normalized(off, {1.0f, 0.0f});
        const Vec2 tangent{-radial.z, radial.x};
        const float side = Dot(goal - prop->position, tangent) < 0.0f ? -1.0f : 1.0f;
        return prop->position + Normalized(radial + tangent * (side * step / keep), radial) * keep;
    }
    return to;
}

/// Slide a walking member around visible bodies in the same public region or habitat. Bodies
/// already overlapping are allowed to separate; a crowded shelter door must not trap anybody.
Vec2 AroundBodies(const Env& env, const Animals& animals, const Visitors& visitors,
                  std::span<const StaffMember> members, std::size_t self, Vec2 from, Vec2 to, Vec2 goal,
                  float distanceThisTick) {
    Vec2 step = to - from;
    if (LengthSquared(step) < 1.0e-8f) {
        return to;
    }
    const EnclosureId region = env.enclosures.At(World::TileAt(from));
    bool blocked = false;
    const auto consider = [&](Vec2 obstacle, float clearance, float tie, int pass) {
        const float before = DistanceSquared(from, obstacle);
        const float after = DistanceSquared(from + step, obstacle);
        if (after >= clearance * clearance || after >= before - 1.0e-5f) {
            return;
        }
        if (pass != 0) {
            blocked = true;
            return;
        }
        const Vec2 away = from - obstacle;
        const float gap = Length(away);
        const Vec2 radial = gap > 1.0e-4f ? away * (1.0f / gap) : Normalized(step) * -1.0f;
        const float inward = Dot(step, radial);
        if (inward >= 0.0f) {
            return;
        }
        step = step - radial * inward;
        if (LengthSquared(step) < 0.25f * distanceThisTick * distanceThisTick) {
            const Vec2 tangent{-radial.z, radial.x};
            float side = Dot(goal - obstacle, tangent) < 0.0f ? -1.0f : 1.0f;
            if (DistanceSquared(goal, obstacle) < 0.01f) {
                side = tie;
            }
            step = tangent * (side * distanceThisTick);
        }
    };
    for (int pass = 0; pass < 2; ++pass) {
        for (const Visitor& visitor : visitors.All()) {
            if (visitor.inside || env.enclosures.At(World::TileAt(visitor.position)) != region) {
                continue;
            }
            consider(visitor.position, kStaffRadius + kVisitorRadius + kBodyClearance,
                     visitor.id.value % 2 == 0 ? -1.0f : 1.0f, pass);
        }
        for (const Animal& animal : animals.All()) {
            if (!animal.InWorld() || animal.shelter || animal.species < 0 ||
                static_cast<std::size_t>(animal.species) >= env.species.All().size() ||
                env.enclosures.At(World::TileAt(animal.position)) != region) {
                continue;
            }
            const SpeciesDef& species = env.species.All()[static_cast<std::size_t>(animal.species)];
            const float radius = species.bodyRadiusM * BodyScale(animal, species, env.time) * 0.75f;
            const bool shy = species.shyness > tuning::kKeeperShynessThreshold;
            const float clearance =
                kStaffRadius + radius + kBodyClearance + (shy ? kShyAnimalYieldMargin : 0.0f);
            // The animal movement runs before staff movement. If a shy animal stepped into the
            // margin this tick, wait in place and let its existing keeper-retreat route clear it.
            if (shy && DistanceSquared(from, animal.position) < clearance * clearance) {
                blocked = true;
                continue;
            }
            consider(animal.position, clearance, animal.id.value % 2 == 0 ? -1.0f : 1.0f, pass);
        }
        for (std::size_t other = 0; other < members.size(); ++other) {
            if (other == self || env.enclosures.At(World::TileAt(members[other].position)) != region) {
                continue;
            }
            consider(members[other].position, 2.0f * kStaffRadius + kBodyClearance,
                     members[other].id.value % 2 == 0 ? -1.0f : 1.0f, pass);
        }
    }
    // A later obstacle can bend the step back into an earlier one. Validate the composed slide;
    // waiting for one tick is safer than letting the last avoidance undo the first.
    const auto unsafe = [&](Vec2 obstacle, float clearance) {
        return DistanceSquared(from + step, obstacle) < clearance * clearance &&
               DistanceSquared(from + step, obstacle) < DistanceSquared(from, obstacle) - 1.0e-5f;
    };
    for (const Visitor& visitor : visitors.All()) {
        if (!visitor.inside && env.enclosures.At(World::TileAt(visitor.position)) == region &&
            unsafe(visitor.position, kStaffRadius + kVisitorRadius + kBodyClearance)) {
            blocked = true;
        }
    }
    for (const Animal& animal : animals.All()) {
        if (!animal.InWorld() || animal.shelter || animal.species < 0 ||
            static_cast<std::size_t>(animal.species) >= env.species.All().size() ||
            env.enclosures.At(World::TileAt(animal.position)) != region) {
            continue;
        }
        const SpeciesDef& species = env.species.All()[static_cast<std::size_t>(animal.species)];
        const float radius = species.bodyRadiusM * BodyScale(animal, species, env.time) * 0.75f;
        const float margin = species.shyness > tuning::kKeeperShynessThreshold ? kShyAnimalYieldMargin : 0.0f;
        if (unsafe(animal.position, kStaffRadius + radius + kBodyClearance + margin)) {
            blocked = true;
        }
    }
    for (std::size_t other = 0; other < members.size(); ++other) {
        if (other != self && env.enclosures.At(World::TileAt(members[other].position)) == region &&
            unsafe(members[other].position, 2.0f * kStaffRadius + kBodyClearance)) {
            blocked = true;
        }
    }
    return blocked ? from : from + step;
}

/// How long a walk may take before it is given up, in ticks: twice the time the route
/// needs at this speed and a minute for the trees in the way.
int WalkBudget(Vec2 from, std::span<const Vec2> route, float speed) {
    float metres = 0.0f;
    Vec2 at = from;
    for (const Vec2 point : route) {
        metres += Distance(at, point);
        at = point;
    }
    return static_cast<int>(metres / (speed * kTickSeconds)) * 2 + 60 * kTicksPerSecond;
}

bool ThroughKeeperGate(const Env& env, EnclosureId enclosureId, Vec2 from, Vec2 to) {
    const Enclosure* enclosure = env.enclosures.Find(enclosureId);
    if (enclosure == nullptr) {
        return false;
    }
    const TileCoord a = World::TileAt(from);
    const TileCoord b = World::TileAt(to);
    for (const EdgeRef gate : enclosure->gates) {
        const TileCoord near{gate.x, gate.z};
        const TileCoord far = gate.horizontal ? TileCoord{gate.x, gate.z - 1} : TileCoord{gate.x - 1, gate.z};
        if ((a == near && b == far) || (a == far && b == near)) {
            return true;
        }
    }
    return false;
}

/// May the walker be moved from `from` to `to`: ground staff walk on, no fence crossed,
/// and no habitat a dangerous animal lives in.
bool MayStep(const Env& env, const Animals& animals, Vec2 from, Vec2 to) {
    const TileCoord a = World::TileAt(from);
    const TileCoord b = World::TileAt(to);
    if (a == b) {
        return true;
    }
    const EnclosureId fromEnclosure = env.enclosures.At(a);
    const EnclosureId toEnclosure = env.enclosures.At(b);
    const EnclosureId crossed = toEnclosure ? toEnclosure : fromEnclosure;
    if (crossed && animals.StaffLockout(crossed) && DangerSecuredForStaff(env, animals, crossed) &&
        ThroughKeeperGate(env, crossed, from, to)) {
        return env.nav.Passable(b, kStaffProfile);
    }
    if (!env.nav.Passable(b, kStaffProfile) || !env.nav.Walkable(from, to, kStaffProfile)) {
        return false;
    }
    const EnclosureId entered = env.enclosures.At(b);
    return !entered || entered == env.enclosures.At(a) || !HoldsDanger(env, animals, entered) ||
           DangerSecuredForStaff(env, animals, entered);
}

}  // namespace

std::string_view ToString(StaffRole v) {
    return v < StaffRole::Count ? kRoleNames[static_cast<std::size_t>(v)] : std::string_view{};
}
std::string_view DisplayName(StaffRole v) {
    return v < StaffRole::Count ? kRoleTitles[static_cast<std::size_t>(v)] : std::string_view{};
}
std::string_view ToString(StaffActivity v) {
    return v < StaffActivity::Count ? kActivityNames[static_cast<std::size_t>(v)] : std::string_view{};
}
std::string_view Describe(StaffActivity v) {
    return v < StaffActivity::Count ? kActivityWords[static_cast<std::size_t>(v)]
                                    : std::string_view{"at work"};
}
std::string_view ToString(StaffLockoutAction v) {
    return v < StaffLockoutAction::Count ? kLockoutNames[static_cast<std::size_t>(v)] : std::string_view{};
}

const StaffMember* Staff::Find(StaffId id) const {
    const auto it = std::lower_bound(members_.begin(), members_.end(), id,
                                     [](const StaffMember& m, StaffId wanted) { return m.id < wanted; });
    return it != members_.end() && it->id == id ? &*it : nullptr;
}

const StaffMember* Staff::KeeperOf(EnclosureId enclosure) const {
    if (!enclosure) {
        return nullptr;
    }
    for (const StaffMember& member : members_) {
        if (member.role != StaffRole::Keeper) {
            continue;
        }
        for (const StaffStop& stop : member.round) {
            if (stop.enclosure == enclosure) {
                return &member;
            }
        }
    }
    return nullptr;
}

std::array<float, 2> Staff::FeedingHours(EnclosureId enclosure) const {
    if (const StaffMember* keeper = KeeperOf(enclosure)) {
        for (const StaffStop& stop : keeper->round) {
            if (stop.enclosure == enclosure) {
                return stop.feedHours;
            }
        }
    }
    return {-1.0f, -1.0f};
}

std::optional<float> Staff::NextFeeding(const Env& env, EnclosureId enclosure) const {
    const StaffMember* keeper = KeeperOf(enclosure);
    if (keeper == nullptr) {
        return std::nullopt;
    }
    const float hour = env.time.Hour();
    const std::int64_t today = env.time.Day();
    std::optional<float> next;
    for (const StaffStop& stop : keeper->round) {
        if (stop.enclosure != enclosure) {
            continue;
        }
        for (std::size_t i = 0; i < stop.feedHours.size(); ++i) {
            const float at = stop.feedHours[i];
            if (at >= 0.0f && stop.fedDay[i] != today && hour < at + kFeedingLate && (!next || at < *next)) {
                next = at;
            }
        }
    }
    return next;
}

void Staff::Apply(Env& env, Animals& animals, Visitors& visitors, const StaffTask& task) {
    Prop* prop = task.prop ? env.objects.FindProp(task.prop) : nullptr;
    const PropRole role =
        prop != nullptr ? env.catalog.props[static_cast<std::size_t>(prop->def)].role : PropRole::Count;
    switch (task.work) {
        case StaffActivity::FillFeeder:
        case StaffActivity::ThrowFeed:
            if (role == PropRole::Feeder && animals.FoodFor(env, *prop) >= 0) {
                animals.RefillFeeder(env, task.prop);
            } else if (const Enclosure* fed = task.prop ? nullptr : env.enclosures.Find(task.enclosure)) {
                // A public feeding is also a full service visit: every feeder is topped up
                // and every trough gets fresh water before the keeper moves on.
                const std::vector<PropId> feeders = fed->feeders;
                for (const PropId id : feeders) {
                    const Prop* feeder = env.objects.FindProp(id);
                    if (feeder != nullptr && animals.FoodFor(env, *feeder) >= 0) {
                        animals.RefillFeeder(env, id);
                    }
                }
                const std::vector<PropId> troughs = fed->troughs;
                for (const PropId id : troughs) {
                    animals.RefillTrough(env, id);
                }
            }
            break;
        case StaffActivity::FillTrough:
        case StaffActivity::Hose:
            if (role == PropRole::WaterTrough) {
                animals.RefillTrough(env, task.prop);
            }
            break;
        case StaffActivity::Rake:
            if (task.enclosure) {
                animals.Clean(task.enclosure, kRakeCleans);
            }
            break;
        case StaffActivity::Sweep:
            visitors.CleanLitter(task.place, kSweepReach);
            break;
        case StaffActivity::EmptyBin:
            if (role == PropRole::Bin) {
                prop->amount = 0.0f;
            }
            break;
        default:
            break;
    }
}

void Staff::Step(Env& env, Animals& animals, Visitors& visitors) {
    std::vector<PropId> scratch;
    for (std::size_t memberIndex = 0; memberIndex < members_.size(); ++memberIndex) {
        StaffMember& member = members_[memberIndex];
        if (member.round.empty()) {
            continue;
        }
        if (member.task >= member.tasks.size()) {
            PlanStop(env, animals, visitors, member);
        }
        // A failed secure action must not degrade into remote work inside the habitat.
        // Skip every inside job until the outside release task (which is then a no-op).
        while (member.task < member.tasks.size()) {
            const StaffTask& candidate = member.tasks[member.task];
            const bool inside = candidate.enclosure &&
                                env.enclosures.At(World::TileAt(candidate.place)) == candidate.enclosure;
            if (!inside || !HoldsDanger(env, animals, candidate.enclosure) ||
                member.lockout == candidate.enclosure) {
                break;
            }
            NextTask(member);
        }
        if (member.task >= member.tasks.size()) {
            continue;
        }
        if (member.role == StaffRole::Cleaner && !member.working && member.route.empty() && member.ticksLeft == 0 &&
            member.tasks[member.task].work == StaffActivity::Sweep && member.tasks[member.task].prop) {
            RefreshSweep(env, visitors, member);
        }
        const StaffTask task = member.tasks[member.task];

        // The exact control point is presentation, not a safety condition. Once the
        // keeper has crossed out of the habitat, nobody remains exposed inside and
        // the group must not stay confined merely because a path to that point changed.
        if (task.lockout == StaffLockoutAction::Release && member.lockout == task.enclosure &&
            env.enclosures.At(member.position) != task.enclosure) {
            member.lockout = {};
            NextTask(member);
            member.activity = StaffActivity::Rest;
            continue;
        }

        if (member.working) {
            member.speed = 0.0f;
            member.activity = task.work;
            member.heading = TurnTowards(member.heading, task.facing, kTurnRate * kTickSeconds);
            if (member.ticksLeft == (std::max(task.ticks, 1) + 1) / 2) {
                if (task.lockout == StaffLockoutAction::Secure) {
                    if (!member.lockout && animals.SecureForStaff(env, task.enclosure)) {
                        member.lockout = task.enclosure;
                        animals.AddStaffLockout(task.enclosure);
                    }
                } else if (task.lockout == StaffLockoutAction::Release) {
                    if (member.lockout == task.enclosure) {
                        member.lockout = {};
                    }
                } else {
                    Apply(env, animals, visitors, task);
                }
            }
            if (--member.ticksLeft <= 0) {
                NextTask(member);
            }
            continue;
        }

        // On the way. What cannot be reached on foot is still done (the feeder is filled
        // as the zoo's automatic care would have filled it), and the member stays put.
        const float speed = SpeedOf(task.approach);
        if (member.routeIndex >= member.route.size()) {
            member.route.clear();
            member.routeIndex = 0;
            bool routeFound = true;
            if (Distance(member.position, task.place) > 0.05f) {
                // The two lockout jobs straddle the same open gate. Grid routing may prefer
                // a very long path-centre detour between their edge points; the direct,
                // explicitly walkable gate crossing is the intended route.
                if (member.lockout == task.enclosure && Distance(member.position, task.place) < 3.0f &&
                    ThroughKeeperGate(env, task.enclosure, member.position, task.place)) {
                    member.route.push_back(task.place);
                } else {
                    routeFound = env.nav.FindPath(member.position, task.place, kStaffProfile, member.route);
                }
            }
            if (!routeFound) {
                if (task.lockout == StaffLockoutAction::Release && member.lockout == task.enclosure) {
                    member.activity = StaffActivity::Rest;
                    member.ticksLeft = 0;
                    continue;
                }
                if (task.lockout == StaffLockoutAction::None) {
                    Apply(env, animals, visitors, task);
                }
                NextTask(member);
                member.activity = StaffActivity::Rest;
                continue;
            }
            member.ticksLeft = WalkBudget(member.position, member.route, speed);
        }
        if (--member.ticksLeft < 0) {
            // Walked far longer than the route is long (hemmed in between trees and a
            // fence): the job is done as automatic care would have done it, on to the next.
            if (task.lockout == StaffLockoutAction::Release && member.lockout == task.enclosure) {
                member.route.clear();
                member.routeIndex = 0;
                member.ticksLeft = 0;
                member.speed = 0.0f;
                member.activity = StaffActivity::Rest;
                continue;
            }
            if (task.lockout == StaffLockoutAction::None) {
                Apply(env, animals, visitors, task);
            }
            NextTask(member);
            member.activity = StaffActivity::Rest;
            continue;
        }
        const Vec2 before = member.position;
        Vec2 after = before;
        std::size_t index = member.routeIndex;
        bool arrived = member.route.empty() || FollowPath(member.route, index, after, speed * kTickSeconds);
        const Vec2 goal = index < member.route.size() ? member.route[index] : task.place;
        if (DistanceSquared(before, after) > 1.0e-8f) {
            Vec2 aside = AroundProps(env, after, goal, speed * kTickSeconds, scratch);
            // No further aside in a tick than a brisk step: who starts within a bush's
            // reach walks out of it and is not thrown out.
            const float reach = speed * kTickSeconds * 1.5f;
            if (const float far = Distance(before, aside); far > reach) {
                aside = before + (aside - before) * (reach / far);
            }
            if (!(aside == after) && MayStep(env, animals, before, aside)) {
                after = aside;
            }
            after = AroundBodies(env, animals, visitors, members_, memberIndex, before, after, goal,
                                 speed * kTickSeconds);
        }
        if (arrived && Distance(after, task.place) > 0.05f) {
            arrived = false;
        }
        if (!MayStep(env, animals, before, after)) {
            // The way is shut (the world changed under the route, a step aside left the
            // line, or a dangerous animal has moved in): another route from here, or none.
            member.speed = 0.0f;
            std::vector<Vec2> other;
            bool open = env.nav.FindPath(before, task.place, kStaffProfile, other);
            if (open && !other.empty()) {
                Vec2 probe = before;
                std::size_t first = 0;
                FollowPath(other, first, probe, speed * kTickSeconds);
                open = MayStep(env, animals, before, probe);
            }
            if (open) {
                member.route = std::move(other);
                member.routeIndex = 0;
            } else {
                if (task.lockout == StaffLockoutAction::Release && member.lockout == task.enclosure) {
                    member.route.clear();
                    member.routeIndex = 0;
                    member.ticksLeft = 0;
                } else {
                    if (task.lockout == StaffLockoutAction::None) {
                        Apply(env, animals, visitors, task);
                    }
                    NextTask(member);
                }
                member.activity = StaffActivity::Rest;
            }
            continue;
        }
        member.position = after;
        member.routeIndex = static_cast<std::uint32_t>(index);
        member.speed = speed;
        member.activity = task.approach;
        if (const Vec2 moved = after - before; LengthSquared(moved) > 1.0e-8f) {
            member.heading = TurnTowards(member.heading, HeadingOf(moved), kTurnRate * kTickSeconds);
        }
        if (arrived && member.role == StaffRole::Cleaner && task.work == StaffActivity::Sweep && task.prop) {
            RefreshSweep(env, visitors, member);
            if (Distance(after, member.tasks[member.task].place) > 0.05f) {
                // Litter may have appeared while the cleaner was walking to the place selected
                // at the bin. Walk to the fresh target before beginning the visible sweep.
                member.route.clear();
                member.routeIndex = 0;
                member.ticksLeft = 0;
                member.speed = 0.0f;
                continue;
            }
        }
        if (arrived) {
            member.route.clear();
            member.routeIndex = 0;
            member.working = true;
            member.ticksLeft = std::max(task.ticks, 1);
            member.speed = 0.0f;
        }
    }
}

std::uint64_t Staff::StateHash() const {
    Hasher h;
    h.Add(static_cast<std::uint64_t>(members_.size()));
    h.Add(nextId_);
    for (const StaffMember& m : members_) {
        h.Add(m.id.value);
        h.Add(static_cast<int>(m.role));
        h.Add(m.look);
        h.Add(static_cast<std::uint64_t>(m.round.size()));
        for (const StaffStop& stop : m.round) {
            h.Add(stop.enclosure.value);
            h.Add(stop.prop.value);
            for (std::size_t i = 0; i < stop.feedHours.size(); ++i) {
                h.Add(stop.feedHours[i]);
                h.Add(stop.fedDay[i]);
            }
        }
        h.Add(m.stop);
        h.Add(m.stopsDone);
        h.Add(m.position);
        h.Add(m.heading);
        h.Add(m.speed);
        h.Add(static_cast<int>(m.activity));
        h.Add(static_cast<std::uint64_t>(m.tasks.size()));
        for (const StaffTask& task : m.tasks) {
            h.Add(static_cast<int>(task.work));
            h.Add(static_cast<int>(task.approach));
            h.Add(task.place);
            h.Add(task.facing);
            h.Add(task.ticks);
            h.Add(task.enclosure.value);
            h.Add(task.prop.value);
            h.Add(static_cast<int>(task.lockout));
        }
        h.Add(m.task);
        h.Add(m.working);
        h.Add(m.ticksLeft);
        h.Add(static_cast<std::uint64_t>(m.route.size()));
        for (const Vec2 point : m.route) {
            h.Add(point);
        }
        h.Add(m.routeIndex);
        h.Add(m.lockout.value);
    }
    return h.value;
}

}  // namespace zoo
