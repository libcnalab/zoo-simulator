// SPDX-License-Identifier: MIT
#include "LionGreeting.hpp"

#include <gtest/gtest.h>

namespace zoogame {
namespace {

TEST(LionGreeting, StandingLionAndRestingCompanionCanMeetWithoutStretching) {
    // World-space joint positions from two stationary lionesses in the seed-1 pride,
    // relative to the standing lioness's feet.
    const LionNeck muzzle{
        {Vector3{0.378f, 0.887f, -0.451f}, {0.477f, 0.917f, -0.690f}, {0.494f, 0.884f, -0.801f}},
        {0.460f, 0.657f, -0.959f}};
    const LionNeck cheek{
        {Vector3{0.669f, 0.418f, -0.430f}, {0.439f, 0.531f, -0.481f}, {0.328f, 0.562f, -0.502f}},
        {0.100f, 0.495f, -0.596f}};
    const auto pair = FitLionGreeting(muzzle, cheek, 1.05f);
    ASSERT_TRUE(pair);
    const auto near = NeckCorrections(muzzle, pair->initiator);
    const auto far = NeckCorrections(cheek, pair->receiver);
    const auto nose = Vector3::Transform(muzzle.landmark, near.back());
    const auto face = Vector3::Transform(cheek.landmark, far.back());
    EXPECT_GT(Vector3::Distance(muzzle.landmark, cheek.landmark), 0.5f);
    EXPECT_LT(Vector3::Distance(nose, face), 0.05f);
    EXPECT_GT(Vector3::Distance(cheek.landmark, face), 0.07f);
    for (std::size_t i = 0; i < muzzle.joints.size(); ++i) {
        const Vector3 before = i == 0 ? muzzle.joints[i] : Vector3::Transform(muzzle.joints[i], near[i - 1]);
        EXPECT_LT(Vector3::Distance(before, Vector3::Transform(muzzle.joints[i], near[i])), 1.0e-5f);
    }
}

TEST(LionGreeting, AGapAlongTheOutstretchedNecksNeedsAnApproach) {
    LionNeck a{{Vector3{0, 1, 0}, {0, 1, .2f}, {0, 1, .4f}}, {0, 1, .6f}};
    LionNeck b{{Vector3{0, 1, 1.7f}, {0, 1, 1.5f}, {0, 1, 1.3f}}, {0, 1, 1.1f}};
    EXPECT_FALSE(FitLionGreeting(a, b, 1.0f));
    b.landmark.Z = 8.0f;
    EXPECT_FALSE(FitLionGreeting(a, b, 1.0f));
}

}  // namespace
}  // namespace zoogame
