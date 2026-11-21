// SPDX-License-Identifier: MIT
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <optional>

#include "zoo/base/Math.hpp"
#include "zoo/build/Catalog.hpp"

namespace zoogame {

/// Animated hips, chest and head in world space, with the radius of the grown or growing body.
struct AnimalSightTarget {
    std::array<zoo::Vec3, 3> joints;
    float radius;
};

struct GazeHit {
    float miss;   ///< zero on the body, up to one at the edge of the aiming tolerance
    float depth;  ///< used to pick the nearer of two bodies under the same sight
    bool operator<(const GazeHit& other) const {
        return miss < other.miss || (miss == other.miss && depth < other.depth);
    }
};

/// The closest approach already lies inside the body. Refine the first point of contact so a
/// large animal cannot obscure a smaller one yet lose the comparison at its deeper centreline.
inline float FirstBodyContact(zoo::Vec3 start, zoo::Vec3 axis, zoo::Vec3 gaze, float radius, float depth) {
    const float length2 = zoo::Dot(axis, axis);
    const float end = zoo::Dot(start + axis, gaze);
    float low = std::max(0.8f, std::min(zoo::Dot(start, gaze), end) - radius);
    float high = depth;
    while (high - low > 0.002f) {
        const float middle = (low + high) * 0.5f;
        const auto offset = gaze * middle - start;
        const float t = length2 > 0 ? std::clamp(zoo::Dot(offset, axis) / length2, 0.0f, 1.0f) : 0.0f;
        const auto separation = offset - axis * t;
        if (zoo::Dot(separation, separation) > radius * radius)
            low = middle;
        else
            high = middle;
    }
    return high;
}

inline std::optional<GazeHit> AimAtAnimal(const AnimalSightTarget& target, zoo::Vec3 eye, zoo::Vec3 direction,
                                          float reach, float magnification) {
    if (reach < 0.8f || magnification <= 0.0f)
        return std::nullopt;
    std::optional<GazeHit> best;
    for (std::size_t i = 1; i < target.joints.size(); ++i) {
        const auto a = target.joints[i - 1] - eye;
        const auto axis = target.joints[i] - target.joints[i - 1];
        const float start = zoo::Dot(a, direction);
        const float lengthAlong = zoo::Dot(axis, direction);
        const float end = start + lengthAlong;
        const float radius = target.radius * (i == 1 ? 1.0f : 0.65f);
        if (std::max(start, end) + radius < 0.8f || std::min(start, end) - radius > reach)
            continue;
        // Project the segment into the plane perpendicular to the gaze. Its closest point
        // covers a long neck as well as a body seen end-on, without extending beyond the animal.
        const auto offset = a - direction * start;
        const auto across = axis - direction * lengthAlong;
        const float width2 = zoo::Dot(across, across);
        float t = width2 > 1.0e-8f ? std::clamp(-zoo::Dot(offset, across) / width2, 0.0f, 1.0f)
                                   : (lengthAlong < 0.0f ? 1.0f : 0.0f);
        const float along = start + t * lengthAlong;
        const float depth = std::clamp(along, 0.8f, reach);
        if (along != depth) {
            // The closest point on the infinite line lies outside our finite sight segment.
            // A body whose centre is outside the range can still reach into it.
            const float length2 = zoo::Dot(axis, axis);
            t = length2 > 0.0f ? std::clamp(zoo::Dot(direction * depth - a, axis) / length2, 0.0f, 1.0f)
                               : 0.0f;
        }
        const float gap = std::max(0.0f, zoo::Length(a + axis * t - direction * depth) - radius);
        const GazeHit hit{gap / (0.20f + 0.012f * depth / magnification),
                          gap == 0.0f ? FirstBodyContact(a, axis, direction, radius, depth) : depth};
        if (hit.miss < 1.0f && (!best || hit < *best))
            best = hit;
    }
    return best;
}

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
