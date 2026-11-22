// SPDX-License-Identifier: MIT
#pragma once

#include <algorithm>
#include <cmath>

namespace zoogame {

constexpr float kAnimalProxyBelowPixels = 10.0f;
constexpr float kAnimalFullAbovePixels = 14.0f;

/// Largest projected body dimension in vertical screen pixels. Using the camera field of view
/// makes binocular magnification select the full model without a separate gameplay exception.
inline float AnimalProjectedPixels(float sizeM, float distanceM, float verticalFieldOfView,
                                   int viewportHeight) {
    const float tangent = std::tan(verticalFieldOfView * 0.5f);
    if (sizeM <= 0.0f || tangent <= 0.0f || viewportHeight <= 0)
        return 0.0f;
    const float pixelsAtOneMetre = static_cast<float>(viewportHeight) / (2.0f * tangent);
    return sizeM * pixelsAtOneMetre / std::max(0.1f, distanceM);
}

/// Keeps the previous representation between ten and fourteen pixels, so walking animals do not
/// pop back and forth at one exact boundary.
inline bool UseAnimalProxy(bool wasProxy, float projectedPixels) {
    return wasProxy ? projectedPixels <= kAnimalFullAbovePixels
                    : projectedPixels < kAnimalProxyBelowPixels;
}

}  // namespace zoogame
