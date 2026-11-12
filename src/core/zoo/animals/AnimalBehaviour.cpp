// SPDX-License-Identifier: MIT
#include "zoo/animals/AnimalInternal.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

#include "zoo/animals/AnimalTuning.hpp"

namespace zoo::animal_internal {
namespace {

void Start(Animal& animal, AnimalActivity activity, Vec2 target, std::int32_t seconds, PropId prop = {}) {
    if (activity == AnimalActivity::Play) {
        animal.boredom = std::max(0.0f, animal.boredom - tuning::kBoredomPlayRelief);
    } else if (activity == AnimalActivity::Socialize) {
        animal.boredom = std::max(0.0f, animal.boredom - tuning::kBoredomSocialRelief);
    } else if (activity == AnimalActivity::Wander) {
        animal.boredom = std::max(0.0f, animal.boredom - tuning::kBoredomWanderRelief);
    }
    animal.activity = activity;
    animal.target = target;
    animal.targetProp = prop;
    animal.activityLeft = seconds * kTicksPerSecond;
    animal.activityTicks = 0;
    animal.moving = DistanceSquared(animal.position, target) > tuning::kMovementStartDistanceSquaredM2;
    animal.route.clear();
    animal.routeIndex = 0;
}

bool IsRepeatBlocked(AnimalActivity current, AnimalActivity next) {
    return current == next && next != AnimalActivity::Wander && next != AnimalActivity::Idle;
}

float CircadianBias(ActivityPattern pattern, float hour) {
    const float fullCycle = 2.0f * kPi / tuning::kCircadianDayHours;
    const float twiceDailyCycle = 2.0f * fullCycle;
    if (pattern == ActivityPattern::Nocturnal) {
        return tuning::kCircadianBiasMidpoint +
               tuning::kCircadianBiasAmplitude * std::cos((hour - tuning::kCircadianMidday) * fullCycle);
    }
    if (pattern == ActivityPattern::Crepuscular) {
        return tuning::kCircadianBiasMidpoint -
               tuning::kCircadianBiasAmplitude * std::cos((hour - tuning::kDawnStartHour) * twiceDailyCycle);
    }
    if (pattern == ActivityPattern::Cathemeral) {
        return tuning::kCircadianBiasMidpoint +
               tuning::kCircadianBiasAmplitude * std::cos(hour * twiceDailyCycle);
    }
    return tuning::kCircadianBiasMidpoint -
           tuning::kCircadianBiasAmplitude * std::cos((hour - tuning::kCircadianMidday) * fullCycle);
}

AnimalActivity ChooseRoutineActivity(const Animal& animal, const SpeciesDef& species, float hour, Rng& rng) {
    std::array<AnimalActivity, 8> choices{
        AnimalActivity::Wander, AnimalActivity::Idle,      AnimalActivity::Rest,     AnimalActivity::Forage,
        AnimalActivity::Play,   AnimalActivity::Socialize, AnimalActivity::Vocalize, AnimalActivity::Wander};
    const bool vocalPeak = (hour >= tuning::kDawnStartHour && hour <= tuning::kDawnEndHour) ||
                           (hour >= tuning::kDuskStartHour && hour <= tuning::kDuskEndHour);
    const float vocalizeWeight =
        tuning::kVocalizeWeight * (1.0f + (vocalPeak ? tuning::kVocalizePeakMultiplier : 0.0f) +
                                   animal.hunger * tuning::kVocalizeHungerMultiplier);
    std::array<float, 8> weights{
        tuning::kWanderWeight,
        tuning::kIdleWeight,
        tuning::kRestBaseWeight + animal.fatigue * tuning::kRestFatigueWeight,
        species.diet == Diet::Carnivore || species.diet == Diet::Piscivore ? 0.0f : tuning::kForageWeight,
        !animal.sick && animal.boredom > tuning::kPlayNeedThreshold
            ? species.playfulness * tuning::kPlayWeight
            : 0.0f,
        species.sociability * tuning::kSocializeWeight,
        vocalizeWeight,
        0.0f};
    for (std::size_t index = 0; index < choices.size(); ++index) {
        if (IsRepeatBlocked(animal.activity, choices[index])) {
            weights[index] = 0.0f;
        }
    }
    float totalWeight = 0.0f;
    for (float weight : weights) {
        totalWeight += weight;
    }
    float roll = rng.Range(0.0f, totalWeight);
    for (std::size_t index = 0; index < choices.size(); ++index) {
        if (roll < weights[index]) {
            return choices[index];
        }
        roll -= weights[index];
    }
    return AnimalActivity::Wander;
}

std::int32_t RandomDuration(Rng& rng, float minimumSeconds, float maximumSeconds) {
    return static_cast<std::int32_t>(rng.Range(minimumSeconds, maximumSeconds));
}

bool DestinationClear(Vec2 point, float bodyRadius, const EnclosureId enclosureId, const Env& env,
                      const SpeciesDef& species, std::vector<PropId>& propIds) {
    const TileCoord tile = World::TileAt(point);
    if (!env.world.InBounds(tile) || env.enclosures.At(tile) != enclosureId ||
        !env.nav.Passable(tile, NavProfile::Animal(0.0f, species.canSwim))) {
        return false;
    }
    constexpr Dir sides[] = {Dir::North, Dir::East, Dir::South, Dir::West};
    for (Dir side : sides) {
        if (env.world.FenceOn(tile, side).Exists() &&
            bodyRadius + tuning::kFenceBodyClearanceM > tuning::kTileCenterFenceClearanceM) {
            return false;
        }
    }
    env.objects.PropsNear(point, bodyRadius + tuning::kDestinationProbeM, propIds);
    for (PropId propId : propIds) {
        const Prop* prop = env.objects.FindProp(propId);
        if (prop == nullptr || prop->def < 0 ||
            static_cast<std::size_t>(prop->def) >= env.catalog.props.size()) {
            continue;
        }
        const PropDef& definition = env.catalog.props[static_cast<std::size_t>(prop->def)];
        const float clearance = definition.radiusM + bodyRadius + tuning::kObstacleClearanceM;
        if (definition.blocksWalking && DistanceSquared(point, prop->position) < clearance * clearance) {
            return false;
        }
    }
    return true;
}

bool PropHasRoom(PropId propId, std::span<const Animal> animals, std::span<const std::size_t> neighbors) {
    int occupants = 0;
    for (std::size_t animalIndex : neighbors) {
        if (animals[animalIndex].targetProp == propId) {
            ++occupants;
        }
    }
    return occupants < static_cast<int>(tuning::kFeederShareCount);
}

int PropSlot(const Animal& animal, const SpeciesDef& species, const Env& env, const Prop& prop,
             std::span<const Animal> animals, std::span<const std::size_t> neighbors) {
    constexpr std::size_t slotCount = static_cast<std::size_t>(tuning::kFeederShareCount);
    std::array<bool, slotCount> used{};
    float propRadius = tuning::kDefaultPropRadiusM;
    if (prop.def >= 0 && static_cast<std::size_t>(prop.def) < env.catalog.props.size()) {
        propRadius = env.catalog.props[static_cast<std::size_t>(prop.def)].radiusM;
    }
    const float radius =
        propRadius + species.bodyRadiusM * BodyScale(animal, species, env.time) + tuning::kObstacleClearanceM;
    const auto nearestSlot = [prop, radius](Vec2 target) {
        int bestSlot = 0;
        float bestDistance = std::numeric_limits<float>::max();
        for (int slotIndex = 0; slotIndex < static_cast<int>(tuning::kFeederShareCount); ++slotIndex) {
            const float angle = static_cast<float>(slotIndex) * (kPi * 0.5f);
            const Vec2 point = prop.position + Vec2{std::sin(angle), std::cos(angle)} * radius;
            const float distance = DistanceSquared(target, point);
            if (distance < bestDistance) {
                bestDistance = distance;
                bestSlot = slotIndex;
            }
        }
        return bestSlot;
    };
    for (std::size_t neighborIndex : neighbors) {
        const Animal& other = animals[neighborIndex];
        if (other.targetProp != prop.id) {
            continue;
        }
        const int slot = nearestSlot(other.target);
        if (other.id == animal.id) {
            return slot;
        }
        used[static_cast<std::size_t>(slot)] = true;
    }
    int bestSlot = 0;
    float bestSeparation = -1.0f;
    float bestDistance = std::numeric_limits<float>::max();
    for (int slotIndex = 0; slotIndex < static_cast<int>(tuning::kFeederShareCount); ++slotIndex) {
        if (used[static_cast<std::size_t>(slotIndex)]) {
            continue;
        }
        const float angle = static_cast<float>(slotIndex) * (kPi * 0.5f);
        const Vec2 point = prop.position + Vec2{std::sin(angle), std::cos(angle)} * radius;
        const float distance = DistanceSquared(animal.position, point);
        float separation = std::numeric_limits<float>::max();
        for (int usedSlot = 0; usedSlot < static_cast<int>(slotCount); ++usedSlot) {
            if (!used[static_cast<std::size_t>(usedSlot)]) {
                continue;
            }
            const float usedAngle = static_cast<float>(usedSlot) * (kPi * 0.5f);
            const Vec2 usedPoint = prop.position + Vec2{std::sin(usedAngle), std::cos(usedAngle)} * radius;
            separation = std::min(separation, DistanceSquared(point, usedPoint));
        }
        if (separation > bestSeparation || (separation == bestSeparation && distance < bestDistance)) {
            bestSeparation = separation;
            bestDistance = distance;
            bestSlot = slotIndex;
        }
    }
    return bestSlot;
}

bool ShelterHasRoom(const Animal& animal, BuildingId buildingId, Vec2 front, const BuildingDef& definition,
                    std::span<const Animal> animals, std::span<const std::size_t> neighbors) {
    int occupants = 0;
    for (std::size_t otherIndex : neighbors) {
        const Animal& other = animals[otherIndex];
        if (other.id == animal.id) {
            continue;
        }
        if (other.shelter == buildingId ||
            ((other.activity == AnimalActivity::Shelter || other.activity == AnimalActivity::Sleep) &&
             DistanceSquared(other.target, front) <= tuning::kWaypointArrivalM * tuning::kWaypointArrivalM)) {
            ++occupants;
        }
    }
    return occupants < definition.capacity;
}

Vec2 PropApproach(const SpeciesDef& species, const Env& env, const Prop& prop, int slot) {
    float propRadius = tuning::kDefaultPropRadiusM;
    if (prop.def >= 0 && static_cast<std::size_t>(prop.def) < env.catalog.props.size()) {
        propRadius = env.catalog.props[static_cast<std::size_t>(prop.def)].radiusM;
    }
    const float radius = propRadius + species.bodyRadiusM + tuning::kObstacleClearanceM;
    const float angle = static_cast<float>(slot) * (kPi * 0.5f);
    return prop.position + Vec2{std::sin(angle), std::cos(angle)} * radius;
}

bool TryPaceLoop(Animal& animal, const SpeciesDef& species, const Enclosure& enclosure, const Env& env,
                 Rng& rng, std::vector<PropId>& propScratch, TileCoord firstTile, int side, int minX,
                 int maxX, int minZ, int maxZ) {
    const bool verticalBoundary = side < 2;
    const int minimumAxis = verticalBoundary ? minZ + 1 : minX + 1;
    const int maximumAxis = verticalBoundary ? maxZ - 1 : maxX - 1;
    const int firstAxis = verticalBoundary ? firstTile.z : firstTile.x;
    const int direction = rng.RangeInt(0, 1) == 0 ? -1 : 1;
    const int offset = static_cast<int>(
        std::round(rng.Range(tuning::kPaceOffsetMinimumM, tuning::kPaceOffsetMaximumM) / kTileSize));
    int secondAxis = std::clamp(firstAxis + direction * offset, minimumAxis, maximumAxis);
    if (secondAxis == firstAxis) {
        secondAxis = std::clamp(firstAxis - direction * offset, minimumAxis, maximumAxis);
    }
    if (secondAxis == firstAxis) {
        return false;
    }
    const TileCoord secondTile = side == 0   ? TileCoord{minX + 1, secondAxis}
                                 : side == 1 ? TileCoord{maxX - 1, secondAxis}
                                 : side == 2 ? TileCoord{secondAxis, minZ + 1}
                                             : TileCoord{secondAxis, maxZ - 1};
    const Vec2 first = World::TileCenter(firstTile);
    const Vec2 second = World::TileCenter(secondTile);
    const float bodyRadius = species.bodyRadiusM * BodyScale(animal, species, env.time);
    if (!DestinationClear(first, bodyRadius, enclosure.id, env, species, propScratch) ||
        !DestinationClear(second, bodyRadius, enclosure.id, env, species, propScratch)) {
        return false;
    }
    const float wadeDepth =
        tuning::kWadingShoulderFraction * species.shoulderHeightM * BodyScale(animal, species, env.time);
    const NavProfile profile = NavProfile::Animal(wadeDepth, species.canSwim);
    if (!env.nav.Walkable(animal.position, first, profile) || !env.nav.Walkable(first, second, profile) ||
        !env.nav.Walkable(second, first, profile)) {
        return false;
    }
    Start(animal, AnimalActivity::Pace, first,
          RandomDuration(rng, tuning::kPaceMinimumSeconds, tuning::kPaceMaximumSeconds));
    animal.route.push_back(first);
    animal.route.push_back(second);
    animal.route.push_back(first);
    animal.routeIndex = 0;
    animal.moving = true;
    return true;
}

}  // namespace

void DecideActivity(Animal& animal, const SpeciesDef& species, Env& env, std::span<const Animal> animals,
                    std::span<const std::size_t> neighbors, Rng& rng, std::vector<PropId>& propScratch) {
    const EnclosureId enclosureId = env.enclosures.At(animal.position);
    const Enclosure* enclosure = env.enclosures.Find(enclosureId);
    if (!enclosureId) {
        animal.loose = true;
        if (animal.thirst > tuning::kThirstUrgent) {
            float nearest = std::numeric_limits<float>::max();
            Vec2 shore{};
            bool foundShore = false;
            const NavProfile profile = NavProfile::Animal(0.0f, species.canSwim);
            constexpr Dir sides[] = {Dir::North, Dir::East, Dir::South, Dir::West};
            for (int z = 0; z < env.world.Depth(); ++z) {
                for (int x = 0; x < env.world.Width(); ++x) {
                    const TileCoord waterTile{x, z};
                    if (!env.world.HasWater(waterTile)) {
                        continue;
                    }
                    for (Dir side : sides) {
                        const TileCoord dryTile = waterTile + Step(side);
                        if (!env.world.InBounds(dryTile) ||
                            env.enclosures.RegionAt(dryTile) != Region::Public ||
                            env.world.HasWater(dryTile) || !env.nav.Passable(dryTile, profile)) {
                            continue;
                        }
                        const Vec2 candidate = World::TileCenter(dryTile);
                        const float distance = DistanceSquared(animal.position, candidate);
                        if (distance < nearest) {
                            nearest = distance;
                            shore = candidate;
                            foundShore = true;
                        }
                    }
                }
            }
            if (foundShore) {
                Start(animal, AnimalActivity::Drink, shore,
                      RandomDuration(rng, tuning::kDrinkMinimumSeconds, tuning::kDrinkMaximumSeconds));
                return;
            }
        }
        const NavProfile profile = NavProfile::Animal(0.0f, species.canSwim);
        const float minX = std::max(0.0f, animal.position.x - tuning::kLooseRoamRadiusM);
        const float maxX = std::min(env.world.SizeX(), animal.position.x + tuning::kLooseRoamRadiusM);
        const float minZ = std::max(0.0f, animal.position.z - tuning::kLooseRoamRadiusM);
        const float maxZ = std::min(env.world.SizeZ(), animal.position.z + tuning::kLooseRoamRadiusM);
        for (int attempt = 0; attempt < tuning::kDestinationAttempts; ++attempt) {
            const Vec2 target{rng.Range(minX, maxX), rng.Range(minZ, maxZ)};
            const TileCoord tile = World::TileAt(target);
            if (env.world.InBounds(tile) && env.enclosures.RegionAt(tile) == Region::Public &&
                env.nav.Passable(tile, profile)) {
                Start(animal, AnimalActivity::Loose, target,
                      RandomDuration(rng, tuning::kIdleMinimumSeconds, tuning::kIdleMaximumSeconds));
                return;
            }
        }
        Start(animal, AnimalActivity::Idle, animal.position,
              RandomDuration(rng, tuning::kIdleMinimumSeconds, tuning::kIdleMaximumSeconds));
        return;
    }
    animal.loose = false;
    animal.home = enclosureId;
    const float hour = env.time.Hour();
    const bool night = hour >= tuning::kNightBeginHour || hour < tuning::kNightEndHour;
    const float circadian = CircadianBias(species.activity, hour);

    if (animal.shelter) {
        const Building* currentShelter = env.objects.FindBuilding(animal.shelter);
        const bool pressed =
            animal.hunger > tuning::kUrgentNeedSleep || animal.thirst > tuning::kUrgentNeedSleep;
        // It stays in to sleep its sleep out, or while the weather that drove it in lasts.
        const bool asleep =
            animal.activity == AnimalActivity::Sleep &&
            !(animal.fatigue < tuning::kSleepWakeFatigue && circadian < tuning::kSleepWakeCircadian);
        const float excess = std::max(
            {species.tempMinC - env.weather.temperatureC, env.weather.temperatureC - species.tempMaxC, 0.0f});
        const bool weatherShelter = env.weather.rain > tuning::kRainLeaveThreshold || env.weather.Snowing() ||
                                    excess > tuning::kShelterLeaveTemperatureC;
        if (currentShelter == nullptr || pressed || !(asleep || weatherShelter)) {
            animal.shelter = {};
            animal.activity = AnimalActivity::Idle;
            animal.activityLeft = 0;
            animal.moving = false;
            animal.route.clear();
            animal.routeIndex = 0;
        } else {
            return;
        }
    }

    const float drinkUrgency =
        animal.activity == AnimalActivity::Sleep ? tuning::kUrgentNeedSleep : tuning::kThirstUrgent;
    const float eatUrgency =
        animal.activity == AnimalActivity::Sleep ? tuning::kUrgentNeedSleep : tuning::kHungerUrgent;
    if (animal.thirst > drinkUrgency && animal.activity != AnimalActivity::Drink) {
        float nearest = std::numeric_limits<float>::max();
        PropId chosen{};
        for (PropId id : enclosure->troughs) {
            const Prop* prop = env.objects.FindProp(id);
            if (prop == nullptr || prop->amount <= 0.0f || !PropHasRoom(id, animals, neighbors)) {
                continue;
            }
            const float distance = DistanceSquared(animal.position, prop->position);
            if (distance < nearest) {
                nearest = distance;
                chosen = id;
            }
        }
        if (chosen) {
            const Prop* prop = env.objects.FindProp(chosen);
            if (prop != nullptr) {
                Start(animal, AnimalActivity::Drink,
                      PropApproach(species, env, *prop,
                                   PropSlot(animal, species, env, *prop, animals, neighbors)),
                      RandomDuration(rng, tuning::kDrinkMinimumSeconds, tuning::kDrinkMaximumSeconds),
                      chosen);
                return;
            }
        }
        float shoreDistance = std::numeric_limits<float>::max();
        Vec2 shore{};
        bool foundShore = false;
        const NavProfile profile = NavProfile::Animal(0.0f, species.canSwim);
        for (int z = enclosure->min.z; z <= enclosure->max.z; ++z) {
            for (int x = enclosure->min.x; x <= enclosure->max.x; ++x) {
                const TileCoord waterTile{x, z};
                if (!env.world.HasWater(waterTile) || env.enclosures.At(waterTile) != enclosureId) {
                    continue;
                }
                constexpr Dir sides[] = {Dir::North, Dir::East, Dir::South, Dir::West};
                for (Dir side : sides) {
                    const TileCoord dryTile = waterTile + Step(side);
                    if (!env.world.InBounds(dryTile) || env.world.HasWater(dryTile) ||
                        env.enclosures.At(dryTile) != enclosureId || !env.nav.Passable(dryTile, profile)) {
                        continue;
                    }
                    const Vec2 candidate = World::TileCenter(dryTile);
                    const float distance = DistanceSquared(animal.position, candidate);
                    if (distance < shoreDistance) {
                        shoreDistance = distance;
                        shore = candidate;
                        foundShore = true;
                    }
                }
            }
        }
        if (foundShore) {
            Start(animal, AnimalActivity::Drink, shore,
                  RandomDuration(rng, tuning::kDrinkMinimumSeconds, tuning::kDrinkMaximumSeconds));
            return;
        }
    }
    if (animal.hunger > eatUrgency && animal.activity != AnimalActivity::Eat) {
        float nearest = std::numeric_limits<float>::max();
        PropId chosen{};
        for (PropId id : enclosure->feeders) {
            const Prop* prop = env.objects.FindProp(id);
            if (prop == nullptr || !PropHasRoom(id, animals, neighbors) ||
                Animals::EdibleKg(env, *prop) <= 0.0f || prop->food < 0 ||
                static_cast<std::size_t>(prop->food) >= env.catalog.foods.size()) {
                continue;
            }
            const std::string& food = env.catalog.foods[static_cast<std::size_t>(prop->food)].id;
            if (std::find(species.foods.begin(), species.foods.end(), food) == species.foods.end()) {
                continue;
            }
            const float distance = DistanceSquared(animal.position, prop->position);
            if (distance < nearest) {
                nearest = distance;
                chosen = id;
            }
        }
        if (chosen) {
            const Prop* prop = env.objects.FindProp(chosen);
            if (prop != nullptr) {
                Start(animal, AnimalActivity::Eat,
                      PropApproach(species, env, *prop,
                                   PropSlot(animal, species, env, *prop, animals, neighbors)),
                      RandomDuration(rng, tuning::kMealMinimumSeconds, tuning::kMealMaximumSeconds), chosen);
                return;
            }
        }
    }

    const float temperatureExcess = std::max(
        {species.tempMinC - env.weather.temperatureC, env.weather.temperatureC - species.tempMaxC, 0.0f});
    const bool needsShelter = temperatureExcess > tuning::kShelterDecisionTemperatureC ||
                              env.weather.rain > tuning::kRainShelterThreshold || env.weather.Snowing();
    if (needsShelter) {
        for (BuildingId buildingId : enclosure->shelters) {
            const Building* building = env.objects.FindBuilding(buildingId);
            if (building == nullptr || !building->open || building->def < 0 ||
                static_cast<std::size_t>(building->def) >= env.catalog.buildings.size()) {
                continue;
            }
            const BuildingDef& definition = env.catalog.buildings[static_cast<std::size_t>(building->def)];
            const bool helps = temperatureExcess <= tuning::kShelterDecisionTemperatureC ||
                               env.weather.temperatureC < species.tempMinC || definition.cooled;
            if (definition.role != BuildingRole::AnimalShelter || !helps) {
                continue;
            }
            const TileCoord front = FrontTile(definition, building->origin, building->facing);
            const Vec2 target = World::TileCenter(front);
            if (env.enclosures.At(target) == enclosureId &&
                env.nav.Passable(front, NavProfile::Animal(0.0f, species.canSwim)) &&
                ShelterHasRoom(animal, buildingId, target, definition, animals, neighbors)) {
                Start(animal, AnimalActivity::Shelter, target,
                      static_cast<std::int32_t>(tuning::kRoutineRecheckSeconds));
                return;
            }
        }
        if (env.weather.temperatureC > species.tempMaxC + tuning::kShelterDecisionTemperatureC) {
            const float bodyScale = BodyScale(animal, species, env.time);
            const float bodyRadius = species.bodyRadiusM * bodyScale;
            const float wadeDepth = tuning::kWadingShoulderFraction * species.shoulderHeightM * bodyScale;
            const NavProfile profile = NavProfile::Animal(wadeDepth, species.canSwim);
            float nearestDistance = std::numeric_limits<float>::max();
            Vec2 coolSpot{};
            bool foundCoolSpot = false;
            for (int z = enclosure->min.z; z <= enclosure->max.z; ++z) {
                for (int x = enclosure->min.x; x <= enclosure->max.x; ++x) {
                    const TileCoord tile{x, z};
                    if (!env.world.HasWater(tile) || env.enclosures.At(tile) != enclosureId ||
                        !env.nav.Passable(tile, profile)) {
                        continue;
                    }
                    const Vec2 candidate = World::TileCenter(tile);
                    const float depth = env.world.WaterDepthAt(candidate);
                    if (depth <= 0.0f || depth > wadeDepth) {
                        continue;
                    }
                    const float distance = DistanceSquared(animal.position, candidate);
                    if (distance < nearestDistance) {
                        nearestDistance = distance;
                        coolSpot = candidate;
                        foundCoolSpot = true;
                    }
                }
            }
            for (const Prop& prop : env.objects.Props()) {
                const TileCoord propTile = World::TileAt(prop.position);
                if (propTile.x < enclosure->min.x || propTile.x > enclosure->max.x ||
                    propTile.z < enclosure->min.z || propTile.z > enclosure->max.z || prop.def < 0 ||
                    static_cast<std::size_t>(prop.def) >= env.catalog.props.size()) {
                    continue;
                }
                const PropDef& definition = env.catalog.props[static_cast<std::size_t>(prop.def)];
                if (definition.role != PropRole::Tree) {
                    continue;
                }
                const float radius = definition.radiusM + bodyRadius + tuning::kObstacleClearanceM;
                for (int pointIndex = 0; pointIndex < tuning::kTreeCoolingSamples; ++pointIndex) {
                    const float angle = static_cast<float>(pointIndex) *
                                        (2.0f * kPi / static_cast<float>(tuning::kTreeCoolingSamples));
                    const Vec2 candidate = prop.position + Vec2{std::sin(angle), std::cos(angle)} * radius;
                    if (!DestinationClear(candidate, bodyRadius, enclosureId, env, species, propScratch)) {
                        continue;
                    }
                    const float distance = DistanceSquared(animal.position, candidate);
                    if (distance < nearestDistance) {
                        nearestDistance = distance;
                        coolSpot = candidate;
                        foundCoolSpot = true;
                    }
                }
            }
            if (foundCoolSpot) {
                Start(animal, AnimalActivity::Shelter, coolSpot,
                      static_cast<std::int32_t>(tuning::kRoutineRecheckSeconds));
                return;
            }
        }
    }

    const bool wakeFromSleep =
        animal.activity == AnimalActivity::Sleep &&
        ((animal.fatigue < tuning::kSleepWakeFatigue && circadian < 0.0f) ||
         animal.hunger > tuning::kUrgentNeedSleep || animal.thirst > tuning::kUrgentNeedSleep);
    if (wakeFromSleep) {
        Start(animal, AnimalActivity::Idle, animal.position,
              static_cast<std::int32_t>(tuning::kIdleMinimumSeconds));
        return;
    }
    if (animal.activity == AnimalActivity::Sleep) {
        Start(animal, AnimalActivity::Sleep, animal.position,
              static_cast<std::int32_t>(tuning::kSleepActivitySeconds));
        return;
    } else if (animal.fatigue + circadian >= tuning::kSleepDecisionThreshold &&
               animal.activity != AnimalActivity::Sleep) {
        if (species.needsShelter && night) {
            for (BuildingId buildingId : enclosure->shelters) {
                const Building* building = env.objects.FindBuilding(buildingId);
                if (building == nullptr || !building->open || building->def < 0 ||
                    static_cast<std::size_t>(building->def) >= env.catalog.buildings.size()) {
                    continue;
                }
                const BuildingDef& definition =
                    env.catalog.buildings[static_cast<std::size_t>(building->def)];
                const TileCoord front = FrontTile(definition, building->origin, building->facing);
                const Vec2 target = World::TileCenter(front);
                if (definition.role == BuildingRole::AnimalShelter &&
                    env.enclosures.At(front) == enclosureId &&
                    env.nav.Passable(front, NavProfile::Animal(0.0f, species.canSwim)) &&
                    ShelterHasRoom(animal, buildingId, target, definition, animals, neighbors)) {
                    Start(animal, AnimalActivity::Sleep, target,
                          static_cast<std::int32_t>(tuning::kRoutineRecheckSeconds));
                    return;
                }
            }
        }
        Start(animal, AnimalActivity::Sleep, animal.position,
              static_cast<std::int32_t>(tuning::kRoutineRecheckSeconds));
        return;
    }

    if (animal.stress > tuning::kPaceStressThreshold && animal.activity != AnimalActivity::Pace &&
        rng.Chance(tuning::kPaceDecisionChance)) {
        const int minX = std::max(enclosure->min.x, 0);
        const int maxX = std::min(enclosure->max.x, env.world.Width() - 1);
        const int minZ = std::max(enclosure->min.z, 0);
        const int maxZ = std::min(enclosure->max.z, env.world.Depth() - 1);
        for (int attempt = 0; attempt < tuning::kDestinationAttempts; ++attempt) {
            const int side = rng.RangeInt(0, tuning::kPacingSides - 1);
            const int coordinate =
                side < 2 ? rng.RangeInt(minX + 1, maxX - 1) : rng.RangeInt(minZ + 1, maxZ - 1);
            const TileCoord tile = side == 0   ? TileCoord{minX + 1, coordinate}
                                   : side == 1 ? TileCoord{maxX - 1, coordinate}
                                   : side == 2 ? TileCoord{coordinate, minZ + 1}
                                               : TileCoord{coordinate, maxZ - 1};
            if (TryPaceLoop(animal, species, *enclosure, env, rng, propScratch, tile, side, minX, maxX, minZ,
                            maxZ)) {
                return;
            }
        }
        Start(animal, AnimalActivity::Pace, animal.position,
              RandomDuration(rng, tuning::kPaceMinimumSeconds, tuning::kPaceMaximumSeconds));
        return;
    }

    const AnimalActivity next = ChooseRoutineActivity(animal, species, hour, rng);
    if (next == AnimalActivity::Play && !enclosure->enrichments.empty()) {
        const Prop* toy = env.objects.FindProp(enclosure->enrichments.front());
        if (toy != nullptr) {
            const PropId propId = enclosure->enrichments.front();
            Start(animal, next,
                  PropApproach(species, env, *toy, PropSlot(animal, species, env, *toy, animals, neighbors)),
                  RandomDuration(rng, tuning::kPlayMinimumSeconds, tuning::kPlayMaximumSeconds), propId);
            return;
        }
    }
    if (next == AnimalActivity::Socialize) {
        // To the nearest companion in sight, and beside it rather than onto it.
        const Animal* companion = nullptr;
        float nearest = std::numeric_limits<float>::max();
        for (std::size_t otherIndex : neighbors) {
            const Animal& other = animals[otherIndex];
            if (other.id == animal.id || other.species != animal.species || !other.Visible() ||
                env.enclosures.At(other.position) != enclosureId) {
                continue;
            }
            const float distance = DistanceSquared(animal.position, other.position);
            if (distance < nearest) {
                nearest = distance;
                companion = &other;
            }
        }
        if (companion != nullptr) {
            const float apart = species.bodyRadiusM * (BodyScale(animal, species, env.time) +
                                                       BodyScale(*companion, species, env.time)) +
                                tuning::kSocialGapM;
            const Vec2 offset = animal.position - companion->position;
            const Vec2 beside =
                companion->position +
                (LengthSquared(offset) > 1.0e-6f ? Normalized(offset) : Vec2{1.0f, 0.0f}) * apart;
            Start(animal, next, nearest > apart * apart ? beside : animal.position,
                  RandomDuration(rng, tuning::kSocializeMinimumSeconds, tuning::kSocializeMaximumSeconds));
            return;
        }
    }
    const int minX = std::max(enclosure->min.x, 0);
    const int maxX = std::min(enclosure->max.x, env.world.Width() - 1);
    const int minZ = std::max(enclosure->min.z, 0);
    const int maxZ = std::min(enclosure->max.z, env.world.Depth() - 1);
    Vec2 target = animal.position;
    const float bodyRadius = species.bodyRadiusM * BodyScale(animal, species, env.time);
    const Animal* mother = nullptr;
    if (animal.mother &&
        AgeYears(animal, env.time) < species.maturityYears * tuning::kJuvenileFollowFraction) {
        for (std::size_t otherIndex : neighbors) {
            const Animal& other = animals[otherIndex];
            if (other.id == animal.mother && other.health > 0.0f) {
                mother = &other;
                break;
            }
        }
    }
    for (int attempt = 0; attempt < tuning::kDestinationAttempts; ++attempt) {
        const TileCoord tile{rng.RangeInt(minX, maxX), rng.RangeInt(minZ, maxZ)};
        const Vec2 candidate = World::TileCenter(tile);
        const std::size_t terrainIndex = static_cast<std::size_t>(env.world.At(tile).terrain);
        float destinationPreference = species.terrainSuitability[terrainIndex];
        if (species.sociability > 0.0f) {
            float nearestGroupDistance = std::numeric_limits<float>::max();
            for (std::size_t otherIndex : neighbors) {
                const Animal& other = animals[otherIndex];
                if (other.id == animal.id || other.species != animal.species || !other.InWorld()) {
                    continue;
                }
                nearestGroupDistance =
                    std::min(nearestGroupDistance, DistanceSquared(candidate, other.position));
            }
            if (nearestGroupDistance < std::numeric_limits<float>::max()) {
                const float proximity =
                    Clamp01(1.0f - std::sqrt(nearestGroupDistance) / tuning::kSocialWanderRadiusM);
                const float groupPreference = tuning::kSocialWanderMinimumPreference +
                                              (1.0f - tuning::kSocialWanderMinimumPreference) * proximity;
                destinationPreference *= 1.0f - species.sociability * (1.0f - groupPreference);
            }
        }
        if (env.world.HasWater(tile) || destinationPreference <= 0.0f ||
            rng.NextFloat() > destinationPreference) {
            continue;
        }
        if (mother != nullptr && DistanceSquared(candidate, mother->position) >
                                     tuning::kMotherFollowRadiusM * tuning::kMotherFollowRadiusM) {
            continue;
        }
        if (DestinationClear(candidate, bodyRadius, enclosureId, env, species, propScratch)) {
            target = candidate;
            break;
        }
    }
    const std::int32_t duration =
        next == AnimalActivity::Wander
            ? RandomDuration(rng, tuning::kWanderMinimumSeconds, tuning::kWanderMaximumSeconds)
        : next == AnimalActivity::Rest
            ? RandomDuration(rng, tuning::kRestMinimumSeconds, tuning::kRestMaximumSeconds)
        : next == AnimalActivity::Forage
            ? RandomDuration(rng, tuning::kForageMinimumSeconds, tuning::kForageMaximumSeconds)
        : next == AnimalActivity::Vocalize ? static_cast<std::int32_t>(tuning::kVocalizeDurationSeconds)
        : next == AnimalActivity::Idle
            ? RandomDuration(rng, tuning::kIdleMinimumSeconds, tuning::kIdleMaximumSeconds)
            : RandomDuration(rng, tuning::kPlayMinimumSeconds, tuning::kPlayMaximumSeconds);
    Start(animal, next, target, duration);
    if (next == AnimalActivity::Vocalize) {
        Event event{};
        event.type = EventType::AnimalCalled;
        event.animal = animal.id;
        event.enclosure = enclosureId;
        event.position = animal.position;
        env.Emit(event);
    }
}

bool TryEscape(Animal& animal, const SpeciesDef& species, const Env& env, Rng& rng) {
    const EnclosureId enclosureId = env.enclosures.At(animal.position);
    const Enclosure* enclosure = env.enclosures.Find(enclosureId);
    if (enclosure == nullptr || animal.activity == AnimalActivity::BreakOut ||
        animal.activity == AnimalActivity::Crated) {
        return false;
    }
    const float containment = Containment(species, enclosure->stats);
    if (containment >= 1.0f) {
        return false;
    }
    const float probability =
        (1.0f - containment) * (tuning::kEscapeBaseChance + tuning::kEscapeStressFactor * animal.stress +
                                tuning::kEscapeWelfareFactor * (1.0f - animal.welfare));
    if (!rng.Chance(Clamp01(probability))) {
        return false;
    }
    const EdgeRef edge = enclosure->weakestEdge;
    TileCoord insideA{};
    TileCoord insideB{};
    if (edge.horizontal) {
        insideA = {edge.x, edge.z - 1};
        insideB = {edge.x, edge.z};
    } else {
        insideA = {edge.x - 1, edge.z};
        insideB = {edge.x, edge.z};
    }
    const TileCoord inside = env.enclosures.At(insideA) == enclosureId ? insideA : insideB;
    if (env.enclosures.At(inside) != enclosureId) {
        return false;
    }
    const Vec2 target = World::TileCenter(inside);
    const float wadeDepth =
        tuning::kWadingShoulderFraction * species.shoulderHeightM * BodyScale(animal, species, env.time);
    const NavProfile profile = NavProfile::Animal(wadeDepth, species.canSwim);
    if (!env.nav.Passable(inside, profile)) {
        return false;
    }
    Start(animal, AnimalActivity::BreakOut, target, 1);
    return true;
}

}  // namespace zoo::animal_internal
