// SPDX-License-Identifier: MIT
#include "GroundPaint.hpp"

#include <algorithm>
#include <bit>
#include <cmath>

#include "CnaEngine/Core/Noise.hpp"
#include "zoo/base/Rng.hpp"

namespace zoogame {

namespace {

namespace Noise = CnaEngine::Noise;
using zoo::Color3;
using zoo::PathKind;
using zoo::Terrain;
using zoo::TileCoord;
using zoo::World;

/// Value noise without a period.
float Value(float x, float z, std::uint32_t seed) {
    return Noise::Value2(x, z, 0, seed);
}

float Cloud(float x, float z, int octaves, std::uint32_t seed) {
    return Noise::Fbm(x, z, 0, octaves, 2.0f, 0.5f, seed);
}

Color3 Multiply(const Color3& c, float s) {
    return {c[0] * s, c[1] * s, c[2] * s};
}

/// `inland` is 0 at the map's rim and 1 well inside: grass takes its large,
/// unrepeated patches only inland, so that it meets the land beyond seamlessly.
Color3 TerrainColor(Terrain terrain, float x, float z, float inland = 0.0f) {
    switch (terrain) {
        case Terrain::Grass: {
            const Color3 c = GrassColor(x, z);
            if (inland <= 0.0f)
                return c;
            const float tone = Cloud(x / 38.0f, z / 38.0f, 3, 111);
            const float lush = Noise::SmoothStep(0.3f, 0.7f, tone);
            const Color3 varied = Mix(Color3{c[0] * 0.78f, c[1] * 0.84f, c[2] * 0.9f},
                                      Color3{c[0] * 1.28f, c[1] * 1.14f, c[2] * 0.95f}, lush);
            return Mix(c, varied, inland);
        }
        case Terrain::Savanna: {
            // Dry grass in tufts over pale earth.
            const float patch = Cloud(x * 0.11f, z * 0.11f, 3, 201);
            const float tuft = Value(x * 3.1f, z * 3.1f, 202);
            const float fine = Value(x * 9.0f, z * 9.0f, 203);
            Color3 c = Mix(Color3{0.46f, 0.38f, 0.17f}, Color3{0.34f, 0.33f, 0.13f},
                           Noise::SmoothStep(0.35f, 0.7f, patch));
            c = Mix(c, Color3{0.36f, 0.26f, 0.14f}, Noise::SmoothStep(0.62f, 0.8f, tuft) * 0.55f);
            return Multiply(c, 0.8f + 0.4f * fine);
        }
        case Terrain::Dirt: {
            const float patch = Cloud(x * 0.3f, z * 0.3f, 3, 211);
            const float fine = Value(x * 7.0f, z * 7.0f, 212);
            const float pebble = Value(x * 14.0f, z * 14.0f, 213);
            Color3 c = Mix(Color3{0.24f, 0.16f, 0.09f}, Color3{0.33f, 0.23f, 0.13f}, patch);
            c = Multiply(c, 0.8f + 0.4f * fine);
            return Mix(c, Color3{0.42f, 0.38f, 0.32f}, Noise::SmoothStep(0.8f, 0.9f, pebble) * 0.6f);
        }
        case Terrain::Sand: {
            const float ripple =
                0.5f + 0.5f * std::sin((x * 0.8f + z * 0.35f + Value(x * 0.4f, z * 0.4f, 221) * 5.0f) * 4.0f);
            const float fine = Value(x * 12.0f, z * 12.0f, 222);
            const float patch = Cloud(x * 0.15f, z * 0.15f, 2, 223);
            const Color3 c = Mix(Color3{0.60f, 0.49f, 0.29f}, Color3{0.68f, 0.58f, 0.37f}, patch);
            return Multiply(c, 0.86f + 0.08f * ripple + 0.12f * fine);
        }
        case Terrain::Rock: {
            float second = 0.0f;
            const float cell = Noise::Worley2(x * 0.9f, z * 0.9f, 0, 231, &second);
            const float crack = Noise::SmoothStep(0.0f, 0.09f, second - cell);
            const float tone = Cloud(x * 0.6f, z * 0.6f, 3, 232);
            const float fine = Value(x * 8.0f, z * 8.0f, 233);
            const Color3 c = Mix(Color3{0.27f, 0.26f, 0.25f}, Color3{0.40f, 0.38f, 0.35f}, tone);
            return Multiply(c, (0.55f + 0.45f * crack) * (0.85f + 0.3f * fine));
        }
        case Terrain::Snow: {
            const float drift = Cloud(x * 0.25f, z * 0.25f, 3, 241);
            const float fine = Value(x * 6.0f, z * 6.0f, 242);
            const Color3 c = Mix(Color3{0.74f, 0.80f, 0.90f}, Color3{0.90f, 0.91f, 0.93f}, drift);
            return Multiply(c, 0.95f + 0.05f * fine);
        }
        case Terrain::ForestFloor: {
            const float patch = Cloud(x * 0.35f, z * 0.35f, 3, 251);
            const float litter = Value(x * 6.0f, z * 6.0f, 252);
            const float leaf = Value(x * 13.0f, z * 13.0f, 253);
            Color3 c = Mix(Color3{0.13f, 0.11f, 0.06f}, Color3{0.20f, 0.17f, 0.08f}, patch);
            c = Mix(c, Color3{0.12f, 0.19f, 0.06f},
                    Noise::SmoothStep(0.6f, 0.8f, Value(x * 1.3f, z * 1.3f, 254)) * 0.6f);
            c = Multiply(c, 0.75f + 0.5f * litter);
            return Mix(c, Color3{0.34f, 0.19f, 0.07f}, Noise::SmoothStep(0.78f, 0.9f, leaf) * 0.7f);
        }
        case Terrain::Mud: {
            const float wet = Cloud(x * 0.5f, z * 0.5f, 3, 261);
            const float fine = Value(x * 5.0f, z * 5.0f, 262);
            const Color3 c = Mix(Color3{0.20f, 0.14f, 0.09f}, Color3{0.11f, 0.08f, 0.06f},
                                 Noise::SmoothStep(0.4f, 0.65f, wet));
            return Multiply(c, 0.85f + 0.3f * fine);
        }
        case Terrain::Count:
            break;
    }
    return {0.5f, 0.5f, 0.5f};
}

Color3 PathColor(PathKind kind, float x, float z) {
    switch (kind) {
        case PathKind::Asphalt: {
            const float fine = Value(x * 16.0f, z * 16.0f, 301);
            const float stain = Cloud(x * 0.5f, z * 0.5f, 2, 302);
            Color3 c = Multiply(Color3{0.105f, 0.105f, 0.115f}, 0.8f + 0.3f * fine + 0.15f * stain);
            return Mix(c, Color3{0.3f, 0.3f, 0.3f},
                       Noise::SmoothStep(0.9f, 0.97f, Value(x * 23.0f, z * 23.0f, 303)) * 0.22f);
        }
        case PathKind::Gravel: {
            const float stone = Value(x * 15.0f, z * 15.0f, 311);
            const float patch = Cloud(x * 0.4f, z * 0.4f, 2, 312);
            const Color3 c = Mix(Color3{0.44f, 0.39f, 0.31f}, Color3{0.54f, 0.49f, 0.40f}, patch);
            return Multiply(c, 0.6f + 0.75f * stone);
        }
        case PathKind::Cobblestone: {
            constexpr float kStone = 0.3333f;
            const float row = std::floor(z / kStone);
            const float shifted = x / kStone + (static_cast<int>(row) & 1 ? 0.5f : 0.0f);
            const float column = std::floor(shifted);
            const float fx = shifted - column;
            const float fz = z / kStone - row;
            const float edge = std::min(std::min(fx, 1.0f - fx), std::min(fz, 1.0f - fz));
            const float tone = Noise::Random2(static_cast<int>(column), static_cast<int>(row), 321);
            const float fine = Value(x * 20.0f, z * 20.0f, 322);
            const Color3 stone = Multiply(Mix(Color3{0.30f, 0.29f, 0.27f}, Color3{0.44f, 0.41f, 0.37f}, tone),
                                          0.9f + 0.2f * fine);
            return Mix(Color3{0.10f, 0.095f, 0.085f}, stone, Noise::SmoothStep(0.05f, 0.2f, edge));
        }
        case PathKind::Boardwalk: {
            constexpr float kPlank = 0.25f;
            const float row = std::floor(z / kPlank);
            const float fz = z / kPlank - row;
            const float tone = Noise::Random2(static_cast<int>(row), 7, 331);
            const float grain = Value(x * 2.0f, z * 30.0f, 332);
            const Color3 plank = Multiply(Mix(Color3{0.32f, 0.22f, 0.13f}, Color3{0.42f, 0.31f, 0.19f}, tone),
                                          0.85f + 0.3f * grain);
            return Mix(Color3{0.06f, 0.045f, 0.03f}, plank,
                       Noise::SmoothStep(0.0f, 0.22f, std::min(fz, 1.0f - fz)));
        }
        case PathKind::None:
        case PathKind::Count:
            break;
    }
    return {0.5f, 0.5f, 0.5f};
}

TileCoord Clamped(const World& world, TileCoord t) {
    return {std::clamp(t.x, 0, world.Width() - 1), std::clamp(t.z, 0, world.Depth() - 1)};
}

PathKind PathAt(const World& world, TileCoord t) {
    return world.InBounds(t) ? world.At(t).path : PathKind::None;
}

/// Distance in metres from a point in tile `t` to the nearest side of the tile
/// beyond which `inside` no longer holds.
template <typename Inside>
float EdgeDistance(TileCoord t, float x, float z, Inside&& inside) {
    const float lx = x - static_cast<float>(t.x) * zoo::kTileSize;
    const float lz = z - static_cast<float>(t.z) * zoo::kTileSize;
    float distance = 1.0e9f;
    if (!inside(TileCoord{t.x - 1, t.z}))
        distance = std::min(distance, lx);
    if (!inside(TileCoord{t.x + 1, t.z}))
        distance = std::min(distance, zoo::kTileSize - lx);
    if (!inside(TileCoord{t.x, t.z - 1}))
        distance = std::min(distance, lz);
    if (!inside(TileCoord{t.x, t.z + 1}))
        distance = std::min(distance, zoo::kTileSize - lz);
    return distance;
}

}  // namespace

zoo::Color3 GrassColor(float x, float z) {
    // Every frequency below divides kGrassPeriodM, so the result repeats.
    const float patch = Noise::Fbm(x * 0.125f, z * 0.125f, 8, 3, 2.0f, 0.5f, 101);
    const float dry = Noise::Fbm(x * 0.625f, z * 0.625f, 40, 2, 2.0f, 0.5f, 102);
    const float clump = Noise::Value2(x * 2.5f, z * 2.5f, 160, 103);
    const float fine = Noise::Value2(x * 7.0f, z * 7.0f, 448, 104);
    const float blade = Noise::Value2(x * 15.0f, z * 15.0f, 960, 105);
    Color3 c = Mix(Color3{0.105f, 0.20f, 0.05f}, Color3{0.155f, 0.27f, 0.068f},
                   Noise::SmoothStep(0.3f, 0.7f, patch));
    c = Mix(c, Color3{0.27f, 0.29f, 0.09f}, Noise::SmoothStep(0.55f, 0.85f, dry) * 0.45f);
    c = Mix(c, Color3{0.06f, 0.14f, 0.035f}, Noise::SmoothStep(0.6f, 0.9f, clump) * 0.35f);
    return {c[0] * (0.78f + 0.3f * fine + 0.18f * blade), c[1] * (0.78f + 0.3f * fine + 0.18f * blade),
            c[2] * (0.78f + 0.3f * fine + 0.18f * blade)};
}

TilePaintInfo PaintInfo(const zoo::World& world, zoo::TileCoord tile) {
    TilePaintInfo info;
    info.tile = tile;
    for (int dz = -1; dz <= 1; ++dz) {
        for (int dx = -1; dx <= 1; ++dx) {
            const TileCoord t{tile.x + dx, tile.z + dz};
            if (world.HasWater(t))
                info.waterLevel = std::max(info.waterLevel, world.At(t).waterLevel);
        }
    }
    return info;
}

zoo::Color3 GroundColor(const zoo::World& world, const TilePaintInfo& info, float x, float z) {
    const TileCoord tile = info.tile;
    const zoo::Tile& here = world.At(tile);

    // The sample point wanders by up to 0.7 m, which makes terrain borders irregular.
    const float warpX =
        (Value(x * 0.35f, z * 0.35f, 21) - 0.5f) * 1.1f + (Value(x * 1.3f, z * 1.3f, 22) - 0.5f) * 0.3f;
    const float warpZ =
        (Value(x * 0.35f, z * 0.35f, 23) - 0.5f) * 1.1f + (Value(x * 1.3f, z * 1.3f, 24) - 0.5f) * 0.3f;

    Color3 color;
    if (here.building) {
        const float fine = Value(x * 9.0f, z * 9.0f, 41);
        color = Multiply(Color3{0.36f, 0.35f, 0.33f}, 0.85f + 0.3f * fine);
    } else {
        // Terrain: the four nearest tile centres, blended over a narrow band.
        const float px = (x + warpX) / zoo::kTileSize - 0.5f;
        const float pz = (z + warpZ) / zoo::kTileSize - 0.5f;
        const int ix = static_cast<int>(std::floor(px));
        const int iz = static_cast<int>(std::floor(pz));
        const float tx = Noise::SmoothStep(0.3f, 0.7f, px - static_cast<float>(ix));
        const float tz = Noise::SmoothStep(0.3f, 0.7f, pz - static_cast<float>(iz));
        const Terrain t00 = world.At(Clamped(world, {ix, iz})).terrain;
        const Terrain t10 = world.At(Clamped(world, {ix + 1, iz})).terrain;
        const Terrain t01 = world.At(Clamped(world, {ix, iz + 1})).terrain;
        const Terrain t11 = world.At(Clamped(world, {ix + 1, iz + 1})).terrain;
        const float rim = std::min(std::min(x, world.SizeX() - x), std::min(z, world.SizeZ() - z));
        const float inland = Noise::SmoothStep(1.0f, 11.0f, rim);
        if (t00 == t10 && t00 == t01 && t00 == t11) {
            color = TerrainColor(t00, x, z, inland);
        } else {
            const Color3 north =
                t00 == t10 ? TerrainColor(t00, x, z, inland)
                           : Mix(TerrainColor(t00, x, z, inland), TerrainColor(t10, x, z, inland), tx);
            const Color3 south =
                t01 == t11 ? TerrainColor(t01, x, z, inland)
                           : Mix(TerrainColor(t01, x, z, inland), TerrainColor(t11, x, z, inland), tx);
            color = Mix(north, south, tz);
        }

        // Paths: crisp, except gravel, whose edge is as ragged as the terrain's.
        PathKind path = here.path;
        const TileCoord wandered = World::TileAt({x + warpX * 0.35f, z + warpZ * 0.35f});
        if (path == PathKind::Gravel && PathAt(world, wandered) == PathKind::None)
            path = PathKind::None;
        else if (path == PathKind::None && PathAt(world, wandered) == PathKind::Gravel)
            path = PathKind::Gravel;
        if (path != PathKind::None) {
            color = PathColor(path, x, z);
            if (path != PathKind::Gravel && here.path != PathKind::None) {
                const float edge = EdgeDistance(tile, x, z, [&](TileCoord t) {
                    return PathAt(world, t) != PathKind::None || (world.InBounds(t) && world.At(t).building);
                });
                const Color3 kerb =
                    path == PathKind::Boardwalk ? Color3{0.2f, 0.14f, 0.08f} : Color3{0.3f, 0.3f, 0.29f};
                color = Mix(kerb, color, Noise::SmoothStep(0.09f, 0.13f, edge));
            }
        } else {
            // Bare rock and earth where the ground is too steep for anything to grow.
            const float h00 = world.VertexHeight(tile.x, tile.z);
            const float h10 = world.VertexHeight(tile.x + 1, tile.z);
            const float h01 = world.VertexHeight(tile.x, tile.z + 1);
            const float h11 = world.VertexHeight(tile.x + 1, tile.z + 1);
            const float u = x / zoo::kTileSize - static_cast<float>(tile.x);
            const float v = z / zoo::kTileSize - static_cast<float>(tile.z);
            const float gx = ((h10 - h00) * (1.0f - v) + (h11 - h01) * v) / zoo::kTileSize;
            const float gz = ((h01 - h00) * (1.0f - u) + (h11 - h10) * u) / zoo::kTileSize;
            const float slope = std::sqrt(gx * gx + gz * gz);
            const float bare =
                Noise::SmoothStep(0.5f, 0.95f, slope + (Value(x * 0.8f, z * 0.8f, 51) - 0.5f) * 0.25f);
            if (bare > 0.0f && here.terrain != Terrain::Rock) {
                const Color3 cliff =
                    Mix(TerrainColor(Terrain::Rock, x, z), TerrainColor(Terrain::Dirt, x, z), 0.35f);
                color = Mix(color, cliff, bare * 0.9f);
            }
        }
    }

    // Water: a dark, wet band along the shore and the bed fading into the deep.
    if (info.waterLevel != zoo::kNoWater) {
        const float depth = info.waterLevel - world.HeightAt({x, z});
        const float wet =
            Noise::SmoothStep(-0.22f, 0.02f, depth + (Value(x * 2.0f, z * 2.0f, 61) - 0.5f) * 0.1f);
        color = Mix(color, Multiply(color, 0.5f), wet);
        color = Mix(color, Color3{0.07f, 0.10f, 0.08f}, Noise::SmoothStep(0.0f, 0.7f, depth) * 0.75f);
    }
    return color;
}

std::uint32_t PaintSignature(const zoo::World& world, zoo::TileCoord tile) {
    std::uint32_t hash = 0x51ed270bu;
    for (int dz = -1; dz <= 1; ++dz) {
        for (int dx = -1; dx <= 1; ++dx) {
            const TileCoord t{tile.x + dx, tile.z + dz};
            if (!world.InBounds(t)) {
                hash = zoo::HashU32(hash, 0xffffffffu);
                continue;
            }
            const zoo::Tile& data = world.At(t);
            hash = zoo::HashU32(hash, static_cast<std::uint32_t>(data.terrain) |
                                          static_cast<std::uint32_t>(data.path) << 8u |
                                          (data.building ? 1u << 16u : 0u));
            hash = zoo::HashU32(hash, std::bit_cast<std::uint32_t>(data.waterLevel));
        }
    }
    for (int dz = 0; dz <= 1; ++dz) {
        for (int dx = 0; dx <= 1; ++dx)
            hash = zoo::HashU32(hash,
                                std::bit_cast<std::uint32_t>(world.VertexHeight(tile.x + dx, tile.z + dz)));
    }
    return hash;
}

}  // namespace zoogame
