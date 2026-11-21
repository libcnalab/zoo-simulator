// SPDX-License-Identifier: MIT
#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <tuple>
#include <utility>
#include <vector>

#include "CnaEngine/Graphics/Image.hpp"
#include "CnaEngine/Graphics/ModelView.hpp"

#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"

namespace zoogame {

/// Shares immutable, fully uploaded mip chains across animal models on a desktop device.
/// The index keeps weak handles so it cannot extend a model's GPU lifetime.
class AnimalTextureCache {
public:
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> Upload(
        Microsoft::Xna::Framework::Graphics::GraphicsDevice& device, const CnaEngine::Graphics::Image& image,
        const CnaEngine::Graphics::MipOptions& options);
    /// Cumulative avoided uploads and their uncompressed RGBA8 bytes at all mip levels.
    std::size_t SharedCount() const { return sharedCount_; }
    std::size_t SharedBytes() const { return sharedBytes_; }

private:
    using Texture = Microsoft::Xna::Framework::Graphics::Texture2D;
    using Key = std::tuple<Microsoft::Xna::Framework::Graphics::GraphicsDevice*, int, int,
                           CnaEngine::Graphics::MipRounding, bool, std::optional<int>,
                           std::vector<std::uint8_t>>;
    struct Entry {
        decltype(std::declval<const Texture&>().GetRendererWeak()) renderer;
        int levels = 0;
        std::size_t bytes = 0;
    };
    std::map<Key, Entry> images_;
    std::size_t sharedCount_ = 0;
    std::size_t sharedBytes_ = 0;
};

/// CNA imports the images of a glTF file with one level only, so a coat seen
/// from a few metres is minified without filtering: it sparkles as the animal
/// moves and every texel fetch misses the cache. This gives every texture of
/// the view's materials (base colour, normal, occlusion-roughness-metallic) a
/// full mip chain. Where every use asks for the same filtering, replaces the
/// resource inside the model-owned texture wrapper, releasing the unused original.
/// Otherwise returns separate variants, which the caller keeps alive as long as
/// the view. The model must outlive the view as usual.
/// Atlases larger than 1024 texels on either side start at their 1K mip: at the game's
/// output size the extra level is not visible, while its decoded pixels are expensive.
/// `discardCpuMipCopies` is only for a caller which has deliberately disabled context recovery
/// and will sample, but never read back, these static textures after this call.
std::vector<std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D>> GiveMipmaps(
    CnaEngine::Graphics::ModelView& view, bool discardCpuMipCopies = false, AnimalTextureCache* cache = nullptr);

/// The same operation on material slots (also usable without an imported model).
std::vector<std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D>> GiveMaterialMipmaps(
    std::span<CnaEngine::Graphics::Material* const> materials, bool discardCpuMipCopies = false,
    AnimalTextureCache* cache = nullptr);

}  // namespace zoogame
