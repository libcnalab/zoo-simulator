// SPDX-License-Identifier: MIT
#include "AnimalTextures.hpp"

#include <array>
#include <gtest/gtest.h>

#include "CnaEngine/Graphics/Image.hpp"

namespace zoogame {
namespace {

namespace Graphics = CnaEngine::Graphics;
namespace Xna = Microsoft::Xna::Framework::Graphics;
using Microsoft::Xna::Framework::Color;

std::vector<std::uint8_t> Level(const Xna::Texture2D& texture, int level, int width, int height) {
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(width * height) * 4u);
    texture.GetData(level, nullptr, bytes.data(), 0, static_cast<int>(bytes.size()));
    return bytes;
}

Graphics::Image Pattern() {
    Graphics::Image image(8, 8);
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 8; ++x)
            image.Set(x, y, Color(x * 31, y * 31, (x + y) * 15, (x * 57 + y * 13) % 256));
    return image;
}

TEST(AnimalTextures, ReleasesOriginalPixelsAndPreservesEveryMipLevel) {
    Xna::GraphicsDevice device;
    const auto image = Pattern();
    auto source = Graphics::UploadTexture(device, image, {}, false);
    const auto oldPixels = source->GetCpuPixelsWeak();
    ASSERT_FALSE(oldPixels.expired());
    Graphics::Material first, second;
    first.baseTexture = source.get();
    first.colorTexturesAreSrgb = true;
    second = first;
    std::array materials{&first, &second};

    auto variants = GiveMaterialMipmaps(materials);
    EXPECT_TRUE(variants.empty());
    EXPECT_TRUE(oldPixels.expired()) << "the unused full-size CPU image must be released";
    EXPECT_EQ(first.baseTexture, source.get());
    EXPECT_EQ(second.baseTexture, source.get());
    const auto expected = Graphics::BuildMipChain(image, {.rounding = Graphics::MipRounding::Nearest,
                                                        .srgb = true, .alphaTestReference = {}});
    ASSERT_EQ(source->getLevelCountProperty(), static_cast<int>(expected.size()));
    for (std::size_t level = 0; level < expected.size(); ++level)
        EXPECT_EQ(Level(*source, static_cast<int>(level), expected[level].Width(), expected[level].Height()),
                  expected[level].Bytes());

    // A later view/load borrows the same model-owned wrapper, without another allocation.
    const auto pixels = source->GetCpuPixelsWeak().lock();
    EXPECT_TRUE(GiveMaterialMipmaps(materials).empty());
    EXPECT_EQ(source->GetCpuPixelsWeak().lock(), pixels);
    EXPECT_EQ(Level(*source, 0, 8, 8), image.Bytes());
}

TEST(AnimalTextures, SharedColourAndNormalMapKeepDistinctFiltering) {
    Xna::GraphicsDevice device;
    Graphics::Image image(2, 2, Color(0, 0, 0, 255));
    image.Set(1, 0, Color(255, 255, 255, 255));
    image.Set(1, 1, Color(255, 255, 255, 255));
    auto source = Graphics::UploadTexture(device, image, {}, false);
    Graphics::Material material;
    material.baseTexture = source.get();
    material.normalTexture = source.get();
    material.occlusionRoughnessMetallicTexture = source.get();
    material.colorTexturesAreSrgb = true;
    std::array materials{&material};

    auto variants = GiveMaterialMipmaps(materials);
    ASSERT_EQ(variants.size(), 2u);
    EXPECT_NE(material.baseTexture, material.normalTexture);
    EXPECT_EQ(material.normalTexture, material.occlusionRoughnessMetallicTexture);
    EXPECT_EQ(Level(*material.baseTexture, 1, 1, 1)[0], 188);
    EXPECT_EQ(Level(*material.normalTexture, 1, 1, 1)[0], 128);
    EXPECT_EQ(source->getLevelCountProperty(), 1);
    EXPECT_EQ(Level(*source, 0, 2, 2), image.Bytes());
}

