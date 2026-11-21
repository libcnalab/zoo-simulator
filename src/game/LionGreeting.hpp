// SPDX-License-Identifier: MIT
#pragma once

#include <array>
#include <optional>

#include "Common.hpp"
#include "Microsoft/Xna/Framework/Quaternion.hpp"

namespace zoogame {

/// The two neck joints, head joint and a skin landmark, as posed in world metres.
struct LionNeck {
    std::array<Vector3, 3> joints;
    Vector3 landmark;
};

using NeckTurns = std::array<Microsoft::Xna::Framework::Quaternion, 3>;
inline const NeckTurns kNoNeckTurn{Microsoft::Xna::Framework::Quaternion::Identity,
                                   Microsoft::Xna::Framework::Quaternion::Identity,
                                   Microsoft::Xna::Framework::Quaternion::Identity};

struct LionGreeting {
    NeckTurns initiator;
    NeckTurns receiver;
};

/// Both animals can turn their necks, with a smaller response from the resting animal.
/// Returns nothing when their feet would have to move to bring their faces together.
std::optional<LionGreeting> FitLionGreeting(const LionNeck& muzzle, const LionNeck& cheek,
                                            float smallerHeight);

/// Cumulative world-space corrections for each neck joint and its descendants.
std::array<Matrix, 3> NeckCorrections(const LionNeck& neck, const NeckTurns& turns);

}  // namespace zoogame
