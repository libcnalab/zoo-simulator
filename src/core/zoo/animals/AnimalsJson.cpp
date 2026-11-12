// SPDX-License-Identifier: MIT
#include "zoo/animals/AnimalsJson.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace zoo {
namespace {
using Json = nlohmann::json;
Json Point(Vec2 point) {
    return Json::array({point.x, point.z});
}
Vec2 ReadPoint(const Json& value, const char* label) {
    if (!value.is_array() || value.size() != 2 || !value[0].is_number() || !value[1].is_number()) {
        throw std::runtime_error(std::string(label) + " must be a two-number array");
    }
    return {value[0].get<float>(), value[1].get<float>()};
}
std::uint64_t ReadUnsigned(const Json& value, const char* label) {
    if (!value.is_number_unsigned())
        throw std::runtime_error(std::string(label) + " must be unsigned");
    return value.get<std::uint64_t>();
}
int ReadInt(const Json& value, const char* label) {
    if (!value.is_number_integer())
        throw std::runtime_error(std::string(label) + " must be an integer");
    return value.get<int>();
}
float ReadFloat(const Json& value, const char* label) {
    if (!value.is_number())
        throw std::runtime_error(std::string(label) + " must be a number");
    return value.get<float>();
}
bool ReadBool(const Json& value, const char* label) {
    if (!value.is_boolean())
        throw std::runtime_error(std::string(label) + " must be boolean");
    return value.get<bool>();
}
std::string ReadString(const Json& value, const char* label) {
    if (!value.is_string())
        throw std::runtime_error(std::string(label) + " must be string");
    return value.get<std::string>();
}
template <typename T>
T ReadId(const Json& value, const char* label) {
    const std::uint64_t raw = ReadUnsigned(value, label);
    if (raw > std::numeric_limits<std::uint32_t>::max())
        throw std::runtime_error(std::string(label) + " out of range");
    return T{static_cast<std::uint32_t>(raw)};
}
Json Encode(const Animal& a, const SpeciesCatalog& species) {
    Json route = Json::array();
    for (Vec2 point : a.route)
        route.push_back(Point(point));
    return Json{{"id", a.id.value},
                {"species", species.All()[static_cast<std::size_t>(a.species)].id},
                {"name", a.name},
                {"sex", ToString(a.sex)},
                {"born_tick", a.bornTick},
                {"mother", a.mother.value},
                {"father", a.father.value},
                {"position", Point(a.position)},
                {"heading", a.heading},
                {"speed", a.speed},
                {"hunger", a.hunger},
                {"thirst", a.thirst},
                {"fatigue", a.fatigue},
                {"boredom", a.boredom},
                {"stress", a.stress},
                {"health", a.health},
                {"welfare", a.welfare},
                {"issues", a.issues},
                {"sick", a.sick},
                {"treated", a.treated},
                {"alerts", a.alerts},
                {"activity", ToString(a.activity)},
                {"moving", a.moving},
                {"activity_ticks", a.activityTicks},
                {"activity_left", a.activityLeft},
                {"target", Point(a.target)},
                {"target_prop", a.targetProp.value},
                {"shelter", a.shelter.value},
                {"route", route},
                {"route_index", a.routeIndex},
                {"home", a.home.value},
                {"loose", a.loose},
                {"breeding", a.breeding},
                {"due_tick", a.dueTick},
                {"mate", a.mate.value},
                {"last_birth_tick", a.lastBirthTick}};
}
Animal DecodeAnimal(const Json& value, const SpeciesCatalog& species) {
    if (!value.is_object())
        throw std::runtime_error("animal must be object");
    Animal a;
    a.id = ReadId<AnimalId>(value.at("id"), "animal id");
    const std::string speciesName = ReadString(value.at("species"), "species");
    a.species = species.IndexOf(speciesName);
    if (a.species < 0)
        throw std::runtime_error("unknown species: " + speciesName);
    a.name = ReadString(value.at("name"), "name");
    const std::string sex = ReadString(value.at("sex"), "sex");
    if (sex == "female")
        a.sex = Sex::Female;
    else if (sex == "male")
        a.sex = Sex::Male;
    else
        throw std::runtime_error("unknown sex name");
    if (!value.at("born_tick").is_number_integer())
        throw std::runtime_error("born_tick must be integer");
    a.bornTick = value.at("born_tick").get<std::int64_t>();
    a.mother = ReadId<AnimalId>(value.at("mother"), "mother");
    a.father = ReadId<AnimalId>(value.at("father"), "father");
    a.position = ReadPoint(value.at("position"), "position");
    a.heading = ReadFloat(value.at("heading"), "heading");
    a.speed = ReadFloat(value.at("speed"), "speed");
    a.hunger = ReadFloat(value.at("hunger"), "hunger");
    a.thirst = ReadFloat(value.at("thirst"), "thirst");
    a.fatigue = ReadFloat(value.at("fatigue"), "fatigue");
    a.boredom = ReadFloat(value.at("boredom"), "boredom");
    a.stress = ReadFloat(value.at("stress"), "stress");
    a.health = ReadFloat(value.at("health"), "health");
    a.welfare = ReadFloat(value.at("welfare"), "welfare");
    const std::uint64_t issues = ReadUnsigned(value.at("issues"), "issues");
    if (issues > std::numeric_limits<std::uint32_t>::max())
        throw std::runtime_error("issues out of range");
    a.issues = static_cast<std::uint32_t>(issues);
    a.sick = ReadBool(value.at("sick"), "sick");
    a.treated = ReadBool(value.at("treated"), "treated");
    const std::uint64_t alerts = ReadUnsigned(value.at("alerts"), "alerts");
    if (alerts > 3U) {
        throw std::runtime_error("alerts out of range");
    }
    a.alerts = static_cast<std::uint8_t>(alerts);
    const std::string activity = ReadString(value.at("activity"), "activity");
    bool found = false;
    for (int i = 0; i < static_cast<int>(AnimalActivity::Count); ++i) {
        const auto candidate = static_cast<AnimalActivity>(i);
        if (ToString(candidate) == activity) {
            a.activity = candidate;
            found = true;
            break;
        }
    }
    if (!found)
        throw std::runtime_error("unknown activity name");
    a.moving = ReadBool(value.at("moving"), "moving");
    a.activityTicks = ReadInt(value.at("activity_ticks"), "activity_ticks");
    a.activityLeft = ReadInt(value.at("activity_left"), "activity_left");
    a.target = ReadPoint(value.at("target"), "target");
    a.targetProp = ReadId<PropId>(value.at("target_prop"), "target_prop");
    a.shelter = ReadId<BuildingId>(value.at("shelter"), "shelter");
    if (!value.at("route").is_array())
        throw std::runtime_error("route must be array");
    for (const Json& point : value.at("route"))
        a.route.push_back(ReadPoint(point, "route point"));
    const std::uint64_t routeIndex = ReadUnsigned(value.at("route_index"), "route_index");
    if (routeIndex > a.route.size())
        throw std::runtime_error("route_index out of range");
    a.routeIndex = static_cast<std::uint32_t>(routeIndex);
    a.home = ReadId<EnclosureId>(value.at("home"), "home");
    a.loose = ReadBool(value.at("loose"), "loose");
    a.breeding = ReadBool(value.at("breeding"), "breeding");
    if (!value.at("due_tick").is_number_integer() || !value.at("last_birth_tick").is_number_integer())
        throw std::runtime_error("birth ticks must be integers");
    a.dueTick = value.at("due_tick").get<std::int64_t>();
    a.mate = ReadId<AnimalId>(value.at("mate"), "mate");
    a.lastBirthTick = value.at("last_birth_tick").get<std::int64_t>();
    return a;
}
}  // namespace

