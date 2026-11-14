// SPDX-License-Identifier: MIT
#include "RockModels.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

#include "zoo/base/Rng.hpp"

namespace zoogame {

namespace {

constexpr float kPi = std::numbers::pi_v<float>;

/// One ring of corners round a crag.
struct CragRing {
    std::vector<Vector3> at;
    float height = 0.0f;  ///< the ring's mean height above the foot (the texture's v)
    float girth = 0.0f;   ///< the ring's length round the rock (the texture's u at the last corner)
};

}  // namespace

void Crag(MeshWriter& w, const Vector3& foot, const Vector3& radii, std::uint32_t seed,
          const CragShape& shape) {
    const int sides = std::max(4, shape.sides);
    const int tiers = std::max(1, shape.tiers);
    const auto count = static_cast<std::size_t>(sides);
    zoo::Rng rng(seed, 91);

    // The corners stand at the same bearings in every bed, so that a facet runs up the rock.
    std::vector<float> bearing(count);
    for (std::size_t i = 0; i < count; ++i) {
        bearing[i] =
            2.0f * kPi * (static_cast<float>(i) + rng.Range(-0.3f, 0.3f)) / static_cast<float>(sides);
    }
    // Beds are not all of one thickness: equal ones read as a stack of slabs.
    std::vector<float> level(static_cast<std::size_t>(tiers + 1), 0.0f);
    for (int t = 1; t <= tiers; ++t) {
        level[static_cast<std::size_t>(t)] = level[static_cast<std::size_t>(t - 1)] + rng.Range(0.55f, 1.45f);
    }
    for (float& l : level) {
        l *= radii.Y / level.back();
    }
    // The top of every bed: where the corners are, before the bed above is set back.
    std::vector<float> rise(count * static_cast<std::size_t>(tiers + 1));
    for (int t = 0; t <= tiers; ++t) {
        for (std::size_t i = 0; i < count; ++i) {
            const float bed = radii.Y / static_cast<float>(tiers);
            const float stray = t == 0 ? 0.0f : bed * rng.Range(-0.2f, 0.2f);
            rise[static_cast<std::size_t>(t) * count + i] =
                t == 0 ? -shape.sink : level[static_cast<std::size_t>(t)] + stray;
        }
    }
    const auto ring = [&](int level, float radius) {
        CragRing r;
        r.at.resize(count);
        float mean = 0.0f;
        for (std::size_t i = 0; i < count; ++i) {
            const float reach = radius * (1.0f + shape.rugged * rng.Range(-1.0f, 1.0f));
            const float angle = bearing[i] + rng.Range(-0.07f, 0.07f);
            const float x = std::cos(angle) * radii.X * reach;
            const float z = std::sin(angle) * radii.Z * reach;
            const float y = rise[static_cast<std::size_t>(level) * count + i];
            r.at[i] = foot + Vector3(x, y + shape.dip * x, z);
            mean += y;
        }
        r.height = mean / static_cast<float>(sides);
        r.girth = 2.0f * kPi * radius * std::max(radii.X, radii.Z);
        return r;
    };
    const auto radiusAt = [&](float f) { return 1.0f - (1.0f - shape.taper) * std::pow(f, 1.3f); };

    CragRing lower = ring(0, 1.0f);
    for (int t = 0; t < tiers; ++t) {
        const float f0 = static_cast<float>(t) / static_cast<float>(tiers);
        const float f1 = static_cast<float>(t + 1) / static_cast<float>(tiers);
        // The face of a bed leans in a little; the rest of the way to the next bed is a ledge.
        const float r0 = radiusAt(f0);
        const float r1 = radiusAt(f1);
        const CragRing upper = ring(t + 1, r0 - (r0 - r1) * rng.Range(0.3f, 0.8f));
        for (std::size_t i = 0; i < count; ++i) {
            const std::size_t j = (i + 1) % count;
            const float u0 = lower.girth * static_cast<float>(i) / static_cast<float>(sides);
            const float u1 = lower.girth * static_cast<float>(i + 1) / static_cast<float>(sides);
            w.Picture(lower.at[j], lower.at[i], upper.at[i], upper.at[j], u1, -upper.height, u0,
                      -lower.height);
        }
        if (t + 1 == tiers) {
            // The top: a low point somewhere on it.
            Vector3 middle(0.0f, 0.0f, 0.0f);
            for (const Vector3& p : upper.at) {
                middle += p;
            }
            // Off the middle and well above the rim, or the rock ends in a table.
            middle = middle / static_cast<float>(sides) + Vector3(radii.X * rng.Range(-0.2f, 0.2f),
                                                                  radii.Y * rng.Range(0.08f, 0.2f),
                                                                  radii.Z * rng.Range(-0.2f, 0.2f));
            for (std::size_t i = 0; i < count; ++i) {
                w.Tri(upper.at[(i + 1) % count], upper.at[i], middle);
            }
            break;
        }
        CragRing next = ring(t + 1, r1);
        for (std::size_t i = 0; i < count; ++i) {
            const std::size_t j = (i + 1) % count;
            w.Quad(upper.at[j], upper.at[i], next.at[i], next.at[j]);
        }
        lower = std::move(next);
    }
}

}  // namespace zoogame
