// SPDX-License-Identifier: MIT
// The visitors of a zoo: the class, who comes when, and the order of a tick.

#include "zoo/visitors/Visitors.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <limits>
#include <span>

#include "zoo/visitors/VisitorInternal.hpp"

namespace zoo {

using namespace visitor_tuning;
using namespace visitors_detail;

bool Elderly(const Visitor& visitor) {
    return !visitor.child && Hash01(visitor.look, 4) < kElderlyShare;
}

float PaceOf(const Visitor& visitor) {
    if (visitor.child) {
        return kChildSpeed;
    }
    const float pace = kWalkSpeed + kWalkSpeedSpread * Hash01(visitor.look, 3);
    return Elderly(visitor) ? pace * kElderlyPace : pace;
}

namespace {

constexpr int kThreatCellTiles = 16;

std::uint64_t ThreatCellKey(int x, int z) {
    return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(z)) << 32u) | static_cast<std::uint32_t>(x);
}

const ViewingSpot* SpreadFront(const ViewSite& site, const Env& env, const Animals& animals,
                               std::size_t groupSize) {
    const ViewingSpot* chosen = nullptr;
    float best = std::numeric_limits<float>::max();
    for (const ViewingSpot& spot : site.spots) {
        const float score = DistanceSquared(World::TileCenter(spot.tile), site.centre) +
                            (spot.side == Dir::North ? 0.0f : 100000.0f);
        if (score >= best)
            continue;
        bool clear = true;
        for (std::size_t slot = 0; slot < groupSize && clear; ++slot) {
            clear = !animals.CrowdsResident(env.enclosures, env.species, env.time,
                                            ViewingPosition(spot, static_cast<int>(slot)));
        }
        if (clear) {
            best = score;
            chosen = &spot;
        }
    }
    return chosen;
}

void SpreadWatchers(std::span<Visitor> group, const ViewSite& site, const ViewingSpot& spot, std::int64_t now,
                    float linger, Rng& rng) {
    for (std::size_t member = 0; member < group.size(); ++member) {
        Visitor& visitor = group[member];
        visitor.slot = static_cast<std::uint8_t>(member);
        visitor.position = ViewingPosition(spot, static_cast<int>(member));
        visitor.target = visitor.position;
        visitor.heading = HeadingOf(ToVec(Step(spot.side)));
        visitor.activity = VisitorActivity::Watching;
        visitor.goal = VisitorGoal::Watch;
        visitor.targetEnclosure = site.enclosure;
    }
    group.front().activityUntil =
        now + Ticks(linger * (kWatchSeconds + kWatchSpreadSeconds * rng.NextFloat()));
    group.front().seenEnclosures.push_back(site.enclosure.value);
}

constexpr std::array<std::string_view, static_cast<std::size_t>(VisitorActivity::Count)> kActivityNames = {
    "idle",    "arriving", "walking", "watching", "queueing", "eating",
    "sitting", "reading",  "inside",  "leaving",  "fleeing"};
constexpr std::array<std::string_view, static_cast<std::size_t>(VisitorGoal::Count)> kGoalNames = {
    "none", "enter", "watch", "food",  "drink", "restroom", "rest",
    "eat",  "bin",   "shop",  "learn", "read",  "leave",    "flee"};
constexpr std::array<std::string_view, static_cast<std::size_t>(VisitorThought::Count)> kThoughtNames = {
    "none",
    "hungry",
    "thirsty",
    "needs_restroom",
    "tired",
    "loved_animal",
    "could_not_see_animals",
    "animals_look_unhappy",
    "too_crowded",
    "litter",
    "nothing_to_see",
    "too_expensive",
    "great_zoo",
    "scared",
    "lost_way"};

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
    void Add(Vec2 v) {
        Add(v.x);
        Add(v.z);
    }
};

