// SPDX-License-Identifier: MIT
#include "zoo/build/ObjectsJson.hpp"
#include <cmath>
#include <limits>
#include <stdexcept>
namespace zoo {
namespace {
using Json = nlohmann::json;
Json Vec(Vec2 v) {
    return Json::array({v.x, v.z});
}
Vec2 ReadVec(const Json& j, const char* field) {
    if (!j.is_array() || j.size() != 2 || !j[0].is_number() || !j[1].is_number())
        throw std::runtime_error(std::string(field) + " must be a two-number array");
    Vec2 v{j[0].get<float>(), j[1].get<float>()};
    if (!std::isfinite(v.x) || !std::isfinite(v.z))
        throw std::runtime_error(std::string(field) + " must be finite");
    return v;
}
std::uint32_t U32(const Json& j, const char* field) {
    if (!j.is_number_integer())
        throw std::runtime_error(std::string(field) + " must be an integer");
    const auto n = j.get<std::int64_t>();
    if (n < 0 || static_cast<std::uint64_t>(n) > std::numeric_limits<std::uint32_t>::max())
        throw std::runtime_error(std::string(field) + " is out of range");
    return static_cast<std::uint32_t>(n);
}
float Number(const Json& j, const char* field) {
    if (!j.is_number())
        throw std::runtime_error(std::string(field) + " must be a number");
    float n = j.get<float>();
    if (!std::isfinite(n))
        throw std::runtime_error(std::string(field) + " must be finite");
    return n;
}
}  // namespace
Json ToJson(const ZooObjects& objects, const Catalog& catalog) {
    Json bs = Json::array(), ps = Json::array();
    for (const auto& b : ObjectsAccess::Buildings(objects)) {
        if (b.def < 0 || static_cast<std::size_t>(b.def) >= catalog.buildings.size())
            throw std::runtime_error("building has invalid catalog index");
        bs.push_back({{"id", b.id.value},
                      {"def", catalog.buildings[static_cast<std::size_t>(b.def)].id},
                      {"origin", {b.origin.x, b.origin.z}},
                      {"facing", static_cast<int>(b.facing)},
                      {"condition", b.condition},
                      {"itemPrice", b.itemPrice},
                      {"open", b.open},
                      {"servedToday", b.servedToday},
                      {"servedTotal", b.servedTotal}});
    }
    for (const auto& p : ObjectsAccess::Props(objects)) {
        if (p.def < 0 || static_cast<std::size_t>(p.def) >= catalog.props.size())
            throw std::runtime_error("prop has invalid catalog index");
        Json item = {{"id", p.id.value},
                     {"def", catalog.props[static_cast<std::size_t>(p.def)].id},
                     {"position", Vec(p.position)},
                     {"heading", p.heading},
                     {"scale", p.scale},
                     {"amount", p.amount},
                     {"food", nullptr},
                     {"filledDay", p.filledDay}};
        if (p.food >= 0) {
            if (static_cast<std::size_t>(p.food) >= catalog.foods.size())
                throw std::runtime_error("prop has invalid food index");
            item["food"] = catalog.foods[static_cast<std::size_t>(p.food)].id;
        }
        ps.push_back(std::move(item));
    }
    return {{"nextBuilding", ObjectsAccess::NextBuilding(objects)},
            {"nextProp", ObjectsAccess::NextProp(objects)},
            {"buildings", std::move(bs)},
            {"props", std::move(ps)}};
}
std::optional<ZooObjects> ObjectsFromJson(const Json& j, const Catalog& catalog, std::string* error) {
    if (error)
        error->clear();
    try {
        if (!j.is_object())
            throw std::runtime_error("objects document must be an object");
        for (const char* k : {"nextBuilding", "nextProp", "buildings", "props"})
            if (!j.contains(k))
                throw std::runtime_error(std::string("missing field: ") + k);
        const auto nextB = U32(j["nextBuilding"], "nextBuilding"), nextP = U32(j["nextProp"], "nextProp");
        if (nextB == 0 || nextP == 0)
            throw std::runtime_error("id counters must be positive");
        if (!j["buildings"].is_array() || !j["props"].is_array())
            throw std::runtime_error("buildings and props must be arrays");
        std::vector<Building> bs;
        std::vector<Prop> ps;
        std::uint32_t prev = 0, maxB = 0, maxP = 0;
        for (const auto& v : j["buildings"]) {
            if (!v.is_object())
                throw std::runtime_error("building must be an object");
            Building b;
            b.id = BuildingId{U32(v.at("id"), "building id")};
            if (b.id.value <= prev || b.id.value == 0)
                throw std::runtime_error("building ids must be positive and ascending");
            prev = b.id.value;
            maxB = prev;
            if (!v.at("def").is_string())
                throw std::runtime_error("building def must be a string id");
            b.def = catalog.BuildingIndex(v.at("def").get<std::string>());
            if (b.def < 0)
                throw std::runtime_error("unknown building definition id: " + v.at("def").get<std::string>());
            const auto o = v.at("origin");
            if (!o.is_array() || o.size() != 2 || !o[0].is_number_integer() || !o[1].is_number_integer())
                throw std::runtime_error("building origin must be two integers");
            b.origin = {o[0].get<int>(), o[1].get<int>()};
            const int f = v.at("facing").get<int>();
            if (f < 0 || f > 3)
                throw std::runtime_error("building facing is out of range");
            b.facing = static_cast<Dir>(f);
            b.condition = Number(v.at("condition"), "condition");
            b.itemPrice = v.at("itemPrice").get<Money>();
            b.open = v.at("open").get<bool>();
            b.servedToday = v.at("servedToday").get<int>();
            b.servedTotal = v.at("servedTotal").get<int>();
            bs.push_back(b);
        }
        prev = 0;
        for (const auto& v : j["props"]) {
            if (!v.is_object())
                throw std::runtime_error("prop must be an object");
            Prop p;
            p.id = PropId{U32(v.at("id"), "prop id")};
            if (p.id.value <= prev || p.id.value == 0)
                throw std::runtime_error("prop ids must be positive and ascending");
            prev = p.id.value;
            maxP = prev;
            if (!v.at("def").is_string())
                throw std::runtime_error("prop def must be a string id");
            p.def = catalog.PropIndex(v.at("def").get<std::string>());
            if (p.def < 0)
                throw std::runtime_error("unknown prop definition id: " + v.at("def").get<std::string>());
            p.position = ReadVec(v.at("position"), "position");
            p.heading = Number(v.at("heading"), "heading");
            p.scale = Number(v.at("scale"), "scale");
            p.amount = Number(v.at("amount"), "amount");
            if (!v.at("food").is_null()) {
                if (!v.at("food").is_string())
                    throw std::runtime_error("food must be a string id or null");
                p.food = catalog.FoodIndex(v.at("food").get<std::string>());
                if (p.food < 0)
                    throw std::runtime_error("unknown food id: " + v.at("food").get<std::string>());
            }
            p.filledDay = Number(v.at("filledDay"), "filledDay");
            ps.push_back(p);
        }
        if (nextB <= maxB || nextP <= maxP)
            throw std::runtime_error("id counters must be above every used id");
        ZooObjects out;
        ObjectsAccess::Set(out, std::move(bs), std::move(ps), nextB, nextP, 1);
        return out;
    } catch (const std::exception& e) {
        if (error)
            *error = e.what();
        return std::nullopt;
    }
}
}  // namespace zoo
