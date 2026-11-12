// SPDX-License-Identifier: MIT
#include "zoo/animals/AnimalInternal.hpp"

#include <algorithm>

#include "zoo/animals/AnimalTuning.hpp"

namespace zoo::animal_internal {
namespace {

float ClampNeed(float value) {
    return Clamp01(value);
}

Event NeedEvent(EventType type, const Animal& animal) {
    Event event{};
    event.type = type;
    event.animal = animal.id;
    event.enclosure = animal.home;
    event.position = animal.position;
    return event;
}

}  // namespace

void AdvanceNeeds(Animal& animal, const SpeciesDef& species, Env& env, const EnclosureStats* enclosure,
                  int viewers, const Animal* mother, bool predatorPresent) {
    const float oldHunger = animal.hunger;
    const float oldThirst = animal.thirst;
    animal.hunger = ClampNeed(animal.hunger + tuning::kHungerPerDay * kDaysPerTick);
    animal.thirst = ClampNeed(animal.thirst + tuning::kThirstPerDay * kDaysPerTick);
    if (animal.hunger >= tuning::kStarvingThreshold && oldHunger < tuning::kStarvingThreshold &&
        (animal.alerts & 1U) == 0U) {
        env.Emit(NeedEvent(EventType::AnimalStarving, animal));
        animal.alerts |= 1U;
    }
    if (animal.thirst >= tuning::kStarvingThreshold && oldThirst < tuning::kStarvingThreshold &&
        (animal.alerts & 2U) == 0U) {
        env.Emit(NeedEvent(EventType::AnimalThirsty, animal));
        animal.alerts |= 2U;
    }
    if (animal.hunger < tuning::kNeedResetThreshold) {
        animal.alerts &= static_cast<std::uint8_t>(~1U);
    }
    if (animal.thirst < tuning::kNeedResetThreshold) {
        animal.alerts &= static_cast<std::uint8_t>(~2U);
    }

    const float hoursPerTick =
        tuning::kMotionSecondsPerTick / (static_cast<float>(kTicksPerGameHour) / kTicksPerSecond);
    const bool sleeping = animal.activity == AnimalActivity::Sleep;
    const bool resting = animal.activity == AnimalActivity::Rest;
    const float sleepHours = std::max(tuning::kMinimumSleepHours, species.sleepHoursPerDay);
    if (sleeping) {
        animal.fatigue = ClampNeed(animal.fatigue - hoursPerTick / sleepHours);
    } else if (resting) {
        animal.fatigue =
            ClampNeed(animal.fatigue - tuning::kFatigueRestMultiplier * hoursPerTick / sleepHours);
    } else {
        const float awakeHours = std::max(tuning::kMinimumSleepHours, tuning::kAwakeHoursPerDay - sleepHours);
        animal.fatigue = ClampNeed(animal.fatigue + hoursPerTick / awakeHours);
    }

    const float area = enclosure == nullptr ? 1.0f : std::max(1.0f, enclosure->areaM2);
    const float enrichment =
        enclosure == nullptr
            ? 0.0f
            : ClampNeed(enclosure->enrichment / (area / tuning::kEnrichmentAreaScaleM2 + 1.0f));
    animal.boredom =
        ClampNeed(animal.boredom + species.playfulness * tuning::kBoredomPerDayMultiplier *
                                       (1.0f - tuning::kEnrichmentBoredomRelief * enrichment) * kDaysPerTick);
    // Session boredom relief is applied when an activity starts so short bouts remove the full amount.

    float stressTarget = animal.loose ? tuning::kLooseStressTarget
                                      : tuning::kWelfarePoorHabitatStress * (1.0f - animal.welfare);
    if (predatorPresent) {
        stressTarget = std::max(stressTarget, tuning::kPredatorStressTarget);
    }
    if (enclosure != nullptr) {
        const float tolerance = tuning::kCrowdStressToleranceBase +
                                tuning::kCrowdStressToleranceShyness * (1.0f - species.shyness);
        const float crowd = species.shyness *
                            ClampNeed((static_cast<float>(viewers) - tolerance) / std::max(1.0f, tolerance)) *
                            (1.0f - tuning::kVegetationCrowdRelief * enclosure->vegetationDensity);
        stressTarget = std::max(stressTarget, crowd);
    }
    if (animal.activity == AnimalActivity::Shelter || animal.shelter) {
        stressTarget *= tuning::kShelterStressMultiplier;
    }
    const float stressRate =
        animal.stress < stressTarget ? tuning::kStressRisePerDay : tuning::kStressFallPerDay;
    const float stressDelta = stressRate * kDaysPerTick;
    animal.stress =
        ClampNeed(animal.stress < stressTarget ? animal.stress + stressDelta : animal.stress - stressDelta);

    const float suckleAgeDays =
        species.maturityYears * kGameDaysPerRealYear * tuning::kJuvenileSuckleFraction;
    const EnclosureId enclosureId = env.enclosures.At(animal.position);
    if (enclosureId && mother != nullptr && mother->health > 0.0f &&
        env.enclosures.At(mother->position) == enclosureId && mother->hunger < tuning::kNeedSuckleThreshold &&
        AgeDays(animal, env.time) < suckleAgeDays) {
        animal.hunger = std::min(animal.hunger, tuning::kJuvenileNeedCap);
        animal.thirst = std::min(animal.thirst, tuning::kJuvenileNeedCap);
    }
}

bool AdvanceHealth(Animal& animal, const SpeciesDef& species, const Env& env, DeathCause& cause) {
    const float hungerDamage =
        animal.hunger >= 1.0f ? tuning::kHealthAtCriticalHungerPerDay * kDaysPerTick : 0.0f;
    const float thirstDamage =
        animal.thirst >= 1.0f ? tuning::kHealthAtCriticalThirstPerDay * kDaysPerTick : 0.0f;
    const float illnessDamage =
        animal.sick && !animal.treated ? tuning::kHealthSickPerDay * kDaysPerTick : 0.0f;
    const float age = AgeYears(animal, env.time);
    const float lifespan =
        species.lifespanYears *
        (tuning::kLifespanVariationBase + tuning::kLifespanVariationRange * HashFloat(animal.id.value, 77U));
    const float ageDamage = age > lifespan ? tuning::kHealthOldAgePerDay * kDaysPerTick : 0.0f;
    float coldDamage = 0.0f;
    float heatDamage = 0.0f;
    const float temperature = env.weather.temperatureC;
    const float coldExcess = species.tempMinC - temperature;
    const float heatExcess = temperature - species.tempMaxC;
    const Building* shelterBuilding = env.objects.FindBuilding(animal.shelter);
    bool shelteredFromWeather = false;
    if (shelterBuilding != nullptr && shelterBuilding->def >= 0 &&
        static_cast<std::size_t>(shelterBuilding->def) < env.catalog.buildings.size()) {
        const BuildingDef& definition = env.catalog.buildings[static_cast<std::size_t>(shelterBuilding->def)];
        shelteredFromWeather = coldExcess > 0.0f   ? definition.heated
                               : heatExcess > 0.0f ? definition.cooled
                                                   : true;
    }
    if (!shelteredFromWeather && coldExcess > tuning::kTemperatureExposureRangeC) {
        coldDamage = tuning::kHealthWeatherPerDay * kDaysPerTick;
    }
    if (!shelteredFromWeather && heatExcess > tuning::kTemperatureExposureRangeC) {
        heatDamage = tuning::kHealthWeatherPerDay * kDaysPerTick;
    }
    const float damage =
        std::max({hungerDamage, thirstDamage, illnessDamage, ageDamage, coldDamage, heatDamage});
    if (animal.hunger < tuning::kHealthyNeedThreshold && animal.thirst < tuning::kHealthyNeedThreshold &&
        !animal.sick && damage <= 0.0f) {
        animal.health = ClampNeed(animal.health + tuning::kHealthRecoveryPerDay * kDaysPerTick);
    } else {
        animal.health = ClampNeed(animal.health - damage);
    }
    cause = DeathCause::Illness;
    float largestDamage = illnessDamage;
    if (ageDamage > largestDamage) {
        largestDamage = ageDamage;
        cause = DeathCause::OldAge;
    }
    if (thirstDamage > largestDamage) {
        largestDamage = thirstDamage;
        cause = DeathCause::Thirst;
    }
    if (hungerDamage > largestDamage) {
        largestDamage = hungerDamage;
        cause = DeathCause::Starvation;
    }
    if (coldDamage > largestDamage) {
        largestDamage = coldDamage;
        cause = DeathCause::Cold;
    }
    if (heatDamage > largestDamage) {
        cause = DeathCause::Heat;
    }
    return animal.health <= 0.0f;
}

}  // namespace zoo::animal_internal

