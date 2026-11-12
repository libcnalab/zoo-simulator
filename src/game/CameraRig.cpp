// SPDX-License-Identifier: MIT
#include "CameraRig.hpp"

#include <algorithm>
#include <cmath>

namespace zoogame {

namespace {

constexpr float kPi = 3.14159265358979f;
constexpr float kEyeHeight = 1.65f;

float Glide(float from, float to, float seconds, float rate) {
    return to + (from - to) * std::exp(-rate * seconds);
}

float WrapAngle(float a) {
    while (a > kPi)
        a -= 2.0f * kPi;
    while (a < -kPi)
        a += 2.0f * kPi;
    return a;
}

Vector3 Direction(float yaw, float pitchDown) {
    return Vector3(std::sin(yaw) * std::cos(pitchDown), -std::sin(pitchDown),
                   std::cos(yaw) * std::cos(pitchDown));
}

float KeyAxis(const Controls& c, Keys negative, Keys positive) {
    return (c.Down(positive) ? 1.0f : 0.0f) - (c.Down(negative) ? 1.0f : 0.0f);
}

zoo::Vec2 ClampToWorld(zoo::Vec2 p, const zoo::World& world, float inset) {
    return {std::clamp(p.x, inset, world.SizeX() - inset), std::clamp(p.z, inset, world.SizeZ() - inset)};
}

}  // namespace

void CameraRig::SetMode(CameraMode mode, const zoo::World& world) {
    if (mode == mode_)
        return;
    if (mode == CameraMode::Walk) {
        // Step down where the overview was looking, facing the same way.
        feet_ = ClampToWorld(focus_, world, 1.0f);
        walkYaw_ = yaw_;
        walkPitch_ = 0.0f;
    } else if (mode == CameraMode::Overview && mode_ == CameraMode::Walk) {
        focusGoal_ = focus_ = feet_;
        yawGoal_ = yaw_ = walkYaw_;
    } else if (mode == CameraMode::Overview && mode_ == CameraMode::Follow) {
        focusGoal_ = focus_ = {followTarget_.X, followTarget_.Z};
        distanceGoal_ = std::max(distanceGoal_, 18.0f);
    } else if (mode == CameraMode::Follow) {
        distanceGoal_ = distance_ = std::clamp(distance_, 3.0f, 14.0f);
        pitchGoal_ = std::min(pitchGoal_, 0.5f);
    }
    mode_ = mode;
}

void CameraRig::SetOverview(zoo::Vec2 focus, float yaw, float pitch, float distance) {
    mode_ = CameraMode::Overview;
    focusGoal_ = focus_ = focus;
    yawGoal_ = yaw_ = yaw;
    pitchGoal_ = pitch_ = pitch;
    distanceGoal_ = distance_ = distance;
}

void CameraRig::GoTo(zoo::Vec2 focus) {
    focusGoal_ = focus;
}

void CameraRig::SetFollowTarget(const Vector3& position, float size) {
    followTarget_ = position;
    followSize_ = size;
}

void CameraRig::Update(const Controls& controls, float seconds, const zoo::World& world) {
    switch (mode_) {
        case CameraMode::Overview:
            UpdateOverview(controls, seconds, world);
            break;
        case CameraMode::Walk:
            UpdateWalk(controls, seconds, world);
            break;
        case CameraMode::Follow:
            UpdateFollow(controls, seconds, world);
            break;
    }
}

void CameraRig::Orbit(const Controls& controls, float seconds, float minDistance, float maxDistance) {
    if (controls.KeyboardFree()) {
        yawGoal_ += KeyAxis(controls, Keys::E, Keys::Q) * 1.6f * seconds;
        pitchGoal_ += KeyAxis(controls, Keys::F, Keys::R) * 0.9f * seconds;
    }
    if (controls.Down(MouseKey::Middle)) {
        const Vector2 move = controls.Drag(MouseKey::Middle);
        yawGoal_ -= move.X * 0.006f;
        pitchGoal_ += move.Y * 0.005f;
    }
    if (controls.PointerFree() && controls.Wheel() != 0.0f)
        distanceGoal_ *= std::pow(0.86f, controls.Wheel());
    pitchGoal_ = std::clamp(pitchGoal_, 0.12f, 1.5f);
    distanceGoal_ = std::clamp(distanceGoal_, minDistance, maxDistance);

    yaw_ += WrapAngle(yawGoal_ - yaw_) * (1.0f - std::exp(-14.0f * seconds));
    pitch_ = Glide(pitch_, pitchGoal_, seconds, 14.0f);
    distance_ = Glide(distance_, distanceGoal_, seconds, 10.0f);
}

void CameraRig::Place(const Vector3& around, const zoo::World& world) {
    target_ = around;
    position_ = around - Direction(yaw_, pitch_) * distance_;
    // Never under the ground (or the hills beyond the map's edge).
    const zoo::Vec2 below = ClampToWorld({position_.X, position_.Z}, world, 0.0f);
    position_.Y = std::max(position_.Y, world.HeightAt(below) + 1.2f);
}

void CameraRig::UpdateOverview(const Controls& controls, float seconds, const zoo::World& world) {
    Orbit(controls, seconds, 4.0f, 320.0f);

    const Vector2 ahead(std::sin(yaw_), std::cos(yaw_));
    const Vector2 right(-std::cos(yaw_), std::sin(yaw_));
    float moveRight = 0.0f;
    float moveAhead = 0.0f;
    if (controls.KeyboardFree()) {
        moveRight = KeyAxis(controls, Keys::A, Keys::D) + KeyAxis(controls, Keys::Left, Keys::Right);
        moveAhead = KeyAxis(controls, Keys::S, Keys::W) + KeyAxis(controls, Keys::Down, Keys::Up);
    }
    const float speed = (distance_ * 0.9f + 4.0f) * (controls.Shift() ? 2.5f : 1.0f);
    focusGoal_.x += (right.X * moveRight + ahead.X * moveAhead) * speed * seconds;
    focusGoal_.z += (right.Y * moveRight + ahead.Y * moveAhead) * speed * seconds;
    if (controls.Down(MouseKey::Right)) {
        // Dragging the ground: it follows the pointer.
        const Vector2 move = controls.Drag(MouseKey::Right);
        const float perPixel = distance_ * 0.0016f;
        focusGoal_.x -= (right.X * move.X - ahead.X * move.Y) * perPixel;
        focusGoal_.z -= (right.Y * move.X - ahead.Y * move.Y) * perPixel;
    }
    focusGoal_ = ClampToWorld(focusGoal_, world, 2.0f);
    focus_.x = Glide(focus_.x, focusGoal_.x, seconds, 12.0f);
    focus_.z = Glide(focus_.z, focusGoal_.z, seconds, 12.0f);
    focusHeight_ = Glide(focusHeight_, world.HeightAt(focus_), seconds, 6.0f);
    Place(Vector3(focus_.x, focusHeight_ + 1.0f, focus_.z), world);
}

void CameraRig::UpdateWalk(const Controls& controls, float seconds, const zoo::World& world) {
    if (controls.KeyboardFree())
        walkYaw_ += KeyAxis(controls, Keys::E, Keys::Q) * 1.8f * seconds;
    if (controls.Down(MouseKey::Right) || controls.Down(MouseKey::Middle)) {
        const Vector2 move =
            controls.Down(MouseKey::Right) ? controls.Drag(MouseKey::Right) : controls.Drag(MouseKey::Middle);
        walkYaw_ -= move.X * 0.005f;
        walkPitch_ = std::clamp(walkPitch_ + move.Y * 0.005f, -1.2f, 1.2f);
    }
    walkYaw_ = WrapAngle(walkYaw_);
    const Vector2 ahead(std::sin(walkYaw_), std::cos(walkYaw_));
    const Vector2 right(-std::cos(walkYaw_), std::sin(walkYaw_));
    float moveRight = 0.0f;
    float moveAhead = 0.0f;
    if (controls.KeyboardFree()) {
        moveRight = KeyAxis(controls, Keys::A, Keys::D) + KeyAxis(controls, Keys::Left, Keys::Right);
        moveAhead = KeyAxis(controls, Keys::S, Keys::W) + KeyAxis(controls, Keys::Down, Keys::Up);
    }
    const float speed = controls.Shift() ? 6.0f : 1.8f;
    feet_.x += (right.X * moveRight + ahead.X * moveAhead) * speed * seconds;
    feet_.z += (right.Y * moveRight + ahead.Y * moveAhead) * speed * seconds;
    feet_ = ClampToWorld(feet_, world, 0.5f);

    const float ground = std::max(world.HeightAt(feet_), world.HeightAt(feet_) + world.WaterDepthAt(feet_));
    focusHeight_ = Glide(focusHeight_, ground, seconds, 12.0f);
    position_ = Vector3(feet_.x, focusHeight_ + kEyeHeight, feet_.z);
    target_ = position_ + Direction(walkYaw_, walkPitch_) * 10.0f;
    focus_ = focusGoal_ = feet_;
    yaw_ = yawGoal_ = walkYaw_;
}

void CameraRig::UpdateFollow(const Controls& controls, float seconds, const zoo::World& world) {
    Orbit(controls, seconds, std::max(1.5f, followSize_ * 1.2f), 60.0f);
    focus_ = focusGoal_ = {followTarget_.X, followTarget_.Z};
    focusHeight_ = followTarget_.Y;
    Place(followTarget_ + Vector3(0.0f, followSize_ * 0.6f, 0.0f), world);
}

void CameraRig::Apply(CnaEngine::Graphics::Camera& camera) const {
    camera.SetPosition(position_);
    camera.LookAt(target_);
}

Vector3 CameraRig::PixelDirection(const CnaEngine::Graphics::Camera& camera, Vector2 pixel,
                                  Vector2 viewport) const {
    const float tangent = std::tan(camera.FieldOfView() * 0.5f);
    const float x = (pixel.X / viewport.X * 2.0f - 1.0f) * tangent * camera.AspectRatio();
    const float y = (1.0f - pixel.Y / viewport.Y * 2.0f) * tangent;
    return Vector3::Normalize(camera.Forward() + camera.Right() * x + camera.Up() * y);
}

}  // namespace zoogame
