// SPDX-License-Identifier: MIT
#pragma once

#include <array>
#include <cstdint>
#include <string_view>
#include <vector>

#include "zoo/base/Money.hpp"

namespace zoo {

/// What money was earned from or spent on. Names are part of the save format;
/// append only (before Count).
enum class Account : std::uint8_t {
    // income
    Tickets = 0,
    FoodSales,
    GiftSales,
    Donations,
    Grants,             ///< conservation and education grants
    AnimalSales,
    Refunds,            ///< money back for demolished construction
    // expenses
    Construction,
    Landscaping,        ///< terrain, water, plants
    AnimalPurchases,
    AnimalFood,
    Wages,
    Upkeep,             ///< buildings and fences
    Veterinary,
    Goods,              ///< cost of what the stalls sell
    Conservation,       ///< release programmes, research
    LoanInterest,
    Count
};

inline constexpr int kAccountCount = static_cast<int>(Account::Count);

/// "tickets", "food_sales", ... (save format).
std::string_view ToString(Account a);
/// "Tickets", "Food sales", ... (UI).
std::string_view DisplayName(Account a);
constexpr bool IsIncome(Account a) { return a <= Account::Refunds; }

/// One closed day of bookkeeping.
struct DayReport {
    std::int64_t day = 0;                           ///< GameTime::Day() of the day that ended
    std::array<Money, kAccountCount> amounts{};     ///< always >= 0; income or expense by account
    Money balance = 0;                              ///< at the end of the day
    Money loan = 0;                                 ///< outstanding at the end of the day
    int visitors = 0;                               ///< tickets sold that day

    Money Income() const;
    Money Expenses() const;
    Money Profit() const { return Income() - Expenses(); }
    bool operator==(const DayReport&) const = default;
};

/// The zoo's books: a balance, the running day's totals by account, the history
/// of closed days and one loan.
///
/// The balance may go negative through running costs (wages are paid whether
/// or not there is money); voluntary spending goes through TrySpend(), which
/// refuses what the zoo cannot afford.
class Finance {
public:
    static constexpr int kHistoryDays = 96;                 ///< two game years of day reports are kept
    static constexpr Money kLoanStep = Dollars(5000);
    static constexpr Money kLoanLimit = Dollars(100000);
    /// Interest per game day on the outstanding loan, in basis points (1/100 %).
    static constexpr int kLoanInterestBasisPointsPerDay = 25;

    explicit Finance(Money startingBalance = Dollars(50000));

    Money Balance() const { return balance_; }
    Money Loan() const { return loan_; }
    bool CanAfford(Money amount) const { return amount <= balance_; }

    /// `amount` >= 0; `a` must be an income account.
    void Earn(Account a, Money amount);
    /// Unconditional payment (running costs); `a` must be an expense account, `amount` >= 0.
    void Pay(Account a, Money amount);
    /// Pays only if the balance covers it; returns whether it paid.
    bool TrySpend(Account a, Money amount);
    void CountVisitor(int n = 1) { today_.visitors += n; }

    /// Borrows `amount` (a positive multiple of kLoanStep) if the limit allows; the
    /// money is added to the balance but is not income.
    bool TakeLoan(Money amount);
    /// Repays up to `amount`, limited by the loan and by the balance; returns what was repaid.
    Money RepayLoan(Money amount);

    /// Closes the running day: charges the loan interest (rounded up to a cent,
    /// account LoanInterest), stamps the report with `day` and appends it to the
    /// history (dropping the oldest beyond kHistoryDays). Returns the closed report.
    const DayReport& CloseDay(std::int64_t day);

    /// The day in progress (balance and loan fields are not maintained in it).
    const DayReport& Today() const { return today_; }
    /// Closed days, oldest first.
    const std::vector<DayReport>& History() const { return history_; }
    /// Sum of the last `days` closed days (fewer if the history is shorter); `day`
    /// is that of the newest report included, balance and loan are its values.
    DayReport Summary(int days) const;
    /// Totals since the game began (income and expenses by account).
    const std::array<Money, kAccountCount>& Lifetime() const { return lifetime_; }

    bool operator==(const Finance&) const = default;

private:
    friend struct FinanceAccess;        // serialisation

    Money balance_ = 0;
    Money loan_ = 0;
    DayReport today_;
    std::vector<DayReport> history_;
    std::array<Money, kAccountCount> lifetime_{};
};

}  // namespace zoo
