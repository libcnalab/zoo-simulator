// SPDX-License-Identifier: MIT
#include "zoo/sim/Zoo.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <utility>

namespace zoo {

namespace {

constexpr std::size_t kMaxPendingEvents = 4096;
/// Game hours at which autoCare does its rounds.
constexpr std::array<int, 2> kCareHours = {7, 15};
/// A feeder is topped up when less than this share of its capacity is still edible.
constexpr float kRefillBelow = 0.35f;

struct Hasher {
    std::uint64_t value = 0xcbf29ce484222325ULL;

    void Add(std::uint64_t v) {
        for (int i = 0; i < 8; ++i) {
            value ^= (v >> (i * 8)) & 0xffu;
            value *= 0x100000001b3ULL;
        }
    }
    void Add(float v) { Add(static_cast<std::uint64_t>(std::bit_cast<std::uint32_t>(v))); }
    void Add(std::int64_t v) { Add(static_cast<std::uint64_t>(v)); }
    void Add(int v) { Add(static_cast<std::uint64_t>(static_cast<std::int64_t>(v))); }
    void Add(std::uint32_t v) { Add(static_cast<std::uint64_t>(v)); }
    void Add(bool v) { Add(static_cast<std::uint64_t>(v ? 1 : 0)); }
};

std::string_view DeathText(int cause) {
    switch (static_cast<DeathCause>(cause)) {
        case DeathCause::OldAge:
            return "of old age";
        case DeathCause::Starvation:
            return "of starvation";
        case DeathCause::Thirst:
            return "of thirst";
        case DeathCause::Illness:
            return "of an illness";
        case DeathCause::Cold:
            return "of the cold";
        case DeathCause::Heat:
            return "of the heat";
        case DeathCause::Killed:
            return "after an attack";
        case DeathCause::Injury:
            return "of its injuries";
        case DeathCause::Count:
            break;
    }
    return "";
}

std::pair<TileCoord, TileCoord> Beside(EdgeRef edge) {
    if (edge.horizontal) {
        return {{edge.x, edge.z - 1}, {edge.x, edge.z}};
    }
    return {{edge.x - 1, edge.z}, {edge.x, edge.z}};
}

bool SealedApocalypseSpecies(std::string_view id) {
    return id == "corn_snake" || id == "black_headed_python";
}

}  // namespace

std::string_view Describe(ActionError e) {
    switch (e) {
        case ActionError::None:
            return "";
        case ActionError::UnknownSpecies:
            return "There is no such species.";
        case ActionError::UnknownAnimal:
            return "There is no such animal.";
        case ActionError::UnknownObject:
            return "There is no such object.";
        case ActionError::BadPlace:
            return "An animal cannot be set down here.";
        case ActionError::NoMoney:
            return "Not enough money.";
        case ActionError::NotLoose:
            return "This animal is not on the loose.";
        case ActionError::NotSick:
            return "This animal is not sick.";
    }
    return "";
}

Zoo::Zoo(const Catalog& catalog, const SpeciesCatalog& species, const ZooConfig& config)
    : Zoo(catalog, species,
          Parts{config, World::Generate(config.world), ZooObjects{}, Finance(config.startingMoney),
                Animals(config.seed), Visitors(config.seed), WeatherSystem(config.seed, config.climate),
                GameTime{}, Staff{}}) {}

Zoo::Zoo(const Catalog& catalog, const SpeciesCatalog& species, Parts&& parts)
    : catalog_(catalog),
      species_(species),
      config_(std::move(parts.config)),
      world_(std::move(parts.world)),
      objects_(std::move(parts.objects)),
      finance_(std::move(parts.finance)),
      builder_(world_, objects_, enclosures_, catalog_, config_.sandbox ? nullptr : &finance_),
      nav_(world_),
      animals_(std::move(parts.animals)),
      visitors_(std::move(parts.visitors)),
      staff_(std::move(parts.staff)),
      weather_(std::move(parts.weather)),
      time_(parts.time) {
    enclosures_.Rebuild(world_, objects_, catalog_);
}

void Zoo::Rename(std::string name) {
    if (name.size() > 40) {
        name.resize(40);
    }
    if (!name.empty()) {
        config_.name = std::move(name);
    }
}

Env Zoo::MakeEnv() {
    return Env{
        world_,   objects_, enclosures_,   catalog_, species_, nav_, config_.sandbox ? nullptr : &finance_,
        &events_, time_,    weather_.Now(), apocalypse_.active};
}

// ---- time ---------------------------------------------------------------------------------

void Zoo::SetDayPace(int pace) {
    config_.dayPace = std::clamp(pace, 0, 1000);
}

void Zoo::SetCalendar(std::int64_t day, float hour) {
    const std::int64_t wanted =
        std::max<std::int64_t>(day, 0) * kTicksPerDay +
        static_cast<std::int64_t>(std::clamp(hour, 0.0f, 23.999f) * static_cast<float>(kTicksPerGameHour));
    time_.shift = wanted - time_.MotionTick();
    // The weather of the new hour (the temperature, the light), as the last step would have left it.
    GameTime before = time_;
    if ((before.tick > 0 || before.shift != 0) && before.AbsoluteTick() > 0) {
        --before.shift;
    }
    weather_.Restore(weather_.State(), before);
}

void Zoo::Step() {
    // The calendar moves with this step or stands (ZooConfig::dayPace); the sky moves with it.
    const int pace = config_.dayPace;
    const bool calendarMoves = pace == 1 || (pace > 1 && (time_.tick + 1) % pace == 0);
    if (calendarMoves) {
        weather_.Step(time_);
    }
    if (events_.size() > kMaxPendingEvents) {
        events_.erase(events_.begin(), events_.begin() + static_cast<std::ptrdiff_t>(events_.size() / 2));
    }
    const std::size_t firstEvent = events_.size();
    RefreshRoster();

    Env env = MakeEnv();
    ContinueApocalypse();
    animals_.ClearKeepers();
    animals_.ClearStaffLockouts();
    for (const StaffMember& member : staff_.All()) {
        animals_.SetKeeper(enclosures_.At(member.position), member.position);
        animals_.AddStaffLockout(member.lockout);
    }
    for (const Enclosure& home : enclosures_.All()) {
        if (!home.walkThrough)
            continue;
        const auto note = [&](Vec2 at) {
            const auto tile = World::TileAt(at);
            if (tile.x >= home.min.x && tile.x <= home.max.x && tile.z >= home.min.z &&
                tile.z <= home.max.z && enclosures_.At(tile) == home.id)
                animals_.NotePerson(home.id, at);
        };
        if (const Vec2* player = visitors_.Bystander())
            note(*player);
        for (const Visitor& guest : visitors_.All()) {
            if (!guest.inside)
                note(guest.position);
        }
    }
    animals_.Step(env);
    visitors_.ClearMovingObstacles();
    for (const StaffMember& member : staff_.All()) {
        visitors_.AddMovingObstacle(member.position);
    }
    for (const Animal& animal : animals_.All()) {
        if (!animal.Visible())
            continue;
        const auto tile = World::TileAt(animal.position);
        if (world_.InBounds(tile) && world_.At(tile).path != PathKind::None) {
            const Enclosure* home = enclosures_.Find(animal.home);
            if (home && home->walkThrough)
                visitors_.AddMovingObstacle(animal.position);
        }
    }
    visitors_.Step(env, animals_);
    staff_.Step(env, animals_, visitors_);
    if (time_.tick % kTicksPerSecond == 0) {
        TellViewers();
    }

    if (config_.autoCare) {
        // By the motion clock: the animals eat at the pace they move at.
        const std::int64_t tickOfDay = time_.MotionTick() % kTicksPerDay;
        for (const int hour : kCareHours) {
            if (tickOfDay == static_cast<std::int64_t>(hour) * kTicksPerGameHour) {
                AutoCare(env);
            }
        }
    }

    const std::int64_t day = time_.Day();
    ++time_.tick;
    if (!calendarMoves) {
        --time_.shift;
    }
    if (time_.Day() != day) {
        CloseDay(day);
    }
    ReportEvents(firstEvent);
}

bool Zoo::EnableApocalypse(std::string* error) {
    if (apocalypse_.active) {
        return true;
    }
    ApocalypseState next;
    next.active = true;
    for (const Enclosure& enclosure : enclosures_.All()) {
        bool occupied = false;
        bool sealed = true;
        for (const Animal& animal : animals_.All()) {
            if (!animal.InWorld() || animal.home != enclosure.id || animal.species < 0 ||
                static_cast<std::size_t>(animal.species) >= species_.All().size()) {
                continue;
            }
            occupied = true;
            sealed = sealed &&
                     SealedApocalypseSpecies(species_.All()[static_cast<std::size_t>(animal.species)].id);
        }
        if (!occupied || sealed) {
            continue;
        }
        const EdgeRef* chosen = nullptr;
        for (const EdgeRef& gate : enclosure.gates) {
            const Fence fence = world_.FenceAt(gate);
            if (!fence.Exists() || !fence.gate || fence.visitorGate || fence.kind == kPerimeterFence) {
                continue;
            }
            const auto [a, b] = Beside(gate);
            const TileCoord inside = enclosures_.At(a) == enclosure.id ? a : b;
            const TileCoord outside = inside == a ? b : a;
            if (enclosures_.At(inside) != enclosure.id || !world_.InBounds(outside) ||
                enclosures_.RegionAt(outside) != Region::Public || world_.At(outside).standOff ||
                world_.At(outside).building || world_.HasWater(outside) ||
                !nav_.Passable(outside, NavProfile::Animal(0.0f, false))) {
                continue;
            }
            chosen = &gate;
            break;
        }
        if (chosen == nullptr) {
            if (error != nullptr) {
                *error = enclosure.name + " has no safe public-side service gate.";
            }
            return false;
        }
        next.breaches.push_back({enclosure.id, *chosen, world_.FenceAt(*chosen)});
    }
    if (next.breaches.empty()) {
        if (error != nullptr) {
            *error = "There are no occupied outdoor habitats to open.";
        }
        return false;
    }
    apocalypse_ = std::move(next);
    for (const ApocalypseBreach& breach : apocalypse_.breaches) {
        world_.SetFence(breach.edge, {});
    }
    ContinueApocalypse();
    Notify(Severity::Alarm, "APOCALYPSE mode opened the habitat service gates.");
    return true;
}

void Zoo::ContinueApocalypse() {
    if (!apocalypse_.active) {
        return;
    }
    for (const ApocalypseBreach& breach : apocalypse_.breaches) {
        const auto [a, b] = Beside(breach.edge);
        const TileCoord inside = enclosures_.At(a) == breach.enclosure ? a : b;
        if (enclosures_.At(inside) != breach.enclosure) {
            continue;
        }
        for (const Animal& animal : animals_.All()) {
            if (!animal.InWorld() || animal.home != breach.enclosure) {
                continue;
            }
            const auto participant =
                std::lower_bound(apocalypse_.animals.begin(), apocalypse_.animals.end(), animal.id,
                                 [](const ApocalypseAnimal& item, AnimalId id) { return item.animal < id; });
            if (participant == apocalypse_.animals.end() || participant->animal != animal.id) {
                apocalypse_.animals.insert(participant, {animal.id, breach.enclosure});
            }
            if (!animal.loose && animal.activity != AnimalActivity::BreakOut) {
                animals_.ReleaseForApocalypse(animal.id, breach.enclosure, World::TileCenter(inside));
            }
        }
    }
}

void Zoo::DisableApocalypse() {
    if (!apocalypse_.active) {
        return;
    }
    Env env = MakeEnv();
    for (const ApocalypseAnimal& participant : apocalypse_.animals) {
        animals_.ReturnFromApocalypse(env, participant.animal, participant.home);
    }
    for (const ApocalypseBreach& breach : apocalypse_.breaches) {
        if (world_.EdgeInBounds(breach.edge)) {
            world_.SetFence(breach.edge, breach.former);
        }
    }
    visitors_.CalmAfterApocalypse();
    apocalypse_ = {};
    Notify(Severity::Good, "APOCALYPSE mode ended; every recorded gate was restored.");
}

void Zoo::DrainEvents(std::vector<Event>& out) {
    out.insert(out.end(), events_.begin(), events_.end());
    events_.clear();
}

void Zoo::TellViewers() {
    // The animals know how many people are looking at them.
    for (const Enclosure& enclosure : enclosures_.All()) {
        animals_.SetViewers(enclosure.id, visitors_.Watching(enclosure.id));
    }
}

void Zoo::AutoCare(Env& env) {
    // Collect first: refilling changes the props.
    std::vector<PropId> feeders;
    std::vector<PropId> troughs;
    for (const Prop& prop : objects_.Props()) {
        const PropDef& def = catalog_.props[static_cast<std::size_t>(prop.def)];
        if (def.role == PropRole::Feeder) {
            if (Animals::EdibleKg(env, prop) < def.capacity * kRefillBelow &&
                animals_.FoodFor(env, prop) >= 0) {
                feeders.push_back(prop.id);
            }
        } else if (def.role == PropRole::WaterTrough) {
            if (prop.amount < def.capacity * 0.6f) {
                troughs.push_back(prop.id);
            }
        }
    }
    for (const PropId id : feeders) {
        animals_.RefillFeeder(env, id);
    }
    for (const PropId id : troughs) {
        animals_.RefillTrough(env, id);
    }
    for (const Enclosure& enclosure : enclosures_.All()) {
        animals_.Clean(enclosure.id, 1.0f);
    }
    std::vector<AnimalId> sick;
    for (const Animal& animal : animals_.All()) {
        if (animal.sick && !animal.treated) {
            sick.push_back(animal.id);
        }
    }
    for (const AnimalId id : sick) {
        if (env.finance != nullptr && !env.finance->TrySpend(Account::Veterinary, kTreatmentFee)) {
            break;
        }
        animals_.Treat(id);
    }
}

Money Zoo::DailyUpkeep() const {
    Money total = 0;
    for (const Building& building : objects_.Buildings()) {
        total += catalog_.buildings[static_cast<std::size_t>(building.def)].upkeepPerDay;
    }
    // Fences: counted without wearing them.
    for (const FenceDef& def : catalog_.fences) {
        if (def.upkeepPerDay <= 0) {
            continue;
        }
        int count = 0;
        for (int z = 0; z <= world_.Depth(); ++z) {
            for (int x = 0; x <= world_.Width(); ++x) {
                for (const bool horizontal : {true, false}) {
                    const EdgeRef edge{x, z, horizontal};
                    if (world_.EdgeInBounds(edge) && world_.FenceAt(edge).kind == def.kind) {
                        ++count;
                    }
                }
            }
        }
        total += def.upkeepPerDay * count;
    }
    return total;
}

void Zoo::CloseDay(std::int64_t day) {
    std::array<float, 256> wear{};
    for (const FenceDef& def : catalog_.fences) {
        if (def.kind > 0 && def.kind < 255) {
            wear[static_cast<std::size_t>(def.kind)] = def.wearPerDay;
        }
    }
    std::array<int, 256> counts{};
    world_.WearFences(wear, counts);

    Money upkeep = 0;
    for (const FenceDef& def : catalog_.fences) {
        if (def.kind > 0 && def.kind < 255) {
            upkeep += def.upkeepPerDay * counts[static_cast<std::size_t>(def.kind)];
        }
    }
    std::vector<BuildingId> buildings;
    for (const Building& building : objects_.Buildings()) {
        upkeep += catalog_.buildings[static_cast<std::size_t>(building.def)].upkeepPerDay;
        buildings.push_back(building.id);
    }
    for (const BuildingId id : buildings) {
        objects_.FindBuilding(id)->servedToday = 0;
    }
    // Worn fences change what the enclosures hold in.
    enclosures_.Rebuild(world_, objects_, catalog_);

    const int guests = visitors_.ArrivedToday();
    visitors_.CloseDay();
    if (config_.autoCare) {
        // The night shift is the auto-care safety net behind a visible cleaner's
        // daytime round: empty anything and sweep any litter it did not reach.
        std::vector<PropId> bins;
        for (const Prop& prop : objects_.Props()) {
            if (catalog_.props[static_cast<std::size_t>(prop.def)].role == PropRole::Bin &&
                prop.amount > 0.0f) {
                bins.push_back(prop.id);
            }
        }
        for (const PropId id : bins) {
            objects_.FindProp(id)->amount = 0.0f;
        }
        visitors_.CleanAllLitter();
    }

    if (config_.sandbox) {
        return;
    }
    if (upkeep > 0) {
        finance_.Pay(Account::Upkeep, upkeep);
    }
    const DayReport report = finance_.CloseDay(day);
    const Money profit = report.Profit();
    std::string text = "Day " + std::to_string(day + 1) + " closed with a ";
    text += profit >= 0 ? "profit of " : "loss of ";
    text += FormatMoney(profit >= 0 ? profit : -profit);
    text += guests == 1 ? "; 1 visitor." : "; " + std::to_string(guests) + " visitors.";
    Push(profit >= 0 ? Severity::Info : Severity::Warning, std::move(text));
    if (finance_.Balance() < 0) {
        Push(Severity::Alarm, "The zoo is in debt. Raise income or take a loan before the bills pile up.");
    }
}

// ---- messages -----------------------------------------------------------------------------

Notification& Zoo::Push(Severity severity, std::string text) {
    Notification n;
    n.id = nextNotification_++;
    n.tick = time_.tick;
    n.severity = severity;
    n.text = std::move(text);
    notifications_.push_back(std::move(n));
    while (notifications_.size() > kMaxNotifications) {
        notifications_.pop_front();
    }
    return notifications_.back();
}

void Zoo::Notify(Severity severity, std::string text) {
    Push(severity, std::move(text));
}

void Zoo::NotifyAt(Severity severity, std::string text, Vec2 position, AnimalId animal,
                   EnclosureId enclosure) {
    Notification& n = Push(severity, std::move(text));
    n.position = position;
    n.located = true;
    n.animal = animal;
    n.enclosure = enclosure;
}

void Zoo::RefreshRoster() {
    const std::vector<Animal>& all = animals_.All();
    if (roster_.size() == all.size() && (all.empty() || roster_.back().id == all.back().id)) {
        return;
    }
    roster_.clear();
    roster_.reserve(all.size());
    for (const Animal& animal : all) {
        roster_.push_back({animal.id, animal.species, animal.name});
    }
}

std::string Zoo::AnimalLabel(AnimalId id) const {
    int species = -1;
    std::string name;
    if (const Animal* animal = animals_.Find(id)) {
        species = animal->species;
        name = animal->name;
    } else {
        const auto it =
            std::lower_bound(roster_.begin(), roster_.end(), id,
                             [](const RosterEntry& e, AnimalId key) { return e.id.value < key.value; });
        if (it != roster_.end() && it->id == id) {
            species = it->species;
            name = it->name;
        }
    }
    if (species < 0 || static_cast<std::size_t>(species) >= species_.All().size()) {
        return name.empty() ? "An animal" : name;
    }
    const std::string& kind = species_.All()[static_cast<std::size_t>(species)].commonName;
    return name.empty() ? "A " + kind : name + " (" + kind + ")";
}

void Zoo::ReportEvents(std::size_t first) {
    for (std::size_t i = first; i < events_.size(); ++i) {
        const Event event = events_[i];
        const std::string who = AnimalLabel(event.animal);
        const Enclosure* enclosure = enclosures_.Find(event.enclosure);
        Severity severity = Severity::Info;
        std::string text;
        switch (event.type) {
            case EventType::AnimalBorn:
                severity = Severity::Good;
                text = who + " was born";
                if (event.other) {
                    text += " to " + AnimalLabel(event.other);
                }
                text += ".";
                break;
            case EventType::AnimalDied:
                severity = Severity::Alarm;
                text = who + " died " + std::string(DeathText(event.detail)) + ".";
                break;
            case EventType::AnimalEscaped:
                severity = Severity::Alarm;
                text = who + " has escaped";
                if (enclosure != nullptr) {
                    text += " from " + enclosure->name;
                }
                text += "!";
                break;
            case EventType::AnimalRecaptured:
                severity = Severity::Good;
                text = who + " was caught";
                text += enclosure != nullptr ? " and returned to " + enclosure->name + "." : " and crated.";
                break;
            case EventType::AnimalSick:
                severity = Severity::Warning;
                text = who + " is sick and needs a vet.";
                break;
            case EventType::AnimalRecovered:
                severity = Severity::Good;
                text = who + " has recovered.";
                break;
            case EventType::AnimalStarving:
                severity = Severity::Warning;
                text = who + " is starving. Check the feeders.";
                break;
            case EventType::AnimalThirsty:
                severity = Severity::Warning;
                text = who + " is very thirsty. Check the water.";
                break;
            case EventType::AnimalFight:
                severity = Severity::Warning;
                text = who + " fought with " + AnimalLabel(event.other) + ".";
                break;
            case EventType::AnimalKilled:
                severity = Severity::Alarm;
                text = who + " killed " + AnimalLabel(event.other) + ".";
                break;
            case EventType::AnimalPregnant:
                severity = Severity::Good;
                text = who + " is pregnant.";
                break;
            case EventType::AnimalUnhappy:
                severity = Severity::Warning;
                text = who + " is unhappy with its enclosure.";
                break;
            case EventType::AnimalCalled:
            case EventType::VisitorArrived:
            case EventType::VisitorLeft:
            case EventType::VisitorBought:
            case EventType::VisitorDonated:
            case EventType::VisitorLittered:
            case EventType::VisitorThought:
                break;
            case EventType::VisitorScared:
                if (time_.tick - lastScareNote_ >= kTicksPerGameHour) {
                    lastScareNote_ = time_.tick;
                    severity = Severity::Alarm;
                    text = "Visitors are running from a dangerous animal on the loose!";
                }
                break;
        }
        if (!text.empty()) {
            NotifyAt(severity, std::move(text), event.position, event.animal, event.enclosure);
        }
    }
}

// ---- the player's direct orders -----------------------------------------------------------

Money Zoo::PriceOf(int species) const {
    if (species < 0 || static_cast<std::size_t>(species) >= species_.All().size()) {
        return 0;
    }
    return Dollars(species_.All()[static_cast<std::size_t>(species)].price);
}

BuyResult Zoo::BuyAnimal(int species, Sex sex, Vec2 position, float ageYears) {
    BuyResult result;
    if (species < 0 || static_cast<std::size_t>(species) >= species_.All().size()) {
        result.error = ActionError::UnknownSpecies;
        return result;
    }
    const SpeciesDef& def = species_.All()[static_cast<std::size_t>(species)];
    result.price = PriceOf(species);
    if (!config_.sandbox && !finance_.CanAfford(result.price)) {
        result.error = ActionError::NoMoney;
        return result;
    }
    if (ageYears < 0.0f) {
        ageYears = def.maturityYears * 1.25f + 0.5f;
    }
    const Env env = MakeEnv();
    result.animal = animals_.Add(env, species, sex, ageYears, position);
    if (!result.animal) {
        result.error = ActionError::BadPlace;
        return result;
    }
    if (!config_.sandbox) {
        finance_.Pay(Account::AnimalPurchases, result.price);
    }
    return result;
}

Money Zoo::SellAnimal(AnimalId id) {
    const Animal* animal = animals_.Find(id);
    if (animal == nullptr) {
        return -1;
    }
    const Env env = MakeEnv();
    const Money value = animals_.Value(env, *animal);
    animals_.Remove(id);
    if (!config_.sandbox && value > 0) {
        finance_.Earn(Account::AnimalSales, value);
    }
    return value;
}

ActionError Zoo::MoveAnimal(AnimalId id, Vec2 position) {
    const Animal* animal = animals_.Find(id);
    if (animal == nullptr) {
        return ActionError::UnknownAnimal;
    }
    const Env env = MakeEnv();
    if (!animal->InWorld()) {
        return animals_.Uncrate(env, id, position) ? ActionError::None : ActionError::BadPlace;
    }
    const Vec2 from = animal->position;
    animals_.Crate(id);
    if (animals_.Uncrate(env, id, position)) {
        return ActionError::None;
    }
    animals_.Uncrate(env, id, from);
    return ActionError::BadPlace;
}

ActionError Zoo::RecaptureAnimal(AnimalId id) {
    if (animals_.Find(id) == nullptr) {
        return ActionError::UnknownAnimal;
    }
    const Env env = MakeEnv();
    return animals_.Recapture(env, id) ? ActionError::None : ActionError::NotLoose;
}

ActionError Zoo::TreatAnimal(AnimalId id) {
    const Animal* animal = animals_.Find(id);
    if (animal == nullptr) {
        return ActionError::UnknownAnimal;
    }
    if (!animal->sick || animal->treated) {
        return ActionError::NotSick;
    }
    if (!config_.sandbox && !finance_.TrySpend(Account::Veterinary, kTreatmentFee)) {
        return ActionError::NoMoney;
    }
    animals_.Treat(id);
    return ActionError::None;
}

int Zoo::SpreadVisitors() {
    const Env env = MakeEnv();
    return visitors_.Spread(env, animals_);
}

Money Zoo::RefillFeeder(PropId feeder) {
    Env env = MakeEnv();
    return animals_.RefillFeeder(env, feeder);
}

void Zoo::RefillTrough(PropId trough) {
    Env env = MakeEnv();
    animals_.RefillTrough(env, trough);
}

// ---- statistics ---------------------------------------------------------------------------

int Zoo::AnimalCount(int species) const {
    int count = 0;
    for (const Animal& animal : animals_.All()) {
        if (species < 0 || animal.species == species) {
            ++count;
        }
    }
    return count;
}

float Zoo::MeanWelfare() const {
    float sum = 0.0f;
    int count = 0;
    for (const Animal& animal : animals_.All()) {
        if (animal.InWorld()) {
            sum += animal.welfare;
            ++count;
        }
    }
    return count == 0 ? 1.0f : sum / static_cast<float>(count);
}

// ---- determinism --------------------------------------------------------------------------

std::uint64_t Zoo::StateHash() const {
    Hasher h;
    h.Add(time_.tick);
    h.Add(time_.shift);
    h.Add(animals_.StateHash());
    h.Add(visitors_.StateHash());
    if (!staff_.All().empty()) {
        h.Add(staff_.StateHash());  // a zoo without staff keeps the hash it had
    }
    h.Add(finance_.Balance());
    h.Add(finance_.Loan());
    for (const Money amount : finance_.Today().amounts) {
        h.Add(amount);
    }
    const WeatherState weather = weather_.State();
    h.Add(static_cast<int>(weather.sky));
    h.Add(weather.ticksLeft);
    h.Add(weather.cloud);
    h.Add(weather.rain);
    h.Add(weather.wind);
    h.Add(weather.offsetC);
    h.Add(weather.snowing);
    h.Add(weather.rngState);
    for (const Prop& prop : objects_.Props()) {
        h.Add(prop.id.value);
        h.Add(prop.amount);
        h.Add(prop.food);
    }
    for (const Building& building : objects_.Buildings()) {
        h.Add(building.id.value);
        h.Add(building.condition);
        h.Add(building.servedTotal);
    }
    for (const Enclosure& enclosure : enclosures_.All()) {
        h.Add(enclosure.id.value);
        h.Add(enclosure.stats.minFenceCondition);
    }
    if (apocalypse_.active) {
        h.Add(true);  // an ordinary zoo keeps its pre-feature deterministic hash
        for (const ApocalypseBreach& breach : apocalypse_.breaches) {
            h.Add(breach.enclosure.value);
            h.Add(breach.edge.x);
            h.Add(breach.edge.z);
            h.Add(breach.edge.horizontal);
            h.Add(static_cast<int>(breach.former.kind));
            h.Add(breach.former.gate);
            h.Add(breach.former.condition);
            h.Add(breach.former.visitorGate);
        }
        for (const ApocalypseAnimal& participant : apocalypse_.animals) {
            h.Add(participant.animal.value);
            h.Add(participant.home.value);
        }
    }
    return h.value;
}

bool Zoo::SameState(const Zoo& other) const {
    return config_ == other.config_ && time_ == other.time_ && world_ == other.world_ &&
           objects_ == other.objects_ && enclosures_ == other.enclosures_ && finance_ == other.finance_ &&
           animals_ == other.animals_ && visitors_ == other.visitors_ && staff_ == other.staff_ &&
           weather_.State() == other.weather_.State() && lastScareNote_ == other.lastScareNote_ &&
           apocalypse_ == other.apocalypse_;
}

}  // namespace zoo
