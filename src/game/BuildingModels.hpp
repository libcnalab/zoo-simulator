// SPDX-License-Identifier: MIT
#pragma once

#include <optional>

#include "CnaEngine/Collision/StaticWorld.hpp"
#include "MeshWriter.hpp"
#include "zoo/build/Catalog.hpp"

namespace zoogame {

/// Writes the model of a building into `model`, in the building's own frame: the
/// footprint (BuildingDef::width x depth tiles) centred on the origin, y = 0 the
/// levelled ground, the door in the +z side (a building facing south). The
/// caller sets the builder's transform; the builder's local transform is used
/// and cleared.
///
/// The shape follows BuildingDef::look ("house", "barn", "hut", "kiosk",
/// "pavilion", "cave"), the size and the colours; unknown looks give a house.
void BuildBuilding(ModelBuilder& model, const zoo::BuildingDef& def);

/// Opaque surfaces in a building's own coordinates, shared by every placement of its design.
class BuildingSight {
public:
    explicit BuildingSight(const zoo::BuildingDef& def);
    std::optional<float> Distance(const Microsoft::Xna::Framework::Ray& localRay, float reach) const;

private:
    Microsoft::Xna::Framework::BoundingBox bounds_;
    CnaEngine::Collision::StaticWorld solids_{1.5f};
};

/// The zoo's entrance: two brick pillars carrying a roofed sign board over a
/// gateway `width` metres wide, the gate leaves swung open inwards. In its own
/// frame: the gateway along x, centred on the origin, the outside towards +z.
void BuildEntranceArch(ModelBuilder& model, float width);

/// A separately operated sliding gate at an animal house: welded mesh inside a
/// steel frame, in its own frame with the doorway centred on x = 0 and +z outside.
/// Its bottom is at y = 0.08 and the caller moves the whole leaf sideways to open it.
void BuildHoldingGate(ModelBuilder& model);

/// The fixed overhead rail along which BuildHoldingGate travels.
void BuildHoldingGateGuides(ModelBuilder& model);

}  // namespace zoogame
