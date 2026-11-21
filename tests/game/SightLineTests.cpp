// SPDX-License-Identifier: MIT
#include "SightLine.hpp"

#include <cmath>
#include <numbers>
#include <string_view>

#include <gtest/gtest.h>

#include "TestData.hpp"

namespace zoogame {
namespace {

TEST(SightLine, LooksAtTheHeadAndNeckOfATallAnimal) {
    const AnimalSightTarget giraffe{{zoo::Vec3{0, 2, 13}, {0, 3, 12}, {0, 5, 11}}, 0.65f};
    const zoo::Vec3 eye{0, 1.62f, 0};
    for (const zoo::Vec3 point : {giraffe.joints[2], (giraffe.joints[1] + giraffe.joints[2]) * 0.5f}) {
        const auto aim = zoo::Normalized(point - eye);
        ASSERT_TRUE(AimAtAnimal(giraffe, eye, aim, 70, 1));
        EXPECT_FALSE(AimAtAnimal(giraffe, eye, aim, 8, 1));
        EXPECT_FALSE(AimAtAnimal(giraffe, eye, aim * -1.0f, 70, 1));
    }
}

TEST(SightLine, SelectsTheNearerFamilyMemberWhenBodiesOverlap) {
    const zoo::Vec3 eye{0, 0.3f, 0};
    const zoo::Vec3 gaze{0, 0, 1};
    const AnimalSightTarget cub{{zoo::Vec3{0.02f, 0.3f, 4.3f}, {0.02f, 0.3f, 4}, {0.02f, 0.35f, 3.8f}},
                                0.12f};
    const AnimalSightTarget mother{{zoo::Vec3{0, 0.3f, 8}, {0, 0.4f, 7}, {0, 0.6f, 6.7f}}, 0.34f};
    const auto near = AimAtAnimal(cub, eye, gaze, 70, 1);
    const auto far = AimAtAnimal(mother, eye, gaze, 70, 1);
    ASSERT_TRUE(near);
    ASSERT_TRUE(far);
    EXPECT_LT(*near, *far);
    EXPECT_FALSE(AimAtAnimal(cub, eye + zoo::Vec3{0, 0.8f, 0}, gaze, 70, 1));
}

TEST(SightLine, NearSurfaceWinsWhenTheLargerAnimalsCentreIsFartherAway) {
    const zoo::Vec3 eye{0, 0, 0};
    const zoo::Vec3 gaze{0, 0, 1};
    const zoo::Vec3 smallCentre{0, 0, 5.6f};
    const zoo::Vec3 largeCentre{0, 0, 6};
    const auto small = AimAtAnimal({{smallCentre, smallCentre, smallCentre}, 0.1f}, eye, gaze, 20, 7);
    const auto large = AimAtAnimal({{largeCentre, largeCentre, largeCentre}, 1.0f}, eye, gaze, 20, 7);
    ASSERT_TRUE(small);
    ASSERT_TRUE(large);
    EXPECT_LT(*large, *small);
    EXPECT_NEAR(large->depth, 5.0f, 0.002f);
    EXPECT_NEAR(small->depth, 5.5f, 0.002f);
}

TEST(SightLine, ABodyCanEnterTheViewingRangeBeforeItsCentreDoes) {
    const zoo::Vec3 eye{0, 0, 0};
    const zoo::Vec3 gaze{0, 0, 1};
    const zoo::Vec3 centre{0, 0, 10.5f};
    const AnimalSightTarget target{{centre, centre, centre}, 1.0f};
    const auto found = AimAtAnimal(target, eye, gaze, 10.0f, 1.0f);
    ASSERT_TRUE(found);
    EXPECT_NEAR(found->depth, 9.5f, 0.002f);
    EXPECT_FALSE(AimAtAnimal(target, eye, gaze, 9.4f, 1.0f));
}

TEST(SightLine, TreeCrownsRatherThanOnlyTheirTrunksBlockAnAutomaticView) {
    const zoo::Catalog& catalog = zoo::testdata::GameCatalog();
    for (const std::string_view id : {"oak", "birch", "pine", "spruce", "acacia", "willow"}) {
        const int index = catalog.PropIndex(id);
        ASSERT_GE(index, 0) << id;
        const zoo::PropDef& tree = catalog.props[static_cast<std::size_t>(index)];
        ASSERT_EQ(tree.role, zoo::PropRole::Tree) << id;
        ASSERT_GT(tree.shadeM2, 0.0f) << id;
        const float crown = std::sqrt(tree.shadeM2 / std::numbers::pi_v<float>);
        EXPECT_FLOAT_EQ(SightBlockingRadius(tree, 1.0f), std::max(tree.radiusM, crown)) << id;
        EXPECT_GT(SightBlockingRadius(tree, 1.0f), tree.radiusM) << id;
        EXPECT_FLOAT_EQ(SightBlockingRadius(tree, 0.7f), 0.7f * std::max(tree.radiusM, crown)) << id;
    }
}

TEST(SightLine, NonTreesKeepTheirSolidGroundRadius) {
    zoo::PropDef bush;
    bush.role = zoo::PropRole::Bush;
    bush.radiusM = 0.6f;
    bush.shadeM2 = 40.0f;
    EXPECT_FLOAT_EQ(SightBlockingRadius(bush, 1.25f), 0.75f);
    EXPECT_FLOAT_EQ(SightBlockingRadius(bush, -1.0f), 0.0f);
}

}  // namespace
}  // namespace zoogame
