// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <functional>

#include "zoo/visitors/Visitors.hpp"

namespace zoo {
class Zoo;

/// How a visit on foot begins (the figures the game uses; see ZooGameVisit.cpp).
struct VisitStartOptions {
    std::int64_t day = 14;
    float hour = 10.5f;
    std::int64_t fillTicks = 6000;  ///< steps with the calendar stopped; negative means zero
    /// A minute or two at a fence (72 to 126 s), forty minutes in the zoo. Measured over
    /// 6,000 steps after the fill with 800 people (2026-11-13): 210 to 260 of them stand
    /// watching and every habitat keeps watchers; at linger 6, stay 6 it was 110 to 170, with
    /// one in three on the way to a stall, a restroom or a bin.
    VisitorPace pace{.linger = 9.0f, .stay = 12.0f, .arrivals = 0.25f};
    float fillArrivals = 1.7f;  ///< arrivals while filling
};

struct VisitStartReport {
    int visitors = 0;
    int spread = 0;  ///< set down about the zoo by SpreadVisitors
    std::int64_t ticks = 0;
};

/// Sets the calendar and runs the morning exactly as ZooGame::PrepareVisit(fill = true) does
/// today, step for step (the same calls in the same order, so the same StateHash), except
/// SetDayPace at the end, which stays the caller's business. `progress`, when given, is called
/// every 250 steps and on completion with the share done (0..1): the loading screen.
/// Leaves the calendar stopped and restores the requested visitor pace.
VisitStartReport FillForVisit(Zoo& zoo, const VisitStartOptions& options = {},
                              const std::function<void(float)>& progress = {});
}  // namespace zoo
