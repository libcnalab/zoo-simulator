// SPDX-License-Identifier: MIT
#include "AnimalTextures.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <exception>
#include <map>
#include <optional>
#include <tuple>
#include <utility>

#include "CnaEngine/Graphics/Image.hpp"

namespace zoogame {

namespace {

namespace Xna = Microsoft::Xna::Framework::Graphics;

std::unique_ptr<Xna::Texture2D> Mipmapped(const Xna::Texture2D& texture, bool srgb,
                                          std::optional<int> alphaTestReference) {
    if (texture.getLevelCountProperty() > 1 || texture.getFormatProperty() != Xna::SurfaceFormat::Color)
        return nullptr;
    Xna::GraphicsDevice* device = texture.getGraphicsDeviceProperty();
    if (device == nullptr)
        return nullptr;
    CnaEngine::Graphics::Image image(texture.getWidthProperty(), texture.getHeightProperty());
    texture.GetData(image.Bytes().data(), static_cast<int>(image.Bytes().size()));
    CnaEngine::Graphics::MipOptions options;
    options.rounding = CnaEngine::Graphics::MipRounding::Nearest;
    options.srgb = srgb;
    // Hair cards (glTF MASK) keep their full-size coverage in every level; plain averages
    // would fade the strands until a lion's mane thins out with distance.
    options.alphaTestReference = alphaTestReference;
    return CnaEngine::Graphics::UploadTexture(*device, image, options, true);
}

}  // namespace

std::vector<std::unique_ptr<Xna::Texture2D>> GiveMaterialMipmaps(
    std::span<CnaEngine::Graphics::Material* const> materials) {
    std::vector<std::unique_ptr<Xna::Texture2D>> made;
    // Count distinct filtering requests before changing any texture. A source shared by
    // colour and data, or cut-out and opaque materials, needs separate mip chains.
    using Key = std::tuple<Xna::Texture2D*, bool, int>;
    std::map<Key, Xna::Texture2D*> replaced;
    std::map<Xna::Texture2D*, std::size_t> variants;
    const auto visit = [&](const auto& give) {
        for (auto* material : materials) {
            std::optional<int> reference;
            if (material->blend == CnaEngine::Graphics::BlendMode::AlphaTest)
                reference = std::clamp(static_cast<int>(std::lround(material->alphaCutoff * 255.0f)), 1, 254);
            give(material->baseTexture, material->colorTexturesAreSrgb, reference);
            give(material->normalTexture, false, std::nullopt);
            give(material->occlusionRoughnessMetallicTexture, false, std::nullopt);
        }
    };
    visit([&](Xna::Texture2D* slot, bool srgb, std::optional<int> reference) {
        if (slot != nullptr && replaced.emplace(Key{slot, srgb, reference.value_or(0)}, nullptr).second)
            ++variants[slot];
    });
    const auto give = [&](Xna::Texture2D*& slot, bool srgb, std::optional<int> reference = {}) {
        if (slot == nullptr)
            return;
        auto& replacement = replaced.at(Key{slot, srgb, reference.value_or(0)});
        if (replacement != nullptr) {
            slot = replacement;
            return;
        }
        Xna::Texture2D* const original = slot;
        try {
            if (auto texture = Mipmapped(*original, srgb, reference)) {
                if (variants.at(original) == 1) {
                    // The imported model (also cached by ContentManager) owns this wrapper.
                    // Keep its address, but dispose the unused single-level GPU image first.
                    // ContentManager keeps a disposal representative: merely overwriting
                    // this wrapper leaves that representative's renderer alive until Unload.
                    // Effects, later views and repeated loads all see the mipmapped resource.
                    original->Dispose();
                    *original = std::move(*texture);
                } else {
                    slot = texture.get();
                    made.push_back(std::move(texture));
                }
            }
        } catch (const std::exception& error) {
            std::printf("zoo: animals: a texture keeps its one level: %s\n", error.what());
        }
        replacement = slot;
    };
    visit(give);
    return made;
}

std::vector<std::unique_ptr<Xna::Texture2D>> GiveMipmaps(CnaEngine::Graphics::ModelView& view) {
    std::vector<CnaEngine::Graphics::Material*> materials;
    for (auto& part : view.Parts())
        materials.push_back(&part.material.material);
    return GiveMaterialMipmaps(materials);
}

}  // namespace zoogame
