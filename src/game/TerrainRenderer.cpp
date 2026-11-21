// SPDX-License-Identifier: MIT
#include "TerrainRenderer.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <mutex>
#include <thread>

#if defined(__GLIBC__)
#include <malloc.h>
#endif

#include "CnaEngine/Core/Noise.hpp"
#include "CnaEngine/Graphics/VertexLayouts.hpp"
#include "Microsoft/Xna/Framework/BoundingBox.hpp"
#include "Microsoft/Xna/Framework/Graphics/SamplerState.hpp"
#include "Microsoft/Xna/Framework/Graphics/SurfaceFormat.hpp"
#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"

#include "GroundPaint.hpp"
#include "StaticTexture.hpp"

namespace zoogame {

namespace {

namespace Noise = CnaEngine::Noise;
using CnaEngine::Graphics::GpuMesh;
using CnaEngine::Graphics::Image;
using CnaEngine::Graphics::MeshData;
using CnaEngine::Graphics::VertexLayout;
using Microsoft::Xna::Framework::Graphics::SamplerState;
using Microsoft::Xna::Framework::Graphics::Texture2D;
using zoo::kChunkTiles;
using zoo::kTileSize;
using zoo::TileCoord;
using zoo::World;

constexpr int kTexels = kChunkTiles * TerrainRenderer::kTexelsPerTile;

/// Tiles where kinds of ground meet, and bank tiles, are cut into this many
/// cells a side (25 cm): the grain of the detail layer's outlines and of a
/// bank's bends.
constexpr int kCells = kGroundCells;
constexpr int kCellPoints = kCells + 1;

/// The detail layer is never drawn farther away than this (a screen of very
/// many pixels would show it farther still).
constexpr float kDetailReachMaxM = 160.0f;

/// How far from the eye the detail layer shows anything: up to where a texel of
/// its first fully transparent mip level is as large as a pixel. Farther away
/// the sampler takes that level or a coarser one, so nothing is lost by not
/// drawing the layer there, and there is no visible end to it. (53 m in a
/// picture 720 pixels high.)
float DetailReach(const CnaEngine::Graphics::Camera& camera, int viewportHeight) {
    const float fieldOfView = camera.FieldOfView();
    if (fieldOfView <= 0.0f || viewportHeight <= 0)
        return kDetailReachMaxM;
    const float pixelAtOneMetre = 2.0f * std::tan(fieldOfView * 0.5f) / static_cast<float>(viewportHeight);
    const float texel = kGroundDetailPeriodM / static_cast<float>(kGroundDetailTexels) *
                        static_cast<float>(1 << kGroundDetailFadedLevel);
    return std::min(texel / pixelAtOneMetre * 1.1f, kDetailReachMaxM);
}

/// The detail layer lies in the ground's own plane: pulled towards the eye by a
/// few steps of the depth buffer, it wins against the ground and against
/// nothing that stands on it.
constexpr float kDetailDepthBias = -3.0e-6f;
constexpr float kDetailSlopeBias = -1.0f;

/// How much of what lies behind the water's surface shows through.
constexpr float kWaterAlpha = 0.62f;

/// sRGB bytes of linear values in steps fine enough to match SrgbByte: painting
/// converts three channels of some twenty-six million texels.
constexpr int kSrgbSteps = 1 << 14;
const std::array<std::uint8_t, kSrgbSteps + 1>& SrgbBytes() {
    static const auto table = [] {
        std::array<std::uint8_t, kSrgbSteps + 1> bytes{};
        for (int i = 0; i <= kSrgbSteps; ++i)
            bytes[static_cast<std::size_t>(i)] =
                SrgbByte(static_cast<float>(i) / static_cast<float>(kSrgbSteps));
        return bytes;
    }();
    return table;
}

/// PaintInfo of a chunk's tiles and of one more tile all around.
class ChunkInfos {
public:
    ChunkInfos(const World& world, TileCoord origin) : world_(world), origin_(origin) {
        infos_.reserve(static_cast<std::size_t>(kSide * kSide));
        for (int dz = -1; dz <= kChunkTiles; ++dz) {
            for (int dx = -1; dx <= kChunkTiles; ++dx)
                infos_.push_back(PaintInfo(world, Inside({origin.x + dx, origin.z + dz})));
        }
    }

    const World& Map() const { return world_; }
    TileCoord Origin() const { return origin_; }

    /// Of the nearest tile of the map that this chunk knows.
    const TilePaintInfo& At(TileCoord tile) const {
        tile = Inside(tile);
        const int dx = std::clamp(tile.x - origin_.x + 1, 0, kSide - 1);
        const int dz = std::clamp(tile.z - origin_.z + 1, 0, kSide - 1);
        return infos_[static_cast<std::size_t>(dz * kSide + dx)];
    }

    /// Height of the drawn ground (GroundHeight).
    float Height(float x, float z) const {
        return GroundHeight(
            At({static_cast<int>(std::floor(x / kTileSize)), static_cast<int>(std::floor(z / kTileSize))}), x,
            z);
    }

private:
    static constexpr int kSide = kChunkTiles + 2;

