// SPDX-License-Identifier: MIT
#include "zoo/animals/AnimalInternal.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#include "zoo/animals/AnimalTuning.hpp"

namespace zoo::animal_internal {
namespace {

bool Available(const Animal& other, const Animal& animal) {
    return other.species == animal.species && other.home == animal.home && other.InWorld() && !other.loose &&
           other.health > 0.0f;
}

void Call(Animal& animal, Env& env, std::int64_t signal) {
    StartActivity(animal, AnimalActivity::Vocalize, animal.position, 3);
    animal.lastCallTick = env.time.tick;
    animal.answeredCallTick = signal;
    Event event{};
    event.type = EventType::AnimalCalled;
    event.animal = animal.id;
    event.enclosure = animal.home;
    event.position = animal.position;
    env.Emit(event);
}

const Animal* SentinelFor(const Animal& animal, const SpeciesDef& species, const Env& env,
                          std::span<const Animal> animals, std::span<const std::size_t> neighbors) {
    std::size_t eligible = 0;
    std::size_t adults = 0;
    for (std::size_t index : neighbors) {
        const Animal& other = animals[index];
        if (Available(other, animal) && IsAdult(other, species, env.time)) {
            ++adults;
        }
        if (Available(other, animal) && !other.sick && !other.shelter && IsAdult(other, species, env.time) &&
            other.hunger < 0.45f && other.thirst < 0.45f && other.activity != AnimalActivity::Eat &&
            other.activity != AnimalActivity::Drink) {
            ++eligible;
        }
    }
    if (eligible == 0 || adults < 2) {
        return nullptr;
    }
    const auto shift = static_cast<std::size_t>(env.time.tick / (45 * kTicksPerSecond));
    std::size_t chosen = shift % eligible;
    for (std::size_t index : neighbors) {
        const Animal& other = animals[index];
        if (Available(other, animal) && !other.sick && !other.shelter && IsAdult(other, species, env.time) &&
            other.hunger < 0.45f && other.thirst < 0.45f && other.activity != AnimalActivity::Eat &&
            other.activity != AnimalActivity::Drink) {
            if (chosen == 0) {
                return &other;
            }
            --chosen;
        }
    }
    return nullptr;
}

void LookAtInterest(Animal& animal, const Env& env, std::span<const Animal> animals,
                    std::span<const std::size_t> neighbors, int viewers) {
    if (animal.attentionLeft > 0) {
        --animal.attentionLeft;
    } else {
        const auto phase = static_cast<std::uint32_t>(env.time.tick / kTicksPerSecond);
        Vec2 interest = animal.position + HeadingVector(animal.heading + 0.5f);
        float best = std::numeric_limits<float>::max();
        for (std::size_t index : neighbors) {
            const Animal& other = animals[index];
            if (other.id == animal.id || !other.Visible()) {
                continue;
            }
            const float distance = DistanceSquared(animal.position, other.position);
            if (distance < best) {
                best = distance;
                interest = other.position;
            }
        }
        const Enclosure* enclosure = env.enclosures.Find(animal.home);
        if (enclosure != nullptr && viewers > 0 && !enclosure->viewing.empty() &&
            HashFloat(animal.id.value, phase) < 0.4f) {
            const auto index = HashU32(animal.id.value, phase) % enclosure->viewing.size();
            interest = World::TileCenter(enclosure->viewing[index].tile);
        }
        // A small varying offset makes a glance different even when the same
        // companion is still the most interesting thing in a quiet habitat.
        animal.attentionHeading =
            HeadingOf(interest - animal.position) + (HashFloat(animal.id.value, phase + 7U) - 0.5f) * 1.2f;
        animal.attentionLeft =
            (3 + static_cast<int>(HashU32(animal.id.value, phase + 9U) % 5U)) * kTicksPerSecond;
    }
    animal.heading = TurnTowards(animal.heading, animal.attentionHeading, 0.6f * kTickSeconds);
}

}  // namespace

const Animal* GroupLeader(const Animal& animal, const SpeciesDef& species, const Env& env,
                          std::span<const Animal> animals, std::span<const std::size_t> neighbors) {
    const Animal* fallback = nullptr;
    for (std::size_t index : neighbors) {
        const Animal& other = animals[index];
        if (!Available(other, animal) || other.shelter || !IsAdult(other, species, env.time)) {
            continue;
        }
        if (other.sex == Sex::Female) {
            return &other;
        }
        if (fallback == nullptr) {
            fallback = &other;
        }
    }
    return fallback;
}

void UpdateSpeciesBehaviour(Animal& animal, const SpeciesDef& species, Env& env,
                            std::span<const Animal> animals, std::span<const std::size_t> neighbors,
                            int viewers, Rng& rng, std::vector<PropId>& propScratch) {
    const BehaviourProfile* profile = ProfileFor(species);
    if (profile == nullptr || animal.loose || !animal.Visible()) {
        animal.following = {};
        return;
    }
    const bool asleep = animal.activity == AnimalActivity::Sleep;
    if (animal.speed < 0.001f && !asleep) {
        ++animal.stillTicks;
        if (!animal.targetProp) {
            LookAtInterest(animal, env, animals, neighbors, viewers);
        }
        // Long resting bouts stay resting, but visibly shift their attention.
        // This bound also covers repeated choices of the same stationary activity.
        if (animal.stillTicks >= static_cast<int>(profile->awakeStillSeconds * kTicksPerSecond)) {
            animal.attentionHeading = animal.heading + 0.35f;
            animal.attentionLeft = 2 * kTicksPerSecond;
            animal.heading = TurnTowards(animal.heading, animal.attentionHeading, 0.03f);
            animal.stillTicks = 0;
        }
    } else {
        animal.stillTicks = 0;
    }
    if ((env.time.tick + animal.id.value) % kTicksPerSecond != 0 ||
        animal.activity == AnimalActivity::BreakOut || animal.activity == AnimalActivity::Eat ||
        animal.activity == AnimalActivity::Drink || animal.activity == AnimalActivity::Shelter) {
        return;
    }
    const Enclosure* enclosure = env.enclosures.Find(animal.home);
    if (enclosure == nullptr) {
        return;
    }
    const float hour = env.time.Hour();
    const bool daylight = hour >= 6.0f && hour < 20.0f;
    if ((species.id == "meerkat" || species.id == "zebra") && daylight) {
        const Animal* sentinel = SentinelFor(animal, species, env, animals, neighbors);
        if (sentinel != nullptr && sentinel->id == animal.id && animal.hunger < 0.5f &&
            animal.thirst < 0.5f) {
            if (animal.activity != AnimalActivity::Sentinel) {
                AnimalActivity duty = AnimalActivity::Sentinel;
                const Vec2 place = RoutinePlace(animal, species, *profile, duty, *enclosure, env, animals,
                                                neighbors, propScratch, viewers);
                StartActivity(animal, duty, place, 45);
            }
            animal.activityLeft = std::max(animal.activityLeft, 2 * kTicksPerSecond);
            return;
        }
        if (animal.activity == AnimalActivity::Sentinel) {
            // Keep watch until the relief has actually taken up its position.
            if (sentinel != nullptr && sentinel->activity == AnimalActivity::Sentinel && !sentinel->moving) {
                animal.activityLeft = 0;
                animal.moving = false;
            } else {
                animal.activityLeft = std::max(animal.activityLeft, 2 * kTicksPerSecond);
            }
            return;
        }
    } else if (animal.activity == AnimalActivity::Sentinel) {
        animal.activityLeft = 0;
    }
    const Animal* leader = GroupLeader(animal, species, env, animals, neighbors);
    if ((species.id == "lion" || species.id == "wolf") && leader != nullptr && animal.hunger < 0.8f &&
        animal.thirst < 0.8f) {
        if (leader->id == animal.id && hour >= 17.0f && hour < 20.0f &&
            (animal.lastCallTick < 0 || env.time.tick - animal.lastCallTick > 6 * kTicksPerGameHour)) {
            Call(animal, env, env.time.tick);
            return;
        }
        const std::int64_t delay = static_cast<std::int64_t>(2U + animal.id.value % 4U) * kTicksPerSecond;
        if (leader->id != animal.id && leader->lastCallTick >= 0 &&
            leader->lastCallTick > animal.answeredCallTick && env.time.tick - leader->lastCallTick >= delay &&
            env.time.tick - leader->lastCallTick < 300) {
            Call(animal, env, leader->lastCallTick);
            return;
        }
    }
    const Animal* mother = nullptr;
    if (animal.mother && AgeYears(animal, env.time) < species.maturityYears * 0.5f) {
        for (std::size_t index : neighbors) {
            const Animal& other = animals[index];
            if (other.id == animal.mother && Available(other, animal) && other.Visible()) {
                mother = &other;
                break;
            }
        }
    }
    if (mother == nullptr) {
        animal.following = {};
    }
    if (mother != nullptr) {
        if (animal.activity == AnimalActivity::Play && animal.following &&
            DistanceSquared(animal.position, mother->position) < 36.0f) {
            return;
        }
        animal.following = mother->id;
        const float apart = species.bodyRadiusM * (BodyScale(animal, species, env.time) +
                                                   BodyScale(*mother, species, env.time)) +
                            0.3f;
        if (DistanceSquared(animal.position, mother->position) > (apart + 2.0f) * (apart + 2.0f) ||
            ((RestingActivity(mother->activity) || mother->activity == AnimalActivity::Sleep) &&
             !RestingActivity(animal.activity) && animal.activity != AnimalActivity::Sleep)) {
            AnimalActivity next =
                RestingActivity(mother->activity) || mother->activity == AnimalActivity::Sleep
                    ? AnimalActivity::Rest
                    : AnimalActivity::Wander;
            const Vec2 place = RoutinePlace(animal, species, *profile, next, *enclosure, env, animals,
                                            neighbors, propScratch, viewers);
            if (!animal.moving || DistanceSquared(place, animal.target) > 4.0f) {
                StartActivity(animal, next, place, ProfileDuration(*profile, next, rng));
            }
        }
    } else if (!asleep && leader != nullptr && leader->id != animal.id && profile->spreadM > 0.0f &&
               animal.hunger < tuning::kHungerUrgent && animal.thirst < tuning::kThirstUrgent) {
        const float limit = profile->spreadM * 1.5f + species.bodyRadiusM * 2.0f;
        if (DistanceSquared(animal.position, leader->position) > limit * limit &&
            (!animal.moving || DistanceSquared(animal.target, leader->position) > limit * limit)) {
            AnimalActivity next =
                RestingActivity(leader->activity) ? leader->activity : AnimalActivity::Wander;
            const Vec2 place = RoutinePlace(animal, species, *profile, next, *enclosure, env, animals,
                                            neighbors, propScratch, viewers);
            StartActivity(animal, next, place, ProfileDuration(*profile, next, rng));
        }
    }
}

}  // namespace zoo::animal_internal
