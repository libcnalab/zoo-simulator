// SPDX-License-Identifier: MIT
#include "zoo/animals/Animals.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <functional>
#include <iterator>
#include <limits>
#include <span>
#include <utility>

#include "zoo/animals/AnimalInternal.hpp"
#include "zoo/animals/AnimalProfile.hpp"
#include "zoo/animals/AnimalTuning.hpp"

namespace zoo {
namespace {
float Clamp(float value) {
    return Clamp01(value);
}
void Issue(WelfareReport& report, WelfareIssue issue) {
    report.issues |= Bit(issue);
}
Event MakeEvent(EventType type, AnimalId animal, EnclosureId enclosure, Vec2 position, int detail = 0) {
    Event event{};
    event.type = type;
    event.animal = animal;
    event.enclosure = enclosure;
    event.position = position;
    event.detail = detail;
    return event;
}
std::string ChooseAnimalName(Rng& rng, const std::vector<Animal>& animals, int species, Sex sex) {
    constexpr std::string_view female[] = {
        "Amina", "Anika",  "Aya",  "Bela", "Cleo", "Dara",  "Eleni", "Esme", "Farah", "Freya",
        "Hana",  "Indira", "Ines", "Jaya", "Kaya", "Laila", "Leila", "Lina", "Mara",  "Mina",
        "Nadia", "Nala",   "Noor", "Nora", "Rina", "Rosa",  "Sana",  "Sara", "Suki",  "Talia",
        "Thea",  "Uma",    "Vera", "Yara", "Zara", "Zuri",  "Amara", "Bina", "Dalia", "Elif",
        "Inez",  "Lumi",   "Mika", "Nia",  "Rhea", "Sora",  "Tara",  "Yuki"};
    constexpr std::string_view male[] = {
        "Aadi",  "Aki",   "Arun", "Basil",  "Cai",   "Davi",  "Eli",   "Emil",  "Enzo",  "Faris",
        "Hugo",  "Idris", "Ivo",  "Jamal",  "Kenji", "Kian",  "Leo",   "Luca",  "Malik", "Milan",
        "Niko",  "Noah",  "Omar", "Pavel",  "Rafi",  "Ravi",  "Ren",   "Rohan", "Samir", "Sami",
        "Tariq", "Theo",  "Uri",  "Vikram", "Yusuf", "Zain",  "Adil",  "Beni",  "Dara",  "Eren",
        "Ilan",  "Jiro",  "Kofi", "Musa",   "Nuri",  "Sacha", "Tomas", "Zev"};
    const auto names = sex == Sex::Female ? std::span<const std::string_view>(female)
                                          : std::span<const std::string_view>(male);
    std::vector<std::size_t> available;
    available.reserve(names.size());
    for (std::size_t i = 0; i < names.size(); ++i) {
        bool used = false;
        for (const Animal& animal : animals) {
            if (animal.species == species && animal.name == names[i]) {
                used = true;
                break;
            }
        }
        if (!used)
            available.push_back(i);
    }
    if (available.empty())
        return std::string(
            names[static_cast<std::size_t>(rng.RangeInt(0, static_cast<int>(names.size()) - 1))]);
    const std::size_t choice =
        available[static_cast<std::size_t>(rng.RangeInt(0, static_cast<int>(available.size()) - 1))];
    return std::string(names[choice]);
}
float WeightedOverall(const WelfareReport& r) {
    constexpr float weights[] = {1.5f, 1.0f, 0.7f, 0.7f, 0.8f, 1.2f, 0.8f};
    const float values[] = {r.space, r.terrain, r.water, r.vegetation, r.shelter, r.social, r.enrichment};
    float total = 0.0f;
    float weight = 0.0f;
    float minimum = 1.0f;
    for (int i = 0; i < 7; ++i) {
        total += values[i] * weights[i];
        weight += weights[i];
        minimum = std::min(minimum, values[i]);
    }
    return Clamp(0.7f * total / weight + 0.3f * minimum);
}
GroupInfo GroupFromMembers(const Env& env, const Animal& animal, std::span<const Animal> animals,
                           std::span<const std::size_t> members) {
    GroupInfo group;
    const EnclosureId enclosure = env.enclosures.At(animal.position);
    if (!enclosure) {
        return group;
    }
    const SpeciesDef& species = env.species.All()[static_cast<std::size_t>(animal.species)];
    for (std::size_t memberIndex : members) {
        const Animal& other = animals[memberIndex];
        if (!other.InWorld() || env.enclosures.At(other.position) != enclosure) {
            continue;
        }
        ++group.allAnimals;
        const SpeciesDef& otherSpecies = env.species.All()[static_cast<std::size_t>(other.species)];
        if (other.species == animal.species) {
            ++group.sameSpecies;
            if (other.sex == Sex::Male && IsAdult(other, species, env.time)) {
                ++group.adultMales;
            }
        } else if (otherSpecies.dangerous &&
                   (otherSpecies.diet == Diet::Carnivore || otherSpecies.diet == Diet::Omnivore) &&
                   species.adultMassKg < tuning::kPredatorMassRatio * otherSpecies.adultMassKg) {
            group.predator = true;
        } else {
            group.otherSpeciesSpaceM2 += SpaceNeededM2(otherSpecies, 1);
        }
    }
    return group;
}
}  // namespace

std::string_view ToString(Sex value) {
    return value == Sex::Female ? "female" : "male";
}
std::string_view ToString(AnimalActivity value) {
    constexpr std::string_view names[] = {"idle",     "wander",   "forage",      "eat",        "drink",
                                          "rest",     "sleep",    "play",        "socialize",  "vocalize",
                                          "pace",     "shelter",  "break_out",   "loose",      "crated",
                                          "browse",   "ruminate", "bathe",       "dust_bathe", "preen",
                                          "sentinel", "dig",      "filter_feed", "patrol",     "huddle"};
    const auto index = static_cast<std::size_t>(value);
    return index < std::size(names) ? names[index] : "idle";
}
std::string_view ToString(DeathCause value) {
    constexpr std::string_view names[] = {"old_age", "starvation", "thirst", "illness",
                                          "cold",    "heat",       "killed", "injury"};
    const auto index = static_cast<std::size_t>(value);
    return index < std::size(names) ? names[index] : "old_age";
}
std::string_view Describe(WelfareIssue issue) {
    constexpr std::string_view names[] = {"This enclosure is too small.",
                                          "The ground is unsuitable.",
                                          "There is too little open water.",
                                          "There is no deep water.",
                                          "The habitat is too bare.",
                                          "The habitat is overgrown.",
                                          "There is no shelter.",
                                          "The shelter is full.",
                                          "This animal is lonely.",
                                          "There are too many animals of this species.",
                                          "There are too many adult males.",
                                          "A predator is nearby.",
                                          "There is no suitable food.",
                                          "There is no drinking water.",
                                          "This animal is hungry.",
                                          "This animal is thirsty.",
                                          "There is too little enrichment.",
                                          "This animal is stressed.",
                                          "The enclosure is dirty.",
                                          "This animal is sick.",
                                          "This animal is too cold.",
                                          "This animal is too hot.",
                                          "This animal is outside an enclosure."};
    const auto index = static_cast<std::size_t>(issue);
    return index < std::size(names) ? names[index] : "Unknown welfare issue.";
}

float SpaceNeededM2(const SpeciesDef& species, int count) {
    const int extras = std::max(0, count - std::max(1, species.groupMin));
    return species.minEnclosureM2 + static_cast<float>(extras) * species.spacePerExtraAnimalM2;
}

WelfareReport EvaluateHabitat(const SpeciesDef& species, const EnclosureStats& stats,
                              const GroupInfo& group) {
    WelfareReport r;
    const float needed = SpaceNeededM2(species, group.sameSpecies) + group.otherSpeciesSpaceM2;
    r.space = needed <= 0.0f ? 1.0f : Clamp(stats.areaM2 / needed);
    if (r.space < tuning::kHabitatMinimumIssue)
        Issue(r, WelfareIssue::TooSmall);
    float weightedTerrain = 0.0f;
    float maxSuitability = 0.0f;
    for (std::size_t i = 0; i < species.terrainSuitability.size(); ++i) {
        weightedTerrain += stats.terrainShare[i] * species.terrainSuitability[i];
        maxSuitability = std::max(maxSuitability, species.terrainSuitability[i]);
    }
    const float dryShare = std::max(0.0f, 1.0f - stats.waterShare);
    const float terrainDenominator = maxSuitability * dryShare;
    r.terrain = terrainDenominator <= 0.0f ? 0.0f : Clamp(weightedTerrain / terrainDenominator);
    if (r.terrain < tuning::kTerrainIssue)
        Issue(r, WelfareIssue::PoorTerrain);
    if (species.waterAreaFraction == 0.0f) {
        r.water = stats.waterShare <= 0.5f ? 1.0f : Clamp(2.0f * (1.0f - stats.waterShare));
    } else {
        r.water = Clamp(stats.waterShare / species.waterAreaFraction);
    }
    if (species.needsDeepWater && !stats.deepWater) {
        r.water = std::min(r.water, tuning::kDeepWaterCap);
        Issue(r, WelfareIssue::NoDeepWater);
    }
    if (r.water < tuning::kWaterIssue)
        Issue(r, WelfareIssue::TooLittleWater);
    const float difference =
        std::max(0.0f, std::abs(stats.vegetationDensity - species.vegetationDensity) - 0.15f);
    r.vegetation = Clamp(1.0f - std::min(1.0f, difference / 0.5f));
    if (r.vegetation < tuning::kVegetationIssue) {
        Issue(r, stats.vegetationDensity < species.vegetationDensity ? WelfareIssue::TooBare
                                                                     : WelfareIssue::TooOvergrown);
    }
    if (!species.needsShelter) {
        r.shelter = 1.0f;
    } else if (stats.shelterCapacity <= 0) {
        r.shelter = tuning::kShelterBase;
        Issue(r, WelfareIssue::NoShelter);
    } else if (stats.shelterCapacity < group.allAnimals) {
        r.shelter = tuning::kShelterPartialBase + 0.5f * static_cast<float>(stats.shelterCapacity) /
                                                      static_cast<float>(std::max(1, group.allAnimals));
        Issue(r, WelfareIssue::ShelterFull);
    }
    if (group.sameSpecies < species.groupMin) {
        const float ratio = static_cast<float>(group.sameSpecies) / static_cast<float>(species.groupMin);
        r.social = 1.0f - (1.0f - ratio) * (tuning::kSocialLonelyPenalty +
                                            tuning::kSocialSociabilityPenalty * species.sociability);
        Issue(r, WelfareIssue::Lonely);
    } else if (group.sameSpecies > species.groupMax) {
        r.social = std::max(0.2f, 1.0f - static_cast<float>(group.sameSpecies - species.groupMax) /
                                             static_cast<float>(std::max(1, species.groupMax)));
        Issue(r, WelfareIssue::Overcrowded);
    }
    if (species.maxAdultMales > 0 && group.adultMales > species.maxAdultMales) {
        r.social *= tuning::kMaleSocialMultiplier;
        Issue(r, WelfareIssue::TooManyMales);
    }
    if (group.predator) {
        r.social = std::min(r.social, tuning::kPredatorSocialCap);
        Issue(r, WelfareIssue::PredatorNearby);
    }
    if (species.playfulness < 0.05f) {
        r.enrichment = 1.0f;
    } else {
        const float enrichmentNeeded =
            species.playfulness * (0.5f + 0.25f * static_cast<float>(group.sameSpecies));
        r.enrichment = Clamp(0.3f + 0.7f * stats.enrichment / std::max(0.001f, enrichmentNeeded));
        if (r.enrichment < tuning::kEnrichmentIssue)
            Issue(r, WelfareIssue::Bored);
    }
    r.overall = WeightedOverall(r);
    return r;
}

float Containment(const SpeciesDef& species, const EnclosureStats& stats) {
    if (stats.tiles <= 0)
        return 0.0f;
    const float need = std::max(0.001f, species.fenceHeightM);
    const float height = Clamp((stats.minFenceHeightM - 0.6f * need) / (0.4f * need));
    const float deficit = static_cast<float>(std::max(0, species.fenceStrength - stats.minFenceStrength));
    const float strength =
        stats.minFenceStrength >= species.fenceStrength ? 1.0f : std::max(0.0f, 1.0f - 0.4f * deficit);
    const float condition = stats.minFenceCondition >= 0.5f ? 1.0f : Clamp(stats.minFenceCondition / 0.5f);
    return std::min({height, strength, condition});
}

float AgeDays(const Animal& animal, GameTime now) {
    return static_cast<float>(static_cast<double>(now.tick - animal.bornTick) /
                              static_cast<double>(kTicksPerDay));
}
float AgeYears(const Animal& animal, GameTime now) {
    return AgeDays(animal, now) / kGameDaysPerRealYear;
}
bool IsAdult(const Animal& animal, const SpeciesDef& species, GameTime now) {
    return AgeYears(animal, now) >= species.maturityYears;
}
float BodyScale(const Animal& animal, const SpeciesDef& species, GameTime now) {
    const float t = Clamp(AgeYears(animal, now) / std::max(0.001f, species.maturityYears));
    const float smooth = t * t * (3.0f - 2.0f * t);
    return 0.35f + 0.65f * smooth;
}

Animals::Animals(std::uint64_t seed) : rng_(seed) {}
AnimalId Animals::Add(const Env& env, int species, Sex sex, float ageYears, Vec2 position) {
    if (species < 0 || static_cast<std::size_t>(species) >= env.species.All().size() ||
        !std::isfinite(ageYears) || ageYears < 0.0f || !env.world.InBounds(World::TileAt(position)) ||
        !env.nav.Passable(World::TileAt(position), NavProfile::Animal(0.0f, false)))
        return {};
    if (nextId_ == 0 || nextId_ == std::numeric_limits<std::uint32_t>::max())
        return {};
    Animal animal;
    animal.id = AnimalId{nextId_++};
    animal.species = species;
    animal.sex = sex;
    animal.bornTick =
        env.time.tick - static_cast<std::int64_t>(ageYears * kGameDaysPerRealYear * kTicksPerDay);
    animal.position = position;
    animal.target = position;
    animal.hunger = rng_.Range(0.1f, 0.3f);
    animal.thirst = rng_.Range(0.1f, 0.3f);
    animal.fatigue = rng_.Range(0.1f, 0.3f);
    animal.boredom = rng_.Range(0.1f, 0.3f);
    animal.name = ChooseAnimalName(rng_, animals_, species, sex);
    animals_.push_back(animal);
    const EnclosureId enclosure = env.enclosures.At(position);
    if (enclosure) {
        auto found = std::lower_bound(
            care_.begin(), care_.end(), enclosure.value,
            [](const EnclosureCare& item, std::uint32_t value) { return item.id.value < value; });
        if (found == care_.end() || found->id != enclosure)
            care_.insert(found, EnclosureCare{enclosure});
        animal.home = enclosure;
        animals_.back().home = enclosure;
        animals_.back().welfare = Welfare(env, animals_.back()).overall;
    }
    return animals_.back().id;
}

bool Animals::Remove(AnimalId id) {
    auto it =
        std::lower_bound(animals_.begin(), animals_.end(), id.value,
                         [](const Animal& animal, std::uint32_t value) { return animal.id.value < value; });
    if (it == animals_.end() || it->id != id)
        return false;
    ForgetAnimalReferences(id);
    animals_.erase(it);
    return true;
}
Animal* Animals::Find(AnimalId id) {
    auto it =
        std::lower_bound(animals_.begin(), animals_.end(), id.value,
                         [](const Animal& animal, std::uint32_t value) { return animal.id.value < value; });
    return it != animals_.end() && it->id == id ? &*it : nullptr;
}
const Animal* Animals::Find(AnimalId id) const {
    auto it =
        std::lower_bound(animals_.begin(), animals_.end(), id.value,
                         [](const Animal& animal, std::uint32_t value) { return animal.id.value < value; });
    return it != animals_.end() && it->id == id ? &*it : nullptr;
}

bool Animals::CrowdsResident(const Enclosures& habitats, const SpeciesCatalog& species, GameTime now,
                             Vec2 next, std::optional<Vec2> from) const {
    const auto homeId = habitats.At(next);
    if (!homeId)
        return false;
    const auto* home = habitats.Find(homeId);
    if (!home || !home->walkThrough)
        return false;
    for (const auto& resident : animals_) {
        if (resident.home != homeId || !resident.Visible())
            continue;
        const auto& def = species.All()[static_cast<std::size_t>(resident.species)];
        const float gap = SharedWalkClearance(def.bodyRadiusM * BodyScale(resident, def, now));
        const float after = DistanceSquared(next, resident.position);
        if (after < gap * gap && (!from || after < DistanceSquared(*from, resident.position)))
            return true;
    }
    return false;
}

void Animals::InEnclosure(const Env& env, EnclosureId enclosure, std::vector<AnimalId>& out) const {
    out.clear();
    for (const Animal& animal : animals_) {
        if (animal.InWorld() && env.enclosures.At(animal.position) == enclosure)
            out.push_back(animal.id);
    }
}
GroupInfo Animals::GroupOf(const Env& env, const Animal& animal) const {
    GroupInfo group;
    group.sameSpecies = 0;
    group.allAnimals = 0;
    const EnclosureId enclosure = env.enclosures.At(animal.position);
    if (!enclosure)
        return {0, 0, 0, 0.0f, false};
    const SpeciesDef& species = env.species.All()[static_cast<std::size_t>(animal.species)];
    for (const Animal& other : animals_) {
        if (!other.InWorld() || env.enclosures.At(other.position) != enclosure)
            continue;
        ++group.allAnimals;
        const SpeciesDef& otherSpecies = env.species.All()[static_cast<std::size_t>(other.species)];
        if (other.species == animal.species) {
            ++group.sameSpecies;
            if (other.sex == Sex::Male && IsAdult(other, species, env.time))
                ++group.adultMales;
        } else if (otherSpecies.dangerous &&
                   (otherSpecies.diet == Diet::Carnivore || otherSpecies.diet == Diet::Omnivore) &&
                   species.adultMassKg < tuning::kPredatorMassRatio * otherSpecies.adultMassKg) {
            group.predator = true;
        } else {
            group.otherSpeciesSpaceM2 += SpaceNeededM2(otherSpecies, 1);
        }
    }
    return group;
}
WelfareReport Animals::Welfare(const Env& env, const Animal& animal) const {
    return WelfareWithGroup(env, animal, GroupOf(env, animal));
}

const WelfareReport& Animals::CachedHabitat(const Env& env, const Animal& animal, const GroupInfo& group) {
    const EnclosureId enclosureId = env.enclosures.At(animal.position);
    const SpeciesDef& species = env.species.All()[static_cast<std::size_t>(animal.species)];
    const std::uint32_t enclosureRevision = env.enclosures.Revision();
    const std::uint32_t objectRevision = env.objects.Revision();
    const std::uint32_t worldRevision = env.world.Revision();
    const std::int64_t refreshHour = env.time.tick / kTicksPerGameHour;
    for (HabitatCache& cache : habitatScratch_) {
        if (cache.enclosure != enclosureId || cache.species != animal.species) {
            continue;
        }
        const bool sameGroup = cache.group.sameSpecies == group.sameSpecies &&
                               cache.group.adultMales == group.adultMales &&
                               cache.group.allAnimals == group.allAnimals &&
                               cache.group.otherSpeciesSpaceM2 == group.otherSpeciesSpaceM2 &&
                               cache.group.predator == group.predator;
        if (sameGroup && cache.enclosureRevision == enclosureRevision &&
            cache.objectRevision == objectRevision && cache.worldRevision == worldRevision &&
            cache.refreshHour == refreshHour) {
            return cache.report;
        }
        cache.group = group;
        cache.enclosureRevision = enclosureRevision;
        cache.objectRevision = objectRevision;
        cache.worldRevision = worldRevision;
        cache.refreshHour = refreshHour;
        const Enclosure* enclosure = env.enclosures.Find(enclosureId);
        cache.report =
            enclosure == nullptr ? WelfareReport{} : EvaluateHabitat(species, enclosure->stats, group);
        return cache.report;
    }
    const Enclosure* enclosure = env.enclosures.Find(enclosureId);
    HabitatCache cache;
    cache.enclosure = enclosureId;
    cache.species = animal.species;
    cache.group = group;
    cache.enclosureRevision = enclosureRevision;
    cache.objectRevision = objectRevision;
    cache.worldRevision = worldRevision;
    cache.refreshHour = refreshHour;
    cache.report = enclosure == nullptr ? WelfareReport{} : EvaluateHabitat(species, enclosure->stats, group);
    habitatScratch_.push_back(cache);
    return habitatScratch_.back().report;
}

WelfareReport Animals::WelfareWithGroup(const Env& env, const Animal& animal, const GroupInfo& group,
                                        const WelfareReport* cachedHabitat) const {
    const SpeciesDef& species = env.species.All()[static_cast<std::size_t>(animal.species)];
    const EnclosureId enclosure = env.enclosures.At(animal.position);
    WelfareReport report;
    const Enclosure* current = env.enclosures.Find(enclosure);
    if (animal.activity == AnimalActivity::Crated) {
        report.space = report.terrain = report.water = report.vegetation = 0.5f;
        report.shelter = report.social = report.enrichment = 0.5f;
    } else if (!enclosure) {
        report.space = report.terrain = report.water = report.vegetation = 0.3f;
        report.shelter = report.social = report.enrichment = 0.3f;
        Issue(report, WelfareIssue::Escaped);
    } else if (current != nullptr) {
        report = cachedHabitat == nullptr ? EvaluateHabitat(species, current->stats, group) : *cachedHabitat;
        const EnclosureCare* care = nullptr;
        for (const EnclosureCare& item : care_) {
            if (item.id == enclosure) {
                care = &item;
                break;
            }
        }
        if (care != nullptr)
            report.cleanliness = Clamp(1.0f - care->dirt);
    }
    const float worstNeed = std::max(animal.hunger, animal.thirst);
    report.nutrition = worstNeed <= 0.4f ? 1.0f : Clamp(1.0f - (worstNeed - 0.4f) / 0.6f);
    if (animal.hunger > tuning::kNeedIssue)
        Issue(report, WelfareIssue::Hungry);
    if (animal.thirst > tuning::kNeedIssue)
        Issue(report, WelfareIssue::Thirsty);
    bool edibleFood = false;
    bool hasDrink = false;
    bool hasOpenWater = false;
    if (current != nullptr) {
        for (PropId propId : current->feeders) {
            const Prop* prop = std::as_const(env.objects).FindProp(propId);
            if (prop == nullptr || prop->def < 0 ||
                static_cast<std::size_t>(prop->def) >= env.catalog.props.size())
                continue;
            const PropDef& def = env.catalog.props[static_cast<std::size_t>(prop->def)];
            for (const std::string& food : species.foods) {
                if (std::find(def.foods.begin(), def.foods.end(), food) != def.foods.end())
                    edibleFood = true;
            }
        }
        hasDrink = !current->troughs.empty();
        for (int z = current->min.z; z <= current->max.z; ++z) {
            for (int x = current->min.x; x <= current->max.x; ++x) {
                const TileCoord tile{x, z};
                if (env.enclosures.At(tile) == enclosure && env.world.HasWater(tile))
                    hasOpenWater = true;
            }
        }
        if (!edibleFood)
            Issue(report, WelfareIssue::NoFood);
        if (!hasDrink && !hasOpenWater)
            Issue(report, WelfareIssue::NoDrink);
        const float temperature = env.weather.temperatureC;
        float excess = std::max({species.tempMinC - temperature, temperature - species.tempMaxC, 0.0f});
        const Building* shelter = env.objects.FindBuilding(animal.shelter);
        const BuildingDef* shelterDef = nullptr;
        if (shelter != nullptr && shelter->def >= 0 &&
            static_cast<std::size_t>(shelter->def) < env.catalog.buildings.size())
            shelterDef = &env.catalog.buildings[static_cast<std::size_t>(shelter->def)];
        if (excess > 0.0f && shelterDef != nullptr) {
            const bool helped = temperature < species.tempMinC ? shelterDef->heated : shelterDef->cooled;
            excess = helped ? 0.0f : excess * tuning::kWarmthShelterFactor;
        }
        if (temperature > species.tempMaxC &&
            (current->stats.shadeM2 > 0.0f || current->stats.waterShare > 0.0f))
            excess = std::max(0.0f, excess - tuning::kHeatShadeReliefC);
        report.comfort = Clamp(1.0f - excess / tuning::kTemperatureComfortSpanC);
        if (report.comfort < tuning::kTemperatureIssue)
            Issue(report, temperature < species.tempMinC ? WelfareIssue::TooCold : WelfareIssue::TooHot);
        if (Dirt(enclosure) > tuning::kDirtyIssue)
            Issue(report, WelfareIssue::Dirty);
    }
    if (animal.sick)
        Issue(report, WelfareIssue::Sick);
    report.health = Clamp(animal.health);
    report.calm = Clamp(1.0f - animal.stress);
    if (animal.stress > 0.6f)
        Issue(report, WelfareIssue::Stressed);
    report.overall = report.space;
    const float aspects[] = {report.space,   report.terrain,     report.water,      report.vegetation,
                             report.shelter, report.social,      report.enrichment, report.nutrition,
                             report.comfort, report.cleanliness, report.health,     report.calm};
    constexpr float weights[] = {1.5f, 1.0f, 0.7f, 0.7f, 0.8f, 1.2f, 0.8f, 1.5f, 0.8f, 0.6f, 1.5f, 1.0f};
    float total = 0.0f;
    float weight = 0.0f;
    float minimum = 1.0f;
    for (std::size_t i = 0; i < std::size(aspects); ++i) {
        total += aspects[i] * weights[i];
        weight += weights[i];
        minimum = std::min(minimum, aspects[i]);
    }
    report.overall = Clamp(0.7f * total / weight + 0.3f * minimum);
    return report;
}
Money Animals::Value(const Env& env, const Animal& animal) const {
    const SpeciesDef& species = env.species.All()[static_cast<std::size_t>(animal.species)];
    const float age = AgeYears(animal, env.time);
    const float mature = species.maturityYears;
    const float lifespan = species.lifespanYears * (0.85f + 0.3f * HashFloat(animal.id.value, 77u));
    float ageFactor =
        age < mature ? Lerp(tuning::kValueBirthFactor, 1.0f, Clamp(age / std::max(0.001f, mature))) : 1.0f;
    const float declineStart = lifespan * tuning::kValueMatureEndFraction;
    if (age > declineStart) {
        ageFactor = Lerp(1.0f, tuning::kValueOldAgeFactor,
                         Clamp((age - declineStart) / std::max(0.001f, lifespan - declineStart)));
    }
    const double amount =
        static_cast<double>(species.price) * 100.0 * static_cast<double>(ageFactor) *
        static_cast<double>(tuning::kValueHealthBase + tuning::kValueHealthFactor * animal.health);
    return static_cast<Money>(std::llround(amount));
}
bool Animals::Crate(AnimalId id) {
    Animal* animal = Find(id);
    if (animal == nullptr || !animal->InWorld())
        return false;
    animal->activity = AnimalActivity::Crated;
    animal->moving = false;
    animal->speed = 0.0f;
    animal->shelter = {};
    animal->route.clear();
    animal->routeIndex = 0;
    return true;
}
bool Animals::Uncrate(const Env& env, AnimalId id, Vec2 position) {
    Animal* animal = Find(id);
    if (animal == nullptr || animal->InWorld() || !env.world.InBounds(World::TileAt(position)) ||
        !env.nav.Passable(World::TileAt(position), NavProfile::Animal(0.0f, false)))
        return false;
    animal->position = position;
    animal->target = position;
    animal->activity = AnimalActivity::Idle;
    animal->activityLeft = 0;
    animal->home = env.enclosures.At(position);
    animal->loose = false;
    return true;
}
bool Animals::Recapture(const Env& env, AnimalId id) {
    Animal* animal = Find(id);
    if (animal == nullptr || !animal->loose)
        return false;
    const Enclosure* home = env.enclosures.Find(animal->home);
    if (home == nullptr) {
        animal->activity = AnimalActivity::Crated;
        animal->loose = false;
        if (env.events != nullptr)
            env.Emit(MakeEvent(EventType::AnimalRecaptured, id, {}, animal->position));
        return true;
    }
    const NavProfile profile = NavProfile::Animal(0.0f, false);
    float best = std::numeric_limits<float>::max();
    Vec2 spot{};
    bool found = false;
    for (int z = home->min.z; z <= home->max.z; ++z) {
        for (int x = home->min.x; x <= home->max.x; ++x) {
            const TileCoord tile{x, z};
            if (env.enclosures.At(tile) != home->id || !env.nav.Passable(tile, profile))
                continue;
            const Vec2 candidate = World::TileCenter(tile);
            const float distance = DistanceSquared(candidate, World::TileCenter(home->anchor));
            if (distance < best) {
                best = distance;
                spot = candidate;
                found = true;
            }
        }
    }
    if (!found) {
        animal->activity = AnimalActivity::Crated;
        animal->loose = false;
    } else {
        animal->position = spot;
        animal->target = spot;
        animal->activity = AnimalActivity::Idle;
        animal->loose = false;
    }
    if (env.events != nullptr)
        env.Emit(
            MakeEvent(EventType::AnimalRecaptured, id, found ? home->id : EnclosureId{}, animal->position));
    return true;
}

int Animals::FoodFor(const Env& env, const Prop& feeder) const {
    if (feeder.def < 0 || static_cast<std::size_t>(feeder.def) >= env.catalog.props.size())
        return -1;
    const PropDef& def = env.catalog.props[static_cast<std::size_t>(feeder.def)];
    if (def.role != PropRole::Feeder)
        return -1;
    const EnclosureId enclosure = env.enclosures.At(feeder.position);
    if (!enclosure)
        return -1;
    std::vector<int> counts(env.catalog.foods.size(), 0);
    for (const Animal& animal : animals_) {
        if (!animal.InWorld() || env.enclosures.At(animal.position) != enclosure)
            continue;
        const SpeciesDef& species = env.species.All()[static_cast<std::size_t>(animal.species)];
        for (const std::string& food : species.foods) {
            const int index = env.catalog.FoodIndex(food);
            if (index >= 0 && std::find(def.foods.begin(), def.foods.end(), food) != def.foods.end()) {
                ++counts[static_cast<std::size_t>(index)];
            }
        }
    }
    int choice = -1;
    int highest = 0;
    for (const std::string& food : def.foods) {
        const int index = env.catalog.FoodIndex(food);
        if (index >= 0 && counts[static_cast<std::size_t>(index)] > highest) {
            highest = counts[static_cast<std::size_t>(index)];
            choice = index;
        }
    }
    return choice;
}
Money Animals::RefillFeeder(Env& env, PropId id) {
    Prop* feeder = env.objects.FindProp(id);
    if (feeder == nullptr || feeder->def < 0 ||
        static_cast<std::size_t>(feeder->def) >= env.catalog.props.size())
        return 0;
    const PropDef& def = env.catalog.props[static_cast<std::size_t>(feeder->def)];
    if (def.role != PropRole::Feeder)
        return 0;
    const int food = FoodFor(env, *feeder);
    if (food < 0) {
        feeder->amount = 0.0f;
        feeder->food = -1;
        return 0;
    }
    const FoodDef& foodDef = env.catalog.foods[static_cast<std::size_t>(food)];
    float amount = def.capacity;
    if (env.finance != nullptr && foodDef.costPerKg > 0) {
        const Money affordable = std::max<Money>(0, env.finance->Balance());
        const double kg = static_cast<double>(affordable) / static_cast<double>(foodDef.costPerKg);
        amount = std::min(amount, static_cast<float>(kg));
    }
    const Money price =
        static_cast<Money>(std::ceil(static_cast<double>(amount) * static_cast<double>(foodDef.costPerKg)));
    Money paid = price;
    if (env.finance != nullptr && price > 0 && !env.finance->TrySpend(Account::AnimalFood, price)) {
        const Money balance = std::max<Money>(0, env.finance->Balance());
        amount = std::min(amount, static_cast<float>(static_cast<double>(balance) /
                                                     static_cast<double>(foodDef.costPerKg)));
        paid = static_cast<Money>(
            std::ceil(static_cast<double>(amount) * static_cast<double>(foodDef.costPerKg)));
        if (paid > balance || (paid > 0 && !env.finance->TrySpend(Account::AnimalFood, paid))) {
            amount = 0.0f;
            paid = 0;
        }
    }
    feeder->amount = amount;
    feeder->food = amount > 0.0f ? food : -1;
    feeder->filledDay = static_cast<float>(env.time.Days());
    return env.finance == nullptr || amount <= 0.0f ? 0 : paid;
}
void Animals::RefillTrough(Env& env, PropId id) {
    Prop* prop = env.objects.FindProp(id);
    if (prop != nullptr && prop->def >= 0 && static_cast<std::size_t>(prop->def) < env.catalog.props.size() &&
        env.catalog.props[static_cast<std::size_t>(prop->def)].role == PropRole::WaterTrough) {
        prop->amount = env.catalog.props[static_cast<std::size_t>(prop->def)].capacity;
    }
}
float Animals::EdibleKg(const Env& env, const Prop& feeder) {
    if (feeder.amount <= 0.0f || feeder.food < 0 ||
        static_cast<std::size_t>(feeder.food) >= env.catalog.foods.size())
        return 0.0f;
    const FoodDef& food = env.catalog.foods[static_cast<std::size_t>(feeder.food)];
    const float age = static_cast<float>(env.time.Days()) - feeder.filledDay;
    return age <= food.shelfLifeDays ? feeder.amount : 0.0f;
}
float Animals::Dirt(EnclosureId enclosure) const {
    for (const EnclosureCare& care : care_)
        if (care.id == enclosure)
            return care.dirt;
    return 0.0f;
}
void Animals::Clean(EnclosureId enclosure, float amount) {
    for (EnclosureCare& care : care_)
        if (care.id == enclosure) {
            care.dirt = Clamp(care.dirt - std::max(0.0f, amount));
            return;
        }
}
bool Animals::Treat(AnimalId id) {
    Animal* animal = Find(id);
    if (animal == nullptr || !animal->sick)
        return false;
    animal->treated = true;
    animal->treatedTick = -1;
    return true;
}
void Animals::SetViewers(EnclosureId enclosure, int viewers) {
    auto it = std::lower_bound(
        care_.begin(), care_.end(), enclosure.value,
        [](const EnclosureCare& item, std::uint32_t value) { return item.id.value < value; });
    if (it == care_.end() || it->id != enclosure) {
        it = care_.insert(it, EnclosureCare{enclosure});
    }
    it->viewers = std::max(0, viewers);
}
int Animals::Viewers(EnclosureId enclosure) const {
    for (const EnclosureCare& care : care_)
        if (care.id == enclosure)
            return care.viewers;
    return 0;
}

void Animals::AddStaffLockout(EnclosureId enclosure) {
    if (!enclosure) {
        return;
    }
    const auto found =
        std::lower_bound(staffLockouts_.begin(), staffLockouts_.end(), enclosure.value,
                         [](EnclosureId candidate, std::uint32_t value) { return candidate.value < value; });
    if (found == staffLockouts_.end() || *found != enclosure) {
        staffLockouts_.insert(found, enclosure);
    }
}

bool Animals::StaffLockout(EnclosureId enclosure) const {
    return std::binary_search(staffLockouts_.begin(), staffLockouts_.end(), enclosure);
}

bool Animals::SecureForStaff(const Env& env, EnclosureId enclosureId) {
    const Enclosure* enclosure = env.enclosures.Find(enclosureId);
    if (enclosure == nullptr) {
        return false;
    }

    std::vector<std::size_t> animalIndexes;
    for (std::size_t index = 0; index < animals_.size(); ++index) {
        const Animal& animal = animals_[index];
        if (!animal.InWorld()) {
            continue;
        }
        const EnclosureId standing = env.enclosures.At(animal.position);
        if (animal.home == enclosureId && standing != enclosureId) {
            return false;
        }
        if (standing == enclosureId) {
            // A routine keeper recall can take two animal-clock hours before the last resident
            // is released and reaches its provisions. Leave a group out if it needs that time.
            constexpr float holdDays = 2.0f / 24.0f;
            if (animal.thirst + holdDays * tuning::kThirstPerDay >= tuning::kNeedIssue ||
                animal.hunger + holdDays * tuning::kHungerPerDay >= tuning::kNeedIssue) {
                return false;
            }
            animalIndexes.push_back(index);
        }
    }
    if (animalIndexes.empty()) {
        return false;
    }

    struct ShelterSlot {
        BuildingId building;
        Vec2 front;
    };
    std::vector<ShelterSlot> slots;
    for (BuildingId buildingId : enclosure->shelters) {
        const Building* building = env.objects.FindBuilding(buildingId);
        if (building == nullptr || !building->open || building->def < 0 ||
            static_cast<std::size_t>(building->def) >= env.catalog.buildings.size()) {
            continue;
        }
        const BuildingDef& definition = env.catalog.buildings[static_cast<std::size_t>(building->def)];
        const TileCoord front = FrontTile(definition, building->origin, building->facing);
        if (definition.role != BuildingRole::AnimalShelter || definition.capacity <= 0 ||
            env.enclosures.At(front) != enclosureId) {
            continue;
        }
        for (int place = 0; place < definition.capacity; ++place) {
            slots.push_back({buildingId, World::TileCenter(front)});
        }
    }
    if (slots.size() < animalIndexes.size()) {
        return false;
    }

    std::vector<int> occupant(slots.size(), -1);
    std::function<bool(int, std::vector<bool>&)> assign = [&](int animalNumber, std::vector<bool>& visited) {
        const Animal& animal = animals_[animalIndexes[static_cast<std::size_t>(animalNumber)]];
        if (animal.species < 0 || static_cast<std::size_t>(animal.species) >= env.species.All().size()) {
            return false;
        }
        const SpeciesDef& species = env.species.All()[static_cast<std::size_t>(animal.species)];
        for (int preference = 0; preference < 2; ++preference) {
            for (std::size_t slot = 0; slot < slots.size(); ++slot) {
                if (visited[slot] || (slots[slot].building == animal.shelter) != (preference == 0) ||
                    !env.nav.Passable(World::TileAt(slots[slot].front),
                                      NavProfile::Animal(0.0f, species.canSwim))) {
                    continue;
                }
                visited[slot] = true;
                if (occupant[slot] < 0 || assign(occupant[slot], visited)) {
                    occupant[slot] = animalNumber;
                    return true;
                }
            }
        }
        return false;
    };
    for (int animal = 0; animal < static_cast<int>(animalIndexes.size()); ++animal) {
        std::vector<bool> visited(slots.size(), false);
        if (!assign(animal, visited)) {
            return false;
        }
    }

    std::vector<std::size_t> chosen(animalIndexes.size());
    for (std::size_t slot = 0; slot < slots.size(); ++slot) {
        if (occupant[slot] >= 0) {
            chosen[static_cast<std::size_t>(occupant[slot])] = slot;
        }
    }
    // Capacity alone is not enough: a door on a disconnected dry island would start a
    // fail-closed recall which can never finish. Check the fallback grid route used by
    // shelter errands before changing any animal, preserving the operation's atomic failure.
    std::vector<Vec2> route;
    for (std::size_t number = 0; number < animalIndexes.size(); ++number) {
        const Animal& animal = animals_[animalIndexes[number]];
        const ShelterSlot& slot = slots[chosen[number]];
        if (animal.shelter == slot.building || animal.position == slot.front) {
            continue;
        }
        const SpeciesDef& species = env.species.All()[static_cast<std::size_t>(animal.species)];
        if (!env.nav.FindPath(animal.position, slot.front, NavProfile::Animal(0.0f, species.canSwim),
                              route)) {
            return false;
        }
    }
    for (std::size_t number = 0; number < animalIndexes.size(); ++number) {
        Animal& animal = animals_[animalIndexes[number]];
        const ShelterSlot& slot = slots[chosen[number]];
        animal.target = slot.front;
        animal.speed = 0.0f;
        animal.home = enclosureId;
        animal.loose = false;
        animal.activity = AnimalActivity::Shelter;
        animal.activityLeft = 0;
        animal.activityTicks = 0;
        if (animal.shelter != slot.building) {
            animal.shelter = {};
        }
        animal.moving = !animal.shelter && DistanceSquared(animal.position, slot.front) >
                                               tuning::kMovementStartDistanceSquaredM2;
        animal.targetProp = {};
        animal.route.clear();
        animal.routeIndex = 0;
        animal.following = {};
    }
    return true;
}

bool Animals::AvoidKeeper(Animal& animal, const SpeciesDef& species, const Env& env, EnclosureId enclosure,
                          bool urgentNeed) {
    if (!enclosure || animal.shelter || animal.loose || urgentNeed ||
        animal.activity == AnimalActivity::BreakOut || species.shyness <= tuning::kKeeperShynessThreshold) {
        return false;
    }
    const KeeperPresence* nearest = nullptr;
    float nearestDistance = std::numeric_limits<float>::max();
    for (const KeeperPresence& keeper : keepers_) {
        if (keeper.enclosure != enclosure) {
            continue;
        }
        const float distance = DistanceSquared(animal.position, keeper.position);
        if (distance < nearestDistance) {
            nearest = &keeper;
            nearestDistance = distance;
        }
    }
    if (nearest == nullptr) {
        return false;
    }
    const float scale = BodyScale(animal, species, env.time);
    const float flight = species.bodyRadiusM * scale + tuning::kKeeperFlightBaseM +
                         species.shyness * tuning::kKeeperFlightShynessM;
    if (nearestDistance >= flight * flight) {
        return false;
    }
    // Do not choose a fresh point every tick while the existing retreat still leads farther away.
    if (animal.moving && DistanceSquared(animal.target, nearest->position) > nearestDistance + 1.0f &&
        Dot(animal.target - animal.position, animal.position - nearest->position) > 0.0f) {
        return true;
    }
    Vec2 away = animal.position - nearest->position;
    if (LengthSquared(away) < 1.0e-6f) {
        const float angle = 2.0f * kPi * HashFloat(animal.id.value, 0x4b454550U);
        away = {std::sin(angle), std::cos(angle)};
    } else {
        away = Normalized(away);
    }
    const float wadeDepth =
        species.canSwim ? tuning::kWadingShoulderFraction * species.shoulderHeightM * scale : 0.0f;
    const NavProfile profile = NavProfile::Animal(wadeDepth, species.canSwim);
    constexpr std::array<float, 7> turns{0.0f, 0.45f, -0.45f, 0.9f, -0.9f, 1.35f, -1.35f};
    for (const float turn : turns) {
        const float sine = std::sin(turn);
        const float cosine = std::cos(turn);
        const Vec2 direction{away.x * cosine - away.z * sine, away.x * sine + away.z * cosine};
        const Vec2 target = animal.position + direction * tuning::kKeeperRetreatM;
        if (env.enclosures.At(target) != enclosure || !env.nav.Passable(World::TileAt(target), profile) ||
            !env.nav.Walkable(animal.position, target, profile)) {
            continue;
        }
        animal.activity = AnimalActivity::Wander;
        animal.target = target;
        animal.targetProp = {};
        animal.activityLeft = tuning::kKeeperRetreatSeconds * kTicksPerSecond;
        animal.activityTicks = 0;
        animal.moving = true;
        animal.route.clear();
        animal.routeIndex = 0;
        return true;
    }
    return false;
}

void Animals::StepAnimals(Env& env, const std::vector<Enclosure>& enclosures, int& pathCalls) {
    std::vector<Vec2> people;
    for (std::size_t index = 0; index < animals_.size();) {
        Animal& animal = animals_[index];
        if (!animal.InWorld()) {
            ++index;
            continue;
        }
        const EnclosureId standingEnclosure = env.enclosures.At(animal.position);
        if (!standingEnclosure && !animal.loose) {
            env.Emit(MakeEvent(EventType::AnimalEscaped, animal.id, animal.home, animal.position));
            animal.loose = true;
            ++escapes_;
        } else if (standingEnclosure) {
            animal.home = standingEnclosure;
            animal.loose = false;
        }
        const SpeciesDef& species = env.species.All()[static_cast<std::size_t>(animal.species)];
        const float scale = BodyScale(animal, species, env.time);
        const float dailyKg = species.foodPerDayKg * scale * scale;
        if (animal.activity == AnimalActivity::Eat && animal.targetProp && !animal.moving) {
            Prop* feeder = env.objects.FindProp(animal.targetProp);
            if (feeder == nullptr || EdibleKg(env, *feeder) <= 0.0f) {
                animal.activity = AnimalActivity::Idle;
                animal.targetProp = {};
                animal.activityLeft = 0;
                animal.moving = false;
                animal.route.clear();
                animal.routeIndex = 0;
            } else {
                const float remainingTicks = static_cast<float>(std::max(1, animal.activityLeft));
                const float wanted =
                    std::min(EdibleKg(env, *feeder),
                             animal.hunger * tuning::kFoodKgPerHunger * dailyKg / remainingTicks);
                const float consumed = std::min(wanted, feeder->amount);
                feeder->amount -= consumed;
                animal.hunger =
                    Clamp(animal.hunger - consumed / std::max(0.001f, tuning::kFoodKgPerHunger * dailyKg));
            }
        }
        const EnclosureId enclosureId = env.enclosures.At(animal.position);
        const Enclosure* enclosure = env.enclosures.Find(enclosureId);
        // A recall is ordinary visible movement until the animal reaches its assigned
        // door. The saved staff lockout keeps needs advancing but prevents a fresh
        // behaviour decision from replacing that route on the way.
        const bool heldForStaff =
            StaffLockout(enclosureId) && (animal.shelter || animal.activity == AnimalActivity::Shelter);
        if (animal.shelter && !heldForStaff) {
            const Building* shelter = env.objects.FindBuilding(animal.shelter);
            bool validShelter = shelter != nullptr && shelter->open && shelter->def >= 0 &&
                                static_cast<std::size_t>(shelter->def) < env.catalog.buildings.size();
            if (validShelter) {
                const BuildingDef& definition = env.catalog.buildings[static_cast<std::size_t>(shelter->def)];
                const TileCoord front = FrontTile(definition, shelter->origin, shelter->facing);
                const bool registered =
                    enclosure != nullptr && std::find(enclosure->shelters.begin(), enclosure->shelters.end(),
                                                      animal.shelter) != enclosure->shelters.end();
                validShelter = definition.role == BuildingRole::AnimalShelter && enclosure != nullptr &&
                               registered && env.enclosures.At(front) == enclosureId &&
                               DistanceSquared(animal.position, World::TileCenter(front)) <=
                                   tuning::kWaypointArrivalM * tuning::kWaypointArrivalM &&
                               env.nav.Passable(front, NavProfile::Animal(0.0f, species.canSwim));
            }
            if (!validShelter) {
                animal.shelter = {};
                animal.activity = AnimalActivity::Idle;
                animal.activityLeft = 0;
                animal.moving = false;
                animal.route.clear();
                animal.routeIndex = 0;
            }
        }
        const Animal* mother = nullptr;
        // A duckling follows its hen for cover but gathers its own food from hatching.
        if (animal.mother && !species.independentYoung && species.group != "duck") {
            const auto foundMother = std::lower_bound(
                animals_.begin(), animals_.end(), animal.mother.value,
                [](const Animal& candidate, std::uint32_t value) { return candidate.id.value < value; });
            if (foundMother != animals_.end() && foundMother->id == animal.mother) {
                mother = &*foundMother;
            }
        }
        bool predatorPresent = false;
        if (enclosure != nullptr) {
            const std::size_t enclosureIndex = static_cast<std::size_t>(enclosure - enclosures.data());
            for (std::size_t memberIndex : enclosureScratch_[enclosureIndex]) {
                const Animal& other = animals_[memberIndex];
                if (other.id == animal.id || other.species == animal.species) {
                    continue;
                }
                const SpeciesDef& otherSpecies = env.species.All()[static_cast<std::size_t>(other.species)];
                if (otherSpecies.dangerous &&
                    (otherSpecies.diet == Diet::Carnivore || otherSpecies.diet == Diet::Omnivore) &&
                    species.adultMassKg < tuning::kPredatorMassRatio * otherSpecies.adultMassKg) {
                    predatorPresent = true;
                    break;
                }
            }
        }
        animal_internal::AdvanceNeeds(animal, species, env,
                                      enclosure == nullptr ? nullptr : &enclosure->stats,
                                      Viewers(enclosureId), mother, predatorPresent);
        std::span<const std::size_t> neighbors;
        if (enclosure != nullptr) {
            const std::size_t enclosureIndex = static_cast<std::size_t>(enclosure - enclosures.data());
            neighbors = enclosureScratch_[enclosureIndex];
        }
        if (animal_internal::FeedingActivity(animal.activity) && animal.moving == false &&
            species.diet != Diet::Carnivore && species.diet != Diet::Piscivore) {
            const Terrain terrain = env.world.At(World::TileAt(animal.position)).terrain;
            const TileCoord tile = World::TileAt(animal.position);
            if (!env.world.HasWater(tile) && (terrain == Terrain::Grass || terrain == Terrain::Savanna ||
                                              terrain == Terrain::ForestFloor)) {
                animal.hunger = std::min(
                    animal.hunger, std::max(tuning::kForageMinimumHunger,
                                            animal.hunger - tuning::kForageHungerPerDay * kDaysPerTick));
            }
        }
        if (animal.activity == AnimalActivity::Drink && animal.targetProp && !animal.moving) {
            Prop* trough = env.objects.FindProp(animal.targetProp);
            if (trough != nullptr && trough->amount > 0.0f) {
                const float remainingTicks = static_cast<float>(std::max(1, animal.activityLeft));
                const float desired = animal.thirst * species.waterPerDayL * scale * scale / remainingTicks;
                const float water = std::min(desired, trough->amount);
                trough->amount -= water;
                animal.thirst =
                    Clamp(animal.thirst - water / std::max(0.001f, species.waterPerDayL * scale * scale));
            } else {
                animal.activity = AnimalActivity::Idle;
                animal.targetProp = {};
                animal.moving = false;
                animal.route.clear();
                animal.routeIndex = 0;
                animal.activityLeft = 0;
            }
        } else if (animal.activity == AnimalActivity::Drink && !animal.moving) {
            const float remainingTicks = static_cast<float>(std::max(1, animal.activityLeft));
            animal.thirst = Clamp(animal.thirst - animal.thirst / remainingTicks);
        }
        const auto [eatUrgency, drinkUrgency] =
            animal_internal::ProvisionThresholds(animal, species, env, enclosure);
        const bool urgentDrink = animal.thirst > drinkUrgency;
        const bool urgentEat = animal.hunger > eatUrgency;
        bool urgentInterrupt = false;
        if (animal.activity != AnimalActivity::BreakOut && env.time.tick >= animal.nextNeedCheck) {
            // A drink satisfies the higher-priority need without restarting it for hunger each tick.
            if (animal.activity == AnimalActivity::Drink) {
                urgentInterrupt = false;
            } else if (animal.activity == AnimalActivity::Eat) {
                urgentInterrupt = urgentDrink;
            } else {
                urgentInterrupt = urgentDrink || urgentEat;
            }
        }
        const std::int64_t escapeTick = env.time.tick + static_cast<std::int64_t>(animal.id.value);
        if (!heldForStaff && escapeTick % kTicksPerGameHour == 0) {
            animal_internal::TryEscape(animal, species, env, rng_);
        }
        const float previousHeading = animal.heading;
        if (!heldForStaff) {
            animal_internal::UpdateSpeciesBehaviour(animal, species, env, animals_, neighbors,
                                                    Viewers(enclosureId), rng_, propScratch_);
        }
        const bool completed = animal.activityLeft <= 0 && !animal.moving;
        if (animal.targetProp && std::as_const(env.objects).FindProp(animal.targetProp) == nullptr) {
            animal.targetProp = {};
            animal.activity = AnimalActivity::Idle;
            animal.activityLeft = 0;
            animal.moving = false;
            animal.route.clear();
            animal.routeIndex = 0;
        }
        if (animal.activity == AnimalActivity::BreakOut && completed && enclosure != nullptr) {
            const EdgeRef edge = enclosure->weakestEdge;
            TileCoord firstInside{};
            TileCoord secondInside{};
            TileCoord firstOutside{};
            TileCoord secondOutside{};
            if (edge.horizontal) {
                firstInside = {edge.x, edge.z - 1};
                secondInside = {edge.x, edge.z};
                firstOutside = {edge.x, edge.z};
                secondOutside = {edge.x, edge.z - 1};
            } else {
                firstInside = {edge.x - 1, edge.z};
                secondInside = {edge.x, edge.z};
                firstOutside = {edge.x, edge.z};
                secondOutside = {edge.x - 1, edge.z};
            }
            const TileCoord outside =
                env.enclosures.At(firstInside) == enclosureId ? firstOutside : secondOutside;
            animal.target = World::TileCenter(outside);
            animal.activityLeft = kTicksPerGameHour;
            animal.moving = true;
            animal.route.clear();
            animal.routeIndex = 0;
        } else if (!heldForStaff && (urgentInterrupt || completed)) {
            animal_internal::DecideActivity(animal, species, env, animals_, neighbors, rng_, propScratch_,
                                            Viewers(enclosureId));
            animal.nextNeedCheck = env.time.tick + 2 * kTicksPerSecond;
        }
        AvoidKeeper(animal, species, env, enclosureId, urgentDrink || urgentEat);
        if (heldForStaff && !animal.shelter && !animal.moving &&
            DistanceSquared(animal.position, animal.target) > tuning::kMovementStartDistanceSquaredM2) {
            // Ordinary errands may be abandoned after their movement budget. A safety recall
            // instead retries deterministically while the keeper remains at the outside control.
            animal.moving = true;
            animal.activityTicks = 0;
            animal.route.clear();
            animal.routeIndex = 0;
        }
        if ((animal.activity == AnimalActivity::Eat || animal.activity == AnimalActivity::Drink ||
             animal.activity == AnimalActivity::Play) &&
            animal.targetProp) {
            const Prop* targetProp = std::as_const(env.objects).FindProp(animal.targetProp);
            if (targetProp == nullptr) {
                animal.activity = AnimalActivity::Idle;
                animal.targetProp = {};
                animal.moving = false;
                animal.route.clear();
            } else {
                const float propRadius =
                    targetProp->def >= 0 &&
                            static_cast<std::size_t>(targetProp->def) < env.catalog.props.size()
                        ? env.catalog.props[static_cast<std::size_t>(targetProp->def)].radiusM
                        : tuning::kDefaultPropRadiusM;
                const float radius = propRadius + species.bodyRadiusM * scale + tuning::kObstacleClearanceM;
                Vec2 desiredTarget = animal.target;
                float nearestTargetDistance = std::numeric_limits<float>::max();
                for (int slot = 0; slot < static_cast<int>(tuning::kFeederShareCount); ++slot) {
                    const float angle = static_cast<float>(slot) * (kPi * 0.5f);
                    const Vec2 candidate =
                        targetProp->position + Vec2{std::sin(angle), std::cos(angle)} * radius;
                    const float distance = DistanceSquared(animal.target, candidate);
                    if (distance < nearestTargetDistance) {
                        nearestTargetDistance = distance;
                        desiredTarget = candidate;
                    }
                }
                if (DistanceSquared(animal.target, desiredTarget) > tuning::kTargetChangeToleranceSquared) {
                    animal.target = desiredTarget;
                    animal.route.clear();
                    animal.routeIndex = 0;
                    animal.moving = DistanceSquared(animal.position, desiredTarget) >
                                    tuning::kWaypointArrivalM * tuning::kWaypointArrivalM;
                }
            }
        }
        people.clear();
        if (enclosure && enclosure->walkThrough) {
            for (const auto& person : keepers_) {
                if (person.enclosure == enclosureId)
                    people.push_back(person.position);
            }
        }
        animal_internal::AdvanceMovement(animal, species, env, pathCalls, animals_, neighbors, propScratch_,
                                         navScratch_, people);
        animal_internal::UpdateFlockRest(animal, species, env, animals_, neighbors);
        animal.heading = TurnTowards(previousHeading, animal.heading,
                                     tuning::kTurnRadiansPerSecond * tuning::kMotionSecondsPerTick);
        if ((animal.activity == AnimalActivity::Shelter || animal.activity == AnimalActivity::Sleep) &&
            !animal.moving && !animal.shelter && enclosure != nullptr) {
            for (BuildingId buildingId : enclosure->shelters) {
                const Building* building = env.objects.FindBuilding(buildingId);
                if (building == nullptr || building->def < 0 ||
                    static_cast<std::size_t>(building->def) >= env.catalog.buildings.size()) {
                    continue;
                }
                const BuildingDef& definition =
                    env.catalog.buildings[static_cast<std::size_t>(building->def)];
                const Vec2 front =
                    World::TileCenter(FrontTile(definition, building->origin, building->facing));
                // At the door (or as near to it as the others let it come): in it goes.
                if (DistanceSquared(animal.target, front) <=
                        tuning::kWaypointArrivalM * tuning::kWaypointArrivalM &&
                    DistanceSquared(animal.position, front) <=
                        tuning::kShelterDoorM * tuning::kShelterDoorM) {
                    animal.shelter = buildingId;
                    animal.position = front;
                    animal.speed = 0.0f;
                    break;
                }
            }
        }
        DeathCause cause = DeathCause::Illness;
        if (animal_internal::AdvanceHealth(animal, species, env, cause)) {
            env.Emit(MakeEvent(EventType::AnimalDied, animal.id, animal.home, animal.position,
                               static_cast<int>(cause)));
            ForgetAnimalReferences(animal.id);
            animals_.erase(animals_.begin() + static_cast<std::ptrdiff_t>(index));
            ++deaths_;
            continue;
        }
        if ((env.time.tick + static_cast<std::int64_t>(animal.id.value)) % kTicksPerGameHour == 0) {
            const GroupInfo group = GroupFromMembers(env, animal, animals_, neighbors);
            const WelfareReport* habitat = nullptr;
            if (enclosure != nullptr) {
                habitat = &CachedHabitat(env, animal, group);
            }
            const WelfareReport report = WelfareWithGroup(env, animal, group, habitat);
            const float previous = animal.welfare;
            animal.welfare += tuning::kWelfareSmoothing * (report.overall - animal.welfare);
            animal.issues = report.issues;
            if (previous >= tuning::kWelfareAlertThreshold &&
                animal.welfare < tuning::kWelfareAlertThreshold) {
                env.Emit(MakeEvent(EventType::AnimalUnhappy, animal.id, animal.home, animal.position));
            }
        }
        ++animal.activityTicks;
        if (animal.activityLeft > 0 && !animal.moving)
            --animal.activityLeft;
        ++index;
    }
}

void Animals::Step(Env& env) {
    int pathCalls = 0;
    const std::vector<Enclosure>& enclosures = env.enclosures.All();
    if (env.time.tick % kTicksPerGameHour == 0) {
        AdvanceLife(env);
    }
    GroupAnimals(env);
    UpdateEnclosureDirt(env);
    StepAnimals(env, enclosures, pathCalls);
    care_.erase(
        std::remove_if(care_.begin(), care_.end(),
                       [&env](const EnclosureCare& care) { return env.enclosures.Find(care.id) == nullptr; }),
        care_.end());
}

std::uint64_t Animals::StateHash() const {
    std::uint64_t hash = 14695981039346656037ULL;
    const auto add = [&hash](std::uint64_t value) {
        hash ^= value;
        hash *= 1099511628211ULL;
    };
    add(nextId_);
    add(rng_.State());
    add(rng_.Stream());
    add(static_cast<std::uint64_t>(births_));
    add(static_cast<std::uint64_t>(deaths_));
    add(static_cast<std::uint64_t>(escapes_));
    for (const Animal& animal : animals_) {
        add(animal.id.value);
        add(static_cast<std::uint64_t>(animal.species));
        for (unsigned char c : animal.name)
            add(c);
        add(static_cast<std::uint64_t>(animal.sex));
        add(static_cast<std::uint64_t>(animal.bornTick));
        add(animal.mother.value);
        add(animal.father.value);
        const float values[] = {animal.position.x, animal.position.z, animal.heading, animal.speed,
                                animal.hunger,     animal.thirst,     animal.fatigue, animal.boredom,
                                animal.stress,     animal.health,     animal.welfare, animal.target.x,
                                animal.target.z};
        for (float value : values)
            add(std::bit_cast<std::uint32_t>(value));
        add(animal.issues);
        add(animal.sick);
        add(animal.treated);
        add(static_cast<std::uint64_t>(animal.sickSinceTick));
        add(static_cast<std::uint64_t>(animal.treatedTick));
        add(animal.alerts);
        add(static_cast<std::uint64_t>(animal.activity));
        add(animal.moving);
        add(static_cast<std::uint32_t>(animal.activityTicks));
        add(static_cast<std::uint32_t>(animal.activityLeft));
        add(animal.targetProp.value);
        add(animal.shelter.value);
        add(animal.routeIndex);
        add(std::bit_cast<std::uint32_t>(animal.stallBest));
        add(static_cast<std::uint32_t>(animal.stallTicks));
        add(animal.onDetour);
        add(animal.detours);
        add(static_cast<std::uint64_t>(animal.nextNeedCheck));
        add(static_cast<std::uint64_t>(animal.answeredCallTick));
        add(static_cast<std::uint64_t>(animal.lastCallTick));
        add(std::bit_cast<std::uint32_t>(animal.attentionHeading));
        add(static_cast<std::uint32_t>(animal.attentionLeft));
        add(static_cast<std::uint32_t>(animal.stillTicks));
        add(animal.following.value);
        add(animal.home.value);
        add(animal.loose);
        add(animal.breeding);
        add(static_cast<std::uint64_t>(animal.dueTick));
        add(animal.mate.value);
        add(static_cast<std::uint64_t>(animal.lastBirthTick));
        for (Vec2 point : animal.route) {
            add(std::bit_cast<std::uint32_t>(point.x));
            add(std::bit_cast<std::uint32_t>(point.z));
        }
    }
    for (const EnclosureCare& care : care_) {
        add(care.id.value);
        add(std::bit_cast<std::uint32_t>(care.dirt));
    }
    return hash;
}
bool Animals::operator==(const Animals& other) const {
    if (animals_ != other.animals_ || care_.size() != other.care_.size() || nextId_ != other.nextId_ ||
        rng_.State() != other.rng_.State() || rng_.Stream() != other.rng_.Stream() ||
        births_ != other.births_ || deaths_ != other.deaths_ || escapes_ != other.escapes_)
        return false;
    for (std::size_t i = 0; i < care_.size(); ++i) {
        if (care_[i].id != other.care_[i].id || care_[i].dirt != other.care_[i].dirt ||
            care_[i].viewers != other.care_[i].viewers)
            return false;
    }
    return true;
}
}  // namespace zoo
