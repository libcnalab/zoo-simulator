// SPDX-License-Identifier: MIT
#include "zoo/build/Catalog.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <set>
#include <stdexcept>

#include <nlohmann/json.hpp>

namespace zoo {
namespace {
using Json = nlohmann::json;
constexpr std::array<std::string_view, 8> kFenceStyles = {"rail", "mesh",  "bars",  "glass",
                                                          "wall", "cable", "hedge", "moat"};
constexpr std::array<std::string_view, 9> kBuildingRoles = {
    "animal_shelter", "food_stall", "drink_stall",      "restroom",       "gift_shop",
    "staff_room",     "vet_clinic", "education_center", "research_center"};
constexpr std::array<std::string_view, 3> kPlacements = {"anywhere", "enclosure", "public"};
constexpr std::array<std::string_view, 11> kPropRoles = {"tree",         "bush",       "rock",      "feeder",
                                                         "water_trough", "enrichment", "bench",     "bin",
                                                         "lamp",         "info_board", "decoration"};

std::string Label(std::string_view file, std::string_view id, std::string_view field) {
    return std::string(file) + ": " + (id.empty() ? "<entry>" : std::string(id)) + ": " + std::string(field);
}
void Require(bool ok, const std::string& label, const std::string& rule) {
    if (!ok)
        throw std::runtime_error(label + " " + rule);
}
const Json& Need(const Json& j, const char* key, const std::string& file, const std::string& id) {
    Require(j.contains(key), Label(file, id, key), "is required");
    return j.at(key);
}
std::string String(const Json& j, const char* key, const std::string& file, const std::string& id) {
    const Json& v = Need(j, key, file, id);
    Require(v.is_string(), Label(file, id, key), "must be a string");
    return v.get<std::string>();
}
double Number(const Json& j, const char* key, const std::string& file, const std::string& id) {
    const Json& v = Need(j, key, file, id);
    Require(v.is_number(), Label(file, id, key), "must be a number");
    const double n = v.get<double>();
    Require(std::isfinite(n), Label(file, id, key), "must be finite");
    return n;
}
int Integer(const Json& j, const char* key, const std::string& file, const std::string& id) {
    const Json& v = Need(j, key, file, id);
    Require(v.is_number_integer(), Label(file, id, key), "must be an integer");
    const auto n = v.get<std::int64_t>();
    Require(n >= std::numeric_limits<int>::min() && n <= std::numeric_limits<int>::max(),
            Label(file, id, key), "is out of range");
    return static_cast<int>(n);
}
bool Boolean(const Json& j, const char* key, const std::string& file, const std::string& id) {
    const Json& v = Need(j, key, file, id);
    Require(v.is_boolean(), Label(file, id, key), "must be a boolean");
    return v.get<bool>();
}
Money WholeCost(const Json& j, const char* key, const std::string& file, const std::string& id) {
    const double n = Number(j, key, file, id);
    Require(n >= 0.0 && n <= 9.0e15 && std::floor(n) == n, Label(file, id, key),
            "must be non-negative whole dollars");
    return Dollars(static_cast<std::int64_t>(n));
}
Money UnitCost(const Json& j, const char* key, const std::string& file, const std::string& id) {
    const double n = Number(j, key, file, id);
    Require(n >= 0.0 && n <= 9.0e15, Label(file, id, key), "must be non-negative");
    return static_cast<Money>(std::llround(n * 100.0));
}
float F(const Json& j, const char* key, const std::string& file, const std::string& id) {
    const double n = Number(j, key, file, id);
    Require(std::abs(n) <= static_cast<double>(std::numeric_limits<float>::max()), Label(file, id, key),
            "is out of range");
    return static_cast<float>(n);
}
Color3 Color(const Json& j, const char* key, const std::string& file, const std::string& id) {
    const Json& v = Need(j, key, file, id);
    std::array<double, 3> srgb{};
    if (v.is_string()) {
        const std::string s = v.get<std::string>();
        Require(s.size() == 7 && s[0] == '#', Label(file, id, key), "must be #rrggbb or three numbers");
        auto hexDigit = [](char ch) -> int {
            if (ch >= '0' && ch <= '9') {
                return ch - '0';
            }
            if (ch >= 'a' && ch <= 'f') {
                return ch - 'a' + 10;
            }
            if (ch >= 'A' && ch <= 'F') {
                return ch - 'A' + 10;
            }
            return -1;
        };
        for (std::size_t i = 0; i < 3; ++i) {
            const int high = hexDigit(s[1 + i * 2]);
            const int low = hexDigit(s[2 + i * 2]);
            Require(high >= 0 && low >= 0, Label(file, id, key), "must be #rrggbb or three numbers");
            srgb[i] = static_cast<double>(high * 16 + low) / 255.0;
        }
    } else {
        Require(v.is_array() && v.size() == 3, Label(file, id, key), "must be #rrggbb or three numbers");
        for (std::size_t i = 0; i < 3; ++i) {
            Require(v[i].is_number(), Label(file, id, key), "components must be numbers");
            srgb[i] = v[i].get<double>();
            Require(srgb[i] >= 0.0 && srgb[i] <= 1.0, Label(file, id, key), "components must be within 0..1");
        }
    }
    Color3 out{};
    for (std::size_t i = 0; i < 3; ++i)
        out[i] = static_cast<float>(srgb[i] <= 0.04045 ? srgb[i] / 12.92
                                                       : std::pow((srgb[i] + 0.055) / 1.055, 2.4));
    return out;
}
void Keys(const Json& j, std::initializer_list<const char*> allowed, const std::string& file,
          const std::string& id) {
    for (auto it = j.begin(); it != j.end(); ++it) {
        const bool known =
            std::any_of(allowed.begin(), allowed.end(), [&](const char* key) { return it.key() == key; });
        Require(known, Label(file, id, it.key()), "is unknown");
    }
}
template <std::size_t N>
int EnumIndex(const std::string& value, const std::array<std::string_view, N>& names) {
    for (std::size_t i = 0; i < N; ++i)
        if (names[i] == value)
            return static_cast<int>(i);
    return -1;
}
void Unit(float x, const std::string& file, const std::string& id, const char* field) {
    Require(x >= 0.0f && x <= 1.0f, Label(file, id, field), "must be within 0..1");
}
Json ReadFile(const std::string& directory, const char* file, std::vector<std::string>& errors) {
    std::ifstream stream(std::filesystem::path(directory) / file);
    if (!stream) {
        errors.emplace_back(std::string(file) + ": file could not be read");
        return Json();
    }
    try {
        Json j;
        stream >> j;
        return j;
    } catch (const std::exception& e) {
        errors.emplace_back(std::string(file) + ": " + e.what());
        return Json();
    }
}
}  // namespace

std::string_view ToString(FenceStyle v) {
    return kFenceStyles.at(static_cast<std::size_t>(v));
}
std::string_view ToString(BuildingRole v) {
    return kBuildingRoles.at(static_cast<std::size_t>(v));
}
std::string_view ToString(Placement v) {
    return kPlacements.at(static_cast<std::size_t>(v));
}
std::string_view ToString(PropRole v) {
    return kPropRoles.at(static_cast<std::size_t>(v));
}

Catalog Catalog::LoadDirectory(const std::string& directory) {
    std::vector<std::string> fileErrors;
    const Json f = ReadFile(directory, "fences.json", fileErrors);
    const Json p = ReadFile(directory, "paths.json", fileErrors);
    const Json t = ReadFile(directory, "terrain.json", fileErrors);
    const Json fo = ReadFile(directory, "foods.json", fileErrors);
    const Json b = ReadFile(directory, "buildings.json", fileErrors);
    const Json pr = ReadFile(directory, "props.json", fileErrors);
    auto parse = [](const Json& j) { return j.dump(); };
    Catalog c = FromJson(parse(f), parse(p), parse(t), parse(fo), parse(b), parse(pr));
    c.errors.insert(c.errors.begin(), fileErrors.begin(), fileErrors.end());
    return c;
}

Catalog Catalog::FromJson(std::string_view fencesText, std::string_view pathsText,
                          std::string_view terrainsText, std::string_view foodsText,
                          std::string_view buildingsText, std::string_view propsText) {
    Catalog c;
    std::set<std::string> fenceIds, foodIds, buildingIds, propIds;
    std::set<int> kinds;
    auto doc = [&](std::string_view text, const char* file, const char* plural) {
        if (text.empty())
            return Json::object({{plural, Json::array()}});
        try {
            Json j = Json::parse(text);
            Require(j.is_object(), file, "document must be an object");
            Require(j.size() == 1 && j.contains(plural), file,
                    std::string("must contain only '") + plural + "'");
            Require(j[plural].is_array(), file, std::string(plural) + " must be an array");
            return j;
        } catch (const std::exception& e) {
            c.errors.emplace_back(std::string(file) + ": " + e.what());
            return Json::object({{plural, Json::array()}});
        }
    };
    auto addErrors = [&](const Json& j, const char* plural, const char* file, auto convert) {
        for (const Json& item : j[plural]) {
            std::string id;
            try {
                if (item.is_object() && item.contains("id") && item["id"].is_string())
                    id = item["id"].get<std::string>();
                convert(item, id);
            } catch (const std::exception& e) {
                const std::string message = e.what();
                if (message.starts_with(std::string(file) + ":")) {
                    c.errors.push_back(message);
                } else {
                    c.errors.emplace_back(std::string(file) + ": " + (id.empty() ? "<entry>" : id) + ": " +
                                          message);
                }
            }
        }
    };
    const Json fj = doc(fencesText, "fences.json", "fences"), pj = doc(pathsText, "paths.json", "paths"),
               tj = doc(terrainsText, "terrain.json", "terrains"),
               foj = doc(foodsText, "foods.json", "foods"),
               bj = doc(buildingsText, "buildings.json", "buildings"),
               prj = doc(propsText, "props.json", "props");
    addErrors(fj, "fences", "fences.json", [&](const Json& j, const std::string& id) {
        Require(j.is_object(), "entry", "must be an object");
        Keys(j,
             {"id", "name", "kind", "style", "heightM", "strength", "visibility", "cost", "gateCost",
              "upkeepPerDay", "wearPerDay", "color", "postColor"},
             "fences.json", id);
        FenceDef d;
        d.id = String(j, "id", "fences.json", id);
        d.name = String(j, "name", "fences.json", id);
        d.kind = Integer(j, "kind", "fences.json", id);
        const int st = EnumIndex(String(j, "style", "fences.json", id), kFenceStyles);
        Require(st >= 0, Label("fences.json", id, "style"), "is unknown");
        d.style = static_cast<FenceStyle>(st);
        d.heightM = F(j, "heightM", "fences.json", id);
        d.strength = Integer(j, "strength", "fences.json", id);
        d.visibility = F(j, "visibility", "fences.json", id);
        d.cost = WholeCost(j, "cost", "fences.json", id);
        d.gateCost = WholeCost(j, "gateCost", "fences.json", id);
        d.upkeepPerDay = WholeCost(j, "upkeepPerDay", "fences.json", id);
        d.wearPerDay = F(j, "wearPerDay", "fences.json", id);
        d.color = Color(j, "color", "fences.json", id);
        d.postColor = Color(j, "postColor", "fences.json", id);
        Require(d.kind >= 1 && d.kind <= 254, Label("fences.json", id, "kind"), "must be within 1..254");
        Require(kinds.insert(d.kind).second, Label("fences.json", id, "kind"), "is duplicated");
        Require(!d.id.empty() && fenceIds.insert(d.id).second, Label("fences.json", id, "id"),
                "is empty or duplicated");
        Require(d.heightM >= 0.3f && d.heightM <= 8.0f, Label("fences.json", id, "heightM"),
                "must be within 0.3..8");
        Require(d.strength >= 1 && d.strength <= 5, Label("fences.json", id, "strength"),
                "must be within 1..5");
        Unit(d.visibility, "fences.json", id, "visibility");
        Unit(d.wearPerDay, "fences.json", id, "wearPerDay");
        c.fences.push_back(d);
    });
    std::array<bool, kPathKindCount> pathSeen{};
    addErrors(pj, "paths", "paths.json", [&](const Json& j, const std::string&) {
        Require(j.is_object(), "entry", "must be an object");
        Keys(j, {"kind", "name", "cost", "speed", "appeal"}, "paths.json", "");
        const auto kind = PathKindFromString(String(j, "kind", "paths.json", ""));
        Require(kind.has_value() && *kind != PathKind::None, Label("paths.json", "", "kind"),
                "must name a buildable path");
        PathDef d;
        d.kind = *kind;
        d.name = String(j, "name", "paths.json", "");
        d.cost = WholeCost(j, "cost", "paths.json", "");
        d.speed = F(j, "speed", "paths.json", "");
        d.appeal = F(j, "appeal", "paths.json", "");
        Require(d.speed > 0.0f, Label("paths.json", ToString(d.kind), "speed"), "must be positive");
        Require(d.appeal >= -1.0f && d.appeal <= 1.0f, Label("paths.json", ToString(d.kind), "appeal"),
                "must be within -1..1");
        const auto ix = static_cast<std::size_t>(d.kind);
        Require(!pathSeen[ix], Label("paths.json", ToString(d.kind), "kind"), "is duplicated");
        pathSeen[ix] = true;
        c.paths[ix] = d;
    });
    for (int i = 1; i < kPathKindCount; ++i)
        if (!pathSeen[static_cast<std::size_t>(i)])
            c.errors.emplace_back("paths.json: missing path " +
                                  std::string(kPathKindNames[static_cast<std::size_t>(i)]));
    std::array<bool, kTerrainCount> terrainSeen{};
    addErrors(tj, "terrains", "terrain.json", [&](const Json& j, const std::string&) {
        Require(j.is_object(), "entry", "must be an object");
        Keys(j, {"terrain", "name", "cost"}, "terrain.json", "");
        const auto kind = TerrainFromString(String(j, "terrain", "terrain.json", ""));
        Require(kind.has_value(), Label("terrain.json", "", "terrain"), "is unknown");
        TerrainDef d;
        d.terrain = *kind;
        d.name = String(j, "name", "terrain.json", "");
        d.cost = WholeCost(j, "cost", "terrain.json", "");
        const auto ix = static_cast<std::size_t>(d.terrain);
        Require(!terrainSeen[ix], Label("terrain.json", ToString(d.terrain), "terrain"), "is duplicated");
        terrainSeen[ix] = true;
        c.terrains[ix] = d;
    });
    for (int i = 0; i < kTerrainCount; ++i)
        if (!terrainSeen[static_cast<std::size_t>(i)])
            c.errors.emplace_back("terrain.json: missing terrain " +
                                  std::string(kTerrainNames[static_cast<std::size_t>(i)]));
    addErrors(foj, "foods", "foods.json", [&](const Json& j, const std::string& id) {
        Require(j.is_object(), "entry", "must be an object");
        Keys(j, {"id", "name", "costPerKg", "shelfLifeDays", "color"}, "foods.json", id);
        FoodDef d;
        d.id = String(j, "id", "foods.json", id);
        d.name = String(j, "name", "foods.json", id);
        d.costPerKg = UnitCost(j, "costPerKg", "foods.json", id);
        d.shelfLifeDays = F(j, "shelfLifeDays", "foods.json", id);
        d.color = Color(j, "color", "foods.json", id);
        Require(!d.id.empty() && foodIds.insert(d.id).second, Label("foods.json", id, "id"),
                "is empty or duplicated");
        Require(d.shelfLifeDays > 0, Label("foods.json", id, "shelfLifeDays"), "must be positive");
        c.foods.push_back(d);
    });
    addErrors(bj, "buildings", "buildings.json", [&](const Json& j, const std::string& id) {
        Require(j.is_object(), "entry", "must be an object");
        Keys(j, {"id",           "name",      "description",    "role",         "placement",
                 "width",        "depth",     "doorOffset",     "heightM",      "cost",
                 "upkeepPerDay", "capacity",  "heated",         "cooled",       "item",
                 "itemPrice",    "itemCost",  "serviceSeconds", "hungerRelief", "thirstRelief",
                 "fun",          "education", "look",           "wallColor",    "roofColor"},
             "buildings.json", id);
        BuildingDef d;
        d.id = String(j, "id", "buildings.json", id);
        d.name = String(j, "name", "buildings.json", id);
        d.description = String(j, "description", "buildings.json", id);
        const int role = EnumIndex(String(j, "role", "buildings.json", id), kBuildingRoles),
                  place = EnumIndex(String(j, "placement", "buildings.json", id), kPlacements);
        Require(role >= 0, Label("buildings.json", id, "role"), "is unknown");
        Require(place >= 0, Label("buildings.json", id, "placement"), "is unknown");
        d.role = static_cast<BuildingRole>(role);
        d.placement = static_cast<Placement>(place);
        d.width = Integer(j, "width", "buildings.json", id);
        d.depth = Integer(j, "depth", "buildings.json", id);
        d.doorOffset = Integer(j, "doorOffset", "buildings.json", id);
        d.heightM = F(j, "heightM", "buildings.json", id);
        d.cost = WholeCost(j, "cost", "buildings.json", id);
        d.upkeepPerDay = WholeCost(j, "upkeepPerDay", "buildings.json", id);
        d.capacity = Integer(j, "capacity", "buildings.json", id);
        d.heated = Boolean(j, "heated", "buildings.json", id);
        d.cooled = Boolean(j, "cooled", "buildings.json", id);
        d.item = String(j, "item", "buildings.json", id);
        d.itemPrice = WholeCost(j, "itemPrice", "buildings.json", id);
        d.itemCost = WholeCost(j, "itemCost", "buildings.json", id);
        d.serviceSeconds = F(j, "serviceSeconds", "buildings.json", id);
        d.hungerRelief = F(j, "hungerRelief", "buildings.json", id);
        d.thirstRelief = F(j, "thirstRelief", "buildings.json", id);
        d.fun = F(j, "fun", "buildings.json", id);
        d.education = F(j, "education", "buildings.json", id);
        d.look = String(j, "look", "buildings.json", id);
        d.wallColor = Color(j, "wallColor", "buildings.json", id);
        d.roofColor = Color(j, "roofColor", "buildings.json", id);
        Require(!d.id.empty() && buildingIds.insert(d.id).second, Label("buildings.json", id, "id"),
                "is empty or duplicated");
        Require(d.width >= 1 && d.width <= 8 && d.depth >= 1 && d.depth <= 8,
                Label("buildings.json", id, "width/depth"), "must be within 1..8 tiles");
        Require(d.doorOffset >= 0 && d.doorOffset < d.width, Label("buildings.json", id, "doorOffset"),
                "must be within width");
        Require(d.heightM > 0 && d.capacity > 0 && d.serviceSeconds > 0,
                Label("buildings.json", id, "size/capacity"), "must be positive");
        Unit(d.hungerRelief, "buildings.json", id, "hungerRelief");
        Unit(d.thirstRelief, "buildings.json", id, "thirstRelief");
        Unit(d.fun, "buildings.json", id, "fun");
        Unit(d.education, "buildings.json", id, "education");
        if (d.role == BuildingRole::FoodStall || d.role == BuildingRole::DrinkStall ||
            d.role == BuildingRole::GiftShop)
            Require(!d.item.empty() && d.itemPrice > d.itemCost, Label("buildings.json", id, "itemPrice"),
                    "requires an item and itemPrice > itemCost");
        c.buildings.push_back(d);
    });
    addErrors(prj, "props", "props.json", [&](const Json& j, const std::string& id) {
        Require(j.is_object(), "entry", "must be an object");
        Keys(j,
             {"id", "name", "role", "placement", "radiusM", "heightM", "blocksWalking", "cost",
              "vegetationM2", "shadeM2", "biome", "enrichment", "foods", "capacity", "feedHeightM", "beauty",
              "look", "color", "color2"},
             "props.json", id);
        PropDef d;
        d.id = String(j, "id", "props.json", id);
        d.name = String(j, "name", "props.json", id);
        const int role = EnumIndex(String(j, "role", "props.json", id), kPropRoles),
                  place = EnumIndex(String(j, "placement", "props.json", id), kPlacements);
        Require(role >= 0, Label("props.json", id, "role"), "is unknown");
        Require(place >= 0, Label("props.json", id, "placement"), "is unknown");
        d.role = static_cast<PropRole>(role);
        d.placement = static_cast<Placement>(place);
        d.radiusM = F(j, "radiusM", "props.json", id);
        d.heightM = F(j, "heightM", "props.json", id);
        d.blocksWalking = Boolean(j, "blocksWalking", "props.json", id);
        d.cost = WholeCost(j, "cost", "props.json", id);
        d.vegetationM2 = F(j, "vegetationM2", "props.json", id);
        d.shadeM2 = F(j, "shadeM2", "props.json", id);
        d.biome = String(j, "biome", "props.json", id);
        d.enrichment = F(j, "enrichment", "props.json", id);
        (void)Need(j, "foods", "props.json", id);
        const Json& foods = j.at("foods");
        Require(foods.is_array(), Label("props.json", id, "foods"), "must be an array");
        for (const auto& food : foods) {
            Require(food.is_string(), Label("props.json", id, "foods"), "entries must be strings");
            d.foods.push_back(food.get<std::string>());
        }
        d.capacity = F(j, "capacity", "props.json", id);
        d.feedHeightM = F(j, "feedHeightM", "props.json", id);
        d.beauty = F(j, "beauty", "props.json", id);
        d.look = String(j, "look", "props.json", id);
        d.color = Color(j, "color", "props.json", id);
        d.color2 = Color(j, "color2", "props.json", id);
        Require(!d.id.empty() && propIds.insert(d.id).second, Label("props.json", id, "id"),
                "is empty or duplicated");
        Require(d.radiusM > 0 && d.heightM > 0, Label("props.json", id, "size"), "must be positive");
        Require(d.vegetationM2 >= 0 && d.shadeM2 >= 0, Label("props.json", id, "vegetationM2/shadeM2"),
                "must be non-negative");
        Unit(d.enrichment, "props.json", id, "enrichment");
        Unit(d.beauty, "props.json", id, "beauty");
        Require(d.capacity >= 0 && d.feedHeightM >= 0, Label("props.json", id, "capacity/feedHeightM"),
                "must be non-negative");
        if (d.role == PropRole::Feeder) {
            Require(!d.foods.empty(), Label("props.json", id, "foods"), "feeder must list foods");
            Require(d.capacity > 0, Label("props.json", id, "capacity"), "feeder capacity must be positive");
            for (const auto& food : d.foods)
                Require(foodIds.contains(food), Label("props.json", id, "foods"),
                        "names unknown food '" + food + "'");
        } else if (d.role == PropRole::WaterTrough || d.role == PropRole::Bin || d.role == PropRole::Bench)
            Require(d.capacity > 0, Label("props.json", id, "capacity"), "must be positive");
        c.props.push_back(d);
    });
    std::sort(c.fences.begin(), c.fences.end(),
              [](const FenceDef& a, const FenceDef& b) { return a.kind < b.kind; });
    return c;
}

const FenceDef* Catalog::FenceByKind(int kind) const {
    for (const auto& d : fences)
        if (d.kind == kind)
            return &d;
    return nullptr;
}
const FenceDef* Catalog::FindFence(std::string_view id) const {
    for (const auto& d : fences)
        if (d.id == id)
            return &d;
    return nullptr;
}
int Catalog::FoodIndex(std::string_view id) const {
    for (std::size_t i = 0; i < foods.size(); ++i)
        if (foods[i].id == id)
            return static_cast<int>(i);
    return -1;
}
int Catalog::BuildingIndex(std::string_view id) const {
    for (std::size_t i = 0; i < buildings.size(); ++i)
        if (buildings[i].id == id)
            return static_cast<int>(i);
    return -1;
}
int Catalog::PropIndex(std::string_view id) const {
    for (std::size_t i = 0; i < props.size(); ++i)
        if (props[i].id == id)
            return static_cast<int>(i);
    return -1;
}
}  // namespace zoo
