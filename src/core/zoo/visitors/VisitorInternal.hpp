// SPDX-License-Identifier: MIT
#pragma once

// Shared by the parts of the visitor simulation (Visitors.cpp, VisitorSites.cpp,
// VisitorPlans.cpp, VisitorActions.cpp); not for use outside them.

#include <array>
#include <cstdint>
#include <utility>
#include <vector>

#include "zoo/visitors/VisitorTuning.hpp"
#include "zoo/visitors/Visitors.hpp"

namespace zoo::visitors_detail {

inline constexpr NavProfile kWalker = NavProfile::Visitor();

/// An enclosure visitors can watch: its viewing places that can be walked to from the gate.
struct ViewSite {
    EnclosureId enclosure;
    std::vector<ViewingSpot> spots;
    Vec2 centre;              ///< mean of the viewing places
    float visibility = 0.0f;  ///< the best of them
};

/// A building visitors use: a stall, the restroom, the shop, the education centre.
struct ServiceSite {
    BuildingId id;
    int def = -1;
    BuildingRole role = BuildingRole::FoodStall;
    Vec2 door;          ///< centre of the tile in front of the door
    float face = 0.0f;  ///< heading that looks at the door from there
    int counters = 1;   ///< groups a stall serves at a time
    /// Where the queue stands, from the counter outwards (stalls).
    std::vector<Vec2> line;
};

/// A bench or a picnic table.
struct SeatSite {
    PropId id;
    Vec2 position;
    float heading = 0.0f;
    int seats = 0;
    Vec2 access;  ///< the centre of the path tile it is reached from
};

/// A bin or an information board.
struct PropSite {
    PropId id;
    Vec2 position;
    Vec2 access;
    float capacity = 0.0f;
};

/// What the map offers visitors: derived from the world, the objects and the
/// enclosures, rebuilt when one of them changes.
struct Sites {
    bool valid = false;
    std::uint32_t nav = 0;
    std::uint32_t fences = 0;
    std::uint32_t objects = 0;
    std::uint32_t enclosures = 0;
    int width = 0;
    int depth = 0;
    /// The gate can be walked to from the end of the approach road.
    bool open = false;
    Vec2 spawn;
    TileCoord entranceTile;
    std::vector<ViewSite> views;        ///< ascending enclosure id
    std::vector<ServiceSite> services;  ///< ascending building id
    std::vector<SeatSite> benches;
    std::vector<PropSite> bins;
    std::vector<PropSite> boards;
    std::vector<float> beauty;  ///< per tile: what the decorations around it add

    void Refresh(const Env& env);
    int ServiceIndex(BuildingId id) const;
    int ViewIndex(EnclosureId id) const;
    std::size_t TileIndex(TileCoord t) const {
        return static_cast<std::size_t>(t.z) * static_cast<std::size_t>(width) +
               static_cast<std::size_t>(t.x);
    }
    bool InBounds(TileCoord t) const { return t.x >= 0 && t.z >= 0 && t.x < width && t.z < depth; }
};

struct QueueEntry {
    int service = 0;
    std::int64_t tick = 0;
    std::uint32_t id = 0;
    std::size_t index = 0;  ///< of the leader in Visitors::All()
    int size = 0;
};

struct Threat {
    AnimalId animal;
    Vec2 position;
};

/// Everything derived: the sites, and what one tick works with. Never saved.
struct Scratch {
    Sites sites;
    std::vector<std::uint8_t> crowd;     ///< per tile: visitors standing or walking on it
    std::vector<std::uint8_t> watchers;  ///< per tile: visitors at or bound for a place at a fence
    std::vector<std::uint8_t> litter;    ///< per tile: pieces of litter
    std::uint32_t litterRevision = 0;    ///< of `litter`
    std::uint32_t litterEdits = 1;       ///< bumped whenever Visitors::litter_ changes
    std::vector<int> insideCount;        ///< per service: visitors inside
    std::vector<int> queuePeople;        ///< per service: visitors in the line
    std::vector<QueueEntry> queue;
    std::vector<int> ahead;     ///< per visitor index (leaders in a line): people in front
    std::vector<int> rank;      ///< per visitor index (leaders in a line): groups in front
    std::vector<float> appeal;  ///< per view site: what its animals are worth watching now
    std::vector<int> counts;
    std::int64_t appealTick = -1;
    float attraction = 0.0f;  ///< of the zoo, this tick
    std::int64_t attractionTick = -1;
    std::vector<Vec2> beasts;     ///< where the animals of the enclosure in question are
    std::vector<Threat> threats;  ///< dangerous animals on the loose
    std::vector<std::uint8_t> gone;
    /// Who stands on which tile as the tick begins (walkers look there for the people in
    /// their way): cellHead[tile] is the first of them, cellNext[visitor] the one after,
    /// indices into Visitors::All() in ascending order, -1 at the end. cellUsed lists the
    /// tiles that have somebody.
    std::vector<std::int32_t> cellHead;
    std::vector<std::int32_t> cellNext;
    std::vector<std::uint32_t> cellUsed;
    std::vector<Vec2> points;
    std::vector<Vec2> spots;  ///< where the people around a plan's places stand or are bound for
    std::vector<Vec2> path;
    std::vector<std::pair<float, int>> ranked;  ///< candidates of a decision, best first
    int budget = 0;                             ///< route searches left this tick
};

/// The members of a group: [first, end) in Visitors::All(), the leader first.
struct Group {
    std::size_t first = 0;
    std::size_t end = 0;
    int Size() const { return static_cast<int>(end - first); }
};

/// What a group is to do next and where each member is to stand.
struct Plan {
    VisitorGoal goal = VisitorGoal::None;
    EnclosureId enclosure;
    BuildingId building;
    PropId prop;
    std::array<Vec2, Visitors::kMaxGroupSize> place{};
    std::array<float, Visitors::kMaxGroupSize> face{};
    std::array<std::uint8_t, Visitors::kMaxGroupSize> slot{};
    std::array<bool, Visitors::kMaxGroupSize> seated{};
};

constexpr bool Travelling(VisitorActivity a) {
    return a == VisitorActivity::Arriving || a == VisitorActivity::Walking || a == VisitorActivity::Leaving ||
           a == VisitorActivity::Fleeing;
}

/// A fraction in [0, 1) that belongs to a visitor's looks and a purpose.
constexpr float Hash01(std::uint32_t look, std::uint32_t salt) {
    return static_cast<float>(HashU32(look, salt) >> 8u) * (1.0f / 16777216.0f);
}

constexpr Vec2 ToVec(TileCoord step) {
    return {static_cast<float>(step.x), static_cast<float>(step.z)};
}

/// Standing slots are measured from the public path edge, including across strips.
inline Vec2 ViewingPosition(const ViewingSpot& spot, int slot) {
    constexpr std::array<int, 4> columns{1, 2, 0, 3};
    const Vec2 toward = ToVec(Step(spot.side));
    const Vec2 lateral{toward.z, -toward.x};
    const int column = columns[static_cast<std::size_t>(slot % 4)];
    return World::TileCenter(spot.tile) + lateral * ((static_cast<float>(column) - 1.5f) * 0.5f) +
           toward * (0.66f - 0.5f * static_cast<float>(slot / 4));
}

/// One tick of the visitor simulation: the Visitors, what they step in, and the
/// tick's working data. The methods are spread over the .cpp files by subject.
struct Stepper {
    Visitors& all;
    Env& env;
    const Animals& fauna;
    Scratch& scr;
    std::int64_t now;
    float hour;

