// SPDX-License-Identifier: MIT
#include "zoo/animals/AnimalProfile.hpp"

#include <array>
#include <cmath>

namespace zoo::animal_internal {
namespace {
using A = AnimalActivity;
// Relative decision weights: rest, feed, walk, signature. Duration also affects
// the resulting time budget, which is measured in AnimalEthogramTests.
constexpr BehaviourProfile profiles[] = {
    {"lion", 18, 0, 2, 1, A::Forage, A::Socialize, 5, 18, true},
    {"tiger", 12, 0, 3, 1, A::Forage, A::Bathe, 0, 18, true},
    {"elephant", 1, 10, 7, 2, A::Forage, A::DustBathe, 7, 14, false},
    {"giraffe", 3, 9, 3, 4, A::Browse, A::Ruminate, 8, 16, false},
    {"zebra", 2, 12, 4, 1, A::Forage, A::Idle, 7, 12, false},
    {"penguin", 3, 0, 7, 4, A::Forage, A::Preen, 3, 10, false},
    {"meerkat", 2, 8, 4, 5, A::Forage, A::Dig, 4, 8, false},
    {"flamingo", 3, 12, 2, 4, A::FilterFeed, A::Preen, 2, 12, false},
    {"brown_bear", 4, 9, 4, 3, A::Forage, A::Play, 0, 15, true},
    {"wolf", 10, 0, 5, 2, A::Forage, A::Patrol, 5, 12, true},
    {"rhino", 5, 10, 3, 3, A::Forage, A::Bathe, 0, 18, true},
    {"camel", 4, 10, 4, 3, A::Forage, A::Ruminate, 7, 16, false},
    {"ostrich", 2, 11, 6, 2, A::Forage, A::DustBathe, 6, 10, false},
    {"kangaroo", 6, 10, 3, 1, A::Forage, A::Socialize, 5, 14, true},
    {"polar_bear", 4, 2, 5, 5, A::Forage, A::Bathe, 0, 15, false},
    {"lynx", 10, 0, 4, 2, A::Forage, A::Patrol, 0, 16, true},
    {"red_panda", 6, 8, 3, 1, A::Forage, A::Play, 0, 14, true},
    {"capybara", 5, 11, 4, 4, A::Forage, A::Bathe, 4, 12, true},
    {"snow_leopard", 12, 0, 4, 2, A::Forage, A::Patrol, 0, 18, true},
};
}  // namespace

const BehaviourProfile* ProfileFor(const SpeciesDef& species) {
    for (const auto& profile : profiles) {
        if (profile.id == species.id) {
            return &profile;
        }
    }
    return nullptr;
}

bool FeedingActivity(AnimalActivity activity) {
    return activity == A::Forage || activity == A::Browse || activity == A::FilterFeed || activity == A::Dig;
}

bool RestingActivity(AnimalActivity activity) {
    return activity == A::Rest || activity == A::Ruminate || activity == A::Huddle;
}

AnimalActivity ProfileActivity(const Animal& animal, const SpeciesDef& species,
                               const BehaviourProfile& profile, float hour, float roll) {
    const bool twilight = (hour >= 5.0f && hour < 8.0f) || (hour >= 17.0f && hour < 21.0f);
    const bool midday = hour >= 11.0f && hour < 16.0f;
    const float active = profile.twilight ? (twilight ? 2.4f : (midday ? 0.45f : 0.8f)) : 1.0f;
    const float rest = profile.rest * (midday ? 1.5f : 1.0f) / active;
    const std::array<A, 7> choices{A::Rest, profile.feeding, A::Wander,   profile.signature,
                                   A::Idle, A::Play,         A::Socialize};
    const std::array<float, 7> weights{
        rest,
        profile.feed * active,
        profile.walk * active,
        (animal.sick && profile.signature == A::Play ? 0.0f : profile.special * active),
        0.3f,
        animal.sick ? 0.0f : species.playfulness * 0.4f,
        species.sociability * 0.5f};
    float total = 0.0f;
    for (float weight : weights) {
        total += weight;
    }
    float remaining = roll * total;
    for (std::size_t index = 0; index < choices.size(); ++index) {
        if (remaining < weights[index]) {
            if (choices[index] == A::Rest && (species.id == "penguin" || species.id == "flamingo")) {
                return A::Huddle;
            }
            if (choices[index] == A::Rest && species.id == "giraffe") {
                return A::Ruminate;
            }
            return choices[index];
        }
        remaining -= weights[index];
    }
    return A::Wander;
}

std::int32_t ProfileDuration(const BehaviourProfile& profile, AnimalActivity activity, Rng& rng) {
    // Most bouts are short, with a bounded tail. Independent draws avoid a
    // fixed sequence and allow consecutive feeding or resting bouts.
    const float draw = rng.NextFloat();
    const float tail = draw * draw * draw;
    if (RestingActivity(activity)) {
        return static_cast<std::int32_t>(20.0f + 80.0f * tail);
    }
    if (activity == A::Vocalize) {
        return 3;
    }
    if (activity == A::Wander || activity == A::Patrol) {
        return 2;
    }
    return static_cast<std::int32_t>(4.0f + profile.awakeStillSeconds * tail);
}

}  // namespace zoo::animal_internal
