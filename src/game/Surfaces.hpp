// SPDX-License-Identifier: MIT
#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <vector>

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
/// The see-through colours the tools draw over the world.
enum class Mark : std::uint8_t {
    Good,    ///< what would be built
    Bad,     ///< where it cannot be
    Note,    ///< an area a tool covers
    Select,  ///< what is selected
    Count
};

class Surfaces {
public:
    explicit Surfaces(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device);

    /// The material of a finish (opaque but for Pattern::WireMesh, Pattern::Glass and the
    /// cut-out sprays of foliage). The reference stays valid for the lifetime of this object.
    const CnaEngine::Graphics::Material& Get(const Finish& finish);
    /// See-through wire mesh in the given colour (texture coordinates in metres).
    const CnaEngine::Graphics::Material& WireMesh(const zoo::Color3& color);
    const CnaEngine::Graphics::Material& Glass() const { return glass_; }
    const CnaEngine::Graphics::Material& Water() const { return water_; }
    const CnaEngine::Graphics::Material& Marker(Mark mark) const {
        return marks_[static_cast<std::size_t>(mark)];
    }

    /// Moves the water on by `seconds`: the ripples are a short loop of normal
    /// maps, and the water's material shows the one that is due.
    void Animate(double seconds);

    /// Releases CPU copies of the immutable procedural textures after their GPU upload.
    /// Use only where context recovery has deliberately been disabled.
    void ReleaseCpuTextureCopies();

    /// Edge length in metres that one repeat of a pattern covers.
    static float PatternMetres(Pattern pattern);
    /// How light a pattern is on the whole (linear, 0..1): what its texture comes to where
    /// it is too far away to make out, as a factor on the material's colour.
    static float PatternBrightness(Pattern pattern);

private:
    using Texture = Microsoft::Xna::Framework::Graphics::Texture2D;

    /// Pattern::Signs, Pattern::Pane and Pattern::WeldMesh: pictures of their own.
    const CnaEngine::Graphics::Material& Built(const Finish& finish);

    std::array<std::unique_ptr<Texture>, static_cast<std::size_t>(Pattern::Count)> patterns_;
    std::unique_ptr<Texture> wire_;
    std::unique_ptr<Texture> leafSpray_;
    std::unique_ptr<Texture> needleSpray_;
    std::unique_ptr<Texture> signs_;
    std::unique_ptr<Texture> pane_;
    std::unique_ptr<Texture> weld_;
    std::unique_ptr<Texture> fronds_;
    std::vector<std::unique_ptr<Texture>> ripples_;  ///< one loop of the water's surface
    double rippleTime_ = 0.0;
    std::map<std::uint32_t, std::unique_ptr<CnaEngine::Graphics::Material>> opaque_;
    std::map<std::uint32_t, std::unique_ptr<CnaEngine::Graphics::Material>> meshes_;
    std::map<std::uint32_t, std::unique_ptr<CnaEngine::Graphics::Material>> sprays_;
    std::map<std::uint32_t, std::unique_ptr<CnaEngine::Graphics::Material>> built_;
    CnaEngine::Graphics::Material glass_;
    CnaEngine::Graphics::Material water_;
    std::array<CnaEngine::Graphics::Material, static_cast<std::size_t>(Mark::Count)> marks_;
};

}  // namespace zoogame
