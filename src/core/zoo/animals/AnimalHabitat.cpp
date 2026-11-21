// SPDX-License-Identifier: MIT
#include "zoo/animals/AnimalInternal.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <utility>

#include "zoo/animals/AnimalTuning.hpp"

namespace zoo::animal_internal {
namespace {

float DistanceToSegmentSquared(Vec2 point, Vec2 from, Vec2 to) {
    const Vec2 segment = to - from;
    const float lengthSquared = LengthSquared(segment);
    const float along =
        lengthSquared > 0.0f ? std::clamp(Dot(point - from, segment) / lengthSquared, 0.0f, 1.0f) : 0.0f;
    return DistanceSquared(point, from + segment * along);
}

bool ClearPlace(Vec2 point, const Animal& animal, const SpeciesDef& species, const Enclosure& enclosure,
                const Env& env, std::span<const Animal> animals, std::span<const std::size_t> neighbors,
                std::vector<PropId>& props) {
    const float radius = species.bodyRadiusM * BodyScale(animal, species, env.time);
    const TileCoord tile = World::TileAt(point);
    const float wade =
        tuning::kWadingShoulderFraction * species.shoulderHeightM * BodyScale(animal, species, env.time);
    if (!env.world.InBounds(tile) || env.enclosures.At(tile) != enclosure.id ||
        !env.nav.Passable(tile, NavProfile::Animal(wade, species.canSwim))) {
        return false;
    }
    constexpr Dir sides[] = {Dir::North, Dir::East, Dir::South, Dir::West};
    // Probe the body's extremities as well as its centre: a tile centre is not
    // enough clearance for an elephant, or for a point near an irregular fence.
    for (Dir side : sides) {
        const TileCoord offset = Step(side);
        const Vec2 edge =
            point + Vec2{static_cast<float>(offset.x), static_cast<float>(offset.z)} * (radius + 0.1f);
        if (env.enclosures.At(edge) != enclosure.id ||
            !env.nav.Walkable(point, edge, NavProfile::Animal(wade, species.canSwim))) {
            return false;
        }
    }
    env.objects.PropsNear(point, radius + 3.0f, props);
    for (PropId id : props) {
        const Prop* prop = std::as_const(env.objects).FindProp(id);
        if (prop == nullptr || prop->def < 0) {
            continue;
        }
        const PropDef& def = env.catalog.props[static_cast<std::size_t>(prop->def)];
        const float apart = radius + def.radiusM + 0.1f;
        if (def.blocksWalking && DistanceSquared(point, prop->position) < apart * apart) {
            return false;
        }
    }
    // A clear point can still be a pocket between rocks, trees and a fence. The tile
    // navigator cannot see props, so retain only places with at least two body-wide ways
    // out; local obstacle avoidance can then approach and leave without repeatedly stalling.
    int exits = 0;
    for (Dir side : sides) {
        const TileCoord offset = Step(side);
        const Vec2 exit =
            point + Vec2{static_cast<float>(offset.x), static_cast<float>(offset.z)} * (radius + 1.0f);
        if (env.enclosures.At(exit) != enclosure.id ||
            !env.nav.Walkable(point, exit, NavProfile::Animal(wade, species.canSwim))) {
            continue;
        }
        bool open = true;
        for (PropId id : props) {
            const Prop* prop = std::as_const(env.objects).FindProp(id);
            if (prop == nullptr || prop->def < 0) {
                continue;
            }
            const PropDef& def = env.catalog.props[static_cast<std::size_t>(prop->def)];
            const float apart = radius + def.radiusM + 0.1f;
            if (def.blocksWalking && DistanceToSegmentSquared(prop->position, point, exit) < apart * apart) {
                open = false;
                break;
            }
        }
        exits += open ? 1 : 0;
    }
    if (exits < 2) {
        return false;
    }
    for (std::size_t index : neighbors) {
        const Animal& other = animals[index];
        if (other.id == animal.id || !other.Visible()) {
            continue;
        }
        const SpeciesDef& otherSpecies = env.species.All()[static_cast<std::size_t>(other.species)];
        const float rival = animal.sex == Sex::Male && other.sex == Sex::Male &&
                                    animal.species == other.species && IsAdult(animal, species, env.time) &&
                                    IsAdult(other, species, env.time)
                                ? species.aggression * 2.0f
                                : 0.0f;
        const float otherRadius = otherSpecies.bodyRadiusM * BodyScale(other, otherSpecies, env.time);
        const float apart =
            GirthRadius(radius, species) + GirthRadius(otherRadius, otherSpecies) + 0.1f + rival;
        if (DistanceSquared(point, other.position) < apart * apart ||
            (other.moving && DistanceSquared(point, other.target) < apart * apart)) {
            return false;
        }
    }
    return true;
}

struct PlaceChoice {
    Vec2 point;
    float score = -std::numeric_limits<float>::max();
    bool found = false;
};

float ShadeAt(Vec2 point, const Env& env, std::vector<PropId>& props) {
    float shade = 0.0f;
    env.objects.PropsNear(point, 12.0f, props);
    for (PropId id : props) {
        const Prop* prop = std::as_const(env.objects).FindProp(id);
        if (prop == nullptr || prop->def < 0) {
            continue;
        }
        const PropDef& def = env.catalog.props[static_cast<std::size_t>(prop->def)];
        if (def.shadeM2 > 0.0f) {
            const float radius = std::sqrt(def.shadeM2 / kPi) + def.radiusM;
            shade = std::max(shade, Clamp01(1.0f - Length(point - prop->position) / radius));
        }
    }
    return shade;
}

Vec2 Favourite(const Animal& animal, const Enclosure& enclosure, std::uint32_t place) {
    const std::uint32_t seed = HashU32(animal.id.value, enclosure.id.value + place * 101U);
    const Vec2 low = World::TileCenter(enclosure.min);
    const Vec2 high = World::TileCenter(enclosure.max);
    // Keep destinations inside the habitat, with three repeatable resting and
    // feeding areas per individual rather than an endlessly fresh random point.
    return {low.x + (high.x - low.x) * (0.2f + 0.6f * HashFloat(seed, 1U)),
            low.z + (high.z - low.z) * (0.2f + 0.6f * HashFloat(seed, 2U))};
}

}  // namespace

Vec2 RoutinePlace(const Animal& animal, const SpeciesDef& species, const BehaviourProfile& profile,
                  AnimalActivity& activity, const Enclosure& enclosure, const Env& env,
                  std::span<const Animal> animals, std::span<const std::size_t> neighbors,
                  std::vector<PropId>& propScratch, int viewers) {
    const float hour = env.time.Hour();
    const bool midday = hour >= 11.0f && hour < 16.0f;
    const bool hot = env.weather.temperatureC > species.tempMaxC - 4.0f;
    const bool coldMorning = hour < 11.0f && env.weather.temperatureC < species.tempMinC + 8.0f;
    const bool rest = RestingActivity(activity) || activity == AnimalActivity::Sleep;
    const bool wantsWater =
        (species.id == "flamingo" && activity != AnimalActivity::Sleep) ||
        activity == AnimalActivity::Bathe || activity == AnimalActivity::FilterFeed ||
        (species.id == "capybara" && activity == AnimalActivity::Rest && (midday || hot)) ||
        (species.id == "penguin" && activity == AnimalActivity::Wander &&
         !env.world.HasWater(World::TileAt(animal.position)));
    // A flamingo is a wader whatever it is doing: it stands in the shallows, it does not swim out.
    const bool wader = species.id == "flamingo";
    const bool shallow = activity == AnimalActivity::FilterFeed || species.id == "rhino" || wader;
    const bool wantsHigh = activity == AnimalActivity::Sentinel ||
                           (rest && (species.group == "big_cat" || species.group == "cat"));
    const std::uint32_t routeStop =
        HashU32(animal.id.value, static_cast<std::uint32_t>(env.time.tick / (45 * kTicksPerSecond))) % 3U;
    const Vec2 favourite = Favourite(animal, enclosure, rest ? 0U : 1U + routeStop);
    const Animal* leader = GroupLeader(animal, species, env, animals, neighbors);
    const Animal* anchor = profile.spreadM > 0.0f ? leader : nullptr;
    std::size_t rank = 0;
    for (std::size_t index : neighbors) {
        const Animal& other = animals[index];
        if (other.species == animal.species && other.id.value < animal.id.value) {
            ++rank;
        }
        if (!species.independentYoung && other.id == animal.mother && other.Visible() &&
            AgeYears(animal, env.time) < species.maturityYears * 0.5f) {
            anchor = &other;
        }
    }
    Vec2 groupSpot = favourite;
    if (anchor != nullptr && anchor->id != animal.id) {
        const float angle = static_cast<float>(rank) * 2.4f;
        const float gap = species.bodyRadiusM *
                              (BodyScale(animal, species, env.time) + BodyScale(*anchor, species, env.time)) +
                          0.3f;
        const float reach =
            rest && rank == 1 ? (gap - 0.3f) * tuning::kBodyWidthFraction + 0.15f
                              : gap + std::min(profile.spreadM * 0.6f, static_cast<float>(rank) * gap * 0.4f);
        groupSpot = (anchor->moving ? anchor->target : anchor->position) +
                    Vec2{std::sin(angle), std::cos(angle)} * reach;
    }
    std::array<Vec2, 112> candidates{};
    std::size_t count = 0;
    candidates[count++] = animal.position;
    candidates[count++] = favourite;
    if (anchor != nullptr && anchor->id != animal.id) {
        candidates[count++] = groupSpot;
        for (int index = 0; index < 8; ++index) {
            const float angle = static_cast<float>(index) * kPi * 0.25f;
            candidates[count++] =
                groupSpot + Vec2{std::sin(angle), std::cos(angle)} * (species.bodyRadiusM * 2.0f + 0.5f);
        }
    }
    // Fixed cost even for large enclosures. The lattice samples high ground,
    // shores and substrate; local samples let feeding and idle bouts shift a little.
    const Vec2 low = World::TileCenter(enclosure.min);
    const Vec2 high = World::TileCenter(enclosure.max);
    for (int z = 0; z < 8; ++z) {
        for (int x = 0; x < 8; ++x) {
            candidates[count++] = {low.x + (high.x - low.x) * (static_cast<float>(x) + 0.5f) / 8.0f,
                                   low.z + (high.z - low.z) * (static_cast<float>(z) + 0.5f) / 8.0f};
        }
    }
    for (int index = 0; index < 8; ++index) {
        const float angle = static_cast<float>(index) * kPi * 0.25f;
        candidates[count++] = animal.position + Vec2{std::sin(angle), std::cos(angle)} * 2.0f;
    }
    // Shade candidates beside nearby trees avoid sampling only the tree trunk.
    env.objects.PropsNear(animal.position, 60.0f, propScratch);
    for (PropId id : propScratch) {
        const Prop* prop = std::as_const(env.objects).FindProp(id);
        if (prop == nullptr || prop->def < 0 || count + 4 > candidates.size()) {
            continue;
        }
        const PropDef& def = env.catalog.props[static_cast<std::size_t>(prop->def)];
        if (def.shadeM2 <= 0.0f) {
            continue;
        }
        for (int index = 0; index < 4; ++index) {
            const float angle = static_cast<float>(index) * kPi * 0.5f;
            const float reach = def.radiusM + species.bodyRadiusM + 0.2f;
            candidates[count++] = prop->position + Vec2{std::sin(angle), std::cos(angle)} * reach;
        }
    }
    PlaceChoice dry;
    PlaceChoice wet;
    for (std::size_t index = 0; index < count; ++index) {
        const Vec2 point = candidates[index];
        if (!env.world.InBounds(World::TileAt(point)) || env.enclosures.At(point) != enclosure.id) {
            continue;
        }
        // A failed route must not immediately choose the same unreachable site.
        if (!animal.moving && animal.activityLeft <= 0 &&
            DistanceSquared(animal.position, animal.target) > 1.0f &&
            DistanceSquared(point, animal.target) < 1.0f) {
            continue;
        }
        const float depth = env.world.WaterDepthAt(point);
        const bool water = env.world.HasWater(World::TileAt(point));
        const float wade = species.shoulderHeightM * BodyScale(animal, species, env.time) *
                           (wader ? tuning::kWaderStandingFraction : tuning::kWadingShoulderFraction);
        if (water && ((!species.canSwim && !shallow) || (shallow && depth > wade))) {
            continue;
        }
        float score = -Length(point - favourite) * 0.12f - Length(point - animal.position) * 0.08f;
        if (rest) {
            score -= Length(point - favourite) * 0.2f;
        }
        if (species.group == "tortoise" && (rest || FeedingActivity(activity))) {
            // A trip to the opposite bank can take several minutes at a tortoise's walk.
            // Graze where grass is available and settle nearby unless better shade merits the trip.
            const float travelSeconds = Length(point - animal.position) / species.walkSpeed;
            score -= travelSeconds * 0.24f;
            if (FeedingActivity(activity) && env.world.At(World::TileAt(point)).terrain == Terrain::Grass) {
                score += 4.0f;
            }
        }
        if (activity == AnimalActivity::Idle || activity == AnimalActivity::Preen ||
            activity == AnimalActivity::Sentinel) {
            score -= Length(point - animal.position);
        }
        if (activity == AnimalActivity::Wander || activity == AnimalActivity::Patrol ||
            FeedingActivity(activity)) {
            // Reusing an area does not mean remaining on one exact coordinate.
            score += std::min(4.0f, Length(point - animal.position)) * 0.5f;
        }
        if (anchor != nullptr && anchor->id != animal.id) {
            score -= Length(point - groupSpot) * (species.id == "flamingo" ? 4.0f : 2.0f);
        }
        if (viewers > 20 && species.shyness > 0.3f && !enclosure.viewing.empty()) {
            // The input is an enclosure count, not individual visitor positions.
            // The first viewing front is a stable proxy for the public side.
            const Vec2 front = World::TileCenter(enclosure.viewing.front().tile);
            score += Length(point - front) * species.shyness * 0.6f;
        }
        const float shade = (midday && rest) || hot || coldMorning || activity == AnimalActivity::Browse
                                ? ShadeAt(point, env, propScratch)
                                : 0.0f;
        if ((midday && rest) || hot) {
            score += shade * 30.0f;
        } else if (coldMorning) {
            score -= shade * 20.0f;
        }
        if (wantsHigh) {
            score += env.world.HeightAt(point) * 4.0f;
        }
        if (activity == AnimalActivity::DustBathe || activity == AnimalActivity::Dig) {
            const Terrain terrain = env.world.At(World::TileAt(point)).terrain;
            score += terrain == Terrain::Sand || terrain == Terrain::Dirt ? 6.0f : 0.0f;
        }
        if (activity == AnimalActivity::Browse) {
            score += shade * 10.0f;
        }
        if (activity == AnimalActivity::Patrol) {
            score -= std::min({point.x - low.x, high.x - point.x, point.z - low.z, high.z - point.z});
        }
        PlaceChoice& choice = water ? wet : dry;
        if ((!choice.found || score > choice.score) &&
            ClearPlace(point, animal, species, enclosure, env, animals, neighbors, propScratch)) {
            choice = {point, score, true};
        }
    }
    if (wantsWater && wet.found) {
        return wet.point;
    }
    if (activity == AnimalActivity::Bathe && !wet.found) {
        activity = species.id == "elephant" || species.id == "rhino" ? AnimalActivity::DustBathe
                                                                     : AnimalActivity::Wander;
    } else if (activity == AnimalActivity::FilterFeed && !wet.found) {
        activity = AnimalActivity::Forage;
    }
    if (dry.found) {
        return dry.point;
    }
    return animal.position;
}

}  // namespace zoo::animal_internal
