// SPDX-License-Identifier: MIT
#include "zoo/animals/Species.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <set>
#include <stdexcept>
#include <utility>

#include <nlohmann/json.hpp>

namespace zoo {
namespace {
using Json = nlohmann::json;

template <typename E, std::size_t N>
std::optional<E> ParseEnum(std::string_view value, const std::array<std::string_view, N>& names) {
    for (std::size_t i = 0; i < N; ++i) {
        if (names[i] == value)
            return static_cast<E>(i);
    }
    return std::nullopt;
}

constexpr std::array<std::string_view, 5> kDietNames = {"carnivore", "herbivore", "omnivore", "piscivore",
                                                        "insectivore"};
constexpr std::array<std::string_view, 4> kActivityNames = {"diurnal", "nocturnal", "crepuscular",
                                                            "cathemeral"};
constexpr std::array<std::string_view, 7> kStatusNames = {"LC", "NT", "VU", "EN", "CR", "EW", "NE"};
constexpr std::array<std::string_view, 7> kStatusDisplay = {
    "Least Concern",         "Near Threatened",     "Vulnerable",   "Endangered",
    "Critically Endangered", "Extinct in the Wild", "Not Evaluated"};
const std::set<std::string> kKeys = {"id",
                                     "commonName",
                                     "scientificName",
                                     "family",
                                     "model",
                                     "group",
                                     "adultMassKg",
                                     "shoulderHeightM",
                                     "bodyLengthM",
                                     "bodyRadiusM",
                                     "walkSpeed",
                                     "runSpeed",
                                     "canSwim",
                                     "canClimb",
                                     "jumpHeightM",
                                     "lifespanYears",
                                     "maturityYears",
                                     "gestationDays",
                                     "litterMin",
                                     "litterMax",
                                     "birthIntervalYears",
                                     "diet",
                                     "foods",
                                     "foodPerDayKg",
                                     "waterPerDayL",
                                     "activity",
                                     "sleepHoursPerDay",
                                     "groupMin",
                                     "groupMax",
                                     "maxAdultMales",
                                     "sociability",
                                     "playfulness",
                                     "shyness",
                                     "aggression",
                                     "dangerous",
                                     "minEnclosureM2",
                                     "spacePerExtraAnimalM2",
                                     "terrainSuitability",
                                     "waterAreaFraction",
                                     "needsDeepWater",
                                     "vegetationDensity",
                                     "needsShelter",
                                     "tempMinC",
                                     "tempMaxC",
                                     "fenceHeightM",
                                     "fenceStrength",
                                     "price",
                                     "appeal",
                                     "status",
                                     "nativeRange",
                                     "biome",
                                     "facts",
                                     "sounds",
                                     "sources"};

void Need(const Json& j, const char* key) {
    if (!j.contains(key))
        throw std::runtime_error(std::string("missing key '") + key + "'");
}

template <typename T>
T Value(const Json& j, const char* key) {
    Need(j, key);
    if (!j.at(key).is_number())
        throw std::runtime_error(std::string("key '") + key + "' has wrong type");
    try {
        return j.at(key).get<T>();
    } catch (...) {
        throw std::runtime_error(std::string("key '") + key + "' has wrong type or range");
    }
}

template <typename T>
T Read(const Json& j, const char* key) {
    Need(j, key);
    try {
        return j.at(key).get<T>();
    } catch (...) {
        throw std::runtime_error(std::string("key '") + key + "' has wrong type");
    }
}

void ReadStrings(const Json& j, const char* key, std::vector<std::string>& out) {
    out = Read<std::vector<std::string>>(j, key);
}

void ReadString(const Json& j, const char* key, std::string& out) {
    out = Read<std::string>(j, key);
}

}  // namespace

std::string_view ToString(Diet v) {
    return kDietNames.at(static_cast<std::size_t>(v));
}
std::string_view ToString(ActivityPattern v) {
    return kActivityNames.at(static_cast<std::size_t>(v));
}
std::string_view ToString(ConservationStatus v) {
    return kStatusNames.at(static_cast<std::size_t>(v));
}
std::string_view DisplayName(ConservationStatus v) {
    return kStatusDisplay.at(static_cast<std::size_t>(v));
}

std::vector<std::string> SpeciesCatalog::Validate(const SpeciesDef& d) {
    std::vector<std::string> e;
    auto req = [&e](bool bad, const char* name) {
        if (bad)
            e.emplace_back(std::string(name) + " is invalid");
    };
    const std::array<float, 31> numbers = {d.adultMassKg,
                                           d.shoulderHeightM,
                                           d.bodyLengthM,
                                           d.bodyRadiusM,
                                           d.walkSpeed,
                                           d.runSpeed,
                                           d.jumpHeightM,
                                           d.lifespanYears,
                                           d.maturityYears,
                                           d.gestationDays,
                                           d.birthIntervalYears,
                                           d.foodPerDayKg,
                                           d.waterPerDayL,
                                           d.sleepHoursPerDay,
                                           d.sociability,
                                           d.playfulness,
                                           d.shyness,
                                           d.aggression,
                                           d.minEnclosureM2,
                                           d.spacePerExtraAnimalM2,
                                           d.waterAreaFraction,
                                           d.vegetationDensity,
                                           d.tempMinC,
                                           d.tempMaxC,
                                           d.fenceHeightM,
                                           d.terrainSuitability[0],
                                           d.terrainSuitability[1],
                                           d.terrainSuitability[2],
                                           d.terrainSuitability[3],
                                           d.terrainSuitability[4],
                                           d.terrainSuitability[5]};
    for (const float value : numbers)
        req(!std::isfinite(value), "numeric value");
    req(!std::isfinite(d.terrainSuitability[6]) || !std::isfinite(d.terrainSuitability[7]),
        "terrainSuitability");
    req(d.id.empty(), "id");
    req(d.commonName.empty(), "commonName");
    req(d.scientificName.empty(), "scientificName");
    req(d.model.empty(), "model");
    req(d.group.empty(), "group");
    req(!d.id.empty() && !(d.id[0] >= 'a' && d.id[0] <= 'z'), "id");
    req(d.id.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_") != std::string::npos, "id");
    req(d.adultMassKg <= 0, "adultMassKg");
    req(d.shoulderHeightM <= 0, "shoulderHeightM");
    req(d.bodyLengthM <= 0, "bodyLengthM");
    req(d.bodyRadiusM <= 0, "bodyRadiusM");
    req(d.walkSpeed <= 0, "walkSpeed");
    req(d.runSpeed <= 0, "runSpeed");
    req(d.runSpeed < d.walkSpeed, "runSpeed");
    req(d.jumpHeightM < 0, "jumpHeightM");
    req(d.lifespanYears <= 0, "lifespanYears");
    req(d.maturityYears <= 0 || d.maturityYears >= d.lifespanYears, "maturityYears");
    req(d.gestationDays <= 0, "gestationDays");
    req(d.birthIntervalYears <= 0, "birthIntervalYears");
    req(d.litterMin < 1 || d.litterMin > d.litterMax, "litterMin/litterMax");
    req(d.foodPerDayKg <= 0, "foodPerDayKg");
    req(d.waterPerDayL < 0, "waterPerDayL");
    req(d.sleepHoursPerDay <= 0 || d.sleepHoursPerDay > 24, "sleepHoursPerDay");
    req(d.groupMin < 1 || d.groupMin > d.groupMax, "groupMin/groupMax");
    req(d.maxAdultMales < 0, "maxAdultMales");
    auto unit = [&req](float v, const char* n) { req(v < 0 || v > 1, n); };
    unit(d.sociability, "sociability");
    unit(d.playfulness, "playfulness");
    unit(d.shyness, "shyness");
    unit(d.aggression, "aggression");
    req(d.minEnclosureM2 <= 0, "minEnclosureM2");
    req(d.spacePerExtraAnimalM2 < 0, "spacePerExtraAnimalM2");
    bool suitable = false;
    for (int i = 0; i < kTerrainCount; ++i) {
        const float v = d.terrainSuitability[static_cast<std::size_t>(i)];
        unit(v, "terrainSuitability");
        suitable = suitable || v >= 0.5f;
    }
    req(!suitable, "terrainSuitability");
    unit(d.waterAreaFraction, "waterAreaFraction");
    req(d.needsDeepWater && d.waterAreaFraction == 0, "needsDeepWater");
    unit(d.vegetationDensity, "vegetationDensity");
    req(d.tempMinC >= d.tempMaxC, "tempMinC/tempMaxC");
    req(d.fenceHeightM <= 0, "fenceHeightM");
    req(d.fenceStrength < 1 || d.fenceStrength > 5, "fenceStrength");
    req(d.price < 0, "price");
    req(d.appeal < 0 || d.appeal > 100, "appeal");
    req(d.foods.empty(), "foods");
    req(d.facts.size() < 3, "facts");
    constexpr std::array<std::string_view, 8> foodIds = {"meat",          "fish",      "hay",     "browse",
                                                         "grass_pellets", "fruit_veg", "insects", "bamboo"};
    for (const auto& food : d.foods)
        req(std::find(foodIds.begin(), foodIds.end(), food) == foodIds.end(), "foods");
    return e;
}

bool SpeciesCatalog::Add(SpeciesDef def, std::string_view origin) {
    auto problems = Validate(def);
    if (Find(def.id) != nullptr)
        problems.emplace_back("duplicate id '" + def.id + "'");
    if (!problems.empty()) {
        for (const auto& p : problems)
            errors_.push_back(std::string(origin) + ": " + p);
        return false;
    }
    species_.push_back(std::move(def));
    return true;
}

bool SpeciesCatalog::AddFromJson(std::string_view jsonText, std::string_view origin) {
    try {
        const Json j = Json::parse(jsonText);
        if (!j.is_object())
            throw std::runtime_error("root must be an object");
        for (auto it = j.begin(); it != j.end(); ++it)
            if (!kKeys.contains(it.key()))
                throw std::runtime_error("unknown key '" + it.key() + "'");
        SpeciesDef d;
#define STR(field) ReadString(j, #field, d.field)
#define NUM(field) d.field = Value<decltype(d.field)>(j, #field)
#define BOOL(field) d.field = Read<bool>(j, #field)
        STR(id);
        STR(commonName);
        STR(scientificName);
        STR(family);
        STR(model);
        STR(group);
        NUM(adultMassKg);
        NUM(shoulderHeightM);
        NUM(bodyLengthM);
        NUM(bodyRadiusM);
        NUM(walkSpeed);
        NUM(runSpeed);
        BOOL(canSwim);
        BOOL(canClimb);
        NUM(jumpHeightM);
        NUM(lifespanYears);
        NUM(maturityYears);
        NUM(gestationDays);
        NUM(litterMin);
        NUM(litterMax);
        NUM(birthIntervalYears);
        {
            auto v = ParseEnum<Diet>(Read<std::string>(j, "diet"), kDietNames);
            if (!v)
                throw std::runtime_error("key 'diet' has unknown enum value");
            d.diet = *v;
        }
        ReadStrings(j, "foods", d.foods);
        NUM(foodPerDayKg);
        NUM(waterPerDayL);
        {
            auto v = ParseEnum<ActivityPattern>(Read<std::string>(j, "activity"), kActivityNames);
            if (!v)
                throw std::runtime_error("key 'activity' has unknown enum value");
            d.activity = *v;
        }
        NUM(sleepHoursPerDay);
        NUM(groupMin);
        NUM(groupMax);
        NUM(maxAdultMales);
        NUM(sociability);
        NUM(playfulness);
        NUM(shyness);
        NUM(aggression);
        BOOL(dangerous);
        NUM(minEnclosureM2);
        NUM(spacePerExtraAnimalM2);
        {
            Need(j, "terrainSuitability");
            const Json& t = j.at("terrainSuitability");
            if (!t.is_object())
                throw std::runtime_error("key 'terrainSuitability' has wrong type");
            for (auto it = t.begin(); it != t.end(); ++it) {
                auto terrain = TerrainFromString(it.key());
                if (!terrain)
                    throw std::runtime_error("key 'terrainSuitability' has unknown terrain '" + it.key() +
                                             "'");
                if (!it.value().is_number())
                    throw std::runtime_error("key 'terrainSuitability' has wrong value type");
                d.terrainSuitability[static_cast<std::size_t>(*terrain)] = it.value().get<float>();
            }
        }
        NUM(waterAreaFraction);
        BOOL(needsDeepWater);
        NUM(vegetationDensity);
        BOOL(needsShelter);
        NUM(tempMinC);
        NUM(tempMaxC);
        NUM(fenceHeightM);
        NUM(fenceStrength);
        NUM(price);
        NUM(appeal);
        {
            auto v = ParseEnum<ConservationStatus>(Read<std::string>(j, "status"), kStatusNames);
            if (!v)
                throw std::runtime_error("key 'status' has unknown enum value");
            d.status = *v;
        }
        STR(nativeRange);
        STR(biome);
        ReadStrings(j, "facts", d.facts);
        if (j.contains("sounds")) {
            ReadStrings(j, "sounds", d.sounds);
        }
        ReadStrings(j, "sources", d.sources);
#undef STR
#undef NUM
#undef BOOL
        const std::filesystem::path p{std::string(origin)};
        if (p.has_filename() && p.extension() == ".json" && p.filename().string() != d.id + ".json")
            throw std::runtime_error("file name must equal id + '.json'");
        return Add(std::move(d), origin);
    } catch (const std::exception& ex) {
        errors_.push_back(std::string(origin) + ": " + ex.what());
        return false;
    } catch (...) {
        errors_.push_back(std::string(origin) + ": invalid input");
        return false;
    }
}

SpeciesCatalog SpeciesCatalog::LoadDirectory(const std::string& directory) {
    SpeciesCatalog c;
    try {
        std::vector<std::filesystem::path> files;
        for (const auto& entry : std::filesystem::directory_iterator(directory))
            if (entry.is_regular_file() && entry.path().extension() == ".json")
                files.push_back(entry.path());
        std::sort(files.begin(), files.end());
        for (const auto& p : files) {
            std::ifstream in(p);
            if (!in) {
                c.errors_.push_back(p.string() + ": cannot open file");
                continue;
            }
            const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            c.AddFromJson(text, p.string());
        }
    } catch (const std::exception& ex) {
        c.errors_.push_back(directory + ": " + ex.what());
    }
    return c;
}

const SpeciesDef* SpeciesCatalog::Find(std::string_view id) const {
    for (const auto& d : species_)
        if (d.id == id)
            return &d;
    return nullptr;
}
int SpeciesCatalog::IndexOf(std::string_view id) const {
    for (std::size_t i = 0; i < species_.size(); ++i)
        if (species_[i].id == id)
            return static_cast<int>(i);
    return -1;
}
}  // namespace zoo
