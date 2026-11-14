// SPDX-License-Identifier: MIT
#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <memory>

#include "CnaEngine/Graphics/Image.hpp"
#include "CnaEngine/Graphics/Material.hpp"
#include "CnaEngine/Graphics/MeshData.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"

#include "Common.hpp"
#include "FoliageTextures.hpp"
#include "MeshWriter.hpp"

namespace zoogame {

/// The finishes of plants seen from far away, all in one texture.
///
/// A draw call costs as much as six thousand triangles here (docs/PERFORMANCE.md), and a
/// grove of mixed plants has some twenty finishes: bark, the inside of each crown, the
/// sprays of each kind. Far away nothing of a pattern can be made out but its colour, so
/// the far form of a grove is two meshes whatever grows in it: what is solid, in a material
/// whose texture holds a swatch of every solid finish's colour, and what is cut out of
/// cards, in one whose texture holds every card picture in miniature and already tinted.
/// Both are the same texture.
class PlantPalette {
public:
    explicit PlantPalette(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device);

    /// Adds a model's parts to the two meshes, their texture coordinates moved to where the
    /// part's finish lies in the palette (which is allotted on first use; see Upload).
    void Merge(const ModelBuilder& model, CnaEngine::Graphics::MeshData& solid,
               CnaEngine::Graphics::MeshData& cards);
    /// Brings the texture up to date with what Merge allotted. Call before the meshes are
    /// drawn; costs nothing when nothing is new.
    void Upload();

    /// The reference stays valid for the lifetime of this object.
    const CnaEngine::Graphics::Material& Solid() const { return solid_; }
    const CnaEngine::Graphics::Material& Cards() const { return cards_; }

private:
    static constexpr int kSize = 1024;    ///< texels along an edge of the texture
    static constexpr int kPicture = 128;  ///< edge of a card atlas in miniature
    static constexpr int kSwatch = 32;    ///< edge of a solid finish's swatch
    /// Pictures fill the texture from the top, swatches the lowest rows from the bottom.
    static constexpr int kSwatchRows = 4;

    CardRect Place(const Finish& finish);
    const SprayAtlas& Miniature(Pattern pattern);

    CnaEngine::Graphics::Image image_;
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> texture_;
    CnaEngine::Graphics::Material solid_;
    CnaEngine::Graphics::Material cards_;
    std::map<std::uint32_t, CardRect> placed_;
    std::map<Pattern, SprayAtlas> miniatures_;
    std::array<float, static_cast<std::size_t>(Pattern::Count)> brightness_{};
    int pictures_ = 0;
    int swatches_ = 0;
    bool dirty_ = false;
};

}  // namespace zoogame