/// How much there is to see, from the species on show and where they can be seen from.
float AttractionOf(const Env& env, const Animals& animals) {
    struct Show {
        int count = 0;
        float welfare = 0.0f;
        float visibility = 0.0f;
    };
    const std::size_t speciesCount = env.species.All().size();
    if (speciesCount == 0 || animals.All().empty()) {
        return 0.0f;
    }
    const int gate = env.nav.ComponentOf(env.world.EntranceTile(), kWalker);
    if (gate < 0) {
        return 0.0f;
    }
    // The best view into each enclosure from a path visitors can reach.
    const auto& enclosures = env.enclosures.All();
    std::vector<float> view(enclosures.size(), 0.0f);
    for (std::size_t i = 0; i < enclosures.size(); ++i) {
        for (const ViewingSpot& spot : enclosures[i].viewing) {
            if (spot.visibility > view[i] && env.nav.ComponentOf(spot.tile, kWalker) == gate) {
                view[i] = spot.visibility;
            }
        }
    }
    std::vector<Show> shows(speciesCount);
    for (const Animal& animal : animals.All()) {
        if (!animal.InWorld() || animal.loose || animal.species < 0 ||
            static_cast<std::size_t>(animal.species) >= speciesCount) {
            continue;
        }
        const auto it = std::lower_bound(enclosures.begin(), enclosures.end(), animal.home,
                                         [](const Enclosure& e, EnclosureId id) { return e.id < id; });
        if (it == enclosures.end() || it->id != animal.home) {
            continue;
        }
        const float visibility = view[static_cast<std::size_t>(it - enclosures.begin())];
        if (visibility <= 0.0f) {
            continue;
        }
        Show& show = shows[static_cast<std::size_t>(animal.species)];
        ++show.count;
        show.welfare += animal.welfare;
        show.visibility = std::max(show.visibility, visibility);
    }
    float score = 0.0f;
    for (std::size_t i = 0; i < speciesCount; ++i) {
        const Show& show = shows[i];
        if (show.count == 0) {
            continue;
        }
        const float appeal = static_cast<float>(env.species.All()[i].appeal) / 100.0f;
        const float herd = 0.6f + 0.4f * static_cast<float>(std::min(show.count, 4)) / 4.0f;
        const float welfare = 0.4f + 0.6f * show.welfare / static_cast<float>(show.count);
        score += appeal * herd * show.visibility * welfare;
    }
    return 1.0f - std::exp(-score / kAttractionScale);
}

}  // namespace

std::string_view ToString(VisitorActivity v) {
    return v < VisitorActivity::Count ? kActivityNames[static_cast<std::size_t>(v)] : "";
}
std::string_view ToString(VisitorGoal v) {
    return v < VisitorGoal::Count ? kGoalNames[static_cast<std::size_t>(v)] : "";
}
std::string_view ToString(VisitorThought v) {
    return v < VisitorThought::Count ? kThoughtNames[static_cast<std::size_t>(v)] : "";
}

// ---- the class ----------------------------------------------------------------------------

Visitors::Visitors(std::uint64_t seed) : rng_(seed, 0x7669736974ULL), scratch_(std::make_unique<Scratch>()) {}
Visitors::~Visitors() = default;
Visitors::Visitors(Visitors&&) noexcept = default;
Visitors& Visitors::operator=(Visitors&&) noexcept = default;

void Visitors::SetTicketPrice(Money price) {
    ticketPrice_ = std::clamp<Money>(price, 0, Dollars(200));
}

const Visitor* Visitors::Find(VisitorId id) const {
    const auto it = std::lower_bound(visitors_.begin(), visitors_.end(), id,
                                     [](const Visitor& v, VisitorId value) { return v.id < value; });
    return it != visitors_.end() && it->id == id ? &*it : nullptr;
}

const Visitor* Visitors::LeaderOf(VisitorId id) const {
    const Visitor* visitor = Find(id);
    while (visitor != nullptr && !visitor->leader && visitor != visitors_.data()) {
        --visitor;
    }
    return visitor;
}

int Visitors::Watching(EnclosureId enclosure) const {
    int count = 0;
    for (const Visitor& v : visitors_) {
        if (v.activity == VisitorActivity::Watching && v.targetEnclosure == enclosure) {
            ++count;
        }
    }
    return count;
}

int Visitors::CleanLitter(Vec2 position, float radius) {
    const float limit = radius * radius;
    const std::size_t before = litter_.size();
    std::erase_if(litter_, [&](const Litter& l) { return DistanceSquared(l.position, position) <= limit; });
    if (before != litter_.size() && scratch_) {
        ++scratch_->litterEdits;
    }
    return static_cast<int>(before - litter_.size());
}

int Visitors::CleanAllLitter() {
    const int count = static_cast<int>(litter_.size());
    litter_.clear();
    if (count != 0 && scratch_) {
        ++scratch_->litterEdits;
    }
    return count;
}

float Visitors::MeanHappiness() const {
    float sum = 0.0f;
    int count = 0;
    for (const Visitor& v : visitors_) {
        if (v.ticketPaid) {
            sum += v.happiness;
            ++count;
        }
    }
    return count > 0 ? sum / static_cast<float>(count) : rating_;
}

float Visitors::Attraction(const Env& env, const Animals& animals) const {
    return AttractionOf(env, animals);
}

Money Visitors::FairTicketPrice(float attraction) {
    const float dollars = kFairPriceBase + kFairPriceRange * Clamp01(attraction);
    return static_cast<Money>(std::lround(dollars * 100.0f));
}

