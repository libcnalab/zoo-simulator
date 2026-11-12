// SPDX-License-Identifier: MIT
#pragma once

#include <array>
#include <vector>

#include "Microsoft/Xna/Framework/GamerServices/AvatarAnimationPreset.hpp"
#include "Microsoft/Xna/Framework/GamerServices/AvatarExpression.hpp"
#include "Microsoft/Xna/Framework/Quaternion.hpp"

#include "Common.hpp"

/// Poses of a CNA avatar's skeleton, built by this game.
///
/// CNA's avatars come with standing animations only (AvatarAnimationPreset:
/// idling, clapping, waving, ...). People in a zoo walk, run, sit and eat, so
/// those poses are made here: the legs by placing the feet and solving the
/// knees, the rest by turning bones of a standing pose.
///
/// Space: the avatar's own, as AvatarRenderer takes it. Y is up, the avatar
/// looks along -Z and its right hand is at +X. A bone's rotation is relative
/// to its parent; bones keep the offsets of the avatar's bind pose, only the
/// root has a position. Angles are radians. Matrices and quaternions compose
/// as in XNA: Concatenate(a, b) turns by a first, then by b.
namespace zoogame {

using Microsoft::Xna::Framework::Quaternion;
using Microsoft::Xna::Framework::GamerServices::AvatarAnimationPreset;
using Microsoft::Xna::Framework::GamerServices::AvatarExpression;

inline constexpr int kAvatarBones = 71;

/// Indices of the bones this game turns (Microsoft.Xna.Framework.GamerServices.AvatarBone).
namespace avatar_bone {
inline constexpr int Root = 0;
inline constexpr int BackLower = 1;
inline constexpr int HipLeft = 2;
inline constexpr int HipRight = 3;
inline constexpr int BackUpper = 5;
inline constexpr int KneeLeft = 6;
inline constexpr int KneeRight = 8;
inline constexpr int AnkleLeft = 11;
inline constexpr int Neck = 14;
inline constexpr int AnkleRight = 15;
inline constexpr int Head = 19;
inline constexpr int ShoulderLeft = 20;
inline constexpr int ToeLeft = 21;
inline constexpr int ShoulderRight = 22;
inline constexpr int ToeRight = 23;
inline constexpr int ElbowLeft = 25;
inline constexpr int ElbowRight = 28;
inline constexpr int WristLeft = 33;
inline constexpr int WristRight = 36;
}  // namespace avatar_bone

/// One pose: every bone's rotation against its parent, and where the root is.
struct AvatarPose {
    std::array<Quaternion, kAvatarBones> rotation;
    Vector3 root;
};

/// The measures of one avatar's skeleton (avatars differ in height and build).
struct AvatarBody {
    std::array<int, kAvatarBones> parent{};
    /// Bind pose: each bone against its parent, and in the avatar's space.
    std::array<Vector3, kAvatarBones> offset{};
    std::array<Vector3, kAvatarBones> position{};
    float thigh = 0.4f;    ///< hip joint to knee
    float shin = 0.4f;     ///< knee to ankle
    float hipY = 0.9f;     ///< height of the hip joints when standing straight
    float ankleY = 0.08f;  ///< height of the ankles when the feet are flat on the ground
    float top = 1.7f;      ///< roughly the top of the head

    /// Fills everything from the parents and the bind offsets.
    void Measure(const std::array<int, kAvatarBones>& parents,
                 const std::array<Vector3, kAvatarBones>& offsets);
    float Leg() const { return thigh + shin; }
};

/// A preset animation sampled into poses (sampling CNA's own player for every
/// person every frame would be wasteful, and its poses could not be blended).
struct AvatarClip {
    static constexpr float kStep = 1.0f / 15.0f;
    float length = 0.0f;
    std::vector<AvatarPose> frames;
    std::vector<AvatarExpression> expressions;

    static AvatarClip Bake(AvatarAnimationPreset preset);
    /// The pose `time` seconds in; a looping clip wraps, another holds its last frame.
    void Sample(float time, bool loop, AvatarPose& pose, AvatarExpression& expression) const;
};

/// a becomes the pose `t` of the way from a to b.
void Blend(AvatarPose& a, const AvatarPose& b, float t);

/// The rotation of a bone in the avatar's space.
Quaternion WorldRotation(const AvatarPose& pose, const AvatarBody& body, int bone);
/// Gives a bone the rotation `world` in the avatar's space, whatever its parents do.
/// Parents must have their final rotations already.
void SetWorldRotation(AvatarPose& pose, const AvatarBody& body, int bone, const Quaternion& world);
/// Turns a bone (and what hangs on it) by `delta` in the avatar's space, on top of the pose it has.
void TurnInModel(AvatarPose& pose, const AvatarBody& body, int bone, const Quaternion& delta);

Quaternion Pitch(float angle);  ///< about +X: a hanging limb swings forward, the head looks up
Quaternion Yaw(float angle);    ///< about +Y: the right shoulder comes forward
Quaternion Roll(float angle);   ///< about +Z

/// How far the body travels in one full cycle of the gait (both legs), in the
/// avatar's own units. `run` 0 is walking, 1 running.
float StrideLength(const AvatarBody& body, float run);

/// Turns a standing pose into a step of the gait: `phase` 0..1 through the
/// cycle (the left heel strikes at 0). The feet are placed on the ground and
/// travel backwards exactly as fast as the body goes forwards, so they do
/// not slide when the phase follows the distance covered (StrideLength).
void ApplyGait(AvatarPose& pose, const AvatarBody& body, float phase, float run);

/// Seats a standing pose: thighs level, the hip joints `hipHeight` above the
/// ground (avatar units), hands on the thighs.
void ApplySit(AvatarPose& pose, const AvatarBody& body, float hipHeight);

/// Brings the right hand to the mouth and back, `time` seconds into the meal.
void ApplyEating(AvatarPose& pose, const AvatarBody& body, float time, float weight);

/// Bows the head (reading a board, looking at something low).
void ApplyHeadDown(AvatarPose& pose, const AvatarBody& body, float angle);

/// The matrices AvatarRenderer::Draw takes.
void ToMatrices(const AvatarPose& pose, std::vector<Matrix>& bones);

}  // namespace zoogame