TEST(AnimalTextures, IdenticalCoatsFromSeparateModelsShareOnlyTheirUploadedImage) {
    Xna::GraphicsDevice device;
    AnimalTextureCache cache;
    const auto image = Pattern();
    auto first = Graphics::UploadTexture(device, image, {}, false);
    auto second = Graphics::UploadTexture(device, image, {}, false);
    auto secondRecord = CNA::Internal::ContentOwnershipAccess::Record(*second);
    const auto duplicate = second->GetRendererWeak();
    Graphics::Material a, b;
    a.baseTexture = first.get();
    b.baseTexture = second.get();
    std::array firstModel{&a}, secondModel{&b};
    EXPECT_TRUE(GiveMaterialMipmaps(firstModel, true, &cache).empty());
    EXPECT_TRUE(GiveMaterialMipmaps(secondModel, true, &cache).empty());
    EXPECT_EQ(a.baseTexture, first.get());
    EXPECT_EQ(b.baseTexture, second.get());
    EXPECT_TRUE(duplicate.expired());
    EXPECT_EQ(first->GetRendererWeak().lock(), second->GetRendererWeak().lock());
    EXPECT_TRUE(second->GetCpuPixelsWeak().expired());
    EXPECT_EQ(cache.SharedCount(), 1u);
    EXPECT_EQ(cache.SharedBytes(), (64u + 16u + 4u + 1u) * 4u);
    secondRecord->Dispose();
    first->Dispose();
    const auto shared = second->GetRendererWeak();
    EXPECT_FALSE(shared.expired());
    EXPECT_FALSE(second->getIsDisposedProperty());
    EXPECT_EQ(second->getLevelCountProperty(), 4);
    second->Dispose();
    EXPECT_TRUE(shared.expired());
    auto next = Graphics::UploadTexture(device, image, {}, false);
    b.baseTexture = next.get();
    EXPECT_TRUE(GiveMaterialMipmaps(secondModel, true, &cache).empty());
    EXPECT_EQ(cache.SharedCount(), 1u);
    EXPECT_EQ(next->getLevelCountProperty(), 4);
}

TEST(AnimalTextures, IndependentCoatsKeepDifferencesInPixelsAndFiltering) {
    Xna::GraphicsDevice device;
    AnimalTextureCache cache;
    auto image = Pattern();
    auto texture = [&](bool srgb, std::optional<int> cutoff = {}) {
        return cache.Upload(device, image, {.rounding = Graphics::MipRounding::Nearest, .srgb = srgb,
                                            .alphaTestReference = cutoff});
    };
    const auto opaque = texture(true);
    const auto normal = texture(false);
    const auto cutout = texture(true, 128);
    const auto harderCutout = texture(true, 170);
    const auto truncated = cache.Upload(device, image, {.rounding = Graphics::MipRounding::Truncate,
                                                        .srgb = true, .alphaTestReference = {}});
    Graphics::Image reshaped(4, 16);
    reshaped.Bytes() = image.Bytes();
    const auto rectangular = cache.Upload(device, reshaped, {.rounding = Graphics::MipRounding::Nearest,
                                                             .srgb = true, .alphaTestReference = {}});
    image.Set(7, 7, Color(1, 2, 3, 4));
    const auto changed = texture(true);
    EXPECT_EQ(cache.SharedCount(), 0u);
    EXPECT_NE(opaque->GetRendererWeak().lock(), normal->GetRendererWeak().lock());
    EXPECT_NE(opaque->GetRendererWeak().lock(), cutout->GetRendererWeak().lock());
    EXPECT_NE(cutout->GetRendererWeak().lock(), harderCutout->GetRendererWeak().lock());
    EXPECT_NE(opaque->GetRendererWeak().lock(), truncated->GetRendererWeak().lock());
    EXPECT_NE(opaque->GetRendererWeak().lock(), rectangular->GetRendererWeak().lock());
    EXPECT_NE(opaque->GetRendererWeak().lock(), changed->GetRendererWeak().lock());
}

TEST(AnimalTextures, OversizedAtlasStartsAtOneK) {
    Xna::GraphicsDevice device;
    const Graphics::Image image(2048, 1024, Color(23, 71, 149, 255));
    auto source = Graphics::UploadTexture(device, image, {}, false);
    Graphics::Material material;
    material.baseTexture = source.get();
    material.colorTexturesAreSrgb = true;
    std::array materials{&material};

    auto variants = GiveMaterialMipmaps(materials);
    EXPECT_TRUE(variants.empty());
    EXPECT_EQ(material.baseTexture, source.get());
    EXPECT_EQ(source->getWidthProperty(), 1024);
    EXPECT_EQ(source->getHeightProperty(), 512);
    EXPECT_EQ(source->getLevelCountProperty(), 11);
    const auto level = Level(*source, 0, 1024, 512);
    ASSERT_EQ(level.size(), static_cast<std::size_t>(1024 * 512 * 4));
    EXPECT_EQ((std::array<std::uint8_t, 4>{level[0], level[1], level[2], level[3]}),
              (std::array<std::uint8_t, 4>{23, 71, 149, 255}));
    EXPECT_EQ((std::array<std::uint8_t, 4>{level[level.size() - 4], level[level.size() - 3],
                                          level[level.size() - 2], level[level.size() - 1]}),
              (std::array<std::uint8_t, 4>{23, 71, 149, 255}));
}

