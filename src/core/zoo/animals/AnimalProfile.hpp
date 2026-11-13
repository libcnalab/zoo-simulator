// SPDX-License-Identifier: MIT
#pragma once

#include "zoo/animals/Animals.hpp"

namespace zoo::animal_internal {

// Internal tuning, deliberately independent of the species JSON loader. Unknown ids
// retain the legacy routine. All figures are design estimates, not field measurements.
struct BehaviourProfile {
    std::string_view id;
    float rest;
    float feed;
    float walk;
    float special;
    AnimalActivity feeding;
    AnimalActivity signature;
    float spreadM;
    float awakeStillSeconds;
    bool twilight;
};

const BehaviourProfile* ProfileFor(const SpeciesDef& species);
AnimalActivity ProfileActivity(const Animal& animal, const SpeciesDef& species,
                               const BehaviourProfile& profile, float hour, float roll);
std::int32_t ProfileDuration(const BehaviourProfile& profile, AnimalActivity activity, Rng& rng);
bool FeedingActivity(AnimalActivity activity);
bool RestingActivity(AnimalActivity activity);

}  // namespace zoo::animal_internal
