// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "zoo/sim/Scenario.hpp"

namespace zoo {
struct VisitorZooOptions {
    /// Species ids that get no animals (the game passes the species it has no model for).
    /// Their habitats are still built, furnished and named.
    std::vector<std::string> withoutSpecies;
};

/// The world and the settings the Visitor Zoo is designed for; pass it to Zoo's constructor.
ZooConfig VisitorZooConfig(std::uint64_t seed = 1);

/// Builds the complete zoo a visitor walks through. Deterministic for a given zoo.
/// Meant for a zoo made from VisitorZooConfig(); on other maps parts are skipped and
/// reported as problems, as BuildStarterZoo does.
ScenarioReport BuildVisitorZoo(Zoo& zoo, const VisitorZooOptions& options = {});
}  // namespace zoo
