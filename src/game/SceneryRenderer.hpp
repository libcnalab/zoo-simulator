// SPDX-License-Identifier: MIT
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "CnaEngine/App/EngineGame.hpp"

#include "StaticBatch.hpp"
#include "Surfaces.hpp"
#include "zoo/build/Catalog.hpp"
#include "zoo/build/Objects.hpp"
#include "zoo/world/World.hpp"

namespace zoogame {

/// Draws everything that stands on the ground and does not move: fences and
/// gates, the entrance, buildings, trees, rocks, feeders, benches.
///
/// The map is divided into cells of kCellTiles x kCellTiles tiles. Each cell
/// merges its fences into one batch and its buildings and props into another
/// (one mesh per finish), so a whole zoo costs a few hundred draw calls however
/// much is built. A cell is rebuilt only when what stands in it changed.
class SceneryRenderer {
public:
    static constexpr int kCellTiles = 32;

    SceneryRenderer(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device, Surfaces& surfaces,
                    const zoo::Catalog& catalog);
    ~SceneryRenderer();

    /// Brings at most `maxCells` out-of-date cells up to date (all of them when
    /// negative); call every frame. Cheap when nothing changed.
    void Update(const zoo::World& world, const zoo::ZooObjects& objects, int maxCells = 2);
    void Submit(CnaEngine::App::Frame& frame) const;

    std::size_t Triangles() const;
    std::size_t DrawCalls() const;

private:
    struct Cell;

    void Layout(const zoo::World& world);
    std::uint64_t FenceHash(const zoo::World& world, const Cell& cell) const;
    std::uint64_t ObjectHash(const zoo::World& world, const zoo::ZooObjects& objects, const Cell& cell) const;
    void BuildFences(const zoo::World& world, Cell& cell);
    void BuildObjects(const zoo::World& world, const zoo::ZooObjects& objects, Cell& cell);

    Microsoft::Xna::Framework::Graphics::GraphicsDevice& device_;
    Surfaces& surfaces_;
    const zoo::Catalog& catalog_;
    std::vector<std::unique_ptr<Cell>> cells_;
    int width_ = 0;
    int depth_ = 0;
    std::uint32_t worldRevision_ = 0;
    std::uint32_t objectsRevision_ = 0;
    bool pending_ = false;
};

}  // namespace zoogame
