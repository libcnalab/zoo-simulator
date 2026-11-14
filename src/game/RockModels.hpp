// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>

#include "MeshWriter.hpp"

namespace zoogame {

/// How a crag is cut.
struct CragShape {
    int sides = 8;        ///< facets round it
    int tiers = 3;        ///< beds from the ground to the top
    float rugged = 0.2f;  ///< how far a corner strays from the clean form, as a fraction of the radius
    float dip = 0.0f;     ///< the beds' dip: metres of rise for every metre along x
    float taper = 0.45f;  ///< the top's radius as a fraction of the foot's
    float sink = 0.3f;    ///< how far it reaches into the ground
};

/// A rock of flat facets, in beds one above the other, each set back from the one beneath
/// it by a ledge: `foot` is the middle of its base on the ground, `radii` its half extents
/// (y: its height). The texture runs round it and up it in metres, so that the strata of
/// Pattern::Rock lie in the beds.
void Crag(MeshWriter& w, const Vector3& foot, const Vector3& radii, std::uint32_t seed,
          const CragShape& shape = {});

}  // namespace zoogame
