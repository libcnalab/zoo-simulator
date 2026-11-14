// SPDX-License-Identifier: MIT
#include "GroundDetail.hpp"

#include <algorithm>
#include <cmath>

#include "CnaEngine/Core/Noise.hpp"

namespace zoogame {

namespace {

namespace Noise = CnaEngine::Noise;
using CnaEngine::Graphics::Image;

/// The two tones of a texture, before a material's tint (linear): the light
/// one, and the dark one of each texture (between blades of grass the eye meets
/// more grass, in a joint only shade).
constexpr float kLightTone = 1.0f;
constexpr float kDarkTone[kGroundDetailTextureCount] = {0.12f, 0.3f, 0.1f};

/// Value noise with its own period along each axis (in lattice cells), which
/// stretched patterns need to repeat with the texture.
float Stretched(float x, float y, int periodX, int periodY, std::uint32_t seed) {
    const int xi = static_cast<int>(std::floor(x));
    const int yi = static_cast<int>(std::floor(y));
    const float u = Noise::Fade(x - static_cast<float>(xi));
    const float v = Noise::Fade(y - static_cast<float>(yi));
    const int x0 = Noise::Wrap(xi, periodX);
    const int x1 = Noise::Wrap(xi + 1, periodX);
    const int y0 = Noise::Wrap(yi, periodY);
    const int y1 = Noise::Wrap(yi + 1, periodY);
    return Noise::Lerp(Noise::Lerp(Noise::Random2(x0, y0, seed), Noise::Random2(x1, y0, seed), u),
                       Noise::Lerp(Noise::Random2(x0, y1, seed), Noise::Random2(x1, y1, seed), u), v);
}

/// Whole lattice cells in one period at `cyclesPerMetre`.
int Cells(float cyclesPerMetre) {
    return static_cast<int>(std::lround(cyclesPerMetre * kGroundDetailPeriodM));
}

/// Salt and pepper of a few millimetres: sand, fines, the pores of a surface. -1 .. 1.
float Speckle(float x, float z, std::uint32_t seed) {
    const float fine = Noise::Value2(x * 120.0f, z * 120.0f, Cells(120.0f), seed);
    const float clump = Noise::Value2(x * 37.0f, z * 37.0f, Cells(37.0f), seed + 1);
    return (fine - 0.5f) * 1.5f + (clump - 0.5f) * 0.9f;
}

/// How much of a stone lies at a point: stones of up to `size` (in cells of
/// `cyclesPerMetre`), in some cells and not in others.
float Stone(float x, float z, float cyclesPerMetre, float size, std::uint32_t seed) {
    const float distance =
        Noise::Worley2(x * cyclesPerMetre, z * cyclesPerMetre, Cells(cyclesPerMetre), seed);
    const float here = Noise::Value2(x * cyclesPerMetre * 0.5f, z * cyclesPerMetre * 0.5f,
                                     Cells(cyclesPerMetre * 0.5f), seed + 1);
    const float radius = size * Noise::SmoothStep(0.35f, 0.9f, here);
    return 1.0f - Noise::SmoothStep(radius * 0.6f, radius + 0.001f, distance);
}

/// Stones lying on a surface: pale on top, a line of shade along one side.
float Stones(float x, float z, float cyclesPerMetre, float size, std::uint32_t seed) {
    const float stone = Stone(x, z, cyclesPerMetre, size, seed);
    const float offset = 0.3f * size / cyclesPerMetre;
    const float shade = std::max(0.0f, Stone(x - offset, z - offset, cyclesPerMetre, size, seed) - stone);
    return stone * 0.7f - shade * 0.9f;
}

/// Gravel, the aggregate of asphalt, coarse sand, crumbs of earth.
float Grain(float x, float z) {
    const float small = Stones(x, z, 60.0f, 0.42f, 711);
    const float large = Stones(x, z, 22.0f, 0.4f, 721);
    const float stones = std::abs(large) > std::abs(small) ? large : small;
    return Noise::Lerp(Speckle(x, z, 701) * 0.55f, stones, std::min(1.0f, std::abs(stones) * 4.0f));
}

/// Blades of grass: short strokes lying three ways, tousled in patches.
float Blades(float x, float z) {
    constexpr float kAcross = 150.0f;  // blades are a few millimetres wide
    constexpr float kAlong = 34.0f;    // and some centimetres long
    const float north = Stretched(x * kAcross, z * kAlong, Cells(kAcross), Cells(kAlong), 801);
    const float east = Stretched(x * kAlong, z * kAcross, Cells(kAlong), Cells(kAcross), 802);
    // Sheared by one: the strokes lie at 45 degrees and still repeat.
    const float slant =
        Stretched((x + z) * kAcross * 0.75f, z * kAlong, Cells(kAcross * 0.75f), Cells(kAlong), 803);
    const float lay = Noise::Value2(x * 3.0f, z * 3.0f, Cells(3.0f), 804);
    const float turn = Noise::Value2(x * 3.0f + 40.0f, z * 3.0f, Cells(3.0f), 805);
    const float blades = Noise::Lerp(Noise::Lerp(north, slant, Noise::SmoothStep(0.3f, 0.7f, lay)), east,
                                     Noise::SmoothStep(0.45f, 0.75f, turn) * 0.7f);
    // Between the tufts the eye looks down into shade.
    const float tuft = Noise::Value2(x * 22.0f, z * 22.0f, Cells(22.0f), 806);
    return (blades - 0.5f) * 1.7f + 0.08f - Noise::SmoothStep(0.55f, 0.9f, tuft) * 0.35f;
}

/// Paving slabs (GroundPaint's running bond): the joints, the worn arris beside
/// them and the slabs' exposed aggregate.
float Slabs(float x, float z) {
    const float row = std::floor(z / kPavingSlabM);
    const float shifted = x / kPavingSlabM + ((static_cast<int>(row) & 1) != 0 ? 0.5f : 0.0f);
    const float fx = shifted - std::floor(shifted);
    const float fz = z / kPavingSlabM - row;
    const float edge = std::min(std::min(fx, 1.0f - fx), std::min(fz, 1.0f - fz)) * kPavingSlabM;  // metres
    const float joint = 1.0f - Noise::SmoothStep(0.003f, 0.008f, edge);
    const float arris = (1.0f - Noise::SmoothStep(0.008f, 0.022f, edge)) * (1.0f - joint);
    const float aggregate = Speckle(x, z, 901);
    const float wear = Noise::Value2(x * 6.0f, z * 6.0f, Cells(6.0f), 903) - 0.5f;
    return Noise::Lerp(aggregate * 0.3f + wear * 0.16f + arris * 0.18f, -0.85f, joint);
}

/// One level: `light` and `dark` are how strongly each tone covers a texel.
Image ToImage(const std::vector<float>& light, const std::vector<float>& dark, int size, float fade,
              float darkTone) {
    Image image(size, size);
    auto& bytes = image.Bytes();
    for (std::size_t at = 0; at < light.size(); ++at) {
        const float cover = light[at] + dark[at];
        const float tone =
            cover > 1.0e-5f ? (light[at] * kLightTone + dark[at] * darkTone) / cover : kLightTone;
        const std::uint8_t byte = SrgbByte(tone);
        bytes[at * 4] = bytes[at * 4 + 1] = bytes[at * 4 + 2] = byte;
        bytes[at * 4 + 3] =
            static_cast<std::uint8_t>(std::lround(std::clamp(cover * fade, 0.0f, 1.0f) * 255.0f));
    }
    return image;
}

std::vector<float> Halved(const std::vector<float>& level, int size) {
    const int half = size / 2;
    std::vector<float> out(static_cast<std::size_t>(half) * static_cast<std::size_t>(half));
    for (int j = 0; j < half; ++j) {
        const float* row = level.data() + static_cast<std::size_t>(j) * 2u * static_cast<std::size_t>(size);
        for (int i = 0; i < half; ++i)
            out[static_cast<std::size_t>(j) * static_cast<std::size_t>(half) + static_cast<std::size_t>(i)] =
                (row[i * 2] + row[i * 2 + 1] + row[i * 2 + size] + row[i * 2 + size + 1]) * 0.25f;
    }
    return out;
}

}  // namespace

GroundDetailLook LookOf(GroundDetail detail) {
    switch (detail) {
        case GroundDetail::Grain:
            return {GroundDetailTexture::Grain, {0.85f, 0.8f, 0.7f}, 1.0f};
        case GroundDetail::Sand:
            return {GroundDetailTexture::Grain, {0.9f, 0.82f, 0.62f}, 2.0f};
        case GroundDetail::Soil:
            return {GroundDetailTexture::Grain, {0.42f, 0.34f, 0.26f}, 1.0f};
        case GroundDetail::Tar:
            return {GroundDetailTexture::Grain, {0.24f, 0.24f, 0.25f}, 1.0f};
        case GroundDetail::Turf:
            return {GroundDetailTexture::Blades, {0.27f, 0.46f, 0.11f}, 1.0f};
        case GroundDetail::Straw:
            return {GroundDetailTexture::Blades, {0.74f, 0.64f, 0.3f}, 1.0f};
        case GroundDetail::Paving:
            return {GroundDetailTexture::Slabs, {0.7f, 0.68f, 0.64f}, 1.0f};
        case GroundDetail::None:
            break;
    }
    return {};
}

std::vector<Image> PaintGroundDetail(GroundDetailTexture texture) {
    int size = kGroundDetailTexels;
    const auto count = static_cast<std::size_t>(size) * static_cast<std::size_t>(size);
    std::vector<float> light(count);
    std::vector<float> dark(count);
    const float step = kGroundDetailPeriodM / static_cast<float>(size);
    for (int j = 0; j < size; ++j) {
        const float z = (static_cast<float>(j) + 0.5f) * step;
        for (int i = 0; i < size; ++i) {
            const float x = (static_cast<float>(i) + 0.5f) * step;
            const float amount = std::clamp(texture == GroundDetailTexture::Grain    ? Grain(x, z)
                                            : texture == GroundDetailTexture::Blades ? Blades(x, z)
                                                                                     : Slabs(x, z),
                                            -1.0f, 1.0f);
            const std::size_t at =
                static_cast<std::size_t>(j) * static_cast<std::size_t>(size) + static_cast<std::size_t>(i);
            (amount >= 0.0f ? light : dark)[at] = std::abs(amount);
        }
    }
    constexpr float kFade[] = {1.0f, 1.0f, 0.8f, 0.4f, 0.0f};
    static_assert(std::size(kFade) == kGroundDetailFadedLevel + 1);
    std::vector<Image> levels;
    for (std::size_t level = 0;; ++level) {
        levels.push_back(ToImage(light, dark, size, kFade[std::min(level, std::size(kFade) - 1)],
                                 kDarkTone[static_cast<std::size_t>(texture)]));
        if (size == 1)
            break;
        light = Halved(light, size);
        dark = Halved(dark, size);
        size /= 2;
    }
    return levels;
}

}  // namespace zoogame
