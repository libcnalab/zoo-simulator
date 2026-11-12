// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>

#include "zoo/base/Ids.hpp"
#include "zoo/base/Math.hpp"

namespace zoo {

/// Things that happened during a step which someone outside the subsystem may
/// care about: the notification list, the sound engine, statistics. Events are
/// output only: no simulation code reads them back, so they are not saved.
/// Append only (replays and tests compare them by value).
enum class EventType : std::uint8_t {
    // ---- animals (Event::animal set; `other`, `enclosure`, `detail` as noted) ----
    AnimalBorn,        ///< other = mother
    AnimalDied,        ///< detail = DeathCause
    AnimalEscaped,     ///< enclosure = the one it left (may be null)
    AnimalRecaptured,  ///< enclosure = where it was put (null: crated)
    AnimalSick,
    AnimalRecovered,
    AnimalStarving,  ///< hunger became critical
    AnimalThirsty,   ///< thirst became critical
    AnimalFight,     ///< other = the opponent
    AnimalKilled,    ///< animal = the predator, other = the victim (which also gets AnimalDied)
    AnimalPregnant,  ///< other = the father
    AnimalCalled,    ///< vocalised (the sound engine plays the species' call at `position`)
    AnimalUnhappy,   ///< welfare fell below the alert threshold (once per decline)

    // ---- visitors (Event::visitor set; `position` where it happened) ----
    VisitorArrived,   ///< paid for a ticket; detail = the price in cents
    VisitorLeft,      ///< detail = happiness on leaving, 0..100
    VisitorBought,    ///< building = the stall or shop; detail = the price in cents
    VisitorDonated,   ///< detail = the amount in cents
    VisitorLittered,  ///< dropped litter at `position`
    VisitorScared,    ///< animal = the loose animal it runs from
    VisitorThought,   ///< detail = VisitorThought; a new thought worth a speech bubble
};

struct Event {
    EventType type = EventType::AnimalBorn;
    AnimalId animal;
    AnimalId other;
    VisitorId visitor;
    StaffId staff;
    EnclosureId enclosure;
    BuildingId building;
    PropId prop;
    Vec2 position;
    int detail = 0;

    bool operator==(const Event&) const = default;
};

}  // namespace zoo
