// SPDX-License-Identifier: MIT
#pragma once

#include <cstddef>
#include <memory>
#include <vector>

#include "CnaEngine/App/EngineGame.hpp"
#include "CnaEngine/Graphics/GpuMesh.hpp"

#include "MeshWriter.hpp"
#include "Surfaces.hpp"

namespace zoogame {

/// Geometry that does not move, uploaded once: one mesh per finish of a
/// ModelBuilder. Many objects merged into one batch cost one draw per finish.
class StaticBatch {
public:
    /// Replaces the contents with the builder's geometry (empty parts are skipped).
    void Build(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device, Surfaces& surfaces,
               const ModelBuilder& builder);
    /// Adds one mesh in a material of the caller's, which must outlive the batch (nothing
    /// when the mesh is empty).
    void Add(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device,
             const CnaEngine::Graphics::MeshData& mesh, const CnaEngine::Graphics::Material& material);
    void Clear();
    void Submit(CnaEngine::App::Frame& frame) const;
    void Submit(CnaEngine::App::Frame& frame, const Matrix& world) const;

    bool Empty() const { return pieces_.empty(); }
    std::size_t Triangles() const { return triangles_; }
    std::size_t Pieces() const { return pieces_.size(); }

private:
    struct Piece {
        CnaEngine::Graphics::GpuMesh mesh;
        const CnaEngine::Graphics::Material* material = nullptr;
    };
    std::vector<std::unique_ptr<Piece>> pieces_;
    std::size_t triangles_ = 0;
};

}  // namespace zoogame
