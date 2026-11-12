// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <deque>
#include <string>
#include <string_view>
#include <vector>

#include "zoo/animals/Animals.hpp"
#include "zoo/animals/Species.hpp"
#include "zoo/base/Ids.hpp"
#include "zoo/base/Money.hpp"
#include "zoo/build/Builder.hpp"
#include "zoo/build/Catalog.hpp"
#include "zoo/build/Enclosures.hpp"
#include "zoo/build/Objects.hpp"
#include "zoo/economy/Finance.hpp"
#include "zoo/nav/Nav.hpp"
#include "zoo/sim/Calendar.hpp"
#include "zoo/sim/Env.hpp"
#include "zoo/sim/Events.hpp"
#include "zoo/sim/WeatherSystem.hpp"
#include "zoo/visitors/Visitors.hpp"
#include "zoo/world/World.hpp"

namespace zoo {

/// How a new game starts.
struct ZooConfig {
    std::string name = "New Zoo";
    WorldGenParams world;
    /// Seeds everything random in the simulation (the map has its own seed in `world`).
    std::uint64_t seed = 1;
    Money startingMoney = Dollars(50000);
    /// Nothing costs anything and nothing is earned: for building freely and for tests.
    bool sandbox = false;
    /// Feeders and troughs are filled, enclosures cleaned and sick animals treated
    /// without keepers and vets having to be hired (the food and the treatment are
    /// still paid for). On by default until a zoo has staff of its own.
    bool autoCare = true;
    /// How many steps one tick of the calendar takes (GameTime): 1 is the pace of a
    /// management game (a day in twelve minutes), 12 lets a game hour last six
    /// minutes, 0 stops the calendar. Bodies move at the same speed whatever it is.
    int dayPace = 1;
    Climate climate;

    bool operator==(const ZooConfig&) const = default;
};

enum class Severity : std::uint8_t {
    Info,     ///< "Day 3 closed with a profit of $420."
    Good,     ///< a birth, a recovery
    Warning,  ///< an animal is hungry, unhappy, sick
    Alarm,    ///< an escape, a death
};

/// A line of the message list. `text` is ready to show.
struct Notification {
    std::uint32_t id = 0;  ///< ascending
    std::int64_t tick = 0;
    Severity severity = Severity::Info;
    std::string text;
    /// Where to look, when `located`.
    Vec2 position;
    bool located = false;
    AnimalId animal;
    EnclosureId enclosure;

    bool operator==(const Notification&) const = default;
};

/// Why a direct order of the player was not carried out.
enum class ActionError : std::uint8_t {
    None,
    UnknownSpecies,
    UnknownAnimal,
    UnknownObject,
    BadPlace,  ///< an animal cannot be set down there (off the map, a building, deep water)
    NoMoney,
    NotLoose,
    NotSick,
};
/// A sentence for the player; empty for None.
std::string_view Describe(ActionError e);

struct BuyResult {
    ActionError error = ActionError::None;
    AnimalId animal;
    Money price = 0;

    constexpr bool Ok() const { return error == ActionError::None; }
};

/// A zoo: the whole simulation of one game, and the one object the game layer
/// (rendering, UI, audio) talks to.
///
/// Owns the map, everything built on it, the animals, the visitors, the money, the weather
/// and the clock. Step() advances all of it by one tick of kTickSeconds and is
/// deterministic: two zoos built from equal configurations and given the same
/// sequence of calls stay bit-identical (see StateHash()).
///
/// The parts are reachable for reading; the map and the objects are changed
/// only through Build(), which keeps the enclosures and the books right.
class Zoo {
public:
    /// What autoCare charges for treating one sick animal.
    static constexpr Money kTreatmentFee = Dollars(180);
    static constexpr std::size_t kMaxNotifications = 200;

    /// A new game on a freshly generated map.
    Zoo(const Catalog& catalog, const SpeciesCatalog& species, const ZooConfig& config = {});
    Zoo(const Zoo&) = delete;
    Zoo& operator=(const Zoo&) = delete;

    // ---- the parts ----------------------------------------------------------------------
    const ZooConfig& Config() const { return config_; }
    const std::string& Name() const { return config_.name; }
    void Rename(std::string name);
    bool Sandbox() const { return config_.sandbox; }
    void SetAutoCare(bool on) { config_.autoCare = on; }
    /// See ZooConfig::dayPace; 0..1000.
    void SetDayPace(int pace);
    /// Sets the visitors down all over the zoo, as if it had been open for hours
    /// (Visitors::Spread). Returns the number of people moved.
    int SpreadVisitors();
    /// Puts the calendar on `day` (days since 1 March of year 1) at `hour` [0, 24).
    /// Nothing else changes: nobody ages, no day is closed. For a zoo that opens
    /// on a late morning in summer rather than at eight on 1 March.
    void SetCalendar(std::int64_t day, float hour);

    const Catalog& Definitions() const { return catalog_; }
    const SpeciesCatalog& SpeciesList() const { return species_; }

    GameTime Time() const { return time_; }
    const WeatherNow& Weather() const { return weather_.Now(); }
    const WeatherSystem& Sky() const { return weather_; }

