// SPDX-License-Identifier: MIT
#pragma once

#include <memory>
#include <span>
#include <vector>

#include "CnaEngine/Graphics/ModelView.hpp"

#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"

namespace zoogame {

/// CNA imports the images of a glTF file with one level only, so a coat seen
/// from a few metres is minified without filtering: it sparkles as the animal
/// moves and every texel fetch misses the cache. This gives every texture of
/// the view's materials (base colour, normal, occlusion-roughness-metallic) a
/// full mip chain. Where every use asks for the same filtering, replaces the
/// resource inside the model-owned texture wrapper, releasing the unused original.
/// Otherwise returns separate variants, which the caller keeps alive as long as
/// the view. The model must outlive the view as usual.
std::vector<std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D>> GiveMipmaps(
    CnaEngine::Graphics::ModelView& view);

/// The same operation on material slots (also usable without an imported model).
std::vector<std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D>> GiveMaterialMipmaps(
    std::span<CnaEngine::Graphics::Material* const> materials);

}  // namespace zoogame
