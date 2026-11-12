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

void Stepper::DropLitter(Vec2 position) {
    if (all.litter_.size() >= Visitors::kMaxLitter) {
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
    // A group walks at the pace of its slowest.
    float pace = 10.0f;
    float farthest = 0.0f;
    for (std::size_t i = g.first; i < g.end; ++i) {
        const Visitor& v = list[i];
        pace = std::min(pace, v.child ? kChildSpeed : kWalkSpeed + kWalkSpeedSpread * Hash01(v.look, 3));
        farthest = std::max(farthest, DistanceSquared(v.position, lead.position));
    }
    if (lead.activity == VisitorActivity::Fleeing) {
        pace *= kFleeFactor;
    }
    const float turn = kTurnRadiansPerSecond * kTickSeconds;
    const float leadLeft = Distance(lead.position, lead.target);
    bool everybody = true;
    for (std::size_t i = g.first; i < g.end; ++i) {
        Visitor& v = list[i];
        if (v.routeIndex >= v.route.size()) {
            v.speed = 0.0f;
            v.heading = TurnTowards(v.heading, v.face, turn);
            continue;
        }
        float speed = pace;
        if (i == g.first) {
            if (farthest > 25.0f) {
                speed *= 0.6f;  // waits for the stragglers
            }
        } else {
            const float left = Distance(v.position, v.target);
            if (left > leadLeft + 2.0f) {
                speed *= 1.3f;  // catches up
            } else if (left < leadLeft - 2.0f) {
                speed *= 0.75f;
            }
        }
        const Vec2 before = v.position;
        std::size_t index = v.routeIndex;
        const bool arrived = FollowPath(v.route, index, v.position, speed * kTickSeconds);
        v.routeIndex = static_cast<std::uint32_t>(index);
        const Vec2 step = v.position - before;
        const float moved = Length(step);
        v.speed = moved / kTickSeconds;
        if (moved > 1.0e-4f) {
            v.heading = TurnTowards(v.heading, HeadingOf(step), turn);
        }
        if (!arrived) {
            everybody = false;
        }
    }
    if (everybody) {
        GroupArrived(g);
    }
}

void Stepper::GroupArrived(Group g) {
    std::vector<Visitor>& list = List();
    Visitor& lead = list[g.first];
    Rng& rng = all.rng_;
    for (std::size_t i = g.first; i < g.end; ++i) {
        list[i].speed = 0.0f;
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
            lead.activityUntil = now + Ticks(kWatchSeconds + kWatchSpreadSeconds * rng.NextFloat());
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
            lead.activityUntil = now + Ticks(kRestSeconds * (1.0f + rng.NextFloat()));
            return;
        case VisitorGoal::Eat:
            SetActivity(g, VisitorActivity::Eating);
            lead.activityUntil = now + Ticks(kEatSeconds * (1.0f + 0.6f * rng.NextFloat()));
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
            lead.activityUntil = now + Ticks(kReadSeconds * (1.0f + 0.5f * rng.NextFloat()));
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
    const float turn = kTurnRadiansPerSecond * kTickSeconds;
    for (std::size_t i = g.first; i < g.end; ++i) {
        list[i].speed = 0.0f;
        list[i].heading = TurnTowards(list[i].heading, list[i].face, turn);
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
    for (std::size_t i = g.first; i < g.end; ++i) {
        Visitor& v = list[i];
        const std::size_t place =
            std::min(static_cast<std::size_t>(ahead) + (i - g.first), site.line.size() - 1);
        const Vec2 to = site.line[place] - v.position;
        const float distance = Length(to);
        if (distance > 0.03f) {
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
            lead.skipBuilding = site.id;
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
        lead.skipBuilding = site.id;
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
            lead.skipBuilding = site.id;
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
