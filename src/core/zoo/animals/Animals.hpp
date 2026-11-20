// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "zoo/animals/Species.hpp"
#include "zoo/base/Ids.hpp"
#include "zoo/base/Math.hpp"
#include "zoo/base/Money.hpp"
#include "zoo/base/Rng.hpp"
#include "zoo/build/Enclosures.hpp"
#include "zoo/sim/Env.hpp"

namespace zoo {

enum class Sex : std::uint8_t { Female, Male };
std::string_view ToString(Sex v);  // "female", "male"

/// What an animal is doing (or, while Animal::moving, is on its way to do).
/// Names are part of the save format; append only.
enum class AnimalActivity : std::uint8_t {
    Idle,        ///< standing, looking around
    Wander,      ///< strolling to another spot of its enclosure
    Forage,      ///< nose down: grazing, sniffing, pecking
    Eat,         ///< at a feeder
    Drink,       ///< at a trough or at the water's edge
    Rest,        ///< lying down awake
    Sleep,       ///< asleep, in the open or in a shelter (Animal::shelter)
    Play,        ///< with an enrichment item, or a burst of running
    Socialize,   ///< seeking the company of a group mate
    Vocalize,    ///< roaring, howling, trumpeting...
    Pace,        ///< stereotypic pacing along the barrier: the visible sign of poor welfare
    Shelter,     ///< hiding in a shelter from weather or crowds, awake
    BreakOut,    ///< heading for the weak spot of the barrier, then over or through it
    Loose,       ///< roaming outside any enclosure
    Crated,      ///< in a transport crate: not in the world at all
    Browse,      ///< feeding with head raised to foliage
    Ruminate,    ///< standing, chewing cud
    Bathe,       ///< swimming or wallowing at a suitable water site
    DustBathe,   ///< rolling or throwing dry substrate
    Preen,       ///< tending feathers
    Sentinel,    ///< scanning from a vantage point while companions feed
    Dig,         ///< excavating and investigating substrate
    FilterFeed,  ///< feeding head down in shallow water
    Patrol,      ///< purposeful boundary travel, distinct from stereotypic pacing
    Huddle,      ///< resting upright in a flock; flamingos use one leg
    Count
};
std::string_view ToString(AnimalActivity v);  // "idle", "wander", ... (save format)

enum class DeathCause : std::uint8_t {
    OldAge,
    Starvation,
    Thirst,
    Illness,
    Cold,
    Heat,
    Killed,
    Injury,
    Count
};
std::string_view ToString(DeathCause v);  // "old_age", "starvation", ...

/// What is wrong in an animal's life. Welfare reports carry these as a bit set.
enum class WelfareIssue : std::uint8_t {
    TooSmall,        ///< the enclosure is too small for the animals in it
    PoorTerrain,     ///< wrong ground cover
    TooLittleWater,  ///< wants more open water
    NoDeepWater,     ///< needs a pool deep enough to swim in
    TooBare,         ///< wants more plants
    TooOvergrown,    ///< wants more open ground
    NoShelter,
    ShelterFull,  ///< shelters hold fewer animals than live here
    Lonely,       ///< fewer of its kind than the species needs
    Overcrowded,  ///< more of its kind than the species tolerates
    TooManyMales,
    PredatorNearby,  ///< shares the enclosure with something that hunts it
    NoFood,          ///< no feeder here holds food it eats
    NoDrink,         ///< no trough with water and no open water
    Hungry,
    Thirsty,
    Bored,     ///< too little enrichment
    Stressed,  ///< crowds, noise, fights
    Dirty,     ///< the enclosure needs cleaning
    Sick,
    TooCold,
    TooHot,
    Escaped,  ///< not in an enclosure at all
    Count
};
/// "This enclosure is too small.", "It is lonely: this species lives in groups." ...
std::string_view Describe(WelfareIssue issue);
constexpr std::uint32_t Bit(WelfareIssue issue) {
    return 1u << static_cast<std::uint32_t>(issue);
}

/// How well an animal lives, each aspect from 0 (terrible) to 1 (ideal).
struct WelfareReport {
    // -- what the enclosure and the group give (EvaluateHabitat) --
    float space = 1.0f;
    float terrain = 1.0f;
    float water = 1.0f;
    float vegetation = 1.0f;
    float shelter = 1.0f;
    float social = 1.0f;
    float enrichment = 1.0f;
    // -- the animal's own state (filled only by Animals::Welfare, 1 otherwise) --
    float nutrition = 1.0f;    ///< hunger and thirst
    float comfort = 1.0f;      ///< temperature
    float cleanliness = 1.0f;  ///< of the enclosure
    float health = 1.0f;
    float calm = 1.0f;  ///< 1 - stress
    /// The verdict: 0.7 * weighted mean of the aspects + 0.3 * the worst aspect.
    float overall = 1.0f;
    std::uint32_t issues = 0;  ///< bits of WelfareIssue

