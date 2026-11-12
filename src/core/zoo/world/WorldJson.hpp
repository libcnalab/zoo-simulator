// SPDX-License-Identifier: MIT
#pragma once

#include "zoo/world/World.hpp"

#include <nlohmann/json.hpp>

#include <optional>
#include <string>

namespace zoo {

nlohmann::json ToJson(const World& world);
std::optional<World> WorldFromJson(const nlohmann::json& json, std::string* error = nullptr);

struct WorldAccess {
    static const std::vector<Tile>& Tiles(const World& world) { return world.tiles_; }
    static const std::vector<float>& Heights(const World& world) { return world.heights_; }
    static const std::vector<Fence>& HorizontalFences(const World& world) { return world.fencesHorizontal_; }
    static const std::vector<Fence>& VerticalFences(const World& world) { return world.fencesVertical_; }
    static void SetProperty(World& world, TileCoord min, TileCoord max, TileCoord entrance) {
        world.propertyMin_ = min;
        world.propertyMax_ = max;
        world.entrance_ = entrance;
    }
    static void SetTile(World& world, TileCoord t, Tile tile) { world.tiles_[world.Index(t)] = tile; }
    static void SetHeight(World& world, int x, int z, float height) {
        world.heights_[static_cast<std::size_t>(z) * static_cast<std::size_t>(world.width_ + 1) +
                       static_cast<std::size_t>(x)] = height;
    }
    static void SetFence(World& world, EdgeRef e, Fence fence) {
        const std::size_t index =
            e.horizontal ? static_cast<std::size_t>(e.z) * static_cast<std::size_t>(world.width_) +
                               static_cast<std::size_t>(e.x)
                         : static_cast<std::size_t>(e.z) * static_cast<std::size_t>(world.width_ + 1) +
                               static_cast<std::size_t>(e.x);
        (e.horizontal ? world.fencesHorizontal_ : world.fencesVertical_)[index] = fence;
    }
};

}  // namespace zoo
