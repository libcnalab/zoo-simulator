// SPDX-License-Identifier: MIT
#include "PlantPalette.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

#include "Microsoft/Xna/Framework/Graphics/SamplerState.hpp"

#include "Surfaces.hpp"

namespace zoogame {

namespace {

using CnaEngine::Graphics::Material;
using CnaEngine::Graphics::MeshData;
using Microsoft::Xna::Framework::Graphics::SamplerState;

bool CutOut(Pattern pattern) {
    return pattern == Pattern::LeafSpray || pattern == Pattern::NeedleSpray || pattern == Pattern::Fronds;
}

float Linear(std::uint8_t srgb) {
    const float s = static_cast<float>(srgb) / 255.0f;
    return s <= 0.04045f ? s / 12.92f : std::pow((s + 0.055f) / 1.055f, 2.4f);
}

/// The share of texels an alpha test at one half lets through.
float Cover(const std::vector<float>& alpha, float gain) {
    std::size_t passing = 0;
    for (const float a : alpha) {
        passing += a * gain > 0.5f ? 1u : 0u;
    }
    return static_cast<float>(passing) / static_cast<float>(alpha.size());
}

/// The atlas at `size` texels along an edge, colours averaged in linear light. Averaging
/// thins leaves out (a leaf two texels wide becomes a haze that fails the alpha test), so
/// the alpha is raised until as much of the picture is leaf as in the original.
SprayAtlas Shrunk(const SprayAtlas& atlas, int size) {
    const int box = atlas.size / size;
    std::vector<float> alpha(static_cast<std::size_t>(size * size));
    SprayAtlas small;
    small.size = size;
    small.cells = atlas.cells;
    small.rgba.resize(alpha.size() * 4u);
    std::size_t solid = 0;
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            float sum[3] = {0.0f, 0.0f, 0.0f};
            float cover = 0.0f;
            for (int j = 0; j < box; ++j) {
                for (int i = 0; i < box; ++i) {
                    const std::size_t at =
                        (static_cast<std::size_t>(y * box + j) * static_cast<std::size_t>(atlas.size) +
                         static_cast<std::size_t>(x * box + i)) *
                        4u;
                    for (std::size_t c = 0; c < 3; ++c) {
                        sum[c] += Linear(atlas.rgba[at + c]);
                    }
                    cover += atlas.rgba[at + 3] > 127 ? 1.0f : 0.0f;
                    solid += atlas.rgba[at + 3] > 127 ? 1u : 0u;
                }
            }
            const std::size_t to = static_cast<std::size_t>(y * size + x);
            const float texels = static_cast<float>(box * box);
            for (std::size_t c = 0; c < 3; ++c) {
                small.rgba[to * 4u + c] = SrgbByte(sum[c] / texels);
            }
            alpha[to] = cover / texels;
        }
    }
    const float wanted = static_cast<float>(solid) / static_cast<float>(atlas.size * atlas.size);
    // Coverage is discrete; the upper end can overshoot badly for a sparse miniature.
    float low = 0.0f, high = 16.0f, best = 1.0f;
    float error = std::abs(Cover(alpha, best) - wanted);
    for (int step = 0; step < 16; ++step) {
        const float gain = (low + high) * 0.5f;
        const float coverage = Cover(alpha, gain);
        const float difference = std::abs(coverage - wanted);
        if (difference < error) {
            best = gain;
            error = difference;
        }
        (coverage < wanted ? low : high) = gain;
    }
    for (std::size_t i = 0; i < alpha.size(); ++i) {
        small.rgba[i * 4u + 3] =
            static_cast<std::uint8_t>(std::lround(std::clamp(alpha[i] * best, 0.0f, 1.0f) * 255.0f));
    }
    return small;
}

std::uint32_t Key(const Finish& finish) {
    return (static_cast<std::uint32_t>(SrgbByte(finish.color[0])) << 24) |
           (static_cast<std::uint32_t>(SrgbByte(finish.color[1])) << 16) |
           (static_cast<std::uint32_t>(SrgbByte(finish.color[2])) << 8) |
           static_cast<std::uint32_t>(finish.pattern);
}

}  // namespace

PlantPalette::PlantPalette(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device)
    : image_(kSize, kSize, Color(std::uint8_t{90}, std::uint8_t{110}, std::uint8_t{70}, std::uint8_t{0})) {
    brightness_.fill(-1.0f);
    texture_ = CnaEngine::Graphics::UploadTexture(device, image_, {.srgb = true, .alphaTestReference = 127});
    solid_.name = "far plants";
    solid_.shading = CnaEngine::Graphics::ShadingModel::PhysicallyBased;
    solid_.roughness = 0.9f;
    solid_.baseTexture = texture_.get();
    solid_.sampler = &SamplerState::AnisotropicClamp;
    cards_ = solid_;
    cards_.name = "far foliage";
    cards_.blend = CnaEngine::Graphics::BlendMode::AlphaTest;
    cards_.alphaCutoff = 0.5f;
    cards_.roughness = 0.72f;
}

