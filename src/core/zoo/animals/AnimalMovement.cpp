// SPDX-License-Identifier: MIT
#include "zoo/animals/AnimalInternal.hpp"

#include <algorithm>
#include <cmath>

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

void AdvanceMovement(Animal& animal, const SpeciesDef& species, const Env& env, int& pathCalls) {
    if (!animal.InWorld() || animal.shelter) {
        animal.speed = 0.0f;
        return;
    }
    const float bodyScale = BodyScale(animal, species, env.time);
    const float speedScale =
        tuning::kYoungSpeedScale + (tuning::kAdultSpeedScale - tuning::kYoungSpeedScale) * bodyScale;
    const bool running =
        animal.activity == AnimalActivity::BreakOut || animal.activity == AnimalActivity::Loose;
    const float maxSpeed = (running ? species.runSpeed : species.walkSpeed) * speedScale;
    if (!animal.moving) {
        animal.moving = false;
        animal.route.clear();
        animal.routeIndex = 0;
        animal.speed =
            Approach(animal.speed, 0.0f, tuning::kBrakeAcceleration * tuning::kMotionSecondsPerTick);
        return;
    }
    const float wadeDepth = tuning::kWadingShoulderFraction * species.shoulderHeightM * bodyScale;
    const NavProfile profile = NavProfile::Animal(wadeDepth, species.canSwim);
    if (animal.route.empty() && animal.position != animal.target) {
        if (env.nav.Walkable(animal.position, animal.target, profile)) {
            animal.route.push_back(animal.target);
        } else {
            if (pathCalls >= tuning::kPathCallsPerStep) {
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
    if (distance <= tuning::kWaypointArrivalM) {
        ++animal.routeIndex;
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
    const float desiredSpeed = std::min(
        maxSpeed,
        std::sqrt(std::max(0.0f, tuning::kRouteBrakingFactor * tuning::kBrakeAcceleration * remaining)));
    const float acceleration =
        desiredSpeed < animal.speed ? tuning::kBrakeAcceleration : tuning::kWalkAcceleration;
    animal.speed = Approach(animal.speed, desiredSpeed, acceleration * tuning::kMotionSecondsPerTick);
    const float distanceThisTick = std::min(animal.speed * tuning::kMotionSecondsPerTick, remaining);
    const Vec2 nextPosition = animal.position + Normalized(nextDelta) * distanceThisTick;
    if (env.nav.Walkable(animal.position, nextPosition, profile)) {
        animal.position = nextPosition;
    } else {
        animal.speed = 0.0f;
    }
}

}  // namespace zoo::animal_internal
