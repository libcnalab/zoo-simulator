// SPDX-License-Identifier: MIT
#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

#include "zoo/animals/Animals.hpp"
#include "zoo/base/Ids.hpp"
#include "zoo/base/Math.hpp"
#include "zoo/sim/Env.hpp"
#include "zoo/visitors/Visitors.hpp"

namespace zoo {

/// What a member of staff is there for. Names are part of the save format; append only.
enum class StaffRole : std::uint8_t {
    Keeper,    ///< looks after the habitats of a round: feed, water, mucking out, the fences
    Gardener,  ///< tends the flower beds along the paths
    Cleaner,   ///< empties the bins and sweeps the paths
    Count
};
std::string_view ToString(StaffRole v);     // "keeper", "gardener", "cleaner"
std::string_view DisplayName(StaffRole v);  // "Keeper", "Gardener", "Cleaner"

/// What a member of staff is seen doing (what the renderer poses and which tool it
/// puts in the hands). Names are part of the save format; append only.
enum class StaffActivity : std::uint8_t {
    Walk,        ///< on the way to the next job, hands free or a tool over the shoulder
    Carry,       ///< walking with a full bucket
    Push,        ///< walking behind a wheelbarrow
    FillFeeder,  ///< bent over a feeder, tipping the bucket in
    FillTrough,  ///< filling a water trough
    ThrowFeed,   ///< throwing or scattering feed, across a barrier when the habitat is not entered
    Rake,        ///< raking, mucking out
    Hose,        ///< hosing: a deck, a pool's edge, or a trough across a barrier
    Inspect,     ///< checking a fence: a hand on it, looking along it
    Sweep,       ///< sweeping a path with a broom
    EmptyBin,    ///< bent over a litter bin, taking the sack out
    Tend,        ///< kneeling at a flower bed
    Water,       ///< watering a flower bed from a can
    Rest,        ///< standing, taking a breather
    Count
};
std::string_view ToString(StaffActivity v);  // "walk", "carry", "push", "fill_feeder", ...
/// A few words for a visitor ("fills the feeder"); never empty.
std::string_view Describe(StaffActivity v);

/// The safety action attached to a task at a dangerous habitat. Names are saved; append only.
enum class StaffLockoutAction : std::uint8_t {
    None,
    Secure,   ///< put the whole group in its houses before entering
    Release,  ///< let it out, only after the keeper has returned to public ground
    Count
};
std::string_view ToString(StaffLockoutAction v);  // "none", "secure", "release"

/// One job: a place to stand, a way to face, something to be seen doing for a while.
/// The job's effect on the zoo (a feeder filled, dirt gone) happens half way through.
struct StaffTask {
    StaffActivity work = StaffActivity::Rest;
    /// How the member gets there: Walk, Carry or Push.
    StaffActivity approach = StaffActivity::Walk;
    Vec2 place;
    float facing = 0.0f;
    std::int32_t ticks = 0;
    EnclosureId enclosure;  ///< the habitat it serves; null for work on the paths
    PropId prop;            ///< the feeder, trough, bin or bed; null when there is none
    StaffLockoutAction lockout = StaffLockoutAction::None;

    bool operator==(const StaffTask&) const = default;
};

/// One stop of a round: a habitat (keepers) or a prop on the paths (a bed, a bin).
struct StaffStop {
    EnclosureId enclosure;
    PropId prop;
    /// Calendar hours of the two feedings a visitor can come to watch; negative: none.
    std::array<float, 2> feedHours{-1.0f, -1.0f};
    /// The calendar day on which each was last given; -1: never.
    std::array<std::int64_t, 2> fedDay{-1, -1};

    bool operator==(const StaffStop&) const = default;
};

struct StaffMember {
    StaffId id;
    StaffRole role = StaffRole::Keeper;
    /// Picks the body the renderer gives them (the uniform is the same for all).
    std::uint32_t look = 0;
    /// Gone round in order, again and again; never empty.
    std::vector<StaffStop> round;
    std::uint32_t stop = 0;       ///< the stop being worked at or walked to
    std::uint32_t stopsDone = 0;  ///< how many stops were planned so far (varies the routine work)

    // -- body --
    Vec2 position;
    float heading = 0.0f;  ///< radians; 0 looks along +z (see Math.hpp)
    float speed = 0.0f;    ///< current ground speed in m/s
    StaffActivity activity = StaffActivity::Rest;

