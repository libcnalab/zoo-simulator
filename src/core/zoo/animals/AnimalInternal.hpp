// SPDX-License-Identifier: MIT
#pragma once

#include "zoo/animals/Animals.hpp"
#include "zoo/sim/Env.hpp"

#include <span>

namespace zoo::animal_internal {

void AdvanceMovement(Animal& animal, const SpeciesDef& species, const Env& env, int& pathCalls,
                     std::span<const Animal> animals, std::span<const std::size_t> neighbors,
                     std::vector<PropId>& propScratch);
void AdvanceNeeds(Animal& animal, const SpeciesDef& species, Env& env, const EnclosureStats* enclosure,
                  int viewers, const Animal* mother, bool predatorPresent);
void DecideActivity(Animal& animal, const SpeciesDef& species, Env& env, std::span<const Animal> animals,
                    std::span<const std::size_t> neighbors, Rng& rng, std::vector<PropId>& propScratch);
bool TryEscape(Animal& animal, const SpeciesDef& species, const Env& env, Rng& rng);
bool AdvanceHealth(Animal& animal, const SpeciesDef& species, const Env& env, DeathCause& cause);

}  // namespace zoo::animal_internal