const SprayAtlas& PlantPalette::Miniature(Pattern pattern) {
    auto found = miniatures_.find(pattern);
    if (found == miniatures_.end()) {
        const SprayAtlas atlas = pattern == Pattern::LeafSpray     ? LeafSprays()
                                 : pattern == Pattern::NeedleSpray ? NeedleSprays()
                                                                   : PlantCards();
        found = miniatures_.emplace(pattern, Shrunk(atlas, kPicture)).first;
    }
    return found->second;
}

CardRect PlantPalette::Place(const Finish& finish) {
    const std::uint32_t key = Key(finish);
    if (const auto found = placed_.find(key); found != placed_.end()) {
        return found->second;
    }
    const float texel = 1.0f / static_cast<float>(kSize);
    CardRect rect{};
    if (CutOut(finish.pattern)) {
        // When the texture is full, a new finish shows as the first of its kind: a wrong
        // green far away rather than nothing.
        const int perRow = kSize / kPicture;
        const int room = perRow * ((kSize - kSwatchRows * kSwatch) / kPicture);
        const int slot = pictures_ < room ? pictures_++ : 0;
        const int x0 = (slot % perRow) * kPicture, y0 = (slot / perRow) * kPicture;
        if (slot == pictures_ - 1) {
            const SprayAtlas& small = Miniature(finish.pattern);
            for (int y = 0; y < kPicture; ++y) {
                for (int x = 0; x < kPicture; ++x) {
                    const std::size_t from = static_cast<std::size_t>(y * kPicture + x) * 4u;
                    image_.Set(x0 + x, y0 + y,
                               Color(SrgbByte(Linear(small.rgba[from]) * finish.color[0]),
                                     SrgbByte(Linear(small.rgba[from + 1]) * finish.color[1]),
                                     SrgbByte(Linear(small.rgba[from + 2]) * finish.color[2]),
                                     small.rgba[from + 3]));
                }
            }
            dirty_ = true;
        }
        rect = {static_cast<float>(x0) * texel, static_cast<float>(y0) * texel,
                static_cast<float>(x0 + kPicture) * texel, static_cast<float>(y0 + kPicture) * texel};
    } else {
        const int perRow = kSize / kSwatch;
        const int slot = swatches_ < perRow * kSwatchRows ? swatches_++ : 0;
        const int x0 = (slot % perRow) * kSwatch, y0 = kSize - (1 + slot / perRow) * kSwatch;
        if (slot == swatches_ - 1) {
            // What a pattern comes to once it is too small to make out: its mean brightness.
            float& mean = brightness_[static_cast<std::size_t>(finish.pattern)];
            if (mean < 0.0f) {
                mean = Surfaces::PatternBrightness(finish.pattern);
            }
            const Color color = SrgbColor(Scaled(finish.color, mean));
            for (int y = 0; y < kSwatch; ++y) {
                for (int x = 0; x < kSwatch; ++x) {
                    image_.Set(x0 + x, y0 + y, color);
                }
            }
            dirty_ = true;
        }
        // Every corner of a solid face reads the middle of its swatch.
        const float u = (static_cast<float>(x0) + 0.5f * kSwatch) * texel;
        const float v = (static_cast<float>(y0) + 0.5f * kSwatch) * texel;
        rect = {u, v, u, v};
    }
    placed_.emplace(key, rect);
    return rect;
}

void PlantPalette::Merge(const ModelBuilder& model, MeshData& solid, MeshData& cards) {
    for (const ModelPart& part : model.Parts()) {
        if (part.writer.Empty()) {
            continue;
        }
        const CardRect rect = Place(part.finish);
        MeshData& into = CutOut(part.finish.pattern) ? cards : solid;
        const std::size_t first = into.texCoords0.size();
        into.Append(part.writer.mesh);
        for (std::size_t i = first; i < into.texCoords0.size(); ++i) {
            Vector2& uv = into.texCoords0[i];
            uv = Vector2(rect.u0 + std::clamp(uv.X, 0.0f, 1.0f) * (rect.u1 - rect.u0),
                         rect.v0 + std::clamp(uv.Y, 0.0f, 1.0f) * (rect.v1 - rect.v0));
        }
    }
}

void PlantPalette::Upload() {
    if (dirty_) {
        CnaEngine::Graphics::UpdateTexture(*texture_, image_, {.srgb = true, .alphaTestReference = 127});
        dirty_ = false;
    }
}

}  // namespace zoogame
