// SPDX-License-Identifier: MIT
#pragma once

#include "zoo/animals/Animals.hpp"
#include "zoo/sim/Env.hpp"

namespace zoo::animal_internal {

void AdvanceMovement(Animal& animal, const SpeciesDef& species, const Env& env, int& pathCalls);

}  // namespace zoo::animal_internal
