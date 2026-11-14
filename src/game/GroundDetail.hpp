// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <vector>

#include "CnaEngine/Graphics/Image.hpp"

#include "GroundPaint.hpp"

namespace zoogame {

/// The detail layer of the ground: what is finer than the baked colour's 6 cm a
/// texel. A few small repeating textures, laid over the ground near the eye as a
/// blended second pass. A texel is either a light or a dark tone and its alpha
/// says how strongly, so the pass lightens here and darkens there and leaves the
/// ground's own colour alone on average.
///
/// The textures repeat every kGroundDetailPeriodM metres (4 mm a texel).
inline constexpr float kGroundDetailPeriodM = 4.0f;
inline constexpr int kGroundDetailTexels = 1024;
/// The first mip level that is fully transparent, and so are all below it: its
/// texels are as coarse as the baked colour's, nothing is left to add there.
inline constexpr int kGroundDetailFadedLevel = 4;

enum class GroundDetailTexture : std::uint8_t { Grain, Blades, Slabs };
inline constexpr int kGroundDetailTextureCount = 3;

/// How one kind of ground wears the layer: which texture, the colour its
/// white is tinted to (the light tone; the dark tone is a fraction of it), and
/// how many times the texture repeats in kGroundDetailPeriodM. The tint is
/// lighter than the ground's own colour, so that light and dark weigh about
/// the same.
struct GroundDetailLook {
    GroundDetailTexture texture = GroundDetailTexture::Grain;
    zoo::Color3 tint{1.0f, 1.0f, 1.0f};
    float repeats = 1.0f;
};
GroundDetailLook LookOf(GroundDetail detail);

/// Paints one detail texture with all its mip levels, largest first (sRGB
/// bytes, straight alpha). The alpha fades out down the levels: from the level
/// whose texels are as coarse as the baked colour's the layer is fully
/// transparent, so it adds only what the ground texture cannot hold, never
/// shimmers in the distance and needs no fading by distance. Only computes;
/// safe on any thread.
std::vector<CnaEngine::Graphics::Image> PaintGroundDetail(GroundDetailTexture texture);

}  // namespace zoogame
