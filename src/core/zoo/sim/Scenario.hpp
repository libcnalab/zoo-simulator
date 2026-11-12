// SPDX-License-Identifier: MIT
#pragma once

#include <string>
#include <vector>

#include "zoo/sim/Zoo.hpp"

namespace zoo {

/// What a scenario script did.
struct ScenarioReport {
    int enclosures = 0;
    int animals = 0;
    int buildings = 0;
    int props = 0;
    /// Things the script wanted and could not have ("no room for the gift shop").
    std::vector<std::string> problems;
};

/// Turns a freshly generated zoo into a small working one, so that a new game
/// starts with something alive in it: paved avenues from the entrance, three
/// furnished enclosures (lions, plains zebras, grey wolves), a few stalls,
/// benches and trees. Nothing is charged; the zoo's money is what the
/// configuration says afterwards.
///
/// Deterministic for a given zoo. Meant for the default map (128 x 128 tiles);
/// on small or very hilly maps parts are skipped and reported as problems.
ScenarioReport BuildStarterZoo(Zoo& zoo);

}  // namespace zoo