    TileCoord Inside(TileCoord tile) const {
        return {std::clamp(tile.x, 0, world_.Width() - 1), std::clamp(tile.z, 0, world_.Depth() - 1)};
    }

    const World& world_;
    TileCoord origin_;
    std::vector<TilePaintInfo> infos_;
};

std::size_t TexelAt(TileCoord tile, TileCoord chunkOrigin, int i, int j) {
    constexpr int n = TerrainRenderer::kTexelsPerTile;
    return (static_cast<std::size_t>((tile.z - chunkOrigin.z) * n + j) * kTexels +
            static_cast<std::size_t>((tile.x - chunkOrigin.x) * n + i)) *
           4u;
}

/// Paints one tile of a chunk's image.
void PaintTile(const TilePaintInfo& info, TileCoord chunkOrigin, Image& image) {
    constexpr int n = TerrainRenderer::kTexelsPerTile;
    const auto& srgb = SrgbBytes();
    const auto byte = [&srgb](float linear) {
        return srgb[static_cast<std::size_t>(std::clamp(linear, 0.0f, 1.0f) * static_cast<float>(kSrgbSteps) +
                                             0.5f)];
    };
    auto& bytes = image.Bytes();
    const float step = kTileSize / static_cast<float>(n);
    for (int j = 0; j < n; ++j) {
        const float z = static_cast<float>(info.tile.z) * kTileSize + (static_cast<float>(j) + 0.5f) * step;
        std::size_t at = TexelAt(info.tile, chunkOrigin, 0, j);
        for (int i = 0; i < n; ++i, at += 4) {
            const float x =
                static_cast<float>(info.tile.x) * kTileSize + (static_cast<float>(i) + 0.5f) * step;
            const zoo::Color3 color = SampleGround(info, x, z).color;
            bytes[at] = byte(color[0]);
            bytes[at + 1] = byte(color[1]);
            bytes[at + 2] = byte(color[2]);
            bytes[at + 3] = 255;
        }
    }
}

/// A tile beyond the map's edge (a map whose size is not a multiple of a chunk).
void BlankTile(TileCoord tile, TileCoord chunkOrigin, Image& image) {
    constexpr int n = TerrainRenderer::kTexelsPerTile;
    auto& bytes = image.Bytes();
    for (int j = 0; j < n; ++j) {
        std::size_t at = TexelAt(tile, chunkOrigin, 0, j);
        for (int i = 0; i < n; ++i, at += 4) {
            bytes[at] = bytes[at + 1] = bytes[at + 2] = 0;
            bytes[at + 3] = 255;
        }
    }
}

/// The mip levels below the image itself, largest first.
std::vector<Image> LowerLevels(const Image& image) {
    std::vector<Image> levels;
    const Image* last = &image;
    while (last->Width() > 1 || last->Height() > 1) {
        levels.push_back(CnaEngine::Graphics::Downsample(*last, {.srgb = true, .alphaTestReference = {}}));
        last = &levels.back();
    }
    return levels;
}

void SetLevel(Texture2D& texture, int level, const Image& image) {
    const auto& bytes = image.Bytes();
    texture.SetData(level, nullptr, bytes.data(), 0, static_cast<int>(bytes.size()));
}

/// The normal of the drawn height field at a vertex: central differences over the
/// neighbouring vertices, the same on both sides of a chunk border, so the
/// lighting has no seams.
Vector3 GridNormal(const World& world, int vx, int vz) {
    const float dx =
        (DrawnVertexHeight(world, vx + 1, vz) - DrawnVertexHeight(world, vx - 1, vz)) / (2.0f * kTileSize);
    const float dz =
        (DrawnVertexHeight(world, vx, vz + 1) - DrawnVertexHeight(world, vx, vz - 1)) / (2.0f * kTileSize);
    return Vector3::Normalize(Vector3(-dx, 1.0f, -dz));
}

/// Normal on a straight tile side, shared by a bank tile and its uncut neighbour.
Vector3 SideNormal(const World& world, TileCoord tile, int i, int j) {
    if ((i == 0 || i == kCells) && (j == 0 || j == kCells))
        return GridNormal(world, tile.x + i / kCells, tile.z + j / kCells);
    if (j == 0 || j == kCells) {
        const int vz = tile.z + j / kCells;
        const Vector3 a = GridNormal(world, tile.x, vz);
        return a +
               (GridNormal(world, tile.x + 1, vz) - a) * (static_cast<float>(i) / static_cast<float>(kCells));
    }
    const int vx = tile.x + i / kCells;
    const Vector3 a = GridNormal(world, vx, tile.z);
    return a + (GridNormal(world, vx, tile.z + 1) - a) * (static_cast<float>(j) / static_cast<float>(kCells));
}

/// A bank tile keeps every lattice point. Its uncut neighbours split their shared
/// sides instead of stretching these cells into fans with the wrong detail.
struct BankPatch {
    std::array<Vector3, kCellPoints * kCellPoints> position;
    std::array<Vector3, kCellPoints * kCellPoints> normal;