    bool Has(WelfareIssue issue) const { return (issues & Bit(issue)) != 0; }
    bool operator==(const WelfareReport&) const = default;
};

/// Who shares an enclosure, from one species' point of view.
struct GroupInfo {
    int sameSpecies = 1;  ///< animals of the species in the enclosure, the asking one included
    int adultMales = 0;   ///< adult males among them
    int allAnimals = 1;   ///< animals of all species in the enclosure
    /// Area the animals of *other* species in the enclosure need for themselves.
    float otherSpeciesSpaceM2 = 0.0f;
    bool predator = false;  ///< a dangerous carnivore of another species lives here
};

/// Area a group of `count` animals of a species needs.
float SpaceNeededM2(const SpeciesDef& species, int count);

/// How a species would fare in an enclosure: the habitat aspects of the report
/// (space, terrain, water, vegetation, shelter, social, enrichment), `overall`
/// over those aspects alone, and the issues among TooSmall .. PredatorNearby
/// and Bored. A pure function: the construction UI calls it to preview "what
/// would a lion think of this enclosure" before any animal is bought.
WelfareReport EvaluateHabitat(const SpeciesDef& species, const EnclosureStats& stats, const GroupInfo& group);

/// Can this barrier hold the species? Height against what it jumps or climbs,
/// strength against what it pushes over. 1 = completely safe, 0 = walks out.
/// Below 1 the animal will escape sooner or later, the sooner the lower.
float Containment(const SpeciesDef& species, const EnclosureStats& stats);

/// One animal.
///
/// Needs run from 0 (satisfied) to 1 (critical). Slow quantities are kept as
/// ticks (integers) so that years of game time lose no precision.
struct Animal {
    AnimalId id;
    int species = -1;  ///< index into SpeciesCatalog::All()
    std::string name;  ///< "Zuri"; unique among the zoo's living animals of the species when possible
    Sex sex = Sex::Female;
    /// GameTime::tick of its birth; negative for animals older than the game.
    std::int64_t bornTick = 0;
    AnimalId mother;  ///< null for animals bought in
    AnimalId father;

    // -- body --
    Vec2 position;
    float heading = 0.0f;  ///< radians; 0 looks along +z (see Math.hpp)
    /// Current ground speed in m/s (what the renderer scales the gait's playback by).
    float speed = 0.0f;

    // -- needs and condition --
    float hunger = 0.2f;
    float thirst = 0.2f;
    float fatigue = 0.2f;
    float boredom = 0.2f;
    float stress = 0.0f;
    float health = 1.0f;  ///< 1 = fit, 0 = dead
    /// Smoothed WelfareReport::overall, refreshed about once a game hour.
    float welfare = 0.7f;
    std::uint32_t issues = 0;  ///< from that same evaluation
    bool sick = false;
    bool treated = false;            ///< a vet has seen to the illness: it recovers
    std::int64_t sickSinceTick = 0;  ///< Tick when the current illness began; 0 for older saves.
    std::int64_t treatedTick = 0;    ///< Tick when treatment began; 0 until treated.
    /// Alert latches: bit 0 starving, bit 1 thirsty; cleared below 0.6.
    std::uint8_t alerts = 0;

    // -- behaviour --
    AnimalActivity activity = AnimalActivity::Idle;
    /// Still walking to where `activity` takes place.
    bool moving = false;
    std::int32_t activityTicks = 0;  ///< spent in the current activity (walking included)
    std::int32_t activityLeft = 0;   ///< remaining once arrived; the next decision follows at 0
    Vec2 target;                     ///< where the activity takes place
    PropId targetProp;               ///< the feeder, trough or toy it uses
    BuildingId shelter;              ///< the shelter it is inside of (hidden from view), else null
    std::vector<Vec2> route;         ///< remaining waypoints while `moving`
    std::uint32_t routeIndex = 0;
    /// The nearest it came to the waypoint it walks to (negative: not measured yet) and the
    /// ticks since it last came nearer: how a walk that leads nowhere is noticed.
    float stallBest = -1.0f;
    std::int32_t stallTicks = 0;
    /// The waypoint it walks to is a sidestep round an obstacle; how many this errand took.
    bool onDetour = false;
    std::uint32_t detours = 0;

