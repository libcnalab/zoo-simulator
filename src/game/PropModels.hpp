// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>

#include "MeshWriter.hpp"
#include "zoo/build/Catalog.hpp"

namespace zoogame {

enum class PropDetail {
    Near,
    Far,
};

/// Writes the model of a prop (a tree, a rock, a feeder, a bench...) into
/// `model`, in the prop's own frame: standing on the origin, y up, its front
/// towards +z, at scale 1. The caller sets the builder's transform.
///
/// The shape follows PropDef::look; `seed` makes every plant and rock its own.
/// Unknown looks give a plain marker post so that they are noticed.
void BuildProp(ModelBuilder& model, const zoo::PropDef& def, std::uint32_t seed,
               PropDetail detail = PropDetail::Near);

}  // namespace zoogame