    static std::size_t At(int i, int j) { return static_cast<std::size_t>(j * kCellPoints + i); }
};

BankPatch CutBankTile(const ChunkInfos& infos, const TilePaintInfo& info) {
    const World& world = infos.Map();
    const TileCoord tile = info.tile;
    constexpr float kCell = kTileSize / static_cast<float>(kCells);
    BankPatch patch;
    for (int j = 0; j <= kCells; ++j) {
        for (int i = 0; i <= kCells; ++i) {
            const float x = static_cast<float>(tile.x) * kTileSize + static_cast<float>(i) * kCell;
            const float z = static_cast<float>(tile.z) * kTileSize + static_cast<float>(j) * kCell;
            const std::size_t at = BankPatch::At(i, j);
            patch.position[at] = Vector3(x, GroundHeight(info, x, z), z);
            if ((i == 0 || i == kCells) && (j == 0 || j == kCells)) {
                patch.normal[at] = GridNormal(world, tile.x + i / kCells, tile.z + j / kCells);
            } else if ((j == 0 && (tile.z == 0 || !infos.At({tile.x, tile.z - 1}).bank)) ||
                       (j == kCells &&
                        (tile.z == world.Depth() - 1 || !infos.At({tile.x, tile.z + 1}).bank)) ||
                       (i == 0 && (tile.x == 0 || !infos.At({tile.x - 1, tile.z}).bank)) ||
                       (i == kCells &&
                        (tile.x == world.Width() - 1 || !infos.At({tile.x + 1, tile.z}).bank))) {
                patch.normal[at] = SideNormal(world, tile, i, j);
            } else {
                const float dx =
                    (infos.Height(x + kCell * 0.5f, z) - infos.Height(x - kCell * 0.5f, z)) / kCell;
                const float dz =
                    (infos.Height(x, z + kCell * 0.5f) - infos.Height(x, z - kCell * 0.5f)) / kCell;
                patch.normal[at] = Vector3::Normalize(Vector3(-dx, 1.0f, -dz));
            }
        }
    }
    return patch;
}

/// Which detail lies over each of a tile's cells; true when it is the same all over.
using CellDetails = std::array<GroundDetail, kCells * kCells>;
bool ClassifyCells(const TilePaintInfo& info, CellDetails& cells) {
    const float x0 = static_cast<float>(info.tile.x) * kTileSize;
    const float z0 = static_cast<float>(info.tile.z) * kTileSize;
    if (info.plain) {
        cells.fill(SampleGround(info, x0 + kTileSize * 0.5f, z0 + kTileSize * 0.5f).detail);
        return true;
    }
    constexpr float kCell = kTileSize / static_cast<float>(kCells);
    bool same = true;
    for (int j = 0; j < kCells; ++j) {
        for (int i = 0; i < kCells; ++i) {
            const GroundDetail detail = SampleGround(info, x0 + (static_cast<float>(i) + 0.5f) * kCell,
                                                     z0 + (static_cast<float>(j) + 0.5f) * kCell)
                                            .detail;
            cells[static_cast<std::size_t>(j * kCells + i)] = detail;
            same = same && detail == cells[0];
        }
    }
    if (same)
        return true;
    // Joints are drawn only in cells that lie wholly in the slabs: a joint must
    // not run out over the border course or the lawn.
    for (int j = 0; j < kCells; ++j) {
        for (int i = 0; i < kCells; ++i) {
            GroundDetail& detail = cells[static_cast<std::size_t>(j * kCells + i)];
            if (detail != GroundDetail::Paving)
                continue;
            for (int corner = 0; corner < 4 && detail == GroundDetail::Paving; ++corner) {
                const float x = x0 + static_cast<float>(i + (corner & 1)) * kCell;
                const float z = z0 + static_cast<float>(j + (corner >> 1)) * kCell;
                if (SampleGround(info, x, z).detail != GroundDetail::Paving)
                    detail = GroundDetail::Soil;
            }
        }
    }
    return false;
}

/// What a chunk draws, before it goes to the GPU.
struct ChunkMeshes {
    MeshData ground;
    MeshData water;
    std::array<MeshData, kGroundDetailCount> detail;
};

/// A mesh cut from pieces that share their points: a point of the chunk's
/// lattice (kCells to a tile) becomes one vertex however many pieces meet in it.
/// Texture coordinates follow from the position.
class WeldedMesh {
public:
    WeldedMesh(MeshData& mesh, float uvScale, float uvOriginX, float uvOriginZ)
        : mesh_(mesh), uvScale_(uvScale), uvOriginX_(uvOriginX), uvOriginZ_(uvOriginZ) {}

    /// The vertex at a lattice point; `make(position, normal)` is asked for it when it is new.
    template <typename Make>
    std::uint32_t Vertex(int li, int lj, Make&& make) {
        if (slots_.empty())
            slots_.assign(static_cast<std::size_t>(kLattice * kLattice), kNone);
        std::uint32_t& slot = slots_[static_cast<std::size_t>(lj * kLattice + li)];
        if (slot == kNone) {
            Vector3 position;
            Vector3 normal;
            make(position, normal);
            slot = static_cast<std::uint32_t>(mesh_.positions.size());
            mesh_.positions.push_back(position);
            mesh_.normals.push_back(normal);
            mesh_.texCoords0.emplace_back((position.X - uvOriginX_) * uvScale_,
                                          (position.Z - uvOriginZ_) * uvScale_);
        }
        return slot;
    }

