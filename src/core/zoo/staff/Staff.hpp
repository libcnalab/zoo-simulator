// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "zoo/animals/Animals.hpp"
#include "zoo/base/Ids.hpp"
#include "zoo/base/Math.hpp"
#include "zoo/base/Money.hpp"
#include "zoo/base/Rng.hpp"
#include "zoo/sim/Env.hpp"
#include "zoo/visitors/Visitors.hpp"
#include "zoo/world/World.hpp"

namespace zoo {

namespace staff_detail {
struct Scratch;
struct Stepper;
}  // namespace staff_detail

/// What a member of staff is employed to do. Names are part of the save format;
/// append only.
enum class StaffRole : std::uint8_t {
    Keeper,    ///< fills feeders and troughs, mucks out enclosures, catches harmless runaways
    Vet,       ///< treats sick animals, catches any animal on the loose
    Cleaner,   ///< sweeps litter off the paths, empties the bins
    Mechanic,  ///< mends worn fences
    Count
};
std::string_view ToString(StaffRole v);     // "keeper", "vet", "cleaner", "mechanic"
std::string_view DisplayName(StaffRole v);  // "Keeper", "Vet", "Cleaner", "Mechanic"

/// What a member of staff is doing right now (what the renderer animates).
enum class StaffActivity : std::uint8_t {
    Idle,     ///< standing: nothing to do, or deciding
    Walking,  ///< on the way to a job, to a place to wait, or to the staff room
    Working,  ///< at the job: `workLeft` seconds of it remain
    Resting,  ///< off shift: in the staff room (`inside`) or by the gate
    Count
};
std::string_view ToString(StaffActivity v);  // "idle", "walking", "working", "resting"

/// The job a member of staff has taken on. Names are part of the save format.
enum class StaffJob : std::uint8_t {
    None,
    RefillFeeder,    ///< `prop`: a feeder below its refill level, for which food exists
    RefillTrough,    ///< `prop`: a water trough below its refill level
    CleanEnclosure,  ///< `enclosure`: dirt above the level worth a visit
    TreatAnimal,     ///< `animal`: sick and not yet treated
    CatchAnimal,     ///< `animal`: on the loose
    SweepLitter,     ///< `spot`: litter on the ground there
    EmptyBin,        ///< `prop`: a bin that is nearly full
    MendFence,       ///< `edge`: a fence whose condition is below the level worth a visit
    Count
};
std::string_view ToString(StaffJob v);

/// One employee. Everything here is saved.
struct StaffMember {
    StaffId id;
    StaffRole role = StaffRole::Keeper;
    std::string name;        ///< "Anna Kovar"; chosen from the staff's own generator
    std::uint32_t look = 0;  ///< seed of the body, face and hair the renderer gives it
    Vec2 position;
    float heading = 0.0f;  ///< radians, 0 looks along +z (as everywhere)
    float speed = 0.0f;    ///< m/s right now (0 when standing)
    StaffActivity activity = StaffActivity::Idle;
    StaffJob job = StaffJob::None;
    // The target of the job (only the one the job names is set).
    PropId prop;
    EnclosureId enclosure;
    AnimalId animal;
    EdgeRef edge{};
    Vec2 spot;
    float workLeft = 0.0f;       ///< seconds of work left at the job
    BuildingId inside;           ///< the staff room it rests in (then it is not drawn)
    std::int64_t hiredTick = 0;  ///< when it was taken on
    int jobsDone = 0;            ///< jobs finished since then
    /// The route being walked (world positions, the next one first) and where it leads.
    std::vector<Vec2> route;
    Vec2 routeGoal;
    std::uint32_t routeRevision = 0;  ///< World::NavRevision() + FenceRevision() it was planned for
    float stuckSeconds = 0.0f;        ///< how long it has made no headway while walking
    std::int64_t idleUntil = 0;       ///< tick before which it does not look for work again

    bool operator==(const StaffMember&) const = default;
};

/// The people who work in the zoo.
///
/// They replace the instant, invisible care of ZooConfig::autoCare: every job is
/// found, walked to (NavProfile::Staff(): anywhere dry, through gates, preferring
/// paths), worked at for a while and only then takes effect through the same
/// calls autoCare uses (Animals::RefillFeeder, RefillTrough, Clean, Treat,
/// Recapture; Visitors::CleanLitter; a bin's Prop::amount; World::SetFence).
///
/// Deterministic: the state is the members, a random stream and a few counters;
/// everything else is derived from the Env each tick. A job belongs to at most
/// one member at a time. docs/STAFF.md describes the behaviour and its numbers.
class Staff {
public:
    static constexpr int kMaxStaff = 80;
    /// How many people the zoo can employ without a staff room.
    static constexpr int kBaseCapacity = 8;

    explicit Staff(std::uint64_t seed);
    ~Staff();
    Staff(Staff&&) noexcept;
    Staff& operator=(Staff&&) noexcept;

    /// A day's pay of one member, cents.
    static Money WagePerDay(StaffRole role);
    /// What taking somebody on costs once (advertising, clothing), cents.
    static Money HiringCost(StaffRole role);

    /// How many people the zoo can employ: kBaseCapacity plus the capacity of
    /// every staff room (BuildingRole::StaffRoom), at most kMaxStaff.
    int Capacity(const Env& env) const;
    /// Takes somebody on. They appear at the spawn point outside the gate
    /// (World::SpawnPoint) and walk in. Books HiringCost to Account::Wages.
    /// Null when the zoo is at capacity or the money is not there.
    StaffId Hire(Env& env, StaffRole role);
    /// Lets somebody go at once: their job is free for others. False for an unknown id.
    bool Fire(StaffId id);

    /// Advances everybody by one tick. `world` is the same map as env.world; it
    /// is written to only by a mechanic finishing a fence (World::SetFence).
    /// True when a fence was mended this tick: the caller then rebuilds the
    /// enclosures (what they hold in depends on the fences' condition).
    bool Step(Env& env, World& world, Animals& animals, Visitors& visitors);
    /// Books the day's wages (Account::Wages; paid even when the balance goes negative).
    void CloseDay(Env& env);

    /// Ascending id.
    const std::vector<StaffMember>& All() const { return members_; }
    const StaffMember* Find(StaffId id) const;
    int Count() const { return static_cast<int>(members_.size()); }
    int Count(StaffRole role) const;
    /// The wages of everybody for one day, cents.
    Money DailyWages() const;
    /// Jobs of a role that are waiting for somebody right now (nobody has taken
    /// them on): what the player reads to see whether to hire. Derived, O(world).
    int OpenJobs(const Env& env, const Animals& animals, const Visitors& visitors, StaffRole role) const;

    /// Removes everybody.
    void Clear();

    /// Hash of the saved state.
    std::uint64_t StateHash() const;
    /// Compares the saved state.
    bool operator==(const Staff& other) const;

private:
    friend struct StaffJsonAccess;
    friend struct staff_detail::Stepper;

    std::vector<StaffMember> members_;
    std::uint32_t nextId_ = 1;
    Rng rng_;
    /// Derived each tick; never saved.
    std::unique_ptr<staff_detail::Scratch> scratch_;
};

}  // namespace zoo
