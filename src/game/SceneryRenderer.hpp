// SPDX-License-Identifier: MIT
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
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
/// The map is divided into square cells. Each cell merges its fences into one batch and
/// its buildings and props into another (one mesh per finish). A cell is rebuilt only when
/// what stands in it changed, and the renderer leaves out the cells outside the view.
///
/// How large the cells are decides what a frame costs. A draw call costs about 6 microseconds
/// here whatever it draws, a million triangles about one millisecond (measured 2026-11-13,
/// docs/PERFORMANCE.md): a zoo nobody builds in is one cell, a zoo under construction has
/// small ones so that an edit rebuilds little.
class SceneryRenderer {
public:
    /// Cell edge for a zoo that changes under the player's hands.
    static constexpr int kEditCellTiles = 32;

    SceneryRenderer(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device, Surfaces& surfaces,
                    const zoo::Catalog& catalog, int cellTiles = kEditCellTiles);
    ~SceneryRenderer();

    /// Brings at most `maxCells` out-of-date cells up to date (all of them when
    /// negative); call every frame. Cheap when nothing changed.
    void Update(const zoo::World& world, const zoo::ZooObjects& objects, int maxCells = 2);
    void Submit(CnaEngine::App::Frame& frame) const;

    std::size_t Triangles() const;
    std::size_t DrawCalls() const;
    /// What the triangles are spent on, the most expensive kind first: a line for all fences
    /// and one for each kind of prop and building that stands in the zoo (for --bench).
    std::string Tally(const zoo::ZooObjects& objects) const;

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
    int cellTiles_ = kEditCellTiles;
    std::uint32_t worldRevision_ = 0;
    std::uint32_t objectsRevision_ = 0;
    bool pending_ = false;
};

}  // namespace zoogame
