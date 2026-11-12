// SPDX-License-Identifier: MIT
#include "zoo/world/World.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <utility>

namespace zoo {
namespace {

float ClampHeight(float value) {
    return std::clamp(value, World::kMinHeight, World::kMaxHeight);
}

float Falloff(float distance, float radius) {
    const float t = Clamp01(1.0f - distance / radius);
    return t * t * (3.0f - 2.0f * t);
}

}  // namespace

World::World(int width, int depth)
    : width_(std::clamp(width, 8, 512)),
      depth_(std::clamp(depth, 8, 512)),
      propertyMin_{0, 0},
      propertyMax_{width_ - 1, depth_ - 1},
      entrance_{width_ / 2, depth_ - 1},
      tiles_(static_cast<std::size_t>(width_) * static_cast<std::size_t>(depth_)),
      heights_(static_cast<std::size_t>(width_ + 1) * static_cast<std::size_t>(depth_ + 1), 0.0f),
      fencesHorizontal_(static_cast<std::size_t>(width_) * static_cast<std::size_t>(depth_ + 1)),
      fencesVertical_(static_cast<std::size_t>(width_ + 1) * static_cast<std::size_t>(depth_)),
      chunkRevisions_(static_cast<std::size_t>(ChunksX()) * static_cast<std::size_t>(ChunksZ()), 1) {}

bool World::IsProperty(TileCoord t) const {
    return InBounds(t) && t.x >= propertyMin_.x && t.x <= propertyMax_.x && t.z >= propertyMin_.z &&
           t.z <= propertyMax_.z;
}

Vec2 World::SpawnPoint() const {
    return TileCenter({std::clamp(entrance_.x, 0, width_ - 1), depth_ - 1});
}

void World::TouchTile(TileCoord t) {
    if (!InBounds(t))
        return;
    const int cx = t.x / kChunkTiles;
    const int cz = t.z / kChunkTiles;
    const auto bump = [this](int x, int z) {
        if (x >= 0 && z >= 0 && x < ChunksX() && z < ChunksZ())
            ++chunkRevisions_[static_cast<std::size_t>(z) * static_cast<std::size_t>(ChunksX()) +
                              static_cast<std::size_t>(x)];
    };
    bump(cx, cz);
    if (t.x % kChunkTiles == 0)
        bump(cx - 1, cz);
    if ((t.x + 1) % kChunkTiles == 0)
        bump(cx + 1, cz);
    if (t.z % kChunkTiles == 0)
        bump(cx, cz - 1);
    if ((t.z + 1) % kChunkTiles == 0)
        bump(cx, cz + 1);
}

void World::TouchVertex(int vx, int vz) {
    const int minX = std::max(0, vx - 1);
    const int maxX = std::min(width_ - 1, vx);
    const int minZ = std::max(0, vz - 1);
    const int maxZ = std::min(depth_ - 1, vz);
    std::array<bool, 1024> touched{};
    for (int z = minZ; z <= maxZ; ++z)
        for (int x = minX; x <= maxX; ++x) {
            const std::size_t index =
                static_cast<std::size_t>(z / kChunkTiles) * static_cast<std::size_t>(ChunksX()) +
                static_cast<std::size_t>(x / kChunkTiles);
            if (!touched[index]) {
                touched[index] = true;
                ++chunkRevisions_[index];
            }
        }
    ++navRevision_;
}

void World::SetTerrain(TileCoord t, Terrain terrain) {
    if (!InBounds(t) || At(t).terrain == terrain)
        return;
    tiles_[Index(t)].terrain = terrain;
    TouchTile(t);
}

void World::SetPath(TileCoord t, PathKind path) {
    if (!InBounds(t) || At(t).path == path)
        return;
    tiles_[Index(t)].path = path;
    TouchTile(t);
    ++navRevision_;
}

void World::SetBuilding(TileCoord t, BuildingId building) {
    if (!InBounds(t) || At(t).building == building)
        return;
    tiles_[Index(t)].building = building;
    ++navRevision_;
}

void World::SetWater(TileCoord t, float level, float depth) {
    if (!InBounds(t))
        return;
    Tile& tile = tiles_[Index(t)];
    const float safeDepth = std::max(0.0f, depth);
    const float safeLevel = std::isfinite(level) ? level : 0.0f;
    const bool changed = tile.waterLevel != safeLevel;
    tile.waterLevel = safeLevel;
    for (int vz : {t.z, t.z + 1}) {
        for (int vx : {t.x, t.x + 1})
            SetVertexHeight(vx, vz, std::min(VertexHeight(vx, vz), safeLevel - safeDepth));
    }
    if (changed) {
        TouchTile(t);
        ++navRevision_;
    }
}

void World::ClearWater(TileCoord t) {
    if (!HasWater(t))
        return;
    tiles_[Index(t)].waterLevel = kNoWater;
    TouchTile(t);
    ++navRevision_;
}

float World::WaterDepthAt(Vec2 p) const {
    const TileCoord t = TileAt(p);
    if (!HasWater(t))
        return 0.0f;
    return std::max(0.0f, At(t).waterLevel - HeightAt(p));
}

float World::VertexHeight(int vx, int vz) const {
    vx = std::clamp(vx, 0, width_);
    vz = std::clamp(vz, 0, depth_);
    return heights_[static_cast<std::size_t>(vz) * static_cast<std::size_t>(width_ + 1) +
                    static_cast<std::size_t>(vx)];
}

void World::SetVertexHeight(int vx, int vz, float height) {
    if (!std::isfinite(height))
        return;
    vx = std::clamp(vx, 0, width_);
    vz = std::clamp(vz, 0, depth_);
    height = ClampHeight(height);
    const std::size_t index =
        static_cast<std::size_t>(vz) * static_cast<std::size_t>(width_ + 1) + static_cast<std::size_t>(vx);
    if (heights_[index] == height)
        return;
    heights_[index] = height;
    TouchVertex(vx, vz);
}

float World::HeightAt(Vec2 p) const {
    const float gx = std::clamp(p.x / kTileSize, 0.0f, static_cast<float>(width_));
    const float gz = std::clamp(p.z / kTileSize, 0.0f, static_cast<float>(depth_));
    const int x = std::min(static_cast<int>(std::floor(gx)), width_ - 1);
    const int z = std::min(static_cast<int>(std::floor(gz)), depth_ - 1);
    const float u = gx - static_cast<float>(x);
    const float v = gz - static_cast<float>(z);
    const float h00 = VertexHeight(x, z);
    const float h10 = VertexHeight(x + 1, z);
    const float h01 = VertexHeight(x, z + 1);
    const float h11 = VertexHeight(x + 1, z + 1);
    if (v <= u)
        return h00 + (h10 - h00) * u + (h11 - h10) * v;
    return h00 + (h11 - h01) * u + (h01 - h00) * v;
}

Vec3 World::NormalAt(Vec2 p) const {
    const float gx = std::clamp(p.x / kTileSize, 0.0f, static_cast<float>(width_));
    const float gz = std::clamp(p.z / kTileSize, 0.0f, static_cast<float>(depth_));
    const int x = std::min(static_cast<int>(std::floor(gx)), width_ - 1);
    const int z = std::min(static_cast<int>(std::floor(gz)), depth_ - 1);
    const float u = gx - static_cast<float>(x);
    const float v = gz - static_cast<float>(z);
    const float h00 = VertexHeight(x, z);
    const float h10 = VertexHeight(x + 1, z);
    const float h01 = VertexHeight(x, z + 1);
    const float h11 = VertexHeight(x + 1, z + 1);
    const float dx = (v <= u ? h10 - h00 : h11 - h01) / kTileSize;
    const float dz = (v <= u ? h11 - h10 : h01 - h00) / kTileSize;
    return Normalized(Vec3{-dx, 1.0f, -dz});
}

float World::TileHeight(TileCoord t) const {
    if (!InBounds(t))
        return 0.0f;
    return (VertexHeight(t.x, t.z) + VertexHeight(t.x + 1, t.z) + VertexHeight(t.x, t.z + 1) +
            VertexHeight(t.x + 1, t.z + 1)) *
           0.25f;
}

float World::TileRelief(TileCoord t) const {
    if (!InBounds(t))
        return 0.0f;
    const std::array<float, 4> values = {VertexHeight(t.x, t.z), VertexHeight(t.x + 1, t.z),
                                         VertexHeight(t.x, t.z + 1), VertexHeight(t.x + 1, t.z + 1)};
    const auto [lo, hi] = std::minmax_element(values.begin(), values.end());
    return *hi - *lo;
}

namespace {
bool VertexHasBuilding(const World& world, int vx, int vz) {
    for (int z = std::max(0, vz - 1); z <= std::min(world.Depth() - 1, vz); ++z) {
        for (int x = std::max(0, vx - 1); x <= std::min(world.Width() - 1, vx); ++x) {
            if (world.At({x, z}).building)
                return true;
        }
    }
    return false;
}
}  // namespace

void World::RaiseBrush(Vec2 center, float radius, float amount) {
    if (!(radius > 0.0f) || !std::isfinite(radius) || !std::isfinite(amount))
        return;
    const int minX = std::max(1, static_cast<int>(std::floor(center.x / kTileSize - radius / kTileSize)));
    const int maxX =
        std::min(width_ - 1, static_cast<int>(std::ceil(center.x / kTileSize + radius / kTileSize)));
    const int minZ = std::max(1, static_cast<int>(std::floor(center.z / kTileSize - radius / kTileSize)));
    const int maxZ =
        std::min(depth_ - 1, static_cast<int>(std::ceil(center.z / kTileSize + radius / kTileSize)));
    for (int z = minZ; z <= maxZ; ++z)
        for (int x = minX; x <= maxX; ++x) {
            const float distance =
                Distance(center, {static_cast<float>(x) * kTileSize, static_cast<float>(z) * kTileSize});
            if (distance < radius && !VertexHasBuilding(*this, x, z))
                SetVertexHeight(x, z, VertexHeight(x, z) + amount * Falloff(distance, radius));
        }
}

void World::FlattenBrush(Vec2 center, float radius, float targetHeight, float strength) {
    if (!(radius > 0.0f) || !std::isfinite(radius) || !std::isfinite(targetHeight))
        return;
    strength = Clamp01(strength);
    const int minX = std::max(1, static_cast<int>(std::floor(center.x / kTileSize - radius / kTileSize)));
    const int maxX =
        std::min(width_ - 1, static_cast<int>(std::ceil(center.x / kTileSize + radius / kTileSize)));
    const int minZ = std::max(1, static_cast<int>(std::floor(center.z / kTileSize - radius / kTileSize)));
    const int maxZ =
        std::min(depth_ - 1, static_cast<int>(std::ceil(center.z / kTileSize + radius / kTileSize)));
    for (int z = minZ; z <= maxZ; ++z)
        for (int x = minX; x <= maxX; ++x) {
            const float distance =
                Distance(center, {static_cast<float>(x) * kTileSize, static_cast<float>(z) * kTileSize});
            if (distance < radius && !VertexHasBuilding(*this, x, z)) {
                const float fraction = strength * Falloff(distance, radius);
                SetVertexHeight(x, z, Lerp(VertexHeight(x, z), targetHeight, fraction));
            }
        }
}

void World::SmoothBrush(Vec2 center, float radius, float strength) {
    if (!(radius > 0.0f) || !std::isfinite(radius))
        return;
    strength = Clamp01(strength);
    const auto original = heights_;
    const int minX = std::max(1, static_cast<int>(std::floor(center.x / kTileSize - radius / kTileSize)));
    const int maxX =
        std::min(width_ - 1, static_cast<int>(std::ceil(center.x / kTileSize + radius / kTileSize)));
    const int minZ = std::max(1, static_cast<int>(std::floor(center.z / kTileSize - radius / kTileSize)));
    const int maxZ =
        std::min(depth_ - 1, static_cast<int>(std::ceil(center.z / kTileSize + radius / kTileSize)));
    for (int z = minZ; z <= maxZ; ++z)
        for (int x = minX; x <= maxX; ++x) {
            const float distance =
                Distance(center, {static_cast<float>(x) * kTileSize, static_cast<float>(z) * kTileSize});
            if (distance >= radius || VertexHasBuilding(*this, x, z))
                continue;
            float sum = 0.0f;
            int count = 0;
            for (const TileCoord offset : std::array<TileCoord, 4>{{{-1, 0}, {1, 0}, {0, -1}, {0, 1}}}) {
                const int nx = x + offset.x;
                const int nz = z + offset.z;
                if (nx >= 0 && nz >= 0 && nx <= width_ && nz <= depth_) {
                    sum += original[static_cast<std::size_t>(nz) * static_cast<std::size_t>(width_ + 1) +
                                    static_cast<std::size_t>(nx)];
                    ++count;
                }
            }
            const float current =
                original[static_cast<std::size_t>(z) * static_cast<std::size_t>(width_ + 1) +
                         static_cast<std::size_t>(x)];
            SetVertexHeight(
                x, z, Lerp(current, sum / static_cast<float>(count), strength * Falloff(distance, radius)));
        }
}

void World::FlattenTiles(TileCoord min, TileCoord max, float height) {
    if (!std::isfinite(height))
        return;
    min.x = std::clamp(min.x, 0, width_ - 1);
    min.z = std::clamp(min.z, 0, depth_ - 1);
    max.x = std::clamp(max.x, 0, width_ - 1);
    max.z = std::clamp(max.z, 0, depth_ - 1);
    if (min.x > max.x || min.z > max.z)
        return;
    for (int z = min.z; z <= max.z; ++z)
        for (int x = min.x; x <= max.x; ++x) {
            SetVertexHeight(x, z, height);
            SetVertexHeight(x + 1, z, height);
            SetVertexHeight(x, z + 1, height);
            SetVertexHeight(x + 1, z + 1, height);
        }
}

bool World::EdgeInBounds(EdgeRef e) const {
    return e.horizontal ? e.x >= 0 && e.x < width_ && e.z >= 0 && e.z <= depth_
                        : e.x >= 0 && e.x <= width_ && e.z >= 0 && e.z < depth_;
}

std::optional<Vec3> World::Raycast(Vec3 origin, Vec3 direction, float maxDistance) const {
    if (!(maxDistance >= 0.0f) || !std::isfinite(maxDistance))
        return std::nullopt;
    direction = Normalized(direction, {});
    if (Dot(direction, direction) < 1.0e-12f)
        return std::nullopt;
    float enter = 0.0f;
    float leave = maxDistance;
    const auto clip = [&enter, &leave](float o, float d, float low, float high) {
        if (std::abs(d) < 1.0e-8f)
            return o >= low && o <= high;
        float a = (low - o) / d;
        float b = (high - o) / d;
        if (a > b)
            std::swap(a, b);
        enter = std::max(enter, a);
        leave = std::min(leave, b);
        return enter <= leave;
    };
    if (!clip(origin.x, direction.x, 0.0f, SizeX()) || !clip(origin.z, direction.z, 0.0f, SizeZ()) ||
        leave < 0.0f)
        return std::nullopt;
    enter = std::max(0.0f, enter);
    const Vec3 atEntry = origin + direction * (enter + 1.0e-5f);
    int x = std::clamp(static_cast<int>(std::floor(atEntry.x / kTileSize)), 0, width_ - 1);
    int z = std::clamp(static_cast<int>(std::floor(atEntry.z / kTileSize)), 0, depth_ - 1);
    const int stepX = direction.x > 0.0f ? 1 : (direction.x < 0.0f ? -1 : 0);
    const int stepZ = direction.z > 0.0f ? 1 : (direction.z < 0.0f ? -1 : 0);
    const float infinity = std::numeric_limits<float>::infinity();
    const float deltaX = stepX == 0 ? infinity : kTileSize / std::abs(direction.x);
    const float deltaZ = stepZ == 0 ? infinity : kTileSize / std::abs(direction.z);
    float nextX = stepX > 0
                      ? (static_cast<float>(x + 1) * kTileSize - origin.x) / direction.x
                      : (stepX < 0 ? (static_cast<float>(x) * kTileSize - origin.x) / direction.x : infinity);
    float nextZ = stepZ > 0
                      ? (static_cast<float>(z + 1) * kTileSize - origin.z) / direction.z
                      : (stepZ < 0 ? (static_cast<float>(z) * kTileSize - origin.z) / direction.z : infinity);
    float best = infinity;
    const auto triangleHit = [&](Vec3 a, Vec3 b, Vec3 c) {
        const Vec3 e1 = b - a;
        const Vec3 e2 = c - a;
        const Vec3 p = Cross(direction, e2);
        const float determinant = Dot(e1, p);
        if (std::abs(determinant) < 1.0e-8f)
            return;
        const float inverse = 1.0f / determinant;
        const Vec3 fromA = origin - a;
        const float u = Dot(fromA, p) * inverse;
        if (u < 0.0f || u > 1.0f)
            return;
        const Vec3 q = Cross(fromA, e1);
        const float v = Dot(direction, q) * inverse;
        if (v < 0.0f || u + v > 1.0f)
            return;
        const float distance = Dot(e2, q) * inverse;
        if (distance < enter - 1.0e-4f || distance > leave + 1.0e-4f || distance >= best)
            return;
        if (Dot(Cross(e2, e1), direction) >= 0.0f)
            return;
        best = distance;
    };
    while (x >= 0 && z >= 0 && x < width_ && z < depth_ && enter <= leave && enter <= best) {
        const float x0 = static_cast<float>(x) * kTileSize;
        const float z0 = static_cast<float>(z) * kTileSize;
        const Vec3 p00{x0, VertexHeight(x, z), z0};
        const Vec3 p10{x0 + kTileSize, VertexHeight(x + 1, z), z0};
        const Vec3 p01{x0, VertexHeight(x, z + 1), z0 + kTileSize};
        const Vec3 p11{x0 + kTileSize, VertexHeight(x + 1, z + 1), z0 + kTileSize};
        triangleHit(p00, p10, p11);
        triangleHit(p00, p11, p01);
        if (nextX < nextZ) {
            enter = nextX;
            nextX += deltaX;
            x += stepX;
        } else {
            enter = nextZ;
            nextZ += deltaZ;
            z += stepZ;
        }
    }
    if (!std::isfinite(best))
        return std::nullopt;
    return origin + direction * best;
}

Fence World::FenceAt(EdgeRef e) const {
    if (!EdgeInBounds(e))
        return {};
    const std::size_t i =
        e.horizontal
            ? static_cast<std::size_t>(e.z) * static_cast<std::size_t>(width_) + static_cast<std::size_t>(e.x)
            : static_cast<std::size_t>(e.z) * static_cast<std::size_t>(width_ + 1) +
                  static_cast<std::size_t>(e.x);
    return e.horizontal ? fencesHorizontal_[i] : fencesVertical_[i];
}

void World::SetFence(EdgeRef e, Fence fence) {
    if (!EdgeInBounds(e))
        return;
    Fence& stored =
        e.horizontal ? fencesHorizontal_[static_cast<std::size_t>(e.z) * static_cast<std::size_t>(width_) +
                                         static_cast<std::size_t>(e.x)]
                     : fencesVertical_[static_cast<std::size_t>(e.z) * static_cast<std::size_t>(width_ + 1) +
                                       static_cast<std::size_t>(e.x)];
    if (stored == fence)
        return;
    stored = fence;
    ++fenceRevision_;
    ++navRevision_;
}

std::uint32_t World::ChunkRevision(int cx, int cz) const {
    if (cx < 0 || cz < 0 || cx >= ChunksX() || cz >= ChunksZ())
        return 0;
    return chunkRevisions_[static_cast<std::size_t>(cz) * static_cast<std::size_t>(ChunksX()) +
                           static_cast<std::size_t>(cx)];
}

bool World::operator==(const World& other) const {
    if (width_ != other.width_ || depth_ != other.depth_ || propertyMin_ != other.propertyMin_ ||
        propertyMax_ != other.propertyMax_ || entrance_ != other.entrance_ || heights_ != other.heights_ ||
        fencesHorizontal_ != other.fencesHorizontal_ || fencesVertical_ != other.fencesVertical_)
        return false;
    for (std::size_t i = 0; i < tiles_.size(); ++i) {
        const Tile& a = tiles_[i];
        const Tile& b = other.tiles_[i];
        if (a.terrain != b.terrain || a.path != b.path || a.building != b.building ||
            a.waterLevel != b.waterLevel)
            return false;
    }
    return true;
}

}  // namespace zoo
