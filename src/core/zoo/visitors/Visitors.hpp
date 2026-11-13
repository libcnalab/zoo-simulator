// SPDX-License-Identifier: MIT
#pragma once

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

#include "zoo/animals/Animals.hpp"
#include "zoo/base/Ids.hpp"
#include "zoo/base/Math.hpp"
#include "zoo/base/Money.hpp"
#include "zoo/base/Rng.hpp"
#include "zoo/sim/Env.hpp"

namespace zoo {

namespace visitors_detail {
struct Scratch;
struct Stepper;
}  // namespace visitors_detail

/// What a visitor is doing right now (what the renderer animates). Names are part
/// of the save format; append only.
enum class VisitorActivity : std::uint8_t {
    Idle,      ///< standing: making up its mind, or waiting for the rest of its group
    Arriving,  ///< walking up the approach road to the gate
    Walking,   ///< on the way to what the group decided on
    Watching,  ///< at a fence, looking at the animals
    Queueing,  ///< in the line of a stall, or waiting for room in a building
    Eating,    ///< having what it bought, standing or seated (Visitor::seat)
    Sitting,   ///< resting on a bench
    Reading,   ///< reading an information board
    Inside,    ///< in a building (Visitor::inside): not drawn
    Leaving,   ///< walking out of the zoo
    Fleeing,   ///< running for the gate from a dangerous animal on the loose
    Count
};
std::string_view ToString(VisitorActivity v);  // "idle", "arriving", ...

/// What a group is on its way to do, or doing. Names are part of the save format.
enum class VisitorGoal : std::uint8_t {
    None,      ///< nothing yet: the leader decides
    Enter,     ///< to the gate, to buy tickets
    Watch,     ///< an enclosure (Visitor::targetEnclosure) from a viewing place
    Food,      ///< a food stall
    Drink,     ///< a drinks stall
    Restroom,  ///<
    Rest,      ///< a bench
    Eat,       ///< somewhere to have what was just bought
    Bin,       ///< a litter bin, to throw the wrappers away
    Shop,      ///< the gift shop
    Learn,     ///< the education centre
    Read,      ///< an information board
    Leave,     ///< out of the zoo
    Flee,      ///< out of the zoo, running
    Count
};
std::string_view ToString(VisitorGoal v);

/// What is on a visitor's mind: the bubble over its head, and the player's best
/// clue to what the zoo lacks. Names are part of the save format.
enum class VisitorThought : std::uint8_t {
    None,
    Hungry,              ///< wants to eat and found no food stall
    Thirsty,             ///< wants a drink and found no drinks stall
    NeedsRestroom,       ///< found no restroom
    Tired,               ///< found no free bench
    LovedAnimal,         ///< saw an animal well
    CouldNotSeeAnimals,  ///< stood at an enclosure and saw nothing
    AnimalsLookUnhappy,  ///< saw animals in poor welfare
    TooCrowded,          ///< a crowd at the fence, or a queue too long to bear
    Litter,              ///< litter on the ground
    NothingToSee,        ///< leaves without having seen an animal
    TooExpensive,        ///< did not buy at that price
    GreatZoo,            ///< leaves delighted
    Scared,              ///< an animal is loose
    LostWay,             ///< cannot get where it wants to go (a path was removed)
    Count
};
std::string_view ToString(VisitorThought v);

/// How fast a visit goes. The management game runs a day in twelve minutes, so its
/// people look at an enclosure for ten seconds and are gone after four minutes; in a
/// zoo one walks through oneself they stand at a fence for a minute and stay long
/// enough to get round. All three are factors on the tuning (VisitorTuning.hpp).
struct VisitorPace {
    /// On the time at a fence, on a bench, over a meal and at a board.
    float linger = 1.0f;
    /// On how long a group stays in the zoo before it goes home; hunger, thirst and
    /// weariness come on that much more slowly, so a long visit has no more meals.
    float stay = 1.0f;
    /// On how many people arrive (with a longer stay, fewer arrivals keep the same crowd).
    float arrivals = 1.0f;
};

/// One person visiting the zoo.
///
/// People come in groups of one to five (families, couples, friends) with
/// consecutive ids. The first of a group leads: the leader decides where the
/// group goes and holds the group's purse; the plan is copied to every member,
/// each of whom walks its own route to its own place at the destination.
struct Visitor {
    VisitorId id;
    std::uint32_t look = 0;   ///< seeds body, clothes and hair in the renderer
    bool child = false;       ///< half price at the gate, shorter, slower
    float heightM = 1.7f;     ///< body height
    std::uint32_t group = 0;  ///< shared by the members of a group; never 0
    bool leader = false;

