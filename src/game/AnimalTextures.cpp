// SPDX-License-Identifier: MIT
#include "AnimalTextures.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <exception>
#include <map>
#include <optional>
#include <stdexcept>
#include <tuple>
#include <utility>

#include "CnaEngine/Graphics/Image.hpp"

#include "StaticTexture.hpp"
#ifdef ZOO_OPENSSL_SHA256
#include <openssl/sha.h>
#else
#include "System/Security/Cryptography/SHA256.hpp"
#endif

namespace zoogame {

namespace {

namespace Xna = Microsoft::Xna::Framework::Graphics;
constexpr int kMaxAnimalTextureSide = 1024;

std::vector<std::uint8_t> ImageDigest(const std::vector<std::uint8_t>& bytes) {
#ifdef ZOO_OPENSSL_SHA256
    std::vector<std::uint8_t> digest(SHA256_DIGEST_LENGTH);
    if (!SHA256(bytes.data(), bytes.size(), digest.data()))
        throw std::runtime_error("could not hash animal texture");
    return digest;
#else
    return System::Security::Cryptography::SHA256().ComputeHash(bytes);
#endif
}

std::unique_ptr<Xna::Texture2D> Mipmapped(const Xna::Texture2D& texture, bool srgb,
                                          std::optional<int> alphaTestReference,
                                          bool discardCpuMipCopies, AnimalTextureCache* cache) {
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
    // Several imported atlases are 2K despite an animal occupying only a fraction of a
    // 720p frame. Their decoded level zero alone cost 210 MB more than the 1K versions.
    // Downsample with the same colour-space and alpha-coverage rules used for later mips.
    const double coverage = alphaTestReference ? CnaEngine::Graphics::AlphaTestCoverage(image, *alphaTestReference)
                                               : 0.0;
    while (std::max(image.Width(), image.Height()) > kMaxAnimalTextureSide) {
        image = alphaTestReference
                    ? CnaEngine::Graphics::DownsampleForAlphaTest(image, *alphaTestReference, coverage, options)
                    : CnaEngine::Graphics::Downsample(image, options);
    }
    if (discardCpuMipCopies && cache)
        return cache->Upload(*device, image, options);
    auto made = CnaEngine::Graphics::UploadTexture(*device, image, options, true);
    if (discardCpuMipCopies) {
        // Context recovery is deliberately unavailable in a desktop visit. UploadTexture has
        // already released level zero there, but CNA retains the smaller levels as CPU staging.
        // Re-wrap the same completed renderer without those copies; GPU sampling stays intact.
        DiscardCpuTextureCopies(*made);
    }
    return made;
}

}  // namespace

std::unique_ptr<Xna::Texture2D> AnimalTextureCache::Upload(
    Xna::GraphicsDevice& device, const CnaEngine::Graphics::Image& image,
    const CnaEngine::Graphics::MipOptions& options) {
    // The capped base level feeds every subsequent mip. Hash its complete RGBA contents.
    const Key key{&device, image.Width(), image.Height(), options.rounding, options.srgb,
                  options.alphaTestReference, ImageDigest(image.Bytes())};
    auto& entry = images_[key];
    if (auto renderer = entry.renderer.lock()) {
        auto alias = Xna::Texture2D::ReconstructFromCache(device, image.Width(), image.Height(),
                                                           Xna::SurfaceFormat::Color, entry.levels,
                                                           std::move(renderer), {});
        ++sharedCount_;
        sharedBytes_ += entry.bytes;
        return std::make_unique<Xna::Texture2D>(std::move(alias));
    }
    auto texture = CnaEngine::Graphics::UploadTexture(device, image, options, true);
    DiscardCpuTextureCopies(*texture);
    entry.renderer = texture->GetRendererWeak();
    entry.levels = texture->getLevelCountProperty();
    entry.bytes = 0;
    for (int i = 0; i < entry.levels; ++i)
        entry.bytes += 4u * static_cast<std::size_t>(std::max(1, image.Width() >> i)) *
                       static_cast<std::size_t>(std::max(1, image.Height() >> i));
    return texture;
}

std::vector<std::unique_ptr<Xna::Texture2D>> GiveMaterialMipmaps(
    std::span<CnaEngine::Graphics::Material* const> materials, bool discardCpuMipCopies, AnimalTextureCache* cache) {
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
            if (auto texture = Mipmapped(*original, srgb, reference, discardCpuMipCopies, cache)) {
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

std::vector<std::unique_ptr<Xna::Texture2D>> GiveMipmaps(CnaEngine::Graphics::ModelView& view,
                                                         bool discardCpuMipCopies, AnimalTextureCache* cache) {
    std::vector<CnaEngine::Graphics::Material*> materials;
    for (auto& part : view.Parts())
        materials.push_back(&part.material.material);
    return GiveMaterialMipmaps(materials, discardCpuMipCopies, cache);
}

}  // namespace zoogame
