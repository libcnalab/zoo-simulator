// SPDX-License-Identifier: MIT
#pragma once

#include "zoo/visitors/Visitors.hpp"

#include <nlohmann/json.hpp>

#include <optional>
#include <span>
#include <string>

namespace zoo {

/// The saved state of the visitors (format in docs/VISITORS.md).
nlohmann::json ToJson(const Visitors& visitors);
/// Nothing (and `error` set) for a document that is not one of ours or does not hold together.
std::optional<Visitors> VisitorsFromJson(const nlohmann::json& json, std::string* error = nullptr);

struct VisitorsJsonAccess {
    static nlohmann::json Save(const Visitors& visitors);
    static Visitors Load(const nlohmann::json& json);
    static void RemapSeenSpecies(Visitors& visitors, std::span<const std::uint64_t> bits);
};

}  // namespace zoo
