// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>

#include "MeshWriter.hpp"
#include "zoo/build/Catalog.hpp"
#include "zoo/world/World.hpp"

namespace zoogame {

/// What a fence looks like: its catalog entry, or the boundary wall.
struct FenceLook {
    zoo::FenceStyle style = zoo::FenceStyle::Mesh;
    float height = 2.0f;
    zoo::Color3 color{0.5f, 0.5f, 0.5f};
    zoo::Color3 postColor{0.3f, 0.3f, 0.3f};
    bool perimeter = false;
};

/// The look of a fence of the world; false when there is no such fence kind.
bool LookOf(const zoo::Catalog& catalog, zoo::Fence fence, FenceLook& out);

/// A post standing on `base`. Gate posts are stouter.
void BuildFencePost(ModelBuilder& model, const FenceLook& look, const Vector3& base, bool gatePost);

/// The panel between two ground points (the ends of a tile edge); it follows the
/// slope. With `gate` it is a gate leaf in the fence's style.
void BuildFencePanel(ModelBuilder& model, const FenceLook& look, const Vector3& a, const Vector3& b,
                     bool gate, std::uint32_t seed);

}  // namespace zoogame