float Visitors::ExpectedArrivalsPerHour(const Env& env, const Animals& animals) const {
    const float hour = env.time.Hour();
    if (!open_ || hour < kOpeningHour || hour >= kLastAdmissionHour) {
        return 0.0f;
    }
    if (!env.nav.Reachable(World::TileAt(env.world.SpawnPoint()), env.world.EntranceTile(), kWalker)) {
        return 0.0f;
    }
    const float attraction = AttractionOf(env, animals);
    // Busiest in the early afternoon.
    const float peak = 0.68f + 0.32f * std::exp(-(hour - 13.0f) * (hour - 13.0f) / 15.0f);
    const float t = env.weather.temperatureC;
    const float weather = std::clamp(1.0f - 0.58f * env.weather.rain - 0.012f * std::max(0.0f, 12.0f - t) -
                                         0.006f * std::max(0.0f, t - 31.0f),
                                     0.12f, 1.0f);
    const float fair = static_cast<float>(FairTicketPrice(attraction));
    const float ratio = static_cast<float>(ticketPrice_) / fair;
    const float price =
        ratio <= 1.0f ? 1.0f + kCheapBonus * (1.0f - ratio) : std::exp(-kPriceSensitivity * (ratio - 1.0f));
    return kBaseArrivalsPerHour * std::max(attraction, kMinAttraction) * peak * weather *
           (0.5f + 0.7f * rating_) * price;
}

void Visitors::SetPace(const VisitorPace& pace) {
    pace_.linger = std::clamp(pace.linger, 0.25f, 20.0f);
    pace_.stay = std::clamp(pace.stay, 0.25f, 20.0f);
    pace_.arrivals = std::clamp(pace.arrivals, 0.0f, 20.0f);
    pace_.most = std::clamp(pace.most, 0, kMaxVisitors);
}

int Visitors::Spread(const Env& env, const Animals& animals) {
    if (!scratch_) {
        scratch_ = std::make_unique<Scratch>();
    }
    Sites& sites = scratch_->sites;
    sites.Refresh(env);
    if (!sites.open) {
        return 0;
    }
    const Vec2 gate = World::TileCenter(sites.entranceTile);
    std::vector<TileCoord> tiles;
    float farthest = 1.0f;
    for (int z = 0; z < sites.depth; ++z) {
        for (int x = 0; x < sites.width; ++x) {
            const TileCoord t{x, z};
            if (env.world.At(t).path == PathKind::None || env.enclosures.RegionAt(t) != Region::Public ||
                !env.nav.Reachable(t, sites.entranceTile, kWalker)) {
                continue;
            }
            tiles.push_back(t);
            farthest = std::max(farthest, Distance(World::TileCenter(t), gate));
        }
    }
    if (tiles.size() < 2) {
        return 0;
    }
    int moved = 0;
    std::size_t nextView = 0;
    for (std::size_t first = 0; first < visitors_.size();) {
        std::size_t end = first + 1;
        while (end < visitors_.size() && visitors_[end].group == visitors_[first].group) {
            ++end;
        }
        Visitor& lead = visitors_[first];
        bool afoot = lead.ticketPaid && !lead.served;
        for (std::size_t i = first; afoot && i < end; ++i) {
            const VisitorActivity doing = visitors_[i].activity;
            afoot = (doing == VisitorActivity::Walking || doing == VisitorActivity::Idle ||
                     doing == VisitorActivity::Watching) &&
                    !visitors_[i].seat && !visitors_[i].inside;
        }
        if (afoot) {
            const Vec2 centre = World::TileCenter(
                tiles[static_cast<std::size_t>(rng_.RangeInt(0, static_cast<int>(tiles.size()) - 1))]);
            const float far = Distance(centre, gate) / farthest;
            const float heading = rng_.Range(0.0f, kTwoPi);
            for (std::size_t i = first; i < end; ++i) {
                Visitor& v = visitors_[i];
                const float angle = kTwoPi * (static_cast<float>(i - first) - 0.5f) / 4.0f;
                v.position = i == first ? centre : centre + Vec2{std::sin(angle), std::cos(angle)} * 0.6f;
                v.heading = heading;
                v.speed = 0.0f;
                v.activity = VisitorActivity::Idle;
                v.goal = VisitorGoal::None;
                v.targetEnclosure = {};
                v.targetBuilding = {};
                v.targetProp = {};
                v.target = v.position;
                v.route.clear();
                v.routeIndex = 0;
                v.hunger = std::min(0.85f, v.hunger + far * rng_.Range(0.0f, 0.4f));
                v.thirst = std::min(0.85f, v.thirst + far * rng_.Range(0.0f, 0.4f));
                v.bladder = std::min(0.85f, v.bladder + far * rng_.Range(0.0f, 0.4f));
                v.tiredness = std::min(0.85f, v.tiredness + far * rng_.Range(0.0f, 0.5f));
                ++moved;
            }
            lead.reroute = false;
            lead.lost = 0;
            lead.previousGoal = VisitorGoal::None;
            // Not all in the same breath: the groups set off over the next few seconds.
            lead.activityUntil = env.time.tick + rng_.RangeInt(0, 4 * kTicksPerSecond);
            // The deeper in, the longer ago it came (none so long ago that it goes home at once).
            const float stayTicks =
                (kVisitHours + 0.5f * kVisitSpreadHours) * static_cast<float>(kTicksPerGameHour) * pace_.stay;
            lead.arrivedTick =
                env.time.tick - static_cast<std::int64_t>(far * rng_.Range(0.15f, 0.75f) * stayTicks);
            lead.seenEnclosures.clear();
            for (const ViewSite& site : sites.views) {
                if (rng_.Chance(0.4f * far)) {
                    lead.seenEnclosures.push_back(site.enclosure.value);
                }
            }
            // Guarantee a representative group at each reachable habitat before
            // scattering the rest over the walks. Short visit fills need this too:
            // random distant paths cannot reach every front in the remaining ticks.
            if (nextView < sites.views.size()) {
                const ViewSite& site = sites.views[nextView++];
                if (const ViewingSpot* spot = SpreadFront(site, env, animals, end - first)) {
                    SpreadWatchers(std::span<Visitor>(visitors_).subspan(first, end - first), site, *spot,
                                   env.time.tick, pace_.linger, rng_);
                }
            }
        }
        first = end;
    }
    return moved;
}

