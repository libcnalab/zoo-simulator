// SPDX-License-Identifier: MIT
#pragma once

namespace zoo::tuning {

inline constexpr float kHungerPerDay = 0.5f;                  // hunger units per game day
inline constexpr float kThirstPerDay = 1.0f;                  // thirst units per game day
inline constexpr float kStarvingThreshold = 0.9f;             // need fraction
inline constexpr float kNeedResetThreshold = 0.6f;            // need fraction
inline constexpr float kHealthAtCriticalHungerPerDay = 0.5f;  // health units per game day
inline constexpr float kHealthAtCriticalThirstPerDay = 1.0f;  // health units per game day
inline constexpr float kHealthSickPerDay = 0.08f;             // health units per game day
inline constexpr float kHealthOldAgePerDay = 0.2f;            // health units per game day
inline constexpr float kHealthRecoveryPerDay = 0.1f;          // health units per game day
inline constexpr float kStressRisePerDay = 3.0f;              // stress units per game day
inline constexpr float kStressFallPerDay = 1.5f;              // stress units per game day
inline constexpr float kDirtyPerMassPowerAreaPerDay = 2.2f;   // dirt units per day per m2
inline constexpr float kMaximumDirtPerDay = 0.4f;             // dirt units per game day
inline constexpr float kNeedsRefreshTicks = 600.0f;           // simulation ticks per welfare refresh
inline constexpr float kMotionSecondsPerTick = 0.05f;         // motion seconds per simulation tick
inline constexpr float kMinimumActivitySeconds = 2.0f;        // motion seconds per activity
inline constexpr float kWalkAcceleration = 2.0f;              // metres per second squared
inline constexpr float kBrakeAcceleration = 4.0f;             // metres per second squared
inline constexpr float kTurnRadiansPerSecond = 3.0f;          // radians per motion second
inline constexpr float kAdultSpeedScale = 1.0f;               // speed fraction at adult size
inline constexpr float kYoungSpeedScale = 0.6f;               // speed fraction at birth size
inline constexpr float kWadingShoulderFraction = 0.6f;        // shoulder height fraction
// A wading bird chooses water that leaves its body clear: up to the joint of its legs.
inline constexpr float kWaderStandingFraction = 0.3f;  // height fraction
inline constexpr float kWaypointArrivalM = 0.05f;      // metres from route waypoint
inline constexpr float kRouteBrakingFactor = 1.0f;     // conservative braking speed equation factor
inline constexpr float kArrivalPauseSeconds = 2.0f;    // motion seconds after arrival
inline constexpr int kPathExpansionLimit = 6000;       // navigation nodes per route
inline constexpr int kPathCallsPerStep = 8;            // A* searches per simulation tick
inline constexpr float kFenceBodyClearanceM = 0.1f;    // metres from fence edge
inline constexpr float kObstacleClearanceM = 0.05f;    // metres from blocking prop
inline constexpr float kEdibleFreshnessDays = 0.0f;    // game days after filled; actual life is food-specific
inline constexpr float kWelfareAlertThreshold = 0.4f;  // welfare fraction
inline constexpr float kWelfareSmoothing = 0.5f;       // fraction applied per welfare refresh
inline constexpr float kHabitatMinimumIssue = 0.8f;    // aspect fraction
inline constexpr float kTerrainIssue = 0.6f;           // aspect fraction
inline constexpr float kWaterIssue = 0.6f;             // aspect fraction
inline constexpr float kVegetationIssue = 0.75f;       // aspect fraction
inline constexpr float kEnrichmentIssue = 0.6f;        // aspect fraction
inline constexpr float kDeepWaterCap = 0.4f;           // habitat fraction without deep water
inline constexpr float kShelterBase = 0.2f;            // habitat fraction with no shelter
inline constexpr float kShelterPartialBase = 0.5f;     // habitat fraction with partial shelter
inline constexpr float kSocialLonelyPenalty = 0.4f;    // social formula coefficient
inline constexpr float kSocialSociabilityPenalty = 0.6f;    // social formula coefficient
inline constexpr float kMaleSocialMultiplier = 0.7f;        // social fraction with surplus males
inline constexpr float kPredatorSocialCap = 0.2f;           // social fraction with predator
inline constexpr float kTemperatureComfortSpanC = 15.0f;    // degrees Celsius
inline constexpr float kTemperatureExposureRangeC = 15.0f;  // degrees Celsius beyond comfort
inline constexpr float kTemperatureIssue = 0.7f;            // comfort aspect fraction
inline constexpr float kNeedIssue = 0.7f;                   // hunger or thirst fraction
inline constexpr float kDirtyIssue = 0.6f;                  // dirt fraction
inline constexpr float kWarmthShelterFactor = 0.5f;        // remaining temperature excess in ordinary shelter
inline constexpr float kHeatShadeReliefC = 3.0f;           // degrees Celsius
inline constexpr float kSleepWakeFatigue = 0.1f;           // fatigue fraction
inline constexpr float kUrgentNeedSleep = 0.8f;            // hunger or thirst fraction
inline constexpr float kHungerUrgent = 0.4f;               // hunger fraction
inline constexpr float kThirstUrgent = 0.45f;              // thirst fraction
inline constexpr float kFeederShareCount = 4.0f;           // animals sharing one feeder
inline constexpr float kFoodKgPerHunger = 2.0f;            // daily intake multiplier
inline constexpr float kForageHungerPerDay = 0.1f;         // hunger units per equivalent day
inline constexpr float kForageMinimumHunger = 0.3f;        // lower hunger bound
inline constexpr float kBoredomPerDayMultiplier = 1.2f;    // boredom units per day per playfulness
inline constexpr float kBoredomPlayRelief = 0.5f;          // boredom units per play session
inline constexpr float kBoredomSocialRelief = 0.15f;       // boredom units per social session
inline constexpr float kBoredomWanderRelief = 0.03f;       // boredom units per wander
inline constexpr float kStressFightAddition = 0.3f;        // stress fraction per fight
inline constexpr float kEscapeStressFactor = 0.5f;         // escape chance coefficient
inline constexpr float kEscapeWelfareFactor = 0.3f;        // escape chance coefficient
inline constexpr float kEscapeBaseChance = 0.15f;          // hourly escape chance multiplier
inline constexpr float kPredationChance = 0.1f;            // predation chance per game hour
inline constexpr float kPredatorHungerRelief = 0.6f;       // hunger fraction after a kill
inline constexpr float kBreedingChance = 0.25f;            // conception chance per eligible day
inline constexpr float kSicknessBaseChance = 0.004f;       // sickness chance per game day
inline constexpr float kSicknessDirtChance = 0.03f;        // sickness chance per dirt unit per day
inline constexpr float kSicknessWelfareChance = 0.03f;     // sickness chance per missing welfare per day
inline constexpr float kSicknessOldAgeChance = 0.02f;      // added sickness chance near lifespan
inline constexpr float kNaturalRecoveryChance = 0.15f;     // daily chance of untreated recovery
inline constexpr float kFightChance = 0.5f;                // fight chance multiplier per game day
inline constexpr float kFightHealthLossMin = 0.05f;        // health fraction per fighter
inline constexpr float kFightHealthLossMax = 0.15f;        // health fraction per fighter
inline constexpr float kConceptionWelfareMinimum = 0.55f;  // welfare fraction
inline constexpr float kBreedingLifespanFraction = 0.85f;  // lifespan fraction
inline constexpr float kSicknessOldAgeFraction = 0.8f;     // lifespan fraction
inline constexpr float kJuvenileSuckleFraction = 0.25f;    // maturity age fraction
inline constexpr float kJuvenileFollowFraction = 0.5f;     // maturity age fraction
inline constexpr float kJuvenileNeedCap = 0.3f;            // hunger and thirst fraction
inline constexpr float kValueBirthFactor = 0.4f;           // value fraction at birth
inline constexpr float kValueMatureEndFraction = 0.5f;     // lifespan fraction
inline constexpr float kValueOldAgeFactor = 0.25f;         // value fraction at lifespan
inline constexpr float kValueHealthBase = 0.4f;            // value fraction at zero health
inline constexpr float kValueHealthFactor = 0.6f;          // added value fraction at full health
inline constexpr float kAwakeHoursPerDay = 24.0f;          // hours per game day
inline constexpr float kCircadianActive = -0.3f;           // fatigue bias, fraction
inline constexpr float kCircadianSleep = 0.5f;             // fatigue bias, fraction
inline constexpr float kCrowdStressToleranceBase = 10.0f;  // viewers
inline constexpr float kCrowdStressToleranceShyness = 30.0f;  // viewers per shyness fraction
inline constexpr float kPredatorStressTarget = 0.9f;          // stress fraction
inline constexpr float kLooseStressTarget = 0.8f;             // stress fraction
inline constexpr float kPoorHabitatStressFactor = 0.6f;       // stress target multiplier
inline constexpr float kFatigueAwakeMultiplier = 1.0f;        // fatigue per awake hour numerator
inline constexpr float kFatigueRestMultiplier = 0.5f;         // fatigue reduction per rest hour
inline constexpr float kFatigueSleepMultiplier = 1.0f;        // fatigue reduction per sleep hour
inline constexpr float kSecondsPerMinute = 60.0f;             // motion seconds per minute
inline constexpr float kSecondsPerHour = 3600.0f;             // motion seconds per hour
inline constexpr float kNeedSuckleThreshold = 0.9f;           // maternal hunger fraction
inline constexpr float kShelterDecisionTemperatureC = 5.0f;   // degrees outside comfort range
inline constexpr float kSleepDecisionThreshold = 1.0f;        // fatigue plus circadian fraction
inline constexpr float kSleepWakeCircadian = 0.0f;            // circadian fraction
inline constexpr float kPaceStressThreshold = 0.7f;           // stress fraction
inline constexpr float kEscapeHourStride = 1.0f;              // hours per escape check
inline constexpr float kLooseRoamRadiusM = 24.0f;             // metres
inline constexpr float kDestinationClearanceM = 0.1f;         // metres around destinations
inline constexpr float kOpenWaterDepthM = 0.15f;              // metres at drinking shore
inline constexpr float kShelterTemperatureReliefC = 5.0f;     // degrees outside comfort range
inline constexpr float kWelfarePoorHabitatStress = 0.6f;      // stress target multiplier
inline constexpr float kCircadianMidday = 12.0f;              // game hour
inline constexpr float kCircadianDayHours = 24.0f;            // game hours per day
inline constexpr float kCircadianBiasMidpoint = 0.1f;         // between active and sleeping bias
inline constexpr float kCircadianBiasAmplitude = 0.4f;        // bias distance from midpoint
inline constexpr float kRoutineDecisionTicks = 1.0f;          // ticks between immediate decisions
inline constexpr float kHeavyRainThreshold = 0.65f;           // precipitation fraction
inline constexpr float kRainShelterThreshold = 0.45f;         // precipitation fraction
inline constexpr float kNightBeginHour = 20.0f;               // game hour
inline constexpr float kNightEndHour = 6.0f;                  // game hour
inline constexpr float kSleepActivitySeconds = 60.0f;         // motion seconds per sleep bout
inline constexpr float kRoutineRecheckSeconds = 30.0f;        // motion seconds per game-hour check
inline constexpr float kHealthWeatherPerDay = 0.2f;           // health units per exposed game day
inline constexpr float kAnimalClearanceM = 0.05f;             // metres between animal body circles
/// The share of its step an animal still makes when companions leave it no way round.
inline constexpr float kSqueezePastFactor = 0.35f;
/// Ticks of walking after which an animal gives its errand up (three minutes of motion).
inline constexpr std::int32_t kWalkGiveUpTicks = 3600;
/// A slide that leaves less than this fraction of the step is a head-on meeting.
inline constexpr float kHeadOnFraction = 0.3f;
/// Squared metres by which a step may near an obstacle inside its clearance (rounding).
inline constexpr float kIntrusionToleranceM2 = 1.0e-5f;
/// An animal that came no nearer to its waypoint than this, metres, for kStallTicks is stalled.
inline constexpr float kStallProgressM = 0.02f;
/// Three quarters of a second at the simulation's 20 Hz: try the next sidestep promptly.
inline constexpr std::int32_t kStallTicks = 15;
/// Food and water are essential errands: retain the proven one-second route before a sidestep.
inline constexpr std::int32_t kNeedStallTicks = 20;
/// Stalled this near to the waypoint (plus the body radius), metres, counts as arrived.
inline constexpr float kStallArriveM = 1.0f;
/// How far outside a blocking prop's clearance a waypoint planned onto it is moved, metres.
inline constexpr float kWaypointRimM = 0.15f;
/// The sidestep out of a pocket: tries, first angle off the goal and its growth (radians), reach (metres).
inline constexpr std::uint32_t kDetourAttempts = 6;
inline constexpr float kDetourAngle = 1.2f;
inline constexpr float kDetourAngleStep = 0.35f;
inline constexpr float kDetourReachM = 3.0f;
/// Sidesteps an errand may take before the animal gives it up.
inline constexpr std::uint32_t kDetourLimit = 5;
/// The share of the body radius (half its length) that is half its width.
inline constexpr float kBodyWidthFraction = 0.6f;
/// Metres between two animals that keep each other company.
inline constexpr float kSocialGapM = 0.3f;
/// Metres from a shelter's door at which an animal goes in.
inline constexpr float kShelterDoorM = 1.9f;
/// Precipitation and degrees outside the comfort range below which a sheltering animal comes out.
inline constexpr float kRainLeaveThreshold = 0.25f;
inline constexpr float kShelterLeaveTemperatureC = 2.0f;
inline constexpr float kWanderWeight = 3.0f;                     // routine choice weight
inline constexpr float kIdleWeight = 1.0f;                       // routine choice weight
inline constexpr float kRestBaseWeight = 1.0f;                   // routine choice weight
inline constexpr float kRestFatigueWeight = 1.0f;                // added rest weight per fatigue fraction
inline constexpr float kForageWeight = 1.5f;                     // routine choice weight
inline constexpr float kPlayWeight = 1.2f;                       // routine choice weight
inline constexpr float kSocializeWeight = 1.0f;                  // routine choice weight
inline constexpr float kVocalizeWeight = 0.7f;                   // routine choice weight
inline constexpr float kVocalizePeakMultiplier = 2.0f;           // added weight at dawn and dusk
inline constexpr float kVocalizeHungerMultiplier = 1.5f;         // added weight at full hunger need
inline constexpr float kDawnStartHour = 5.0f;                    // game hour
inline constexpr float kDawnEndHour = 7.0f;                      // game hour
inline constexpr float kDuskStartHour = 17.0f;                   // game hour
inline constexpr float kDuskEndHour = 19.0f;                     // game hour
inline constexpr float kPlayNeedThreshold = 0.4f;                // boredom fraction
inline constexpr float kVocalizeDurationSeconds = 3.0f;          // motion seconds
inline constexpr float kIdleMinimumSeconds = 3.0f;               // motion seconds
inline constexpr float kIdleMaximumSeconds = 10.0f;              // motion seconds
inline constexpr float kTileCenterFenceClearanceM = 1.0f;        // metres from tile centre to tile edge
inline constexpr float kDestinationProbeM = 3.0f;                // metres queried for blockers
inline constexpr float kPredatorMassRatio = 1.5f;                // prey-to-predator adult mass ratio
inline constexpr float kTargetChangeToleranceSquared = 0.0001f;  // squared metres
inline constexpr float kMealMinimumSeconds = 10.0f;              // motion seconds per meal
inline constexpr float kMealMaximumSeconds = 25.0f;              // motion seconds per meal
inline constexpr float kDrinkMinimumSeconds = 5.0f;              // motion seconds per drink
inline constexpr float kDrinkMaximumSeconds = 10.0f;             // motion seconds per drink
inline constexpr float kPaceDecisionChance = 0.5f;               // chance per decision
inline constexpr float kPaceMinimumSeconds = 30.0f;              // motion seconds per pacing bout
inline constexpr float kPaceMaximumSeconds = 90.0f;              // motion seconds per pacing bout
inline constexpr int kPacingSides = 4;                           // enclosure boundary sides
inline constexpr int kDestinationAttempts = 8;                   // candidate positions per decision
inline constexpr int kTreeCoolingSamples = 8;                    // candidate offsets around a tree
inline constexpr float kObstacleSearchRadiusM = 2.0f;            // extra prop query radius in metres
inline constexpr float kMotherFollowRadiusM = 4.0f;              // juvenile wander radius from its mother
inline constexpr float kSocialWanderRadiusM = 8.0f;              // group attraction radius
inline constexpr float kSocialWanderMinimumPreference = 0.35f;   // distant group preference multiplier
inline constexpr float kPaceOffsetMinimumM = 6.0f;               // metres between pacing endpoints
inline constexpr float kPaceOffsetMaximumM = 10.0f;              // metres between pacing endpoints
inline constexpr float kWanderMinimumSeconds = 6.0f;             // motion seconds
inline constexpr float kWanderMaximumSeconds = 20.0f;            // motion seconds
inline constexpr float kRestMinimumSeconds = 20.0f;              // motion seconds
inline constexpr float kRestMaximumSeconds = 90.0f;              // motion seconds
inline constexpr float kForageMinimumSeconds = 6.0f;             // motion seconds
inline constexpr float kForageMaximumSeconds = 20.0f;            // motion seconds
inline constexpr float kPlayMinimumSeconds = 8.0f;               // motion seconds
inline constexpr float kPlayMaximumSeconds = 20.0f;              // motion seconds
inline constexpr float kSocializeMinimumSeconds = 6.0f;          // motion seconds
inline constexpr float kSocializeMaximumSeconds = 15.0f;         // motion seconds
inline constexpr float kMovementStartDistanceSquaredM2 = 0.01f;  // squared metres
inline constexpr float kDefaultPropRadiusM = 0.5f;               // metres when catalog data is invalid
inline constexpr float kMinimumSleepHours = 1.0f;                // game hours per day
inline constexpr float kEnrichmentAreaScaleM2 = 500.0f;          // enclosure area for enrichment scaling
inline constexpr float kVegetationCrowdRelief = 0.5f;            // crowd stress reduction at full cover
inline constexpr float kHealthyNeedThreshold = 0.7f;             // need fraction for health recovery
/// A species above this shyness leaves room for staff inside its habitat (the same threshold
/// used when crowds bias routine places away from the viewing front).
inline constexpr float kKeeperShynessThreshold = 0.3f;
inline constexpr float kKeeperFlightBaseM = 0.7f;      // personal space beyond the body
inline constexpr float kKeeperFlightShynessM = 2.0f;  // extra metres at shyness 1
inline constexpr float kKeeperRetreatM = 4.0f;         // length of a clear retreat
inline constexpr std::int32_t kKeeperRetreatSeconds = 6;
inline constexpr float kLifespanVariationBase = 0.85f;           // lifespan multiplier
inline constexpr float kLifespanVariationRange = 0.3f;           // additional lifespan multiplier
inline constexpr float kShelterStressMultiplier = 0.5f;          // stress target while sheltered
inline constexpr float kEnrichmentBoredomRelief = 0.5f;          // boredom rate reduction at full enrichment

inline constexpr float kIllnessBaseChancePerDay = 0.002f;        // chance per game day
inline constexpr float kIllnessPoorWelfareChancePerDay = 0.06f;  // added chance at zero welfare per game day
inline constexpr float kIllnessWelfareLevel = 0.7f;              // welfare from which that chance is nothing
inline constexpr float kIllnessNeedLevel = 0.75f;           // hunger or thirst that counts as going without
inline constexpr float kIllnessTemperatureSpanC = 10.0f;    // degrees out of range for the full chance
inline constexpr float kIllnessDirtyChancePerDay = 0.025f;  // added chance at full dirt per game day
inline constexpr float kIllnessUrgentNeedChancePerDay = 0.035f;  // added chance per urgent need per game day
inline constexpr float kIllnessTemperatureChancePerDay =
    0.02f;  // added chance outside comfort range per game day
inline constexpr float kIllnessOldAgeChancePerDay =
    0.015f;  // added chance past 80 percent lifespan per game day
inline constexpr float kIllnessContagionChancePerDay = 0.02f;  // added chance near same species per game day
inline constexpr float kTreatedRecoveryDays = 2.0f;            // game days
inline constexpr float kUntreatedRecoveryChancePerHour = 0.01f;  // chance per game hour
inline constexpr float kUntreatedRecoveryWelfare = 0.65f;        // minimum welfare fraction
inline constexpr float kBreedingWelfare = 0.6f;                  // minimum welfare fraction
inline constexpr float kBreedingMaximumAgeFraction = 0.75f;      // maximum lifespan fraction
inline constexpr float kConceptionChancePerDay = 0.16f;          // chance per game day
inline constexpr float kStillbirthBaseChance = 0.02f;            // chance per litter
inline constexpr float kStillbirthPoorWelfareChance = 0.5f;      // added chance at zero welfare per litter
inline constexpr float kFightChancePerDay = 0.3f;                // chance per game day at aggression 1
inline constexpr float kFightHealthDamage = 0.12f;               // health fraction before mass scaling
inline constexpr float kFightStress = 0.25f;                     // stress fraction added per fight
inline constexpr float kPredatorHungerThreshold = 0.5f;          // hunger fraction to hunt
inline constexpr float kPredatorAttackDistanceM = 20.0f;         // metres
inline constexpr float kPredatorAttackChancePerHour = 0.25f;     // chance per hour at threshold hunger
inline constexpr float kPredatorHungerDrop = 0.65f;              // hunger fraction restored on kill
inline constexpr float kPredatorFailedWound = 0.08f;             // prey health fraction lost
inline constexpr float kPregnancyFoodNeedMultiplier = 1.5f;      // maternal hunger rate while pregnant
inline constexpr float kLifeChecksPerGameDay = 24.0f;            // hourly life checks per game day
inline constexpr float kUntreatedRecoveryMinimumDays =
    3.0f;  // untreated illness duration before recovery rolls
inline constexpr float kFightExcessMultiplierPerAnimal = 1.0f;  // chance multiplier per animal over the limit
inline constexpr float kPredatorBaseSuccessChance = 0.9f;       // base chance that an attack succeeds
inline constexpr float kPredatorMassSuccessPenalty = 0.25f;     // success chance per prey/predator mass ratio
inline constexpr float kPredatorHealthSuccessBonus = 0.3f;      // success chance at zero prey health
inline constexpr float kPredatorJuvenileSuccessBonus = 0.25f;   // extra success chance against juveniles

}  // namespace zoo::tuning
