// SPDX-License-Identifier: MIT
#include "PlantModels.hpp"

#include <array>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

#include "CnaEngine/Collision/StaticWorld.hpp"
#include "Microsoft/Xna/Framework/Ray.hpp"

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

TEST(PlantModels, SolidTreeGeometryBlocksTheTrunkAndLeavesTheGapBesideItOpen) {
    using Microsoft::Xna::Framework::Ray;
    const auto tree = Tree("oak");
    const auto placement = Matrix::CreateRotationY(0.4f) * Matrix::CreateTranslation(20, 0, 15);
    for (const auto detail : {PlantDetail::Near, PlantDetail::Far}) {
        CnaEngine::Collision::StaticWorld sight;
        BuildTreeSight(sight, tree, 12345, detail, placement);
        const auto hit = sight.Raycast(Ray({20, 1, 5}, {0, 0, 1}), 20);
        ASSERT_TRUE(hit);
        EXPECT_GT(hit->distance, 9.0f);
        EXPECT_LT(hit->distance, 10.0f);
        EXPECT_FALSE(sight.Raycast(Ray({20, 1, 5}, {0, 0, 1}), 8.0f));
        EXPECT_FALSE(sight.Raycast(Ray({23, 1, 5}, {0, 0, 1}), 20));
    }
    EXPECT_FALSE(OpaqueTreeSurface(Pattern::LeafSpray));
    EXPECT_FALSE(OpaqueTreeSurface(Pattern::NeedleSpray));
}

std::size_t PatternTriangles(const ModelBuilder& model, Pattern pattern) {
    std::size_t triangles = 0;
    for (const ModelPart& part : model.Parts()) {
        if (part.finish.pattern == pattern) {
            triangles += part.writer.mesh.indices.size() / 3;
        }
    }
    return triangles;
}

std::size_t Triangles(const ModelBuilder& model) {
    std::size_t triangles = 0;
    for (const ModelPart& part : model.Parts()) {
        triangles += part.writer.mesh.indices.size() / 3;
    }
    return triangles;
}

TEST(PlantModels, BroadleafLitterIsNearOnlyAndConifersDoNotGetIt) {
    static constexpr std::array<std::string_view, 5> broadleaves = {"oak", "birch", "willow", "acacia",
                                                                    "baobab"};
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

TEST(PlantModels, ShrubsHaveFineNearSpraysAndALeanFarForm) {
    zoo::PropDef def = Tree("shrub");
    def.radiusM = 0.6f;
    def.heightM = 1.8f;

    ModelBuilder near;
    ASSERT_TRUE(BuildPlant(near, def, 12345, PlantDetail::Near));
    ModelBuilder far;
    ASSERT_TRUE(BuildPlant(far, def, 12345, PlantDetail::Far));

    const std::size_t nearSprays = PatternTriangles(near, Pattern::LeafSpray);
    const std::size_t farSprays = PatternTriangles(far, Pattern::LeafSpray);
    EXPECT_GE(nearSprays, 280u);
    EXPECT_LT(farSprays, 160u);
    EXPECT_GT(nearSprays, 4u * farSprays);
    EXPECT_LT(Triangles(near), 900u);
}

}  // namespace
}  // namespace zoogame
