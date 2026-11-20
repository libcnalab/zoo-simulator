// SPDX-License-Identifier: MIT
// What a group does next: the decision, the places its members take, the routes there.

#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <utility>

#include "zoo/visitors/VisitorInternal.hpp"

namespace zoo::visitors_detail {

using namespace visitor_tuning;

namespace {

constexpr float kAnywhere = 1.0e9f;
/// Columns of a viewing tile in the order they fill: the middle first.

VisitorActivity ActivityOnTheWay(VisitorGoal goal) {
    switch (goal) {
        case VisitorGoal::Enter:
            return VisitorActivity::Arriving;
        case VisitorGoal::Leave:
            return VisitorActivity::Leaving;
        case VisitorGoal::Flee:
            return VisitorActivity::Fleeing;
        default:
            return VisitorActivity::Walking;
    }
}

}  // namespace

Vec2 Stepper::Cluster(Vec2 centre, int member) const {
    if (member == 0) {
        return centre;
    }
    const float angle = kTwoPi * (static_cast<float>(member) - 0.5f) / 4.0f;
    return centre + Vec2{std::sin(angle), std::cos(angle)} * 0.6f;
}

float Stepper::Need(Group g, float Visitor::* need) {
    float most = 0.0f;
    for (std::size_t i = g.first; i < g.end; ++i) {
        most = std::max(most, List()[i].*need);
    }
    return most;
}

float Stepper::MeanHappiness(Group g) {
    float sum = 0.0f;
    for (std::size_t i = g.first; i < g.end; ++i) {
        sum += List()[i].happiness;
    }
    return sum / static_cast<float>(g.Size());
}

void Stepper::EnsureAppeal() {
    if (scr.appealTick == now) {
        return;
    }
    scr.appealTick = now;
    const Sites& sites = scr.sites;
    scr.appeal.assign(sites.views.size(), 0.0f);
    scr.counts.assign(sites.views.size(), 0);
    // The best species of each enclosure, and a little for every further animal.
    for (const Animal& animal : fauna.All()) {
        if (!animal.Visible() || animal.loose || animal.species < 0 ||
            static_cast<std::size_t>(animal.species) >= env.species.All().size()) {
            continue;
        }
        const int view = sites.ViewIndex(animal.home);
        if (view < 0) {
            continue;
        }
        const std::size_t v = static_cast<std::size_t>(view);
        const float own =
            static_cast<float>(env.species.All()[static_cast<std::size_t>(animal.species)].appeal) / 100.0f;
        scr.appeal[v] = std::max(scr.appeal[v], own);
        ++scr.counts[v];
    }
    for (std::size_t v = 0; v < scr.appeal.size(); ++v) {
        if (scr.counts[v] > 0) {
            scr.appeal[v] =
                std::max(scr.appeal[v], 0.01f) + 0.1f * static_cast<float>(std::min(scr.counts[v] - 1, 3));
        }
    }
}

std::uint32_t Stepper::TakenSeats(PropId bench) const {
    std::uint32_t taken = 0;
    for (const Visitor& v : all.visitors_) {
        if (v.seat == bench) {
            taken |= 1u << (v.slot & 31u);
        }
    }
    return taken;
}

// ---- routes -------------------------------------------------------------------------------

bool Stepper::Route(Vec2 from, Vec2 to, std::vector<Vec2>& out) {
    out.clear();
    const Navigator& nav = env.nav;
    Vec2 start = from;
    Vec2 goal = to;
    bool hop = false;
    bool tail = false;
    if (const TileCoord tile = World::TileAt(from); !nav.Passable(tile, kWalker)) {
        // Off the paving at a bench: leave by the same path access that made the seat
        // usable. A generic nearest-path search can pick the far side of a lawn from a
        // member standing at the outer end of a picnic table.
        const auto near = nav.NearestPassable(tile, kWalker, 4);
        if (!near) {
            return false;
        }
        start = World::TileCenter(*near);
        float nearestBench = 3.5f * 3.5f;
        for (const SeatSite& bench : scr.sites.benches) {
            const float distance = DistanceSquared(from, bench.position);
            NavProfile ground = kWalker;
            ground.pathsOnly = false;
            if (distance < nearestBench && nav.Walkable(from, bench.access, ground)) {
                start = bench.access;
                nearestBench = distance;
            }
        }
        hop = true;
    }
    if (const TileCoord tile = World::TileAt(to); !nav.Passable(tile, kWalker)) {
        const auto near = nav.NearestPassable(tile, kWalker, 3);
        if (!near) {
            return false;
        }
        goal = World::TileCenter(*near);
        float nearestBench = 3.5f * 3.5f;
        for (const SeatSite& bench : scr.sites.benches) {
            const float distance = DistanceSquared(to, bench.position);
            NavProfile ground = kWalker;
            ground.pathsOnly = false;
            if (distance < nearestBench && nav.Walkable(bench.access, to, ground)) {
                goal = bench.access;
                nearestBench = distance;
            }
        }
        tail = true;
    }
    if (!nav.Reachable(World::TileAt(start), World::TileAt(goal), kWalker)) {
        return false;
    }
    --scr.budget;
    if (!nav.FindPath(start, goal, kWalker, scr.path, kMaxPathExpansions)) {
        return false;
    }
    if (hop) {
        out.push_back(start);
    }
    out.insert(out.end(), scr.path.begin(), scr.path.end());
    if (tail) {
        out.push_back(to);
    }
    return true;
}

void Stepper::Lanes(Visitor& v, int place, int size, std::uint32_t leaderLook) {
    const std::size_t count = v.route.size();
    if (count < 2) {
        return;
    }
    // Everybody keeps a little to one side of the route: a crowd on a path then spreads over
    // its width instead of walking in single file. Somebody alone takes a lane of its own;
    // a group takes one together and walks two abreast in it, the odd fifth in the middle.
    Vec2 offset{(Hash01(leaderLook, 11) - 0.5f) * 2.0f, (Hash01(leaderLook, 12) - 0.5f) * 2.0f};
    if (size <= 1) {
        offset = offset * kLaneOffset;
    } else {
        const float side = place == 4 ? 0.0f : (place % 2 == 1 ? 0.5f : -0.5f) * kAbreastM;
        offset = offset * (kLaneOffset - 0.5f * kAbreastM) + Vec2{side, side};
    }
    const Navigator& nav = env.nav;
    std::vector<Vec2>& out = scr.points;
    out.clear();
    Vec2 previous = v.position;  // the point walked from
    Vec2 anchor = v.position;    // the waypoint it stands for
    for (std::size_t i = 0; i < count; ++i) {
        const Vec2 original = v.route[i];
        Vec2 next = original;
        if (i + 1 < count) {
            const Vec2 shifted = original + offset;
            if (nav.Passable(World::TileAt(shifted), kWalker) && nav.Walkable(previous, shifted, kWalker)) {
                next = shifted;
            }
        }
        if (next == original && previous != anchor && nav.Passable(World::TileAt(original), kWalker) &&
            !nav.Walkable(previous, original, kWalker)) {
            out.push_back(anchor);  // back to the middle of the path first
        }
        out.push_back(next);
        previous = next;
        anchor = original;
    }
    v.route.assign(out.begin(), out.end());
}

bool Stepper::RouteGroup(Group g) {
    std::vector<Visitor>& list = List();
    Visitor& lead = list[g.first];
    lead.routeIndex = 0;
    if (!Route(lead.position, lead.target, lead.route)) {
        return false;
    }
    const Navigator& nav = env.nav;
    for (std::size_t i = g.first + 1; i < g.end; ++i) {
        Visitor& v = list[i];
        v.routeIndex = 0;
        v.route.clear();
        bool own = true;
        if (!lead.route.empty() && DistanceSquared(v.position, lead.position) < 36.0f &&
            nav.Walkable(v.position, lead.route.front(), kWalker)) {
            // Walks with the leader and parts ways at the end.
            v.route.assign(lead.route.begin(), lead.route.end() - 1);
            const Vec2 knot = v.route.empty() ? v.position : v.route.back();
            if (nav.Passable(World::TileAt(v.target), kWalker) && !nav.Walkable(knot, v.target, kWalker) &&
                nav.Passable(World::TileAt(lead.route.back()), kWalker)) {
                v.route.push_back(lead.route.back());
            }
            v.route.push_back(v.target);
            own = false;
        }
        if (own && !Route(v.position, v.target, v.route)) {
            v.route.assign(1, v.target);
        }
        Lanes(v, static_cast<int>(i - g.first), g.Size(), lead.look);
    }
    Lanes(lead, 0, g.Size(), lead.look);
    return true;
}

void Stepper::MakeRoom(Group g, Plan& plan, const Vec2* focus) {
    const std::vector<Visitor>& list = all.visitors_;
    const int size = g.Size();
    int first = -1;
    for (int k = 0; k < size; ++k) {
        if (!plan.seated[static_cast<std::size_t>(k)]) {
            first = k;
            break;
        }
    }
    if (first < 0) {
        return;
    }
    // Where the people around stand, or will.
    const Vec2 around = plan.place[static_cast<std::size_t>(first)];
    std::vector<Vec2>& spots = scr.spots;
    spots.clear();
    for (std::size_t i = 0; i < list.size(); ++i) {
        const Visitor& v = list[i];
        if ((i >= g.first && i < g.end) || v.activity == VisitorActivity::Inside) {
            continue;
        }
        const Vec2 spot = Travelling(v.activity) ? v.target : v.position;
        if (DistanceSquared(spot, around) < 49.0f) {
            spots.push_back(spot);
        }
    }
    // And where the lines of the stalls stand (their first places; a line is seldom longer):
    // nobody has its lunch in the queue.
    for (const ServiceSite& site : scr.sites.services) {
        if (DistanceSquared(site.door, around) > 100.0f) {
            continue;
        }
        for (std::size_t i = 0; i < site.line.size() && i < kLineKeptClear; ++i) {
            if (i > 0) {
                spots.push_back((site.line[i - 1] + site.line[i]) * 0.5f);
            }
            spots.push_back(site.line[i]);
        }
    }
    const auto free = [&](Vec2 place) {
        return std::none_of(spots.begin(), spots.end(),
                            [&](Vec2 spot) { return DistanceSquared(spot, place) < kRoomM * kRoomM; });
    };
    const Navigator& nav = env.nav;
    NavProfile ground = kWalker;
    ground.pathsOnly = nav.Passable(World::TileAt(around), kWalker);
    const float turn = kTwoPi * Hash01(list[g.first].look, 33);
    const auto ring = [&](int n, float step) {
        if (n == 0) {
            return Vec2{};
        }
        const float angle = turn + kTwoPi * static_cast<float>((n - 1) % 8) / 8.0f;
        return Vec2{std::sin(angle), std::cos(angle)} * (step * static_cast<float>((n - 1) / 8 + 1));
    };
    // (Places that fell on top of one another, pulled back from a fence or a bed, are not a
    // shape to keep: then each finds its own.)
    bool apart = true;
    for (int a = 0; a < size && apart; ++a) {
        for (int b = a + 1; b < size && apart; ++b) {
            const std::size_t i = static_cast<std::size_t>(a);
            const std::size_t k = static_cast<std::size_t>(b);
            apart = plan.seated[i] || plan.seated[k] ||
                    DistanceSquared(plan.place[i], plan.place[k]) >= kRoomM * kRoomM;
        }
    }
    // Together to where there is room for everybody, as near as it is to be had...
    for (int n = 0; apart && n <= 32; ++n) {
        const Vec2 shift = ring(n, 0.8f);
        bool fits = true;
        for (int k = 0; k < size && fits; ++k) {
            const std::size_t m = static_cast<std::size_t>(k);
            if (plan.seated[m]) {
                continue;
            }
            const Vec2 place = plan.place[m] + shift;
            fits = free(place) && (n == 0 || (nav.Passable(World::TileAt(place), ground) &&
                                              (k != first || nav.Walkable(around, place, ground))));
        }
        if (!fits) {
            continue;
        }
        if (n == 0) {
            return;
        }
        for (int k = 0; k < size; ++k) {
            const std::size_t m = static_cast<std::size_t>(k);
            if (!plan.seated[m]) {
                plan.place[m] = plan.place[m] + shift;
                if (focus != nullptr) {
                    plan.face[m] = HeadingOf(*focus - plan.place[m]);
                }
            }
        }
        return;
    }
    // ...or each for itself.
    for (int k = 0; k < size; ++k) {
        const std::size_t m = static_cast<std::size_t>(k);
        if (plan.seated[m]) {
            continue;
        }
        for (int n = 0; n <= 24; ++n) {
            const Vec2 place = plan.place[m] + ring(n, 0.6f);
            if (free(place) && (n == 0 || (nav.Passable(World::TileAt(place), ground) &&
                                           nav.Walkable(plan.place[m], place, ground)))) {
                plan.place[m] = place;
                if (focus != nullptr && n != 0) {
                    plan.face[m] = HeadingOf(*focus - place);
                }
                break;
            }
        }
        spots.push_back(plan.place[m]);
    }
}

void Stepper::Start(Group g, const Plan& plan) {
    std::vector<Visitor>& list = List();
    const VisitorActivity activity = ActivityOnTheWay(plan.goal);
    for (std::size_t i = g.first; i < g.end; ++i) {
        const std::size_t k = i - g.first;
        Visitor& v = list[i];
        v.goal = plan.goal;
        v.targetEnclosure = plan.enclosure;
        v.targetBuilding = plan.building;
        v.targetProp = plan.prop;
        v.target = plan.place[k];
        v.face = plan.face[k];
        v.slot = plan.slot[k];
        v.seat = plan.seated[k] ? plan.prop : PropId{};
        v.inside = {};
        v.activity = activity;
    }
    list[g.first].served = false;
    list[g.first].reroute = false;
}

bool Stepper::Apply(Group g, const Plan& plan) {
    Start(g, plan);
    if (!RouteGroup(g)) {
        Finish(g);
        return false;
    }
    Lead(g).lost = 0;
    return true;
}

void Stepper::Failed(Group g) {
    Finish(g);
    Visitor& lead = Lead(g);
    if (++lead.lost >= 3) {
        // No way to anywhere: a path was taken away under its feet.
        Think(g, VisitorThought::LostWay);
        Cheer(g, -0.2f);
        Depart(g, false);
        return;
    }
    lead.activityUntil = now + Ticks(2.0f);
}

// ---- plans --------------------------------------------------------------------------------

bool Stepper::PlanLeave(Group g, VisitorGoal goal) {
    Plan plan;
    plan.goal = goal;
    for (int k = 0; k < g.Size(); ++k) {
        plan.place[static_cast<std::size_t>(k)] = Cluster(scr.sites.spawn, k);
    }
    return Apply(g, plan);
}

bool Stepper::TryWatch(Group g) {
    const Sites& sites = scr.sites;
    Visitor& lead = Lead(g);
    const int size = g.Size();
    EnsureAppeal();
    scr.ranked.clear();
    for (std::size_t i = 0; i < sites.views.size(); ++i) {
        const ViewSite& site = sites.views[i];
        const auto looks =
            std::count(lead.seenEnclosures.begin(), lead.seenEnclosures.end(), site.enclosure.value);
        if (scr.appeal[i] <= 0.0f || looks >= kLooksPerEnclosure) {
            continue;
        }
        // The better the animals and the nearer, the likelier. A second look from somewhere
        // else is worth less than something new, and a front where people already stand
        // shoulder to shoulder draws fewer than a free one.
        int bound = 0;
        for (const ViewingSpot& spot : site.spots) {
            bound += scr.watchers[sites.TileIndex(spot.tile)];
        }
        const float density =
            static_cast<float>(bound) / static_cast<float>(std::max<std::size_t>(site.spots.size(), 1));
        const float star = 0.2f + scr.appeal[i];
        const float near = kWatchNearM / (kWatchNearM + Distance(lead.position, site.centre));
        const float score = star * star * near * near * (looks == 0 ? 1.0f : kSecondLookWeight) /
                            (1.0f + density / kWatchersPerTileAtEase);
        scr.ranked.emplace_back(score, static_cast<int>(i));
    }
    // Three of them drawn by lot, each with the chance of its weight: were it always the
    // best, the habitats beside the busiest paths would take everybody and a habitat that is
    // a little less of a star, or a little further, would stand unwatched all day.
    for (std::size_t first = 0; first < std::min<std::size_t>(scr.ranked.size(), 3); ++first) {
        float total = 0.0f;
        for (std::size_t k = first; k < scr.ranked.size(); ++k) {
            total += scr.ranked[k].first;
        }
        float lot = all.rng_.NextFloat() * total;
        std::size_t drawn = scr.ranked.size() - 1;
        for (std::size_t k = first; k < scr.ranked.size(); ++k) {
            lot -= scr.ranked[k].first;
            if (lot <= 0.0f) {
                drawn = k;
                break;
            }
        }
        std::swap(scr.ranked[first], scr.ranked[drawn]);
    }
    if (scr.ranked.size() > 3) {
        scr.ranked.resize(3);
    }
    for (const auto& [unused, index] : scr.ranked) {
        const ViewSite& site = sites.views[static_cast<std::size_t>(index)];
        const bool again = std::find(lead.seenEnclosures.begin(), lead.seenEnclosures.end(),
                                     site.enclosure.value) != lead.seenEnclosures.end();
        scr.beasts.clear();
        for (const Animal& animal : fauna.All()) {
            if (animal.home == site.enclosure && animal.Visible() && !animal.loose) {
                scr.beasts.push_back(animal.position);
            }
        }
        const ViewingSpot* best = nullptr;
        float bestScore = -std::numeric_limits<float>::infinity();
        for (const ViewingSpot& spot : site.spots) {
            const int standing = scr.watchers[sites.TileIndex(spot.tile)];
            if (standing + size > kSlotsPerViewingTile) {
                continue;
            }
            const Vec2 centre = World::TileCenter(spot.tile);
            if (again && DistanceSquared(centre, lead.position) < kSecondLookM * kSecondLookM) {
                continue;  // another look is from another side
            }
            float nearest = 40.0f * 40.0f;
            for (const Vec2 beast : scr.beasts) {
                nearest = std::min(nearest, DistanceSquared(beast, centre));
            }
            // Where the animals are near and the view is clear, away from the crowd.
            const float score =
                spot.visibility * std::clamp(1.25f - std::sqrt(nearest) / 40.0f, 0.25f, 1.0f) -
                0.06f * static_cast<float>(standing) - 0.004f * Distance(lead.position, centre) +
                0.1f * all.rng_.NextFloat();
            if (score > bestScore) {
                bestScore = score;
                best = &spot;
            }
        }
        if (best == nullptr) {
            continue;
        }
        std::uint32_t taken = 0;
        for (const Visitor& v : all.visitors_) {
            if (v.goal == VisitorGoal::Watch && World::TileAt(v.target) == best->tile) {
                taken |= 1u << (v.slot & 31u);
            }
        }
        Plan plan;
        plan.goal = VisitorGoal::Watch;
        plan.enclosure = site.enclosure;
        const Vec2 toward = ToVec(Step(best->side));
        int slot = 0;
        bool enoughRoom = true;
        for (int k = 0; k < size; ++k) {
            Vec2 place;
            bool available = false;
            while (slot < kSlotsPerViewingTile) {
                place = ViewingPosition(*best, slot);
                available = (taken & (1u << slot)) == 0;
                for (std::size_t i = 0; i < all.visitors_.size() && available; ++i) {
                    if ((i >= g.first && i < g.end) || all.visitors_[i].activity == VisitorActivity::Inside) {
                        continue;
                    }
                    const Visitor& other = all.visitors_[i];
                    const Vec2 reserved = Travelling(other.activity) ? other.target : other.position;
                    available = DistanceSquared(place, reserved) >= kRoomM * kRoomM;
                }
                if (available) {
                    break;
                }
                ++slot;
            }
            if (!available) {
                enoughRoom = false;
                break;
            }
            taken |= 1u << slot;
            const std::size_t m = static_cast<std::size_t>(k);
            plan.place[m] = place;
            plan.face[m] = HeadingOf(toward);
            plan.slot[m] = static_cast<std::uint8_t>(slot);
        }
        if (!enoughRoom) {
            continue;
        }
        std::uint8_t& standing = scr.watchers[sites.TileIndex(best->tile)];
        standing = static_cast<std::uint8_t>(std::min(255, standing + size));
        lead.seenEnclosures.push_back(site.enclosure.value);
        if (!Apply(g, plan)) {
            Failed(g);
        }
        return true;
    }
    return false;
}

bool Stepper::TryService(Group g, VisitorGoal goal, float maxDistance, bool complain) {
    const Sites& sites = scr.sites;
    Visitor& lead = Lead(g);
    const int size = g.Size();
    BuildingRole role = BuildingRole::FoodStall;
    VisitorThought lacking = VisitorThought::None;
    std::uint8_t bit = 0;
    switch (goal) {
        case VisitorGoal::Food:
            role = BuildingRole::FoodStall;
            lacking = VisitorThought::Hungry;
            bit = kUnmetFood;
            break;
        case VisitorGoal::Drink:
            role = BuildingRole::DrinkStall;
            lacking = VisitorThought::Thirsty;
            bit = kUnmetDrink;
            break;
        case VisitorGoal::Restroom:
            role = BuildingRole::Restroom;
            lacking = VisitorThought::NeedsRestroom;
            bit = kUnmetRestroom;
            break;
        case VisitorGoal::Shop:
            role = BuildingRole::GiftShop;
            break;
        case VisitorGoal::Learn:
            role = BuildingRole::EducationCenter;
            break;
        default:
            return false;
    }
    const bool stall = goal == VisitorGoal::Food || goal == VisitorGoal::Drink;
    int best = -1;
    float bestCost = std::numeric_limits<float>::infinity();
    for (std::size_t s = 0; s < sites.services.size(); ++s) {
        const ServiceSite& site = sites.services[s];
        if (site.role != role || lead.Avoids(site.id)) {
            continue;
        }
        const Building* building = env.objects.FindBuilding(site.id);
        if (building == nullptr || !building->open) {
            continue;
        }
        const float distance = Distance(lead.position, site.door);
        if (distance > maxDistance) {
            continue;
        }
        const BuildingDef& def = env.catalog.buildings[static_cast<std::size_t>(site.def)];
        float cost = distance;
        if (stall) {
            if (site.line.empty() || scr.queuePeople[s] + size > kQueueLimit ||
                building->itemPrice > lead.cash) {
                continue;
            }
            // A queue is worth a detour; a proper meal too.
            cost += 4.0f * static_cast<float>(scr.queuePeople[s]);
            cost -= 60.0f * (goal == VisitorGoal::Food ? def.hungerRelief : def.thirstRelief);
        } else {
            cost += 3.0f * static_cast<float>(std::max(0, scr.insideCount[s] + size - def.capacity));
        }
        if (cost < bestCost) {
            bestCost = cost;
            best = static_cast<int>(s);
        }
    }
    if (best < 0) {
        if (complain && bit != 0) {
            lead.unmet |= bit;
            Think(g, lacking);
            Cheer(g, -0.04f);
        }
        return false;
    }
    const std::size_t s = static_cast<std::size_t>(best);
    const ServiceSite& site = sites.services[s];
    Plan plan;
    plan.goal = goal;
    plan.building = site.id;
    for (int k = 0; k < size; ++k) {
        const std::size_t m = static_cast<std::size_t>(k);
        if (stall) {
            const std::size_t place =
                std::min(static_cast<std::size_t>(scr.queuePeople[s] + k), site.line.size() - 1);
            plan.place[m] = site.line[place];
            plan.face[m] = place == 0 ? site.face : HeadingOf(site.line[place - 1] - site.line[place]);
        } else {
            plan.place[m] = Cluster(site.door, k);
            plan.face[m] = site.face;
        }
    }
    if (stall) {
        scr.queuePeople[s] += size;
    }
    if (!Apply(g, plan)) {
        Failed(g);
    }
    return true;
}

bool Stepper::PlanSeats(Group g, Vec2 around, float maxDistance, Plan& plan) {
    const Sites& sites = scr.sites;
    const int size = g.Size();
    scr.ranked.clear();
    for (std::size_t i = 0; i < sites.benches.size(); ++i) {
        const float distance = Distance(around, sites.benches[i].access);
        if (distance <= maxDistance) {
            scr.ranked.emplace_back(distance, static_cast<int>(i));
        }
    }
    std::sort(scr.ranked.begin(), scr.ranked.end());
    // The nearest with a free seat; one that seats everybody is worth fifteen metres more.
    int chosen = -1;
    float chosenDistance = 0.0f;
    std::uint32_t chosenTaken = 0;
    const std::size_t looked = std::min<std::size_t>(scr.ranked.size(), 8);
    for (std::size_t n = 0; n < looked; ++n) {
        const auto [distance, index] = scr.ranked[n];
        const SeatSite& bench = sites.benches[static_cast<std::size_t>(index)];
        const std::uint32_t mask = (1u << bench.seats) - 1u;
        const std::uint32_t taken = TakenSeats(bench.id) & mask;
        const int free = bench.seats - std::popcount(taken);
        // A bench is for sitting down together: a family does not walk up to a stranger's
        // bench so that one of them sits and four stand before it. At least half must sit.
        if (free <= 0 || 2 * free < size) {
            continue;
        }
        if (chosen >= 0 && distance > chosenDistance + 15.0f) {
            break;
        }
        if (chosen < 0 || free >= size) {
            chosen = index;
            chosenDistance = distance;
            chosenTaken = taken;
            if (free >= size) {
                break;
            }
        }
    }
    if (chosen < 0) {
        return false;
    }
    const SeatSite& bench = sites.benches[static_cast<std::size_t>(chosen)];
    plan.prop = bench.id;
    int seat = 0;
    int standing = 0;
    for (int k = 0; k < size; ++k) {
        const std::size_t m = static_cast<std::size_t>(k);
        while (seat < bench.seats && (chosenTaken & (1u << seat)) != 0) {
            ++seat;
        }
        if (seat < bench.seats) {
            chosenTaken |= 1u << seat;
            plan.place[m] = SeatPosition(bench, seat, plan.face[m]);
            plan.slot[m] = static_cast<std::uint8_t>(seat);
            plan.seated[m] = true;
        } else {
            // No seat left: stands by the others, in a loose arc before a bench (not in a
            // rank at arm's length from those who sit) or at the end of a table.
            const Vec2 forward = HeadingVector(bench.heading);
            const Vec2 right{forward.z, -forward.x};
            const float side = standing % 2 == 0 ? 1.0f : -1.0f;
            const float step = static_cast<float>((standing + 1) / 2);
            const Visitor& member = List()[g.first + m];
            const float out = 1.25f + 0.45f * Hash01(member.look, 41);
            const float along = side * (0.35f + 0.8f * (step - 1.0f) + 0.5f * Hash01(member.look, 42));
            plan.place[m] =
                bench.seats <= 3
                    ? bench.position + forward * out + right * along
                    : bench.position + right * (side * (1.35f + 0.5f * static_cast<float>(standing / 2)));
            ++standing;
            // Where one can walk to from the bench's path tile, never across a fence or onto
            // planting behind a rail: otherwise nearer to that tile, at worst on it.
            NavProfile ground = kWalker;
            ground.pathsOnly = false;
            for (int pull = 0; pull < 3 && !env.nav.Walkable(bench.access, plan.place[m], ground); ++pull) {
                plan.place[m] = bench.access + (plan.place[m] - bench.access) * 0.6f;
            }
            if (!env.nav.Walkable(bench.access, plan.place[m], ground)) {
                plan.place[m] = bench.access;
            }
            plan.face[m] = HeadingOf(bench.position - plan.place[m]);
            plan.slot[m] = 0;
            plan.seated[m] = false;
        }
    }
    if (standing > 0) {
        MakeRoom(g, plan, &bench.position);
    }
    return true;
}

bool Stepper::TryRest(Group g, bool complain) {
    Visitor& lead = Lead(g);
    Plan plan;
    plan.goal = VisitorGoal::Rest;
    if (!PlanSeats(g, lead.position, kSeatSearchM, plan)) {
        if (complain) {
            lead.unmet |= kUnmetRest;
            Think(g, VisitorThought::Tired);
            Cheer(g, -0.03f);
        }
        return false;
    }
    if (!Apply(g, plan)) {
        Failed(g);
    }
    return true;
}

bool Stepper::PlanEat(Group g, const ServiceSite& site) {
    Visitor& lead = Lead(g);
    const int size = g.Size();
    Plan plan;
    plan.goal = VisitorGoal::Eat;
    if (PlanSeats(g, site.door, kSeatSearchM, plan)) {
        return Apply(g, plan);
    }
    // No seat: stand in a little circle out of the way of the queue.
    const Navigator& nav = env.nav;
    Vec2 centre = lead.position;
    const float turn = kTwoPi * Hash01(lead.look, 31);
    for (int n = 0; n < 6; ++n) {
        const float angle = turn + kTwoPi * static_cast<float>(n) / 6.0f;
        const Vec2 spot =
            site.door + Vec2{std::sin(angle), std::cos(angle)} * (2.2f + 1.2f * Hash01(lead.look, 32));
        if (nav.Passable(World::TileAt(spot), kWalker) && nav.Walkable(site.door, spot, kWalker)) {
            centre = spot;
            break;
        }
    }
    plan.prop = {};
    for (int k = 0; k < size; ++k) {
        const std::size_t m = static_cast<std::size_t>(k);
        plan.place[m] = Cluster(centre, k);
        if (!nav.Passable(World::TileAt(plan.place[m]), kWalker)) {
            plan.place[m] = centre;
        }
        plan.face[m] = k == 0 ? turn : HeadingOf(centre - plan.place[m]);
        plan.slot[m] = 0;
        plan.seated[m] = false;
    }
    MakeRoom(g, plan, nullptr);
    return Apply(g, plan);
}

bool Stepper::TryBin(Group g) {
    const Sites& sites = scr.sites;
    Visitor& lead = Lead(g);
    const PropSite* best = nullptr;
    float bestDistance = kBinSearchM;
    for (const PropSite& bin : sites.bins) {
        const float distance = Distance(lead.position, bin.access);
        if (distance >= bestDistance) {
            continue;
        }
        const Prop* prop = std::as_const(env.objects).FindProp(bin.id);
        if (prop == nullptr || prop->amount + static_cast<float>(lead.litter) > bin.capacity) {
            continue;  // full to the brim
        }
        best = &bin;
        bestDistance = distance;
    }
    if (best == nullptr) {
        return false;
    }
    Plan plan;
    plan.goal = VisitorGoal::Bin;
    plan.prop = best->id;
    for (int k = 0; k < g.Size(); ++k) {
        const std::size_t m = static_cast<std::size_t>(k);
        plan.place[m] = Cluster(best->access, k);
        if (!env.nav.Passable(World::TileAt(plan.place[m]), kWalker)) {
            plan.place[m] = best->access;
        }
        plan.face[m] = HeadingOf(best->position - plan.place[m]);
    }
    MakeRoom(g, plan, &best->position);
    if (!Apply(g, plan)) {
        Failed(g);
    }
    return true;
}

bool Stepper::TryRead(Group g) {
    const Sites& sites = scr.sites;
    Visitor& lead = Lead(g);
    const PropSite* best = nullptr;
    float bestDistance = kBoardSearchM;
    for (const PropSite& board : sites.boards) {
        const float distance = Distance(lead.position, board.access);
        if (distance < bestDistance) {
            best = &board;
            bestDistance = distance;
        }
    }
    if (best == nullptr) {
        return false;
    }
    Plan plan;
    plan.goal = VisitorGoal::Read;
    plan.prop = best->id;
    for (int k = 0; k < g.Size(); ++k) {
        const std::size_t m = static_cast<std::size_t>(k);
        plan.place[m] = Cluster(best->access, k);
        if (!env.nav.Passable(World::TileAt(plan.place[m]), kWalker)) {
            plan.place[m] = best->access;
        }
        plan.face[m] = HeadingOf(best->position - plan.place[m]);
    }
    MakeRoom(g, plan, &best->position);
    if (!Apply(g, plan)) {
        Failed(g);
    }
    return true;
}

// ---- the decision -------------------------------------------------------------------------

void Stepper::Decide(Group g) {
    const Sites& sites = scr.sites;
    Visitor& lead = Lead(g);
    const int size = g.Size();
    Rng& rng = all.rng_;

    if (!lead.ticketPaid) {
        if (!sites.open) {
            Depart(g, false);  // the gate cannot be reached: they turn round on the road
            return;
        }
        Plan plan;
        plan.goal = VisitorGoal::Enter;
        for (int k = 0; k < size; ++k) {
            plan.place[static_cast<std::size_t>(k)] = Cluster(World::TileCenter(sites.entranceTile), k);
            plan.face[static_cast<std::size_t>(k)] = kPi;
        }
        if (!Apply(g, plan)) {
            Failed(g);
        }
        return;
    }

    const auto leave = [&] {
        if (MeanHappiness(g) >= 0.75f) {
            Think(g, VisitorThought::GreatZoo);
        }
        if (!PlanLeave(g, VisitorGoal::Leave)) {
            Failed(g);
        }
    };
    const float stay = kVisitHours + kVisitSpreadHours * Hash01(lead.look, 5);
    const bool over = static_cast<float>(now - lead.arrivedTick) >
                      stay * all.pace_.stay * static_cast<float>(kTicksPerGameHour);
    if (Closing() || MeanHappiness(g) < 0.18f) {
        leave();
        return;
    }
    if (over) {
        // Time to go; the shop is on the way out for those who liked it here.
        if ((lead.done & kDoneShop) == 0) {
            lead.done |= kDoneShop;
            if (MeanHappiness(g) >= 0.5f && rng.Chance(0.5f) &&
                TryService(g, VisitorGoal::Shop, kAnywhere, false)) {
                return;
            }
        }
        leave();
        return;
    }

    if (lead.litter > 0) {
        if (TryBin(g)) {
            return;
        }
        // No bin within reach: some pocket their wrappers, the others drop them.
        for (int n = lead.litter; n > 0; --n) {
            if (rng.Chance(0.55f)) {
                DropLitter(lead.position + Vec2{rng.Range(-0.6f, 0.6f), rng.Range(-0.6f, 0.6f)});
            }
        }
        lead.litter = 0;
    }

    struct Want {
        float level;
        VisitorGoal goal;
        std::uint8_t bit;
    };
    std::array<Want, 4> wants = {{{Need(g, &Visitor::bladder), VisitorGoal::Restroom, kUnmetRestroom},
                                  {Need(g, &Visitor::thirst), VisitorGoal::Drink, kUnmetDrink},
                                  {Need(g, &Visitor::hunger), VisitorGoal::Food, kUnmetFood},
                                  {Need(g, &Visitor::tiredness), VisitorGoal::Rest, kUnmetRest}}};
    std::stable_sort(wants.begin(), wants.end(),
                     [](const Want& a, const Want& b) { return a.level > b.level; });
    const auto see_to = [&](const Want& want, float reach, bool complain) {
        return want.goal == VisitorGoal::Rest ? TryRest(g, complain)
                                              : TryService(g, want.goal, reach, complain);
    };
    // What cannot wait.
    for (const Want& want : wants) {
        if (want.level >= kUrgent && (lead.unmet & want.bit) == 0 && see_to(want, kAnywhere, true)) {
            return;
        }
    }
    // A look at the board of the animals just watched.
    if (lead.previousGoal == VisitorGoal::Watch && rng.Chance(0.45f) && TryRead(g)) {
        return;
    }
    // What would be welcome, if it is on the way.
    for (const Want& want : wants) {
        if (want.level >= kNotable && want.level < kUrgent && (lead.unmet & want.bit) == 0 &&
            see_to(want, kHandyM, false)) {
            return;
        }
    }
    if (TryWatch(g)) {
        return;
    }
    // Everything seen: the shop and the education centre, once.
    if ((lead.done & kDoneLearn) == 0) {
        lead.done |= kDoneLearn;
        if (rng.Chance(0.4f) && TryService(g, VisitorGoal::Learn, kAnywhere, false)) {
            return;
        }
    }
    if ((lead.done & kDoneShop) == 0) {
        lead.done |= kDoneShop;
        if (rng.Chance(0.55f) && TryService(g, VisitorGoal::Shop, kAnywhere, false)) {
            return;
        }
    }
    if (lead.seenEnclosures.empty() && (lead.done & kDoneNothingSeen) == 0) {
        lead.done |= kDoneNothingSeen;
        Think(g, VisitorThought::NothingToSee);
        Cheer(g, -0.15f);
    }
    leave();
}

}  // namespace zoo::visitors_detail
