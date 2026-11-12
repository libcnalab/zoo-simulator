// SPDX-License-Identifier: MIT
#include <gtest/gtest.h>

#include "zoo/animals/Animals.hpp"

namespace zoo {

TEST(AnimalWelfareTests, SpaceAndContainmentUseLimitingRequirements) {
    SpeciesDef species;
    species.groupMin = 2;
    species.minEnclosureM2 = 100.0f;
    species.spacePerExtraAnimalM2 = 20.0f;
    EXPECT_FLOAT_EQ(SpaceNeededM2(species, 1), 100.0f);
    EXPECT_FLOAT_EQ(SpaceNeededM2(species, 4), 140.0f);

    EnclosureStats stats;
    stats.tiles = 10;
    stats.minFenceHeightM = 1.5f;
    stats.minFenceStrength = 1;
    stats.minFenceCondition = 1.0f;
    species.fenceHeightM = 2.0f;
    species.fenceStrength = 2;
    EXPECT_NEAR(Containment(species, stats), 0.375f, 0.00001f);
    stats.tiles = 0;
    EXPECT_FLOAT_EQ(Containment(species, stats), 0.0f);
}

TEST(AnimalWelfareTests, HabitatFlagsDeepWaterSocialAndShelterIssues) {
    SpeciesDef species;
    species.minEnclosureM2 = 100.0f;
    species.groupMin = 2;
    species.groupMax = 3;
    species.maxAdultMales = 1;
    species.needsShelter = true;
    species.needsDeepWater = true;
    species.waterAreaFraction = 0.2f;
    species.playfulness = 0.0f;
    species.terrainSuitability[static_cast<std::size_t>(Terrain::Grass)] = 1.0f;
    EnclosureStats stats;
    stats.areaM2 = 100.0f;
    stats.tiles = 20;
    stats.terrainShare[static_cast<std::size_t>(Terrain::Grass)] = 0.8f;
    stats.waterShare = 0.2f;
    GroupInfo group;
    group.sameSpecies = 1;
    group.allAnimals = 2;
    group.adultMales = 2;
    WelfareReport report = EvaluateHabitat(species, stats, group);
    EXPECT_TRUE(report.Has(WelfareIssue::Lonely));
    EXPECT_TRUE(report.Has(WelfareIssue::NoDeepWater));
    EXPECT_TRUE(report.Has(WelfareIssue::NoShelter));
    EXPECT_TRUE(report.Has(WelfareIssue::TooManyMales));
    EXPECT_FLOAT_EQ(report.water, 0.4f);
}

TEST(AnimalWelfareTests, AgeAndBodyScaleAreSmoothAndBounded) {
    Animal animal;
    animal.bornTick = 0;
    SpeciesDef species;
    species.maturityYears = 2.0f;
    GameTime now{0};
    EXPECT_FLOAT_EQ(BodyScale(animal, species, now), 0.35f);
    now.tick = static_cast<std::int64_t>(species.maturityYears * kGameDaysPerRealYear * kTicksPerDay);
    EXPECT_TRUE(IsAdult(animal, species, now));
    EXPECT_FLOAT_EQ(BodyScale(animal, species, now), 1.0f);
}

}  // namespace zoo