    /// A failed food/water search is retried after a short delay, not every tick.
    std::int64_t nextNeedCheck = 0;
    /// Shared call timestamp last answered, and the tick of this individual's call.
    std::int64_t answeredCallTick = -1;
    std::int64_t lastCallTick = -1;
    /// Interest direction and time until the next glance; motion time, not calendar time.
    float attentionHeading = 0.0f;
    std::int32_t attentionLeft = 0;
    /// Motionless awake ticks, including repeated bouts of the same activity.
    std::int32_t stillTicks = 0;
    /// Mother or juvenile playmate currently followed; null for independent movement.
    AnimalId following;

    // -- life --
    /// The enclosure it belongs to: where it was last inside one. Null when that
    /// enclosure has ceased to exist.
    EnclosureId home;
    bool loose = false;              ///< outside every enclosure (escaped or left homeless)
    bool breeding = true;            ///< the player allows it to breed
    std::int64_t dueTick = 0;        ///< pregnant when > 0: the tick of birth
    AnimalId mate;                   ///< father of the litter it carries
    std::int64_t lastBirthTick = 0;  ///< 0 = never gave birth

    bool Pregnant() const { return dueTick > 0; }
    bool InWorld() const { return activity != AnimalActivity::Crated; }
    /// Visible to visitors: in the world and not inside a shelter.
    bool Visible() const { return InWorld() && !shelter; }
    bool operator==(const Animal&) const = default;
};

/// Age in game days / in years of the species' life (a game year stands for a real year).
float AgeDays(const Animal& animal, GameTime now);
float AgeYears(const Animal& animal, GameTime now);
bool IsAdult(const Animal& animal, const SpeciesDef& species, GameTime now);
/// Body size relative to an adult: from about 0.35 at birth to 1 at maturity (smooth).
float BodyScale(const Animal& animal, const SpeciesDef& species, GameTime now);

/// The animals of a zoo: their needs, behaviour, health, breeding and escapes.
///
/// Step() is deterministic: equal state and equal inputs (Env contents, viewer
/// counts) give bit-identical results. The class owns its random stream.
class Animals {
public:
    explicit Animals(std::uint64_t seed = 1);

    // ---- population -------------------------------------------------------------------

    /// Puts a new animal of `ageYears` into the world at `position` (which must be a
    /// tile it can stand on: in bounds, no building, water within its limits; the
    /// null id otherwise, also for an unknown species). It gets a fresh name, mild
    /// random needs and starts Idle. Buying and its price are the caller's business.
    AnimalId Add(const Env& env, int species, Sex sex, float ageYears, Vec2 position);
    /// Removes an animal without ceremony (sold, released to the wild).
    bool Remove(AnimalId id);
    Animal* Find(AnimalId id);
    const Animal* Find(AnimalId id) const;
    /// Ascending id; dead animals are gone from the list in the step they die.
    const std::vector<Animal>& All() const { return animals_; }

    /// The animals currently inside an enclosure (by position), ascending id.
    void InEnclosure(const Env& env, EnclosureId enclosure, std::vector<AnimalId>& out) const;
    /// The group an animal lives in right now (everything 0 / false when it is not in an enclosure).
    GroupInfo GroupOf(const Env& env, const Animal& animal) const;

    // ---- simulation -------------------------------------------------------------------

    /// Advances every animal by one tick.
    void Step(Env& env);

    /// Staff currently inside habitats. This is an input rebuilt by Zoo before Step(), not saved,
    /// compared or hashed; shy animals use it to retreat from a keeper in their own enclosure.
    void ClearKeepers() { keepers_.clear(); }
    void SetKeeper(EnclosureId enclosure, Vec2 position) {
        if (enclosure) {
            keepers_.push_back({enclosure, position});
        }
    }

    /// Recalls every animal standing in `enclosure` to an assigned registered shelter,
    /// provided all of them fit and every assigned door is usable by its species. Assignment
    /// is atomic: false leaves every animal unchanged. A resident loose outside the habitat
    /// also makes it fail, since the habitat cannot then be made safe for staff. The caller
    /// must hold a staff lockout while they walk and wait until every animal is sheltered.
    bool SecureForStaff(const Env& env, EnclosureId enclosure);
    /// Habitats whose animals must remain secured this tick. Like keeper positions, this is
    /// transient input rebuilt by Zoo before Step(), never saved, compared or hashed.
    void ClearStaffLockouts() { staffLockouts_.clear(); }
    void AddStaffLockout(EnclosureId enclosure);
    bool StaffLockout(EnclosureId enclosure) const;

    /// The full report for one animal, evaluated now.
    WelfareReport Welfare(const Env& env, const Animal& animal) const;

    /// What an animal would fetch if sold today: the species' price scaled by age
    /// (young adults are worth most) and health.
    Money Value(const Env& env, const Animal& animal) const;

    // ---- care (called by staff and by the player's direct orders) ----------------------

