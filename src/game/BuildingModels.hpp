// SPDX-License-Identifier: MIT
#pragma once

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

/// The zoo's entrance: two brick pillars carrying a roofed sign board over a
/// gateway `width` metres wide, the gate leaves swung open inwards. In its own
/// frame: the gateway along x, centred on the origin, the outside towards +z.
void BuildEntranceArch(ModelBuilder& model, float width);

}  // namespace zoogame