    Vec2 position;
    float heading = 0.0f;  ///< see HeadingVector()
    float speed = 0.0f;    ///< ground speed now, motion m/s (0: standing)
    VisitorActivity activity = VisitorActivity::Idle;
    BuildingId inside;  ///< the building it is in while Inside
    PropId seat;        ///< the bench it sits on (Sitting, or Eating at a table)

    // -- needs, 0 satisfied .. 1 urgent --
    float hunger = 0.0f;
    float thirst = 0.0f;
    float bladder = 0.0f;
    float tiredness = 0.0f;
    float happiness = 0.6f;  ///< 0..1; what the zoo's rating is made of when it leaves
    float education = 0.0f;  ///< what it learned on this visit, 0..1

    VisitorThought thought = VisitorThought::None;
    std::int64_t thoughtTick = 0;  ///< when it had that thought (bubbles fade)
    std::int64_t arrivedTick = 0;  ///< when the group appeared on the approach road

    // -- the plan (the same for all members of a group, set by the leader) --
    VisitorGoal goal = VisitorGoal::None;
    EnclosureId targetEnclosure;
    BuildingId targetBuilding;
    PropId targetProp;
    Vec2 target;                   ///< where this member will stand or sit
    float face = 0.0f;             ///< the heading it takes up there
    std::uint8_t slot = 0;         ///< its place at a fence or on a bench
    std::vector<Vec2> route;       ///< waypoints still to walk, ending at `target`
    std::uint32_t routeIndex = 0;  ///< next waypoint; == route.size(): it has arrived

    // -- kept by the leader for the group --
    Money cash = 0;                  ///< the group's purse, cents
    Money spent = 0;                 ///< tickets, purchases and donations so far
    bool reroute = false;            ///< the map changed under its routes: find them again
    std::int64_t activityUntil = 0;  ///< tick at which what the group is doing ends
    std::int64_t queuedTick = 0;     ///< when it joined the queue it stands in
    VisitorGoal previousGoal = VisitorGoal::None;
    std::uint64_t seenSpecies = 0;              ///< bit per species index watched (novelty)
    std::vector<std::uint32_t> seenEnclosures;  ///< enclosure ids it has been to, in order
    std::uint8_t unmet = 0;                     ///< bit per need it found nothing for (see VisitorTuning)
    std::uint8_t done = 0;                      ///< bit per once-a-visit thing done (see VisitorTuning)
    BuildingId skipBuilding;                    ///< the stall whose queue it walked away from
    std::uint8_t litter = 0;                    ///< wrappers and cups in hand
    std::uint8_t lost = 0;                      ///< plans that failed in a row
    bool ticketPaid = false;
    bool served = false;  ///< the stall is preparing its order (activityUntil: when it is ready)

    bool operator==(const Visitor&) const = default;
};

/// A piece of litter on the ground.
struct Litter {
    Vec2 position;
    std::int64_t tick = 0;  ///< when it was dropped
    bool operator==(const Litter&) const = default;
};

/// The visitors of a zoo: who comes, what they do, what they think of it.
///
/// Deterministic: the state is the visitors, the litter, a random stream and a
/// few counters; everything else is derived from the Env each tick. The cost of
/// a step grows linearly with the number of visitors (route searches are rationed
/// per tick), see docs/VISITORS.md.
class Visitors {
public:
    static constexpr int kMaxVisitors = 800;
    static constexpr int kMaxGroupSize = 5;
    static constexpr std::size_t kMaxLitter = 1500;

    explicit Visitors(std::uint64_t seed);
    ~Visitors();
    Visitors(Visitors&&) noexcept;
    Visitors& operator=(Visitors&&) noexcept;