void Visitors::CloseDay() {
    arrivedToday_ = 0;
    leftToday_ = 0;
}

void Visitors::Clear() {
    visitors_.clear();
    CleanAllLitter();
    arrivalRemainder_ = 0.0f;
}

void Visitors::CalmAfterApocalypse() {
    for (Visitor& visitor : visitors_) {
        if (visitor.goal != VisitorGoal::Flee && visitor.activity != VisitorActivity::Fleeing) {
            continue;
        }
        visitor.goal = VisitorGoal::None;
        visitor.activity = VisitorActivity::Idle;
        visitor.thought = VisitorThought::None;
        visitor.targetEnclosure = {};
        visitor.targetBuilding = {};
        visitor.targetProp = {};
        visitor.target = visitor.position;
        visitor.route.clear();
        visitor.routeIndex = 0;
        visitor.speed = 0.0f;
        visitor.reroute = false;
    }
}

int Visitors::SendHome(int keep) {
    keep = std::max(keep, 0);
    const int before = Count();
    if (before <= keep) {
        return 0;
    }
    // The groups, consecutive in the list, ordered by who goes first.
    struct Party {
        std::size_t first = 0, end = 0;
        float order = 0.0f;
    };
    std::vector<Party> parties;
    for (std::size_t i = 0; i < visitors_.size();) {
        std::size_t end = i + 1;
        while (end < visitors_.size() && visitors_[end].group == visitors_[i].group) {
            ++end;
        }
        const float order = hasBystander_ ? DistanceSquared(visitors_[i].position, bystander_)
                                          : static_cast<float>(visitors_[i].id.value);
        parties.push_back({i, end, order});
        i = end;
    }
    std::stable_sort(parties.begin(), parties.end(),
                     [](const Party& a, const Party& b) { return a.order > b.order; });
    std::vector<char> going(visitors_.size(), 0);
    int left = before;
    for (const Party& party : parties) {
        if (left <= keep) {
            break;
        }
        std::fill(going.begin() + static_cast<std::ptrdiff_t>(party.first),
                  going.begin() + static_cast<std::ptrdiff_t>(party.end), 1);
        left -= static_cast<int>(party.end - party.first);
    }
    std::size_t kept = 0;
    for (std::size_t i = 0; i < visitors_.size(); ++i) {
        if (going[i] == 0) {
            if (kept != i) {
                visitors_[kept] = std::move(visitors_[i]);
            }
            ++kept;
        }
    }
    visitors_.resize(kept);
    arrivalRemainder_ = 0.0f;
    return before - left;
}

