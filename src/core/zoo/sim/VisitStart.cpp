// SPDX-License-Identifier: MIT
#include "zoo/sim/VisitStart.hpp"

#include <algorithm>

#include "zoo/sim/Zoo.hpp"

namespace zoo {
VisitStartReport FillForVisit(Zoo& zoo, const VisitStartOptions& options,
                              const std::function<void(float)>& progress) {
    zoo.Guests().SetPace(options.pace);
    zoo.SetCalendar(options.day, options.hour);
    zoo.SetDayPace(0);
    const std::int64_t ticks = std::max<std::int64_t>(0, options.fillTicks);
    // Quotient and remainder avoid overflow for a large requested duration.
    // Let the gate admit enough distinct groups to represent every habitat in a large zoo, while
    // retaining a final fifth of the fill for the staged visitors to resume their visit.
    const std::int64_t before = (ticks / 5) * 4 + (ticks % 5) * 4 / 5;
    VisitorPace filling = options.pace;
    filling.arrivals = options.fillArrivals;
    zoo.Guests().SetPace(filling);
    VisitStartReport report;
    for (std::int64_t i = 0; i < ticks; ++i) {
        if (i == before) {
            report.spread = zoo.SpreadVisitors();
            zoo.Guests().SetPace(options.pace);
        }
        zoo.Step();
        if (progress && (i + 1) % 250 == 0 && i + 1 < ticks) {
            progress(static_cast<float>(i + 1) / static_cast<float>(ticks));
        }
    }
    if (ticks == 0) {
        report.spread = zoo.SpreadVisitors();
        zoo.Guests().SetPace(options.pace);
    }
    report.ticks = ticks;
    report.visitors = zoo.Guests().Count();
    if (progress) {
        progress(1.0f);
    }
    return report;
}
}  // namespace zoo
