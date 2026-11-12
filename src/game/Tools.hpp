// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "CnaEngine/App/EngineGame.hpp"
#include "CnaEngine/Graphics/Camera.hpp"
#include "CnaEngine/Graphics/GpuMesh.hpp"

#include "Controls.hpp"
#include "MeshWriter.hpp"
#include "Play.hpp"
#include "StaticBatch.hpp"
#include "Surfaces.hpp"

namespace zoogame {

/// Where the pointer is in the world.
struct Pointer {
    bool onGround = false;
    zoo::Vec3 origin;     ///< the eye
    zoo::Vec3 direction;  ///< unit
    zoo::Vec3 ground;     ///< where the ray meets the ground
    zoo::Vec2 at;         ///< the same, on the map
    zoo::TileCoord tile;
    float distance = 0.0f;  ///< from the eye to the ground
};

/// The animal under the pointer, nearer than the ground behind it.
zoo::AnimalId PickAnimal(const Play& play, const Pointer& pointer);

/// The tools in the player's hand: what the pointer does to the world, and the
/// preview of it drawn over the world before the click.
class WorldTools {
public:
    WorldTools(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device, Surfaces& surfaces,
               const zoo::Catalog& catalog);

    /// Call after the interface was built (which claims the pointer over its panels).
    void Update(Play& play, const Controls& controls, const CnaEngine::Graphics::Camera& camera,
                Vector2 viewport, float seconds);
    void Submit(CnaEngine::App::Frame& frame) const;

private:
    void Inspect(Play& play, const Controls& controls, const Pointer& pointer);
    void Paths(Play& play, const Controls& controls, const Pointer& pointer);
    void Fences(Play& play, const Controls& controls, const Pointer& pointer);
    void Terrain(Play& play, const Controls& controls, const Pointer& pointer);
    void Water(Play& play, const Controls& controls, const Pointer& pointer);
    void Terraform(Play& play, const Controls& controls, const Pointer& pointer, float seconds);
    void Buildings(Play& play, const Controls& controls, const Pointer& pointer);
    void Props(Play& play, const Controls& controls, const Pointer& pointer);
    void Animals(Play& play, const Controls& controls, const Pointer& pointer);
    void Bulldoze(Play& play, const Controls& controls, const Pointer& pointer);
    void ShowSelection(const Play& play);

    MeshWriter& Layer(Mark mark) { return layers_[static_cast<std::size_t>(mark)].writer; }
    void MarkTile(Mark mark, const zoo::World& world, zoo::TileCoord tile);
    void MarkEdge(Mark mark, const zoo::World& world, zoo::EdgeRef edge, float height = 1.4f);
    void MarkBand(Mark mark, const zoo::World& world, zoo::TileCoord tile, zoo::Dir side, float width);
    void MarkRing(Mark mark, const zoo::World& world, zoo::Vec2 center, float radius, float width = 0.25f);
    void UploadLayers();
    void ShowFences(const zoo::World& world, const std::vector<zoo::EdgeRef>& edges, int kind, bool gate);
    void ShowGhost(std::uint64_t key, const Matrix& world, auto&& build);

    Microsoft::Xna::Framework::Graphics::GraphicsDevice& device_;
    Surfaces& surfaces_;
    const zoo::Catalog& catalog_;

    struct LayerMesh {
        MeshWriter writer;
        CnaEngine::Graphics::GpuMesh mesh;
        std::uint64_t hash = 0;
        bool shown = false;
    };
    std::array<LayerMesh, static_cast<std::size_t>(Mark::Count)> layers_;

    StaticBatch fences_;
    std::uint64_t fencesKey_ = 0;
    bool fencesShown_ = false;
    StaticBatch ghost_;
    std::uint64_t ghostKey_ = 0;
    Matrix ghostWorld_;
    bool ghostShown_ = false;

    // The gesture in progress.
    bool dragging_ = false;
    zoo::TileCoord anchorTile_;
    zoo::TileCoord anchorVertex_;
    std::optional<zoo::TileCoord> lastPainted_;
    std::optional<zoo::Vec2> lastProp_;
    float terraformClock_ = 0.0f;
    float rightTravel_ = 0.0f;
    std::uint32_t scatter_ = 1;
    std::uint64_t lastBulldozed_ = 0;
};

}  // namespace zoogame