    const World& Map() const { return world_; }
    const ZooObjects& Objects() const { return objects_; }
    /// For changing what belongs to an object but not to the map: a stall's price,
    /// whether it is open. Adding and removing goes through Build().
    ZooObjects& Objects() { return objects_; }
    const Enclosures& Habitats() const { return enclosures_; }
    bool RenameEnclosure(EnclosureId id, std::string name) { return enclosures_.Rename(id, std::move(name)); }
    /// Everything the player constructs, with prices and validation.
    Builder& Build() { return builder_; }
    const Builder& Build() const { return builder_; }
    /// A builder that charges nothing whatever the game mode: for scenario
    /// scripts that set a zoo up before the game begins.
    Builder FreeBuilder() { return Builder(world_, objects_, enclosures_, catalog_, nullptr); }
    const Finance& Accounts() const { return finance_; }
    Finance& Accounts() { return finance_; }
    const Navigator& Routes() const { return nav_; }
    const Animals& Fauna() const { return animals_; }
    Animals& Fauna() { return animals_; }
    /// The visitors: who is in the zoo, the ticket price, the gate, the rating.
    const Visitors& Guests() const { return visitors_; }
    Visitors& Guests() { return visitors_; }

    /// The view of the zoo the subsystems step with, as of now. Events go to the
    /// zoo's own list. For queries that need one (Animals::Welfare, GroupOf, ...).
    Env MakeEnv();

    // ---- time ---------------------------------------------------------------------------
    /// Advances the whole zoo by one tick.
    void Step();

    /// Everything that happened since the last call (for the sound engine and
    /// effects), oldest first; at most a few thousand are kept between calls.
    void DrainEvents(std::vector<Event>& out);

    // ---- messages -----------------------------------------------------------------------
    const std::deque<Notification>& Notifications() const { return notifications_; }
    /// The id the next notification will get: remember it to tell new ones apart.
    std::uint32_t NextNotificationId() const { return nextNotification_; }
    void Notify(Severity severity, std::string text);
    void NotifyAt(Severity severity, std::string text, Vec2 position, AnimalId animal = {},
                  EnclosureId enclosure = {});

    // ---- the player's direct orders -----------------------------------------------------
    /// What an animal of this species costs to buy; 0 for an unknown species.
    Money PriceOf(int species) const;
    /// Buys an animal and sets it down at `position`. `ageYears` < 0 buys a young
    /// adult. Books AnimalPurchases.
    BuyResult BuyAnimal(int species, Sex sex, Vec2 position, float ageYears = -1.0f);
    /// Sells an animal for Animals::Value() (AnimalSales). Returns what it fetched,
    /// or -1 for an unknown animal.
    Money SellAnimal(AnimalId id);
    /// Carries an animal to another place (crate, set down).
    ActionError MoveAnimal(AnimalId id, Vec2 position);
    /// Catches a loose animal and returns it to its enclosure.
    ActionError RecaptureAnimal(AnimalId id);
    /// Treats a sick animal now (Veterinary, kTreatmentFee).
    ActionError TreatAnimal(AnimalId id);
    /// Fills a feeder with what its animals eat (AnimalFood); returns what was paid.
    Money RefillFeeder(PropId feeder);
    void RefillTrough(PropId trough);

    // ---- statistics ---------------------------------------------------------------------
    /// Animals in the world or crated, of one species (-1: all).
    int AnimalCount(int species = -1) const;
    /// Mean welfare of the animals in the world, 0..1 (1 for an empty zoo).
    float MeanWelfare() const;
    /// What the buildings and fences cost per game day.
    Money DailyUpkeep() const;

    // ---- determinism and persistence ----------------------------------------------------
    /// Hash of the simulation state (not of messages or statistics caches).
    std::uint64_t StateHash() const;
    /// True when the two zoos are in the same saved state.
    bool SameState(const Zoo& other) const;

private:
    friend struct ZooAccess;  // persistence

    struct Parts;
    Zoo(const Catalog& catalog, const SpeciesCatalog& species, Parts&& parts);

    void AutoCare(Env& env);
    void CloseDay(std::int64_t day);
    void ReportEvents(std::size_t first);
    std::string AnimalLabel(AnimalId id) const;
    void RefreshRoster();
    Notification& Push(Severity severity, std::string text);

    const Catalog& catalog_;
    const SpeciesCatalog& species_;
    ZooConfig config_;
    World world_;
    ZooObjects objects_;
    Enclosures enclosures_;
    Finance finance_;
    Builder builder_;
    Navigator nav_;
    Animals animals_;
    Visitors visitors_;
    WeatherSystem weather_;
    GameTime time_;

    std::vector<Event> events_;  // undrained
    std::deque<Notification> notifications_;
    std::uint32_t nextNotification_ = 1;

    /// Names of the animals alive before the running step: an animal that dies is
    /// gone from the list by the time its death is reported.
    struct RosterEntry {
        AnimalId id;
        int species = -1;
        std::string name;
    };
    std::vector<RosterEntry> roster_;
    /// When visitors last ran from a loose animal: one message per scare, not per group.
    std::int64_t lastScareNote_ = -1000000;
};

/// The pieces a Zoo is assembled from, either new or read from a save.
struct Zoo::Parts {
    ZooConfig config;
    World world;
    ZooObjects objects;
    Finance finance;
    Animals animals;
    Visitors visitors;
    WeatherSystem weather;
    GameTime time;
};

}  // namespace zoo