TEST(AnimalTextures, DiscardedCpuMipCopiesKeepTheWholeGpuChain) {
    Xna::GraphicsDevice device;
    const auto image = Pattern();
    auto source = Graphics::UploadTexture(device, image, {}, false);
    Graphics::Material material;
    material.baseTexture = source.get();
    material.colorTexturesAreSrgb = true;
    std::array materials{&material};

    auto variants = GiveMaterialMipmaps(materials, true);
    EXPECT_TRUE(variants.empty());
    EXPECT_EQ(material.baseTexture, source.get());
    EXPECT_TRUE(source->GetCpuPixelsWeak().expired());
    const auto expected = Graphics::BuildMipChain(image, {.rounding = Graphics::MipRounding::Nearest,
                                                        .srgb = true, .alphaTestReference = {}});
    EXPECT_EQ(source->getLevelCountProperty(), static_cast<int>(expected.size()));
    EXPECT_FALSE(source->GetRendererWeak().expired());
}

TEST(AnimalTextures, ReleasesRendererRetainedByContentManagerRecord) {
    Xna::GraphicsDevice device;
    const auto image = Pattern();
    auto source = Graphics::UploadTexture(device, image, {}, false);
    // The importer records a separate wrapper for ContentManager::Unload. A plain
    // texture test cannot detect keeping that wrapper's unused GPU resource alive.
    auto record = CNA::Internal::ContentOwnershipAccess::Record(*source);
    const auto oldRenderer = source->GetRendererWeak();
    Graphics::Material material;
    material.baseTexture = source.get();
    std::array materials{&material};

    auto variants = GiveMaterialMipmaps(materials);
    EXPECT_TRUE(variants.empty());
    EXPECT_TRUE(oldRenderer.expired());
    EXPECT_FALSE(source->getIsDisposedProperty());
    EXPECT_EQ(Level(*source, 0, 8, 8), image.Bytes());
    // A subsequent Unload of the old record must not dispose the replacement.
    record->Dispose();
    EXPECT_FALSE(source->getIsDisposedProperty());
    EXPECT_EQ(Level(*source, 0, 8, 8), image.Bytes());
}

TEST(AnimalTextures, SharedOpaqueAndCutoutKeepTheirCoverageRules) {
    Xna::GraphicsDevice device;
    const auto image = Pattern();
    auto source = Graphics::UploadTexture(device, image, {}, false);
    Graphics::Material opaque, cutout;
    opaque.baseTexture = source.get();
    opaque.colorTexturesAreSrgb = true;
    cutout = opaque;
    cutout.blend = Graphics::BlendMode::AlphaTest;
    cutout.alphaCutoff = 0.5f;
    std::array materials{&opaque, &cutout};

    auto variants = GiveMaterialMipmaps(materials);
    ASSERT_EQ(variants.size(), 2u);
    EXPECT_NE(opaque.baseTexture, cutout.baseTexture);
    const auto plain = Graphics::BuildMipChain(image, {.rounding = Graphics::MipRounding::Nearest,
                                                     .srgb = true, .alphaTestReference = {}});
    const auto kept = Graphics::BuildMipChain(image, {.rounding = Graphics::MipRounding::Nearest,
                                                    .srgb = true, .alphaTestReference = 128});
    for (std::size_t level = 0; level < plain.size(); ++level) {
        EXPECT_EQ(Level(*opaque.baseTexture, static_cast<int>(level), plain[level].Width(), plain[level].Height()),
                  plain[level].Bytes());
        EXPECT_EQ(Level(*cutout.baseTexture, static_cast<int>(level), kept[level].Width(), kept[level].Height()),
                  kept[level].Bytes());
    }
}

}  // namespace
}  // namespace zoogame
