// SPDX-License-Identifier: MIT
#include "StaticBatch.hpp"

namespace zoogame {

void StaticBatch::Build(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device, Surfaces& surfaces,
                        const ModelBuilder& builder) {
    Clear();
    for (const ModelPart& part : builder.Parts()) {
        if (part.writer.Empty()) {
            continue;
        }
        auto piece = std::make_unique<Piece>();
        piece->mesh = CnaEngine::Graphics::GpuMesh::Upload(
            device, part.writer.mesh, CnaEngine::Graphics::VertexLayout::PositionNormalTexture());
        piece->material = &surfaces.Get(part.finish);
        triangles_ += part.writer.mesh.TriangleCount();
        pieces_.push_back(std::move(piece));
    }
}

void StaticBatch::Clear() {
    pieces_.clear();
    triangles_ = 0;
}

void StaticBatch::Submit(CnaEngine::App::Frame& frame) const {
    Submit(frame, Matrix::getIdentityProperty());
}

void StaticBatch::Submit(CnaEngine::App::Frame& frame, const Matrix& world) const {
    for (const auto& piece : pieces_) {
        frame.Add(piece->mesh, *piece->material, world);
    }
}

}  // namespace zoogame
