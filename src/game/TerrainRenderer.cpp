// SPDX-License-Identifier: MIT
#include "TerrainRenderer.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <thread>

#include "CnaEngine/Core/Noise.hpp"
#include "CnaEngine/Graphics/VertexLayouts.hpp"
#include "Microsoft/Xna/Framework/Graphics/SamplerState.hpp"

#include "GroundPaint.hpp"

namespace zoogame {

namespace {

namespace Noise = CnaEngine::Noise;
using CnaEngine::Graphics::GpuMesh;
using CnaEngine::Graphics::Image;
using CnaEngine::Graphics::MeshData;
using CnaEngine::Graphics::VertexLayout;
using Microsoft::Xna::Framework::Graphics::SamplerState;
using zoo::kChunkTiles;
using zoo::kTileSize;
using zoo::TileCoord;
using zoo::World;

constexpr int kTexels = kChunkTiles * TerrainRenderer::kTexelsPerTile;

/// Paints one tile of a chunk's image.
void PaintTile(const World& world, TileCoord tile, TileCoord chunkOrigin, Image& image) {
    constexpr int n = TerrainRenderer::kTexelsPerTile;
    const int baseX = (tile.x - chunkOrigin.x) * n;
    const int baseY = (tile.z - chunkOrigin.z) * n;
    auto& bytes = image.Bytes();
    if (!world.InBounds(tile)) {
        // Beyond the map's edge (a map whose size is not a multiple of a chunk).
        for (int j = 0; j < n; ++j) {
            for (int i = 0; i < n; ++i) {
                const std::size_t at =
                    (static_cast<std::size_t>(baseY + j) * kTexels + static_cast<std::size_t>(baseX + i)) *
                    4u;
                bytes[at] = bytes[at + 1] = bytes[at + 2] = 0;
                bytes[at + 3] = 255;
            }
        }
        return;
    }
    const TilePaintInfo info = PaintInfo(world, tile);
    const float step = kTileSize / static_cast<float>(n);
    for (int j = 0; j < n; ++j) {
        const float z = static_cast<float>(tile.z) * kTileSize + (static_cast<float>(j) + 0.5f) * step;
        for (int i = 0; i < n; ++i) {
            const float x = static_cast<float>(tile.x) * kTileSize + (static_cast<float>(i) + 0.5f) * step;
            const zoo::Color3 color = GroundColor(world, info, x, z);
            const std::size_t at =
                (static_cast<std::size_t>(baseY + j) * kTexels + static_cast<std::size_t>(baseX + i)) * 4u;
            bytes[at] = SrgbByte(color[0]);
            bytes[at + 1] = SrgbByte(color[1]);
            bytes[at + 2] = SrgbByte(color[2]);
            bytes[at + 3] = 255;
        }
    }
}

/// Smooth normals from the triangles (the engine's front faces are clockwise).
void ComputeNormals(MeshData& mesh) {
    mesh.normals.assign(mesh.positions.size(), Vector3::Zero);
    for (std::size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        const std::uint32_t a = mesh.indices[i];
        const std::uint32_t b = mesh.indices[i + 1];
        const std::uint32_t c = mesh.indices[i + 2];
        const Vector3 normal =
            Vector3::Cross(mesh.positions[c] - mesh.positions[a], mesh.positions[b] - mesh.positions[a]);
        mesh.normals[a] = mesh.normals[a] + normal;
        mesh.normals[b] = mesh.normals[b] + normal;
        mesh.normals[c] = mesh.normals[c] + normal;
    }
    for (Vector3& normal : mesh.normals) {
        const float length = normal.Length();
        normal = length > 1.0e-6f ? normal * (1.0f / length) : Vector3(0.0f, 1.0f, 0.0f);
    }
}

/// Adds a triangle so that it faces up.
void AddUpTriangle(MeshData& mesh, std::uint32_t a, std::uint32_t b, std::uint32_t c) {
    const Vector3 normal =
        Vector3::Cross(mesh.positions[c] - mesh.positions[a], mesh.positions[b] - mesh.positions[a]);
    if (normal.Y < 0.0f)
        std::swap(b, c);
    mesh.indices.push_back(a);
    mesh.indices.push_back(b);
    mesh.indices.push_back(c);
}

}  // namespace

struct TerrainRenderer::Chunk {
    TileCoord origin;
    GpuMesh ground;
    GpuMesh water;
    bool hasWater = false;
    Image image;
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> texture;
    CnaEngine::Graphics::Material material;
    std::vector<std::uint32_t> signatures;
    std::uint32_t revision = 0;
};

TerrainRenderer::TerrainRenderer(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device,
                                 Surfaces& surfaces)
    : device_(device), surfaces_(surfaces) {}

TerrainRenderer::~TerrainRenderer() = default;

void TerrainRenderer::Build(const zoo::World& world) {
    chunksX_ = world.ChunksX();
    chunksZ_ = world.ChunksZ();
    chunks_.clear();
    for (int cz = 0; cz < chunksZ_; ++cz) {
        for (int cx = 0; cx < chunksX_; ++cx) {
            auto chunk = std::make_unique<Chunk>();
            chunk->origin = {cx * kChunkTiles, cz * kChunkTiles};
            chunk->image = Image(kTexels, kTexels);
            chunk->signatures.resize(static_cast<std::size_t>(kChunkTiles * kChunkTiles));
            chunk->revision = world.ChunkRevision(cx, cz);
            chunks_.push_back(std::move(chunk));
        }
    }

    // Painting only reads the world, so the chunks can be painted side by side.
    const auto started = std::chrono::steady_clock::now();
    std::atomic<std::size_t> next{0};
    const auto work = [&] {
        for (std::size_t index = next.fetch_add(1); index < chunks_.size(); index = next.fetch_add(1)) {
            Chunk& chunk = *chunks_[index];
            for (int tz = 0; tz < kChunkTiles; ++tz) {
                for (int tx = 0; tx < kChunkTiles; ++tx) {
                    const TileCoord tile{chunk.origin.x + tx, chunk.origin.z + tz};
                    PaintTile(world, tile, chunk.origin, chunk.image);
                    chunk.signatures[static_cast<std::size_t>(tz * kChunkTiles + tx)] =
                        world.InBounds(tile) ? PaintSignature(world, tile) : 0u;
                }
            }
        }
    };
    {
        const unsigned count = std::clamp(std::thread::hardware_concurrency(), 1u, 6u);
        std::vector<std::jthread> threads;
        for (unsigned i = 1; i < count; ++i)
            threads.emplace_back(work);
        work();
    }
    paintSeconds_ = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();

    for (auto& chunk : chunks_) {
        chunk->texture = CnaEngine::Graphics::UploadTexture(device_, chunk->image, {.srgb = true});
        chunk->material.name = "terrain";
        chunk->material.shading = CnaEngine::Graphics::ShadingModel::PhysicallyBased;
        chunk->material.diffuseColor = Vector3(1.0f, 1.0f, 1.0f);
        chunk->material.roughness = 0.95f;
        chunk->material.baseTexture = chunk->texture.get();
        chunk->material.sampler = &SamplerState::AnisotropicClamp;
        BuildMesh(world, *chunk);
    }
    BuildApron(world);
}

void TerrainRenderer::Update(const zoo::World& world, int maxChunks) {
    if (world.ChunksX() != chunksX_ || world.ChunksZ() != chunksZ_) {
        Build(world);
        return;
    }
    int done = 0;
    for (int cz = 0; cz < chunksZ_ && done < maxChunks; ++cz) {
        for (int cx = 0; cx < chunksX_ && done < maxChunks; ++cx) {
            Chunk& chunk = *chunks_[static_cast<std::size_t>(cz * chunksX_ + cx)];
            const std::uint32_t revision = world.ChunkRevision(cx, cz);
            if (chunk.revision == revision)
                continue;
            chunk.revision = revision;
            ++done;
            bool repainted = false;
            for (int tz = 0; tz < kChunkTiles; ++tz) {
                for (int tx = 0; tx < kChunkTiles; ++tx) {
                    const TileCoord tile{chunk.origin.x + tx, chunk.origin.z + tz};
                    if (!world.InBounds(tile))
                        continue;
                    std::uint32_t& signature =
                        chunk.signatures[static_cast<std::size_t>(tz * kChunkTiles + tx)];
                    const std::uint32_t now = PaintSignature(world, tile);
                    if (now == signature)
                        continue;
                    signature = now;
                    PaintTile(world, tile, chunk.origin, chunk.image);
                    repainted = true;
                }
            }
            if (repainted)
                CnaEngine::Graphics::UpdateTexture(*chunk.texture, chunk.image, {.srgb = true});
            BuildMesh(world, chunk);
        }
    }
}

void TerrainRenderer::BuildMesh(const zoo::World& world, Chunk& chunk) {
    const int tilesX = std::min(kChunkTiles, world.Width() - chunk.origin.x);
    const int tilesZ = std::min(kChunkTiles, world.Depth() - chunk.origin.z);

    MeshData ground;
    const std::size_t vertexCount = static_cast<std::size_t>((tilesX + 1) * (tilesZ + 1));
    ground.positions.reserve(vertexCount);
    ground.normals.reserve(vertexCount);
    ground.texCoords0.reserve(vertexCount);
    for (int j = 0; j <= tilesZ; ++j) {
        for (int i = 0; i <= tilesX; ++i) {
            const int vx = chunk.origin.x + i;
            const int vz = chunk.origin.z + j;
            ground.positions.emplace_back(static_cast<float>(vx) * kTileSize, world.VertexHeight(vx, vz),
                                          static_cast<float>(vz) * kTileSize);
            // Central differences over the neighbouring vertices: the same on
            // both sides of a chunk border, so the lighting has no seams.
            const float dx =
                (world.VertexHeight(vx + 1, vz) - world.VertexHeight(vx - 1, vz)) / (2.0f * kTileSize);
            const float dz =
                (world.VertexHeight(vx, vz + 1) - world.VertexHeight(vx, vz - 1)) / (2.0f * kTileSize);
            ground.normals.push_back(Vector3::Normalize(Vector3(-dx, 1.0f, -dz)));
            ground.texCoords0.emplace_back(static_cast<float>(i) / static_cast<float>(kChunkTiles),
                                           static_cast<float>(j) / static_cast<float>(kChunkTiles));
        }
    }
    const auto vertex = [tilesX](int i, int j) { return static_cast<std::uint32_t>(j * (tilesX + 1) + i); };
    ground.indices.reserve(static_cast<std::size_t>(tilesX * tilesZ * 6));
    for (int j = 0; j < tilesZ; ++j) {
        for (int i = 0; i < tilesX; ++i) {
            // Split along the diagonal from (x, z) to (x + 1, z + 1), as World::HeightAt does.
            ground.indices.insert(ground.indices.end(),
                                  {vertex(i, j), vertex(i + 1, j + 1), vertex(i, j + 1), vertex(i, j),
                                   vertex(i + 1, j), vertex(i + 1, j + 1)});
        }
    }
    chunk.ground = GpuMesh::Upload(device_, ground, VertexLayout::PositionNormalTexture());

    // Water: a flat sheet over every water tile and its neighbours (the banks
    // slope down inside the neighbouring tiles; the ground hides the rest).
    MeshData water;
    for (int j = 0; j < tilesZ; ++j) {
        for (int i = 0; i < tilesX; ++i) {
            const TileCoord tile{chunk.origin.x + i, chunk.origin.z + j};
            const float level = PaintInfo(world, tile).waterLevel;
            if (level == zoo::kNoWater)
                continue;
            const float x0 = static_cast<float>(tile.x) * kTileSize;
            const float z0 = static_cast<float>(tile.z) * kTileSize;
            const auto base = static_cast<std::uint32_t>(water.positions.size());
            for (int corner = 0; corner < 4; ++corner) {
                const float x = x0 + ((corner & 1) != 0 ? kTileSize : 0.0f);
                const float z = z0 + ((corner & 2) != 0 ? kTileSize : 0.0f);
                water.positions.emplace_back(x, level, z);
                water.normals.emplace_back(0.0f, 1.0f, 0.0f);
                water.texCoords0.emplace_back(x, z);
            }
            water.indices.insert(water.indices.end(), {base, base + 3, base + 2, base, base + 1, base + 3});
        }
    }
    chunk.hasWater = !water.indices.empty();
    chunk.water =
        chunk.hasWater ? GpuMesh::Upload(device_, water, VertexLayout::PositionNormalTexture()) : GpuMesh();
}

void TerrainRenderer::BuildApron(const zoo::World& world) {
    // One repeating grass texture for all the land beyond the map.
    constexpr int kSize = 512;
    Image image(kSize, kSize);
    for (int j = 0; j < kSize; ++j) {
        for (int i = 0; i < kSize; ++i) {
            const zoo::Color3 color = GrassColor((static_cast<float>(i) + 0.5f) * kGrassPeriodM / kSize,
                                                 (static_cast<float>(j) + 0.5f) * kGrassPeriodM / kSize);
            image.Set(i, j, SrgbColor(color));
        }
    }
    apronTexture_ = CnaEngine::Graphics::UploadTexture(device_, image, {.srgb = true});

    // Crops: tints of the one texture. The first is the meadow around the zoo.
    const Vector3 kTints[] = {
        {1.0f, 1.0f, 1.0f},    // meadow
        {0.78f, 0.9f, 0.85f},  // pasture
        {2.3f, 1.25f, 0.9f},   // ripening grain
        {1.75f, 0.8f, 0.85f},  // ploughed
        {0.45f, 0.58f, 0.5f},  // woodland floor
        {1.35f, 1.2f, 0.8f},   // hay meadow
    };
    const int kCrops = static_cast<int>(std::size(kTints));
    land_.clear();
    for (const Vector3& tint : kTints) {
        auto land = std::make_unique<Land>();
        land->material.name = "land";
        land->material.shading = CnaEngine::Graphics::ShadingModel::PhysicallyBased;
        land->material.diffuseColor = tint;
        land->material.roughness = 0.95f;
        land->material.baseTexture = apronTexture_.get();
        land->material.sampler = &SamplerState::AnisotropicWrap;
        land_.push_back(std::move(land));
    }

    // The map's outline, vertex by vertex, each with the direction away from the map.
    struct Rim {
        int vx, vz;
        float dx, dz;
    };
    std::vector<Rim> rim;
    const int w = world.Width();
    const int d = world.Depth();
    for (int x = 0; x < w; ++x)
        rim.push_back({x, 0, x == 0 ? -1.0f : 0.0f, -1.0f});
    for (int z = 0; z < d; ++z)
        rim.push_back({w, z, 1.0f, z == 0 ? -1.0f : 0.0f});
    for (int x = w; x > 0; --x)
        rim.push_back({x, d, x == w ? 1.0f : 0.0f, 1.0f});
    for (int z = d; z > 0; --z)
        rim.push_back({0, z, -1.0f, z == d ? 1.0f : 0.0f});

    float base = 0.0f;
    for (const Rim& r : rim)
        base += world.VertexHeight(r.vx, r.vz);
    base /= static_cast<float>(rim.size());

    // The skirt: rings around the rim whose heights ease to the level `base`.
    constexpr float kSkirt = 16.0f;
    constexpr float kRings[] = {0.0f, 2.0f, 5.0f, 10.0f, kSkirt};
    constexpr int kRingCount = static_cast<int>(std::size(kRings));
    MeshData skirt;
    for (const float distance : kRings) {
        for (const Rim& r : rim) {
            const float x = static_cast<float>(r.vx) * kTileSize + r.dx * distance;
            const float z = static_cast<float>(r.vz) * kTileSize + r.dz * distance;
            const float edge = world.VertexHeight(r.vx, r.vz);
            skirt.positions.emplace_back(x, edge + (base - edge) * Noise::SmoothStep(0.0f, kSkirt, distance),
                                         z);
            skirt.texCoords0.emplace_back(x / kGrassPeriodM, z / kGrassPeriodM);
        }
    }
    const auto count = static_cast<std::uint32_t>(rim.size());
    for (std::uint32_t ring = 0; ring + 1 < static_cast<std::uint32_t>(kRingCount); ++ring) {
        for (std::uint32_t i = 0; i < count; ++i) {
            const std::uint32_t a = ring * count + i;
            const std::uint32_t b = ring * count + (i + 1) % count;
            AddUpTriangle(skirt, a, b, b + count);
            AddUpTriangle(skirt, a, b + count, a + count);
        }
    }
    ComputeNormals(skirt);
    // Along the rim the light must fall as it does on the map's own edge.
    for (std::size_t i = 0; i < rim.size(); ++i) {
        const Rim& r = rim[i];
        const float dx =
            (world.VertexHeight(r.vx + 1, r.vz) - world.VertexHeight(r.vx - 1, r.vz)) / (2.0f * kTileSize);
        const float dz =
            (world.VertexHeight(r.vx, r.vz + 1) - world.VertexHeight(r.vx, r.vz - 1)) / (2.0f * kTileSize);
        skirt.normals[i] = Vector3::Normalize(Vector3(-dx, 1.0f, -dz));
    }
    auto skirtLand = std::make_unique<Land>();
    skirtLand->material = land_.front()->material;
    skirtLand->mesh = GpuMesh::Upload(device_, skirt, VertexLayout::PositionNormalTexture(),
                                      {.preferSixteenBitIndices = false});

    // Beyond the skirt: a grid of fields over rolling hills, out to the horizon.
    const float minX = -kSkirt;
    const float maxX = world.SizeX() + kSkirt;
    const float minZ = -kSkirt;
    const float maxZ = world.SizeZ() + kSkirt;
    const auto axis = [](float from, float to) {
        std::vector<float> lines;
        constexpr float kCell = 32.0f;
        const int inside = std::max(1, static_cast<int>(std::lround((to - from) / kCell)));
        std::vector<float> outward;
        float offset = 0.0f;
        while (offset < 1500.0f) {
            offset += offset < 640.0f ? kCell : 4.0f * kCell;
            outward.push_back(offset);
        }
        for (auto it = outward.rbegin(); it != outward.rend(); ++it)
            lines.push_back(from - *it);
        for (int i = 0; i <= inside; ++i)
            lines.push_back(from + (to - from) * static_cast<float>(i) / static_cast<float>(inside));
        for (const float o : outward)
            lines.push_back(to + o);
        return lines;
    };
    const std::vector<float> xs = axis(minX, maxX);
    const std::vector<float> zs = axis(minZ, maxZ);
    const auto outside = [&](float x, float z) {
        const float ox = std::max(std::max(minX - x, x - maxX), 0.0f);
        const float oz = std::max(std::max(minZ - z, z - maxZ), 0.0f);
        return std::sqrt(ox * ox + oz * oz);
    };
    MeshData grid;
    for (const float z : zs) {
        for (const float x : xs) {
            const float distance = outside(x, z);
            const float hills =
                (Noise::Fbm(x / 260.0f + 31.7f, z / 260.0f + 11.3f, 0, 4, 2.0f, 0.5f, 77) - 0.42f) * 80.0f *
                Noise::SmoothStep(20.0f, 420.0f, distance);
            grid.positions.emplace_back(x, base + hills, z);
            grid.texCoords0.emplace_back(x / kGrassPeriodM, z / kGrassPeriodM);
        }
    }
    const auto columns = static_cast<std::uint32_t>(xs.size());
    std::vector<std::vector<std::uint32_t>> cropIndices(static_cast<std::size_t>(kCrops));
    for (std::uint32_t j = 0; j + 1 < zs.size(); ++j) {
        for (std::uint32_t i = 0; i + 1 < columns; ++i) {
            const float cx = (xs[i] + xs[i + 1]) * 0.5f;
            const float cz = (zs[j] + zs[j + 1]) * 0.5f;
            if (cx > minX && cx < maxX && cz > minZ && cz < maxZ)
                continue;  // the map and its skirt
            // Fields are blocks of 128 m x 192 m, every row shifted.
            int crop = 0;
            if (outside(cx, cz) > 50.0f) {
                const int row = static_cast<int>(std::floor(cz / 192.0f));
                const float shift = Noise::Random2(row, 3, 601) * 128.0f;
                const int column = static_cast<int>(std::floor((cx + shift) / 128.0f));
                const float pick = Noise::Random2(column, row, 602);
                crop = pick < 0.34f   ? 4
                       : pick < 0.56f ? 1
                       : pick < 0.7f  ? 0
                       : pick < 0.82f ? 2
                       : pick < 0.9f  ? 3
                                      : 5;
            }
            const std::uint32_t a = j * columns + i;
            const std::size_t before = grid.indices.size();
            AddUpTriangle(grid, a, a + 1, a + columns + 1);
            AddUpTriangle(grid, a, a + columns + 1, a + columns);
            auto& indices = cropIndices[static_cast<std::size_t>(crop)];
            indices.insert(indices.end(), grid.indices.begin() + static_cast<std::ptrdiff_t>(before),
                           grid.indices.end());
        }
    }
    ComputeNormals(grid);
    for (int crop = 0; crop < kCrops; ++crop) {
        MeshData part = grid;
        part.indices = cropIndices[static_cast<std::size_t>(crop)];
        if (!part.indices.empty())
            land_[static_cast<std::size_t>(crop)]->mesh = GpuMesh::Upload(
                device_, part, VertexLayout::PositionNormalTexture(), {.preferSixteenBitIndices = false});
    }
    std::erase_if(land_, [&](const std::unique_ptr<Land>& land) { return land->mesh.Vertices() == nullptr; });
    land_.push_back(std::move(skirtLand));
}

void TerrainRenderer::Submit(CnaEngine::App::Frame& frame) const {
    const Matrix identity = Matrix::getIdentityProperty();
    for (const auto& chunk : chunks_) {
        frame.Add(chunk->ground, chunk->material, identity);
        if (chunk->hasWater)
            frame.Add(chunk->water, surfaces_.Water(), identity);
    }
    for (const auto& land : land_)
        frame.Add(land->mesh, land->material, identity);
}

}  // namespace zoogame
