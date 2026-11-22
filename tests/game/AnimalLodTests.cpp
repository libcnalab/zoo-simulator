// SPDX-License-Identifier: MIT
#include <gtest/gtest.h>

#include <numbers>

#include "AnimalLod.hpp"

namespace zoogame {
namespace {

TEST(AnimalLodTests, DistantBodyUsesProxyButBinocularsRestoreFullModel) {
    constexpr float radians50 = 50.0f * std::numbers::pi_v<float> / 180.0f;
    const float ordinary = AnimalProjectedPixels(1.0f, 100.0f, radians50, 720);
    EXPECT_LT(ordinary, kAnimalProxyBelowPixels);
    EXPECT_TRUE(UseAnimalProxy(false, ordinary));

    const float binoculars = AnimalProjectedPixels(1.0f, 100.0f, radians50 / 7.0f, 720);
    EXPECT_GT(binoculars, kAnimalFullAbovePixels);
    EXPECT_FALSE(UseAnimalProxy(true, binoculars));
}

TEST(AnimalLodTests, HysteresisKeepsThePreviousRepresentationBetweenThresholds) {
    const float between = 0.5f * (kAnimalProxyBelowPixels + kAnimalFullAbovePixels);
    EXPECT_FALSE(UseAnimalProxy(false, between));
    EXPECT_TRUE(UseAnimalProxy(true, between));
    EXPECT_TRUE(UseAnimalProxy(false, kAnimalProxyBelowPixels - 0.01f));
    EXPECT_FALSE(UseAnimalProxy(true, kAnimalFullAbovePixels + 0.01f));
}

}  // namespace
}  // namespace zoogame
