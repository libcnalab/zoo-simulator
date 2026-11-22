// SPDX-License-Identifier: MIT
#include "zoo/world/WorldJson.hpp"

#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace zoo {
namespace {

using Json = nlohmann::json;

Json CoordJson(TileCoord coord) {
    return Json::array({coord.x, coord.z});
}

TileCoord ReadCoord(const Json& value, const char* label) {
    if (!value.is_array() || value.size() != 2 || !value[0].is_number_integer() ||
        !value[1].is_number_integer())
        throw std::runtime_error(std::string(label) + " must be a two-integer array");
    return {value[0].get<int>(), value[1].get<int>()};
}

template <typename T, typename Equal, typename Convert>
Json EncodeRuns(const std::vector<T>& values, Equal equal, Convert convert) {
    Json runs = Json::array();
    for (std::size_t i = 0; i < values.size();) {
        std::size_t end = i + 1;
        while (end < values.size() && equal(values[i], values[end]))
            ++end;
        Json run = convert(values[i]);
        run["count"] = end - i;
        runs.push_back(std::move(run));
        i = end;
    }
    return runs;
}

std::size_t ReadCount(const Json& run) {
    if (!run.is_object() || !run.contains("count") || !run["count"].is_number_unsigned())
        throw std::runtime_error("run count must be a positive integer");
    const auto count = run["count"].get<std::uint64_t>();
    if (count == 0 || count > 262656u)
        throw std::runtime_error("run count is out of range");
    return static_cast<std::size_t>(count);
}

std::uint32_t ReadU32(const Json& value, const char* label) {
    if (!value.is_number_unsigned() && !value.is_number_integer())
        throw std::runtime_error(std::string(label) + " must be an integer");
    const std::int64_t number = value.get<std::int64_t>();
    if (number < 0 || static_cast<std::uint64_t>(number) > std::numeric_limits<std::uint32_t>::max())
        throw std::runtime_error(std::string(label) + " is out of range");
    return static_cast<std::uint32_t>(number);
}

std::vector<Tile> DecodeTiles(const Json& runs, std::size_t expected) {
    if (!runs.is_array())
        throw std::runtime_error("tiles must be an array of runs");
    std::vector<Tile> tiles;
    tiles.reserve(expected);
    for (const Json& run : runs) {
        const std::size_t count = ReadCount(run);
        if (count > expected - tiles.size())
            throw std::runtime_error("tile run lengths do not match map size");
        if (!run.contains("terrain") || !run["terrain"].is_string() || !run.contains("path") ||
            !run["path"].is_string() || !run.contains("building") || !run.contains("water"))
            throw std::runtime_error("tile run is missing a required field");
        const auto terrain = TerrainFromString(run["terrain"].get<std::string>());
        const auto path = PathKindFromString(run["path"].get<std::string>());
        if (!terrain)
            throw std::runtime_error("unknown terrain name");
        if (!path)
            throw std::runtime_error("unknown path name");
        const BuildingId building{ReadU32(run["building"], "building id")};
        float water = kNoWater;
        if (!run["water"].is_null()) {
            if (!run["water"].is_number())
                throw std::runtime_error("water level must be a number of metres");
            water = run["water"].get<float>();
            if (!std::isfinite(water) || water < -12.0f || water > 40.0f)
                throw std::runtime_error("water level is out of range");
        }
        tiles.insert(tiles.end(), count, Tile{*terrain, *path, building, water});
    }
    if (tiles.size() != expected)
        throw std::runtime_error("tile run lengths do not match map size");
    return tiles;
}

std::vector<float> DecodeHeights(const Json& values, std::size_t expected) {
    if (!values.is_array() || values.size() != expected)
        throw std::runtime_error("heights have the wrong length");
    std::vector<float> heights;
    heights.reserve(expected);
    for (const Json& value : values) {
        if (!value.is_number())
            throw std::runtime_error("height must be a number of metres");
        const float height = value.get<float>();
        if (!std::isfinite(height) || height < -12.0f || height > 40.0f)
            throw std::runtime_error("height is out of range");
        heights.push_back(height);
    }
    return heights;
}

std::vector<Fence> DecodeFences(const Json& runs, std::size_t expected) {
    if (!runs.is_array())
        throw std::runtime_error("fences must be an array of runs");
    std::vector<Fence> fences;
    fences.reserve(expected);
    for (const Json& run : runs) {
        const std::size_t count = ReadCount(run);
        if (count > expected - fences.size())
            throw std::runtime_error("fence run lengths do not match map size");
        if (!run.contains("kind") || !run.contains("gate") || !run.contains("condition") ||
            !run["gate"].is_boolean() || !run["condition"].is_number())
            throw std::runtime_error("fence run is missing a required field");
        const std::uint32_t kind = ReadU32(run["kind"], "fence kind");
        const float condition = run["condition"].get<float>();
        if (kind > 255u || !std::isfinite(condition) || condition < 0.0f || condition > 1.0f)
            throw std::runtime_error("fence value is out of range");
        const bool publicDoor = run.value("visitorGate", false);
        const bool gate = run["gate"].get<bool>();
        if (publicDoor && (!gate || kind == 0u || kind == kPerimeterFence))
            throw std::runtime_error("invalid public aviary gate");
        fences.insert(fences.end(), count,
                      Fence{static_cast<std::uint8_t>(kind), gate, condition, publicDoor});
    }
    if (fences.size() != expected)
        throw std::runtime_error("fence run lengths do not match map size");
    return fences;
}

Json EncodeFences(const std::vector<Fence>& fences) {
    return EncodeRuns(
        fences, [](Fence a, Fence b) { return a == b; },
        [](Fence f) {
            Json record{{"kind", f.kind}, {"gate", f.gate}, {"condition", f.condition}};
            if (f.visitorGate)
                record["visitorGate"] = true;
            return record;
        });
}

}  // namespace

