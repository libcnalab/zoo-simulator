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
    int size = 0;                    ///< texels along an edge of the whole image
    int cells = 0;                   ///< sprays along an edge (0: pictures of several sizes, see RectOf)
    std::vector<std::uint8_t> rgba;  ///< sRGB bytes, row-major, the top row first
};

/// Broadleaf sprays: a branching twig with some three hundred leaves a cell.
SprayAtlas LeafSprays();
/// Conifer boughs: a stem with side twigs, all lined with needles.
SprayAtlas NeedleSprays();

/// The pictures of the plants that are not crowns of sprays, all in one atlas (Pattern::Fronds).
/// Each has its foot at the middle of its lower edge. The two fronds are stored square
/// but belong on cards some two and a half times as long as wide.
enum class PlantCard : std::uint8_t {
    PalmFrond,  ///< a feather palm's frond, stalk and all
    FernFrond,
    GrassA,  ///< a tuft of grass
    GrassB,
    ReedsA,  ///< reeds with bulrush cobs
    ReedsB,
    BambooA,  ///< a twig of bamboo with its fans of narrow leaves
    BambooB,
    Daisies,  ///< flower heads, white: the material gives the petals their colour
    Umbels,   ///< the same in domes of small florets
    Count
};

/// Where a picture lies in its texture; (u0, v0) is its top left.
struct CardRect {
    float u0, v0, u1, v1;
};

SprayAtlas PlantCards();
CardRect RectOf(PlantCard card);

}  // namespace zoogame
