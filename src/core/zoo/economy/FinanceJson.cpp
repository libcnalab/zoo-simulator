// SPDX-License-Identifier: MIT
#include "zoo/economy/FinanceJson.hpp"

#include <limits>
#include <stdexcept>

namespace zoo {
namespace {

using Json = nlohmann::json;

Json AmountsToJson(const std::array<Money, kAccountCount>& amounts) {
    Json result = Json::object();
    for (int i = 0; i < kAccountCount; ++i) {
        const auto account = static_cast<Account>(i);
        const Money amount = amounts[static_cast<std::size_t>(i)];
        if (amount != 0) {
            result[std::string(ToString(account))] = amount;
        }
    }
    return result;
}

std::array<Money, kAccountCount> AmountsFromJson(const Json& value) {
    if (!value.is_object()) {
        throw std::runtime_error("accounts must be an object");
    }
    std::array<Money, kAccountCount> amounts{};
    for (auto it = value.begin(); it != value.end(); ++it) {
        int found = -1;
        for (int i = 0; i < kAccountCount; ++i) {
            if (it.key() == ToString(static_cast<Account>(i))) {
                found = i;
                break;
            }
        }
        if (found < 0) {
            throw std::runtime_error("unknown account name: " + it.key());
        }
        if (!it.value().is_number_integer()) {
            throw std::runtime_error("account amount must be integer cents");
        }
        const Money amount = it.value().get<Money>();
        if (amount < 0) {
            throw std::runtime_error("account amount cannot be negative");
        }
        amounts[static_cast<std::size_t>(found)] = amount;
    }
    return amounts;
}

Json ReportToJson(const DayReport& report) {
    return Json{{"day", report.day},
                {"accounts", AmountsToJson(report.amounts)},
                {"balance", report.balance},
                {"loan", report.loan},
                {"visitors", report.visitors}};
}

DayReport ReportFromJson(const Json& value) {
    if (!value.is_object() || !value.contains("day") || !value["day"].is_number_integer() ||
        !value.contains("accounts") || !value.contains("balance") || !value["balance"].is_number_integer() ||
        !value.contains("loan") || !value["loan"].is_number_integer() || !value.contains("visitors") ||
        !value["visitors"].is_number_integer()) {
        throw std::runtime_error("day report is missing a valid field");
    }
    DayReport report;
    report.day = value["day"].get<std::int64_t>();
    report.amounts = AmountsFromJson(value["accounts"]);
    report.balance = value["balance"].get<Money>();
    report.loan = value["loan"].get<Money>();
    report.visitors = value["visitors"].get<int>();
    if (report.loan < 0 || report.visitors < 0) {
        throw std::runtime_error("day report loan and visitors cannot be negative");
    }
    return report;
}

}  // namespace

nlohmann::json ToJson(const Finance& finance) {
    Json history = Json::array();
    for (const DayReport& report : finance.History()) {
        history.push_back(ReportToJson(report));
    }
    return Json{{"balance", finance.Balance()},
                {"loan", finance.Loan()},
                {"today", ReportToJson(finance.Today())},
                {"history", std::move(history)},
                {"lifetime", AmountsToJson(finance.Lifetime())}};
}

std::optional<Finance> FinanceFromJson(const nlohmann::json& json, std::string* error) {
    if (error != nullptr) {
        error->clear();
    }
    try {
        if (!json.is_object() || !json.contains("balance") || !json["balance"].is_number_integer() ||
            !json.contains("loan") || !json["loan"].is_number_integer() || !json.contains("today") ||
            !json.contains("history") || !json["history"].is_array() || !json.contains("lifetime")) {
            throw std::runtime_error("finance document is missing a valid field");
        }
        const Money balance = json["balance"].get<Money>();
        const Money loan = json["loan"].get<Money>();
        if (loan < 0) {
            throw std::runtime_error("loan cannot be negative");
        }
        const Json& historyJson = json["history"];
        if (historyJson.size() > static_cast<std::size_t>(Finance::kHistoryDays)) {
            throw std::runtime_error("history exceeds the maximum number of days");
        }
        Finance finance(balance);
        FinanceAccess::Loan(finance) = loan;
        FinanceAccess::Today(finance) = ReportFromJson(json["today"]);
        FinanceAccess::Lifetime(finance) = AmountsFromJson(json["lifetime"]);
        std::int64_t previousDay = std::numeric_limits<std::int64_t>::min();
        for (const Json& reportJson : historyJson) {
            DayReport report = ReportFromJson(reportJson);
            if (report.day <= previousDay) {
                throw std::runtime_error("history days must be strictly ascending");
            }
            previousDay = report.day;
            FinanceAccess::History(finance).push_back(std::move(report));
        }
        return finance;
    } catch (const std::exception& exception) {
        if (error != nullptr) {
            *error = exception.what();
        }
        return std::nullopt;
    }
}

}  // namespace zoo
