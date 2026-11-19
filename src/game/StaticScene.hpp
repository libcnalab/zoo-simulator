// SPDX-License-Identifier: MIT
#pragma once

#include "zoo/build/Objects.hpp"
#include "zoo/world/World.hpp"

namespace zoogame {

/// Whether two zoos need exactly the same immutable ground and scenery meshes.
///
/// A visit changes fence and building condition, sales counters and the contents of
/// feeders, troughs and bins. None of those values changes the model that is drawn, so a
/// saved visit may keep the already uploaded static scene. Edits to terrain, fences or the
/// placement of an object do change it and must force a rebuild.
inline bool SameStaticScene(const zoo::World& a, const zoo::ZooObjects& aObjects,
                            const zoo::World& b, const zoo::ZooObjects& bObjects) {
    if (a.Width() != b.Width() || a.Depth() != b.Depth() ||
        a.PropertyMin() != b.PropertyMin() || a.PropertyMax() != b.PropertyMax() ||
        a.EntranceTile() != b.EntranceTile())
        return false;

    for (int z = 0; z <= a.Depth(); ++z) {
        for (int x = 0; x <= a.Width(); ++x) {
            if (a.VertexHeight(x, z) != b.VertexHeight(x, z))
                return false;
        }
    }
    for (int z = 0; z < a.Depth(); ++z) {
        for (int x = 0; x < a.Width(); ++x) {
            const zoo::Tile& one = a.At({x, z});
            const zoo::Tile& two = b.At({x, z});
            if (one.terrain != two.terrain || one.path != two.path ||
                one.building != two.building || one.waterLevel != two.waterLevel ||
                one.standOff != two.standOff)
                return false;
        }
    }
    // Fence condition is deliberately absent: wear has no visual form.
    for (int z = 0; z <= a.Depth(); ++z) {
        for (int x = 0; x < a.Width(); ++x) {
            const zoo::Fence one = a.FenceAt({x, z, true});
            const zoo::Fence two = b.FenceAt({x, z, true});
            if (one.kind != two.kind || one.gate != two.gate)
                return false;
        }
    }
    for (int z = 0; z < a.Depth(); ++z) {
        for (int x = 0; x <= a.Width(); ++x) {
            const zoo::Fence one = a.FenceAt({x, z, false});
            const zoo::Fence two = b.FenceAt({x, z, false});
            if (one.kind != two.kind || one.gate != two.gate)
                return false;
        }
    }

    const auto& aBuildings = aObjects.Buildings();
    const auto& bBuildings = bObjects.Buildings();
    if (aBuildings.size() != bBuildings.size())
        return false;
    for (std::size_t i = 0; i < aBuildings.size(); ++i) {
        const zoo::Building& one = aBuildings[i];
        const zoo::Building& two = bBuildings[i];
        if (one.id != two.id || one.def != two.def || one.origin != two.origin ||
            one.facing != two.facing)
            return false;
    }

    const auto& aProps = aObjects.Props();
    const auto& bProps = bObjects.Props();
    if (aProps.size() != bProps.size())
        return false;
    for (std::size_t i = 0; i < aProps.size(); ++i) {
        const zoo::Prop& one = aProps[i];
        const zoo::Prop& two = bProps[i];
        if (one.id != two.id || one.def != two.def || one.position != two.position ||
            one.heading != two.heading || one.scale != two.scale)
            return false;
    }
    return true;
}

}  // namespace zoogame
