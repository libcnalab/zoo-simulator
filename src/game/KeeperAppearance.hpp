// SPDX-License-Identifier: MIT
#pragma once

#include <cstddef>

#include "Microsoft/Xna/Framework/GamerServices/AvatarDescription.hpp"

namespace zoogame {

/// One repeatable member of the zoo's four-person staff cast. Women and men alternate;
/// face, hair, build and height are generated from CNA's current catalog with a fixed
/// per-slot seed, so the same build does not recast the crew every time it starts.
Microsoft::Xna::Framework::GamerServices::AvatarDescription StaffAppearance(std::size_t slot);

}  // namespace zoogame