std::uint64_t Visitors::StateHash() const {
    Hasher h;
    h.Add(static_cast<std::uint64_t>(visitors_.size()));
    for (const Visitor& v : visitors_) {
        h.Add(v.id.value);
        h.Add(v.look);
        h.Add(v.group);
        h.Add(v.position);
        h.Add(v.heading);
        h.Add(static_cast<int>(v.activity));
        h.Add(static_cast<int>(v.goal));
        h.Add(v.hunger);
        h.Add(v.thirst);
        h.Add(v.bladder);
        h.Add(v.tiredness);
        h.Add(v.happiness);
        h.Add(v.education);
        h.Add(static_cast<int>(v.thought));
        h.Add(v.target);
        h.Add(v.routeIndex);
        h.Add(static_cast<std::uint64_t>(v.route.size()));
        h.Add(v.cash);
        h.Add(v.spent);
        h.Add(v.activityUntil);
        h.Add(v.seenSpecies);
        h.Add(static_cast<int>(v.litter));
    }
    h.Add(static_cast<std::uint64_t>(litter_.size()));
    for (const Litter& l : litter_) {
        h.Add(l.position);
    }
    h.Add(nextId_);
    h.Add(nextGroup_);
    h.Add(rng_.State());
    h.Add(ticketPrice_);
    h.Add(open_);
    h.Add(arrivalRemainder_);
    h.Add(rating_);
    h.Add(educationRating_);
    h.Add(leftWeight_);
    h.Add(arrivedToday_);
    h.Add(leftToday_);
    h.Add(arrivedEver_);
    return h.value;
}

bool Visitors::operator==(const Visitors& other) const {
    return visitors_ == other.visitors_ && litter_ == other.litter_ && nextId_ == other.nextId_ &&
           nextGroup_ == other.nextGroup_ && rng_.State() == other.rng_.State() &&
           rng_.Stream() == other.rng_.Stream() && ticketPrice_ == other.ticketPrice_ &&
           open_ == other.open_ && arrivalRemainder_ == other.arrivalRemainder_ && rating_ == other.rating_ &&
           educationRating_ == other.educationRating_ && leftWeight_ == other.leftWeight_ &&
           arrivedToday_ == other.arrivedToday_ && leftToday_ == other.leftToday_ &&
           arrivedEver_ == other.arrivedEver_;
}

void Visitors::Step(Env& env, const Animals& animals) {
    if (!scratch_) {
        scratch_ = std::make_unique<Scratch>();
    }
    Stepper(*this, env, animals).Run();
}

// ---- a tick -------------------------------------------------------------------------------

