// SPDX-License-Identifier: MIT
#include "KeeperAppearance.hpp"

#include <cstdint>
#include <optional>
#include <random>

#include "CNA/Internal/GamerServices/AvatarDescriptionCodec.hpp"

namespace zoogame {

Microsoft::Xna::Framework::GamerServices::AvatarDescription StaffAppearance(std::size_t slot) {
    namespace Avatars = CNA::Internal::GamerServices::Avatars;
    constexpr std::uint32_t kCastSeed = 0x5a00657u;
    const std::uint32_t seed = kCastSeed + static_cast<std::uint32_t>(slot) * 7919u;
    std::mt19937 random(seed);
    const auto body = std::optional<std::uint8_t>(static_cast<std::uint8_t>(slot % 2));
    return Microsoft::Xna::Framework::GamerServices::AvatarDescription(
        Avatars::encode(Avatars::randomDescriptor(body, random)));
}

}  // namespace zoogame
