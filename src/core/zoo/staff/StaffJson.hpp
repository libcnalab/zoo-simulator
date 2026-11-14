// SPDX-License-Identifier: MIT
#pragma once

#include "zoo/staff/Staff.hpp"

#include <nlohmann/json.hpp>

#include <optional>
#include <string>

namespace zoo {

/// The saved state of the staff (format in docs/KEEPERS.md).
nlohmann::json ToJson(const Staff& staff);
/// Nothing (and `error` set) for a document that is not one of ours or does not hold together.
std::optional<Staff> StaffFromJson(const nlohmann::json& json, std::string* error = nullptr);

struct StaffJsonAccess {
    static nlohmann::json Save(const Staff& staff);
    static Staff Load(const nlohmann::json& json);
};

}  // namespace zoo
