// SPDX-License-Identifier: MIT
#include "SkyLight.hpp"

#include <vector>

#include "CnaEngine/Graphics/Image.hpp"
#include "Microsoft/Xna/Framework/Graphics/CubeMapFace.hpp"
#include "Microsoft/Xna/Framework/Graphics/SurfaceFormat.hpp"

namespace zoogame {

namespace {

using Microsoft::Xna::Framework::Graphics::CubeMapFace;
using Microsoft::Xna::Framework::Graphics::SurfaceFormat;
using Microsoft::Xna::Framework::Graphics::TextureCube;

constexpr float kPi = 3.14159265f;
constexpr int kHeights = 65;  ///< entries of a table over the height of a direction

float Halton(int index, int base) {
    float result = 0.0f;
    float fraction = 1.0f / static_cast<float>(base);
    for (int i = index; i > 0; i /= base) {
        result += fraction * static_cast<float>(i % base);
        fraction /= static_cast<float>(base);
    }
    return result;
}

/// What a lobe around every height sees: the average of `radiance` over
/// directions spread around the axis as cos^power (power 1 is what a matt
/// surface gathers).
template <typename Radiance>
std::array<Vector3, kHeights> Gather(const Radiance& radiance, float power) {
    constexpr int kSamples = 192;
    std::array<Vector3, kHeights> table;
    for (int i = 0; i < kHeights; ++i) {
        const float y = -1.0f + 2.0f * static_cast<float>(i) / static_cast<float>(kHeights - 1);
        const float side = std::sqrt(std::max(0.0f, 1.0f - y * y));
        // The axis is (side, y, 0); (-y, side, 0) and (0, 0, 1) complete its frame, and only
        // the height of a sample matters.
        Vector3 sum(0.0f, 0.0f, 0.0f);
        for (int s = 0; s < kSamples; ++s) {
            const float u = (static_cast<float>(s) + 0.5f) / static_cast<float>(kSamples);
            const float cosine = std::pow(u, 1.0f / (power + 1.0f));
            const float sine = std::sqrt(std::max(0.0f, 1.0f - cosine * cosine));
            const float around = 2.0f * kPi * Halton(s + 1, 2);
            const float height = cosine * y + sine * std::cos(around) * side;
            sum += radiance(std::clamp(height, -1.0f, 1.0f));
        }
        table[static_cast<std::size_t>(i)] = sum * (1.0f / static_cast<float>(kSamples));
    }
    return table;
}

Vector3 At(const std::array<Vector3, kHeights>& table, float y) {
    const float at = std::clamp((y + 1.0f) * 0.5f, 0.0f, 1.0f) * static_cast<float>(kHeights - 1);
    const int low = std::min(static_cast<int>(at), kHeights - 2);
    const float part = at - static_cast<float>(low);
    return table[static_cast<std::size_t>(low)] * (1.0f - part) + table[static_cast<std::size_t>(low) + 1] * part;
}

int Byte(float value) {
    return static_cast<int>(std::clamp(value, 0.0f, 1.0f) * 255.0f + 0.5f);
}

/// Fills one level of a cube from a table over the height. The faces are laid
/// out as OpenGL and Direct3D both have them: on the four side faces the
/// first row is the top.
void Fill(TextureCube& cube, int level, int size, const std::array<Vector3, kHeights>& table) {
    std::vector<Color> texels(static_cast<std::size_t>(size) * static_cast<std::size_t>(size));
    const CubeMapFace faces[] = {CubeMapFace::PositiveX, CubeMapFace::NegativeX, CubeMapFace::PositiveY,
                                 CubeMapFace::NegativeY, CubeMapFace::PositiveZ, CubeMapFace::NegativeZ};
    for (int face = 0; face < 6; ++face) {
        for (int row = 0; row < size; ++row) {
            for (int column = 0; column < size; ++column) {
                const float u = 2.0f * (static_cast<float>(column) + 0.5f) / static_cast<float>(size) - 1.0f;
                const float v = 2.0f * (static_cast<float>(row) + 0.5f) / static_cast<float>(size) - 1.0f;
                // Only the height of the direction is needed.
                const float length = std::sqrt(1.0f + u * u + v * v);
                const float y = face == 2 ? 1.0f / length : face == 3 ? -1.0f / length : -v / length;
                const Vector3 color = At(table, y);
                texels[static_cast<std::size_t>(row) * static_cast<std::size_t>(size) +
                       static_cast<std::size_t>(column)] = Color(Byte(color.X), Byte(color.Y), Byte(color.Z), 255);
            }
        }
        cube.SetData(faces[face], level, nullptr, texels.data(), 0, static_cast<int>(texels.size()));
    }
}

/// The table of the split sum: how much of a reflection survives, by the
/// cosine of the view (across) and the roughness (down); red scales the
/// surface's own reflectance, green is added.
CnaEngine::Graphics::Image SplitSumTable(int size) {
    CnaEngine::Graphics::Image image(size, size);
    constexpr int kSamples = 128;
    for (int row = 0; row < size; ++row) {
        const float roughness = (static_cast<float>(row) + 0.5f) / static_cast<float>(size);
        const float alpha = roughness * roughness;
        for (int column = 0; column < size; ++column) {
            const float nv = std::max((static_cast<float>(column) + 0.5f) / static_cast<float>(size), 1.0e-3f);
            const float vx = std::sqrt(1.0f - nv * nv);
            float scale = 0.0f;
            float bias = 0.0f;
            for (int s = 0; s < kSamples; ++s) {
                const float e1 = (static_cast<float>(s) + 0.5f) / static_cast<float>(kSamples);
                const float e2 = Halton(s + 1, 2);
                const float cosTheta = std::sqrt((1.0f - e1) / (1.0f + (alpha * alpha - 1.0f) * e1));
                const float sinTheta = std::sqrt(std::max(0.0f, 1.0f - cosTheta * cosTheta));
                const float hx = sinTheta * std::cos(2.0f * kPi * e2);
                const float hz = cosTheta;
                const float vh = vx * hx + nv * hz;
                const float lz = 2.0f * vh * hz - nv;
                if (lz <= 0.0f || vh <= 0.0f)
                    continue;
                const float k = alpha * 0.5f;
                const float visible = (lz / (lz * (1.0f - k) + k)) * (nv / (nv * (1.0f - k) + k)) * vh /
                                      (std::max(hz, 1.0e-4f) * nv);
                const float fresnel = std::pow(1.0f - vh, 5.0f);
                scale += (1.0f - fresnel) * visible;
                bias += fresnel * visible;
            }
            image.Set(column, row,
                      Color(Byte(scale / static_cast<float>(kSamples)), Byte(bias / static_cast<float>(kSamples)), 0,
                            255));
        }
    }
    return image;
}

float Luminance(const Vector3& color) {
    return 0.2126f * color.X + 0.7152f * color.Y + 0.0722f * color.Z;
}

}  // namespace

Vector3 SkyLight::Radiance(const CnaEngine::Graphics::SkyColors& colors, const Vector3& directionToSun, float y) {
    const float sun = std::max(directionToSun.Y, 0.0f);
    if (y >= 0.0f) {
        // The dome's own gradient: most of the change is close to the horizon.
        const float up = std::pow(y, 0.45f);
        const Vector3 sky = colors.horizon * (1.0f - up) + colors.zenith * up;
        // A zoo is planted all round: what stands on the horizon is trees, hedges and roofs,
        // not open sky. Water and glass seen at a flat angle reflect that band, which is why
        // a pond is dark at its far bank and not the colour of the sky.
        const Vector3 foliage(0.11f, 0.16f, 0.07f);
        const Vector3 trees = foliage * (colors.sunLight * (0.55f * sun) + colors.skyFill * 0.6f + colors.ambient);
        const float t = std::clamp((y - 0.10f) / (0.36f - 0.10f), 0.0f, 1.0f);
        const float open = t * t * (3.0f - 2.0f * t);
        return trees * (1.0f - open) + sky * open;
    }
    // The ground: lawns, sand and paths in the sun and under the sky.
    const Vector3 ground(0.26f, 0.29f, 0.17f);
    return ground * (colors.sunLight * sun + colors.skyFill * 0.6f + colors.ambient);
}

SkyLight::SkyLight(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device) {
    if (!device.SupportsImageBasedLightingEXT())
        return;
    irradiance_ = std::make_unique<TextureCube>(device, kIrradianceSize, false, SurfaceFormat::Color);
    specular_ = std::make_unique<TextureCube>(device, kSpecularSize, true, SurfaceFormat::Color);
    table_ = CnaEngine::Graphics::UploadTexture(device, SplitSumTable(32), {}, false);
}

bool SkyLight::Update(const CnaEngine::Graphics::SkyColors& colors, const Vector3& directionToSun) {
    if (!specular_)
        return false;
    const std::array<float, 16> now{colors.zenith.X,  colors.zenith.Y,  colors.zenith.Z,   colors.horizon.X,
                                    colors.horizon.Y, colors.horizon.Z, colors.sunLight.X, colors.sunLight.Y,
                                    colors.sunLight.Z, colors.ambient.X, colors.ambient.Y,  colors.ambient.Z,
                                    colors.skyFill.X, colors.skyFill.Y, colors.skyFill.Z,
                                    std::max(directionToSun.Y, 0.0f)};
    bool changed = !any_;
    for (std::size_t i = 0; i < now.size(); ++i)
        changed = changed || std::abs(now[i] - made_[i]) > 0.008f;
    if (!changed)
        return false;
    made_ = now;
    any_ = true;

    const auto radiance = [&](float y) { return Radiance(colors, directionToSun, y); };
    // What a matt surface gathers. The scene's lights were tuned with a flat ambient colour
    // and a fill from straight above, and both stay: a surface facing up keeps exactly the
    // ambient it had, and the environment only says how much less reaches a wall and the
    // underside of a belly, and with which tint (half of it: the tuned colour keeps the rest,
    // or everything in the shade turns blue).
    std::array<Vector3, kHeights> matt = Gather(radiance, 1.0f);
    const float top = std::max(Luminance(At(matt, 1.0f)), 1.0e-3f);
    const float ambient = Luminance(colors.ambient);
    for (Vector3& entry : matt) {
        const float share = Luminance(entry) / top;
        const Vector3 tinted = entry * (ambient / top);
        entry = tinted * 0.5f + colors.ambient * (0.5f * share);
    }
    Fill(*irradiance_, 0, kIrradianceSize, matt);

    // Reflections, blurred more with every level; the roughness of a level is its share of
    // the way down, and the lobe a rough surface reflects from is about cos^(2/a^2 - 2) wide
    // with a the roughness squared.
    for (int mip = 0; mip < kSpecularLevels; ++mip) {
        const float roughness = static_cast<float>(mip) / static_cast<float>(kSpecularLevels - 1);
        const float alpha = std::max(roughness * roughness, 0.02f);
        const float power = std::clamp(2.0f / (alpha * alpha) - 2.0f, 1.0f, 4000.0f);
        std::array<Vector3, kHeights> table;
        if (mip == 0) {
            for (int i = 0; i < kHeights; ++i)
                table[static_cast<std::size_t>(i)] =
                    radiance(-1.0f + 2.0f * static_cast<float>(i) / static_cast<float>(kHeights - 1));
        } else {
            table = Gather(radiance, power);
        }
        Fill(*specular_, mip, kSpecularSize >> mip, table);
    }

    light_.Irradiance = irradiance_.get();
    light_.PrefilteredSpecular = specular_.get();
    light_.BrdfLut = table_.get();
    light_.PrefilteredMipCount = kSpecularLevels;
    light_.Intensity = 1.0f;
    return true;
}

}  // namespace zoogame
