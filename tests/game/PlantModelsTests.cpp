// SPDX-License-Identifier: MIT
#include "PlantModels.hpp"

#include <array>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

namespace zoogame {
namespace {

zoo::PropDef Tree(std::string_view look) {
    zoo::PropDef def;
    def.id = std::string(look);
    def.look = std::string(look);
    def.radiusM = 1.0f;
    def.heightM = 6.0f;
    def.color = {0.25f, 0.45f, 0.18f};
    def.color2 = {0.32f, 0.22f, 0.13f};
    return def;
}

std::size_t LeafTriangles(const ModelBuilder& model) {
    std::size_t triangles = 0;
    for (const ModelPart& part : model.Parts()) {
        if (part.finish.pattern == Pattern::Leaves) {
            triangles += part.writer.mesh.indices.size() / 3;
        }
    }
    return triangles;
}

TEST(PlantModels, BroadleafLitterIsNearOnlyAndConifersDoNotGetIt) {
    static constexpr std::array<std::string_view, 5> broadleaves = {"oak", "birch", "willow",
                                                                    "acacia", "baobab"};
    for (const std::string_view look : broadleaves) {
        SCOPED_TRACE(look);
        const zoo::PropDef def = Tree(look);
        ModelBuilder near;
        ASSERT_TRUE(BuildPlant(near, def, 12345, PlantDetail::Near));
        EXPECT_EQ(LeafTriangles(near), 64u);

        ModelBuilder far;
        ASSERT_TRUE(BuildPlant(far, def, 12345, PlantDetail::Far));
        EXPECT_EQ(LeafTriangles(far), 0u);
    }

    ModelBuilder pine;
    ASSERT_TRUE(BuildPlant(pine, Tree("pine"), 12345, PlantDetail::Near));
    EXPECT_EQ(LeafTriangles(pine), 0u);
}

}  // namespace
}  // namespace zoogame
