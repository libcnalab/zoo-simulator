// SPDX-License-Identifier: MIT
#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <memory>

#include "CnaEngine/Graphics/Material.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"

#include "Common.hpp"
#include "MeshWriter.hpp"

namespace zoogame {

/// The materials of everything built procedurally: a small set of tileable
/// pattern textures generated at start-up, tinted per material.
///
/// Everything is physically based so that terrain, buildings, plants and the
/// imported animal models share one lighting model.
class Surfaces {
public:
    explicit Surfaces(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device);

    /// The material of a finish (opaque but for Pattern::WireMesh and
    /// Pattern::Glass). The reference stays valid for the lifetime of this object.
    const CnaEngine::Graphics::Material& Get(const Finish& finish);
    /// See-through wire mesh in the given colour (texture coordinates in metres).
    const CnaEngine::Graphics::Material& WireMesh(const zoo::Color3& color);
    const CnaEngine::Graphics::Material& Glass() const { return glass_; }
    const CnaEngine::Graphics::Material& Water() const { return water_; }

    /// Edge length in metres that one repeat of a pattern covers.
    static float PatternMetres(Pattern pattern);

private:
    using Texture = Microsoft::Xna::Framework::Graphics::Texture2D;

    std::array<std::unique_ptr<Texture>, static_cast<std::size_t>(Pattern::Count)> patterns_;
    std::unique_ptr<Texture> wire_;
    std::map<std::uint32_t, std::unique_ptr<CnaEngine::Graphics::Material>> opaque_;
    std::map<std::uint32_t, std::unique_ptr<CnaEngine::Graphics::Material>> meshes_;
    CnaEngine::Graphics::Material glass_;
    CnaEngine::Graphics::Material water_;
};

}  // namespace zoogame
