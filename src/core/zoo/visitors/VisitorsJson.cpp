// SPDX-License-Identifier: MIT
#include "zoo/visitors/VisitorsJson.hpp"

#include <stdexcept>

namespace zoo {

namespace {

using Json = nlohmann::json;

constexpr int kVersion = 1;

Json Point(Vec2 point) {
    return Json::array({point.x, point.z});
}

Vec2 ReadPoint(const Json& value) {
    if (!value.is_array() || value.size() != 2 || !value[0].is_number() || !value[1].is_number()) {
        throw std::runtime_error("a point must be an array of two numbers");
    }
    return {value[0].get<float>(), value[1].get<float>()};
}

template <typename E>
E ReadEnum(const Json& value, const char* what) {
    const std::string name = value.get<std::string>();
    for (int i = 0; i < static_cast<int>(E::Count); ++i) {
        if (ToString(static_cast<E>(i)) == name) {
            return static_cast<E>(i);
        }
    }
    throw std::runtime_error(std::string("unknown ") + what + " '" + name + "'");
}

float ReadUnit(const Json& value, const char* field) {
    const float number = value.at(field).get<float>();
    if (!(number >= 0.0f && number <= 1.0f)) {
        throw std::runtime_error(std::string(field) + " must be within 0..1");
    }
    return number;
}

Json Encode(const Visitor& v) {
    Json route = Json::array();
    for (const Vec2 point : v.route) {
        route.push_back(Point(point));
    }
    return Json{{"id", v.id.value},
                {"look", v.look},
                {"child", v.child},
                {"height", v.heightM},
                {"group", v.group},
                {"leader", v.leader},
                {"position", Point(v.position)},
                {"heading", v.heading},
                {"speed", v.speed},
                {"activity", ToString(v.activity)},
                {"inside", v.inside.value},
                {"seat", v.seat.value},
                {"hunger", v.hunger},
                {"thirst", v.thirst},
                {"bladder", v.bladder},
                {"tiredness", v.tiredness},
                {"happiness", v.happiness},
                {"education", v.education},
                {"thought", ToString(v.thought)},
                {"thought_tick", v.thoughtTick},
                {"arrived_tick", v.arrivedTick},
                {"goal", ToString(v.goal)},
                {"target_enclosure", v.targetEnclosure.value},
                {"target_building", v.targetBuilding.value},
                {"target_prop", v.targetProp.value},
                {"target", Point(v.target)},
                {"face", v.face},
                {"slot", v.slot},
                {"route", std::move(route)},
                {"route_index", v.routeIndex},
                {"cash", v.cash},
                {"spent", v.spent},
                {"reroute", v.reroute},
                {"activity_until", v.activityUntil},
                {"queued_tick", v.queuedTick},
                {"previous_goal", ToString(v.previousGoal)},
                {"seen_species", v.seenSpecies},
                {"seen_enclosures", v.seenEnclosures},
                {"unmet", v.unmet},
                {"done", v.done},
                {"skip_building", v.skipBuilding.value},
                {"litter", v.litter},
                {"lost", v.lost},
                {"ticket_paid", v.ticketPaid},
                {"served", v.served}};
}

Visitor Decode(const Json& j) {
    if (!j.is_object()) {
        throw std::runtime_error("a visitor must be an object");
    }
    Visitor v;
    v.id = VisitorId{j.at("id").get<std::uint32_t>()};
    v.look = j.at("look").get<std::uint32_t>();
    v.child = j.at("child").get<bool>();
    v.heightM = j.at("height").get<float>();
    v.group = j.at("group").get<std::uint32_t>();
    v.leader = j.at("leader").get<bool>();
    v.position = ReadPoint(j.at("position"));
    v.heading = j.at("heading").get<float>();
    v.speed = j.at("speed").get<float>();
    v.activity = ReadEnum<VisitorActivity>(j.at("activity"), "activity");
    v.inside = BuildingId{j.at("inside").get<std::uint32_t>()};
    v.seat = PropId{j.at("seat").get<std::uint32_t>()};
    v.hunger = ReadUnit(j, "hunger");
    v.thirst = ReadUnit(j, "thirst");
    v.bladder = ReadUnit(j, "bladder");
    v.tiredness = ReadUnit(j, "tiredness");
    v.happiness = ReadUnit(j, "happiness");
    v.education = ReadUnit(j, "education");
    v.thought = ReadEnum<VisitorThought>(j.at("thought"), "thought");
    v.thoughtTick = j.at("thought_tick").get<std::int64_t>();
    v.arrivedTick = j.at("arrived_tick").get<std::int64_t>();
    v.goal = ReadEnum<VisitorGoal>(j.at("goal"), "goal");
    v.targetEnclosure = EnclosureId{j.at("target_enclosure").get<std::uint32_t>()};
    v.targetBuilding = BuildingId{j.at("target_building").get<std::uint32_t>()};
    v.targetProp = PropId{j.at("target_prop").get<std::uint32_t>()};
    v.target = ReadPoint(j.at("target"));
    v.face = j.at("face").get<float>();
    v.slot = j.at("slot").get<std::uint8_t>();
    for (const Json& point : j.at("route")) {
        v.route.push_back(ReadPoint(point));
    }
    v.routeIndex = j.at("route_index").get<std::uint32_t>();
    if (v.routeIndex > v.route.size()) {
        throw std::runtime_error("route_index beyond the route");
    }
    v.cash = j.at("cash").get<Money>();
    v.spent = j.at("spent").get<Money>();
    v.reroute = j.at("reroute").get<bool>();
    v.activityUntil = j.at("activity_until").get<std::int64_t>();
    v.queuedTick = j.at("queued_tick").get<std::int64_t>();
    v.previousGoal = ReadEnum<VisitorGoal>(j.at("previous_goal"), "goal");
    v.seenSpecies = j.at("seen_species").get<std::uint64_t>();
    v.seenEnclosures = j.at("seen_enclosures").get<std::vector<std::uint32_t>>();
    v.unmet = j.at("unmet").get<std::uint8_t>();
    v.done = j.at("done").get<std::uint8_t>();
    v.skipBuilding = BuildingId{j.at("skip_building").get<std::uint32_t>()};
    v.litter = j.at("litter").get<std::uint8_t>();
    v.lost = j.at("lost").get<std::uint8_t>();
    v.ticketPaid = j.at("ticket_paid").get<bool>();
    v.served = j.at("served").get<bool>();
    return v;
}

}  // namespace

Json VisitorsJsonAccess::Save(const Visitors& visitors) {
    Json list = Json::array();
    for (const Visitor& v : visitors.visitors_) {
        list.push_back(Encode(v));
    }
    Json litter = Json::array();
    for (const Litter& piece : visitors.litter_) {
        litter.push_back(Json::array({piece.position.x, piece.position.z, piece.tick}));
    }
    return Json{{"version", kVersion},
                {"visitors", std::move(list)},
                {"litter", std::move(litter)},
                {"next_id", visitors.nextId_},
                {"next_group", visitors.nextGroup_},
                {"rng_state", visitors.rng_.State()},
                {"rng_stream", visitors.rng_.Stream()},
                {"ticket_price", visitors.ticketPrice_},
                {"open", visitors.open_},
                {"arrival_remainder", visitors.arrivalRemainder_},
                {"rating", visitors.rating_},
                {"education_rating", visitors.educationRating_},
                {"left_weight", visitors.leftWeight_},
                {"arrived_today", visitors.arrivedToday_},
                {"left_today", visitors.leftToday_},
                {"arrived_ever", visitors.arrivedEver_}};
}

Visitors VisitorsJsonAccess::Load(const Json& json) {
    if (!json.is_object()) {
        throw std::runtime_error("the visitors must be an object");
    }
    if (const int version = json.at("version").get<int>(); version != kVersion) {
        throw std::runtime_error("unsupported visitors version " + std::to_string(version));
    }
    Visitors visitors(0);
    const Json& list = json.at("visitors");
    if (!list.is_array() || list.size() > static_cast<std::size_t>(Visitors::kMaxVisitors)) {
        throw std::runtime_error("too many visitors");
    }
    visitors.visitors_.reserve(list.size());
    for (const Json& item : list) {
        Visitor v = Decode(item);
        const Visitor* previous = visitors.visitors_.empty() ? nullptr : &visitors.visitors_.back();
        if (!v.id || v.group == 0 || (previous != nullptr && !(previous->id < v.id))) {
            throw std::runtime_error("visitor ids must be ascending and groups numbered");
        }
        // A group is a run of members behind its leader.
        const bool starts = previous == nullptr || previous->group != v.group;
        if (starts != v.leader) {
            throw std::runtime_error("every group needs its leader first");
        }
        visitors.visitors_.push_back(std::move(v));
    }
    const Json& litter = json.at("litter");
    if (!litter.is_array() || litter.size() > Visitors::kMaxLitter) {
        throw std::runtime_error("too much litter");
    }
    for (const Json& piece : litter) {
        if (!piece.is_array() || piece.size() != 3) {
            throw std::runtime_error("a piece of litter must be [x, z, tick]");
        }
        visitors.litter_.push_back(
            {{piece[0].get<float>(), piece[1].get<float>()}, piece[2].get<std::int64_t>()});
    }
    visitors.nextId_ = json.at("next_id").get<std::uint32_t>();
    visitors.nextGroup_ = json.at("next_group").get<std::uint32_t>();
    if (!visitors.visitors_.empty() && (visitors.nextId_ <= visitors.visitors_.back().id.value ||
                                        visitors.nextGroup_ <= visitors.visitors_.back().group)) {
        throw std::runtime_error("next_id and next_group must lie beyond the visitors");
    }
    visitors.rng_.Restore(json.at("rng_state").get<std::uint64_t>(),
                          json.at("rng_stream").get<std::uint64_t>());
    visitors.SetTicketPrice(json.at("ticket_price").get<Money>());
    visitors.open_ = json.at("open").get<bool>();
    visitors.arrivalRemainder_ = json.at("arrival_remainder").get<float>();
    visitors.rating_ = ReadUnit(json, "rating");
    visitors.educationRating_ = ReadUnit(json, "education_rating");
    visitors.leftWeight_ = json.at("left_weight").get<float>();
    if (!(visitors.leftWeight_ >= 1.0f) ||
        !(visitors.arrivalRemainder_ > -static_cast<float>(Visitors::kMaxGroupSize))) {
        throw std::runtime_error("left_weight or arrival_remainder out of range");
    }
    visitors.arrivedToday_ = json.at("arrived_today").get<int>();
    visitors.leftToday_ = json.at("left_today").get<int>();
    visitors.arrivedEver_ = json.at("arrived_ever").get<std::int64_t>();
    return visitors;
}

Json ToJson(const Visitors& visitors) {
    return VisitorsJsonAccess::Save(visitors);
}

std::optional<Visitors> VisitorsFromJson(const Json& json, std::string* error) {
    try {
        return VisitorsJsonAccess::Load(json);
    } catch (const std::exception& e) {
        if (error != nullptr) {
            *error = std::string("visitors: ") + e.what();
        }
        return std::nullopt;
    }
}

}  // namespace zoo
