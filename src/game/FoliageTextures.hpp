// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <vector>

namespace zoogame {

/// A picture of cut-out foliage: `cells` x `cells` different sprays in one square image,
/// each with its twig's foot at the middle of its cell's lower edge and a clear margin all
/// round (so that neighbours do not bleed into each other in the smaller mip levels).
///
/// The colours are those of a neutral leaf (near white; the material tints them) with the
/// differences that make foliage: darker leaves under lighter ones, some yellower, the twigs
/// brown. Alpha is 255 on a leaf and 0 between; the colour of the clear texels is that of the
/// leaves next to them, so that filtering does not darken the edges.
struct SprayAtlas {
    int size = 0;                     ///< texels along an edge of the whole image
    int cells = 0;                    ///< sprays along an edge
    std::vector<std::uint8_t> rgba;   ///< sRGB bytes, row-major, the top row first
};

/// Broadleaf sprays: a branching twig with some three hundred leaves a cell.
SprayAtlas LeafSprays();
/// Conifer boughs: a stem with side twigs, all lined with needles.
SprayAtlas NeedleSprays();

}  // namespace zoogame
