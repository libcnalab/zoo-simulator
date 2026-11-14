// SPDX-License-Identifier: MIT
#include "BarrierTextures.hpp"

#include <algorithm>
#include <cmath>

namespace zoogame {

namespace {

float Random(int x, int y, std::uint32_t seed) {
    std::uint32_t h = static_cast<std::uint32_t>(x) * 374761393u +
                      static_cast<std::uint32_t>(y) * 668265263u + seed * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return static_cast<float>((h ^ (h >> 16)) & 0xFFFFu) / 65535.0f;
}

/// Smooth noise in 0..1 with features one unit across.
float Smooth(float x, float y, std::uint32_t seed) {
    const int xi = static_cast<int>(std::floor(x));
    const int yi = static_cast<int>(std::floor(y));
    float fx = x - static_cast<float>(xi);
    float fy = y - static_cast<float>(yi);
    fx = fx * fx * (3.0f - 2.0f * fx);
    fy = fy * fy * (3.0f - 2.0f * fy);
    const float top = std::lerp(Random(xi, yi, seed), Random(xi + 1, yi, seed), fx);
    const float bottom = std::lerp(Random(xi, yi + 1, seed), Random(xi + 1, yi + 1, seed), fx);
    return std::lerp(top, bottom, fy);
}

}  // namespace

BarrierImage ViewingPane() {
    BarrierImage image;
    image.width = 128;
    image.height = 256;
    image.rgba.resize(static_cast<std::size_t>(image.width * image.height * 4));
    for (int y = 0; y < image.height; ++y) {
        for (int x = 0; x < image.width; ++x) {
            const float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(image.width);
            const float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(image.height);
            const float up = 1.0f - v;  // 0 at the foot
            // The glass itself: a breath of colour.
            float cover = 0.035f;
            // What the wiper left: long faint streaks leaning a little, stronger in bands.
            const float lean = u * 14.0f + v * 3.0f;
            const float streak = Smooth(lean, v * 2.5f, 3) * Smooth(u * 2.0f, v * 3.0f, 4);
            cover += 0.11f * streak * streak;
            // Hands and noses, between a child's and a grown-up's height.
            const float reachBand = std::exp(-std::pow((up - 0.3f) / 0.11f, 2.0f));
            const float smear = std::max(0.0f, Smooth(u * 7.0f, v * 16.0f, 5) - 0.55f);
            cover += 0.5f * reachBand * smear;
            // Dust and splashes along the foot, highest where the rain throws the most.
            const float splash = 0.05f + 0.07f * Smooth(u * 9.0f, 0.0f, 6);
            const float dust = std::clamp(1.0f - up / splash, 0.0f, 1.0f);
            cover += 0.24f * dust * (0.6f + 0.4f * Smooth(u * 40.0f, v * 90.0f, 7));
            // A faint bloom along the upper edge, where nobody cleans.
            cover += 0.1f * std::clamp(1.0f - v / 0.04f, 0.0f, 1.0f);
            cover = std::clamp(cover, 0.0f, 0.6f);
            // Pale, a touch green where it is clean and dull where it is dirty.
            const float dirt = std::clamp(dust + reachBand * smear * 2.0f, 0.0f, 1.0f);
            std::uint8_t* texel = &image.rgba[static_cast<std::size_t>((y * image.width + x) * 4)];
            texel[0] = static_cast<std::uint8_t>(std::lround(std::lerp(214.0f, 198.0f, dirt)));
            texel[1] = static_cast<std::uint8_t>(std::lround(std::lerp(232.0f, 196.0f, dirt)));
            texel[2] = static_cast<std::uint8_t>(std::lround(std::lerp(228.0f, 186.0f, dirt)));
            texel[3] = static_cast<std::uint8_t>(std::lround(cover * 255.0f));
        }
    }
    return image;
}

BarrierImage WeldedMesh() {
    BarrierImage image;
    image.width = 128;
    image.height = 128;
    image.rgba.assign(static_cast<std::size_t>(image.width * image.height * 4), 255);
    constexpr int kPeriod = 16;
    const auto wire = [](int along) {
        // Distance in texels from the wire's axis, which runs through the cell boundary.
        const int wrapped = along % kPeriod;
        const float off = std::min(static_cast<float>(wrapped), static_cast<float>(kPeriod - wrapped));
        return std::clamp(1.2f - off, 0.0f, 1.0f);
    };
    for (int y = 0; y < image.height; ++y) {
        for (int x = 0; x < image.width; ++x) {
            const float cover = std::max(wire(x), wire(y));
            image.rgba[static_cast<std::size_t>((y * image.width + x) * 4 + 3)] =
                static_cast<std::uint8_t>(std::lround(cover * 255.0f));
        }
    }
    return image;
}

}  // namespace zoogame
