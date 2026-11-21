// SPDX-License-Identifier: MIT
#pragma once

#include <cstddef>
#include <cstdint>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "CnaEngine/App/EngineGame.hpp"

#include "PlantPalette.hpp"
#include "StaticBatch.hpp"
#include "Surfaces.hpp"
#include "zoo/build/Catalog.hpp"
#include "zoo/build/Objects.hpp"
#include "zoo/world/World.hpp"

namespace zoo {
class Zoo;
}

namespace zoogame {

class BuildingSight;

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
///
/// Plants are kept apart, in groves: squares of kGroveTiles (of the cells' size in a zoo under
/// construction, so that an edit rebuilds as little). They are most of the triangles of a zoo,
/// and each grove is built twice. Near the eye it is drawn as it is, one mesh per finish. Far
/// from it, it is drawn in the plants' far form (PlantDetail): about a quarter of the
/// triangles, merged into two meshes whatever the finishes are, because their colours and
/// cut-outs come from one picture (PlantPalette). The far form keeps its opaque wood and
/// crown hearts, so a far plant still casts its shadow. Groves out of view are culled by the
/// renderer like any item (docs/PLANTS.md has the measurements that chose the sizes).
class SceneryRenderer {
public:
    /// Cell edge for a zoo that changes under the player's hands.
    static constexpr int kEditCellTiles = 32;
    /// Edge of a grove during a visit: 80 m, 16 groves for the visitor zoo. Of 20, 32, 40 and
    /// 80 tiles (the last offset by half a grove) this was as fast as any at the entrance.
    static constexpr int kGroveTiles = 40;
    /// A grove is drawn in the far form when all of it is farther from the eye than
    /// kFarBeyond metres, and in the near one again when some of it is nearer than
    /// kNearWithin; between them it stays as it was, so that a visitor pacing up and down
    /// does not flip it. At 50 m a pixel is 7 cm wide: what the far form leaves out (three
    /// sprays of four, twigs, the rings of a palm) is below that or hidden in the crown, and
    /// a walk across the switch shows no jump (docs/PLANTS.md).
    static constexpr float kFarBeyond = 56.0f;
    static constexpr float kNearWithin = 48.0f;

    SceneryRenderer(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device, Surfaces& surfaces,
                    const zoo::Catalog& catalog, int cellTiles = kEditCellTiles);
    ~SceneryRenderer();

    /// Brings at most `maxCells` out-of-date cells up to date (all of them when
    /// negative); call every frame. Cheap when nothing changed.
    void Update(const zoo::World& world, const zoo::ZooObjects& objects, int maxCells = 2);
    /// Animates the separately operated holding gates at animal houses while a
    /// dangerous habitat is recalled, worked in and released.
    void UpdateHoldingGates(const zoo::Zoo& zoo, double seconds);
    /// Releases the completed far-plant palette's CPU edit data in a read-only visit.
    void DiscardEditCache(bool discardCpuTextureCopies = false);
    /// Submits the scenery as seen from `eye`: the groves far from it in their far form.
    void Submit(CnaEngine::App::Frame& frame, const Vector3& eye);
    /// Submits everything in full detail.
    void Submit(CnaEngine::App::Frame& frame) const;
    /// Distance to the nearest solid part of a tree or building between the eye and the animal.
    std::optional<float> ObstructionInSight(const Vector3& eye, const Vector3& direction, float reach);

    /// Triangles and draw calls of everything in full detail; a frame draws what is in view
    /// of it, and the far groves with fewer triangles.
    std::size_t Triangles() const;
    std::size_t DrawCalls() const;
    /// What the triangles are spent on, the most expensive kind first: a line for all fences
    /// and one for each kind of prop and building that stands in the zoo (for --bench).
    std::string Tally(const zoo::ZooObjects& objects) const;

private:
    struct Cell;
    struct Grove;
    struct HoldingGate;
    struct TreeGeometry;

    void Layout(const zoo::World& world);
    std::uint64_t FenceHash(const zoo::World& world, const Cell& cell) const;
    std::uint64_t ObjectHash(const zoo::World& world, const zoo::ZooObjects& objects, const Cell& cell) const;
    void BuildFences(const zoo::World& world, Cell& cell);
    void BuildObjects(const zoo::World& world, const zoo::ZooObjects& objects, Cell& cell);
    const zoo::PropDef* PlantDef(const zoo::Prop* prop) const;
    std::uint64_t GroveHash(const zoo::World& world, const zoo::ZooObjects& objects,
                            const Grove& grove) const;
    void BuildGrove(const zoo::World& world, const zoo::ZooObjects& objects, Grove& grove);

    Microsoft::Xna::Framework::Graphics::GraphicsDevice& device_;
    Surfaces& surfaces_;
    const zoo::Catalog& catalog_;
    PlantPalette palette_;
    StaticBatch holdingGateModel_;
    StaticBatch holdingGateGuides_;
    std::map<zoo::BuildingId, HoldingGate> holdingGates_;
    std::vector<std::unique_ptr<Cell>> cells_;
    std::vector<std::unique_ptr<Grove>> groves_;
    std::list<TreeGeometry> treeGeometry_;
    std::map<int, BuildingSight> buildingGeometry_;
    int width_ = 0;
    int depth_ = 0;
    int cellTiles_ = kEditCellTiles;
    std::uint32_t worldRevision_ = 0;
    std::uint32_t objectsRevision_ = 0;
    bool pending_ = false;
    bool fullDetail_ = false;  ///< ZOO_PLANTS_FULL_DETAIL: fixed-view comparison of near and far forms
    bool editCacheDiscarded_ = false;
};

}  // namespace zoogame
