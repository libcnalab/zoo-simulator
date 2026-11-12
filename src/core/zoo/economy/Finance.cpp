// SPDX-License-Identifier: MIT
#include "zoo/economy/Finance.hpp"

#include <algorithm>
#include <cassert>
#include <limits>

namespace zoo {
namespace {

std::size_t AccountIndex(Account account) {
    return static_cast<std::size_t>(account);
}

}  // namespace

std::string_view ToString(Account account) {
    constexpr std::array<std::string_view, kAccountCount> names = {
        "tickets", "food_sales",   "gift_sales",  "donations",        "grants",       "animal_sales",
        "refunds", "construction", "landscaping", "animal_purchases", "animal_food",  "wages",
        "upkeep",  "veterinary",   "goods",       "conservation",     "loan_interest"};
    const std::size_t index = AccountIndex(account);
    return index < names.size() ? names[index] : std::string_view{};
}

std::string_view DisplayName(Account account) {
    constexpr std::array<std::string_view, kAccountCount> names = {
        "Tickets", "Food sales",   "Gift sales",  "Donations",        "Grants",       "Animal sales",
        "Refunds", "Construction", "Landscaping", "Animal purchases", "Animal food",  "Wages",
        "Upkeep",  "Veterinary",   "Goods",       "Conservation",     "Loan interest"};
    const std::size_t index = AccountIndex(account);
    return index < names.size() ? names[index] : std::string_view{};
}

Money DayReport::Income() const {
    Money total = 0;
    for (int i = 0; i <= static_cast<int>(Account::Refunds); ++i) {
        total += amounts[static_cast<std::size_t>(i)];
    }
    return total;
}

Money DayReport::Expenses() const {
    Money total = 0;
    for (int i = static_cast<int>(Account::Construction); i < kAccountCount; ++i) {
        total += amounts[static_cast<std::size_t>(i)];
    }
    return total;
}

Finance::Finance(Money startingBalance) : balance_(startingBalance) {}

void Finance::Earn(Account account, Money amount) {
    assert(amount >= 0);
    assert(IsIncome(account));
    if (amount < 0 || !IsIncome(account)) {
        return;
    }
    balance_ += amount;
    today_.amounts[AccountIndex(account)] += amount;
    lifetime_[AccountIndex(account)] += amount;
}

void Finance::Pay(Account account, Money amount) {
    assert(amount >= 0);
    assert(!IsIncome(account) && AccountIndex(account) < static_cast<std::size_t>(kAccountCount));
    if (amount < 0 || IsIncome(account) || AccountIndex(account) >= static_cast<std::size_t>(kAccountCount)) {
        return;
    }
    balance_ -= amount;
    today_.amounts[AccountIndex(account)] += amount;
    lifetime_[AccountIndex(account)] += amount;
}

bool Finance::TrySpend(Account account, Money amount) {
    assert(amount >= 0);
    assert(!IsIncome(account));
    if (amount < 0 || IsIncome(account) || !CanAfford(amount)) {
        return false;
    }
    Pay(account, amount);
    return true;
}

bool Finance::TakeLoan(Money amount) {
    if (amount <= 0 || amount % kLoanStep != 0 || amount > kLoanLimit - loan_) {
        return false;
    }
    loan_ += amount;
    balance_ += amount;
    return true;
}

Money Finance::RepayLoan(Money amount) {
    if (amount <= 0) {
        return 0;
    }
    const Money repayment = std::min({amount, loan_, std::max<Money>(balance_, 0)});
    balance_ -= repayment;
    loan_ -= repayment;
    return repayment;
}

const DayReport& Finance::CloseDay(std::int64_t day) {
    if (loan_ > 0) {
        const Money interest = (loan_ * kLoanInterestBasisPointsPerDay + 9999) / 10000;
        Pay(Account::LoanInterest, interest);
    }
    today_.day = day;
    today_.balance = balance_;
    today_.loan = loan_;
    history_.push_back(today_);
    if (history_.size() > static_cast<std::size_t>(kHistoryDays)) {
        history_.erase(history_.begin());
    }
    today_ = {};
    return history_.back();
}

DayReport Finance::Summary(int days) const {
    DayReport result;
    if (days <= 0 || history_.empty()) {
        return result;
    }
    const std::size_t count = std::min(static_cast<std::size_t>(days), history_.size());
    const std::size_t first = history_.size() - count;
    for (std::size_t i = first; i < history_.size(); ++i) {
        const DayReport& report = history_[i];
        result.day = report.day;
        result.balance = report.balance;
        result.loan = report.loan;
        result.visitors += report.visitors;
        for (std::size_t account = 0; account < result.amounts.size(); ++account) {
            result.amounts[account] += report.amounts[account];
        }
    }
    return result;
}

}  // namespace zoo