    void Triangle(std::uint32_t a, std::uint32_t b, std::uint32_t c) {
        mesh_.indices.insert(mesh_.indices.end(), {a, b, c});
    }

    /// A four-cornered piece (corners in the order of a tile's: index dz * 2 + dx),
    /// split along the diagonal from corner 0 to corner 3 as the tiles are
    /// (World::HeightAt). Triangles folded to a line are left out.
    void Piece(const std::array<std::uint32_t, 4>& corner) {
        if (corner[0] == corner[3])
            return;
        if (corner[2] != corner[0] && corner[2] != corner[3])
            mesh_.indices.insert(mesh_.indices.end(), {corner[0], corner[3], corner[2]});
        if (corner[1] != corner[0] && corner[1] != corner[3])
            mesh_.indices.insert(mesh_.indices.end(), {corner[0], corner[1], corner[3]});
    }

private:
    static constexpr int kLattice = kChunkTiles * kCells + 1;
    static constexpr std::uint32_t kNone = 0xffffffffu;

    MeshData& mesh_;
    float uvScale_;
    float uvOriginX_;
    float uvOriginZ_;
    std::vector<std::uint32_t> slots_;
};

/// A chunk's meshes while they are cut.
struct ChunkCut {
    ChunkCut(ChunkMeshes& meshes, TileCoord chunkOrigin)
        : origin(chunkOrigin),
          ground(meshes.ground, 1.0f / (static_cast<float>(kChunkTiles) * kTileSize),
                 static_cast<float>(chunkOrigin.x) * kTileSize,
                 static_cast<float>(chunkOrigin.z) * kTileSize),
          detail{Layer(meshes, 0), Layer(meshes, 1), Layer(meshes, 2), Layer(meshes, 3),
                 Layer(meshes, 4), Layer(meshes, 5), Layer(meshes, 6), Layer(meshes, 7)} {}

    static WeldedMesh Layer(ChunkMeshes& meshes, std::size_t kind) {
        return WeldedMesh(meshes.detail[kind], 1.0f / kGroundDetailPeriodM, 0.0f, 0.0f);
    }