    // -- the jobs of the current stop --
    std::vector<StaffTask> tasks;
    std::uint32_t task = 0;  ///< index into `tasks`; == size: the stop is done
    bool working = false;    ///< arrived at the task's place
    /// Of the task's work once `working`; on the way, of the patience the walk still has.
    std::int32_t ticksLeft = 0;
    std::vector<Vec2> route;  ///< remaining waypoints while walking to the task
    std::uint32_t routeIndex = 0;
    /// Dangerous habitat currently secured for this member, until an outside release task.
    EnclosureId lockout;

    const StaffTask* Task() const { return task < tasks.size() ? &tasks[task] : nullptr; }

    bool operator==(const StaffMember&) const = default;
};

/// The staff a visitor sees at work: a fixed crew, each with a round they go about all
/// day. There is no management here: nobody is hired or paid, nobody tires.
///
/// A keeper's round is a few habitats. At each stop the jobs are planned from what the
/// habitat needs (feeders and troughs running low, dirt) and from the hour (mucking out
/// in the morning, hosing and fence checks later, the two feedings a day visitors can
/// come to). Keepers go in through the keeper gate, except where a dangerous species
/// lives: nothing in the animal model shuts those animals in, so there the keeper never
/// enters only after the whole group has been secured in capacity-checked houses. Public
/// feeding stays outside; routine work ends by returning outside before releasing the group.
///
/// Step() is deterministic: equal state and equal inputs give bit-identical results.
/// The class has no random stream; variety comes from the stateless hashes. Members
/// are stepped in ascending id.
class Staff {
public:
    /// Walking speeds in m/s.
    static constexpr float kWalkSpeed = 1.6f;
    static constexpr float kCarrySpeed = 1.45f;
    static constexpr float kPushSpeed = 1.35f;
    /// A feeder below this share of its capacity is filled at the keeper's next stop; a trough likewise.
    static constexpr float kFeederDue = 0.85f;
    static constexpr float kTroughDue = 0.9f;
    /// A feeding visitors can watch begins at a stop planned from this long before its
    /// hour until this long after it (calendar hours).
    static constexpr float kFeedingEarly = 0.15f;
    static constexpr float kFeedingLate = 0.75f;

    /// Engages the crew for the zoo as it stands now (replacing any earlier crew): one
    /// keeper for every two habitats, a gardener when there are flower beds to reach,
    /// a cleaner when there are bins. Everybody starts at the first stop of their round.
    void Engage(const Env& env, const Animals& animals);

    /// Ascending id.
    const std::vector<StaffMember>& All() const { return members_; }
    const StaffMember* Find(StaffId id) const;

    /// Advances everybody by one tick. Fills feeders and troughs, removes dirt, empties
    /// bins and picks up litter as the jobs are done.
    void Step(Env& env, Animals& animals, Visitors& visitors);

    /// The keeper who has this habitat on the round, if any.
    const StaffMember* KeeperOf(EnclosureId enclosure) const;

    /// The calendar hours of the habitat's two public feedings (negative: none).
    std::array<float, 2> FeedingHours(EnclosureId enclosure) const;
    /// The next public feeding of today that has not been given yet, as a calendar hour.
    std::optional<float> NextFeeding(const Env& env, EnclosureId enclosure) const;
    /// Publishes every habitat's next feeding to the animal simulation in one pass over the
    /// keepers. Zoo calls this every tick after clearing the previous transient schedule.
    void PublishNextFeedings(const Env& env, Animals& animals) const;

    /// Hash over the whole saved state (FNV-1a, floats by bit pattern).
    std::uint64_t StateHash() const;
    bool operator==(const Staff& other) const = default;

private:
    friend struct StaffJsonAccess;  // serialisation

    /// Chooses the member's next stop and fills `tasks` with its jobs (at least one).
    void PlanStop(const Env& env, const Animals& animals, const Visitors& visitors,
                  StaffMember& member) const;
    static void Apply(Env& env, Animals& animals, Visitors& visitors, const StaffTask& task);

    std::vector<StaffMember> members_;  // ascending id
    std::uint32_t nextId_ = 1;
};

/// May keepers walk into this habitat: nothing dangerous lives in it, and its inside can
/// be reached on foot through a keeper gate.
bool KeepersMayEnter(const Env& env, const Animals& animals, EnclosureId enclosure);

}  // namespace zoo
