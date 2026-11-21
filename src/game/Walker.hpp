// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>

#include "CnaEngine/Collision/CharacterController.hpp"

#include "Common.hpp"
#include "zoo/sim/Zoo.hpp"

namespace zoogame {

/// The player on foot: a visitor's body that walks the zoo's ground and is
/// stopped by what stops a person there (fences, walls, buildings, trees and
/// rocks, water one cannot wade), with the eyes it looks out of.
///
/// The body is the engine's character controller over a collision world made
/// from the zoo; it is made anew when the zoo is built on.
class Walker {
public:
    struct Input {
        float right = 0.0f;  ///< -1..1, strafing
        float ahead = 0.0f;  ///< -1..1
        bool run = false;
        /// Turning this frame, radians: to the right, and downwards.
        float turn = 0.0f;
        float tilt = 0.0f;
    };

    // The owner's choice (2026-11-14): a zoo of 320 m is crossed on foot, so the walk is what was
    // the jog (it was a 1.6 m/s stroll) and the run twice that.
    static constexpr float kWalkSpeed = 4.4f;  ///< m/s
    static constexpr float kRunSpeed = 8.8f;   ///< m/s
    static constexpr float kBodyRadius = 0.28f;

    Walker();

    /// Stands the body at `at`, looking along `yaw` (0 = +z/south, pi = north).
    void Enter(const zoo::Zoo& zoo, zoo::Vec2 at, float yaw, float pitch = 0.0f);
    void Update(const zoo::Zoo& zoo, const Input& input, float seconds);

    Vector3 Feet() const { return body_.Feet(); }
    zoo::Vec2 Position() const { return {body_.Feet().X, body_.Feet().Z}; }
    /// The eyes: above the feet, without the jolts of uneven ground.
    Vector3 Eye() const;
    /// The unit vector the eyes look along.
    Vector3 Gaze() const;
    float Yaw() const { return yaw_; }
    /// The angle below the horizon.
    float Pitch() const { return pitch_; }
    /// Metres per second over the ground, as walked.
    float Speed() const;
    /// The last Update() put a foot down (a footstep to be heard).
    bool Stepped() const { return stepped_; }
    /// A gentle sway of the eyes with the steps (0 turns it off).
    void SetHeadBob(float metres) { headBob_ = metres; }

    /// The shapes of the collision world (diagnostics).
    std::size_t Shapes() const { return world_.ShapeCount(); }
    /// What a ray from the eyes meets first of the solid world, metres; `reach` when nothing.
    float SightTo(const Vector3& direction, float reach) const;

private:
    void Rebuild(const zoo::Zoo& zoo);

    CnaEngine::Collision::StaticWorld world_;
    CnaEngine::Collision::CharacterController body_;
    std::uint32_t objectsRevision_ = 0;
    std::uint32_t fenceRevision_ = 0;
    std::uint32_t navRevision_ = 0;
    bool built_ = false;
    float yaw_ = 3.14159265f;
    float pitch_ = 0.0f;
    float eyeY_ = 0.0f;
    float headBob_ = 0.012f;
    bool stepped_ = false;
};

/// Whether the player's body fits between the placed props at this position.
bool ClearOfPlayerProps(const zoo::Zoo& zoo, zoo::Vec2 at);
/// Whether the player can arrive at this tile centre from the visitor map.
bool CanLandOnPath(const zoo::Zoo& zoo, zoo::TileCoord tile);

}  // namespace zoogame
