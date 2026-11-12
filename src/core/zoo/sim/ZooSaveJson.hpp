// SPDX-License-Identifier: MIT
#pragma once

#include <memory>
#include <nlohmann/json.hpp>
#include <string>

#include "zoo/sim/ZooSave.hpp"

namespace zoo {

nlohmann::json ZooToJson(const Zoo& zoo);
/// nullptr and a message in `error` for anything that is not a readable save.
std::unique_ptr<Zoo> ZooFromJson(const nlohmann::json& json, const Catalog& catalog,
                                 const SpeciesCatalog& species, std::string* error = nullptr);
bool SummaryFromJson(const nlohmann::json& json, SaveSummary& out, std::string* error = nullptr);

}  // namespace zoo
