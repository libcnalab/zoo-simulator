// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "CnaEngine/App/EngineGame.hpp"
#include "CnaEngine/Graphics/GpuMesh.hpp"
#include "CnaEngine/Graphics/Image.hpp"
#include "CnaEngine/Graphics/Material.hpp"

#include "Common.hpp"
#include "Surfaces.hpp"
#include "zoo/world/World.hpp"

namespace zoogame {

/// Draws the ground: the height field in chunks, each with its own baked colour
/// texture (terrain, paths, shores), the water surfaces, and the land beyond the
/// map out to the horizon.
///
/// Chunks follow World::ChunkRevision. A changed chunk gets a new mesh, and only
/// the tiles whose appearance really changed are repainted in its texture.
class TerrainRenderer {
public:
    /// Texels of baked ground colour along one tile edge.
    static constexpr int kTexelsPerTile = 32;

    TerrainRenderer(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device, Surfaces& surfaces);
    ~TerrainRenderer();

    /// Builds everything for a world (start of a game, after loading one).
    /// Paints on several threads; takes a second or two.
    void Build(const zoo::World& world);
    /// Brings at most `maxChunks` changed chunks up to date; call every frame.
    void Update(const zoo::World& world, int maxChunks = 3);
    void Submit(CnaEngine::App::Frame& frame) const;

    /// Seconds the last Build spent painting textures (for the performance log).
    double LastPaintSeconds() const { return paintSeconds_; }

private:
    struct Chunk;

    void BuildMesh(const zoo::World& world, Chunk& chunk);
    void BuildApron(const zoo::World& world);

    Microsoft::Xna::Framework::Graphics::GraphicsDevice& device_;
    Surfaces& surfaces_;
    std::vector<std::unique_ptr<Chunk>> chunks_;
    int chunksX_ = 0;
    int chunksZ_ = 0;
    /// The land beyond the map: a skirt easing the rim's heights to a level, then
    /// a patchwork of fields over rolling hills, one mesh and material per crop.
    struct Land {
        CnaEngine::Graphics::GpuMesh mesh;
        CnaEngine::Graphics::Material material;
    };
    std::vector<std::unique_ptr<Land>> land_;
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> apronTexture_;
    double paintSeconds_ = 0.0;
};

}  // namespace zoogame
