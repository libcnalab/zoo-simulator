// SPDX-License-Identifier: MIT
#pragma once

#include <nlohmann/json.hpp>

#include <string>

#include "zoo/build/Enclosures.hpp"

namespace zoo {

nlohmann::json ToJson(const Enclosures& enclosures);
bool EnclosuresFromJson(const nlohmann::json& json, Enclosures& out, std::string* error = nullptr);

struct EnclosuresAccess {
    static std::uint32_t NextId(const Enclosures& enclosures) { return enclosures.nextId_; }
    static const std::vector<Enclosure>& Values(const Enclosures& enclosures) {
        return enclosures.enclosures_;
    }
    static void Restore(std::uint32_t nextId, std::vector<Enclosure> enclosures, Enclosures& out) {
        out.width_ = 0;
        out.depth_ = 0;
        out.codes_.clear();
        out.labels_.clear();
        out.stack_.clear();
        out.previousCodes_.clear();
        out.nextId_ = nextId;
        out.enclosures_ = std::move(enclosures);
    }
};

}  // namespace zoo
