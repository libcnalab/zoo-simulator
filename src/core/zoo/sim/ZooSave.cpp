// SPDX-License-Identifier: MIT
#include "zoo/sim/ZooSaveJson.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <set>
#include <stdexcept>
#include <utility>

#include "zoo/animals/AnimalsJson.hpp"
#include "zoo/base/Version.hpp"
#include "zoo/build/EnclosuresJson.hpp"
#include "zoo/build/ObjectsJson.hpp"
#include "zoo/economy/FinanceJson.hpp"
#include "zoo/world/WorldJson.hpp"

namespace zoo {

using Json = nlohmann::json;

struct ZooAccess {
    static const ZooConfig& Config(const Zoo& zoo) { return zoo.config_; }
    static const std::deque<Notification>& Notifications(const Zoo& zoo) { return zoo.notifications_; }
    static std::uint32_t NextNotification(const Zoo& zoo) { return zoo.nextNotification_; }

    static std::unique_ptr<Zoo> Assemble(const Catalog& catalog, const SpeciesCatalog& species,
                                         ZooConfig config, World world, ZooObjects objects, Finance finance,
                                         Animals animals, const WeatherState& weather, GameTime time,
                                         const Json& enclosures, std::deque<Notification> notifications,
                                         std::uint32_t nextNotification, std::string* error) {
        WeatherSystem sky(config.seed, config.climate);
        sky.Restore(weather, GameTime{time.tick > 0 ? time.tick - 1 : 0});
        Zoo::Parts parts{std::move(config),
                         std::move(world),
                         std::move(objects),
                         std::move(finance),
                         std::move(animals),
                         std::move(sky),
                         time};
        std::unique_ptr<Zoo> zoo(new Zoo(catalog, species, std::move(parts)));
        // Ids and names of the enclosures are state; their extent follows from the map.
        if (!EnclosuresFromJson(enclosures, zoo->enclosures_, error)) {
            return nullptr;
        }
        zoo->enclosures_.Rebuild(zoo->world_, zoo->objects_, catalog);
        zoo->notifications_ = std::move(notifications);
        zoo->nextNotification_ = nextNotification;
        return zoo;
    }
};

namespace {

Json ClimateToJson(const Climate& c) {
    return Json{{"mean_c", c.meanC},
                {"seasonal_swing_c", c.seasonalSwingC},
                {"diurnal_swing_c", c.diurnalSwingC},
                {"day_length_swing_h", c.dayLengthSwingH},
                {"wetness", c.wetness}};
}

Climate ClimateFromJson(const Json& j) {
    Climate c;
    c.meanC = j.at("mean_c").get<float>();
    c.seasonalSwingC = j.at("seasonal_swing_c").get<float>();
    c.diurnalSwingC = j.at("diurnal_swing_c").get<float>();
    c.dayLengthSwingH = j.at("day_length_swing_h").get<float>();
    c.wetness = j.at("wetness").get<float>();
    return c;
}

Json ConfigToJson(const ZooConfig& c) {
    return Json{{"name", c.name},
                {"seed", c.seed},
                {"starting_money", c.startingMoney},
                {"sandbox", c.sandbox},
                {"auto_care", c.autoCare},
                {"climate", ClimateToJson(c.climate)},
                {"map", Json{{"width", c.world.width},
                             {"depth", c.world.depth},
                             {"seed", c.world.seed},
                             {"relief", c.world.relief},
                             {"margin", c.world.margin},
                             {"entrance_width", c.world.entranceWidth}}}};
}

ZooConfig ConfigFromJson(const Json& j) {
    ZooConfig c;
    c.name = j.at("name").get<std::string>();
    if (c.name.empty() || c.name.size() > 40) {
        throw std::runtime_error("the zoo's name must have 1 to 40 characters");
    }
    c.seed = j.at("seed").get<std::uint64_t>();
    c.startingMoney = j.at("starting_money").get<Money>();
    c.sandbox = j.at("sandbox").get<bool>();
    c.autoCare = j.at("auto_care").get<bool>();
    c.climate = ClimateFromJson(j.at("climate"));
    const Json& map = j.at("map");
    c.world.width = map.at("width").get<int>();
    c.world.depth = map.at("depth").get<int>();
    c.world.seed = map.at("seed").get<std::uint64_t>();
    c.world.relief = map.at("relief").get<float>();
    c.world.margin = map.at("margin").get<int>();
    c.world.entranceWidth = map.at("entrance_width").get<int>();
    return c;
}

constexpr const char* kSkyNames[] = {"clear", "cloudy", "overcast", "rain", "storm"};

Json WeatherToJson(const WeatherState& w) {
    return Json{{"sky", kSkyNames[static_cast<int>(w.sky)]},
                {"ticks_left", w.ticksLeft},
                {"cloud", w.cloud},
                {"rain", w.rain},
                {"wind", w.wind},
                {"rain_target", w.rainTarget},
                {"wind_target", w.windTarget},
                {"offset_c", w.offsetC},
                {"offset_target_c", w.offsetTargetC},
                {"snowing", w.snowing},
                {"rng_state", w.rngState},
                {"rng_stream", w.rngStream}};
}

WeatherState WeatherFromJson(const Json& j) {
    WeatherState w;
    const std::string sky = j.at("sky").get<std::string>();
    int found = -1;
    for (int i = 0; i < 5; ++i) {
        if (sky == kSkyNames[i]) {
            found = i;
        }
    }
    if (found < 0) {
        throw std::runtime_error("unknown sky: " + sky);
    }
    w.sky = static_cast<WeatherKind>(found);
    w.ticksLeft = j.at("ticks_left").get<std::int32_t>();
    w.cloud = j.at("cloud").get<float>();
    w.rain = j.at("rain").get<float>();
    w.wind = j.at("wind").get<float>();
    w.rainTarget = j.at("rain_target").get<float>();
    w.windTarget = j.at("wind_target").get<float>();
    w.offsetC = j.at("offset_c").get<float>();
    w.offsetTargetC = j.at("offset_target_c").get<float>();
    w.snowing = j.at("snowing").get<bool>();
    w.rngState = j.at("rng_state").get<std::uint64_t>();
    w.rngStream = j.at("rng_stream").get<std::uint64_t>();
    return w;
}

constexpr const char* kSeverityNames[] = {"info", "good", "warning", "alarm"};

Json NotificationToJson(const Notification& n) {
    Json j{{"id", n.id},
           {"tick", n.tick},
           {"severity", kSeverityNames[static_cast<int>(n.severity)]},
           {"text", n.text}};
    if (n.located) {
        j["at"] = Json::array({n.position.x, n.position.z});
    }
    if (n.animal) {
        j["animal"] = n.animal.value;
    }
    if (n.enclosure) {
        j["enclosure"] = n.enclosure.value;
    }
    return j;
}

Notification NotificationFromJson(const Json& j) {
    Notification n;
    n.id = j.at("id").get<std::uint32_t>();
    n.tick = j.at("tick").get<std::int64_t>();
    const std::string severity = j.at("severity").get<std::string>();
    int found = -1;
    for (int i = 0; i < 4; ++i) {
        if (severity == kSeverityNames[i]) {
            found = i;
        }
    }
    if (found < 0) {
        throw std::runtime_error("unknown severity: " + severity);
    }
    n.severity = static_cast<Severity>(found);
    n.text = j.at("text").get<std::string>();
    if (j.contains("at")) {
        n.position = {j.at("at").at(0).get<float>(), j.at("at").at(1).get<float>()};
        n.located = true;
    }
    if (j.contains("animal")) {
        n.animal = AnimalId{j.at("animal").get<std::uint32_t>()};
    }
    if (j.contains("enclosure")) {
        n.enclosure = EnclosureId{j.at("enclosure").get<std::uint32_t>()};
    }
    return n;
}

void SetError(std::string* error, std::string message) {
    if (error != nullptr) {
        *error = std::move(message);
    }
}

/// Checks the envelope and returns the version, or 0 with an error.
int CheckEnvelope(const Json& json, std::string* error) {
    if (!json.is_object() || !json.contains("format") || !json["format"].is_string() ||
        json["format"].get<std::string>() != kSaveFormat) {
        SetError(error, "This is not a Zoo Simulator save.");
        return 0;
    }
    if (!json.contains("version") || !json["version"].is_number_integer()) {
        SetError(error, "The save has no version.");
        return 0;
    }
    const int version = json["version"].get<int>();
    if (version < 1) {
        SetError(error, "The save has an invalid version.");
        return 0;
    }
    if (version > kSaveVersion) {
        SetError(error, "The save was written by a newer version of Zoo Simulator (format " +
                            std::to_string(version) + ", this program reads up to " +
                            std::to_string(kSaveVersion) + ").");
        return 0;
    }
    return version;
}

}  // namespace

Json ZooToJson(const Zoo& zoo) {
    std::set<int> species;
    for (const Animal& animal : zoo.Fauna().All()) {
        species.insert(animal.species);
    }
    Json notifications = Json::array();
    for (const Notification& n : ZooAccess::Notifications(zoo)) {
        notifications.push_back(NotificationToJson(n));
    }
    Json body{{"config", ConfigToJson(ZooAccess::Config(zoo))},
              {"tick", zoo.Time().tick},
              {"weather", WeatherToJson(zoo.Sky().State())},
              {"world", ToJson(zoo.Map())},
              {"objects", ToJson(zoo.Objects(), zoo.Definitions())},
              {"enclosures", ToJson(zoo.Habitats())},
              {"finance", ToJson(zoo.Accounts())},
              {"animals", ToJson(zoo.Fauna(), zoo.SpeciesList())},
              {"notifications", std::move(notifications)},
              {"next_notification", ZooAccess::NextNotification(zoo)}};
    return Json{{"format", kSaveFormat},
                {"version", kSaveVersion},
                {"summary", Json{{"name", zoo.Name()},
                                 {"day", zoo.Time().Day()},
                                 {"year", zoo.Time().Year()},
                                 {"balance", zoo.Accounts().Balance()},
                                 {"animals", zoo.Fauna().All().size()},
                                 {"species", species.size()},
                                 {"game_version", VersionString()}}},
                {"zoo", std::move(body)}};
}

bool SummaryFromJson(const Json& json, SaveSummary& out, std::string* error) {
    const int version = CheckEnvelope(json, error);
    if (version == 0) {
        return false;
    }
    try {
        const Json& s = json.at("summary");
        out.version = version;
        out.name = s.at("name").get<std::string>();
        out.day = s.at("day").get<std::int64_t>();
        out.year = s.at("year").get<int>();
        out.balance = s.at("balance").get<Money>();
        out.animals = s.at("animals").get<int>();
        out.species = s.at("species").get<int>();
        out.gameVersion = s.value("game_version", std::string());
    } catch (const std::exception& e) {
        SetError(error, std::string("The save's summary is damaged: ") + e.what());
        return false;
    }
    return true;
}

std::unique_ptr<Zoo> ZooFromJson(const Json& json, const Catalog& catalog, const SpeciesCatalog& species,
                                 std::string* error) {
    const int version = CheckEnvelope(json, error);
    if (version == 0) {
        return nullptr;
    }
    // Migrations of older formats go here, each turning version N into N + 1 in a
    // copy of the document. Version 1 is the first.
    try {
        const Json& body = json.at("zoo");
        ZooConfig config = ConfigFromJson(body.at("config"));
        GameTime time;
        time.tick = body.at("tick").get<std::int64_t>();
        if (time.tick < 0) {
            throw std::runtime_error("the clock is before the start of the game");
        }
        const WeatherState weather = WeatherFromJson(body.at("weather"));

        std::string part;
        std::optional<World> world = WorldFromJson(body.at("world"), &part);
        if (!world) {
            throw std::runtime_error("map: " + part);
        }
        std::optional<ZooObjects> objects = ObjectsFromJson(body.at("objects"), catalog, &part);
        if (!objects) {
            throw std::runtime_error("objects: " + part);
        }
        std::optional<Finance> finance = FinanceFromJson(body.at("finance"), &part);
        if (!finance) {
            throw std::runtime_error("finance: " + part);
        }
        std::optional<Animals> animals = AnimalsFromJson(body.at("animals"), species, &part);
        if (!animals) {
            throw std::runtime_error("animals: " + part);
        }
        std::deque<Notification> notifications;
        for (const Json& n : body.at("notifications")) {
            notifications.push_back(NotificationFromJson(n));
        }
        const std::uint32_t nextNotification = body.at("next_notification").get<std::uint32_t>();

        std::unique_ptr<Zoo> zoo =
            ZooAccess::Assemble(catalog, species, std::move(config), std::move(*world), std::move(*objects),
                                std::move(*finance), std::move(*animals), weather, time,
                                body.at("enclosures"), std::move(notifications), nextNotification, &part);
        if (!zoo) {
            throw std::runtime_error("enclosures: " + part);
        }
        return zoo;
    } catch (const std::exception& e) {
        SetError(error, std::string("The save is damaged (") + e.what() + ").");
        return nullptr;
    }
}

bool SaveZoo(const Zoo& zoo, const std::string& path, std::string* error) {
    const std::string temporary = path + ".tmp";
    try {
        const std::filesystem::path target(path);
        if (target.has_parent_path()) {
            std::filesystem::create_directories(target.parent_path());
        }
        {
            std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
            if (!out) {
                SetError(error, "Cannot write " + temporary + ".");
                return false;
            }
            out << ZooToJson(zoo).dump();
            out.flush();
            if (!out) {
                SetError(error, "Writing " + temporary + " failed (disk full?).");
                return false;
            }
        }
        std::filesystem::rename(temporary, target);
    } catch (const std::exception& e) {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        SetError(error, std::string("Saving failed: ") + e.what());
        return false;
    }
    return true;
}

namespace {

bool ReadJsonFile(const std::string& path, Json& out, std::string* error) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        SetError(error, "Cannot open " + path + ".");
        return false;
    }
    out = Json::parse(in, nullptr, false);
    if (out.is_discarded()) {
        SetError(error, path + " is not a valid save (it is not JSON).");
        return false;
    }
    return true;
}

}  // namespace

std::unique_ptr<Zoo> LoadZoo(const std::string& path, const Catalog& catalog, const SpeciesCatalog& species,
                             std::string* error) {
    Json json;
    if (!ReadJsonFile(path, json, error)) {
        return nullptr;
    }
    return ZooFromJson(json, catalog, species, error);
}

bool ReadSaveSummary(const std::string& path, SaveSummary& out, std::string* error) {
    Json json;
    if (!ReadJsonFile(path, json, error)) {
        return false;
    }
    return SummaryFromJson(json, out, error);
}

}  // namespace zoo
