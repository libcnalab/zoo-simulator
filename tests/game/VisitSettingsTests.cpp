#include <gtest/gtest.h>

#include "Visit.hpp"
#include "zoo/visitors/Visitors.hpp"

TEST(VisitSettings, ManyIsAnExplicitExperimentalCrowdLevel) {
    ASSERT_EQ(zoogame::kCrowdLevels.size(), 6U);
    const zoogame::CrowdLevel& many = zoogame::kCrowdLevels.back();
    EXPECT_STREQ(many.key, "many");
    EXPECT_STREQ(many.name, "Many (experimental)");
    EXPECT_EQ(many.people, 3000);
    EXPECT_LE(many.people, zoo::Visitors::kMaxVisitors);
    EXPECT_STREQ(zoogame::kCrowdLevels[static_cast<std::size_t>(zoogame::kNormalCrowd)].key, "normal");
}
