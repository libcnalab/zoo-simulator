// SPDX-License-Identifier: MIT
#pragma once

#include <vector>

#include "zoo/animals/Species.hpp"
#include "zoo/build/Catalog.hpp"
#include "zoo/build/Enclosures.hpp"
#include "zoo/build/Objects.hpp"
#include "zoo/economy/Finance.hpp"
#include "zoo/nav/Nav.hpp"
#include "zoo/sim/Calendar.hpp"
#include "zoo/sim/Events.hpp"
#include "zoo/sim/Weather.hpp"
#include "zoo/world/World.hpp"

namespace zoo {

/// What a subsystem (animals, visitors, staff) sees of the rest of the zoo while
/// it steps: a bundle of references assembled by the Zoo for each call. The
/// subsystems own their entities and their random stream; everything else they
/// reach through this.
struct Env {
    const World& world;
    /// Mutable because feeders are emptied, troughs drunk from, stalls count sales.
    /// Subsystems never add or remove objects (that is the Builder's job).
    ZooObjects& objects;
    const Enclosures& enclosures;
    const Catalog& catalog;
    const SpeciesCatalog& species;
    const Navigator& nav;
    /// Where money is booked; nullptr = everything is free (tests, sandbox mode).
    Finance* finance = nullptr;
    /// Receives the step's events when not null.
    std::vector<Event>* events = nullptr;
    GameTime time;
    WeatherNow weather;
    /// The reversible visit spectacle is active. Derived visitor destinations may keep their
    /// pre-spectacle snapshot while gates are only being opened.
    bool apocalypse = false;

    void Emit(const Event& event) const {
        if (events != nullptr) {
            events->push_back(event);
        }
    }
};

}  // namespace zoo
