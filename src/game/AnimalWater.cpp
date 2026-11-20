// SPDX-License-Identifier: MIT
#include "AnimalWater.hpp"

#include <algorithm>
#include <cmath>

namespace zoogame {
namespace {

constexpr int kSize = 64;

float SoftRing(float radius, float centre, float halfWidth) {
    const float feather = 2.0f / static_cast<float>(kSize);
    return std::clamp((halfWidth + feather - std::abs(radius - centre)) / feather, 0.0f, 1.0f);
}

}  // namespace

AnimalWaterImage MakeAnimalWaterImage() {
    AnimalWaterImage image;
    image.size = kSize;
    image.rgba.resize(static_cast<std::size_t>(kSize * kSize * 4), 255U);
    for (int y = 0; y < kSize; ++y) {
        for (int x = 0; x < kSize; ++x) {
            const float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(kSize) * 2.0f - 1.0f;
            const float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(kSize) * 2.0f - 1.0f;
            const float radius = std::sqrt(u * u + v * v);
            // The inner displacement is strongest; the two older rings dissolve
            // as they travel out. A low broad shoulder keeps filtering smooth.
            const float alpha = std::max({0.58f * SoftRing(radius, 0.30f, 0.016f),
                                          0.38f * SoftRing(radius, 0.56f, 0.014f),
                                          0.20f * SoftRing(radius, 0.82f, 0.012f)});
            const std::size_t pixel = static_cast<std::size_t>((y * kSize + x) * 4);
            image.rgba[pixel + 0] = 226U;
            image.rgba[pixel + 1] = 244U;
            image.rgba[pixel + 2] = 255U;
            image.rgba[pixel + 3] = static_cast<std::uint8_t>(std::lround(alpha * 255.0f));
        }
    }
    return image;
}

}  // namespace zoogame
