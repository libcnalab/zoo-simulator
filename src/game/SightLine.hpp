// SPDX-License-Identifier: MIT
#pragma once

#include <algorithm>
#include <cmath>
#include <numbers>

#include "zoo/build/Catalog.hpp"

namespace zoogame {

/// The opaque horizontal footprint an automatic viewpoint keeps out of its line of sight.
/// A tree's walking radius describes its trunk; shade area is the catalog's existing estimate
/// of the crown projected on the ground and is the useful conservative radius for a clear view.
inline float SightBlockingRadius(const zoo::PropDef& def, float scale) {
    float radius = def.radiusM;
    if (def.role == zoo::PropRole::Tree && def.shadeM2 > 0.0f) {
        radius = std::max(radius, std::sqrt(def.shadeM2 / std::numbers::pi_v<float>));
    }
    return radius * std::max(scale, 0.0f);
}

}  // namespace zoogame
