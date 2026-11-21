// SPDX-License-Identifier: MIT
#pragma once

#include "CnaEngine/Collision/StaticWorld.hpp"
#include "MeshWriter.hpp"

namespace zoogame {

/// An opaque surface hides an animal. A sheet of wire or cut-out foliage has open space.
inline bool BlocksAnimalSight(Pattern pattern) {
    return pattern != Pattern::Glass && pattern != Pattern::Pane && pattern != Pattern::WireMesh &&
           pattern != Pattern::WeldMesh && pattern != Pattern::LeafSpray &&
           pattern != Pattern::NeedleSpray && pattern != Pattern::Fronds;
}

inline void AddSightSurfaces(CnaEngine::Collision::StaticWorld& solids, const ModelBuilder& model) {
    for (const ModelPart& part : model.Parts()) {
        if (BlocksAnimalSight(part.finish.pattern)) {
            const auto& mesh = part.writer.mesh;
            solids.AddTriangles(mesh.positions, mesh.indices, Matrix::getIdentityProperty());
        }
    }
}

}  // namespace zoogame