nlohmann::json ToJson(const World& world) {
    const auto& tiles = WorldAccess::Tiles(world);
    Json tileRuns = EncodeRuns(
        tiles,
        [](const Tile& a, const Tile& b) {
            return a.terrain == b.terrain && a.path == b.path && a.building == b.building &&
                   a.waterLevel == b.waterLevel;
        },
        [](const Tile& tile) {
            Json value{{"terrain", ToString(tile.terrain)},
                       {"path", ToString(tile.path)},
                       {"building", tile.building.value}};
            // Metres, exactly: a loaded world must equal the saved one bit for bit.
            value["water"] = tile.waterLevel == kNoWater ? Json(nullptr) : Json(tile.waterLevel);
            return value;
        });
    Json heights = Json::array();
    for (float height : WorldAccess::Heights(world))
        heights.push_back(height);
    return Json{
        {"width", world.Width()},
        {"depth", world.Depth()},
        {"property", {{"min", CoordJson(world.PropertyMin())}, {"max", CoordJson(world.PropertyMax())}}},
        {"entrance", CoordJson(world.EntranceTile())},
        {"heights", std::move(heights)},
        {"tiles", std::move(tileRuns)},
        {"fences_h", EncodeFences(WorldAccess::HorizontalFences(world))},
        {"fences_v", EncodeFences(WorldAccess::VerticalFences(world))}};
}

std::optional<World> WorldFromJson(const nlohmann::json& json, std::string* error) {
    if (error != nullptr)
        error->clear();
    try {
        if (!json.is_object())
            throw std::runtime_error("world document must be an object");
        if (!json.contains("width") || !json.contains("depth") || !json["width"].is_number_integer() ||
            !json["depth"].is_number_integer())
            throw std::runtime_error("width and depth must be integers");
        const int width = json["width"].get<int>();
        const int depth = json["depth"].get<int>();
        if (width < 8 || width > World::kMaxDimension || depth < 8 || depth > World::kMaxDimension)
            throw std::runtime_error("world dimensions must be in [8, " +
                                     std::to_string(World::kMaxDimension) + "]");
        for (const char* key : {"property", "entrance", "heights", "tiles", "fences_h", "fences_v"})
            if (!json.contains(key))
                throw std::runtime_error(std::string("missing field: ") + key);
        const Json& property = json["property"];
        if (!property.is_object() || !property.contains("min") || !property.contains("max"))
            throw std::runtime_error("property must contain min and max coordinates");
        const TileCoord min = ReadCoord(property["min"], "property min");
        const TileCoord max = ReadCoord(property["max"], "property max");
        const TileCoord entrance = ReadCoord(json["entrance"], "entrance");
        if (min.x < 0 || min.z < 0 || max.x >= width || max.z >= depth || min.x > max.x || min.z > max.z ||
            entrance.x < min.x || entrance.x > max.x || entrance.z < min.z || entrance.z > max.z)
            throw std::runtime_error("property rectangle or entrance is out of bounds");
        const std::size_t tileCount = static_cast<std::size_t>(width) * static_cast<std::size_t>(depth);
        const std::size_t horizontalCount =
            static_cast<std::size_t>(width) * static_cast<std::size_t>(depth + 1);
        const std::size_t verticalCount =
            static_cast<std::size_t>(width + 1) * static_cast<std::size_t>(depth);
        const auto tiles = DecodeTiles(json["tiles"], tileCount);
        const auto heights = DecodeHeights(
            json["heights"], static_cast<std::size_t>(width + 1) * static_cast<std::size_t>(depth + 1));
        const auto horizontal = DecodeFences(json["fences_h"], horizontalCount);
        const auto vertical = DecodeFences(json["fences_v"], verticalCount);
        World world(width, depth);
        WorldAccess::SetProperty(world, min, max, entrance);
        for (int z = 0; z < depth; ++z)
            for (int x = 0; x < width; ++x)
                WorldAccess::SetTile(world, {x, z},
                                     tiles[static_cast<std::size_t>(z) * static_cast<std::size_t>(width) +
                                           static_cast<std::size_t>(x)]);
        for (int z = 0; z <= depth; ++z)
            for (int x = 0; x <= width; ++x)
                WorldAccess::SetHeight(
                    world, x, z,
                    heights[static_cast<std::size_t>(z) * static_cast<std::size_t>(width + 1) +
                            static_cast<std::size_t>(x)]);
        for (int z = 0; z <= depth; ++z)
            for (int x = 0; x < width; ++x)
                WorldAccess::SetFence(
                    world, {x, z, true},
                    horizontal[static_cast<std::size_t>(z) * static_cast<std::size_t>(width) +
                               static_cast<std::size_t>(x)]);
        for (int z = 0; z < depth; ++z)
            for (int x = 0; x <= width; ++x)
                WorldAccess::SetFence(
                    world, {x, z, false},
                    vertical[static_cast<std::size_t>(z) * static_cast<std::size_t>(width + 1) +
                             static_cast<std::size_t>(x)]);
        return world;
    } catch (const std::exception& exception) {
        if (error != nullptr)
            *error = exception.what();
        return std::nullopt;
    }
}

}  // namespace zoo
