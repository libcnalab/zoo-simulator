// SPDX-License-Identifier: MIT
#pragma once

#include <cstddef>
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
// What a walker does at full stride. Walking was a constant speed along a line once; since it
// speeds up, brakes, turns and gives way (some 6 % of the way's time) the paces are that much
// higher, so that a visit takes as long as it did.
inline constexpr float kWalkSpeed = 1.37f;        ///< adults: this plus up to kWalkSpeedSpread
inline constexpr float kWalkSpeedSpread = 0.25f;  ///<
inline constexpr float kChildSpeed = 1.22f;
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

// ---- how they walk ---------------------------------------------------------------------
/// This share of the adults is old: walks at kElderlyPace of an adult's pace.
inline constexpr float kElderlyShare = 0.12f;
inline constexpr float kElderlyPace = 0.8f;
/// A walker speeds up and slows down this fast, m/s2, and brakes for its place with kBrake.
inline constexpr float kGainSpeed = 2.4f;
inline constexpr float kDropSpeed = 4.0f;
inline constexpr float kBrake = 1.2f;
/// It turns this fast at full stride and kTurnOnTheSpot faster when it stands, rad/s.
inline constexpr float kTurnWalking = 3.4f;
inline constexpr float kTurnOnTheSpot = 2.6f;
/// It rounds a corner: on to the next waypoint from this far, when the way there is open...
inline constexpr float kCornerM = 0.55f;
/// ...and otherwise from this far; nearer than kDirectM it steps straight at its waypoint.
inline constexpr float kReachM = 0.1f;
inline constexpr float kDirectM = 0.4f;
/// Nearer than this to the place it is bound for it minds nobody (the place is its own).
inline constexpr float kSettleM = 1.0f;
/// The people it walks around: those up to kAheadM in front of it and nearer to its line
/// than kShoulderM (kOwnShoulderM for its own group).
inline constexpr float kAheadM = 1.4f;
inline constexpr float kShoulderM = 0.55f;
inline constexpr float kOwnShoulderM = 0.55f;
/// Somebody going the same way is followed, and walked around only when nearer than this.
inline constexpr float kFollowM = 0.5f;
/// A group walks two abreast, kAbreastM apart, in rows kRowM behind one another.
inline constexpr float kAbreastM = 0.62f;
inline constexpr float kRowM = 0.85f;
/// A child runs this far ahead of its people now and then, and comes back.
inline constexpr float kChildAheadM = 1.9f;
/// Two people who stand (not at a fence, on a seat or in a line) keep this far apart.
inline constexpr float kRoomM = 0.5f;
/// ...and that far from every place which the queue limit can occupy.
inline constexpr std::size_t kLineKeptClear = 16;
/// A watcher looks at an animal up to this far off the straight line across the fence, rad,
/// picks another every kAimSeconds or so and turns to it at kAimEase times what is left of
/// the turn a second, never slower than kAimTurn rad/s.
inline constexpr float kAimSpread = 0.5f;
inline constexpr float kAimSeconds = 2.5f;
inline constexpr float kAimEase = 4.0f;
inline constexpr float kAimTurn = 0.5f;

// ---- what they do ----------------------------------------------------------------------
inline constexpr float kWatchSeconds = 8.0f;  ///< plus up to kWatchSpreadSeconds
inline constexpr float kWatchSpreadSeconds = 6.0f;
/// A group looks at an enclosure this many times, each time from a place at least
/// kSecondLookM from where it stands; later looks draw it kSecondLookWeight as much.
/// Metres added to the way to a habitat when visitors weigh it: the larger, the less the
/// nearest habitat wins over a better one further off.
inline constexpr float kWatchNearM = 60.0f;
/// People at or bound for a viewing tile (2 m of front) at which a habitat is half as inviting.
inline constexpr float kWatchersPerTileAtEase = 1.0f;
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
inline constexpr float kQueueBodyM = 0.3f;  ///< a line waits rather than entering a standing person
inline constexpr int kQueuePlaces = 18;
/// A walking visitor never steps inside the player on foot even when steering has not
/// yet turned far enough to clear them.
inline constexpr float kBystanderBodyM = 0.36f;
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
