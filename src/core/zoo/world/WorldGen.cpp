// SPDX-License-Identifier: MIT
#include "zoo/world/World.hpp"

#include "zoo/base/Rng.hpp"
#include "zoo/world/WorldJson.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace zoo {
namespace {

float Smooth(float value) {
    const float t = Clamp01(value);
    return t * t * (3.0f - 2.0f * t);
}

float Lattice(int x, int z, std::uint64_t seed, std::uint32_t octave) {
    const auto ux = static_cast<std::uint32_t>(x);
    const auto uz = static_cast<std::uint32_t>(z);
    const auto seedBits = static_cast<std::uint32_t>(seed) ^ static_cast<std::uint32_t>(seed >> 32u);
    const std::uint32_t h = HashU32(ux ^ seedBits, uz ^ (octave * 0x9e3779b9u));
    return static_cast<float>(h >> 8u) * (2.0f / 16777215.0f) - 1.0f;
}

float Noise(float x, float z, std::uint64_t seed, std::uint32_t octave) {
    const int x0 = static_cast<int>(std::floor(x));
    const int z0 = static_cast<int>(std::floor(z));
    const float fx = Smooth(x - static_cast<float>(x0));
    const float fz = Smooth(z - static_cast<float>(z0));
    return Lerp(Lerp(Lattice(x0, z0, seed, octave), Lattice(x0 + 1, z0, seed, octave), fx),
                Lerp(Lattice(x0, z0 + 1, seed, octave), Lattice(x0 + 1, z0 + 1, seed, octave), fx), fz);
}

float ReliefAt(float x, float z, std::uint64_t seed, float relief) {
    const float value = Noise(x / 30.0f, z / 30.0f, seed, 1u) * 1.65f +
                        Noise(x / 13.0f, z / 13.0f, seed, 2u) * 0.68f +
                        Noise(x / 5.5f, z / 5.5f, seed, 3u) * 0.32f;
    return value * relief;
}

}  // namespace

World World::Generate(const WorldGenParams& params) {
    World world(params.width, params.depth);
    const int marginMaxX = std::max(0, (world.width_ - 16) / 2);
    const int marginMaxZ = std::max(0, (world.depth_ - 16) / 2);
    const int marginX = std::clamp(params.margin, 0, marginMaxX);
    const int marginZ = std::clamp(params.margin, 0, marginMaxZ);
    world.propertyMin_ = {marginX, marginZ};
    world.propertyMax_ = {world.width_ - marginX - 1, world.depth_ - marginZ - 1};
    const int gateWidth = std::clamp(params.entranceWidth, 2, 8);
    const int gateStart = (world.width_ - gateWidth) / 2;
    const int gateCenter = gateStart + (gateWidth - 1) / 2;
    world.entrance_ = {gateCenter, world.propertyMax_.z};

    const float relief = std::isfinite(params.relief) ? std::max(0.0f, params.relief) : 0.0f;
    const int plazaHalfWidth = (gateWidth + 4) / 2;
    const float target = ReliefAt(static_cast<float>(world.entrance_.x),
                                  static_cast<float>(world.entrance_.z), params.seed, relief);
    for (int z = 0; z <= world.depth_; ++z) {
        for (int x = 0; x <= world.width_; ++x) {
            float h = ReliefAt(static_cast<float>(x), static_cast<float>(z), params.seed, relief);
            const float dx = std::max(0.0f, std::abs(static_cast<float>(x - world.entrance_.x)) -
                                                static_cast<float>(plazaHalfWidth));
            const float insideDistance =
                std::max(dx, std::max(0.0f, static_cast<float>(world.entrance_.z - 5 - z)));
            const float outsideDistance =
                std::max(dx, std::max(0.0f, static_cast<float>(z - world.entrance_.z - 1)));
            const float distance = z > world.entrance_.z ? outsideDistance : insideDistance;
            const float flatten = 1.0f - Smooth(distance / 3.0f);
            h = Lerp(h, target, flatten);
            h = static_cast<float>(std::lround(h * 1000.0f)) / 1000.0f;
            WorldAccess::SetHeight(world, x, z, h);
        }
    }

    const int plazaWidth = gateWidth + 4;
    const int plazaStart = std::max(0, world.entrance_.x - (plazaWidth - 1) / 2);
    const int plazaEnd = std::min(world.width_ - 1, plazaStart + plazaWidth - 1);
    for (int z = std::max(0, world.entrance_.z - 5); z <= world.entrance_.z; ++z) {
        for (int x = plazaStart; x <= plazaEnd; ++x) {
            Tile& tile = world.tiles_[world.Index({x, z})];
            tile.path = PathKind::Asphalt;
        }
    }
    for (int z = world.entrance_.z + 1; z < world.depth_; ++z) {
        for (int x = gateStart; x < gateStart + gateWidth; ++x)
            world.tiles_[world.Index({x, z})].path = PathKind::Asphalt;
    }

    for (int z = 0; z < world.depth_; ++z) {
        for (int x = 0; x < world.width_; ++x) {
            const TileCoord t{x, z};
            const float patch = Noise((static_cast<float>(x) + 91.0f) / 8.0f,
                                      (static_cast<float>(z) - 37.0f) / 8.0f, params.seed ^ 0xa51u, 4u);
            if (std::abs(x - world.entrance_.x) > plazaWidth && std::abs(z - world.entrance_.z) > 8) {
                Tile& tile = world.tiles_[world.Index(t)];
                if (patch > 0.52f)
                    tile.terrain = Terrain::ForestFloor;
                else if (patch < -0.58f)
                    tile.terrain = Terrain::Dirt;
            }
        }
    }

    for (int z = world.propertyMin_.z; z <= world.propertyMax_.z; ++z) {
        for (int x = world.propertyMin_.x; x <= world.propertyMax_.x; ++x) {
            const TileCoord t{x, z};
            const bool north = !world.IsProperty({x, z - 1});
            const bool south = !world.IsProperty({x, z + 1});
            const bool west = !world.IsProperty({x - 1, z});
            const bool east = !world.IsProperty({x + 1, z});
            if (north)
                WorldAccess::SetFence(world, EdgeOf(t, Dir::North), {kPerimeterFence, false, 1.0f});
            if (south) {
                const bool gate = z == world.propertyMax_.z && x >= gateStart && x < gateStart + gateWidth;
                WorldAccess::SetFence(world, EdgeOf(t, Dir::South), {kPerimeterFence, gate, 1.0f});
            }
            if (west)
                WorldAccess::SetFence(world, EdgeOf(t, Dir::West), {kPerimeterFence, false, 1.0f});
            if (east)
                WorldAccess::SetFence(world, EdgeOf(t, Dir::East), {kPerimeterFence, false, 1.0f});
        }
    }
    return world;
}

}  // namespace zoo
