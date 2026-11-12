// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <string>

namespace zoo {

/// An amount of money in cents. Integer, so that books always balance and a
/// saved game reloads to the same cent.
using Money = std::int64_t;

constexpr Money Dollars(std::int64_t dollars) { return dollars * 100; }

/// "$1,234" for whole amounts, "$1,234.50" otherwise, "-$12" for negatives.
std::string FormatMoney(Money amount);

}  // namespace zoo
