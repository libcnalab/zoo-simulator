// SPDX-License-Identifier: MIT
#include "zoo/animals/Animals.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

#include "zoo/animals/AnimalInternal.hpp"
#include "zoo/animals/AnimalTuning.hpp"

namespace zoo {
namespace {

float ClampLife(float value) {
    return Clamp01(value);
}

float HealthRoll(const Animal& animal, GameTime time, std::uint32_t purpose) {
    // A new walk or social choice must not reroll illness. Birth time separates
    // cohorts; id separates individuals. This stateless stream is reconstructible
    // from old saves and independent of the behaviour RNG's consumption.
    const auto cohort = static_cast<std::uint32_t>(animal.bornTick);
    const auto hour = static_cast<std::uint32_t>(time.tick / kTicksPerGameHour);
    return HashFloat(HashU32(animal.id.value, cohort), HashU32(hour, purpose));
}

Event LifeEvent(EventType type, AnimalId animal, AnimalId other, EnclosureId enclosure, Vec2 position,
                int detail = 0) {
    Event event{};
    event.type = type;
    event.animal = animal;
    event.other = other;
    event.enclosure = enclosure;
    event.position = position;
    event.detail = detail;
    return event;
}

bool IsCarnivore(const SpeciesDef& species) {
    return species.dangerous && (species.diet == Diet::Carnivore || species.diet == Diet::Omnivore);
}

GroupInfo ProjectedNurseryGroup(const std::vector<Animal>& animals, const Env& env, const Animal& mother,
                                EnclosureId enclosure, int young) {
    GroupInfo group{0, 0, 0, 0.0f, false};
    const SpeciesDef& species = env.species.All()[static_cast<std::size_t>(mother.species)];
    for (const Animal& other : animals) {
        const bool physicallyHere = other.InWorld() && env.enclosures.At(other.position) == enclosure;
        if (!physicallyHere && other.home != enclosure) {
            continue;
        }
        const SpeciesDef& otherSpecies = env.species.All()[static_cast<std::size_t>(other.species)];
        ++group.allAnimals;
        if (other.species == mother.species) {
            ++group.sameSpecies;
            if (other.sex == Sex::Male && IsAdult(other, species, env.time)) {
                ++group.adultMales;
            }
        } else {
            group.otherSpeciesSpaceM2 += SpaceNeededM2(otherSpecies, 1);
            if (IsCarnivore(otherSpecies) &&
                species.adultMassKg < tuning::kPredatorMassRatio * otherSpecies.adultMassKg) {
                group.predator = true;
            }
        }
        // A mother gives birth in her home enclosure. Reserve the minimum litter of
        // every other pregnancy committed to that enclosure, including a temporarily
        // crated resident, before allowing another pregnancy or a larger litter.
        if (other.id == mother.id || other.home != enclosure || !other.Pregnant()) {
            continue;
        }
        group.allAnimals += otherSpecies.litterMin;
        if (other.species == mother.species) {
            group.sameSpecies += otherSpecies.litterMin;
        } else {
            group.otherSpeciesSpaceM2 +=
                static_cast<float>(otherSpecies.litterMin) * SpaceNeededM2(otherSpecies, 1);
        }
    }
    group.allAnimals += young;
    group.sameSpecies += young;
    return group;
}

bool NurseryFits(const std::vector<Animal>& animals, const Env& env, const Animal& mother,
                 EnclosureId enclosure, int young) {
    const Enclosure* home = env.enclosures.Find(enclosure);
    if (home == nullptr) {
        return false;
    }
    const SpeciesDef& species = env.species.All()[static_cast<std::size_t>(mother.species)];
    const WelfareReport projected =
        EvaluateHabitat(species, home->stats, ProjectedNurseryGroup(animals, env, mother, enclosure, young));
    return projected.space >= 1.0f && projected.shelter >= 1.0f;
}

int NurseryRoom(const std::vector<Animal>& animals, const Env& env, const Animal& mother,
                EnclosureId enclosure, int requested) {
    // Old saves can contain a due mother without a valid home (and the group-limit
    // tests intentionally construct one). There is no habitat capacity to assess.
    if (env.enclosures.Find(enclosure) == nullptr) {
        return requested;
    }
    for (int young = requested; young > 0; --young) {
        if (NurseryFits(animals, env, mother, enclosure, young)) {
            return young;
        }
    }
    return 0;
}

}  // namespace

void Animals::ForgetAnimalReferences(AnimalId id) {
    for (Animal& animal : animals_) {
        if (animal.following == id) {
            animal.following = {};
        }
        if (animal.mate == id && !animal.Pregnant()) {
            animal.mate = {};
        }
    }
}

void Animals::AdvanceLife(Env& env) {
    std::vector<AnimalId> killed;
    // The young are added while the animals are walked through: room for every
    // litter due now, so that no reference into the list moves.
    std::size_t young = 0;
    for (const Animal& animal : animals_) {
        if (animal.Pregnant() && env.time.tick >= animal.dueTick) {
            young += static_cast<std::size_t>(
                std::max(0, env.species.All()[static_cast<std::size_t>(animal.species)].litterMax));
        }
    }
    animals_.reserve(animals_.size() + young);

    // push_back invalidates the old end iterator even after reserve; visit only the
    // original residents, by index, while newborns are appended.
    const std::size_t residents = animals_.size();
    for (std::size_t index = 0; index < residents; ++index) {
        Animal& animal = animals_[index];
        if (!animal.InWorld()) {
            continue;
        }
        const SpeciesDef& species = env.species.All()[static_cast<std::size_t>(animal.species)];
        const EnclosureId enclosureId = env.enclosures.At(animal.position);
        if (animal.sick) {
            if (animal.treated && animal.treatedTick < 0) {
                animal.treatedTick = env.time.tick - kTicksPerGameHour;
            }
            if (animal.sickSinceTick == 0) {
                animal.sickSinceTick = env.time.tick;
            }
            if (animal.treated && animal.treatedTick >= 0 &&
                env.time.tick - animal.treatedTick >=
                    static_cast<std::int64_t>(tuning::kTreatedRecoveryDays * kTicksPerDay)) {
                animal.sick = false;
                animal.treated = false;
                animal.sickSinceTick = 0;
                animal.treatedTick = 0;
                env.Emit(LifeEvent(EventType::AnimalRecovered, animal.id, {}, enclosureId, animal.position));
            } else if (!animal.treated && animal.welfare >= tuning::kUntreatedRecoveryWelfare &&
                       animal.sickSinceTick > 0 &&
                       env.time.tick - animal.sickSinceTick >=
                           static_cast<std::int64_t>(tuning::kUntreatedRecoveryMinimumDays * kTicksPerDay) &&
                       HealthRoll(animal, env.time, 0x7265636fU) < tuning::kUntreatedRecoveryChancePerHour) {
                animal.sick = false;
                animal.sickSinceTick = 0;
                env.Emit(LifeEvent(EventType::AnimalRecovered, animal.id, {}, enclosureId, animal.position));
            }
        } else {
            // Only welfare below the comfortable level makes an animal sickly.
            const float deficit =
                std::max(0.0f, tuning::kIllnessWelfareLevel - animal.welfare) / tuning::kIllnessWelfareLevel;
            float riskPerDay =
                tuning::kIllnessBaseChancePerDay + deficit * tuning::kIllnessPoorWelfareChancePerDay;
            if (enclosureId) {
                riskPerDay += Dirt(enclosureId) * tuning::kIllnessDirtyChancePerDay;
            }
            // Going hungry or thirsty for long, not the appetite before every meal.
            if (animal.hunger > tuning::kIllnessNeedLevel) {
                riskPerDay += tuning::kIllnessUrgentNeedChancePerDay;
            }
            if (animal.thirst > tuning::kIllnessNeedLevel) {
                riskPerDay += tuning::kIllnessUrgentNeedChancePerDay;
            }
            // Weather the species is not made for, by how far it is out; a shelter keeps it off.
            const float beyond = std::max(species.tempMinC - env.weather.temperatureC,
                                          env.weather.temperatureC - species.tempMaxC);
            if (beyond > 0.0f && !animal.shelter) {
                riskPerDay += tuning::kIllnessTemperatureChancePerDay *
                              std::min(1.0f, beyond / tuning::kIllnessTemperatureSpanC);
            }
            const float lifeFraction = AgeYears(animal, env.time) / std::max(0.001f, species.lifespanYears);
            if (lifeFraction >= tuning::kSicknessOldAgeFraction) {
                riskPerDay += tuning::kIllnessOldAgeChancePerDay;
            }
            bool contagious = false;
            for (const Animal& other : animals_) {
                if (other.id != animal.id && other.sick && other.InWorld() &&
                    other.species == animal.species && env.enclosures.At(other.position) == enclosureId &&
                    enclosureId) {
                    contagious = true;
                    break;
                }
            }
            if (contagious) {
                riskPerDay += tuning::kIllnessContagionChancePerDay;
            }
            // Preserve the established stream advancement for other life events,
            // but use an independent roll for the health outcome itself.
            static_cast<void>(rng_.NextFloat());
            if (HealthRoll(animal, env.time, 0x696c6c6eU) <
                std::min(1.0f, riskPerDay / tuning::kLifeChecksPerGameDay)) {
                animal.sick = true;
                animal.treated = false;
                animal.sickSinceTick = std::max<std::int64_t>(1, env.time.tick);
                animal.treatedTick = 0;
                env.Emit(LifeEvent(EventType::AnimalSick, animal.id, {}, enclosureId, animal.position));
            }
        }

        if (animal.Pregnant() && env.time.tick >= animal.dueTick && animal.InWorld() &&
            animal.health > 0.0f) {
            // Conception reserves a minimum litter for every pregnant mother. Keep those
            // reservations when choosing this litter, including births due in later hours.
            int group = 0;
            for (const Animal& other : animals_) {
                if (other.species == animal.species && other.home == animal.home) {
                    ++group;
                    if (other.id != animal.id && other.Pregnant())
                        group += species.litterMin;
                }
            }
            // A moved-in animal can consume the reserved room. As with a crated mother,
            // wait until birth is possible instead of exceeding groupMax or losing the young.
            const int groupRoom = species.groupMax - group;
            const int habitatRoom = NurseryRoom(animals_, env, animal, animal.home, groupRoom);
            if (groupRoom < species.litterMin || habitatRoom < species.litterMin)
                continue;
            const int litterSize = std::min(rng_.RangeInt(species.litterMin, species.litterMax),
                                            std::min(groupRoom, habitatRoom));
            const bool lost = rng_.Chance(tuning::kStillbirthBaseChance +
                                          (1.0f - animal.welfare) * tuning::kStillbirthPoorWelfareChance);
            animal.dueTick = 0;
            const AnimalId father = animal.mate;
            animal.mate = {};
            animal.lastBirthTick = env.time.tick;
            if (!lost) {
                for (int birthIndex = 0; birthIndex < litterSize; ++birthIndex) {
                    Vec2 position = animal.position;
                    constexpr Vec2 offsets[] = {{1.0f, 0.0f}, {-1.0f, 0.0f}, {0.0f, 1.0f}, {0.0f, -1.0f},
                                                {2.0f, 0.0f}, {-2.0f, 0.0f}, {0.0f, 2.0f}, {0.0f, -2.0f}};
                    for (Vec2 offset : offsets) {
                        const Vec2 candidate = animal.position + offset;
                        const TileCoord tile = World::TileAt(candidate);
                        const float babyRadius = species.bodyRadiusM * 0.35f;
                        bool unoccupied = true;
                        for (const Animal& other : animals_) {
                            if (!other.InWorld() || other.id == animal.id) {
                                continue;
                            }
                            const SpeciesDef& otherSpecies =
                                env.species.All()[static_cast<std::size_t>(other.species)];
                            const float otherRadius =
                                otherSpecies.bodyRadiusM * BodyScale(other, otherSpecies, env.time);
                            const float clearance = babyRadius + otherRadius + tuning::kAnimalClearanceM;
                            if (DistanceSquared(candidate, other.position) < clearance * clearance) {
                                unoccupied = false;
                                break;
                            }
                        }
                        if (unoccupied && env.world.InBounds(tile) &&
                            env.enclosures.At(tile) == animal.home &&
                            env.nav.Passable(tile, NavProfile::Animal(0.0f, species.canSwim))) {
                            position = candidate;
                            break;
                        }
                    }
                    const Sex sex = rng_.Chance(0.5f) ? Sex::Female : Sex::Male;
                    const AnimalId babyId = Add(env, animal.species, sex, 0.0f, position);
                    Animal* baby = Find(babyId);
                    if (baby == nullptr) {
                        continue;
                    }
                    baby->mother = animal.id;
                    baby->father = father;
                    baby->home = animal.home;
                    baby->hunger = 0.05f;
                    baby->thirst = 0.05f;
                    baby->health = 1.0f;
                    ++births_;
                    env.Emit(LifeEvent(EventType::AnimalBorn, babyId, animal.id, animal.home, position));
                }
            }
        }

        if (animal.sex == Sex::Female && animal.breeding && !animal.sick && !animal.Pregnant() &&
            animal.InWorld() && enclosureId && animal.welfare >= tuning::kBreedingWelfare &&
            AgeYears(animal, env.time) < species.lifespanYears * tuning::kBreedingMaximumAgeFraction &&
            (animal.lastBirthTick == 0 ||
             env.time.tick - animal.lastBirthTick >=
                 static_cast<std::int64_t>(species.birthIntervalYears * kGameDaysPerRealYear *
                                           kTicksPerDay))) {
            int speciesCount = 0;
            Animal* male = nullptr;
            for (Animal& other : animals_) {
                if (other.species != animal.species || other.home != enclosureId) {
                    continue;
                }
                ++speciesCount;
                if (other.Pregnant()) {
                    speciesCount += species.litterMin;
                }
                if (!other.InWorld() || env.enclosures.At(other.position) != enclosureId) {
                    continue;
                }
                if (other.sex == Sex::Male && other.breeding && !other.sick &&
                    IsAdult(other, species, env.time) && other.id != animal.father &&
                    other.mother != animal.id) {
                    male = &other;
                }
            }
            if (male != nullptr && speciesCount + species.litterMin <= species.groupMax &&
                NurseryFits(animals_, env, animal, enclosureId, species.litterMin) &&
                rng_.Chance(tuning::kConceptionChancePerDay / tuning::kLifeChecksPerGameDay)) {
                animal.dueTick =
                    env.time.tick +
                    static_cast<std::int64_t>(species.gestationDays * kGameDaysPerRealDay * kTicksPerDay);
                animal.mate = male->id;
                env.Emit(
                    LifeEvent(EventType::AnimalPregnant, animal.id, male->id, enclosureId, animal.position));
            }
        }
    }

    for (const Enclosure& enclosure : env.enclosures.All()) {
        std::vector<Animal*> members;
        for (Animal& animal : animals_) {
            if (animal.InWorld() && env.enclosures.At(animal.position) == enclosure.id) {
                members.push_back(&animal);
            }
        }
        for (std::size_t i = 0; i < members.size(); ++i) {
            Animal& predator = *members[i];
            const SpeciesDef& predatorSpecies = env.species.All()[static_cast<std::size_t>(predator.species)];
            if (!IsCarnivore(predatorSpecies) || predator.sick || predator.shelter ||
                predator.activity == AnimalActivity::Sleep ||
                predator.hunger <= tuning::kPredatorHungerThreshold) {
                continue;
            }
            Animal* prey = nullptr;
            float nearest = tuning::kPredatorAttackDistanceM * tuning::kPredatorAttackDistanceM;
            for (Animal* candidate : members) {
                const SpeciesDef& preySpecies =
                    env.species.All()[static_cast<std::size_t>(candidate->species)];
                if (candidate->species == predator.species || candidate->sick || candidate->shelter ||
                    candidate->activity == AnimalActivity::Sleep ||
                    preySpecies.adultMassKg >= predatorSpecies.adultMassKg * tuning::kPredatorMassRatio ||
                    std::find(killed.begin(), killed.end(), candidate->id) != killed.end()) {
                    continue;
                }
                const float distance = DistanceSquared(predator.position, candidate->position);
                if (distance <= nearest) {
                    nearest = distance;
                    prey = candidate;
                }
            }
            if (prey == nullptr ||
                !rng_.Chance(tuning::kPredatorAttackChancePerHour * ClampLife(predator.hunger))) {
                continue;
            }
            const SpeciesDef& preySpecies = env.species.All()[static_cast<std::size_t>(prey->species)];
            const float massRatio = preySpecies.adultMassKg / std::max(0.001f, predatorSpecies.adultMassKg);
            const float juvenileBonus =
                IsAdult(*prey, preySpecies, env.time) ? 0.0f : tuning::kPredatorJuvenileSuccessBonus;
            const float success = ClampLife(
                tuning::kPredatorBaseSuccessChance - massRatio * tuning::kPredatorMassSuccessPenalty +
                (1.0f - prey->health) * tuning::kPredatorHealthSuccessBonus + juvenileBonus);
            if (rng_.Chance(success)) {
                env.Emit(
                    LifeEvent(EventType::AnimalKilled, predator.id, prey->id, enclosure.id, prey->position));
                killed.push_back(prey->id);
                predator.hunger = std::max(0.0f, predator.hunger - tuning::kPredatorHungerDrop);
            } else {
                prey->health = std::max(0.0f, prey->health - tuning::kPredatorFailedWound);
                prey->stress = ClampLife(prey->stress + tuning::kFightStress);
            }
        }

        for (int speciesIndex = 0; speciesIndex < static_cast<int>(env.species.All().size());
             ++speciesIndex) {
            const SpeciesDef& species = env.species.All()[static_cast<std::size_t>(speciesIndex)];
            std::vector<Animal*> same;
            std::vector<Animal*> males;
            int population = 0;
            for (Animal* animal : members) {
                if (animal->species == speciesIndex) {
                    ++population;
                    if (!animal->sick && !animal->shelter && animal->activity != AnimalActivity::Sleep &&
                        IsAdult(*animal, species, env.time)) {
                        same.push_back(animal);
                        if (animal->sex == Sex::Male) {
                            males.push_back(animal);
                        }
                    }
                }
            }
            const int excessMales =
                species.maxAdultMales > 0 ? static_cast<int>(males.size()) - species.maxAdultMales : 0;
            const int excessGroup = population - species.groupMax;
            const bool maleFight = excessMales > 0 && males.size() >= 2;
            const bool crowdFight = excessGroup > 0 && same.size() >= 2;
            if (!maleFight && !crowdFight) {
                continue;
            }
            std::vector<Animal*>& fighters = maleFight ? males : same;
            const float excess = static_cast<float>(std::max(excessMales, excessGroup));
            const float chance =
                std::min(1.0f, tuning::kFightChancePerDay * species.aggression *
                                   (1.0f + excess * tuning::kFightExcessMultiplierPerAnimal) /
                                   tuning::kLifeChecksPerGameDay);
            if (!rng_.Chance(chance)) {
                continue;
            }
            const std::size_t firstIndex =
                static_cast<std::size_t>(rng_.RangeInt(0, static_cast<int>(fighters.size()) - 1));
            std::size_t secondIndex =
                static_cast<std::size_t>(rng_.RangeInt(0, static_cast<int>(fighters.size()) - 2));
            if (secondIndex >= firstIndex) {
                ++secondIndex;
            }
            Animal& first = *fighters[firstIndex];
            Animal& second = *fighters[secondIndex];
            const SpeciesDef& firstSpecies = env.species.All()[static_cast<std::size_t>(first.species)];
            const SpeciesDef& secondSpecies = env.species.All()[static_cast<std::size_t>(second.species)];
            first.health =
                std::max(0.0f, first.health - tuning::kFightHealthDamage * secondSpecies.adultMassKg /
                                                  firstSpecies.adultMassKg);
            second.health =
                std::max(0.0f, second.health - tuning::kFightHealthDamage * firstSpecies.adultMassKg /
                                                   secondSpecies.adultMassKg);
            first.stress = ClampLife(first.stress + tuning::kFightStress);
            second.stress = ClampLife(second.stress + tuning::kFightStress);
            env.Emit(LifeEvent(EventType::AnimalFight, first.id, second.id, enclosure.id, first.position));
        }
    }

    for (AnimalId id : killed) {
        Animal* animal = Find(id);
        if (animal == nullptr) {
            continue;
        }
        env.Emit(LifeEvent(EventType::AnimalDied, id, {}, animal->home, animal->position,
                           static_cast<int>(DeathCause::Killed)));
        ForgetAnimalReferences(id);
        animals_.erase(
            std::lower_bound(animals_.begin(), animals_.end(), id.value,
                             [](const Animal& value, std::uint32_t raw) { return value.id.value < raw; }));
        ++deaths_;
    }
}

}  // namespace zoo
