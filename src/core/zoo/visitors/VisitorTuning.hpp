// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>

#include "zoo/sim/Calendar.hpp"

/// The numbers of the visitor simulation in one place. Durations are in motion
/// seconds (bodies move in them; a game hour is 30 of them), slow processes in
/// game hours. docs/VISITORS.md explains what each is for.
namespace zoo::visitor_tuning {

// ---- the day ---------------------------------------------------------------------------
inline constexpr float kOpeningHour = 9.0f;
inline constexpr float kLastAdmissionHour = 16.5f;
inline constexpr float kClosingHour = 19.0f;
/// How long a group stays at most, game hours: this much plus up to kVisitSpreadHours.
inline constexpr float kVisitHours = 6.0f;
inline constexpr float kVisitSpreadHours = 2.0f;

// ---- arrivals --------------------------------------------------------------------------
/// People per game hour at the peak of a fine day for a zoo with everything to
/// see, a fair price and a perfect rating.
inline constexpr float kBaseArrivalsPerHour = 110.0f;
/// The attraction of a zoo with nothing to see: a few come anyway and say so.
inline constexpr float kMinAttraction = 0.004f;
/// Attraction = 1 - exp(-score / this); score sums appeal / 100 over the species on show.
inline constexpr float kAttractionScale = 4.5f;
inline constexpr float kFairPriceBase = 4.0f;    ///< dollars, for attraction 0
inline constexpr float kFairPriceRange = 46.0f;  ///< dollars more for attraction 1
/// Arrivals fall by exp(-this * (price / fair - 1)) above the fair price.
inline constexpr float kPriceSensitivity = 2.2f;
/// A free zoo draws this much more than one at the fair price.
inline constexpr float kCheapBonus = 0.35f;
/// The rating counts as the mean of this many departed visitors.
inline constexpr float kRatingMemory = 300.0f;

// ---- bodies ----------------------------------------------------------------------------
inline constexpr float kWalkSpeed = 1.3f;         ///< adults: this plus up to kWalkSpeedSpread
inline constexpr float kWalkSpeedSpread = 0.25f;  ///<
inline constexpr float kChildSpeed = 1.15f;
inline constexpr float kFleeFactor = 2.3f;
inline constexpr float kTurnRadiansPerSecond = 5.0f;
/// How far to the side of a route's waypoints a visitor keeps (its own lane), metres.
inline constexpr float kLaneOffset = 0.5f;

// ---- needs, per game hour --------------------------------------------------------------
inline constexpr float kHungerPerHour = 0.07f;
inline constexpr float kThirstPerHour = 0.09f;
inline constexpr float kBladderPerHour = 0.075f;
inline constexpr float kTirednessPerHour = 0.06f;
/// A need this strong is seen to before anything else...
inline constexpr float kUrgent = 0.62f;
/// ...and one this strong when what satisfies it is within kHandyM.
inline constexpr float kNotable = 0.4f;
inline constexpr float kHandyM = 36.0f;
/// Happiness lost per motion second for each need above kSuffering.
inline constexpr float kSuffering = 0.82f;
inline constexpr float kSufferingPerSecond = 0.003f;

// ---- what they do ----------------------------------------------------------------------
inline constexpr float kWatchSeconds = 8.0f;  ///< plus up to kWatchSpreadSeconds
inline constexpr float kWatchSpreadSeconds = 6.0f;
/// A group looks at an enclosure this many times, each time from a place at least
/// kSecondLookM from where it stands; later looks draw it kSecondLookWeight as much.
inline constexpr int kLooksPerEnclosure = 2;
inline constexpr float kSecondLookM = 10.0f;
inline constexpr float kSecondLookWeight = 0.6f;
inline constexpr float kViewRangeM = 55.0f;  ///< animals farther than this are not seen
inline constexpr int kSlotsPerViewingTile = 12;
inline constexpr float kEatSeconds = 5.0f;
inline constexpr float kRestSeconds = 8.0f;
inline constexpr float kReadSeconds = 4.0f;
/// A visit to a restroom, the shop or the education centre takes this share of
/// BuildingDef::serviceSeconds (give or take a fifth).
inline constexpr float kInsideShare = 0.6f;
/// A group waits this long for room in a building before it gives up.
inline constexpr float kDoorPatienceSeconds = 12.0f;
/// A stall hands a group its order after this share of BuildingDef::serviceSeconds...
inline constexpr float kServiceShare = 0.3f;
/// ...and serves capacity / this many groups at a time (at least one).
inline constexpr int kCapacityPerCounter = 6;
/// A group joins no queue longer than this many people and waits no longer than that.
inline constexpr int kQueueLimit = 16;
inline constexpr float kQueuePatienceSeconds = 24.0f;
inline constexpr float kQueueSpacingM = 0.7f;
inline constexpr int kQueuePlaces = 18;
/// Seats and bins farther than this from where a group stands are not looked for.
inline constexpr float kSeatSearchM = 30.0f;
inline constexpr float kBinSearchM = 24.0f;
inline constexpr float kBoardSearchM = 8.0f;
inline constexpr float kThreatM = 28.0f;  ///< a dangerous animal loose this near: run

// ---- performance -----------------------------------------------------------------------
/// Route searches per tick, shared by all groups; a group that finds none left waits a tick.
inline constexpr int kPathSearchesPerTick = 12;
inline constexpr int kMaxPathExpansions = 20000;

// ---- bits of Visitor::unmet and Visitor::done ------------------------------------------
inline constexpr std::uint8_t kUnmetFood = 1;
inline constexpr std::uint8_t kUnmetDrink = 2;
inline constexpr std::uint8_t kUnmetRestroom = 4;
inline constexpr std::uint8_t kUnmetRest = 8;
inline constexpr std::uint8_t kDoneShop = 1;
inline constexpr std::uint8_t kDoneLearn = 2;
inline constexpr std::uint8_t kDoneNothingSeen = 4;

constexpr std::int64_t Ticks(float seconds) {
    return static_cast<std::int64_t>(seconds * static_cast<float>(kTicksPerSecond) + 0.5f);
}
/// Motion seconds in a game hour.
inline constexpr float kSecondsPerGameHour =
    static_cast<float>(kTicksPerGameHour) / static_cast<float>(kTicksPerSecond);

}  // namespace zoo::visitor_tuning
