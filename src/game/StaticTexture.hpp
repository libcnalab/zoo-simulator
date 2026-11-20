// SPDX-License-Identifier: MIT
#pragma once

#include <utility>

#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"

namespace zoogame {

/// Keeps an already-uploaded texture and its complete GPU mip chain, but releases every CPU
/// pixel copy. The wrapper keeps its address, so materials which point to it remain valid.
/// Use only for a static, sample-only texture after context recovery has been disabled.
inline bool DiscardCpuTextureCopies(
    Microsoft::Xna::Framework::Graphics::Texture2D& texture) {
    namespace Xna = Microsoft::Xna::Framework::Graphics;
    Xna::GraphicsDevice* const device = texture.getGraphicsDeviceProperty();
    auto renderer = texture.GetRendererWeak().lock();
    if (device == nullptr || !renderer)
        return false;
    texture = Xna::Texture2D::ReconstructFromCache(
        *device, texture.getWidthProperty(), texture.getHeightProperty(), texture.getFormatProperty(),
        texture.getLevelCountProperty(), std::move(renderer), {});
    return true;
}

}  // namespace zoogame
