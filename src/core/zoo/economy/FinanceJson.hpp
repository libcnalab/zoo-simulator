// SPDX-License-Identifier: MIT
#pragma once

#include "zoo/economy/Finance.hpp"

#include <nlohmann/json.hpp>

#include <optional>
#include <string>

namespace zoo {

nlohmann::json ToJson(const Finance& finance);
std::optional<Finance> FinanceFromJson(const nlohmann::json& json, std::string* error = nullptr);

struct FinanceAccess {
    static Money& Balance(Finance& finance) { return finance.balance_; }
    static Money& Loan(Finance& finance) { return finance.loan_; }
    static DayReport& Today(Finance& finance) { return finance.today_; }
    static std::vector<DayReport>& History(Finance& finance) { return finance.history_; }
    static std::array<Money, kAccountCount>& Lifetime(Finance& finance) { return finance.lifetime_; }
};

}  // namespace zoo
