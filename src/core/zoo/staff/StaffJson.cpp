// SPDX-License-Identifier: MIT
#include "zoo/staff/StaffJson.hpp"

#include <cmath>
#include <stdexcept>

namespace zoo {

namespace {

using Json = nlohmann::json;

constexpr int kVersion = 1;
/// More than any zoo engages; a guard against a document that is not ours.
constexpr std::size_t kMostMembers = 256;
constexpr std::size_t kMostStops = 4096;

Json Point(Vec2 point) {
    return Json::array({point.x, point.z});
}

Vec2 ReadPoint(const Json& value) {
    if (!value.is_array() || value.size() != 2 || !value[0].is_number() || !value[1].is_number()) {
        throw std::runtime_error("a point must be an array of two numbers");
    }
    const Vec2 point{value[0].get<float>(), value[1].get<float>()};
    if (!std::isfinite(point.x) || !std::isfinite(point.z)) {
        throw std::runtime_error("a point must be finite");
    }
    return point;
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

Json Encode(const StaffStop& stop) {
    return Json{{"enclosure", stop.enclosure.value},
                {"prop", stop.prop.value},
                {"feed_hours", Json::array({stop.feedHours[0], stop.feedHours[1]})},
                {"fed_day", Json::array({stop.fedDay[0], stop.fedDay[1]})}};
}

StaffStop DecodeStop(const Json& json) {
    StaffStop stop;
    stop.enclosure = EnclosureId{json.at("enclosure").get<std::uint32_t>()};
    stop.prop = PropId{json.at("prop").get<std::uint32_t>()};
    const Json& hours = json.at("feed_hours");
    const Json& days = json.at("fed_day");
    if (!hours.is_array() || hours.size() != 2 || !days.is_array() || days.size() != 2) {
        throw std::runtime_error("feed_hours and fed_day must hold two values each");
    }
    for (std::size_t i = 0; i < 2; ++i) {
        stop.feedHours[i] = hours[i].get<float>();
        stop.fedDay[i] = days[i].get<std::int64_t>();
        if (!(stop.feedHours[i] < 24.0f)) {
            throw std::runtime_error("a feeding hour must lie before 24");
        }
    }
    return stop;
}

Json Encode(const StaffTask& task) {
    return Json{{"work", ToString(task.work)}, {"approach", ToString(task.approach)},
                {"place", Point(task.place)},  {"facing", task.facing},
                {"ticks", task.ticks},         {"enclosure", task.enclosure.value},
                {"prop", task.prop.value}};
}

StaffTask DecodeTask(const Json& json) {
    StaffTask task;
    task.work = ReadEnum<StaffActivity>(json.at("work"), "activity");
    task.approach = ReadEnum<StaffActivity>(json.at("approach"), "activity");
    task.place = ReadPoint(json.at("place"));
    task.facing = json.at("facing").get<float>();
    task.ticks = json.at("ticks").get<std::int32_t>();
    task.enclosure = EnclosureId{json.at("enclosure").get<std::uint32_t>()};
    task.prop = PropId{json.at("prop").get<std::uint32_t>()};
    if (!std::isfinite(task.facing) || task.ticks < 0) {
        throw std::runtime_error("a task needs a finite facing and no negative duration");
    }
    return task;
}

Json Encode(const StaffMember& m) {
    Json round = Json::array();
    for (const StaffStop& stop : m.round) {
        round.push_back(Encode(stop));
    }
    Json tasks = Json::array();
    for (const StaffTask& task : m.tasks) {
        tasks.push_back(Encode(task));
    }
    Json route = Json::array();
    for (const Vec2 point : m.route) {
        route.push_back(Point(point));
    }
    return Json{{"id", m.id.value},
                {"role", ToString(m.role)},
                {"look", m.look},
                {"round", std::move(round)},
                {"stop", m.stop},
                {"stops_done", m.stopsDone},
                {"position", Point(m.position)},
                {"heading", m.heading},
                {"speed", m.speed},
                {"activity", ToString(m.activity)},
                {"tasks", std::move(tasks)},
                {"task", m.task},
                {"working", m.working},
                {"ticks_left", m.ticksLeft},
                {"route", std::move(route)},
                {"route_index", m.routeIndex}};
}

StaffMember DecodeMember(const Json& json) {
    StaffMember m;
    m.id = StaffId{json.at("id").get<std::uint32_t>()};
    m.role = ReadEnum<StaffRole>(json.at("role"), "role");
    m.look = json.at("look").get<std::uint32_t>();
    const Json& round = json.at("round");
    if (!round.is_array() || round.empty() || round.size() > kMostStops) {
        throw std::runtime_error("a round needs between one and a few thousand stops");
    }
    for (const Json& stop : round) {
        m.round.push_back(DecodeStop(stop));
    }
    m.stop = json.at("stop").get<std::uint32_t>();
    m.stopsDone = json.at("stops_done").get<std::uint32_t>();
    m.position = ReadPoint(json.at("position"));
    m.heading = json.at("heading").get<float>();
    m.speed = json.at("speed").get<float>();
    m.activity = ReadEnum<StaffActivity>(json.at("activity"), "activity");
    const Json& tasks = json.at("tasks");
    if (!tasks.is_array() || tasks.size() > kMostStops) {
        throw std::runtime_error("the tasks must be a list");
    }
    for (const Json& task : tasks) {
        m.tasks.push_back(DecodeTask(task));
    }
    m.task = json.at("task").get<std::uint32_t>();
    m.working = json.at("working").get<bool>();
    m.ticksLeft = json.at("ticks_left").get<std::int32_t>();
    const Json& route = json.at("route");
    if (!route.is_array()) {
        throw std::runtime_error("the route must be a list of points");
    }
    for (const Json& point : route) {
        m.route.push_back(ReadPoint(point));
    }
    m.routeIndex = json.at("route_index").get<std::uint32_t>();
    if (m.stop >= m.round.size() || m.task > m.tasks.size() || m.routeIndex > m.route.size() ||
        !std::isfinite(m.heading) || !(m.speed >= 0.0f) || m.ticksLeft < 0) {
        throw std::runtime_error("a member's stop, task, route index or body is out of range");
    }
    return m;
}

}  // namespace

Json StaffJsonAccess::Save(const Staff& staff) {
    Json members = Json::array();
    for (const StaffMember& member : staff.members_) {
        members.push_back(Encode(member));
    }
    return Json{{"version", kVersion}, {"members", std::move(members)}, {"next_id", staff.nextId_}};
}

Staff StaffJsonAccess::Load(const Json& json) {
    if (!json.is_object()) {
        throw std::runtime_error("the staff must be an object");
    }
    if (const int version = json.at("version").get<int>(); version != kVersion) {
        throw std::runtime_error("unsupported staff version " + std::to_string(version));
    }
    Staff staff;
    const Json& members = json.at("members");
    if (!members.is_array() || members.size() > kMostMembers) {
        throw std::runtime_error("too many members of staff");
    }
    for (const Json& item : members) {
        StaffMember member = DecodeMember(item);
        if (!member.id || (!staff.members_.empty() && !(staff.members_.back().id < member.id))) {
            throw std::runtime_error("staff ids must be ascending");
        }
        staff.members_.push_back(std::move(member));
    }
    staff.nextId_ = json.at("next_id").get<std::uint32_t>();
    if (staff.nextId_ == 0 || (!staff.members_.empty() && staff.nextId_ <= staff.members_.back().id.value)) {
        throw std::runtime_error("next_id must lie beyond the staff");
    }
    return staff;
}

Json ToJson(const Staff& staff) {
    return StaffJsonAccess::Save(staff);
}

std::optional<Staff> StaffFromJson(const Json& json, std::string* error) {
    try {
        return StaffJsonAccess::Load(json);
    } catch (const std::exception& e) {
        if (error != nullptr) {
            *error = std::string("staff: ") + e.what();
        }
        return std::nullopt;
    }
}

}  // namespace zoo