namespace visitors_detail {

Stepper::Stepper(Visitors& visitors, Env& environment, const Animals& animals)
    : all(visitors),
      env(environment),
      fauna(animals),
      scr(*visitors.scratch_),
      now(environment.time.tick),
      hour(environment.time.Hour()) {}

Group Stepper::GroupAt(std::size_t first) const {
    const std::vector<Visitor>& list = all.visitors_;
    std::size_t end = first + 1;
    while (end < list.size() && list[end].group == list[first].group) {
        ++end;
    }
    return {first, end};
}

float Stepper::AttractionNow() {
    if (scr.attractionTick != now) {
        scr.attraction = AttractionOf(env, fauna);
        scr.attractionTick = now;
    }
    return scr.attraction;
}

void Stepper::Run() {
    Sites& sites = scr.sites;
    const bool known = sites.valid;
    const std::uint32_t nav = sites.nav;
    sites.Refresh(env);
    if (!known || nav != sites.nav) {
        // A removed path may leave wrappers behind before its ground is reserved.
        // Reservations exclude old litter as well as new drops from viewing rails.
        const auto removed = std::erase_if(all.litter_, [this](const Litter& litter) {
            return env.enclosures.RegionAt(World::TileAt(litter.position)) == Region::StandOff;
        });
        if (removed > 0) {
            ++scr.litterEdits;
        }
    }
    if (known && nav != sites.nav) {
        // Paths, fences or buildings changed: the routes may lead through a wall now.
        for (Visitor& v : all.visitors_) {
            if (v.leader) {
                v.reroute = true;
            }
        }
    }
    if (now % kTicksPerSecond == 0) {
        Arrivals();
    }
    BuildFrame();
    scr.budget = kPathSearchesPerTick;
    for (std::size_t i = 0; i < all.visitors_.size();) {
        const Group g = GroupAt(i);
        StepGroup(g);
        i = g.end;
    }
    if (std::find(scr.gone.begin(), scr.gone.end(), std::uint8_t{1}) != scr.gone.end()) {
        std::size_t write = 0;
        for (std::size_t read = 0; read < all.visitors_.size(); ++read) {
            if (scr.gone[read] == 0) {
                if (write != read) {
                    all.visitors_[write] = std::move(all.visitors_[read]);
                }
                ++write;
            }
        }
        all.visitors_.resize(write);
    }
}

void Stepper::BuildFrame() {
    const Sites& sites = scr.sites;
    const std::vector<Visitor>& list = all.visitors_;
    const std::size_t tiles = static_cast<std::size_t>(sites.width) * static_cast<std::size_t>(sites.depth);
    scr.crowd.assign(tiles, 0);
    scr.watchers.assign(tiles, 0);
    scr.insideCount.assign(sites.services.size(), 0);
    scr.queuePeople.assign(sites.services.size(), 0);
    scr.queue.clear();
    scr.ahead.assign(list.size(), 0);
    scr.rank.assign(list.size(), 0);
    scr.gone.assign(list.size(), 0);
    const auto bump = [](std::uint8_t& cell) {
        if (cell < 255) {
            ++cell;
        }
    };
    // Who is on which tile (people inside buildings are nowhere): a chain a tile, in the
    // order of the list. Only the tiles somebody stood on are swept, not the whole map.
    if (scr.cellHead.size() != tiles) {
        scr.cellHead.assign(tiles, -1);
        scr.cellUsed.clear();
    }
    for (const std::uint32_t tile : scr.cellUsed) {
        scr.cellHead[tile] = -1;
    }
    scr.cellUsed.clear();
    scr.cellNext.resize(list.size());
    for (std::size_t i = list.size(); i-- > 0;) {
        const TileCoord t = World::TileAt(list[i].position);
        if (list[i].activity == VisitorActivity::Inside || !sites.InBounds(t)) {
            continue;
        }
        const std::size_t tile = sites.TileIndex(t);
        if (scr.cellHead[tile] < 0) {
            scr.cellUsed.push_back(static_cast<std::uint32_t>(tile));
        }
        scr.cellNext[i] = scr.cellHead[tile];
        scr.cellHead[tile] = static_cast<std::int32_t>(i);
    }
    int service = -1;  // of the group being walked through
    for (std::size_t i = 0; i < list.size(); ++i) {
        const Visitor& v = list[i];
        if (v.activity != VisitorActivity::Inside) {
            const TileCoord t = World::TileAt(v.position);
            if (sites.InBounds(t)) {
                bump(scr.crowd[sites.TileIndex(t)]);
            }
        }
        if (v.goal == VisitorGoal::Watch) {
            const TileCoord t = World::TileAt(v.target);
            if (sites.InBounds(t)) {
                bump(scr.watchers[sites.TileIndex(t)]);
            }
        }
        if (v.leader) {
            service = v.targetBuilding ? sites.ServiceIndex(v.targetBuilding) : -1;
            if (service >= 0 && (v.goal == VisitorGoal::Food || v.goal == VisitorGoal::Drink) &&
                v.activity == VisitorActivity::Queueing) {
                scr.queue.push_back({service, v.queuedTick, v.id.value, i, 0});
            }
        }
        if (service < 0) {
            continue;
        }
        const std::size_t s = static_cast<std::size_t>(service);
        if (v.activity == VisitorActivity::Inside) {
            ++scr.insideCount[s];
        } else if (v.goal == VisitorGoal::Food || v.goal == VisitorGoal::Drink) {
            ++scr.queuePeople[s];
            if (!scr.queue.empty() && v.activity == VisitorActivity::Queueing) {
                ++scr.queue.back().size;
            }
        }
    }
    std::sort(scr.queue.begin(), scr.queue.end(), [](const QueueEntry& a, const QueueEntry& b) {
        if (a.service != b.service) {
            return a.service < b.service;
        }
        if (a.tick != b.tick) {
            return a.tick < b.tick;
        }
        return a.id < b.id;
    });
    int people = 0;
    int groups = 0;
    for (std::size_t i = 0; i < scr.queue.size(); ++i) {
        const QueueEntry& entry = scr.queue[i];
        if (i == 0 || scr.queue[i - 1].service != entry.service) {
            people = 0;
            groups = 0;
        }
        scr.ahead[entry.index] = people;
        scr.rank[entry.index] = groups;
        people += entry.size;
        ++groups;
    }

    if (scr.litterRevision != scr.litterEdits || scr.litter.size() != tiles) {
        scr.litter.assign(tiles, 0);
        for (const Litter& l : all.litter_) {
            const TileCoord t = World::TileAt(l.position);
            if (sites.InBounds(t)) {
                bump(scr.litter[sites.TileIndex(t)]);
            }
        }
        scr.litterRevision = scr.litterEdits;
    }

    scr.threatCells.clear();
    for (const Animal& animal : fauna.All()) {
        if (animal.loose && animal.InWorld() && animal.species >= 0 &&
            static_cast<std::size_t>(animal.species) < env.species.All().size() &&
            env.species.All()[static_cast<std::size_t>(animal.species)].dangerous) {
            const TileCoord tile = World::TileAt(animal.position);
            scr.threatCells[ThreatCellKey(tile.x / kThreatCellTiles, tile.z / kThreatCellTiles)].push_back(
                {animal.id, animal.position});
        }
    }
}

void Stepper::Arrivals() {
    if (!scr.sites.open) {
        all.arrivalRemainder_ = 0.0f;
        return;
    }
    all.arrivalRemainder_ +=
        all.pace_.arrivals * all.ExpectedArrivalsPerHour(env, fauna) / kSecondsPerGameHour;
    while (all.arrivalRemainder_ >= 1.0f) {
        // Alone, couples, families and parties of friends.
        const float roll = all.rng_.NextFloat();
        const int size = roll < 0.2f ? 1 : roll < 0.55f ? 2 : roll < 0.75f ? 3 : roll < 0.93f ? 4 : 5;
        if (all.Count() + size > all.pace_.most) {
            all.arrivalRemainder_ = 0.0f;
            break;
        }
        SpawnGroup(size);
        all.arrivalRemainder_ -= static_cast<float>(size);
    }
}

void Stepper::SpawnGroup(int size) {
    Rng& rng = all.rng_;
    const std::uint32_t group = all.nextGroup_++;
    // A family brings children; friends and couples do not.
    int adults = size;
    if (size == 2) {
        adults = rng.Chance(0.2f) ? 1 : 2;
    } else if (size >= 3 && rng.Chance(0.7f)) {
        adults = size == 3 ? rng.RangeInt(1, 2) : 2;
    }
    Money cash = 0;
    const std::size_t first = all.visitors_.size();
    for (int k = 0; k < size; ++k) {
        Visitor v;
        v.id = VisitorId{all.nextId_++};
        v.look = rng.NextU32();
        v.child = k >= adults;
        v.heightM = v.child ? rng.Range(1.05f, 1.45f) : rng.Range(1.56f, 1.92f);
        v.group = group;
        v.leader = k == 0;
        v.position = Cluster(scr.sites.spawn, k);
        v.heading = kPi;  // north, up the road
        v.hunger = rng.Range(0.05f, 0.42f);
        v.thirst = rng.Range(0.05f, 0.42f);
        v.bladder = rng.Range(0.0f, 0.3f);
        v.tiredness = rng.Range(0.0f, 0.15f);
        v.happiness = rng.Range(0.55f, 0.7f);
        v.arrivedTick = now;
        v.activity = VisitorActivity::Arriving;
        cash += v.child ? Dollars(rng.RangeInt(4, 12)) : Dollars(rng.RangeInt(22, 70));
        all.visitors_.push_back(std::move(v));
    }
    all.visitors_[first].cash = cash;
    all.visitors_[first].activityUntil = now;
}

void Stepper::StepGroup(Group g) {
    if ((now + Lead(g).group) % kTicksPerSecond == 0) {
        EverySecond(g);
        if (scr.gone[g.first] != 0) {
            return;
        }
    }
    Visitor& lead = Lead(g);
    if (lead.reroute) {
        if (!Travelling(lead.activity) || lead.goal == VisitorGoal::None) {
            lead.reroute = false;
        } else if (scr.budget > 0) {
            lead.reroute = false;
            if (!RouteGroup(g)) {
                Failed(g);
                return;
            }
        }
    }
    if (lead.goal == VisitorGoal::None) {
        if (now >= lead.activityUntil && scr.budget > 0) {
            Decide(g);
        } else {
            for (std::size_t i = g.first; i < g.end; ++i) {
                List()[i].speed = 0.0f;
            }
        }
    } else if (Travelling(lead.activity)) {
        Move(g);
    } else {
        Act(g);
    }
}

void Stepper::EverySecond(Group g) {
    const Sites& sites = scr.sites;
    std::vector<Visitor>& list = List();
    Visitor& lead = list[g.first];
    // A longer stay is the same visit taken slowly: no more meals than in a short one.
    const float hours = 1.0f / (kSecondsPerGameHour * all.pace_.stay);
    const float heat = 1.0f + 0.04f * std::max(0.0f, env.weather.temperatureC - 22.0f);
    for (std::size_t i = g.first; i < g.end; ++i) {
        Visitor& v = list[i];
        v.hunger = Clamp01(v.hunger + kHungerPerHour * hours);
        v.thirst = Clamp01(v.thirst + kThirstPerHour * heat * hours);
        v.bladder = Clamp01(v.bladder + kBladderPerHour * hours);
        const bool resting = v.activity == VisitorActivity::Sitting || v.activity == VisitorActivity::Inside;
        if (!resting) {
            v.tiredness = Clamp01(v.tiredness + kTirednessPerHour * (v.child ? 1.2f : 1.0f) * hours);
        }
        float loss = 0.0f;
        for (const float need : {v.hunger, v.thirst, v.bladder, v.tiredness}) {
            if (need > kSuffering) {
                loss += kSufferingPerSecond;
            }
        }
        v.happiness = Clamp01(v.happiness - loss);
    }
    if (!lead.ticketPaid) {
        return;
    }

    // What it is like where they are.
    const bool indoors = lead.activity == VisitorActivity::Inside;
    const TileCoord tile = World::TileAt(lead.position);
    if (!indoors && sites.InBounds(tile)) {
        float mood = 0.0015f * std::min(sites.beauty[sites.TileIndex(tile)], 1.0f);
        int litter = 0;
        for (int dz = -1; dz <= 1; ++dz) {
            for (int dx = -1; dx <= 1; ++dx) {
                const TileCoord t{tile.x + dx, tile.z + dz};
                if (sites.InBounds(t)) {
                    litter += scr.litter[sites.TileIndex(t)];
                }
            }
        }
        if (litter >= 3) {
            mood -= 0.002f;
            Think(g, VisitorThought::Litter);
        }
        if (scr.crowd[sites.TileIndex(tile)] >= 10) {
            mood -= 0.001f;
        }
        if (env.weather.rain > 0.3f) {
            mood -= 0.001f * env.weather.rain;
        }
        Cheer(g, mood);
    }

    if (lead.goal == VisitorGoal::Flee) {
        return;
    }
    if (!indoors) {
        const int cellX = tile.x / kThreatCellTiles;
        const int cellZ = tile.z / kThreatCellTiles;
        const Threat* nearby = nullptr;
        for (int dz = -1; dz <= 1; ++dz) {
            for (int dx = -1; dx <= 1; ++dx) {
                const auto cell = scr.threatCells.find(ThreatCellKey(cellX + dx, cellZ + dz));
                if (cell == scr.threatCells.end())
                    continue;
                for (const Threat& threat : cell->second) {
                    if (DistanceSquared(threat.position, lead.position) <= kThreatM * kThreatM &&
                        (nearby == nullptr || threat.animal < nearby->animal)) {
                        nearby = &threat;
                    }
                }
            }
        }
        if (nearby != nullptr) {
            if (scr.budget <= 0) {
                return;  // next second, then
            }
            Think(g, VisitorThought::Scared);
            Cheer(g, -0.3f);
            Event event;
            event.type = EventType::VisitorScared;
            event.visitor = lead.id;
            event.animal = nearby->animal;
            event.position = lead.position;
            env.Emit(event);
            if (!PlanLeave(g, VisitorGoal::Flee)) {
                Depart(g, false);
            }
            return;
        }
    }
    if (Closing() && lead.goal != VisitorGoal::Leave && lead.goal != VisitorGoal::None) {
        // Closing time: whoever is not being served right now makes for the gate.
        const bool busy = indoors || (lead.activity == VisitorActivity::Queueing && lead.served);
        if (!busy && scr.budget > 0 && !PlanLeave(g, VisitorGoal::Leave)) {
            Failed(g);
        }
    }
}

void Stepper::Depart(Group g, bool donate) {
    std::vector<Visitor>& list = List();
    Visitor& lead = list[g.first];
    if (lead.ticketPaid) {
        float happiness = 0.0f;
        for (std::size_t i = g.first; i < g.end; ++i) {
            happiness += list[i].happiness;
        }
        happiness /= static_cast<float>(g.Size());
        if (donate && happiness >= 0.75f && lead.cash >= Dollars(2) && all.rng_.Chance(0.25f)) {
            const Money amount = std::min<Money>(lead.cash, Dollars(all.rng_.RangeInt(2, 12)));
            lead.cash -= amount;
            lead.spent += amount;
            if (env.finance != nullptr) {
                env.finance->Earn(Account::Donations, amount);
            }
            Event event;
            event.type = EventType::VisitorDonated;
            event.visitor = lead.id;
            event.position = lead.position;
            event.detail = static_cast<int>(amount);
            env.Emit(event);
        }
        for (std::size_t i = g.first; i < g.end; ++i) {
            const Visitor& v = list[i];
            all.leftWeight_ = std::min(all.leftWeight_ + 1.0f, kRatingMemory);
            all.rating_ += (v.happiness - all.rating_) / all.leftWeight_;
            all.educationRating_ += (v.education - all.educationRating_) / all.leftWeight_;
            ++all.leftToday_;
            Event event;
            event.type = EventType::VisitorLeft;
            event.visitor = v.id;
            event.position = v.position;
            event.detail = static_cast<int>(std::lround(v.happiness * 100.0f));
            env.Emit(event);
        }
    }
    for (std::size_t i = g.first; i < g.end; ++i) {
        scr.gone[i] = 1;
    }
}

}  // namespace visitors_detail

}  // namespace zoo
