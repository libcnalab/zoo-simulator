// SPDX-License-Identifier: MIT
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Matrix.hpp"
#include "Microsoft/Xna/Framework/Vector2.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"

#include "zoo/base/Math.hpp"
#include "zoo/build/Catalog.hpp"

/// The game layer: everything that draws, sounds or reads input. The simulation
/// (namespace zoo, src/core) knows nothing of it.
namespace zoogame {

using Microsoft::Xna::Framework::Color;
using Microsoft::Xna::Framework::Matrix;
using Microsoft::Xna::Framework::Vector2;
using Microsoft::Xna::Framework::Vector3;

inline Vector3 ToXna(zoo::Vec3 v) {
    return Vector3(v.x, v.y, v.z);
}
inline zoo::Vec3 FromXna(const Vector3& v) {
    return {v.X, v.Y, v.Z};
}
inline Vector3 ToVector(const zoo::Color3& c) {
    return Vector3(c[0], c[1], c[2]);
}

/// Linear 0..1 to an sRGB byte.
inline std::uint8_t SrgbByte(float linear) {
    const float c = std::clamp(linear, 0.0f, 1.0f);
    const float s = c <= 0.0031308f ? c * 12.92f : 1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f;
    return static_cast<std::uint8_t>(std::lround(s * 255.0f));
}
inline Color SrgbColor(const zoo::Color3& linear, float alpha = 1.0f) {
    return Color(SrgbByte(linear[0]), SrgbByte(linear[1]), SrgbByte(linear[2]),
                 static_cast<std::uint8_t>(std::lround(std::clamp(alpha, 0.0f, 1.0f) * 255.0f)));
}

inline zoo::Color3 Scaled(const zoo::Color3& c, float s) {
    return {c[0] * s, c[1] * s, c[2] * s};
}
inline zoo::Color3 Mix(const zoo::Color3& a, const zoo::Color3& b, float t) {
    return {a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t, a[2] + (b[2] - a[2]) * t};
}

/// World matrix of something standing at `position` (y up), turned to a
/// simulation heading (0 looks along +z) and scaled uniformly.
inline Matrix Placement(const Vector3& position, float heading, float scale = 1.0f) {
    return Matrix::CreateScale(scale) * Matrix::CreateRotationY(heading) *
           Matrix::CreateTranslation(position);
}

}  // namespace zoogame
