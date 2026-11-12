// SPDX-License-Identifier: MIT
#include "AvatarPose.hpp"

#include <algorithm>
#include <cmath>

#include "Microsoft/Xna/Framework/GamerServices/AvatarAnimation.hpp"
#include "System/TimeSpan.hpp"

namespace zoogame {

namespace {

using namespace avatar_bone;

constexpr float kPi = 3.14159265f;

float Length(const Vector3& v) {
    return std::sqrt(v.X * v.X + v.Y * v.Y + v.Z * v.Z);
}

float Fraction(float v) {
    return v - std::floor(v);
}

/// 0 below `from`, 1 above `to`, smooth between.
float Smooth(float from, float to, float v) {
    const float t = std::clamp((v - from) / (to - from), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

float Mix(float a, float b, float t) {
    return a + (b - a) * t;
}

Quaternion Nlerp(const Quaternion& a, const Quaternion& b, float t) {
    // The shorter way round: q and -q are the same rotation.
    const float sign = a.X * b.X + a.Y * b.Y + a.Z * b.Z + a.W * b.W < 0.0f ? -1.0f : 1.0f;
    const float u = 1.0f - t;
    const float v = t * sign;
    Quaternion q(a.X * u + b.X * v, a.Y * u + b.Y * v, a.Z * u + b.Z * v, a.W * u + b.W * v);
    const float length = std::sqrt(q.X * q.X + q.Y * q.Y + q.Z * q.Z + q.W * q.W);
    if (length < 1e-6f)
        return a;
    const float inverse = 1.0f / length;
    return Quaternion(q.X * inverse, q.Y * inverse, q.Z * inverse, q.W * inverse);
}

struct LegAngles {
    float thigh;  ///< forward swing of the thigh from straight down
    float knee;   ///< how far the knee is bent
};

/// Thigh and knee that put the ankle `forward` of the hip joint and `drop`
/// below it. A target out of reach is approached with a straight leg.
LegAngles SolveLeg(float thigh, float shin, float forward, float drop) {
    const float reach =
        std::clamp(std::hypot(forward, drop), std::abs(thigh - shin) + 1e-3f, (thigh + shin) * 0.9995f);
    const float towards = std::atan2(forward, drop);
    const float atHip = std::acos(
        std::clamp((thigh * thigh + reach * reach - shin * shin) / (2.0f * thigh * reach), -1.0f, 1.0f));
    const float atKnee = std::acos(
        std::clamp((thigh * thigh + shin * shin - reach * reach) / (2.0f * thigh * shin), -1.0f, 1.0f));
    return {towards + atHip, kPi - atKnee};
}

struct Leg {
    int hip, knee, ankle;
};
constexpr Leg kLeft{HipLeft, KneeLeft, AnkleLeft};
constexpr Leg kRight{HipRight, KneeRight, AnkleRight};

void PoseLeg(AvatarPose& pose, const AvatarBody& body, const Leg& leg, const LegAngles& angles, float foot) {
    SetWorldRotation(pose, body, leg.hip, Pitch(angles.thigh));
    SetWorldRotation(pose, body, leg.knee, Pitch(angles.thigh - angles.knee));
    SetWorldRotation(pose, body, leg.ankle, Pitch(foot));
}

/// Where the ankle is against the point under the hip, and how the foot is held.
struct Footfall {
    float forward;
    float lift;
    float pitch;
};

Footfall FootAt(float phase, float duty, float reach, float lift, float leg) {
    Footfall foot{};
    const float heelRise = 0.035f * leg;
    if (phase < duty) {
        // On the ground: carried backwards at the pace of the body, the heel
        // rising before the foot leaves.
        const float u = phase / duty;
        const float rise = Smooth(0.7f, 1.0f, u);
        foot.forward = reach * (1.0f - 2.0f * u);
        foot.lift = heelRise * rise;
        foot.pitch = 0.22f * (1.0f - Smooth(0.0f, 0.16f, u)) - 0.5f * rise;
    } else {
        // In the air: swung forwards, toes down at first and up for the heel strike.
        const float v = (phase - duty) / (1.0f - duty);
        const float s = Smooth(0.0f, 1.0f, v);
        foot.forward = reach * (2.0f * s - 1.0f);
        foot.lift = heelRise * (1.0f - s) + lift * std::sin(kPi * v);
        foot.pitch = Mix(-0.5f, 0.22f, Smooth(0.15f, 0.9f, v));
    }
    return foot;
}

}  // namespace

Quaternion Pitch(float angle) {
    return Quaternion::CreateFromAxisAngle(Vector3(1.0f, 0.0f, 0.0f), angle);
}
Quaternion Yaw(float angle) {
    return Quaternion::CreateFromAxisAngle(Vector3(0.0f, 1.0f, 0.0f), angle);
}
Quaternion Roll(float angle) {
    return Quaternion::CreateFromAxisAngle(Vector3(0.0f, 0.0f, 1.0f), angle);
}

void AvatarBody::Measure(const std::array<int, kAvatarBones>& parents,
                         const std::array<Vector3, kAvatarBones>& offsets) {
    parent = parents;
    offset = offsets;
    for (std::size_t i = 0; i < kAvatarBones; ++i) {
        // Parents come before their children.
        const int p = parent[i];
        position[i] = p >= 0 ? position[static_cast<std::size_t>(p)] + offset[i] : offset[i];
    }
    thigh = (Length(offset[KneeLeft]) + Length(offset[KneeRight])) * 0.5f;
    shin = (Length(offset[AnkleLeft]) + Length(offset[AnkleRight])) * 0.5f;
    hipY = (position[HipLeft].Y + position[HipRight].Y) * 0.5f;
    ankleY = (position[AnkleLeft].Y + position[AnkleRight].Y) * 0.5f;
    top = position[Head].Y + 0.22f;
}

AvatarClip AvatarClip::Bake(AvatarAnimationPreset preset) {
    using Microsoft::Xna::Framework::GamerServices::AvatarAnimation;
    AvatarAnimation animation(preset);
    AvatarClip clip;
    clip.length =
        std::max(kStep, static_cast<float>(animation.getLengthProperty().getTotalSecondsProperty()));
    const auto count = static_cast<std::size_t>(std::ceil(clip.length / kStep)) + 1;
    clip.frames.resize(count);
    clip.expressions.resize(count);
    for (std::size_t f = 0; f < count; ++f) {
        const float time = std::min(clip.length, static_cast<float>(f) * kStep);
        animation.setCurrentPositionProperty(System::TimeSpan::FromSeconds(static_cast<double>(time)));
        const auto transforms = animation.getBoneTransformsProperty();
        AvatarPose& pose = clip.frames[f];
        for (int b = 0; b < kAvatarBones; ++b) {
            Vector3 scale;
            Vector3 translation;
            Quaternion rotation;
            if (!transforms[b].Decompose(scale, rotation, translation))
                rotation = Quaternion::Identity;
            pose.rotation[static_cast<std::size_t>(b)] = rotation;
            if (b == Root)
                pose.root = translation;
        }
        clip.expressions[f] = animation.getExpressionProperty();
    }
    return clip;
}

void AvatarClip::Sample(float time, bool loop, AvatarPose& pose, AvatarExpression& expression) const {
    if (frames.empty())
        return;
    const float t = loop ? std::fmod(std::max(time, 0.0f), length) : std::clamp(time, 0.0f, length);
    const float at = t / kStep;
    const auto last = frames.size() - 1;
    const auto first = std::min(static_cast<std::size_t>(at), last);
    // The last frame stands at `length`, a little less than a step after the one before it.
    const float span = first + 1 == last ? length - static_cast<float>(first) * kStep : kStep;
    const std::size_t second = std::min(first + 1, last);
    const float blend =
        span > 1e-5f ? std::clamp((t - static_cast<float>(first) * kStep) / span, 0.0f, 1.0f) : 0.0f;
    pose = frames[first];
    Blend(pose, frames[second], blend);
    expression = expressions[blend < 0.5f ? first : second];
}

void Blend(AvatarPose& a, const AvatarPose& b, float t) {
    if (t <= 0.0f)
        return;
    if (t >= 1.0f) {
        a = b;
        return;
    }
    for (std::size_t i = 0; i < kAvatarBones; ++i)
        a.rotation[i] = Nlerp(a.rotation[i], b.rotation[i], t);
    a.root = a.root + (b.root - a.root) * t;
}

Quaternion WorldRotation(const AvatarPose& pose, const AvatarBody& body, int bone) {
    Quaternion world = pose.rotation[static_cast<std::size_t>(bone)];
    for (int p = body.parent[static_cast<std::size_t>(bone)]; p >= 0;
         p = body.parent[static_cast<std::size_t>(p)])
        world = Quaternion::Concatenate(world, pose.rotation[static_cast<std::size_t>(p)]);
    return world;
}

void SetWorldRotation(AvatarPose& pose, const AvatarBody& body, int bone, const Quaternion& world) {
    const int p = body.parent[static_cast<std::size_t>(bone)];
    pose.rotation[static_cast<std::size_t>(bone)] =
        p < 0 ? world : Quaternion::Concatenate(world, Quaternion::Inverse(WorldRotation(pose, body, p)));
}

void TurnInModel(AvatarPose& pose, const AvatarBody& body, int bone, const Quaternion& delta) {
    SetWorldRotation(pose, body, bone, Quaternion::Concatenate(WorldRotation(pose, body, bone), delta));
}

float StrideLength(const AvatarBody& body, float run) {
    return body.Leg() * Mix(1.42f, 2.7f, run);
}

void ApplyGait(AvatarPose& pose, const AvatarBody& body, float phase, float run) {
    const float leg = body.Leg();
    // The share of the cycle a foot spends on the ground: more than half
    // when walking (both feet down between steps), less when running (a
    // moment of flight).
    const float duty = Mix(0.6f, 0.38f, run);
    const float reach = duty * StrideLength(body, run) * 0.5f;
    const float lift = leg * Mix(0.1f, 0.3f, run);
    const float swing = std::cos(2.0f * kPi * phase);

    // The hips ride a little below straight legs, highest over the standing
    // foot (twice a cycle), and sway towards it.
    const float bob = leg * Mix(0.014f, 0.03f, run) * std::cos(4.0f * kPi * (phase - duty * 0.5f));
    const float hip = leg * Mix(0.955f, 0.9f, run) + bob;
    const float sway =
        leg * 0.018f * (1.0f - run) * std::sin(2.0f * kPi * (phase - duty * 0.5f) + kPi * 0.5f);
    pose.root = Vector3(sway, body.position[Root].Y + hip - (body.hipY - body.ankleY), 0.0f);

    // Trunk: the pelvis follows the leading leg a little, the chest leans
    // into the pace and turns the other way with the arms.
    pose.rotation[Root] = Yaw(-0.05f * swing);
    TurnInModel(pose, body, BackLower, Pitch(-Mix(0.03f, 0.2f, run)));
    TurnInModel(pose, body, BackUpper, Yaw(Mix(0.11f, 0.2f, run) * swing));
    TurnInModel(pose, body, Neck, Yaw(-Mix(0.06f, 0.12f, run) * swing));
    TurnInModel(pose, body, Neck, Pitch(Mix(0.0f, 0.12f, run)));

    // Arms swing against the legs: the right arm is forward when the left heel strikes.
    const float arm = Mix(0.36f, 0.85f, run);
    const float elbow = Mix(0.18f, 1.25f, run);
    TurnInModel(pose, body, ShoulderLeft, Pitch(-arm * swing));
    TurnInModel(pose, body, ShoulderRight, Pitch(arm * swing));
    TurnInModel(pose, body, ElbowLeft, Pitch(elbow + 0.28f * std::max(0.0f, -swing)));
    TurnInModel(pose, body, ElbowRight, Pitch(elbow + 0.28f * std::max(0.0f, swing)));

    const Footfall left = FootAt(Fraction(phase), duty, reach, lift, leg);
    const Footfall right = FootAt(Fraction(phase + 0.5f), duty, reach, lift, leg);
    PoseLeg(pose, body, kLeft, SolveLeg(body.thigh, body.shin, left.forward, hip - left.lift), left.pitch);
    PoseLeg(pose, body, kRight, SolveLeg(body.thigh, body.shin, right.forward, hip - right.lift),
            right.pitch);
}

void ApplySit(AvatarPose& pose, const AvatarBody& body, float hipHeight) {
    pose.root = Vector3(0.0f, body.position[Root].Y + hipHeight - body.hipY, 0.0f);
    pose.rotation[Root] = Quaternion::Identity;
    TurnInModel(pose, body, BackLower, Pitch(0.06f));
    TurnInModel(pose, body, ShoulderLeft, Pitch(0.42f));
    TurnInModel(pose, body, ShoulderRight, Pitch(0.42f));
    TurnInModel(pose, body, ElbowLeft, Pitch(0.75f));
    TurnInModel(pose, body, ElbowRight, Pitch(0.75f));

    // Thighs level on the seat; the shins hang, tilted as far forward as it
    // takes for the feet of a tall sitter to stay above the ground.
    constexpr float kThigh = 1.5f;
    const float room = hipHeight - body.thigh * std::cos(kThigh) - body.ankleY;
    const float shin = std::acos(std::clamp(room / body.shin, 0.35f, 1.0f));
    for (const Leg& leg : {kLeft, kRight}) {
        SetWorldRotation(pose, body, leg.hip, Pitch(kThigh));
        SetWorldRotation(pose, body, leg.knee, Pitch(shin));
        SetWorldRotation(pose, body, leg.ankle, Pitch(shin * 0.5f));
    }
}

void ApplyEating(AvatarPose& pose, const AvatarBody& body, float time, float weight) {
    if (weight <= 0.0f)
        return;
    // A bite every few seconds: the hand goes up, stays a moment, comes down.
    const float t = std::fmod(time, 3.6f);
    const float bite = Smooth(0.2f, 0.7f, t) * (1.0f - Smooth(1.5f, 2.1f, t));
    TurnInModel(pose, body, ShoulderRight, Pitch(weight * (0.3f + 0.5f * bite)));
    TurnInModel(pose, body, ShoulderRight, Yaw(weight * 0.35f * bite));
    TurnInModel(pose, body, ElbowRight, Pitch(weight * (0.9f + 1.3f * bite)));
    TurnInModel(pose, body, Head, Pitch(-weight * 0.12f * bite));
}

void ApplyHeadDown(AvatarPose& pose, const AvatarBody& body, float angle) {
    if (angle == 0.0f)
        return;
    TurnInModel(pose, body, Neck, Pitch(-angle * 0.5f));
    TurnInModel(pose, body, Head, Pitch(-angle * 0.5f));
}

void ToMatrices(const AvatarPose& pose, std::vector<Matrix>& bones) {
    bones.resize(kAvatarBones);
    for (std::size_t i = 0; i < kAvatarBones; ++i)
        bones[i] = Matrix::CreateFromQuaternion(pose.rotation[i]);
    bones[avatar_bone::Root] = bones[avatar_bone::Root] * Matrix::CreateTranslation(pose.root);
}

}  // namespace zoogame