    TileCoord origin;
    WeldedMesh ground;
    std::array<WeldedMesh, kGroundDetailCount> detail;
};

/// The detail layer over a tile that is not a bank tile: whole where one detail
/// covers it, else in rows of cells, runs of the same detail joined.
void AddTileDetail(const ChunkInfos& infos, const TilePaintInfo& info, const CellDetails& cells, bool same,
                   ChunkCut& cut) {
    const World& world = infos.Map();
    const TileCoord tile = info.tile;
    const int latticeI = (tile.x - cut.origin.x) * kCells;
    const int latticeJ = (tile.z - cut.origin.z) * kCells;
    std::array<Vector3, 4> cornerNormal;
    for (int corner = 0; corner < 4; ++corner)
        cornerNormal[static_cast<std::size_t>(corner)] =
            GridNormal(world, tile.x + (corner & 1), tile.z + (corner >> 1));
    const auto vertex = [&](WeldedMesh& mesh, int i, int j) {
        // A point of the tile's surface: the two triangles carry heights and normals alike.
        return mesh.Vertex(latticeI + i, latticeJ + j, [&](Vector3& position, Vector3& normal) {
            const float u = static_cast<float>(i) / static_cast<float>(kCells);
            const float v = static_cast<float>(j) / static_cast<float>(kCells);
            const float x = (static_cast<float>(tile.x) + u) * kTileSize;
            const float z = (static_cast<float>(tile.z) + v) * kTileSize;
            position = Vector3(x, GroundHeight(info, x, z), z);
            const auto& n = cornerNormal;
            if (i == 0 || i == kCells || j == 0 || j == kCells)
                normal = SideNormal(world, tile, i, j);
            else
                normal = v <= u ? n[0] + (n[1] - n[0]) * u + (n[3] - n[1]) * v
                                : n[0] + (n[3] - n[2]) * u + (n[2] - n[0]) * v;
        });
    };
    const auto piece = [&](GroundDetail kind, int i0, int j0, int i1, int j1) {
        if (kind == GroundDetail::None)
            return;
        WeldedMesh& mesh = cut.detail[static_cast<std::size_t>(kind)];
        mesh.Piece({vertex(mesh, i0, j0), vertex(mesh, i1, j0), vertex(mesh, i0, j1), vertex(mesh, i1, j1)});
    };
    if (same) {
        piece(cells[0], 0, 0, kCells, kCells);
        return;
    }
    for (int j = 0; j < kCells; ++j) {
        for (int i = 0; i < kCells;) {
            const int start = i;
            const GroundDetail kind = cells[static_cast<std::size_t>(j * kCells + i)];
            ++i;
            // The tile's crease runs through the cell on its diagonal: that one
            // stands alone, the runs on either side of it are flat.
            if (start != j) {
                while (i < kCells && i != j && cells[static_cast<std::size_t>(j * kCells + i)] == kind)
                    ++i;
            }
            piece(kind, start, j, i, j + 1);
        }
    }
}

/// A bank tile: its ground, cut into cells, and the detail layer over each cell.
void AddBankTile(const ChunkInfos& infos, const TilePaintInfo& info, const CellDetails& cells,
                  ChunkCut& cut) {
    const BankPatch patch = CutBankTile(infos, info);
    const int latticeI = (info.tile.x - cut.origin.x) * kCells;
    const int latticeJ = (info.tile.z - cut.origin.z) * kCells;
    const auto vertex = [&](WeldedMesh& mesh, int i, int j) {
        const std::size_t at = BankPatch::At(i, j);
        return mesh.Vertex(latticeI + i, latticeJ + j, [&](Vector3& position, Vector3& normal) {
            position = patch.position[at];
            normal = patch.normal[at];
        });
    };
    const auto piece = [&](WeldedMesh& mesh, int i, int j) {
        mesh.Piece(
            {vertex(mesh, i, j), vertex(mesh, i + 1, j), vertex(mesh, i, j + 1), vertex(mesh, i + 1, j + 1)});
    };
    for (int j = 0; j < kCells; ++j) {
        for (int i = 0; i < kCells; ++i) {
            piece(cut.ground, i, j);
            const GroundDetail kind = cells[static_cast<std::size_t>(j * kCells + i)];
            if (kind != GroundDetail::None)
                piece(cut.detail[static_cast<std::size_t>(kind)], i, j);
        }
    }
}

/// Split only the sides facing a bank tile. The diagonal midpoint is on both of
/// the original triangles, so each fan stays in its original plane. Neither
/// this tile nor an adjacent chunk leaves a T-junction on the shared side.
void AddBankNeighbour(const ChunkInfos& infos, const TilePaintInfo& info, ChunkCut& cut) {
    const TileCoord tile = info.tile;
    const World& world = infos.Map();
    const int li = (tile.x - cut.origin.x) * kCells;
    const int lj = (tile.z - cut.origin.z) * kCells;
    const auto vertex = [&](int i, int j) {
        return cut.ground.Vertex(li + i, lj + j, [&](Vector3& position, Vector3& normal) {
            const float x = static_cast<float>(tile.x) * kTileSize +
                            static_cast<float>(i) * kTileSize / static_cast<float>(kCells);
            const float z = static_cast<float>(tile.z) * kTileSize +
                            static_cast<float>(j) * kTileSize / static_cast<float>(kCells);
            position = Vector3(x, GroundHeight(info, x, z), z);
            normal =
                i == kCells / 2 && j == kCells / 2
                    ? (GridNormal(world, tile.x, tile.z) + GridNormal(world, tile.x + 1, tile.z + 1)) * 0.5f
                    : SideNormal(world, tile, i, j);
        });
    };
    const auto centre = vertex(kCells / 2, kCells / 2);
    const auto side = [&](int i0, int j0, int i1, int j1, TileCoord neighbour) {
        const int steps = world.InBounds(neighbour) && infos.At(neighbour).bank ? kCells : 1;
        for (int k = 0; k < steps; ++k) {
            const auto a = vertex(i0 + (i1 - i0) * k / steps, j0 + (j1 - j0) * k / steps);
            const auto b = vertex(i0 + (i1 - i0) * (k + 1) / steps, j0 + (j1 - j0) * (k + 1) / steps);
            cut.ground.Triangle(centre, a, b);
        }
    };
    side(0, 0, kCells, 0, {tile.x, tile.z - 1});
    side(kCells, 0, kCells, kCells, {tile.x + 1, tile.z});
    side(kCells, kCells, 0, kCells, {tile.x, tile.z + 1});
    side(0, kCells, 0, 0, {tile.x - 1, tile.z});
}

/// Everything a chunk draws. Only reads the world; safe on any thread.
ChunkMeshes CutMeshes(const ChunkInfos& infos) {
    const World& world = infos.Map();
    const TileCoord origin = infos.Origin();
    const int tilesX = std::min(kChunkTiles, world.Width() - origin.x);
    const int tilesZ = std::min(kChunkTiles, world.Depth() - origin.z);
    ChunkMeshes meshes;
    ChunkCut cut(meshes, origin);

    // The tiles' corners first: the ground's vertices as they are drawn (DrawnVertexHeight).
    const auto corner = [&](int i, int j) {
        return cut.ground.Vertex(i * kCells, j * kCells, [&](Vector3& position, Vector3& normal) {
            const int vx = origin.x + i;
            const int vz = origin.z + j;
            position = Vector3(static_cast<float>(vx) * kTileSize, DrawnVertexHeight(world, vx, vz),
                               static_cast<float>(vz) * kTileSize);
            normal = GridNormal(world, vx, vz);
        });
    };
    for (int j = 0; j <= tilesZ; ++j) {
        for (int i = 0; i <= tilesX; ++i)
            corner(i, j);
    }
    meshes.ground.indices.reserve(static_cast<std::size_t>(tilesX * tilesZ * 6));

    CellDetails cells;
    for (int j = 0; j < tilesZ; ++j) {
        for (int i = 0; i < tilesX; ++i) {
            const TileCoord tile{origin.x + i, origin.z + j};
            const TilePaintInfo& info = infos.At(tile);
            const bool same = ClassifyCells(info, cells);
            if (info.bank) {
                AddBankTile(infos, info, cells, cut);
            } else {
                if (infos.At({tile.x - 1, tile.z}).bank || infos.At({tile.x + 1, tile.z}).bank ||
                    infos.At({tile.x, tile.z - 1}).bank || infos.At({tile.x, tile.z + 1}).bank)
                    AddBankNeighbour(infos, info, cut);
                else
                    cut.ground.Piece(
                        {corner(i, j), corner(i + 1, j), corner(i, j + 1), corner(i + 1, j + 1)});
                AddTileDetail(infos, info, cells, same, cut);
            }

            // Water: a flat sheet over every water tile (its bank rises out of the
            // water inside it; the ground hides the sheet there).
            if (!world.HasWater(tile))
                continue;
            const float level = world.At(tile).waterLevel;
            MeshData& water = meshes.water;
            const float x0 = static_cast<float>(tile.x) * kTileSize;
            const float z0 = static_cast<float>(tile.z) * kTileSize;
            const auto base = static_cast<std::uint32_t>(water.positions.size());
            for (int at = 0; at < 4; ++at) {
                const float x = x0 + ((at & 1) != 0 ? kTileSize : 0.0f);
                const float z = z0 + ((at & 2) != 0 ? kTileSize : 0.0f);
                water.positions.emplace_back(x, level, z);
                water.normals.emplace_back(0.0f, 1.0f, 0.0f);
                water.tangents.emplace_back(1.0f, 0.0f, 0.0f, 1.0f);  // for the ripples' normal map
                water.texCoords0.emplace_back(x, z);
            }
            water.indices.insert(water.indices.end(), {base, base + 3, base + 2, base, base + 1, base + 3});
        }
    }
    return meshes;
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
    /// The detail layer's mesh for each GroundDetail, and whether there is one.
    std::array<GpuMesh, kGroundDetailCount> detail;
    std::array<bool, kGroundDetailCount> hasDetail{};
    Image image;
    std::unique_ptr<Texture2D> texture;
    CnaEngine::Graphics::Material material;
    std::vector<std::uint32_t> signatures;
    std::uint32_t revision = 0;
    /// A fence changed somewhere: the tiles' signatures are to be looked at again.
    bool fencesStale = false;

    void Take(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device, const ChunkMeshes& meshes) {
        ground = GpuMesh::Upload(device, meshes.ground, VertexLayout::PositionNormalTexture());
        hasWater = !meshes.water.indices.empty();
        water = hasWater ? GpuMesh::Upload(device, meshes.water, VertexLayout::PositionNormalTangentTexture())
                         : GpuMesh();
        for (std::size_t kind = 0; kind < detail.size(); ++kind) {
            hasDetail[kind] = !meshes.detail[kind].indices.empty();
            detail[kind] = hasDetail[kind] ? GpuMesh::Upload(device, meshes.detail[kind],
                                                             VertexLayout::PositionNormalTexture())
                                           : GpuMesh();
        }
    }
};

TerrainRenderer::TerrainRenderer(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device,
                                 Surfaces& surfaces)
    : device_(device), surfaces_(surfaces) {}

TerrainRenderer::~TerrainRenderer() = default;

void TerrainRenderer::Build(const zoo::World& world, bool editable) {
    const auto started = std::chrono::steady_clock::now();
    editable_ = editable;
    chunksX_ = world.ChunksX();
    chunksZ_ = world.ChunksZ();
    fenceRevision_ = world.FenceRevision();
    chunks_.clear();
    for (int cz = 0; cz < chunksZ_; ++cz) {
        for (int cx = 0; cx < chunksX_; ++cx) {
            auto chunk = std::make_unique<Chunk>();
            chunk->origin = {cx * kChunkTiles, cz * kChunkTiles};
            chunk->revision = world.ChunkRevision(cx, cz);
            chunks_.push_back(std::move(chunk));
        }
    }

    // Everything that only computes is done on threads, a chunk (or a detail
    // texture) at a time: painting, the mip levels, cutting the meshes. Each
    // piece of work reads the world and writes what is its own, so the result
    // is the same however many threads there are.
    struct Prepared {
        std::vector<Image> lowerLevels;
        ChunkMeshes meshes;
    };
    std::vector<Prepared> prepared(chunks_.size());
    std::array<std::vector<Image>, kGroundDetailTextureCount> detailLevels;
    const std::size_t detailJobs = detailTextures_[0] ? 0u : detailLevels.size();
    const std::size_t jobs = detailJobs + chunks_.size();
    std::atomic<std::size_t> next{0};
    std::mutex mutex;
    std::condition_variable finished;
    std::vector<std::size_t> ready;  // chunks prepared and not yet handed to the GPU
    // Does one piece of work; false when none is left.
    const auto workOne = [&]() -> bool {
        const std::size_t job = next.fetch_add(1);
        if (job >= jobs)
            return false;
        if (job < detailJobs) {
            detailLevels[job] = PaintGroundDetail(static_cast<GroundDetailTexture>(job));
            return true;
        }
        const std::size_t index = job - detailJobs;
        Chunk& chunk = *chunks_[index];
        const ChunkInfos infos(world, chunk.origin);
        chunk.image = Image(kTexels, kTexels);
        if (editable)
            chunk.signatures.resize(static_cast<std::size_t>(kChunkTiles * kChunkTiles));
        for (int tz = 0; tz < kChunkTiles; ++tz) {
            for (int tx = 0; tx < kChunkTiles; ++tx) {
                const TileCoord tile{chunk.origin.x + tx, chunk.origin.z + tz};
                if (world.InBounds(tile)) {
                    PaintTile(infos.At(tile), chunk.origin, chunk.image);
                    if (editable) {
                        const auto indexInChunk = static_cast<std::size_t>(tz * kChunkTiles + tx);
                        chunk.signatures[indexInChunk] = PaintSignature(world, tile);
                    }
                } else {
                    BlankTile(tile, chunk.origin, chunk.image);
                }
            }
        }
        prepared[index].lowerLevels = LowerLevels(chunk.image);
        prepared[index].meshes = CutMeshes(infos);
        {
            const std::lock_guard lock(mutex);
            ready.push_back(index);
        }
        finished.notify_one();
        return true;
    };
    using Microsoft::Xna::Framework::Graphics::SurfaceFormat;
    const auto handOver = [&](std::size_t index) {
        Chunk& chunk = *chunks_[index];
        Prepared& made = prepared[index];
        chunk.texture = std::make_unique<Texture2D>(device_, kTexels, kTexels, true, SurfaceFormat::Color);
        SetLevel(*chunk.texture, 0, chunk.image);
        const int count =
            std::min(chunk.texture->getLevelCountProperty() - 1, static_cast<int>(made.lowerLevels.size()));
        for (int level = 0; level < count; ++level)
            SetLevel(*chunk.texture, level + 1, made.lowerLevels[static_cast<std::size_t>(level)]);
        if (!editable) {
            // Once uploaded, this piece of a visitor map will never be repainted.
            chunk.image = Image();
#if defined(__linux__) && !defined(__ANDROID__)
            DiscardCpuTextureCopies(*chunk.texture);
#endif
        }
        chunk.material.name = "terrain";
        chunk.material.shading = CnaEngine::Graphics::ShadingModel::PhysicallyBased;
        chunk.material.diffuseColor = Vector3(1.0f, 1.0f, 1.0f);
        chunk.material.roughness = 0.95f;
        chunk.material.baseTexture = chunk.texture.get();
        chunk.material.sampler = &SamplerState::AnisotropicClamp;
        chunk.Take(device_, made.meshes);
        made = {};  // at once: a hundred chunks' worth of these is a lot of memory
    };
    {
#ifdef __EMSCRIPTEN__
        // The browser build has no threads (CNA's Wasm is built without pthreads).
        const unsigned count = 1;
#else
        const unsigned count = std::clamp(std::thread::hardware_concurrency(), 1u, 6u);
#endif
        std::vector<std::jthread> threads;
        for (unsigned i = 1; i < count; ++i)
            threads.emplace_back([&workOne] {
                while (workOne()) {}
            });
        // This thread owns the graphics device: it hands over what the others
        // have finished, and works as they do while nothing waits.
        std::vector<std::size_t> batch;
        for (std::size_t handed = 0; handed < chunks_.size();) {
            {
                const std::lock_guard lock(mutex);
                batch.swap(ready);
            }
            if (batch.empty()) {
                if (!workOne()) {
                    std::unique_lock lock(mutex);
                    finished.wait(lock, [&ready] { return !ready.empty(); });
                }
                continue;
            }
            for (const std::size_t index : batch)
                handOver(index);
            handed += batch.size();
            batch.clear();
        }
    }
    paintSeconds_ = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();

    for (std::size_t index = 0; index < detailJobs; ++index) {
        const std::vector<Image>& levels = detailLevels[index];
        detailTextures_[index] = std::make_unique<Texture2D>(
            device_, levels.front().Width(), levels.front().Height(), true, SurfaceFormat::Color);
        const int count =
            std::min(detailTextures_[index]->getLevelCountProperty(), static_cast<int>(levels.size()));
        for (int level = 0; level < count; ++level)
            SetLevel(*detailTextures_[index], level, levels[static_cast<std::size_t>(level)]);
    }
    for (std::size_t kind = 1; kind < detailMaterials_.size(); ++kind) {
        const GroundDetailLook look = LookOf(static_cast<GroundDetail>(kind));
        CnaEngine::Graphics::Material& material = detailMaterials_[kind];
        material = {};
        material.name = "ground detail";
        material.shading = CnaEngine::Graphics::ShadingModel::PhysicallyBased;
        material.blend = CnaEngine::Graphics::BlendMode::AlphaBlend;
        material.diffuseColor = ToVector(look.tint);
        material.roughness = 1.0f;
        material.specularFactor = 0.0f;
        material.baseTexture = detailTextures_[static_cast<std::size_t>(look.texture)].get();
        material.sampler = &SamplerState::AnisotropicWrap;
        material.uvScale = Microsoft::Xna::Framework::Vector2(look.repeats, look.repeats);
    }
    if (detailRasterizer_ == nullptr) {
        using Microsoft::Xna::Framework::Graphics::CullMode;
        detailRasterizer_ = std::make_unique<Microsoft::Xna::Framework::Graphics::RasterizerState>();
        detailRasterizer_->setCullModeProperty(CullMode::CullCounterClockwiseFace);
        detailRasterizer_->setDepthBiasProperty(kDetailDepthBias);
        detailRasterizer_->setSlopeScaleDepthBiasProperty(kDetailSlopeBias);
    }
    water_ = surfaces_.Water();
    water_.alpha = kWaterAlpha;

    BuildApron(world);
    if (!editable) {
#if defined(__linux__) && !defined(__ANDROID__)
        for (auto& texture : detailTextures_) {
            if (texture)
                DiscardCpuTextureCopies(*texture);
        }
        if (apronTexture_)
            DiscardCpuTextureCopies(*apronTexture_);
#endif
#if defined(__GLIBC__)
        (void)malloc_trim(0);
#endif
    }
    buildSeconds_ = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
}

void TerrainRenderer::Update(const zoo::World& world, int maxChunks) {
    // Surfaces animates its water by changing the ripples' texture.
    water_.normalTexture = surfaces_.Water().normalTexture;
    if (!editable_)
        return;
    if (world.ChunksX() != chunksX_ || world.ChunksZ() != chunksZ_) {
        Build(world);
        return;
    }
    if (world.FenceRevision() != fenceRevision_) {
        // Fences wear the ground beside them, and they do not touch a chunk's revision.
        fenceRevision_ = world.FenceRevision();
        for (auto& chunk : chunks_)
            chunk->fencesStale = true;
    }
    int done = 0;
    for (int cz = 0; cz < chunksZ_ && done < maxChunks; ++cz) {
        for (int cx = 0; cx < chunksX_ && done < maxChunks; ++cx) {
            Chunk& chunk = *chunks_[static_cast<std::size_t>(cz * chunksX_ + cx)];
            const std::uint32_t revision = world.ChunkRevision(cx, cz);
            const bool changed = chunk.revision != revision;
            if (!changed && !chunk.fencesStale)
                continue;
            chunk.revision = revision;
            chunk.fencesStale = false;
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
                    PaintTile(PaintInfo(world, tile), chunk.origin, chunk.image);
                    repainted = true;
                }
            }
            if (!changed && !repainted)
                continue;  // the fence was somewhere else
            ++done;
            if (repainted)
                CnaEngine::Graphics::UpdateTexture(*chunk.texture, chunk.image,
                                                   {.srgb = true, .alphaTestReference = {}});
            chunk.Take(device_, CutMeshes(ChunkInfos(world, chunk.origin)));
        }
    }
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
    apronTexture_ =
        CnaEngine::Graphics::UploadTexture(device_, image, {.srgb = true, .alphaTestReference = {}});

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
            frame.Add(chunk->water, water_, identity);
    }
    for (const auto& land : land_)
        frame.Add(land->mesh, land->material, identity);
}