    /// Takes an animal out of the world into a crate (to move or sell it). False if
    /// unknown or already crated.
    bool Crate(AnimalId id);
    /// Sets a crated animal down at `position` (same conditions as Add()).
    bool Uncrate(const Env& env, AnimalId id, Vec2 position);
    /// A loose animal has been caught: it is put back at a free spot of its home
    /// enclosure, or crated when it has none. Emits AnimalRecaptured. False if it
    /// is not loose.
    bool Recapture(const Env& env, AnimalId id);

    /// The food a feeder should hold for the animals living around it: among the
    /// foods the feeder accepts, the one the most animals of its enclosure eat
    /// (ties: the earlier in PropDef::foods); -1 when it stands in no enclosure,
    /// nothing living there eats any of them, or the prop is not a feeder.
    int FoodFor(const Env& env, const Prop& feeder) const;
    /// Throws away spoiled or unsuitable content and fills the feeder to capacity
    /// with FoodFor(). Books the food bought to Account::AnimalFood (when it cannot
    /// be afforded, buys what the balance allows). Returns what was paid.
    Money RefillFeeder(Env& env, PropId feeder);
    /// Fills a trough with fresh water (free).
    void RefillTrough(Env& env, PropId trough);
    /// Is the food in this feeder still good at env.time, and how much (kg) is left to eat?
    static float EdibleKg(const Env& env, const Prop& feeder);

    /// Dirt of an enclosure, 0 clean .. 1 filthy; grows with the animals living in it.
    float Dirt(EnclosureId enclosure) const;
    /// A keeper cleans: removes `amount` of dirt.
    void Clean(EnclosureId enclosure, float amount);

    /// A vet treats a sick animal: it will recover. Books nothing (the staff system
    /// pays the vet). False if it is not sick.
    bool Treat(AnimalId id);

    /// How many visitors are watching an enclosure right now. An input, not state:
    /// the Zoo sets it before every Step() from the visitors' positions (it is
    /// neither saved nor compared), and it is what shy animals react to.
    void SetViewers(EnclosureId enclosure, int viewers);
    int Viewers(EnclosureId enclosure) const;

    // ---- bookkeeping ------------------------------------------------------------------

    int Births() const { return births_; }  ///< since the game began
    int Deaths() const { return deaths_; }
    int Escapes() const { return escapes_; }

    /// Hash over the whole saved state (FNV-1a over the fields in a fixed order,
    /// floats by bit pattern): equal for equal states, used by determinism tests.
    std::uint64_t StateHash() const;
    /// Compares the saved state (everything but caches).
    bool operator==(const Animals& other) const;

private:
    friend struct AnimalsAccess;  // serialisation

    struct EnclosureCare {
        EnclosureId id;
        float dirt = 0.0f;
        /// How many people watched at the last whole second (Zoo::Step tells it): an input,
        /// but saved, as it steers the animals' glances until the next second.
        int viewers = 0;
    };
    struct HabitatCache {
        EnclosureId enclosure;
        int species = -1;
        GroupInfo group;
        WelfareReport report;
        std::uint32_t enclosureRevision = 0;
        std::uint32_t objectRevision = 0;
        std::uint32_t worldRevision = 0;
        std::int64_t refreshHour = -1;
    };
    struct KeeperPresence {
        EnclosureId enclosure;
        Vec2 position;
    };

    std::vector<Animal> animals_;             // ascending id
    std::vector<EnclosureCare> care_;         // ascending id
    std::vector<KeeperPresence> keepers_;     // current input, not simulation state
    std::vector<EnclosureId> staffLockouts_;  // current input, sorted, not simulation state
    std::vector<std::vector<std::size_t>> enclosureScratch_;
    std::vector<PropId> propScratch_;
    std::vector<NavObstacle> navScratch_;
    std::vector<HabitatCache> habitatScratch_;
    void GroupAnimals(const Env& env);
    void UpdateEnclosureDirt(const Env& env);
    WelfareReport WelfareWithGroup(const Env& env, const Animal& animal, const GroupInfo& group,
                                   const WelfareReport* cachedHabitat = nullptr) const;
    const WelfareReport& CachedHabitat(const Env& env, const Animal& animal, const GroupInfo& group);
    void StepAnimals(Env& env, const std::vector<Enclosure>& enclosures, int& pathCalls);
    bool AvoidKeeper(Animal& animal, const SpeciesDef& species, const Env& env, EnclosureId enclosure,
                     bool urgentNeed);
    void AdvanceLife(Env& env);
    void ForgetAnimalReferences(AnimalId id);
    std::uint32_t nextId_ = 1;
    Rng rng_;
    int births_ = 0;
    int deaths_ = 0;
    int escapes_ = 0;
};

}  // namespace zoo