namespace zoo {

void Animals::GroupAnimals(const Env& env) {
    const std::vector<Enclosure>& enclosures = env.enclosures.All();
    if (enclosureScratch_.size() != enclosures.size()) {
        enclosureScratch_.resize(enclosures.size());
    }
    for (std::vector<std::size_t>& members : enclosureScratch_) {
        members.clear();
    }
    for (std::size_t animalIndex = 0; animalIndex < animals_.size(); ++animalIndex) {
        const Animal& animal = animals_[animalIndex];
        if (!animal.InWorld()) {
            continue;
        }
        const EnclosureId id = env.enclosures.At(animal.position);
        if (!id) {
            continue;
        }
        const auto found = std::lower_bound(
            enclosures.begin(), enclosures.end(), id.value,
            [](const Enclosure& enclosure, std::uint32_t value) { return enclosure.id.value < value; });
        if (found != enclosures.end() && found->id == id) {
            enclosureScratch_[static_cast<std::size_t>(found - enclosures.begin())].push_back(animalIndex);
        }
    }
}

void Animals::UpdateEnclosureDirt(const Env& env) {
    const std::vector<Enclosure>& enclosures = env.enclosures.All();
    for (std::size_t enclosureIndex = 0; enclosureIndex < enclosures.size(); ++enclosureIndex) {
        const Enclosure& enclosure = enclosures[enclosureIndex];
        auto it = std::lower_bound(
            care_.begin(), care_.end(), enclosure.id.value,
            [](const EnclosureCare& item, std::uint32_t value) { return item.id.value < value; });
        if (it == care_.end() || it->id != enclosure.id) {
            it = care_.insert(it, EnclosureCare{enclosure.id});
        }
        float load = 0.0f;
        for (std::size_t animalIndex : enclosureScratch_[enclosureIndex]) {
            const Animal& animal = animals_[animalIndex];
            const SpeciesDef& species = env.species.All()[static_cast<std::size_t>(animal.species)];
            const float scale = BodyScale(animal, species, env.time);
            load += std::pow(species.adultMassKg, 0.75f) * scale * scale;
        }
        const float area = std::max(1.0f, enclosure.stats.areaM2);
        const float dailyDirt =
            std::min(tuning::kMaximumDirtPerDay, tuning::kDirtyPerMassPowerAreaPerDay * load / area);
        it->dirt = Clamp01(it->dirt + dailyDirt * kDaysPerTick);
    }
}

}  // namespace zoo
