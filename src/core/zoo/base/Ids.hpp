// SPDX-License-Identifier: MIT
#pragma once

#include <compare>
#include <cstdint>
#include <functional>

namespace zoo {

/// A typed integer handle. 0 is "none"; ids are never reused within one game,
/// so a stale id simply fails to resolve.
template <typename Tag>
struct Id {
    std::uint32_t value = 0;

    constexpr explicit operator bool() const { return value != 0; }
    constexpr auto operator<=>(const Id&) const = default;
};

using AnimalId = Id<struct AnimalTag>;
using VisitorId = Id<struct VisitorTag>;
using StaffId = Id<struct StaffTag>;
using EnclosureId = Id<struct EnclosureTag>;
using BuildingId = Id<struct BuildingTag>;
using PropId = Id<struct PropTag>;

}  // namespace zoo

template <typename Tag>
struct std::hash<zoo::Id<Tag>> {
    std::size_t operator()(zoo::Id<Tag> id) const noexcept { return std::hash<std::uint32_t>{}(id.value); }
};