    /// The price of an adult's ticket (children pay half), cents.
    Money TicketPrice() const { return ticketPrice_; }
    /// Clamped to $0..$200.
    void SetTicketPrice(Money price);
    /// A closed zoo admits nobody; visitors already inside finish their visit.
    bool Open() const { return open_; }
    void SetOpen(bool open) { open_ = open; }

    /// Advances every visitor by one tick.
    void Step(Env& env, const Animals& animals);

    /// Ascending id; the members of a group are consecutive, the leader first.
    const std::vector<Visitor>& All() const { return visitors_; }
    const Visitor* Find(VisitorId id) const;
    int Count() const { return static_cast<int>(visitors_.size()); }
    /// The leader of a visitor's group (itself when it leads); nullptr for an unknown id.
    const Visitor* LeaderOf(VisitorId id) const;
    /// Visitors standing at the fence of an enclosure right now (what its animals feel).
    int Watching(EnclosureId enclosure) const;

    const std::vector<Litter>& LitterOnGround() const { return litter_; }
    /// Removes the litter within `radius` of `position`; returns how many pieces.
    int CleanLitter(Vec2 position, float radius);
    /// Removes all of it (a night's cleaning).
    int CleanAllLitter();

    /// What visitors think of the zoo: the smoothed happiness of those who left, 0..1.
    float Rating() const { return rating_; }
    /// The smoothed education of those who left, 0..1.
    float MeanEducation() const { return educationRating_; }
    /// Mean happiness of the visitors in the zoo now (the rating when there are none).
    float MeanHappiness() const;
    /// How much there is to see, 0..1: the species that can be watched from a path
    /// reachable from the gate, weighted by their appeal, how well they can be seen
    /// and how they fare.
    float Attraction(const Env& env, const Animals& animals) const;
    /// The ticket price visitors find fair for that much to see, cents.
    static Money FairTicketPrice(float attraction);
    /// People arriving per game hour at this moment (0 at night and when closed).
    float ExpectedArrivalsPerHour(const Env& env, const Animals& animals) const;

    int ArrivedToday() const { return arrivedToday_; }
    int LeftToday() const { return leftToday_; }
    std::int64_t ArrivedEver() const { return arrivedEver_; }
    /// The pace of a visit (see VisitorPace). A setting, not state: it is not saved
    /// and takes no part in StateHash().
    const VisitorPace& Pace() const { return pace_; }
    void SetPace(const VisitorPace& pace);
    /// As if the zoo had been open for hours: every group that is on its feet inside
    /// (walking, standing or at a fence; not seated, served, queueing or in a house)
    /// is set down somewhere on the public paths the gate leads to, the farther in the
    /// more it has seen, the longer it has been here and the hungrier and wearier it
    /// is. First assign one eligible group per reachable habitat to viewing slots at
    /// its front (as many as the available groups allow); scatter the others over
    /// paths to make up their minds there. For a zoo one walks into in mid-morning;
    /// Step() never does this.
    /// Returns the number of people moved.
    int Spread(const Env& env);
    /// Starts the day's counters afresh.
    void CloseDay();
    /// Removes everybody and all litter (the ticket price and the rating stay).
    void Clear();

    /// Hash of the saved state.
    std::uint64_t StateHash() const;
    /// Compares the saved state.
    bool operator==(const Visitors& other) const;

private:
    friend struct VisitorsJsonAccess;
    friend struct visitors_detail::Stepper;

    std::vector<Visitor> visitors_;
    std::vector<Litter> litter_;
    std::uint32_t nextId_ = 1;
    std::uint32_t nextGroup_ = 1;
    Rng rng_;
    Money ticketPrice_ = Dollars(15);
    bool open_ = true;
    VisitorPace pace_;
    float arrivalRemainder_ = 0.0f;
    float rating_ = 0.6f;
    float educationRating_ = 0.0f;
    float leftWeight_ = 8.0f;
    int arrivedToday_ = 0;
    int leftToday_ = 0;
    std::int64_t arrivedEver_ = 0;
    /// Derived each tick; never saved.
    std::unique_ptr<visitors_detail::Scratch> scratch_;
};

}  // namespace zoo
