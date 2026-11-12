// SPDX-License-Identifier: MIT
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

/// Small maths vocabulary of the simulation. The simulation lives on the ground
/// plane: a position is (x, z) in metres, heights are separate values (y is up
/// in the renderer). A heading is an angle in radians about the vertical axis
/// with heading 0 looking along +z and heading pi/2 looking along +x, so the
/// forward vector is (sin h, cos h).
namespace zoo {

inline constexpr float kPi = 3.14159265358979323846f;
inline constexpr float kTwoPi = 2.0f * kPi;

struct Vec2 {
    float x = 0.0f;
    float z = 0.0f;

    constexpr Vec2 operator+(Vec2 o) const { return {x + o.x, z + o.z}; }
    constexpr Vec2 operator-(Vec2 o) const { return {x - o.x, z - o.z}; }
    constexpr Vec2 operator*(float s) const { return {x * s, z * s}; }
    constexpr Vec2 operator/(float s) const { return {x / s, z / s}; }
    constexpr Vec2& operator+=(Vec2 o) { x += o.x; z += o.z; return *this; }
    constexpr Vec2& operator-=(Vec2 o) { x -= o.x; z -= o.z; return *this; }
    constexpr bool operator==(const Vec2&) const = default;
};

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    constexpr Vec3 operator+(Vec3 o) const { return {x + o.x, y + o.y, z + o.z}; }
    constexpr Vec3 operator-(Vec3 o) const { return {x - o.x, y - o.y, z - o.z}; }
    constexpr Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
    constexpr bool operator==(const Vec3&) const = default;
};

constexpr float Dot(Vec2 a, Vec2 b) { return a.x * b.x + a.z * b.z; }
constexpr float Dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
constexpr Vec3 Cross(Vec3 a, Vec3 b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
inline float Length(Vec2 v) { return std::sqrt(Dot(v, v)); }
inline float Length(Vec3 v) { return std::sqrt(Dot(v, v)); }
constexpr float LengthSquared(Vec2 v) { return Dot(v, v); }
inline float Distance(Vec2 a, Vec2 b) { return Length(a - b); }
constexpr float DistanceSquared(Vec2 a, Vec2 b) { return LengthSquared(a - b); }

/// Unit vector, or `fallback` for a (nearly) zero vector.
inline Vec2 Normalized(Vec2 v, Vec2 fallback = {0.0f, 1.0f}) {
    const float len = Length(v);
    return len > 1e-6f ? v / len : fallback;
}
inline Vec3 Normalized(Vec3 v, Vec3 fallback = {0.0f, 1.0f, 0.0f}) {
    const float len = Length(v);
    return len > 1e-6f ? v * (1.0f / len) : fallback;
}

constexpr float Lerp(float a, float b, float t) { return a + (b - a) * t; }
constexpr Vec2 Lerp(Vec2 a, Vec2 b, float t) { return a + (b - a) * t; }
constexpr float Clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

/// Forward vector of a heading (see the namespace comment).
inline Vec2 HeadingVector(float heading) { return {std::sin(heading), std::cos(heading)}; }
/// Heading that looks along `direction`.
inline float HeadingOf(Vec2 direction) { return std::atan2(direction.x, direction.z); }

/// Wraps an angle into (-pi, pi].
inline float WrapAngle(float a) {
    a = std::fmod(a + kPi, kTwoPi);
    if (a <= 0.0f) {
        a += kTwoPi;
    }
    return a - kPi;
}

/// Turns `from` towards `to` by at most `maxStep` radians, the short way round.
inline float TurnTowards(float from, float to, float maxStep) {
    const float delta = WrapAngle(to - from);
    return WrapAngle(from + std::clamp(delta, -maxStep, maxStep));
}

/// Integer tile coordinate on the world grid.
struct TileCoord {
    int x = 0;
    int z = 0;

    constexpr TileCoord operator+(TileCoord o) const { return {x + o.x, z + o.z}; }
    constexpr TileCoord operator-(TileCoord o) const { return {x - o.x, z - o.z}; }
    constexpr bool operator==(const TileCoord&) const = default;
};

/// The four axis directions on the grid. North is -z (away from the entrance), east is +x.
enum class Dir : std::uint8_t { North = 0, East = 1, South = 2, West = 3 };

constexpr TileCoord Step(Dir d) {
    switch (d) {
        case Dir::North: return {0, -1};
        case Dir::East: return {1, 0};
        case Dir::South: return {0, 1};
        case Dir::West: return {-1, 0};
    }
    return {0, 0};
}
constexpr Dir Opposite(Dir d) { return static_cast<Dir>((static_cast<int>(d) + 2) % 4); }
constexpr Dir RotatedClockwise(Dir d, int quarterTurns = 1) {
    return static_cast<Dir>(((static_cast<int>(d) + quarterTurns) % 4 + 4) % 4);
}

}  // namespace zoo
