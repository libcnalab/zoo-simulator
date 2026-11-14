// SPDX-License-Identifier: MIT
#include "GroundPaint.hpp"

#include <algorithm>
#include <array>
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

/// The slow tones of a lawn at one point (TilePaintInfo's corner values, interpolated).
struct Lawn {
    float lush = 0.0f;
    float rough = 0.0f;
    /// 0 at the map's rim and 1 well inside: grass takes its large, unrepeated
    /// patches only inland, so that it meets the land beyond seamlessly.
    float inland = 0.0f;
};

// No noise below is finer than about six cycles a metre: the baked texture holds
// sixteen texels a metre, and anything finer came out as blurred mush under the
// feet. Stones, blades and joints are the detail layer's (GroundDetail.hpp).

Color3 TerrainColor(Terrain terrain, float x, float z, const Lawn& lawn = {}) {
    switch (terrain) {
        case Terrain::Grass: {
            const Color3 c = GrassColor(x, z);
            if (lawn.inland <= 0.0f)
                return c;
            Color3 varied = Mix(Color3{c[0] * 0.78f, c[1] * 0.84f, c[2] * 0.9f},
                                Color3{c[0] * 1.28f, c[1] * 1.14f, c[2] * 0.95f}, lawn.lush);
            if (lawn.rough > 0.0f) {
                // Left unmown: darker, in tufts, with the straw of seed heads.
                const float tuft = Value(x * 1.6f, z * 1.6f, 113);
                Color3 rough = Multiply(varied, 0.7f + 0.36f * tuft);
                rough = Mix(rough, Color3{0.3f, 0.29f, 0.1f}, Noise::SmoothStep(0.6f, 0.85f, tuft) * 0.4f);
                varied = Mix(varied, rough, lawn.rough);
            }
            return Mix(c, varied, lawn.inland);
        }
        case Terrain::Savanna: {
            // Dry grass in tufts over pale earth.
            const float patch = Cloud(x * 0.11f, z * 0.11f, 2, 201);
            const float tuft = Value(x * 3.1f, z * 3.1f, 202);
            const float fine = Value(x * 5.5f, z * 5.5f, 203);
            Color3 c = Mix(Color3{0.46f, 0.38f, 0.17f}, Color3{0.34f, 0.33f, 0.13f},
                           Noise::SmoothStep(0.35f, 0.7f, patch));
            c = Mix(c, Color3{0.36f, 0.26f, 0.14f}, Noise::SmoothStep(0.62f, 0.8f, tuft) * 0.55f);
            return Multiply(c, 0.84f + 0.32f * fine);
        }
        case Terrain::Dirt: {
            const float patch = Cloud(x * 0.3f, z * 0.3f, 2, 211);
            const float fine = Value(x * 5.0f, z * 5.0f, 212);
            const Color3 c = Mix(Color3{0.24f, 0.16f, 0.09f}, Color3{0.33f, 0.23f, 0.13f}, patch);
            return Multiply(c, 0.84f + 0.32f * fine);
        }
        case Terrain::Sand: {
            const float ripple =
                0.5f + 0.5f * std::sin((x * 0.8f + z * 0.35f + Value(x * 0.4f, z * 0.4f, 221) * 5.0f) * 4.0f);
            const float fine = Value(x * 5.0f, z * 5.0f, 222);
            const float patch = Cloud(x * 0.15f, z * 0.15f, 2, 223);
            const Color3 c = Mix(Color3{0.60f, 0.49f, 0.29f}, Color3{0.68f, 0.58f, 0.37f}, patch);
            return Multiply(c, 0.88f + 0.08f * ripple + 0.08f * fine);
        }
        case Terrain::Rock: {
            float second = 0.0f;
            const float cell = Noise::Worley2(x * 0.9f, z * 0.9f, 0, 231, &second);
            const float crack = Noise::SmoothStep(0.0f, 0.09f, second - cell);
            const float tone = Cloud(x * 0.6f, z * 0.6f, 2, 232);
            const float fine = Value(x * 5.0f, z * 5.0f, 233);
            const Color3 c = Mix(Color3{0.27f, 0.26f, 0.25f}, Color3{0.40f, 0.38f, 0.35f}, tone);
            return Multiply(c, (0.55f + 0.45f * crack) * (0.88f + 0.24f * fine));
        }
        case Terrain::Snow: {
            const float drift = Cloud(x * 0.25f, z * 0.25f, 2, 241);
            const float fine = Value(x * 5.0f, z * 5.0f, 242);
            const Color3 c = Mix(Color3{0.74f, 0.80f, 0.90f}, Color3{0.90f, 0.91f, 0.93f}, drift);
            return Multiply(c, 0.95f + 0.05f * fine);
        }
        case Terrain::ForestFloor: {
            const float patch = Cloud(x * 0.35f, z * 0.35f, 2, 251);
            const float litter = Value(x * 4.5f, z * 4.5f, 252);
            const float moss = Value(x * 1.3f, z * 1.3f, 254);
            Color3 c = Mix(Color3{0.13f, 0.11f, 0.06f}, Color3{0.20f, 0.17f, 0.08f}, patch);
            c = Mix(c, Color3{0.12f, 0.19f, 0.06f}, Noise::SmoothStep(0.6f, 0.8f, moss) * 0.6f);
            c = Multiply(c, 0.78f + 0.44f * litter);
            // Fallen leaves gather in drifts.
            return Mix(c, Color3{0.34f, 0.19f, 0.07f},
                       Noise::SmoothStep(0.72f, 0.9f, litter) * Noise::SmoothStep(0.3f, 0.6f, patch) * 0.6f);
        }
        case Terrain::Mud: {
            const float wet = Cloud(x * 0.5f, z * 0.5f, 2, 261);
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

GroundDetail DetailOf(Terrain terrain) {
    switch (terrain) {
        case Terrain::Grass:
            return GroundDetail::Turf;
        case Terrain::Savanna:
            return GroundDetail::Straw;
        case Terrain::Sand:
            return GroundDetail::Sand;
        case Terrain::Rock:
            return GroundDetail::Grain;
        case Terrain::Dirt:
        case Terrain::ForestFloor:
        case Terrain::Mud:
            return GroundDetail::Soil;
        case Terrain::Snow:
        case Terrain::Count:
            break;
    }
    return GroundDetail::None;
}

GroundDetail DetailOf(PathKind kind) {
    switch (kind) {
        case PathKind::Asphalt:
            return GroundDetail::Tar;
        case PathKind::Gravel:
            return GroundDetail::Grain;
        case PathKind::Cobblestone:
            return GroundDetail::Soil;  // the border course; the slabs inside it are Paving
        case PathKind::Boardwalk:
        case PathKind::None:
        case PathKind::Count:
            break;
    }
    return GroundDetail::None;
}

bool GrowsGrass(Terrain terrain) {
    return terrain == Terrain::Grass || terrain == Terrain::Savanna || terrain == Terrain::ForestFloor;
}

/// Trodden, bare earth.
Color3 EarthColor(float x, float z) {
    return Multiply(Color3{0.27f, 0.2f, 0.125f}, 0.8f + 0.36f * Value(x * 4.5f, z * 4.5f, 83));
}

/// `inside` is the distance in metres to the path's edge.
Color3 PathColor(PathKind kind, float x, float z, float inside) {
    switch (kind) {
        case PathKind::Asphalt: {
            const float stain = Cloud(x * 0.5f, z * 0.5f, 2, 302);
            const float mottle = Value(x * 3.3f, z * 3.3f, 301);
            return Multiply(Color3{0.105f, 0.105f, 0.115f}, 0.84f + 0.2f * stain + 0.12f * mottle);
        }
        case PathKind::Gravel: {
            // Bound gravel: pale where feet keep it clean, darker and dirtier along the edges.
            const float patch = Cloud(x * 0.4f, z * 0.4f, 2, 312);
            const float mottle = Value(x * 4.0f, z * 4.0f, 311);
            const Color3 c = Mix(Color3{0.42f, 0.37f, 0.29f}, Color3{0.53f, 0.48f, 0.39f}, patch);
            return Multiply(c,
                            (0.9f + 0.2f * mottle) * (0.84f + 0.2f * Noise::SmoothStep(0.1f, 0.9f, inside)));
        }
        case PathKind::Cobblestone: {
            // Concrete paving slabs in a running bond. The joints here are only the dirt
            // beside them (two texels); the detail layer draws the joints themselves.
            const float row = std::floor(z / kPavingSlabM);
            const float shifted = x / kPavingSlabM + ((static_cast<int>(row) & 1) != 0 ? 0.5f : 0.0f);
            const float column = std::floor(shifted);
            const float fx = shifted - column;
            const float fz = z / kPavingSlabM - row;
            const float edge = std::min(std::min(fx, 1.0f - fx), std::min(fz, 1.0f - fz));
            const float tone = Noise::Random2(static_cast<int>(column), static_cast<int>(row), 321);
            const float warm = Noise::Random2(static_cast<int>(column), static_cast<int>(row), 323);
            Color3 slab = Mix(Color3{0.31f, 0.3f, 0.285f}, Color3{0.41f, 0.395f, 0.37f}, tone);
            slab = Mix(slab, Color3{0.42f, 0.35f, 0.29f}, Noise::SmoothStep(0.7f, 1.0f, warm) * 0.5f);
            slab = Multiply(slab, 0.9f + 0.2f * Cloud(x * 0.7f, z * 0.7f, 2, 322));
            return Multiply(slab, 0.72f + 0.28f * Noise::SmoothStep(0.0f, 0.15f, edge));
        }
        case PathKind::Boardwalk: {
            constexpr float kPlank = 0.25f;
            const float row = std::floor(z / kPlank);
            const float fz = z / kPlank - row;
            const float tone = Noise::Random2(static_cast<int>(row), 7, 331);
            const float grain = Value(x * 1.5f, z * 6.0f, 332);
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

float Bilinear(const std::array<float, 4>& corners, float u, float v) {
    return (corners[0] * (1.0f - u) + corners[1] * u) * (1.0f - v) +
           (corners[2] * (1.0f - u) + corners[3] * u) * v;
}

/// From `a` to `b`, the ends exact.
float Along(float a, float b, float t) {
    return t <= 0.0f ? a : t >= 1.0f ? b : a + (b - a) * t;
}

/// The two triangles of World::HeightAt over a tile's four corner values. Along a
/// side it is computed from that side's two corners alone, so the two tiles that
/// share the side get the same number to the last bit.
float Triangles(const std::array<float, 4>& h, float u, float v) {
    if (u <= 0.0f)
        return Along(h[0], h[2], v);
    if (u >= 1.0f)
        return Along(h[1], h[3], v);
    if (v <= 0.0f)
        return Along(h[0], h[1], u);
    if (v >= 1.0f)
        return Along(h[2], h[3], u);
    if (v <= u)
        return h[0] + (h[1] - h[0]) * u + (h[3] - h[1]) * v;
    return h[0] + (h[3] - h[2]) * u + (h[2] - h[0]) * v;
}

/// In a bank tile the ground begins to fall from the dry land's edge somewhere
/// between 0 and kBankWanderM out (the waterline wanders), and reaches the bed
/// kBankWidthM further on. Together they stay under a tile's width: a point on the
/// side between two water tiles can then come that close to dry land only through
/// the dry sides and corners at that side's ends, which both tiles know, so both
/// draw it at the same height.
constexpr float kBankWanderM = 0.9f;
constexpr float kBankWidthM = 0.9f;
static_assert(kBankWanderM + kBankWidthM < zoo::kTileSize);
/// Where dry land lies along two sides of a water tile that meet, the water's
/// corner is rounded off with this radius (it stays clear of the other two sides).
constexpr float kBankCornerM = 1.0f;
static_assert(kBankCornerM < zoo::kTileSize);

/// How far down a bank tile's ground has come at a point: 0 at the dry land's edge,
/// 1 where it lies at the world's own depth.
float BankFall(const TilePaintInfo& info, float x, float z, float u, float v) {
    const float across = u * zoo::kTileSize;
    const float down = v * zoo::kTileSize;
    // Distance to the nearest dry side or dry corner of the tile.
    float dry = 1.0e9f;
    const float sides[4] = {down, zoo::kTileSize - across, zoo::kTileSize - down, across};  // N, E, S, W
    for (unsigned side = 0; side < 4; ++side) {
        if ((info.dryEdges & (1u << side)) != 0)
            dry = std::min(dry, sides[side]);
    }
    for (unsigned corner = 0; corner < 4; ++corner) {
        if ((info.dryEdges & (16u << corner)) == 0)
            continue;
        // Distances to the two sides that meet in this corner.
        const float side = (corner & 1u) != 0 ? zoo::kTileSize - across : across;
        const float end = (corner & 2u) != 0 ? zoo::kTileSize - down : down;
        dry = std::min(dry, std::hypot(side, end));
        const unsigned eastOrWest = (corner & 1u) != 0 ? 2u : 8u;
        const unsigned southOrNorth = (corner & 2u) != 0 ? 4u : 1u;
        if ((info.dryEdges & eastOrWest) != 0 && (info.dryEdges & southOrNorth) != 0 && side < kBankCornerM &&
            end < kBankCornerM)
            dry = std::min(dry, kBankCornerM - std::hypot(kBankCornerM - side, kBankCornerM - end));
    }
    const float wander = 0.55f * Value(x * 0.8f, z * 0.8f, 95) + 0.45f * Value(x * 2.3f, z * 2.3f, 96);
    const float start = kBankWanderM * wander;
    return Noise::SmoothStep(start, start + kBankWidthM, dry);
}

/// What the water does at a vertex of the tile grid: whether water and dry tiles
/// touch it, and the height kBankLipM above the highest water there (never below
/// the world's own height).
struct Rim {
    bool wet = false;
    bool dry = false;
    float rim = 0.0f;
};
Rim VertexRim(const World& world, int vx, int vz) {
    Rim out;
    float level = 0.0f;
    for (int dz = -1; dz <= 0; ++dz) {
        for (int dx = -1; dx <= 0; ++dx) {
            const TileCoord t{vx + dx, vz + dz};
            if (!world.InBounds(t))
                continue;
            const float tileLevel = world.At(t).waterLevel;
            if (tileLevel == zoo::kNoWater) {
                out.dry = true;
            } else {
                level = out.wet ? std::max(level, tileLevel) : tileLevel;
                out.wet = true;
            }
        }
    }
    const float height = world.VertexHeight(vx, vz);
    out.rim = out.wet ? std::max(height, level + kBankLipM) : height;
    return out;
}

/// The heights a tile's drawn ground is made from: its drawn and its world corners,
/// whether they touch water, and for a water tile where the dry land is. `water`
/// and `inside` are the nine tiles' (index (dz + 1) * 3 + (dx + 1)).
void TakeHeights(const World& world, const bool (&water)[9], const bool (&inside)[9], TilePaintInfo& info) {
    for (int dz = 0; dz <= 1; ++dz) {
        for (int dx = 0; dx <= 1; ++dx) {
            const auto corner = static_cast<std::size_t>(dz * 2 + dx);
            const int vx = info.tile.x + dx;
            const int vz = info.tile.z + dz;
            info.bed[corner] = world.VertexHeight(vx, vz);
            info.height[corner] = DrawnVertexHeight(world, vx, vz);
            info.rim[corner] = VertexRim(world, vx, vz).rim;
            // The four tiles around the corner, as indices into the nine.
            const std::size_t around[4] = {static_cast<std::size_t>(dz * 3 + dx),
                                           static_cast<std::size_t>(dz * 3 + dx + 1),
                                           static_cast<std::size_t>((dz + 1) * 3 + dx),
                                           static_cast<std::size_t>((dz + 1) * 3 + dx + 1)};
            bool dry = false;
            info.dug[corner] = false;
            for (const std::size_t at : around) {
                info.dug[corner] = info.dug[corner] || water[at];
                dry = dry || (inside[at] && !water[at]);
            }
            if (water[4] && dry)
                info.dryEdges = static_cast<std::uint8_t>(info.dryEdges | (16u << corner));
        }
    }
    if (water[4]) {
        constexpr std::size_t kAcross[4] = {1, 5, 7, 3};  // N, E, S, W
        for (unsigned side = 0; side < 4; ++side) {
            if (inside[kAcross[side]] && !water[kAcross[side]])
                info.dryEdges = static_cast<std::uint8_t>(info.dryEdges | (1u << side));
        }
    }
    info.bank = (info.dryEdges & 0xf0u) != 0;
}

/// Bare rock shows from this slope on (rise over run); its edge wanders by kRockWander either way.
constexpr float kRockSlope = 0.5f;
constexpr float kRockWander = 0.125f;

TileCoord Clamped(const World& world, TileCoord t) {
    return {std::clamp(t.x, 0, world.Width() - 1), std::clamp(t.z, 0, world.Depth() - 1)};
}

/// The three tiles' shares along one axis at `t` (0..1 across the middle tile):
/// a quadratic B-spline, so a field of tile values becomes smooth, a straight
/// border stays on the tiles' common side and corners come out rounded.
struct Spread {
    float weight[3];
    float slope[3];
};
Spread SpreadAt(float t) {
    return {{0.5f * (1.0f - t) * (1.0f - t), 0.5f + t * (1.0f - t), 0.5f * t * t},
            {t - 1.0f, 1.0f - 2.0f * t, t}};
}

}  // namespace

zoo::Color3 GrassColor(float x, float z) {
    // Every frequency below divides kGrassPeriodM, so the result repeats.
    const float patch = Noise::Fbm(x * 0.125f, z * 0.125f, 8, 3, 2.0f, 0.5f, 101);
    const float dry = Noise::Fbm(x * 0.625f, z * 0.625f, 40, 2, 2.0f, 0.5f, 102);
    const float clump = Noise::Value2(x * 2.5f, z * 2.5f, 160, 103);
    const float fine = Noise::Value2(x * 5.0f, z * 5.0f, 320, 104);
    Color3 c = Mix(Color3{0.105f, 0.20f, 0.05f}, Color3{0.155f, 0.27f, 0.068f},
                   Noise::SmoothStep(0.3f, 0.7f, patch));
    c = Mix(c, Color3{0.27f, 0.29f, 0.09f}, Noise::SmoothStep(0.55f, 0.85f, dry) * 0.45f);
    c = Mix(c, Color3{0.06f, 0.14f, 0.035f}, Noise::SmoothStep(0.6f, 0.9f, clump) * 0.35f);
    return Multiply(c, 0.84f + 0.32f * fine);
}

TilePaintInfo PaintInfo(const zoo::World& world, zoo::TileCoord tile) {
    TilePaintInfo info;
    info.tile = tile;
    bool grass = false;
    bool water[9] = {};
    bool inside[9] = {};
    for (int dz = -1; dz <= 1; ++dz) {
        for (int dx = -1; dx <= 1; ++dx) {
            const auto at = static_cast<std::size_t>((dz + 1) * 3 + dx + 1);
            const TileCoord t{tile.x + dx, tile.z + dz};
            const zoo::Tile& data = world.At(Clamped(world, t));
            info.terrain[at] = data.terrain;
            grass = grass || data.terrain == Terrain::Grass;
            info.mixedTerrain = info.mixedTerrain || data.terrain != world.At(Clamped(world, tile)).terrain;
            if (!world.InBounds(t))
                continue;
            inside[at] = true;
            info.path[at] = data.path;
            info.building[at] = static_cast<bool>(data.building);
            info.standOff[at] = data.standOff;
            info.paved = info.paved || data.path != PathKind::None || static_cast<bool>(data.building);
            info.mulched = info.mulched || data.standOff;
            if (data.waterLevel != zoo::kNoWater) {
                water[at] = true;
                info.waterLevel = std::max(info.waterLevel, data.waterLevel);
            }
        }
    }
    TakeHeights(world, water, inside, info);
    for (int dz = 0; dz <= 1; ++dz) {
        for (int dx = 0; dx <= 1; ++dx) {
            const auto corner = static_cast<std::size_t>(dz * 2 + dx);
            const int vx = tile.x + dx;
            const int vz = tile.z + dz;
            const float height = info.height[corner];
            const float slopeX = std::max(std::abs(DrawnVertexHeight(world, vx + 1, vz) - height),
                                          std::abs(height - DrawnVertexHeight(world, vx - 1, vz)));
            const float slopeZ = std::max(std::abs(DrawnVertexHeight(world, vx, vz + 1) - height),
                                          std::abs(height - DrawnVertexHeight(world, vx, vz - 1)));
            info.slope[corner] = std::sqrt(slopeX * slopeX + slopeZ * slopeZ) / zoo::kTileSize;
            const float px = static_cast<float>(vx) * zoo::kTileSize;
            const float pz = static_cast<float>(vz) * zoo::kTileSize;
            const float rim = std::min(std::min(px, world.SizeX() - px), std::min(pz, world.SizeZ() - pz));
            info.inland[corner] = Noise::SmoothStep(1.0f, 11.0f, rim);
            if (grass) {
                info.lush[corner] = Noise::SmoothStep(0.3f, 0.7f, Cloud(px / 38.0f, pz / 38.0f, 3, 111));
                info.rough[corner] = Noise::SmoothStep(0.52f, 0.66f, Cloud(px / 17.0f, pz / 17.0f, 2, 112));
            }
        }
    }
    // Rock shows from a slope of 0.5 on, give or take its wandering edge (SampleGround).
    info.steep = *std::max_element(info.slope.begin(), info.slope.end()) > kRockSlope - kRockWander;
    if (world.InBounds(tile)) {
        for (int side = 0; side < 4; ++side) {
            const zoo::Fence fence = world.FenceOn(tile, static_cast<zoo::Dir>(side));
            if (fence.Exists())
                info.fences =
                    static_cast<std::uint8_t>(info.fences | (1u << static_cast<unsigned>(side)) |
                                              (fence.gate ? 16u << static_cast<unsigned>(side) : 0u));
        }
    }
    info.plain = !info.mixedTerrain && !info.paved && !info.mulched && !info.steep && info.fences == 0 &&
                 info.waterLevel == zoo::kNoWater;
    return info;
}

float DrawnVertexHeight(const zoo::World& world, int vx, int vz) {
    const Rim rim = VertexRim(world, vx, vz);
    return rim.wet && rim.dry ? rim.rim : world.VertexHeight(vx, vz);
}

float GroundHeight(const TilePaintInfo& info, float x, float z) {
    const float u = std::clamp(x / zoo::kTileSize - static_cast<float>(info.tile.x), 0.0f, 1.0f);
    const float v = std::clamp(z / zoo::kTileSize - static_cast<float>(info.tile.z), 0.0f, 1.0f);
    if (!info.bank)
        return Triangles(info.height, u, v);
    // From the rim down to the world's bed. Not from the plane through the drawn
    // corners: that sinks under the water a few centimetres from a dry side, along a
    // straight line. At a dry corner the rim is the drawn corner; at any other corner
    // the bank has reached the bed (BankFall), so the rim there does not show.
    // std::lerp is exact at both ends, where the neighbours' numbers must be met.
    return std::lerp(Triangles(info.rim, u, v), Triangles(info.bed, u, v), BankFall(info, x, z, u, v));
}

float DrawnGroundHeight(const zoo::World& world, zoo::Vec2 at) {
    at.x = std::clamp(at.x, 0.0f, world.SizeX());
    at.z = std::clamp(at.z, 0.0f, world.SizeZ());
    TilePaintInfo info;
    info.tile = Clamped(world, World::TileAt(at));
    bool water[9] = {};
    bool inside[9] = {};
    bool nearWater = false;
    for (int dz = -1; dz <= 1; ++dz) {
        for (int dx = -1; dx <= 1; ++dx) {
            const TileCoord t{info.tile.x + dx, info.tile.z + dz};
            const auto index = static_cast<std::size_t>((dz + 1) * 3 + dx + 1);
            inside[index] = world.InBounds(t);
            water[index] = world.HasWater(t);
            nearWater = nearWater || water[index];
        }
    }
    if (!nearWater)
        return world.HeightAt(at);
    TakeHeights(world, water, inside, info);
    if (!info.bank) {
        // The tile's two triangles (an uncut tile beside a bank tile is cut into fans
        // that lie in the same planes).
        const float u = std::clamp(at.x / zoo::kTileSize - static_cast<float>(info.tile.x), 0.0f, 1.0f);
        const float v = std::clamp(at.z / zoo::kTileSize - static_cast<float>(info.tile.z), 0.0f, 1.0f);
        return Triangles(info.height, u, v);
    }

    // Interpolate the same three vertices as the mesh, not the curved profile
    // between them.
    constexpr float cell = zoo::kTileSize / static_cast<float>(kGroundCells);
    const float gx = (at.x - static_cast<float>(info.tile.x) * zoo::kTileSize) / cell;
    const float gz = (at.z - static_cast<float>(info.tile.z) * zoo::kTileSize) / cell;
    const int i = std::min(static_cast<int>(gx), kGroundCells - 1);
    const int j = std::min(static_cast<int>(gz), kGroundCells - 1);
    const float u = gx - static_cast<float>(i);
    const float v = gz - static_cast<float>(j);
    const float x = static_cast<float>(info.tile.x) * zoo::kTileSize + static_cast<float>(i) * cell;
    const float z = static_cast<float>(info.tile.z) * zoo::kTileSize + static_cast<float>(j) * cell;
    const float a = GroundHeight(info, x, z);
    const float d = GroundHeight(info, x + cell, z + cell);
    if (v <= u) {
        const float b = GroundHeight(info, x + cell, z);
        return a + (b - a) * u + (d - b) * v;
    }
    const float c = GroundHeight(info, x, z + cell);
    return a + (d - c) * u + (c - a) * v;
}

GroundSample SampleGround(const TilePaintInfo& info, float x, float z) {
    const float u = std::clamp(x / zoo::kTileSize - static_cast<float>(info.tile.x), 0.0f, 1.0f);
    const float v = std::clamp(z / zoo::kTileSize - static_cast<float>(info.tile.z), 0.0f, 1.0f);
    const Terrain here = info.terrain[4];
    Lawn lawn;
    lawn.inland = Bilinear(info.inland, u, v);
    if (here == Terrain::Grass || info.mixedTerrain) {
        lawn.lush = Bilinear(info.lush, u, v);
        lawn.rough = Bilinear(info.rough, u, v);
    }

    GroundSample out;
    if (info.plain) {
        out.color = TerrainColor(here, x, z, lawn);
        out.detail = DetailOf(here);
        return out;
    }

    const Spread sx = SpreadAt(u);
    const Spread sz = SpreadAt(v);

    // Terrain: each kind's share of the nine tiles, pushed about by its own
    // noise; the strongest wins, over a narrow soft edge that wanders a metre.
    Terrain top = here;
    if (!info.mixedTerrain) {
        out.color = TerrainColor(here, x, z, lawn);
    } else {
        float share[static_cast<std::size_t>(Terrain::Count)] = {};
        for (std::size_t j = 0; j < 3; ++j) {
            for (std::size_t i = 0; i < 3; ++i)
                share[static_cast<std::size_t>(info.terrain[j * 3 + i])] += sx.weight[i] * sz.weight[j];
        }
        float best = -1.0e9f;
        float second = -1.0e9f;
        Terrain runnerUp = here;
        for (std::size_t t = 0; t < static_cast<std::size_t>(Terrain::Count); ++t) {
            if (share[t] <= 0.0f)
                continue;
            const auto seed = static_cast<std::uint32_t>(400 + t * 3);
            const float score = share[t] + (Value(x * 0.45f, z * 0.45f, seed) - 0.5f) * 0.5f +
                                (Value(x * 1.9f, z * 1.9f, seed + 1) - 0.5f) * 0.2f +
                                (Value(x * 5.0f, z * 5.0f, seed + 2) - 0.5f) * 0.07f;
            if (score > best) {
                second = best;
                runnerUp = top;
                best = score;
                top = static_cast<Terrain>(t);
            } else if (score > second) {
                second = score;
                runnerUp = static_cast<Terrain>(t);
            }
        }
        out.color = TerrainColor(top, x, z, lawn);
        const float lead = Noise::SmoothStep(0.0f, 0.1f, best - second);
        if (lead < 1.0f)
            out.color = Mix(TerrainColor(runnerUp, x, z, lawn), out.color, 0.5f + 0.5f * lead);
    }
    out.detail = DetailOf(top);

    bool paved = false;
    if (info.building[4]) {
        const float fine = Value(x * 5.0f, z * 5.0f, 41);
        out.color = Multiply(Color3{0.36f, 0.35f, 0.33f}, 0.88f + 0.24f * fine);
        out.detail = GroundDetail::Grain;
        paved = true;
    } else {
        // Paths: the tiles' paths spread into a smooth field, whose half-way
        // contour is the path's outline. `inside` is the distance to it in metres.
        PathKind kind = PathKind::None;
        float inside = -4.0f;
        if (info.paved) {
            float share[static_cast<std::size_t>(PathKind::Count)] = {};
            float total = 0.0f;
            float slopeX = 0.0f;
            float slopeZ = 0.0f;
            for (std::size_t j = 0; j < 3; ++j) {
                for (std::size_t i = 0; i < 3; ++i) {
                    const PathKind path = info.path[j * 3 + i];
                    if (path == PathKind::None && !info.building[j * 3 + i])
                        continue;
                    share[static_cast<std::size_t>(path)] += sx.weight[i] * sz.weight[j];
                    total += sx.weight[i] * sz.weight[j];
                    slopeX += sx.slope[i] * sz.weight[j];
                    slopeZ += sx.weight[i] * sz.slope[j];
                }
            }
            float most = 0.0f;
            for (std::size_t k = 1; k < static_cast<std::size_t>(PathKind::Count); ++k) {
                if (share[k] > most) {
                    most = share[k];
                    kind = static_cast<PathKind>(k);
                }
            }
            const float slope = std::max(std::sqrt(slopeX * slopeX + slopeZ * slopeZ), 0.25f);
            inside = (total - 0.5f) / slope * zoo::kTileSize;
            // Gravel has no kerb to hold it: its edge is ragged.
            if (kind == PathKind::Gravel)
                inside += (Value(x * 1.1f, z * 1.1f, 313) - 0.5f) * 0.5f +
                          (Value(x * 3.7f, z * 3.7f, 314) - 0.5f) * 0.18f;
        }
        if (kind != PathKind::None && inside > 0.0f) {
            paved = true;
            const Color3 ground = out.color;
            out.color = PathColor(kind, x, z, inside);
            out.detail = DetailOf(kind);
            switch (kind) {
                case PathKind::Asphalt: {
                    // A flush concrete edging.
                    const Color3 kerb = Multiply(Color3{0.34f, 0.335f, 0.32f},
                                                 0.88f + 0.24f * Value(x * 3.0f, z * 3.0f, 304));
                    out.color = Mix(kerb, out.color, Noise::SmoothStep(0.1f, 0.16f, inside));
                    if (inside < 0.13f)
                        out.detail = GroundDetail::Grain;
                    break;
                }
                case PathKind::Cobblestone: {
                    // A border course of darker setts closes the slabs off.
                    const Color3 setts =
                        Multiply(Color3{0.2f, 0.195f, 0.19f}, 0.85f + 0.3f * Value(x * 4.0f, z * 4.0f, 324));
                    const float border = Noise::SmoothStep(0.2f, 0.26f, inside);
                    out.color = Mix(setts, out.color, border);
                    if (border >= 0.5f)
                        out.detail = GroundDetail::Paving;
                    break;
                }
                case PathKind::Gravel: {
                    // Grass creeps in from the sides in tufts.
                    const float tufts = Noise::SmoothStep(0.5f, 0.0f, inside) *
                                        Noise::SmoothStep(0.5f, 0.75f, Value(x * 2.6f, z * 2.6f, 315));
                    out.color = Mix(out.color, ground, tufts * 0.75f);
                    break;
                }
                case PathKind::Boardwalk:
                    out.color =
                        Mix(Color3{0.2f, 0.14f, 0.08f}, out.color, Noise::SmoothStep(0.09f, 0.14f, inside));
                    break;
                case PathKind::None:
                case PathKind::Count:
                    break;
            }
        } else {
            float bare = 0.0f;  // how much of the plant cover is worn away
            if (info.mulched) {
                // A planted stand-off: dark bark mulch with paler chips, ragged where it meets grass.
                float mulched = 0.0f;
                for (std::size_t j = 0; j < 3; ++j) {
                    for (std::size_t i = 0; i < 3; ++i)
                        mulched += info.standOff[j * 3 + i] ? sx.weight[i] * sz.weight[j] : 0.0f;
                }
                mulched += (Value(x * 0.9f, z * 0.9f, 73) - 0.5f) * 0.3f +
                           (Value(x * 3.1f, z * 3.1f, 74) - 0.5f) * 0.12f;
                const float cover = Noise::SmoothStep(0.46f, 0.54f, mulched);
                if (cover > 0.0f) {
                    const float chips = Value(x * 5.5f, z * 5.5f, 71);
                    const float patch = Value(x * 1.7f, z * 1.7f, 72);
                    Color3 mulch = Multiply(Color3{0.21f, 0.145f, 0.095f}, 0.78f + 0.3f * patch);
                    mulch =
                        Mix(mulch, Color3{0.33f, 0.25f, 0.17f}, Noise::SmoothStep(0.65f, 0.9f, chips) * 0.5f);
                    mulch = Mix(mulch, Color3{0.1f, 0.075f, 0.055f},
                                (1.0f - Noise::SmoothStep(0.1f, 0.35f, chips)) * 0.4f);
                    out.color = Mix(out.color, mulch, cover * 0.93f);
                    if (cover > 0.5f)
                        out.detail = GroundDetail::Soil;
                }
            }
            if (GrowsGrass(top)) {
                if (kind != PathKind::None && inside > -1.0f) {
                    // The verge: a strip of soil along the path's edge, and here
                    // and there the grass trodden away where people cut the corner.
                    const float crumble = Value(x * 2.3f, z * 2.3f, 82);
                    const float trodden = Noise::SmoothStep(0.5f, 0.68f, Value(x * 0.19f, z * 0.19f, 81));
                    bare = std::max(
                        Noise::SmoothStep(-0.14f - 0.12f * crumble, -0.03f, inside) * 0.85f,
                        Noise::SmoothStep(-0.9f, -0.05f, inside) * trodden * (0.5f + 0.5f * crumble));
                }
                if ((info.fences & 0x0fu) != 0) {
                    // Along a fence the animals' pacing and the keepers' boots leave
                    // the ground bare; at a gate much more so.
                    const float sides[4] = {v, 1.0f - u, 1.0f - v, u};
                    const float ragged = (Value(x * 1.7f, z * 1.7f, 91) - 0.5f) * 0.4f;
                    for (unsigned side = 0; side < 4; ++side) {
                        if ((info.fences & (1u << side)) == 0)
                            continue;
                        const float reach = (info.fences & (16u << side)) != 0 ? 1.5f : 0.55f;
                        const float distance = sides[side] * zoo::kTileSize + ragged;
                        bare = std::max(bare, Noise::SmoothStep(reach, reach * 0.3f, distance) * 0.75f);
                    }
                }
            }
            if (bare > 0.0f) {
                out.color = Mix(out.color, EarthColor(x, z), bare);
                if (bare > 0.5f)
                    out.detail = GroundDetail::Soil;
            }
            if (kind == PathKind::Gravel && inside > -0.6f) {
                // Stones kicked off the path.
                const float scatter = Noise::SmoothStep(-0.6f, 0.0f, inside) *
                                      Noise::SmoothStep(0.62f, 0.8f, Value(x * 5.0f, z * 5.0f, 84));
                out.color = Mix(out.color, PathColor(kind, x, z, 0.0f), scatter * 0.6f);
            }
            if (info.steep && top != Terrain::Rock) {
                // Bare rock and earth where the ground is too steep for anything to grow.
                const float slope = Bilinear(info.slope, u, v);
                const float wander = (Value(x * 0.8f, z * 0.8f, 51) - 0.5f) * 2.0f * kRockWander;
                const float rock = Noise::SmoothStep(kRockSlope, kRockSlope + 0.45f, slope + wander);
                if (rock > 0.0f) {
                    const Color3 cliff =
                        Mix(TerrainColor(Terrain::Rock, x, z), TerrainColor(Terrain::Dirt, x, z), 0.35f);
                    out.color = Mix(out.color, cliff, rock * 0.9f);
                    if (rock > 0.5f)
                        out.detail = GroundDetail::Grain;
                }
            }
        }
    }

    // Water: the bank above it wet and trodden to mud along a ragged line, the
    // bed pale in the shallows and dark in the deep (the water lets it show).
    if (info.waterLevel != zoo::kNoWater) {
        const float depth = info.waterLevel - GroundHeight(info, x, z);
        if (depth > -0.4f) {
            const float coarse = Value(x * 1.6f, z * 1.6f, 61) - 0.5f;
            const float fine = Value(x * 5.0f, z * 5.0f, 62) - 0.5f;
            // How near the water a point of a dry tile is: 1 on its side at the water,
            // 0 on its far side (2 m away), where the next tile has no water to paint
            // from. Corner weights are shared on tile sides, chunk borders included.
            const float nearWater = (info.dug[0] ? (1.0f - u) * (1.0f - v) : 0.0f) +
                                    (info.dug[1] ? u * (1.0f - v) : 0.0f) +
                                    (info.dug[2] ? (1.0f - u) * v : 0.0f) + (info.dug[3] ? u * v : 0.0f);
            // Damp up to some 25 cm above the water and a metre or so from it, along a ragged line.
            const float ragged = (Value(x * 0.9f, z * 0.9f, 63) - 0.5f) * 0.5f + coarse * 0.25f;
            const float wet = Noise::SmoothStep(-0.24f, -0.03f, depth + coarse * 0.12f + fine * 0.05f) *
                              Noise::SmoothStep(0.4f, 0.8f, nearWater + ragged);
            if (wet > 0.0f) {
                const Color3 mud = Multiply(Color3{0.125f, 0.092f, 0.064f}, 0.85f + 0.5f * (fine + 0.5f));
                const Color3 damp = Mix(Multiply(out.color, 0.5f), mud, paved ? 0.0f : 0.7f);
                out.color = Mix(out.color, damp, wet);
                if (!paved && wet > 0.5f)
                    out.detail = GroundDetail::Soil;
            }
            if (depth > -0.02f) {
                const Color3 bed = Multiply(Mix(Color3{0.4f, 0.37f, 0.25f}, Color3{0.03f, 0.055f, 0.055f},
                                                Noise::SmoothStep(0.02f, 0.5f, depth + coarse * 0.08f)),
                                            0.85f + 0.3f * (fine + 0.5f));
                out.color = Mix(out.color, bed, Noise::SmoothStep(-0.02f, 0.07f, depth));
                if (depth > 0.03f)
                    out.detail = GroundDetail::None;
            }
        }
    }
    return out;
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
            hash = zoo::HashU32(
                hash, static_cast<std::uint32_t>(data.terrain) | static_cast<std::uint32_t>(data.path) << 8u |
                          (data.building ? 1u << 16u : 0u) | (data.standOff ? 1u << 17u : 0u));
            hash = zoo::HashU32(hash, std::bit_cast<std::uint32_t>(data.waterLevel));
        }
    }
    // The water one tile further out: it decides the drawn heights of the ring of
    // vertices around the corners, and so the corners' slopes.
    for (int dz = -2; dz <= 2; ++dz) {
        for (int dx = -2; dx <= 2; ++dx) {
            if (std::max(std::abs(dx), std::abs(dz)) == 2)
                hash = zoo::HashU32(hash, std::bit_cast<std::uint32_t>(
                                              world.InBounds({tile.x + dx, tile.z + dz})
                                                  ? world.At({tile.x + dx, tile.z + dz}).waterLevel
                                                  : 0.0f));
        }
    }
    // The tile's corners and the ring of vertices around them (the corners' slopes).
    for (int dz = -1; dz <= 2; ++dz) {
        for (int dx = -1; dx <= 2; ++dx)
            hash = zoo::HashU32(hash,
                                std::bit_cast<std::uint32_t>(world.VertexHeight(tile.x + dx, tile.z + dz)));
    }
    std::uint32_t fences = 0;
    for (unsigned side = 0; side < 4; ++side) {
        const zoo::Fence fence = world.FenceOn(tile, static_cast<zoo::Dir>(side));
        if (fence.Exists())
            fences |= (fence.gate ? 0x11u : 0x1u) << side;
    }
    return zoo::HashU32(hash, fences);
}

}  // namespace zoogame
