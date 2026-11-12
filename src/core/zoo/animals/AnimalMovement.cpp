// SPDX-License-Identifier: MIT
#include "zoo/animals/AnimalInternal.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#include "zoo/animals/AnimalTuning.hpp"

namespace zoo::animal_internal {
namespace {

float Approach(float current, float target, float amount) {
    if (current < target) {
        return std::min(target, current + amount);
    }
    return std::max(target, current - amount);
}

}  // namespace

void AdvanceMovement(Animal& animal, const SpeciesDef& species, const Env& env, int& pathCalls,
                     std::span<const Animal> animals, std::span<const std::size_t> neighbors,
                     std::vector<PropId>& propScratch) {
    if (!animal.InWorld() || animal.shelter) {
        animal.speed = 0.0f;
        return;
    }
    const float bodyScale = BodyScale(animal, species, env.time);
    const TileCoord currentTile = World::TileAt(animal.position);
    const NavProfile profile = NavProfile::Animal(
        tuning::kWadingShoulderFraction * species.shoulderHeightM * bodyScale, species.canSwim);
    if (!env.nav.Passable(currentTile, profile)) {
        const Region region = env.enclosures.RegionAt(currentTile);
        const EnclosureId enclosure = env.enclosures.At(currentTile);
        float nearest = std::numeric_limits<float>::max();
        Vec2 relocated{};
        bool found = false;
        for (int z = 0; z < env.world.Depth(); ++z) {
            for (int x = 0; x < env.world.Width(); ++x) {
                const TileCoord candidate{x, z};
                if (!env.nav.Passable(candidate, profile) || env.enclosures.RegionAt(candidate) != region ||
                    env.enclosures.At(candidate) != enclosure) {
                    continue;
                }
                const Vec2 center = World::TileCenter(candidate);
                const float distance = DistanceSquared(animal.position, center);
                if (distance < nearest) {
                    nearest = distance;
                    relocated = center;
                    found = true;
                }
            }
        }
        if (found) {
            animal.position = relocated;
            animal.target = relocated;
            animal.moving = false;
            animal.speed = 0.0f;
            animal.route.clear();
            animal.routeIndex = 0;
            animal.activityLeft = 0;
        }
    }
    const float speedScale =
        tuning::kYoungSpeedScale + (tuning::kAdultSpeedScale - tuning::kYoungSpeedScale) * bodyScale;
    const bool running = animal.activity == AnimalActivity::BreakOut ||
                         animal.activity == AnimalActivity::Loose ||
                         (animal.activity == AnimalActivity::Play && !animal.targetProp);
    const float maxSpeed = (running ? species.runSpeed : species.walkSpeed) * speedScale;
    if (!animal.moving) {
        animal.moving = false;
        animal.route.clear();
        animal.routeIndex = 0;
        animal.stallBest = -1.0f;
        animal.stallTicks = 0;
        animal.onDetour = false;
        animal.detours = 0;
        if (animal.targetProp) {
            const Prop* prop = env.objects.FindProp(animal.targetProp);
            if (prop != nullptr) {
                animal.heading = TurnTowards(animal.heading, HeadingOf(prop->position - animal.position),
                                             tuning::kTurnRadiansPerSecond * tuning::kMotionSecondsPerTick);
            }
        }
        animal.speed = 0.0f;
        return;
    }
    // An errand that takes for ever is one that cannot be done: think again.
    if (animal.activityTicks > tuning::kWalkGiveUpTicks && animal.activity != AnimalActivity::BreakOut) {
        animal.moving = false;
        animal.speed = 0.0f;
        animal.activityLeft = 0;
        animal.route.clear();
        animal.routeIndex = 0;
        return;
    }
    if (animal.route.empty()) {
        animal.stallBest = -1.0f;
        animal.stallTicks = 0;
        animal.onDetour = false;
        animal.detours = 0;
    }
    if (animal.route.empty() && animal.position != animal.target &&
        animal.activity == AnimalActivity::BreakOut) {
        animal.route.push_back(animal.target);
        animal.routeIndex = 0;
    } else if (animal.route.empty() && animal.position != animal.target) {
        if (env.nav.Walkable(animal.position, animal.target, profile)) {
            animal.route.push_back(animal.target);
        } else {
            if (pathCalls >= tuning::kPathCallsPerStep) {
                animal.moving = false;
                animal.route.clear();
                animal.routeIndex = 0;
                animal.activityLeft = 1;
                animal.speed =
                    Approach(animal.speed, 0.0f, tuning::kBrakeAcceleration * tuning::kMotionSecondsPerTick);
                return;
            }
            ++pathCalls;
            if (!env.nav.FindPath(animal.position, animal.target, profile, animal.route,
                                  tuning::kPathExpansionLimit)) {
                animal.moving = false;
                animal.route.clear();
                animal.routeIndex = 0;
                animal.speed = 0.0f;
                animal.activityLeft = 0;
                return;
            }
            animal.routeIndex = 0;
        }
    }
    if (animal.route.empty() || animal.routeIndex >= animal.route.size()) {
        animal.moving = false;
        animal.speed = 0.0f;
        animal.activityLeft = std::max(
            animal.activityLeft, static_cast<std::int32_t>(tuning::kArrivalPauseSeconds * kTicksPerSecond));
        return;
    }
    const Vec2 delta = animal.route[animal.routeIndex] - animal.position;
    const float distance = Length(delta);
    const bool finalWaypoint = animal.routeIndex + 1U >= animal.route.size();
    const float oneTickBraking = tuning::kBrakeAcceleration * tuning::kMotionSecondsPerTick;
    if (distance <= tuning::kWaypointArrivalM && (!finalWaypoint || animal.speed <= oneTickBraking)) {
        ++animal.routeIndex;
        animal.stallBest = -1.0f;
        animal.stallTicks = 0;
        animal.onDetour = false;
        if (animal.routeIndex >= animal.route.size()) {
            animal.moving = false;
            animal.speed = 0.0f;
            animal.activityLeft =
                std::max(animal.activityLeft,
                         static_cast<std::int32_t>(tuning::kArrivalPauseSeconds * kTicksPerSecond));
            return;
        }
    }
    const Vec2 nextDelta = animal.route[animal.routeIndex] - animal.position;
    const float heading = HeadingOf(nextDelta);
    animal.heading =
        TurnTowards(animal.heading, heading, tuning::kTurnRadiansPerSecond * tuning::kMotionSecondsPerTick);
    const float remaining = Length(nextDelta);
    // It slows down for where it is going, not for every corner on the way.
    const bool lastLeg = animal.routeIndex + 1U >= animal.route.size();
    const float desiredSpeed =
        lastLeg ? std::min(maxSpeed, std::sqrt(std::max(0.0f, tuning::kRouteBrakingFactor *
                                                                  tuning::kBrakeAcceleration * remaining)))
                : maxSpeed;
    const float acceleration =
        desiredSpeed < animal.speed ? tuning::kBrakeAcceleration : tuning::kWalkAcceleration;
    animal.speed = Approach(animal.speed, desiredSpeed, acceleration * tuning::kMotionSecondsPerTick);
    const float distanceThisTick = std::min(animal.speed * tuning::kMotionSecondsPerTick, remaining);
    const float bodyRadius = species.bodyRadiusM * bodyScale;
    // Between trees it is the width of the body that counts, not its length.
    const float girth = bodyRadius * tuning::kBodyWidthFraction;
    const Vec2 planned = Normalized(nextDelta) * distanceThisTick;
    Vec2 step = planned;
    // Sliding along what stands in the way. Only what the step comes closer to counts: an
    // animal that already overlaps another (three leave a shelter by the same door) or stands
    // inside a rock's clearance must be free to walk out of it, or it never moves again.
    const auto slide = [&](Vec2 obstacle, float clearance) {
        if (DistanceSquared(animal.position + step, obstacle) >= clearance * clearance) {
            return;
        }
        const Vec2 away = animal.position - obstacle;
        const float gap = Length(away);
        const Vec2 radial = gap > 1.0e-4f ? away * (1.0f / gap) : Normalized(planned) * -1.0f;
        const float inward = Dot(step, radial);
        if (inward >= 0.0f) {
            return;
        }
        step = step - radial * inward;
        if (LengthSquared(step) <
            tuning::kHeadOnFraction * tuning::kHeadOnFraction * distanceThisTick * distanceThisTick) {
            // Head on: round it, always by the same hand, so that two animals that meet pass
            // each other instead of mirroring each other's sidesteps.
            step = Vec2{radial.z, -radial.x} * distanceThisTick;
        }
    };
    // Whether the step ends nearer to the obstacle than it began, inside its clearance.
    const auto runsInto = [&](Vec2 obstacle, float clearance) {
        const float after = DistanceSquared(animal.position + step, obstacle);
        return after < clearance * clearance &&
               after < DistanceSquared(animal.position, obstacle) - tuning::kIntrusionToleranceM2;
    };
    const auto blocker = [&](PropId propId, float& clearance) -> const Prop* {
        // What the animal is walking up to (its feeder, its trough) is not in its way.
        if (propId == animal.targetProp) {
            return nullptr;
        }
        const Prop* prop = env.objects.FindProp(propId);
        if (prop == nullptr || prop->def < 0 ||
            static_cast<std::size_t>(prop->def) >= env.catalog.props.size()) {
            return nullptr;
        }
        const PropDef& definition = env.catalog.props[static_cast<std::size_t>(prop->def)];
        if (!definition.blocksWalking) {
            return nullptr;
        }
        clearance = definition.radiusM + girth + tuning::kObstacleClearanceM;
        return prop;
    };
    env.objects.PropsNear(animal.position,
                          bodyRadius + tuning::kObstacleClearanceM + tuning::kObstacleSearchRadiusM,
                          propScratch);
    for (PropId propId : propScratch) {
        float clearance = 0.0f;
        if (const Prop* prop = blocker(propId, clearance)) {
            slide(prop->position, clearance);
        }
    }
    bool crowded = false;
    for (int pass = 0; pass < 2; ++pass) {
        for (std::size_t otherIndex : neighbors) {
            const Animal& other = animals[otherIndex];
            if (other.id == animal.id || !other.InWorld() || other.shelter) {
                continue;
            }
            const SpeciesDef& otherSpecies = env.species.All()[static_cast<std::size_t>(other.species)];
            const float otherRadius = otherSpecies.bodyRadiusM * BodyScale(other, otherSpecies, env.time);
            const float clearance = bodyRadius + otherRadius + tuning::kAnimalClearanceM;
            // Arriving beside a companion (two at one trough) is not a collision.
            if (finalWaypoint && animal.targetProp && remaining < clearance) {
                continue;
            }
            if (pass == 0) {
                slide(other.position, clearance);
            } else if (runsInto(other.position, clearance)) {
                crowded = true;
            }
        }
    }
    if (crowded) {
        // Hemmed in by its companions: squeeze past slowly rather than wait for ever.
        step = planned * tuning::kSqueezePastFactor;
    }
    for (PropId propId : propScratch) {
        float clearance = 0.0f;
        const Prop* prop = blocker(propId, clearance);
        if (prop != nullptr && runsInto(prop->position, clearance)) {
            step = {};
            break;
        }
    }
    if (animal.activity != AnimalActivity::BreakOut && step != Vec2{} &&
        !env.nav.Walkable(animal.position, animal.position + step, profile)) {
        if (step == planned) {
            // The route itself is shut (a fence went up): think again.
            animal.speed = 0.0f;
            animal.moving = false;
            animal.activityLeft = 0;
            animal.route.clear();
            animal.routeIndex = 0;
            return;
        }
        step = {};
    }
    animal.speed = Length(step) / tuning::kMotionSecondsPerTick;
    animal.position = animal.position + step;

    // Not getting any nearer: what is aimed at cannot be reached this way.
    const float left = Length(animal.route[animal.routeIndex] - animal.position);
    if (animal.stallBest < 0.0f || left < animal.stallBest - tuning::kStallProgressM) {
        animal.stallBest = left;
        animal.stallTicks = 0;
        return;
    }
    if (++animal.stallTicks <= tuning::kStallTicks) {
        return;
    }
    animal.stallBest = -1.0f;
    animal.stallTicks = 0;
    if (left < tuning::kStallArriveM + bodyRadius) {
        // Near enough (a tree stands on the very spot).
        ++animal.routeIndex;
        animal.onDetour = false;
        if (animal.routeIndex >= animal.route.size()) {
            animal.moving = false;
            animal.speed = 0.0f;
            animal.route.clear();
            animal.routeIndex = 0;
            animal.activityLeft =
                std::max(animal.activityLeft,
                         static_cast<std::int32_t>(tuning::kArrivalPauseSeconds * kTicksPerSecond));
        }
        return;
    }
    // Caught in a pocket (between a log pile and a tree): walk out of it sideways and come
    // at the goal from elsewhere, each try further round than the one before.
    if (animal.onDetour) {
        animal.route.erase(animal.route.begin() + static_cast<std::ptrdiff_t>(animal.routeIndex));
        animal.onDetour = false;
    }
    if (animal.detours >= tuning::kDetourLimit || animal.routeIndex >= animal.route.size()) {
        animal.speed = 0.0f;
        animal.moving = false;
        animal.activityLeft = 0;
        animal.route.clear();
        animal.routeIndex = 0;
        return;
    }
    const Vec2 ahead = Normalized(animal.route[animal.routeIndex] - animal.position);
    const std::uint32_t pick = animal.id.value * 2654435761U + static_cast<std::uint32_t>(env.time.tick);
    for (std::uint32_t attempt = 0; attempt < tuning::kDetourAttempts; ++attempt) {
        const float side = ((pick + attempt) & 1U) != 0U ? 1.0f : -1.0f;
        const float angle =
            side * (tuning::kDetourAngle +
                    tuning::kDetourAngleStep * static_cast<float>(animal.detours + attempt / 2U));
        const float reach = tuning::kDetourReachM + static_cast<float>(((pick >> 1U) + attempt) % 3U);
        const Vec2 direction = ahead * std::cos(angle) + Vec2{ahead.z, -ahead.x} * std::sin(angle);
        const Vec2 detour = animal.position + direction * reach;
        if (!env.nav.Walkable(animal.position, detour, profile)) {
            continue;
        }
        bool clear = true;
        env.objects.PropsNear(
            detour, bodyRadius + tuning::kObstacleClearanceM + tuning::kObstacleSearchRadiusM, propScratch);
        for (PropId propId : propScratch) {
            float clearance = 0.0f;
            const Prop* prop = blocker(propId, clearance);
            if (prop != nullptr && DistanceSquared(detour, prop->position) < clearance * clearance) {
                clear = false;
                break;
            }
        }
        if (clear) {
            animal.route.insert(animal.route.begin() + static_cast<std::ptrdiff_t>(animal.routeIndex),
                                detour);
            animal.onDetour = true;
            ++animal.detours;
            return;
        }
    }
    animal.speed = 0.0f;
    animal.moving = false;
    animal.activityLeft = 0;
    animal.route.clear();
    animal.routeIndex = 0;
}

}  // namespace zoo::animal_internal
