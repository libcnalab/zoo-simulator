// SPDX-License-Identifier: MIT
#include "AnimalTextures.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <exception>
#include <map>
#include <optional>
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

std::vector<std::unique_ptr<Xna::Texture2D>> GiveMipmaps(CnaEngine::Graphics::ModelView& view) {
    std::vector<std::unique_ptr<Xna::Texture2D>> made;
    // Keyed by texture and alpha reference: the same image may serve a cut-out and an
    // opaque material, which need different chains.
    std::map<std::pair<const Xna::Texture2D*, int>, Xna::Texture2D*> replaced;
    const auto give = [&](Xna::Texture2D*& slot, bool srgb, std::optional<int> reference = {}) {
        if (slot == nullptr)
            return;
        const std::pair<const Xna::Texture2D*, int> key{slot, reference.value_or(0)};
        if (const auto found = replaced.find(key); found != replaced.end()) {
            slot = found->second;
            return;
        }
        Xna::Texture2D* const original = slot;
        try {
            if (auto texture = Mipmapped(*original, srgb, reference)) {
                slot = texture.get();
                made.push_back(std::move(texture));
            }
        } catch (const std::exception& error) {
            std::printf("zoo: animals: a texture keeps its one level: %s\n", error.what());
        }
        replaced.emplace(key, slot);
    };
    for (CnaEngine::Graphics::ModelPart& part : view.Parts()) {
        CnaEngine::Graphics::Material& material = part.material.material;
        std::optional<int> reference;
        if (material.blend == CnaEngine::Graphics::BlendMode::AlphaTest)
            reference = std::clamp(static_cast<int>(std::lround(material.alphaCutoff * 255.0f)), 1, 254);
        give(material.baseTexture, material.colorTexturesAreSrgb, reference);
        give(material.normalTexture, false);
        give(material.occlusionRoughnessMetallicTexture, false);
    }
    return made;
}

}  // namespace zoogame
