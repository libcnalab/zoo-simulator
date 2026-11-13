// SPDX-License-Identifier: MIT
#pragma once

#include <array>
#include <memory>

#include "CnaEngine/Graphics/Sky.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Graphics/ImageBasedLightEXT.hpp"
#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"
#include "Microsoft/Xna/Framework/Graphics/TextureCube.hpp"

#include "Common.hpp"

namespace zoogame {

/// The light of the sky and of the ground under it, as the physically based
/// materials take it: from above the sky's blue, from the side the horizon's
/// haze, from below what the sunlit ground throws back, and in smooth
/// surfaces (water, glass) the sky's reflection. Made from the same colours
/// the sky dome is painted with, and made again as the day moves on.
///
/// The environment is the same all the way round (it depends on the height
/// of a direction only, not on its bearing): the sun's own glint is the key
/// light's business.
class SkyLight {
public:
    explicit SkyLight(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device);

    /// Brings the light up to the moment; cheap when little has changed.
    /// Returns whether it was made again.
    bool Update(const CnaEngine::Graphics::SkyColors& colors, const Vector3& directionToSun);

    /// What to hand to the material binder; not valid where the renderer
    /// cannot light from an environment (the flat ambient colour stays then).
    [[nodiscard]] const Microsoft::Xna::Framework::Graphics::ImageBasedLightEXT& Light() const { return light_; }

    /// The radiance of a direction at height `y` (-1 straight down, 1 straight
    /// up) for those colours, linear: the sky's gradient above the horizon,
    /// the lit ground below it.
    [[nodiscard]] static Vector3 Radiance(const CnaEngine::Graphics::SkyColors& colors, const Vector3& directionToSun,
                                          float y);

private:
    static constexpr int kSpecularSize = 32;
    static constexpr int kSpecularLevels = 6;
    static constexpr int kIrradianceSize = 8;

    std::unique_ptr<Microsoft::Xna::Framework::Graphics::TextureCube> irradiance_;
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::TextureCube> specular_;
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> table_;
    Microsoft::Xna::Framework::Graphics::ImageBasedLightEXT light_;
    std::array<float, 16> made_{};  ///< the colours the cubes were made from
    bool any_ = false;
};

}  // namespace zoogame