    Stepper(Visitors& visitors, Env& environment, const Animals& animals);

    std::vector<Visitor>& List() { return all.visitors_; }
    Visitor& Lead(Group g) { return all.visitors_[g.first]; }
    Group GroupAt(std::size_t first) const;
    bool Closing() const {
        return hour >= visitor_tuning::kClosingHour || hour < visitor_tuning::kOpeningHour - 1.0f;
    }

    // ---- Visitors.cpp: the tick ---------------------------------------------------------
    void Run();
    void BuildFrame();
    void Arrivals();
    void SpawnGroup(int size);
    void StepGroup(Group g);
    void EverySecond(Group g);
    void Depart(Group g, bool donate);

    // ---- VisitorActions.cpp: carrying a plan out ----------------------------------------
    void Move(Group g);
    /// How hard the people in the way of member `self`, walking along `way`, push it to its
    /// right ({-way.z, way.x}), -1..1; `slow` is lowered when it is about to walk into one.
    float Dodge(std::size_t self, Group g, Vec2 way, float& slow) const;
    /// A step from `from` to `to` crosses no fence and enters no building, water or planting.
    bool Clear(Vec2 from, Vec2 to) const;
    /// On the stand-off planting before a barrier, which is nobody's ground.
    bool InPlanting(Vec2 position) const;
    /// Watchers turn to the animals they look at.
    void Aim(Group g);
    void GroupArrived(Group g);
    void Act(Group g);
    void ActQueue(Group g);
    void ActBuilding(Group g);
    void Purchase(Group g, const ServiceSite& site, Building& building);
    void Watched(Group g);
    void PayTickets(Group g);
    /// The group is done with what it was doing and stands where it is, free to decide.
    void Finish(Group g);
    void Think(Group g, VisitorThought thought);
    void Cheer(Group g, float amount);
    void DropLitter(Vec2 position);

    // ---- VisitorPlans.cpp: deciding -----------------------------------------------------
    void Decide(Group g);
    bool TryWatch(Group g);
    bool TryService(Group g, VisitorGoal goal, float maxDistance, bool complain);
    bool TryRest(Group g, bool complain);
    bool TryBin(Group g);
    bool TryRead(Group g);
    bool PlanEat(Group g, const ServiceSite& site);
    bool PlanLeave(Group g, VisitorGoal goal);
    bool PlanSeats(Group g, Vec2 around, float maxDistance, Plan& plan);
    /// A plan could not be routed: wait a little; after a few in a row the group is lost.
    void Failed(Group g);
    float AttractionNow();
    /// Gives the group the plan and routes it; false (and the group free again) without a way.
    bool Apply(Group g, const Plan& plan);
    void Start(Group g, const Plan& plan);
    bool RouteGroup(Group g);
    bool Route(Vec2 from, Vec2 to, std::vector<Vec2>& out);
    void Lanes(Visitor& member, int place, int size, std::uint32_t leaderLook);
    /// Moves the standing places of a plan off the places other people stand at or are bound for.
    /// With a `focus` they then face it.
    void MakeRoom(Group g, Plan& plan, const Vec2* focus);
    Vec2 Cluster(Vec2 centre, int member) const;
    void EnsureAppeal();
    std::uint32_t TakenSeats(PropId bench) const;
    float Need(Group g, float Visitor::* need);
    float MeanHappiness(Group g);
    void SetActivity(Group g, VisitorActivity activity);
};

Vec2 SeatPosition(const SeatSite& bench, int seat, float& face);

}  // namespace zoo::visitors_detail
