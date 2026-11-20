// SPDX-License-Identifier: MIT
#include <gtest/gtest.h>

#include "AnimalWater.hpp"

namespace zoogame {
namespace {

std::uint8_t AlphaAt(const AnimalWaterImage& image, int x, int y) {
    return image.rgba[static_cast<std::size_t>((y * image.size + x) * 4 + 3)];
}

TEST(AnimalWater, ThreeSoftRingsLeaveTheAnimalAndCornersClear) {
    const AnimalWaterImage image = MakeAnimalWaterImage();
    ASSERT_EQ(image.size, 64);
    ASSERT_EQ(image.rgba.size(), static_cast<std::size_t>(image.size * image.size * 4));

    const int middle = image.size / 2;
    EXPECT_EQ(AlphaAt(image, middle, middle), 0U);
    EXPECT_EQ(AlphaAt(image, 0, 0), 0U);
    const auto radial = [&](float radius) {
        return AlphaAt(
            image,
            middle + static_cast<int>(radius * static_cast<float>(image.size) * 0.5f), middle);
    };
    EXPECT_GT(radial(0.30f), radial(0.56f));
    EXPECT_GT(radial(0.56f), radial(0.82f));
    EXPECT_GT(radial(0.82f), 0U);

    int covered = 0;
    for (std::size_t alpha = 3; alpha < image.rgba.size(); alpha += 4) {
        covered += image.rgba[alpha] != 0U ? 1 : 0;
    }
    EXPECT_GT(covered, image.size * 2);
    EXPECT_LT(covered, image.size * image.size / 3);
}

}  // namespace
}  // namespace zoogame
