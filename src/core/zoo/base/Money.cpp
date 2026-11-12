// SPDX-License-Identifier: MIT
#include "zoo/base/Money.hpp"

namespace zoo {

std::string FormatMoney(Money amount) {
    const bool negative = amount < 0;
    const std::uint64_t cents = negative ? static_cast<std::uint64_t>(-(amount + 1)) + 1u
                                         : static_cast<std::uint64_t>(amount);
    std::string digits = std::to_string(cents / 100u);
    std::string out;
    for (std::size_t i = 0; i < digits.size(); ++i) {
        if (i != 0 && (digits.size() - i) % 3 == 0) {
            out += ',';
        }
        out += digits[i];
    }
    if (cents % 100u != 0) {
        out += '.';
        out += static_cast<char>('0' + (cents % 100u) / 10u);
        out += static_cast<char>('0' + (cents % 100u) % 10u);
    }
    return (negative ? "-$" : "$") + out;
}

}  // namespace zoo