void TerrainRenderer::DrawDetail(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device,
                                 const CnaEngine::Graphics::Camera& camera,
                                 CnaEngine::Graphics::IMaterialBinder& binder,
                                 CnaEngine::Graphics::RenderStatistics& statistics) const {
    namespace Xna = Microsoft::Xna::Framework::Graphics;
    using Microsoft::Xna::Framework::ContainmentType;
    if (detailRasterizer_ == nullptr)
        return;
    static const CnaEngine::Graphics::FallbackSink kQuiet = [](const CnaEngine::Graphics::Material&,
                                                               const std::string&) {};
    const Xna::BlendState blend = device.getBlendStateProperty();
    const Xna::DepthStencilState depth = device.getDepthStencilStateProperty();
    const Xna::RasterizerState rasterizer = device.getRasterizerStateProperty();
    device.setDepthStencilStateProperty(Xna::DepthStencilState::DepthRead);
    device.setRasterizerStateProperty(*detailRasterizer_);
    const Vector3 eye = camera.Position();
    const auto& frustum = camera.Frustum();
    const float reach = DetailReach(camera, device.getViewportProperty().getHeightProperty());
    CnaEngine::Graphics::RenderItem item;
    for (const auto& chunk : chunks_) {
        for (std::size_t kind = 1; kind < chunk->detail.size(); ++kind) {
            if (!chunk->hasDetail[kind])
                continue;
            const auto& box = chunk->detail[kind].Bounds();
            // A texture laid twice as often has texels half the size and is gone at half the distance.
            const float farthest = reach / detailMaterials_[kind].uvScale.X;
            const Vector3 nearest = Vector3::Clamp(eye, box.Min, box.Max);
            if (Vector3::DistanceSquared(eye, nearest) > farthest * farthest ||
                frustum.Contains(box) == ContainmentType::Disjoint)
                continue;
            item.mesh = &chunk->detail[kind];
            item.material = &detailMaterials_[kind];
            device.setBlendStateProperty(binder.BlendStateFor(item));
            if (binder.Apply(device, item, CnaEngine::Graphics::RenderBucket::Transparent, camera, kQuiet))
                item.mesh->Draw(device, &statistics);
        }
    }
    device.setBlendStateProperty(blend);
    device.setDepthStencilStateProperty(depth);
    device.setRasterizerStateProperty(rasterizer);
}

}  // namespace zoogame
