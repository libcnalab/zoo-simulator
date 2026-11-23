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
    {"przewalski_horse", 2, 12, 5, 1, A::Forage, A::Socialize, 7, 12, false},
    {"kiang", 3, 11, 6, 1, A::Forage, A::Idle, 8, 12, false},
    {"penguin", 3, 0, 7, 4, A::Forage, A::Preen, 3, 10, false},
    {"meerkat", 2, 8, 4, 5, A::Forage, A::Dig, 4, 8, false},
    {"flamingo", 3, 12, 2, 4, A::FilterFeed, A::Preen, 2, 12, false},
    {"brown_bear", 4, 9, 4, 3, A::Forage, A::Play, 0, 15, true},
    {"wolf", 10, 0, 5, 2, A::Forage, A::Patrol, 5, 12, true},
    {"dingo", 9, 0, 6, 3, A::Forage, A::Patrol, 5, 13, true},
    {"african_wild_dog", 6, 0, 8, 5, A::Forage, A::Socialize, 7, 12, true},
    {"dhole", 7, 0, 7, 5, A::Forage, A::Socialize, 7, 13, true},
    {"rhino", 5, 10, 3, 3, A::Forage, A::Bathe, 0, 18, true},
    {"camel", 4, 10, 4, 3, A::Forage, A::Ruminate, 7, 16, false},
    {"ostrich", 2, 11, 6, 2, A::Forage, A::DustBathe, 6, 10, false},
    {"kangaroo", 6, 10, 3, 1, A::Forage, A::Socialize, 5, 14, true},
    {"polar_bear", 4, 2, 5, 5, A::Forage, A::Bathe, 0, 15, false},
    {"lynx", 10, 0, 4, 2, A::Forage, A::Patrol, 0, 16, true},
    {"red_panda", 6, 8, 3, 1, A::Forage, A::Play, 0, 14, true},
    {"mandarin_duck", 4, 3, 6, 6, A::Forage, A::Bathe, 0, 11, false},
    {"corn_snake", 8, 0, 4, 1, A::Forage, A::Idle, 0, 18, false},
    {"black_headed_python", 12, 0, 3, 1, A::Forage, A::Idle, 0, 22, true},
    {"aldabra_tortoise", 8, 12, 5, 0, A::Forage, A::Idle, 0, 18, false},
    {"capybara", 5, 11, 4, 4, A::Forage, A::Bathe, 4, 12, true},
    {"snow_leopard", 12, 0, 4, 2, A::Forage, A::Patrol, 0, 18, true},
    {"addax", 3, 11, 5, 2, A::Forage, A::Ruminate, 6, 13, false},
    {"american_bison", 3, 13, 4, 2, A::Forage, A::Ruminate, 6, 14, false},
    {"arctic_fox", 7, 3, 6, 4, A::Forage, A::Dig, 0, 13, true},
    {"barbary_macaque", 4, 8, 5, 6, A::Forage, A::Socialize, 6, 15, false},
    {"black_tailed_prairie_dog", 3, 9, 5, 6, A::Forage, A::Dig, 5, 10, false},
    {"cheetah", 12, 0, 5, 2, A::Forage, A::Patrol, 6, 16, false},
    {"collared_peccary", 4, 10, 5, 4, A::Forage, A::Socialize, 4, 12, false},
    {"common_wombat", 9, 8, 4, 5, A::Forage, A::Dig, 0, 14, true},
    {"emu", 3, 11, 6, 3, A::Forage, A::DustBathe, 6, 11, false},
    {"european_bison", 3, 12, 4, 2, A::Forage, A::Ruminate, 6, 14, false},
    {"giant_anteater", 8, 9, 5, 5, A::Forage, A::Dig, 0, 14, true},
    {"guanaco", 3, 12, 5, 2, A::Forage, A::Ruminate, 6, 13, false},
    {"hippopotamus", 7, 8, 3, 6, A::Forage, A::Bathe, 0, 16, true},
    {"maned_wolf", 9, 3, 6, 3, A::Forage, A::Patrol, 0, 14, true},
    {"musk_ox", 4, 12, 4, 3, A::Forage, A::Ruminate, 6, 14, false},
    {"okapi", 5, 10, 3, 4, A::Browse, A::Ruminate, 6, 14, false},
    {"puma", 13, 0, 4, 2, A::Forage, A::Patrol, 0, 17, true},
    {"red_deer", 4, 11, 5, 3, A::Forage, A::Ruminate, 5, 13, true},
    {"reindeer", 3, 12, 5, 3, A::Forage, A::Ruminate, 5, 13, false},
    {"ring_tailed_lemur", 4, 9, 5, 6, A::Forage, A::Socialize, 6, 14, false},
    {"scimitar_horned_oryx", 3, 12, 5, 2, A::Forage, A::Ruminate, 6, 13, false},
    {"south_american_tapir", 7, 9, 4, 5, A::Browse, A::Bathe, 0, 14, true},
    {"southern_cassowary", 5, 10, 5, 3, A::Forage, A::DustBathe, 5, 12, false},
    {"spotted_hyena", 9, 2, 6, 4, A::Forage, A::Socialize, 0, 15, true},
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
