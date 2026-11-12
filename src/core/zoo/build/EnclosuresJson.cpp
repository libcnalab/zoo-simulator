// SPDX-License-Identifier: MIT
#include "zoo/build/EnclosuresJson.hpp"

#include <limits>
#include <utility>
#include <vector>

namespace zoo {
namespace {

bool Fail(std::string* error, const char* message) {
    if (error != nullptr) {
        *error = message;
    }
    return false;
}

bool ReadUnsigned(const nlohmann::json& value, std::uint32_t& out) {
    if (value.is_number_unsigned()) {
        const std::uint64_t unsignedValue = value.get<std::uint64_t>();
        if (unsignedValue > static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max())) {
            return false;
        }
        out = static_cast<std::uint32_t>(unsignedValue);
        return true;
    }
    if (!value.is_number_integer()) {
        return false;
    }
    const std::int64_t signedValue = value.get<std::int64_t>();
    if (signedValue < 0 || static_cast<std::uint64_t>(signedValue) >
                               static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max())) {
        return false;
    }
    out = static_cast<std::uint32_t>(signedValue);
    return true;
}

bool ReadCoordinate(const nlohmann::json& value, int& out) {
    if (!value.is_number_integer() && !value.is_number_unsigned()) {
        return false;
    }
    if (value.is_number_unsigned()) {
        const std::uint64_t coordinate = value.get<std::uint64_t>();
        if (coordinate > static_cast<std::uint64_t>(std::numeric_limits<int>::max())) {
            return false;
        }
        out = static_cast<int>(coordinate);
        return true;
    }
    const std::int64_t coordinate = value.get<std::int64_t>();
    if (coordinate < std::numeric_limits<int>::min() || coordinate > std::numeric_limits<int>::max()) {
        return false;
    }
    out = static_cast<int>(coordinate);
    return true;
}

}  // namespace

nlohmann::json ToJson(const Enclosures& enclosures) {
    nlohmann::json result;
    result["nextId"] = EnclosuresAccess::NextId(enclosures);
    result["enclosures"] = nlohmann::json::array();
    for (const Enclosure& enclosure : EnclosuresAccess::Values(enclosures)) {
        result["enclosures"].push_back({{"id", enclosure.id.value},
                                        {"name", enclosure.name},
                                        {"anchor", {enclosure.anchor.x, enclosure.anchor.z}}});
    }
    return result;
}

bool EnclosuresFromJson(const nlohmann::json& json, Enclosures& out, std::string* error) {
    if (!json.is_object() || !json.contains("nextId") || !json.contains("enclosures")) {
        return Fail(error, "Expected an object with nextId and enclosures.");
    }
    std::uint32_t nextId = 0;
    if (!ReadUnsigned(json["nextId"], nextId) || nextId == 0) {
        return Fail(error, "nextId must be a positive 32-bit integer.");
    }
    if (!json["enclosures"].is_array()) {
        return Fail(error, "enclosures must be an array.");
    }
    std::vector<Enclosure> values;
    values.reserve(json["enclosures"].size());
    std::uint32_t previousId = 0;
    for (const nlohmann::json& item : json["enclosures"]) {
        if (!item.is_object() || !item.contains("id") || !item.contains("name") || !item.contains("anchor")) {
            return Fail(error, "Each enclosure needs id, name and anchor.");
        }
        std::uint32_t id = 0;
        if (!ReadUnsigned(item["id"], id) || id == 0 || id <= previousId || id >= nextId) {
            return Fail(error, "Enclosure ids must be ascending, unique and less than nextId.");
        }
        if (!item["name"].is_string() || item["name"].get_ref<const std::string&>().empty()) {
            return Fail(error, "Enclosure names must be non-empty strings.");
        }
        const nlohmann::json& anchor = item["anchor"];
        if (!anchor.is_array() || anchor.size() != 2) {
            return Fail(error, "An anchor must contain x and z coordinates.");
        }
        Enclosure enclosure;
        enclosure.id = EnclosureId{id};
        enclosure.name = item["name"].get<std::string>();
        if (!ReadCoordinate(anchor[0], enclosure.anchor.x) ||
            !ReadCoordinate(anchor[1], enclosure.anchor.z)) {
            return Fail(error, "Anchor coordinates must be 32-bit integers.");
        }
        previousId = id;
        values.push_back(std::move(enclosure));
    }
    EnclosuresAccess::Restore(nextId, std::move(values), out);
    if (error != nullptr) {
        error->clear();
    }
    return true;
}

}  // namespace zoo
