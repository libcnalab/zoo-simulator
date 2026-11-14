// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <vector>

namespace zoogame {

/// A picture with coverage: sRGB bytes and alpha, row-major, the top row first.
struct BarrierImage {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> rgba;
};

/// One pane of a viewing window, the whole pane in the picture (the top row is the pane's top).
/// Glass that is perfectly clear is not seen at all and glass of one even tint is a sheet of
/// plastic: this one is nearly clear where one looks through it, with the streaks the wiper
/// left, smears at the height of hands and noses, and dust and splashes along its foot.
BarrierImage ViewingPane();

/// Welded mesh: straight wires crossing at right angles, see-through between them. It tiles;
/// eight cells along each edge.
BarrierImage WeldedMesh();

}  // namespace zoogame
