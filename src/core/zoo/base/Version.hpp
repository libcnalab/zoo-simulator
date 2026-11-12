// SPDX-License-Identifier: MIT
#pragma once

namespace zoo
{
    /// The simulator's own version (not the save format's; see persistence).
    [[nodiscard]] const char* VersionString() noexcept;
}
