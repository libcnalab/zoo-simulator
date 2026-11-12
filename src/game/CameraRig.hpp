// SPDX-License-Identifier: MIT
#pragma once

#include "CnaEngine/Graphics/Camera.hpp"

#include "Common.hpp"
#include "Controls.hpp"
#include "zoo/world/World.hpp"

namespace zoogame {

/// The player's three ways of looking at the zoo.
enum class CameraMode {
    Overview,  ///< management view: orbiting a point on the ground
    Walk,      ///< on foot, at a visitor's eye height
    Follow,    ///< orbiting an animal (or anything else that moves)
};

class CameraRig {
public:
    CameraMode Mode() const { return mode_; }
    void SetMode(CameraMode mode, const zoo::World& world);

    /// Places the overview: `yaw` is the compass heading the camera looks along
    /// (0 = +z/south, pi = north), `pitch` the angle below the horizon.
    void SetOverview(zoo::Vec2 focus, float yaw, float pitch, float distance);
    /// Glides the overview to a point (alerts, "go to animal").
    void GoTo(zoo::Vec2 focus);
    /// The thing Follow mode orbits; call every frame while in that mode.
    void SetFollowTarget(const Vector3& position, float size);

    void Update(const Controls& controls, float seconds, const zoo::World& world);
    void Apply(CnaEngine::Graphics::Camera& camera) const;

    /// What the camera looks at (overview: the ground point; walk: ahead).
    Vector3 Target() const { return target_; }
    Vector3 Position() const { return position_; }
    float Yaw() const { return yaw_; }
    float Distance() const { return distance_; }
    /// The world-space direction through a pixel.
    Vector3 PixelDirection(const CnaEngine::Graphics::Camera& camera, Vector2 pixel, Vector2 viewport) const;

private:
    void UpdateOverview(const Controls& controls, float seconds, const zoo::World& world);
    void UpdateWalk(const Controls& controls, float seconds, const zoo::World& world);
    void UpdateFollow(const Controls& controls, float seconds, const zoo::World& world);
    void Orbit(const Controls& controls, float seconds, float minDistance, float maxDistance);
    void Place(const Vector3& around, const zoo::World& world);

    CameraMode mode_ = CameraMode::Overview;
    // Wanted values (input moves these) and shown values (which glide after them).
    zoo::Vec2 focusGoal_{64.0f, 64.0f};
    zoo::Vec2 focus_{64.0f, 64.0f};
    float focusHeight_ = 0.0f;
    float yawGoal_ = 3.14159265f;
    float yaw_ = 3.14159265f;
    float pitchGoal_ = 0.85f;
    float pitch_ = 0.85f;
    float distanceGoal_ = 60.0f;
    float distance_ = 60.0f;
    // Walk.
    zoo::Vec2 feet_{0.0f, 0.0f};
    float walkYaw_ = 3.14159265f;
    float walkPitch_ = 0.0f;
    // Follow.
    Vector3 followTarget_ = Vector3::Zero;
    float followSize_ = 1.0f;

    Vector3 position_ = Vector3(0.0f, 30.0f, 0.0f);
    Vector3 target_ = Vector3::Zero;
};

}  // namespace zoogame
