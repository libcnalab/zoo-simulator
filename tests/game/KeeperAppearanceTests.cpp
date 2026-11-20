// SPDX-License-Identifier: MIT
#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "KeeperAppearance.hpp"
#include "Microsoft/Xna/Framework/GamerServices/AvatarBodyType.hpp"

namespace zoogame {
namespace {

using Microsoft::Xna::Framework::GamerServices::AvatarBodyType;

TEST(KeeperAppearance, CastIsValidDistinctAndRepeatable) {
    std::array<std::vector<std::uint8_t>, 4> descriptions;
    for (std::size_t slot = 0; slot < descriptions.size(); ++slot) {
        const auto first = StaffAppearance(slot);
        const auto second = StaffAppearance(slot);
        ASSERT_TRUE(first.getIsValidProperty());
        EXPECT_GT(first.getHeightProperty(), 0.0f);
        EXPECT_EQ(first.getBodyTypeProperty(), slot % 2 == 0 ? AvatarBodyType::Female : AvatarBodyType::Male);
        descriptions[slot] = first.getDescriptionProperty();
        EXPECT_EQ(descriptions[slot], second.getDescriptionProperty());
        for (std::size_t earlier = 0; earlier < slot; ++earlier) {
            EXPECT_NE(descriptions[slot], descriptions[earlier]);
        }
    }
}

}  // namespace
}  // namespace zoogame
