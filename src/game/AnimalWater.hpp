// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <vector>

namespace zoogame {

/// A small transparent picture placed on the water below a moving animal. The
/// three soft rings read as displaced water rather than a solid selection mark;
/// the renderer stretches the picture along the animal's heading into a wake.
struct AnimalWaterImage {
    int size = 0;
    std::vector<std::uint8_t> rgba;
};

AnimalWaterImage MakeAnimalWaterImage();

}  // namespace zoogame
