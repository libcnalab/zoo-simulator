// SPDX-License-Identifier: MIT
#pragma once

#include <memory>
#include <vector>

#include "CnaEngine/Graphics/ModelView.hpp"

#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"

namespace zoogame {

/// CNA imports the images of a glTF file with one level only, so a coat seen
/// from a few metres is minified without filtering: it sparkles as the animal
/// moves and every texel fetch misses the cache. This gives every texture of
/// the view's materials (base colour, normal, occlusion-roughness-metallic) a
/// full mip chain and points the materials at the new textures, which the
/// caller keeps alive as long as the view.
std::vector<std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D>> GiveMipmaps(
    CnaEngine::Graphics::ModelView& view);

}  // namespace zoogame
