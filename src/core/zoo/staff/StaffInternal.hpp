// SPDX-License-Identifier: MIT
#pragma once

#include "zoo/staff/Staff.hpp"

namespace zoo::staff_detail {

/// How staff walk: anywhere dry, through keeper gates, preferring paths.
inline constexpr NavProfile kStaffProfile = NavProfile::Staff();

/// Does a dangerous animal live in this habitat, or stand in it right now?
bool HoldsDanger(const Env& env, const Animals& animals, EnclosureId enclosure);

/// True only while this habitat has a transient lockout and every dangerous resident is hidden.
bool DangerSecuredForStaff(const Env& env, const Animals& animals, EnclosureId enclosure);

/// Ground staff can stand on and walk to from the entrance.
bool Reachable(const Env& env, TileCoord tile);

}  // namespace zoo::staff_detail
