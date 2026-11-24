// SPDX-License-Identifier: MIT
#pragma once

#include "zoo/animals/AnimalProfile.hpp"
#include "zoo/animals/Animals.hpp"
#include "zoo/sim/Env.hpp"

#include <span>

namespace zoo::animal_internal {

struct NeedThresholds {
    float hunger;
    float thirst;
};
NeedThresholds ProvisionThresholds(const Animal& animal, const SpeciesDef& species, const Env& env,
                                   const Enclosure* home);

/// Horizontal clearance of the body while moving or turning, in metres.
float GirthRadius(float bodyRadius, const SpeciesDef& species);
void StartActivity(Animal& animal, AnimalActivity activity, Vec2 target, std::int32_t seconds,
                   PropId prop = {});
Vec2 RoutinePlace(const Animal& animal, const SpeciesDef& species, const BehaviourProfile& profile,
                  AnimalActivity& activity, const Enclosure& enclosure, const Env& env,
                  std::span<const Animal> animals, std::span<const std::size_t> neighbors,
                  std::vector<PropId>& propScratch, int viewers = 0);
void AdvanceMovement(Animal& animal, const SpeciesDef& species, const Env& env, int& pathCalls,
                     std::span<const Animal> animals, std::span<const std::size_t> neighbors,
                     std::vector<PropId>& propScratch, std::vector<NavObstacle>& navScratch,
                     std::span<const Vec2> people = {});
void AdvanceNeeds(Animal& animal, const SpeciesDef& species, Env& env, const EnclosureStats* enclosure,
                  int viewers, const Animal* mother, bool predatorPresent);
void DecideActivity(Animal& animal, const SpeciesDef& species, Env& env, std::span<const Animal> animals,
                    std::span<const std::size_t> neighbors, Rng& rng, std::vector<PropId>& propScratch,
                    int viewers = 0, bool feedingSoon = false);
void UpdateSpeciesBehaviour(Animal& animal, const SpeciesDef& species, Env& env,
                            std::span<const Animal> animals, std::span<const std::size_t> neighbors,
                            int viewers, Rng& rng, std::vector<PropId>& propScratch);
const Animal* GroupLeader(const Animal& animal, const SpeciesDef& species, const Env& env,
                          std::span<const Animal> animals, std::span<const std::size_t> neighbors);
void UpdateFlockRest(Animal& animal, const SpeciesDef& species, const Env& env,
                     std::span<const Animal> animals, std::span<const std::size_t> neighbors);
bool TryEscape(Animal& animal, const SpeciesDef& species, const Env& env, Rng& rng);
bool AdvanceHealth(Animal& animal, const SpeciesDef& species, const Env& env, DeathCause& cause);

}  // namespace zoo::animal_internal
