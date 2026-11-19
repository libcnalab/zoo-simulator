// SPDX-License-Identifier: MIT
#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

#include "CnaEngine/App/EngineGame.hpp"
#include "CnaEngine/Graphics/Camera.hpp"
#include "CnaEngine/Graphics/GpuMesh.hpp"
#include "CnaEngine/Graphics/Image.hpp"
#include "CnaEngine/Graphics/Material.hpp"
#include "CnaEngine/Graphics/MaterialBinder.hpp"
#include "CnaEngine/Graphics/RenderStatistics.hpp"
#include "Microsoft/Xna/Framework/Graphics/RasterizerState.hpp"

#include "Common.hpp"
#include "GroundDetail.hpp"
#include "Surfaces.hpp"
#include "zoo/world/World.hpp"

namespace zoogame {

/// Draws the ground: the height field in chunks, each with its own baked colour
/// texture (terrain, paths, shores), the detail layer over it near the eye
/// (GroundDetail.hpp), the water surfaces, and the land beyond the map out to
/// the horizon.
///
/// Chunks follow World::ChunkRevision and World::FenceRevision. A changed chunk
/// gets new meshes, and only the tiles whose appearance really changed are
/// repainted in its texture.
class TerrainRenderer {
public:
    /// Texels of baked ground colour along one tile edge.
    static constexpr int kTexelsPerTile = 32;

    TerrainRenderer(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device, Surfaces& surfaces);
    ~TerrainRenderer();

    /// Builds everything for a world (start of a game, after loading one).
    /// Paints and cuts the meshes on several threads, each chunk by itself, so
    /// the result does not depend on how many threads there are.
    void Build(const zoo::World& world);
    /// Releases the CPU paint images and tile signatures after building a read-only world.
    /// Afterwards Update still advances animated water, but ignores map and fence revisions.
    /// A later Build restores the editable state.
    void DiscardEditCache();
    /// Brings at most `maxChunks` changed chunks up to date; call every frame.
    void Update(const zoo::World& world, int maxChunks = 3);
    /// Everything but the detail layer (DrawDetail).
    void Submit(CnaEngine::App::Frame& frame) const;
    /// Draws the detail layer; for a render pass right after the opaque scene
    /// (PassPoint::AfterOpaque), added before any pass that lays something on
    /// the ground. The layer is blended onto the ground, and a Frame's blended
    /// items are all drawn after those passes: submitted there it would lie
    /// over the shade under people instead of under it. `binder` is the
    /// renderer's own; the device's states are left as they were.
    void DrawDetail(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device,
                    const CnaEngine::Graphics::Camera& camera, CnaEngine::Graphics::IMaterialBinder& binder,
                    CnaEngine::Graphics::RenderStatistics& statistics) const;

    /// Seconds the last Build spent on its chunks: painting the textures and
    /// their mip levels, cutting the meshes, handing them to the GPU (for the
    /// performance log).
    double LastPaintSeconds() const { return paintSeconds_; }
    /// Seconds the last Build took altogether, handing everything to the GPU included.
    double LastBuildSeconds() const { return buildSeconds_; }

private:
    struct Chunk;

    void BuildApron(const zoo::World& world);

    Microsoft::Xna::Framework::Graphics::GraphicsDevice& device_;
    Surfaces& surfaces_;
    std::vector<std::unique_ptr<Chunk>> chunks_;
    int chunksX_ = 0;
    int chunksZ_ = 0;
    std::uint32_t fenceRevision_ = 0;
    std::array<std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D>, kGroundDetailTextureCount>
        detailTextures_;
    /// One material for each GroundDetail (None's is not used).
    std::array<CnaEngine::Graphics::Material, kGroundDetailCount> detailMaterials_;
    /// Pulls the layer a hair towards the eye, so that it never fights the ground it lies on.
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::RasterizerState> detailRasterizer_;
    /// Surfaces' water, let through more light: the painted bed shows, pale in
    /// the shallows and dark in the deep.
    CnaEngine::Graphics::Material water_;
    /// The land beyond the map: a skirt easing the rim's heights to a level, then
    /// a patchwork of fields over rolling hills, one mesh and material per crop.
    struct Land {
        CnaEngine::Graphics::GpuMesh mesh;
        CnaEngine::Graphics::Material material;
    };
    std::vector<std::unique_ptr<Land>> land_;
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> apronTexture_;
    bool editCacheDiscarded_ = false;
    double paintSeconds_ = 0.0;
    double buildSeconds_ = 0.0;
};

}  // namespace zoogame
