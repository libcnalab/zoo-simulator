// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>

#include "MeshWriter.hpp"
#include "zoo/build/Catalog.hpp"

namespace CnaEngine::Collision {
class StaticWorld;
}

namespace zoogame {

/// How much of a plant is written.
enum class PlantDetail : std::uint8_t {
    Near,  ///< every spray, twig and root: what a visitor stands next to
    /// The same plant, the same shape in the same place, with a quarter or less of the
    /// triangles: one card stands for four and carries their four sprays, hearts and stems
    /// have fewer sides, and what is thinner than a wrist is left out. Made to be taken for
    /// the near one from some seventy metres on.
    Far,
};

/// Whether BuildPlant builds this prop (trees, bushes, ferns, bamboo, tufts, flower beds).
bool IsPlant(const zoo::PropDef& def);

/// Writes a plant's model in the prop's own frame (as BuildProp does); false, and nothing
/// written, when the prop is not a plant. The same seed gives the same individual in both
/// details.
bool BuildPlant(ModelBuilder& model, const zoo::PropDef& def, std::uint32_t seed,
                PlantDetail detail = PlantDetail::Near);

inline bool OpaqueTreeSurface(Pattern finish) {
    return finish == Pattern::Bark || finish == Pattern::BirchBark ||
           finish == Pattern::LeafShade || finish == Pattern::Needles;
}

/// Add the wood and opaque crown surfaces of this individual to a sight query. Leaves on
/// cut-out cards have holes, so a card's rectangular outline cannot obstruct the whole view.
void BuildTreeSight(CnaEngine::Collision::StaticWorld& sight, const zoo::PropDef& def,
                    std::uint32_t seed, PlantDetail detail, const Matrix& placement);

/// A clipped hedge from `a` to `b` on the ground line, `height` metres high: a dark solid
/// core, which hides what is behind it and casts the shadow, in a coat of leaf sprays.
void BuildHedge(ModelBuilder& model, const Vector3& a, const Vector3& b, float height,
                const zoo::Color3& color, std::uint32_t seed);

}  // namespace zoogame
