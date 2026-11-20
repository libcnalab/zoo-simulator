// SPDX-License-Identifier: MIT
// Carrying a plan out: walking there, and what happens at the fence, in the queue,
// at the counter, on the bench.

#include <algorithm>
#include <cmath>

#include "zoo/visitors/VisitorInternal.hpp"

namespace zoo::visitors_detail {

using namespace visitor_tuning;

namespace {

/// How good an animal is to watch for what it is doing.
float ShowOf(AnimalActivity activity) {
    switch (activity) {
        case AnimalActivity::Sleep:
            return 0.55f;
        case AnimalActivity::Rest:
            return 0.75f;
        case AnimalActivity::Pace:
            return 0.6f;
        case AnimalActivity::Play:
        case AnimalActivity::Socialize:
        case AnimalActivity::Vocalize:
            return 1.2f;
        default:
            return 1.0f;
    }
}

}  // namespace

void Stepper::SetActivity(Group g, VisitorActivity activity) {
    for (std::size_t i = g.first; i < g.end; ++i) {
        List()[i].activity = activity;
    }
}

void Stepper::Think(Group g, VisitorThought thought) {
    std::vector<Visitor>& list = List();
    const Visitor& lead = list[g.first];
    if (lead.thought == thought && now - lead.thoughtTick < 600) {
        return;  // still thinking that
    }
    for (std::size_t i = g.first; i < g.end; ++i) {
        list[i].thought = thought;
        list[i].thoughtTick = now;
    }
    Event event;
    event.type = EventType::VisitorThought;
    event.visitor = lead.id;
    event.position = lead.position;
    event.detail = static_cast<int>(thought);
    env.Emit(event);
}

void Stepper::Cheer(Group g, float amount) {
    for (std::size_t i = g.first; i < g.end; ++i) {
        Visitor& v = List()[i];
        v.happiness = Clamp01(v.happiness + amount);
    }
}

bool Stepper::InPlanting(Vec2 position) const {
    return env.enclosures.RegionAt(World::TileAt(position)) == Region::StandOff;
}

void Stepper::DropLitter(Vec2 position) {
    if (InPlanting(position) ||
        all.litter_.size() >= Visitors::kMaxLitter) {
        return;
    }
    all.litter_.push_back({position, now});
    ++scr.litterEdits;
    Event event;
    event.type = EventType::VisitorLittered;
    event.position = position;
    env.Emit(event);
}

void Stepper::Finish(Group g) {
    std::vector<Visitor>& list = List();
    Visitor& lead = list[g.first];
    if (lead.goal != VisitorGoal::None) {
        lead.previousGoal = lead.goal;
    }
    lead.served = false;
    lead.reroute = false;
    lead.activityUntil = now;
    for (std::size_t i = g.first; i < g.end; ++i) {
        Visitor& v = list[i];
        v.goal = VisitorGoal::None;
        v.activity = VisitorActivity::Idle;
        v.inside = {};
        v.seat = {};
        v.slot = 0;
        v.targetEnclosure = {};
        v.targetBuilding = {};
        v.targetProp = {};
        v.route.clear();
        v.routeIndex = 0;
        v.speed = 0.0f;
    }
}

// ---- walking ------------------------------------------------------------------------------

void Stepper::Move(Group g) {
    std::vector<Visitor>& list = List();
    Visitor& lead = list[g.first];
    const Navigator& nav = env.nav;
    const bool fleeing = lead.activity == VisitorActivity::Fleeing;
    // A group walks at the pace of its slowest, and its leader no faster than those who
    // are still on their way can follow.
    float pace = 10.0f;
    float lag = 0.0f;
    for (std::size_t i = g.first; i < g.end; ++i) {
        const Visitor& v = list[i];
        pace = std::min(pace, PaceOf(v));
        if (v.routeIndex < v.route.size()) {
            lag = std::max(lag, DistanceSquared(v.position, lead.position));
        }
    }
    if (fleeing) {
        pace *= kFleeFactor;
    }
    const bool leadWalks = lead.routeIndex < lead.route.size();
    const Vec2 leadWay = HeadingVector(lead.heading);
    const float leadLeft = Distance(lead.position, lead.target);
    const float seconds = static_cast<float>(now) * kTickSeconds;
    bool everybody = true;
    for (std::size_t i = g.first; i < g.end; ++i) {
        Visitor& v = list[i];
        const std::size_t count = v.route.size();
        if (v.routeIndex >= count) {
            v.speed = 0.0f;
            v.heading = TurnTowards(v.heading, v.face, kTurnRadiansPerSecond * kTickSeconds);
            continue;
        }
        // How fast it would like to go.
        float want = pace;
        const int k = static_cast<int>(i - g.first);
        if (k == 0) {
            if (lag > 16.0f) {
                want *= 0.6f;  // waits for the stragglers
            }
        } else if (leadWalks && DistanceSquared(v.position, lead.position) < 36.0f) {
            // Keeps its place: beside the leader or in a row behind it. A child runs a few
            // steps ahead now and then and lets the others catch up.
            float place = -kRowM * static_cast<float>(k / 2);
            if (v.child && !fleeing) {
                const float period = 9.0f + 6.0f * Hash01(v.look, 13);
                const float at = seconds / period + Hash01(v.look, 14);
                const float through = at - std::floor(at);
                if (through < 0.35f) {
                    place += kChildAheadM * std::sin(kPi * through / 0.35f);
                }
            }
            const float along = Dot(v.position - lead.position, leadWay);
            want *= std::clamp(1.0f + 0.9f * (place - along), 0.55f, 1.45f);
        } else {
            const float left = Distance(v.position, v.target);
            if (left > leadLeft + 1.5f) {
                want *= 1.35f;  // catches up
            } else if (left < leadLeft - 1.5f) {
                want *= 0.75f;
            }
        }
        // Rounds the corner: on to the next waypoint as soon as the way to it is open.
        std::size_t index = v.routeIndex;
        Vec2 to = v.route[index] - v.position;
        float distance = Length(to);
        while (index + 1 < count && distance < kCornerM) {
            if (distance > kReachM && !nav.Walkable(v.position, v.route[index + 1], kWalker)) {
                break;
            }
            ++index;
            to = v.route[index] - v.position;
            distance = Length(to);
        }
        const bool last = index + 1 == count;
        const Vec2 way = distance > 1.0e-4f ? to * (1.0f / distance) : HeadingVector(v.heading);
        const auto entersBystander = [&](Vec2 next) {
            return all.hasBystander_ &&
                   DistanceSquared(next, all.bystander_) < kBystanderBodyM * kBystanderBodyM &&
                   DistanceSquared(next, all.bystander_) < DistanceSquared(v.position, all.bystander_);
        };
        const auto entersStandingVisitor = [&](Vec2 next) {
            const Sites& sites = scr.sites;
            const TileCoord low = World::TileAt(next - Vec2{kQueueBodyM, kQueueBodyM});
            const TileCoord high = World::TileAt(next + Vec2{kQueueBodyM, kQueueBodyM});
            for (int z = std::max(0, low.z); z <= std::min(sites.depth - 1, high.z); ++z) {
                for (int x = std::max(0, low.x); x <= std::min(sites.width - 1, high.x); ++x) {
                    const std::size_t tile = sites.TileIndex({x, z});
                    for (std::int32_t n = scr.cellHead[tile]; n >= 0;
                         n = scr.cellNext[static_cast<std::size_t>(n)]) {
                        const std::size_t otherIndex = static_cast<std::size_t>(n);
                        if ((otherIndex >= g.first && otherIndex < g.end) ||
                            list[otherIndex].activity == VisitorActivity::Inside ||
                            Travelling(list[otherIndex].activity)) {
                            continue;
                        }
                        const Vec2 other = list[otherIndex].position;
                        if (DistanceSquared(next, other) < kQueueBodyM * kQueueBodyM &&
                            DistanceSquared(next, other) < DistanceSquared(v.position, other)) {
                            return true;
                        }
                    }
                }
            }
            return false;
        };
        // Steers at the waypoint and around people in the way. Near its own place it normally
        // settles directly, unless that place has become occupied since the route was planned.
        float slow = 1.0f;
        Vec2 wish = way;
        if (!last || distance > kSettleM || entersBystander(v.route[index]) ||
            entersStandingVisitor(v.route[index])) {
            const float push = Dodge(i, g, way, slow);
            wish = Normalized(way + Vec2{-way.z, way.x} * (0.9f * push), way);
        }
        const float wishHeading = HeadingOf(wish);
        const float error = std::abs(WrapAngle(wishHeading - v.heading));
        want *= slow * std::max(0.12f, std::cos(error));  // turns before it strides out
        if (last && !fleeing) {
            want = std::min(want, std::sqrt(2.0f * kBrake * distance) + 0.25f);  // slows to stop
        }
        const float gain = fleeing ? 3.0f * kGainSpeed : kGainSpeed;
        v.speed += std::clamp(want - v.speed, -kDropSpeed * kTickSeconds, gain * kTickSeconds);
        const float standing = 1.0f - std::min(v.speed / std::max(pace, 0.1f), 1.0f);
        const float turn = (fleeing ? 2.0f : 1.0f) * (kTurnWalking + kTurnOnTheSpot * standing);
        v.heading = TurnTowards(v.heading, wishHeading, turn * kTickSeconds);
        const float stride = v.speed * kTickSeconds;
        if (last && distance <= std::max(stride, 0.03f) && !entersBystander(v.route[index]) &&
            !entersStandingVisitor(v.route[index])) {
            v.position = v.route[index];
            v.speed = 0.0f;
            index = count;
        } else {
            // Goes where it looks, so long as that crosses no fence and leaves no paving;
            // otherwise, and for the last steps, straight at the waypoint as the route was laid
            // (which may end off the paving: a seat, a place on the grass by a bench).
            const Vec2 steered = v.position + HeadingVector(v.heading) * stride;
            Vec2 next = distance >= kDirectM && Clear(v.position, steered)
                            ? steered
                            : v.position + way * std::min(stride, distance);
            if (InPlanting(next) && !InPlanting(v.position)) {
                // The straight line cuts the corner of a stand-off strip: round it along one
                // axis, the longer part of the step first, or wait a tick.
                const Vec2 alongX{next.x, v.position.z};
                const Vec2 alongZ{v.position.x, next.z};
                const bool xFirst = std::abs(next.x - v.position.x) >= std::abs(next.z - v.position.z);
                const Vec2 first = xFirst ? alongX : alongZ;
                const Vec2 second = xFirst ? alongZ : alongX;
                next = !InPlanting(first) ? first : !InPlanting(second) ? second : v.position;
            }
            if (entersBystander(next)) {
                const Vec2 away = v.position - all.bystander_;
                const Vec2 radial = Normalized(away);
                const Vec2 step = next - v.position;
                const float inward = Dot(step, radial);
                next = inward < 0.0f ? next - radial * inward : next;
                if (DistanceSquared(next, all.bystander_) < kBystanderBodyM * kBystanderBodyM) {
                    next = v.position;
                }
                v.speed = Distance(v.position, next) / kTickSeconds;
            }
            if (entersStandingVisitor(next)) {
                next = v.position;
                v.speed = 0.0f;
            }
            v.position = next;
            everybody = false;
        }
        v.routeIndex = static_cast<std::uint32_t>(index);
    }
    if (everybody) {
        GroupArrived(g);
    }
}

float Stepper::Dodge(std::size_t self, Group g, Vec2 way, float& slow) const {
    const std::vector<Visitor>& list = all.visitors_;
    const Sites& sites = scr.sites;
    const Visitor& me = list[self];
    const Vec2 right{-way.z, way.x};
    const float mine = std::max(me.speed, 0.5f);
    float push = 0.0f;
    // `shoulder` is how near to my line somebody has to be to matter, `tie` the side I take
    // when it is dead ahead and goes my way.
    const auto meet = [&](Vec2 position, float heading, float speed, float shoulder, float tie,
                          float weight) {
        const Vec2 d = position - me.position;
        const float along = Dot(d, way);
        if (along < -0.3f || along > kAheadM) {
            return;
        }
        const float side = Dot(d, right);
        if (std::abs(side) >= shoulder) {
            return;
        }
        const float theirs = speed > 0.0f ? speed * Dot(HeadingVector(heading), way) : 0.0f;
        const bool with = theirs > 0.6f * mine;
        if (with && along > kFollowM) {
            return;  // goes my way: followed, not walked around
        }
        const bool oncoming = theirs < -0.3f;
        const float near = 1.0f - std::max(along, 0.0f) / kAheadM;
        const float away = std::abs(side) > 0.06f ? (side > 0.0f ? -1.0f : 1.0f) : (oncoming ? 1.0f : tie);
        push += away * weight * (oncoming ? 1.5f : 1.0f) * near * (1.0f - std::abs(side) / shoulder);
        if (!with && along > 0.0f && along < 0.7f && std::abs(side) < 0.34f) {
            slow = std::min(slow, std::max(0.45f, along / 0.7f));  // about to walk into it
        }
    };
    // The tiles its next steps and its shoulders reach.
    const Vec2 ahead = me.position + way * kAheadM;
    const TileCoord low = World::TileAt(
        {std::min(me.position.x, ahead.x) - kShoulderM, std::min(me.position.z, ahead.z) - kShoulderM});
    const TileCoord high = World::TileAt(
        {std::max(me.position.x, ahead.x) + kShoulderM, std::max(me.position.z, ahead.z) + kShoulderM});
    for (int z = std::max(low.z, 0); z <= std::min(high.z, sites.depth - 1); ++z) {
        for (int x = std::max(low.x, 0); x <= std::min(high.x, sites.width - 1); ++x) {
            const std::size_t tile = sites.TileIndex({x, z});
            for (std::int32_t n = scr.cellHead[tile]; n >= 0; n = scr.cellNext[static_cast<std::size_t>(n)]) {
                const std::size_t j = static_cast<std::size_t>(n);
                if (j == self) {
                    continue;
                }
                const Visitor& other = list[j];
                const bool own = j >= g.first && j < g.end;
                meet(other.position, other.heading, other.speed, own ? kOwnShoulderM : kShoulderM,
                     self < j ? -1.0f : 1.0f, 1.0f);
            }
        }
    }
    if (all.hasBystander_) {
        meet(all.bystander_, 0.0f, 0.0f, 0.75f, 1.0f, 1.6f);
    }
    const EnclosureId region = env.enclosures.At(World::TileAt(me.position));
    for (const Vec2 position : all.movingObstacles_) {
        // A keeper behind a habitat fence can be close in plan view without sharing the
        // visitor's walking space. Only bodies in the same public region or habitat matter.
        if (env.enclosures.At(World::TileAt(position)) == region) {
            meet(position, 0.0f, 0.0f, 0.68f, 1.0f, 1.35f);
        }
    }
    return std::clamp(push, -1.0f, 1.0f);
}

bool Stepper::Clear(Vec2 from, Vec2 to) const {
    const TileCoord a = World::TileAt(from);
    const TileCoord b = World::TileAt(to);
    if (a == b) {
        return true;
    }
    const int dx = b.x - a.x;
    const int dz = b.z - a.z;
    if (std::abs(dx) > 1 || std::abs(dz) > 1) {
        return false;
    }
    const Navigator& nav = env.nav;
    const Dir sideX = dx > 0 ? Dir::East : Dir::West;
    const Dir sideZ = dz > 0 ? Dir::South : Dir::North;
    if (dz == 0) {
        return nav.CanStep(a, sideX, kWalker);
    }
    if (dx == 0) {
        return nav.CanStep(a, sideZ, kWalker);
    }
    // Across a corner: both ways round it must be open.
    return nav.CanStep(a, sideX, kWalker) && nav.CanStep(a + Step(sideX), sideZ, kWalker) &&
           nav.CanStep(a, sideZ, kWalker) && nav.CanStep(a + Step(sideZ), sideX, kWalker);
}

void Stepper::Aim(Group g) {
    std::vector<Visitor>& list = List();
    const Visitor& lead = list[g.first];
    const Sites& sites = scr.sites;
    const int view = sites.ViewIndex(lead.targetEnclosure);
    if (view < 0) {
        return;
    }
    const ViewSite& site = sites.views[static_cast<std::size_t>(view)];
    const std::int64_t every = Ticks(kAimSeconds);
    bool gathered = false;
    for (std::size_t i = g.first; i < g.end; ++i) {
        Visitor& v = list[i];
        // Each in its own time, so that a row of people does not turn its heads as one.
        if ((now + static_cast<std::int64_t>(HashU32(v.look, 51) % static_cast<std::uint32_t>(every))) % every != 0) {
            continue;
        }
        const TileCoord tile = World::TileAt(v.target);
        const auto spot = std::find_if(site.spots.begin(), site.spots.end(),
                                       [&](const ViewingSpot& s) { return s.tile == tile; });
        if (spot == site.spots.end()) {
            continue;
        }
        if (!gathered) {
            gathered = true;
            scr.beasts.clear();
            for (const Animal& animal : fauna.All()) {
                if (animal.home == site.enclosure && animal.Visible() && !animal.loose) {
                    scr.beasts.push_back(animal.position);
                }
            }
        }
        const float across = HeadingOf(ToVec(Step(spot->side)));
        // Mostly what the group looks at together, sometimes an animal of its own; of those
        // that can be looked at without turning away from the fence, the nearer the likelier.
        const std::uint32_t turnOf = static_cast<std::uint32_t>(now / every);
        const std::uint32_t who = Hash01(v.look, 52 + turnOf) < 0.6f ? lead.look : v.look;
        float best = -1.0f;
        float aim = across;
        for (std::size_t b = 0; b < scr.beasts.size(); ++b) {
            const Vec2 d = scr.beasts[b] - v.position;
            const float off = WrapAngle(HeadingOf(d) - across);
            const float distance = Length(d);
            if (distance > kViewRangeM || std::abs(off) > 1.2f) {
                continue;
            }
            const float score = (0.3f + Hash01(who, 53 + turnOf * 31u + static_cast<std::uint32_t>(b))) /
                                (6.0f + distance) * (std::abs(off) > kAimSpread ? 0.4f : 1.0f);
            if (score > best) {
                best = score;
                aim = across + std::clamp(off, -kAimSpread, kAimSpread);
            }
        }
        v.face = WrapAngle(aim);
    }
}

void Stepper::GroupArrived(Group g) {
    std::vector<Visitor>& list = List();
    Visitor& lead = list[g.first];
    Rng& rng = all.rng_;
    for (std::size_t i = g.first; i < g.end; ++i) {
        list[i].speed = 0.0f;
    }
    const bool needsStandingRoom = lead.goal == VisitorGoal::Watch || lead.goal == VisitorGoal::Eat ||
                                   lead.goal == VisitorGoal::Rest || lead.goal == VisitorGoal::Read;
    if (needsStandingRoom) {
        for (std::size_t i = g.first; i < g.end; ++i) {
            if (list[i].seat) {
                continue;
            }
            for (std::size_t k = 0; k < list.size(); ++k) {
                const Visitor& other = list[k];
                if ((k >= g.first && k < g.end) || other.activity == VisitorActivity::Inside || other.seat ||
                    Travelling(other.activity)) {
                    continue;
                }
                if (DistanceSquared(list[i].position, other.position) < kRoomM * kRoomM) {
                    Finish(g);
                    return;
                }
            }
        }
    }
    switch (lead.goal) {
        case VisitorGoal::Enter:
            if (!all.open_ || Closing()) {
                // The gate is shut: back down the road.
                if (!PlanLeave(g, VisitorGoal::Leave)) {
                    Depart(g, false);
                }
                return;
            }
            PayTickets(g);
            Finish(g);
            return;
        case VisitorGoal::Watch:
            SetActivity(g, VisitorActivity::Watching);
            lead.activityUntil =
                now + Ticks(all.pace_.linger * (kWatchSeconds + kWatchSpreadSeconds * rng.NextFloat()));
            return;
        case VisitorGoal::Food:
        case VisitorGoal::Drink:
            SetActivity(g, VisitorActivity::Queueing);
            lead.queuedTick = now;
            lead.served = false;
            return;
        case VisitorGoal::Restroom:
        case VisitorGoal::Shop:
        case VisitorGoal::Learn:
            SetActivity(g, VisitorActivity::Queueing);
            lead.queuedTick = now;
            ActBuilding(g);
            return;
        case VisitorGoal::Rest:
            for (std::size_t i = g.first; i < g.end; ++i) {
                list[i].activity = list[i].seat ? VisitorActivity::Sitting : VisitorActivity::Idle;
            }
            lead.activityUntil = now + Ticks(all.pace_.linger * kRestSeconds * (1.0f + rng.NextFloat()));
            return;
        case VisitorGoal::Eat:
            SetActivity(g, VisitorActivity::Eating);
            lead.activityUntil =
                now + Ticks(all.pace_.linger * kEatSeconds * (1.0f + 0.6f * rng.NextFloat()));
            return;
        case VisitorGoal::Bin: {
            SetActivity(g, VisitorActivity::Idle);
            Prop* bin = env.objects.FindProp(lead.targetProp);
            const int index = bin != nullptr ? bin->def : -1;
            const float capacity = index >= 0 && index < static_cast<int>(env.catalog.props.size())
                                       ? env.catalog.props[static_cast<std::size_t>(index)].capacity
                                       : 0.0f;
            for (; lead.litter > 0; --lead.litter) {
                if (bin != nullptr && bin->amount + 1.0f <= capacity) {
                    bin->amount += 1.0f;
                } else {
                    DropLitter(lead.position + Vec2{rng.Range(-0.5f, 0.5f), rng.Range(-0.5f, 0.5f)});
                }
            }
            lead.activityUntil = now + Ticks(1.0f);
            return;
        }
        case VisitorGoal::Read:
            SetActivity(g, VisitorActivity::Reading);
            lead.activityUntil =
                now + Ticks(all.pace_.linger * kReadSeconds * (1.0f + 0.5f * rng.NextFloat()));
            return;
        case VisitorGoal::Leave:
            Depart(g, true);
            return;
        case VisitorGoal::Flee:
            Depart(g, false);
            return;
        default:
            Finish(g);
            return;
    }
}

void Stepper::PayTickets(Group g) {
    std::vector<Visitor>& list = List();
    Visitor& lead = list[g.first];
    const Money price = all.ticketPrice_;
    Money total = 0;
    for (std::size_t i = g.first; i < g.end; ++i) {
        Visitor& v = list[i];
        const Money own = v.child ? price / 2 : price;
        total += own;
        v.ticketPaid = true;
        Event event;
        event.type = EventType::VisitorArrived;
        event.visitor = v.id;
        event.position = v.position;
        event.detail = static_cast<int>(own);
        env.Emit(event);
    }
    lead.spent += total;
    if (env.finance != nullptr) {
        if (total > 0) {
            env.finance->Earn(Account::Tickets, total);
        }
        env.finance->CountVisitor(g.Size());
    }
    all.arrivedToday_ += g.Size();
    all.arrivedEver_ += g.Size();
    // What they make of the price sets the mood of the visit.
    const float fair = static_cast<float>(Visitors::FairTicketPrice(AttractionNow()));
    const float ratio = static_cast<float>(price) / fair;
    if (ratio > 1.15f) {
        Cheer(g, -0.12f * std::min(ratio - 1.0f, 1.0f));
        Think(g, VisitorThought::TooExpensive);
    } else if (ratio < 0.7f) {
        Cheer(g, 0.03f);
    }
}

// ---- at the destination -------------------------------------------------------------------

void Stepper::Act(Group g) {
    std::vector<Visitor>& list = List();
    Visitor& lead = list[g.first];
    switch (lead.goal) {
        case VisitorGoal::Food:
        case VisitorGoal::Drink:
            ActQueue(g);
            return;
        case VisitorGoal::Restroom:
        case VisitorGoal::Shop:
        case VisitorGoal::Learn:
            ActBuilding(g);
            return;
        default:
            break;
    }
    // People who look at animals follow them with their eyes, unhurried.
    const bool watching = lead.goal == VisitorGoal::Watch;
    if (watching) {
        // Somebody can settle here after this viewing place was reserved.  Give them room and
        // choose another view instead of continuing to stand shoulder to shoulder.
        for (std::size_t i = g.first; i < g.end; ++i) {
            for (std::size_t k = 0; k < list.size(); ++k) {
                const Visitor& other = list[k];
                if ((k >= g.first && k < g.end) || other.activity == VisitorActivity::Inside ||
                    Travelling(other.activity) || other.seat || other.speed != 0.0f) {
                    continue;
                }
                if (DistanceSquared(list[i].position, other.position) < kRoomM * kRoomM) {
                    Finish(g);
                    return;
                }
            }
        }
        Aim(g);
    }
    for (std::size_t i = g.first; i < g.end; ++i) {
        Visitor& v = list[i];
        v.speed = 0.0f;
        // A look that swings over eases into its place; somebody who has just walked up turns round.
        const float turn =
            watching ? std::clamp(kAimEase * std::abs(WrapAngle(v.face - v.heading)), kAimTurn, kTurnRadiansPerSecond)
                     : kTurnRadiansPerSecond;
        v.heading = TurnTowards(v.heading, v.face, turn * kTickSeconds);
    }
    if (now < lead.activityUntil) {
        return;
    }
    switch (lead.goal) {
        case VisitorGoal::Watch:
            Watched(g);
            break;
        case VisitorGoal::Rest:
            for (std::size_t i = g.first; i < g.end; ++i) {
                list[i].tiredness = Clamp01(list[i].tiredness - (list[i].seat ? 0.5f : 0.2f));
            }
            break;
        case VisitorGoal::Read:
            for (std::size_t i = g.first; i < g.end; ++i) {
                list[i].education = Clamp01(list[i].education + 0.12f);
            }
            break;
        default:
            break;
    }
    Finish(g);
}

void Stepper::ActQueue(Group g) {
    const Sites& sites = scr.sites;
    std::vector<Visitor>& list = List();
    Visitor& lead = list[g.first];
    const int s = sites.ServiceIndex(lead.targetBuilding);
    Building* building = s >= 0 ? env.objects.FindBuilding(lead.targetBuilding) : nullptr;
    if (building == nullptr || !building->open) {
        Finish(g);  // sold, or shut in their faces
        return;
    }
    const ServiceSite& site = sites.services[static_cast<std::size_t>(s)];
    if (site.line.empty()) {
        Finish(g);
        return;
    }
    // Shuffle up as the line shortens.
    const float turn = kTurnRadiansPerSecond * kTickSeconds;
    const int ahead = scr.ahead[g.first];
    bool bystander = false;
    for (std::size_t i = g.first; i < g.end && !bystander; ++i) {
        const Visitor& v = list[i];
        const std::size_t place =
            std::min(static_cast<std::size_t>(ahead) + (i - g.first), site.line.size() - 1);
        const Vec2 to = site.line[place] - v.position;
        const float distance = Length(to);
        if (distance <= 0.03f) {
            continue;
        }
        const float step = std::min(distance, 0.9f * kTickSeconds);
        const Vec2 next = v.position + to * (step / distance);
        for (std::size_t other = 0; other < list.size() && !bystander; ++other) {
            if ((other >= g.first && other < g.end) || list[other].activity == VisitorActivity::Inside ||
                Travelling(list[other].activity) || list[other].goal == VisitorGoal::Food ||
                list[other].goal == VisitorGoal::Drink) {
                continue;
            }
            bystander = list[other].speed == 0.0f &&
                        DistanceSquared(next, list[other].position) < kQueueBodyM * kQueueBodyM;
        }
    }
    for (std::size_t i = g.first; i < g.end; ++i) {
        Visitor& v = list[i];
        const std::size_t place =
            std::min(static_cast<std::size_t>(ahead) + (i - g.first), site.line.size() - 1);
        const Vec2 to = site.line[place] - v.position;
        const float distance = Length(to);
        if (bystander) {
            v.speed = 0.0f;
        } else if (distance > 0.03f) {
            const float step = std::min(distance, 0.9f * kTickSeconds);
            v.position += to * (step / distance);
            v.speed = step / kTickSeconds;
            v.heading = TurnTowards(v.heading, HeadingOf(to), turn);
        } else {
            v.speed = 0.0f;
            const float face = place == 0 ? site.face : HeadingOf(site.line[place - 1] - site.line[place]);
            v.heading = TurnTowards(v.heading, face, turn);
        }
    }
    const BuildingDef& def = env.catalog.buildings[static_cast<std::size_t>(site.def)];
    if (!lead.served) {
        if (scr.rank[g.first] < site.counters) {
            lead.served = true;
            lead.activityUntil =
                now + Ticks(def.serviceSeconds * kServiceShare + 0.5f * static_cast<float>(g.Size() - 1));
        } else if (now - lead.queuedTick > Ticks(kQueuePatienceSeconds)) {
            lead.Avoid(site.id);
            Think(g, VisitorThought::TooCrowded);
            Cheer(g, -0.03f);
            Finish(g);
        }
        return;
    }
    if (now >= lead.activityUntil) {
        Purchase(g, site, *building);
    }
}

void Stepper::Purchase(Group g, const ServiceSite& site, Building& building) {
    std::vector<Visitor>& list = List();
    Visitor& lead = list[g.first];
    const BuildingDef& def = env.catalog.buildings[static_cast<std::size_t>(site.def)];
    const bool drink = site.role == BuildingRole::DrinkStall;
    const Money price = building.itemPrice;
    int bought = 0;
    for (std::size_t i = g.first; i < g.end; ++i) {
        Visitor& v = list[i];
        const float need = drink ? v.thirst : v.hunger;
        if (need < 0.25f) {
            continue;
        }
        // The hungrier, the more they will pay; some are tighter than others.
        const float willing =
            static_cast<float>(def.itemPrice) * (1.15f + 0.5f * need + 0.4f * Hash01(v.look, 21));
        if (static_cast<float>(price) > willing || price > lead.cash) {
            continue;
        }
        lead.cash -= price;
        lead.spent += price;
        if (env.finance != nullptr) {
            if (price > 0) {
                env.finance->Earn(Account::FoodSales, price);
            }
            if (def.itemCost > 0) {
                env.finance->Pay(Account::Goods, def.itemCost);
            }
        }
        ++building.servedToday;
        ++building.servedTotal;
        v.hunger = Clamp01(v.hunger - def.hungerRelief);
        v.thirst = Clamp01(v.thirst - def.thirstRelief);
        if (drink) {
            v.bladder = Clamp01(v.bladder + 0.25f);
        }
        v.happiness = Clamp01(v.happiness + def.fun * 0.35f);
        if (lead.litter < 250) {
            ++lead.litter;
        }
        Event event;
        event.type = EventType::VisitorBought;
        event.visitor = v.id;
        event.building = building.id;
        event.position = v.position;
        event.detail = static_cast<int>(price);
        env.Emit(event);
        ++bought;
    }
    if (bought == 0) {
        Think(g, VisitorThought::TooExpensive);
        Cheer(g, -0.03f);
        lead.unmet |= drink ? kUnmetDrink : kUnmetFood;
        lead.Avoid(site.id);
        Finish(g);
        return;
    }
    PlanEat(g, site);  // without a way to a seat they have it where they stand
}

void Stepper::ActBuilding(Group g) {
    const Sites& sites = scr.sites;
    std::vector<Visitor>& list = List();
    Visitor& lead = list[g.first];
    Rng& rng = all.rng_;
    const int s = sites.ServiceIndex(lead.targetBuilding);
    Building* building = s >= 0 ? env.objects.FindBuilding(lead.targetBuilding) : nullptr;
    if (building == nullptr) {
        Finish(g);
        return;
    }
    const ServiceSite& site = sites.services[static_cast<std::size_t>(s)];
    const BuildingDef& def = env.catalog.buildings[static_cast<std::size_t>(site.def)];

    if (lead.activity != VisitorActivity::Inside) {
        // At the door.
        const float turn = kTurnRadiansPerSecond * kTickSeconds;
        for (std::size_t i = g.first; i < g.end; ++i) {
            list[i].speed = 0.0f;
            list[i].heading = TurnTowards(list[i].heading, list[i].face, turn);
        }
        if (!building->open) {
            Finish(g);
        } else if (scr.insideCount[static_cast<std::size_t>(s)] + g.Size() <= def.capacity) {
            scr.insideCount[static_cast<std::size_t>(s)] += g.Size();
            for (std::size_t i = g.first; i < g.end; ++i) {
                list[i].activity = VisitorActivity::Inside;
                list[i].inside = site.id;
            }
            lead.activityUntil = now + Ticks(std::max(2.0f, def.serviceSeconds * kInsideShare) *
                                             (0.8f + 0.4f * rng.NextFloat()));
        } else if (now - lead.queuedTick > Ticks(kDoorPatienceSeconds)) {
            lead.Avoid(site.id);
            if (site.role == BuildingRole::Restroom) {
                lead.unmet |= kUnmetRestroom;
            }
            Think(g, VisitorThought::TooCrowded);
            Cheer(g, -0.03f);
            Finish(g);
        }
        return;
    }
    if (now < lead.activityUntil) {
        return;
    }
    // Out again.
    for (std::size_t i = g.first; i < g.end; ++i) {
        Visitor& v = list[i];
        switch (site.role) {
            case BuildingRole::Restroom:
                v.bladder = 0.0f;
                break;
            case BuildingRole::GiftShop: {
                const Money price = building->itemPrice;
                const bool wants = rng.Chance(0.25f + 0.6f * v.happiness);
                const float willing = static_cast<float>(def.itemPrice) * (1.2f + 0.5f * Hash01(v.look, 22));
                if (!wants || price > lead.cash || static_cast<float>(price) > willing) {
                    break;
                }
                lead.cash -= price;
                lead.spent += price;
                if (env.finance != nullptr) {
                    if (price > 0) {
                        env.finance->Earn(Account::GiftSales, price);
                    }
                    if (def.itemCost > 0) {
                        env.finance->Pay(Account::Goods, def.itemCost);
                    }
                }
                ++building->servedToday;
                ++building->servedTotal;
                v.happiness = Clamp01(v.happiness + def.fun * 0.5f);
                Event event;
                event.type = EventType::VisitorBought;
                event.visitor = v.id;
                event.building = building->id;
                event.position = v.position;
                event.detail = static_cast<int>(price);
                env.Emit(event);
                break;
            }
            case BuildingRole::EducationCenter:
                v.education = Clamp01(v.education + def.education * 0.5f);
                v.happiness = Clamp01(v.happiness + def.fun * 0.3f);
                break;
            default:
                break;
        }
    }
    if (site.role == BuildingRole::EducationCenter) {
        ++building->servedToday;
        ++building->servedTotal;
    }
    Finish(g);
}

void Stepper::Watched(Group g) {
    const Sites& sites = scr.sites;
    std::vector<Visitor>& list = List();
    Visitor& lead = list[g.first];
    const TileCoord tile = World::TileAt(lead.target);
    float visibility = 0.0f;
    if (const int view = sites.ViewIndex(lead.targetEnclosure); view >= 0) {
        for (const ViewingSpot& spot : sites.views[static_cast<std::size_t>(view)].spots) {
            if (spot.tile == tile) {
                visibility = std::max(visibility, spot.visibility);
            }
        }
    }
    float best = 0.0f;
    float welfare = 0.0f;
    int count = 0;
    std::uint64_t species = 0;
    for (const Animal& animal : fauna.All()) {
        if (animal.home != lead.targetEnclosure || !animal.Visible() || animal.loose || animal.species < 0 ||
            static_cast<std::size_t>(animal.species) >= env.species.All().size()) {
            continue;
        }
        const float distance = Distance(animal.position, lead.target);
        if (distance > kViewRangeM) {
            continue;
        }
        ++count;
        welfare += animal.welfare;
        const std::uint64_t bit = animal.species < 64 ? std::uint64_t{1} << animal.species : 0;
        const bool seen = (lead.seenSpecies & bit) != 0;
        species |= bit;
        const float appeal =
            static_cast<float>(env.species.All()[static_cast<std::size_t>(animal.species)].appeal) / 100.0f;
        best = std::max(best, appeal * std::clamp(1.25f - distance / 40.0f, 0.25f, 1.0f) *
                                  ShowOf(animal.activity) * (seen ? 0.3f : 1.0f));
    }
    if (count == 0 || visibility <= 0.0f) {
        Think(g, VisitorThought::CouldNotSeeAnimals);
        Cheer(g, -0.04f);
    } else {
        float gain = best * visibility * 0.16f + 0.008f * static_cast<float>(std::min(count - 1, 4));
        const bool crowded = sites.InBounds(tile) && scr.watchers[sites.TileIndex(tile)] > 9;
        if (crowded) {
            gain *= 0.7f;
            Think(g, VisitorThought::TooCrowded);
        }
        Cheer(g, gain);
        const int fresh = std::popcount(species & ~lead.seenSpecies);
        for (std::size_t i = g.first; i < g.end; ++i) {
            list[i].education = Clamp01(list[i].education + 0.03f * static_cast<float>(fresh));
        }
        lead.seenSpecies |= species;
        if (welfare / static_cast<float>(count) < 0.45f) {
            Think(g, VisitorThought::AnimalsLookUnhappy);
            Cheer(g, -0.05f);
        } else if (best >= 0.45f && !crowded) {
            Think(g, VisitorThought::LovedAnimal);
        }
    }
    // A new stretch of the zoo: perhaps what was missing can be found here.
    lead.unmet = 0;
}

}  // namespace zoo::visitors_detail
