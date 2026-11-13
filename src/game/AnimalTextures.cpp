// SPDX-License-Identifier: MIT
#include "AnimalTextures.hpp"

#include <cstdio>
#include <exception>
#include <unordered_map>
#include <utility>

#include "CnaEngine/Graphics/Image.hpp"

namespace zoogame {

namespace {

namespace Xna = Microsoft::Xna::Framework::Graphics;

std::unique_ptr<Xna::Texture2D> Mipmapped(const Xna::Texture2D& texture, bool srgb) {
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
    return CnaEngine::Graphics::UploadTexture(*device, image, options, true);
}

}  // namespace

std::vector<std::unique_ptr<Xna::Texture2D>> GiveMipmaps(CnaEngine::Graphics::ModelView& view) {
    std::vector<std::unique_ptr<Xna::Texture2D>> made;
    std::unordered_map<const Xna::Texture2D*, Xna::Texture2D*> replaced;
    const auto give = [&](Xna::Texture2D*& slot, bool srgb) {
        if (slot == nullptr)
            return;
        if (const auto found = replaced.find(slot); found != replaced.end()) {
            slot = found->second;
            return;
        }
        Xna::Texture2D* const original = slot;
        try {
            if (auto texture = Mipmapped(*original, srgb)) {
                slot = texture.get();
                made.push_back(std::move(texture));
            }
        } catch (const std::exception& error) {
            std::printf("zoo: animals: a texture keeps its one level: %s\n", error.what());
        }
        replaced.emplace(original, slot);
    };
    for (CnaEngine::Graphics::ModelPart& part : view.Parts()) {
        CnaEngine::Graphics::Material& material = part.material.material;
        give(material.baseTexture, material.colorTexturesAreSrgb);
        give(material.normalTexture, false);
        give(material.occlusionRoughnessMetallicTexture, false);
    }
    return made;
}

}  // namespace zoogame
