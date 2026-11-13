// SPDX-License-Identifier: MIT
#include "Surfaces.hpp"

#include <algorithm>
#include <array>
#include <cmath>

#include "CnaEngine/Core/Noise.hpp"
#include "CnaEngine/Graphics/Image.hpp"

namespace zoogame {

namespace {

namespace Noise = CnaEngine::Noise;
using CnaEngine::Graphics::Image;
using CnaEngine::Graphics::Material;
using Microsoft::Xna::Framework::Graphics::SamplerState;

constexpr int kSize = 128;

/// Value noise that tiles with separate periods along x and y, in [0, 1).
float Tiled(float x, float y, int periodX, int periodY, std::uint32_t seed) {
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

/// Several octaves of Tiled over the unit square (u, v in 0..1), in about [0, 1].
float Cloud(float u, float v, int cellsX, int cellsY, int octaves, std::uint32_t seed) {
    float sum = 0.0f;
    float weight = 0.0f;
    float amplitude = 1.0f;
    for (int o = 0; o < octaves; ++o) {
        const int px = cellsX << o;
        const int py = cellsY << o;
        sum += amplitude * Tiled(u * static_cast<float>(px), v * static_cast<float>(py), px, py,
                                 seed + static_cast<std::uint32_t>(o) * 131u);
        weight += amplitude;
        amplitude *= 0.5f;
    }
    return sum / weight;
}

/// The brightness (linear, 0..1) of a pattern at (u, v) in 0..1.
float Shade(Pattern pattern, float u, float v) {
    switch (pattern) {
        case Pattern::Plain:
        case Pattern::WireMesh:
        case Pattern::Glass:
        case Pattern::Count:
            return 1.0f;
        case Pattern::Wood: {
            const float grain = Cloud(u, v, 24, 2, 2, 11);
            const float figure = Cloud(u, v, 4, 2, 2, 12);
            return 0.72f + 0.2f * grain + 0.08f * figure;
        }
        case Pattern::Planks: {
            const float boards = 4.0f;
            const float across = u * boards;
            const int board = static_cast<int>(std::floor(across)) % 4;
            const float within = across - std::floor(across);
            const float gap = std::min(within, 1.0f - within) < 0.035f ? 0.45f : 1.0f;
            const float tone = 0.82f + 0.18f * Noise::Random2(board, 0, 21);
            const float grain = Cloud(u, v, 32, 2, 2, 22 + static_cast<std::uint32_t>(board));
            return gap * tone * (0.8f + 0.2f * grain);
        }
        case Pattern::Bark: {
            const float ridges = Cloud(u, v, 12, 3, 3, 31);
            const float ridge = 1.0f - std::abs(ridges * 2.0f - 1.0f);
            return 0.5f + 0.5f * ridge;
        }
        case Pattern::Leaves: {
            const float clumps = Cloud(u, v, 6, 6, 3, 41);
            const float specks = Cloud(u, v, 32, 32, 1, 42);
            return std::clamp(0.5f + 0.55f * clumps + 0.18f * (specks - 0.5f), 0.0f, 1.0f);
        }
        case Pattern::Needles: {
            const float clumps = Cloud(u, v, 5, 5, 2, 51);
            const float streaks = Cloud(u, v, 48, 6, 1, 52);
            return std::clamp(0.45f + 0.4f * clumps + 0.25f * streaks, 0.0f, 1.0f);
        }
        case Pattern::Stone: {
            const float blotches = Cloud(u, v, 4, 4, 4, 61);
            const float cracks = Cloud(u, v, 7, 7, 2, 62);
            const float crack = std::abs(cracks - 0.5f) < 0.02f ? 0.7f : 1.0f;
            return (0.68f + 0.32f * blotches) * crack;
        }
        case Pattern::Brick: {
            const float courses = 8.0f;
            const float row = v * courses;
            const int course = static_cast<int>(std::floor(row));
            const float along = u * 4.0f + (course % 2 == 0 ? 0.0f : 0.5f);
            const int brick = static_cast<int>(std::floor(along)) & 3;
            const float fx = along - std::floor(along);
            const float fy = row - std::floor(row);
            const bool mortar = fx < 0.04f || fx > 0.96f || fy < 0.09f || fy > 0.91f;
            if (mortar) {
                return 0.93f;
            }
            const float tone = 0.62f + 0.3f * Noise::Random2(brick, course, 71);
            return tone * (0.92f + 0.08f * Cloud(u, v, 32, 32, 1, 72));
        }
        case Pattern::Plaster:
            return 0.9f + 0.1f * Cloud(u, v, 8, 8, 3, 81);
        case Pattern::Thatch: {
            const float straws = Cloud(u, v, 40, 3, 2, 91);
            const float layers = v * 3.0f - std::floor(v * 3.0f);
            return (0.58f + 0.42f * straws) * (0.8f + 0.2f * layers);
        }
        case Pattern::RoofTiles: {
            const float rows = 4.0f;
            const float row = v * rows;
            const int course = static_cast<int>(std::floor(row));
            const float fy = row - std::floor(row);
            const float along = u * 6.0f + (course % 2 == 0 ? 0.0f : 0.5f);
            const int tile = static_cast<int>(std::floor(along)) % 6;
            const float fx = along - std::floor(along);
            const float joint = fx < 0.05f ? 0.75f : 1.0f;
            const float lap = 0.72f + 0.28f * fy;  // each course is shaded by the one above it
            return joint * lap * (0.88f + 0.12f * Noise::Random2(tile, course, 101));
        }
        case Pattern::Metal:
            return 0.9f + 0.1f * Cloud(u, v, 2, 24, 2, 111);
        case Pattern::Concrete: {
            const float grain = Cloud(u, v, 24, 24, 2, 121);
            const float stains = Cloud(u, v, 3, 3, 2, 122);
            return 0.8f + 0.1f * grain + 0.1f * stains;
        }
    }
    return 1.0f;
}

float Roughness(Pattern pattern) {
    switch (pattern) {
        case Pattern::Metal:
            return 0.4f;
        case Pattern::RoofTiles:
        case Pattern::Leaves:
            return 0.7f;
        case Pattern::Wood:
        case Pattern::Planks:
        case Pattern::Plain:
            return 0.78f;
        default:
            return 0.92f;
    }
}

constexpr int kRippleFrames = 48;       ///< frames of the loop
constexpr double kRippleSeconds = 4.0;  ///< length of the loop
constexpr int kRippleSize = 128;        ///< texels along an edge
constexpr float kRippleMetres = 4.0f;   ///< edge that one repeat covers

/// One frame of a pond in a light breeze as a tangent-space normal map: a sum
/// of small waves from all round, the short ones steeper and quicker. Every
/// wave fits the tile a whole number of times and passes a whole number of
/// times in the loop, so the picture repeats in space and in time without a seam.
CnaEngine::Graphics::Image RippleFrame(int frame) {
    struct Wave {
        int kx, kz;       ///< crests along each edge of the tile
        int passes;       ///< times it passes in the loop
        float slope;      ///< steepest slope it gives the surface
        float phase;
    };
    static const std::array<Wave, 14> waves = [] {
        std::array<Wave, 14> made{};
        // The breeze comes from one side: the waves run within a sixth of a turn of it, which
        // draws the ripples out across it as on a real pond.
        const int crests[14][2] = {{3, 1},  {4, -1}, {6, 2},   {7, -3},  {9, 1},  {10, -4}, {12, 5},
                                   {13, -2}, {15, 3}, {17, -6}, {19, 7}, {21, -1}, {23, -8}, {26, 5}};
        for (std::size_t i = 0; i < made.size(); ++i) {
            const float k = std::sqrt(static_cast<float>(crests[i][0] * crests[i][0] + crests[i][1] * crests[i][1]));
            made[i].kx = crests[i][0];
            made[i].kz = crests[i][1];
            made[i].passes = std::max(1, static_cast<int>(std::lround(0.9f * std::sqrt(k))));
            made[i].slope = 0.018f + 0.0016f * k;
            made[i].phase = 2.399963f * static_cast<float>(i * i + 1);
        }
        return made;
    }();
    constexpr float kTurn = 6.2831853f;
    CnaEngine::Graphics::Image image(kRippleSize, kRippleSize);
    const float time = static_cast<float>(frame) / static_cast<float>(kRippleFrames);
    for (int y = 0; y < kRippleSize; ++y) {
        for (int x = 0; x < kRippleSize; ++x) {
            float slopeX = 0.0f;
            float slopeZ = 0.0f;
            for (const Wave& wave : waves) {
                const float angle = kTurn * (static_cast<float>(wave.kx * x + wave.kz * y) / static_cast<float>(kRippleSize) -
                                             static_cast<float>(wave.passes) * time) +
                                    wave.phase;
                const float k = std::sqrt(static_cast<float>(wave.kx * wave.kx + wave.kz * wave.kz));
                const float c = std::cos(angle) * wave.slope / k;
                slopeX += c * static_cast<float>(wave.kx);
                slopeZ += c * static_cast<float>(wave.kz);
            }
            const float length = std::sqrt(slopeX * slopeX + slopeZ * slopeZ + 1.0f);
            const auto byte = [](float v) { return static_cast<int>(std::clamp(v * 0.5f + 0.5f, 0.0f, 1.0f) * 255.0f + 0.5f); };
            image.Set(x, y, Color(byte(-slopeX / length), byte(-slopeZ / length), byte(1.0f / length), 255));
        }
    }
    return image;
}

std::uint32_t Key(const zoo::Color3& color, std::uint32_t extra) {
    return (static_cast<std::uint32_t>(SrgbByte(color[0])) << 24) |
           (static_cast<std::uint32_t>(SrgbByte(color[1])) << 16) |
           (static_cast<std::uint32_t>(SrgbByte(color[2])) << 8) | extra;
}

}  // namespace

float Surfaces::PatternMetres(Pattern pattern) {
    switch (pattern) {
        case Pattern::Wood:
        case Pattern::Planks:
            return 0.8f;
        case Pattern::Bark:
        case Pattern::Metal:
            return 1.0f;
        case Pattern::Leaves:
        case Pattern::Stone:
            return 2.5f;
        case Pattern::Needles:
        case Pattern::Plaster:
        case Pattern::Brick:
            return 2.0f;
        case Pattern::Thatch:
        case Pattern::RoofTiles:
            return 1.2f;
        case Pattern::Concrete:
            return 3.0f;
        case Pattern::WireMesh:
            return 0.6f;
        default:
            return 1.0f;
    }
}

Surfaces::Surfaces(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device) {
    for (std::size_t p = 0; p < patterns_.size(); ++p) {
        const auto pattern = static_cast<Pattern>(p);
        const bool flat =
            pattern == Pattern::Plain || pattern == Pattern::WireMesh || pattern == Pattern::Glass;
        const int size = flat ? 4 : kSize;
        Image image(size, size);
        for (int y = 0; y < size; ++y) {
            for (int x = 0; x < size; ++x) {
                const float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(size);
                const float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(size);
                const std::uint8_t grey = SrgbByte(Shade(pattern, u, v));
                image.Set(x, y, Color(grey, grey, grey, std::uint8_t{255}));
            }
        }
        patterns_[p] = CnaEngine::Graphics::UploadTexture(device, image, {.srgb = true});
    }

    // Chain-link: two families of diagonal wires, see-through between them.
    {
        const int size = 64;
        Image image(size, size,
                    Color(std::uint8_t{255}, std::uint8_t{255}, std::uint8_t{255}, std::uint8_t{0}));
        for (int y = 0; y < size; ++y) {
            for (int x = 0; x < size; ++x) {
                const int a = (x + y) % 16;
                const int b = ((x - y) % 16 + 16) % 16;
                if (a < 2 || b < 2) {
                    image.Set(
                        x, y,
                        Color(std::uint8_t{255}, std::uint8_t{255}, std::uint8_t{255}, std::uint8_t{255}));
                }
            }
        }
        wire_ = CnaEngine::Graphics::UploadTexture(device, image, {.srgb = true});
    }

    glass_.name = "glass";
    glass_.shading = CnaEngine::Graphics::ShadingModel::PhysicallyBased;
    glass_.blend = CnaEngine::Graphics::BlendMode::AlphaBlend;
    glass_.diffuseColor = Vector3(0.55f, 0.75f, 0.8f);
    glass_.alpha = 0.28f;
    glass_.roughness = 0.08f;
    glass_.doubleSided = true;
    glass_.baseTexture = patterns_[0].get();

    water_.name = "water";
    water_.shading = CnaEngine::Graphics::ShadingModel::PhysicallyBased;
    water_.blend = CnaEngine::Graphics::BlendMode::AlphaBlend;
    water_.diffuseColor = Vector3(0.05f, 0.2f, 0.26f);
    water_.alpha = 0.78f;
    water_.roughness = 0.1f;
    water_.baseTexture = patterns_[0].get();
    // Ripples (only where the device takes the sky's reflection they are worth their memory:
    // a flat sheet lit by a flat ambient colour shows nothing of them).
    ripples_.reserve(kRippleFrames);
    for (int frame = 0; frame < kRippleFrames; ++frame)
        ripples_.push_back(CnaEngine::Graphics::UploadTexture(device, RippleFrame(frame), {}, true));
    water_.normalTexture = ripples_.front().get();
    water_.sampler = &SamplerState::AnisotropicWrap;
    water_.uvScale = Vector2(1.0f / kRippleMetres, 1.0f / kRippleMetres);

    // Marks glow in their own colour, whatever the light.
    const Vector3 colors[] = {Vector3(0.15f, 0.9f, 0.3f), Vector3(1.0f, 0.12f, 0.08f),
                              Vector3(0.25f, 0.6f, 1.0f), Vector3(1.0f, 0.85f, 0.2f)};
    for (std::size_t i = 0; i < marks_.size(); ++i) {
        auto& mark = marks_[i];
        mark.name = "mark";
        mark.shading = CnaEngine::Graphics::ShadingModel::Unlit;
        mark.blend = CnaEngine::Graphics::BlendMode::AlphaBlend;
        mark.diffuseColor = colors[i];
        mark.alpha = 0.42f;
        mark.doubleSided = true;
        mark.baseTexture = patterns_[0].get();
    }
}

void Surfaces::Animate(double seconds) {
    if (ripples_.empty())
        return;
    rippleTime_ = std::fmod(rippleTime_ + seconds, kRippleSeconds);
    const auto frame = static_cast<std::size_t>(rippleTime_ / kRippleSeconds * static_cast<double>(kRippleFrames));
    water_.normalTexture = ripples_[std::min(frame, ripples_.size() - 1)].get();
}

const Material& Surfaces::Get(const Finish& finish) {
    if (finish.pattern == Pattern::WireMesh) {
        return WireMesh(finish.color);
    }
    if (finish.pattern == Pattern::Glass) {
        return glass_;
    }
    const std::uint32_t key = Key(finish.color, static_cast<std::uint32_t>(finish.pattern));
    auto& slot = opaque_[key];
    if (!slot) {
        slot = std::make_unique<Material>();
        slot->name = "surface";
        slot->shading = CnaEngine::Graphics::ShadingModel::PhysicallyBased;
        slot->diffuseColor = ToVector(finish.color);
        slot->roughness = Roughness(finish.pattern);
        slot->baseTexture = patterns_[static_cast<std::size_t>(finish.pattern)].get();
        slot->sampler = &SamplerState::AnisotropicWrap;
        const float scale = 1.0f / PatternMetres(finish.pattern);
        slot->uvScale = Vector2(scale, scale);
    }
    return *slot;
}

const Material& Surfaces::WireMesh(const zoo::Color3& color) {
    auto& slot = meshes_[Key(color, 0)];
    if (!slot) {
        slot = std::make_unique<Material>();
        slot->name = "wire mesh";
        slot->shading = CnaEngine::Graphics::ShadingModel::PhysicallyBased;
        slot->blend = CnaEngine::Graphics::BlendMode::AlphaBlend;
        slot->diffuseColor = ToVector(color);
        slot->roughness = 0.5f;
        slot->doubleSided = true;
        slot->baseTexture = wire_.get();
        slot->sampler = &SamplerState::AnisotropicWrap;
        const float scale = 1.0f / PatternMetres(Pattern::WireMesh);  // four wire cells per repeat
        slot->uvScale = Vector2(scale, scale);
    }
    return *slot;
}

}  // namespace zoogame