Json ToJson(const Animals& animals, const SpeciesCatalog& species) {
    Json list = Json::array();
    for (const Animal& animal : AnimalsAccess::List(animals))
        list.push_back(Encode(animal, species));
    Json care = Json::array();
    for (const auto& item : AnimalsAccess::Care(animals))
        care.push_back(Json{{"id", item.id.value}, {"dirt", item.dirt}});
    return Json{{"next_id", AnimalsAccess::NextId(animals)},
                {"rng", Json::array({AnimalsAccess::RngState(animals), AnimalsAccess::RngStream(animals)})},
                {"births", AnimalsAccess::Births(animals)},
                {"deaths", AnimalsAccess::Deaths(animals)},
                {"escapes", AnimalsAccess::Escapes(animals)},
                {"animals", list},
                {"care", care}};
}
std::optional<Animals> AnimalsFromJson(const Json& json, const SpeciesCatalog& species, std::string* error) {
    try {
        if (!json.is_object() || !json.at("rng").is_array() || json.at("rng").size() != 2)
            throw std::runtime_error("animals JSON has invalid root or rng");
        const std::uint64_t next = ReadUnsigned(json.at("next_id"), "next_id");
        if (next == 0 || next > std::numeric_limits<std::uint32_t>::max())
            throw std::runtime_error("next_id out of range");
        if (!json.at("animals").is_array() || !json.at("care").is_array())
            throw std::runtime_error("animals and care must be arrays");
        std::vector<Animal> animals;
        std::uint32_t previous = 0;
        for (const Json& value : json.at("animals")) {
            Animal animal = DecodeAnimal(value, species);
            if (animal.id.value <= previous || animal.id.value >= next)
                throw std::runtime_error("animal ids must ascend below next_id");
            previous = animal.id.value;
            animals.push_back(std::move(animal));
        }
        auto care = AnimalsAccess::EmptyCare();
        for (const Json& value : json.at("care")) {
            if (!value.is_object())
                throw std::runtime_error("care entry must be object");
            care.push_back(
                {ReadId<EnclosureId>(value.at("id"), "care id"), ReadFloat(value.at("dirt"), "dirt"), 0});
        }
        const int births = ReadInt(json.at("births"), "births");
        const int deaths = ReadInt(json.at("deaths"), "deaths");
        const int escapes = ReadInt(json.at("escapes"), "escapes");
        Animals result;
        AnimalsAccess::Restore(result, std::move(animals), std::move(care), static_cast<std::uint32_t>(next),
                               ReadUnsigned(json.at("rng")[0], "rng state"),
                               ReadUnsigned(json.at("rng")[1], "rng stream"), births, deaths, escapes);
        if (error != nullptr)
            error->clear();
        return result;
    } catch (const std::exception& exception) {
        if (error != nullptr)
            *error = exception.what();
        return std::nullopt;
    }
}
}  // namespace zoo
