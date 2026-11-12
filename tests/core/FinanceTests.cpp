// SPDX-License-Identifier: MIT
#include "zoo/economy/FinanceJson.hpp"

#include <gtest/gtest.h>

#include <set>

namespace zoo {
namespace {

TEST(Finance, EarnPayAndTrySpend) {
    Finance finance(Dollars(10));
    finance.Earn(Account::Tickets, Dollars(3));
    finance.Pay(Account::Wages, Dollars(2));
    EXPECT_TRUE(finance.TrySpend(Account::Goods, Dollars(5)));
    EXPECT_FALSE(finance.TrySpend(Account::Goods, Dollars(10)));
    EXPECT_EQ(finance.Balance(), Dollars(6));
    EXPECT_EQ(finance.Today().Income(), Dollars(3));
    EXPECT_EQ(finance.Today().Expenses(), Dollars(7));
}

TEST(Finance, LoansRepaymentInterestAndReports) {
    Finance finance(0);
    EXPECT_TRUE(finance.TakeLoan(Finance::kLoanStep));
    EXPECT_FALSE(finance.TakeLoan(Finance::kLoanStep + 1));
    EXPECT_EQ(finance.RepayLoan(Dollars(6000)), Dollars(5000));
    EXPECT_EQ(finance.Loan(), 0);
    ASSERT_TRUE(finance.TakeLoan(Finance::kLoanStep));
    finance.Pay(Account::Wages, Dollars(4999));
    const DayReport& report = finance.CloseDay(4);
    EXPECT_EQ(report.amounts[static_cast<std::size_t>(Account::LoanInterest)], 1250);
    EXPECT_EQ(report.day, 4);
    EXPECT_EQ(finance.Today(), DayReport{});
    EXPECT_EQ(finance.Lifetime()[static_cast<std::size_t>(Account::LoanInterest)], 1250);
}

TEST(Finance, LoanLimitAndBalanceLimitRepayment) {
    Finance finance(Dollars(10));
    EXPECT_EQ(finance.RepayLoan(Dollars(1)), 0);
    ASSERT_TRUE(finance.TakeLoan(Finance::kLoanLimit));
    EXPECT_FALSE(finance.TakeLoan(Finance::kLoanStep));
    EXPECT_EQ(finance.RepayLoan(Dollars(100)), Dollars(100));
    EXPECT_EQ(finance.Loan(), Finance::kLoanLimit - Dollars(100));
}

TEST(Finance, HistorySummaryAndLifetime) {
    Finance finance(0);
    for (int day = 0; day < Finance::kHistoryDays + 4; ++day) {
        finance.Earn(Account::Tickets, 10);
        finance.CountVisitor();
        finance.CloseDay(day);
    }
    EXPECT_EQ(finance.History().size(), static_cast<std::size_t>(Finance::kHistoryDays));
    EXPECT_EQ(finance.History().front().day, 4);
    const DayReport summary = finance.Summary(3);
    EXPECT_EQ(summary.Income(), 30);
    EXPECT_EQ(summary.visitors, 3);
    EXPECT_EQ(summary.day, Finance::kHistoryDays + 3);
    EXPECT_EQ(finance.Lifetime()[static_cast<std::size_t>(Account::Tickets)],
              (Finance::kHistoryDays + 4) * 10);
}

TEST(Finance, AccountNamesAndMoneyFormatting) {
    std::set<std::string_view> names;
    std::set<std::string_view> labels;
    for (int i = 0; i < kAccountCount; ++i) {
        const Account account = static_cast<Account>(i);
        EXPECT_FALSE(ToString(account).empty());
        EXPECT_FALSE(DisplayName(account).empty());
        EXPECT_TRUE(names.insert(ToString(account)).second);
        EXPECT_TRUE(labels.insert(DisplayName(account)).second);
        EXPECT_EQ(IsIncome(account), i <= static_cast<int>(Account::Refunds));
    }
    EXPECT_EQ(FormatMoney(0), "$0");
    EXPECT_EQ(FormatMoney(5), "$0.05");
    EXPECT_EQ(FormatMoney(Dollars(1)), "$1");
    EXPECT_EQ(FormatMoney(Dollars(1234)), "$1,234");
    EXPECT_EQ(FormatMoney(Dollars(1234) + 50), "$1,234.50");
    EXPECT_EQ(FormatMoney(-Dollars(12)), "-$12");
    EXPECT_EQ(FormatMoney(Dollars(1000000)), "$1,000,000");
}

TEST(Finance, JsonRoundTripAndRejectsCorruption) {
    Finance finance(Dollars(1200));
    finance.Earn(Account::Donations, 75);
    finance.TakeLoan(Finance::kLoanStep);
    finance.CloseDay(1);
    const auto document = ToJson(finance);
    const auto restored = FinanceFromJson(document);
    ASSERT_TRUE(restored.has_value());
    EXPECT_EQ(finance, *restored);
    std::string error;
    auto corrupt = document;
    corrupt["history"][0]["accounts"]["unknown"] = 1;
    EXPECT_FALSE(FinanceFromJson(corrupt, &error).has_value());
    EXPECT_FALSE(error.empty());
    corrupt = document;
    corrupt["history"][0]["accounts"]["donations"] = -1;
    EXPECT_FALSE(FinanceFromJson(corrupt, &error).has_value());
}

}  // namespace
}  // namespace zoo
