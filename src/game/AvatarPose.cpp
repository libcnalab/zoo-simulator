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

void ApplyGait(AvatarPose& pose, const AvatarBody& body, float phase, float run, float stride, float swing) {
    const float leg = body.Leg();
    // The share of the cycle a foot spends on the ground: more than half
    // when walking (both feet down between steps), less when running (a
    // moment of flight).
    const float duty = Mix(0.6f, 0.38f, run);
    const float reach = duty * StrideLength(body, run) * stride * 0.5f;
    const float lift = leg * Mix(0.1f, 0.3f, run) * Mix(0.6f, 1.0f, std::min(stride, 1.0f));
    const float turn = std::cos(2.0f * kPi * phase);
    const float ease = std::clamp(stride, 0.4f, 1.1f);

    // The hips ride a little below straight legs, highest over the standing
    // foot (twice a cycle), and sway towards it. A short step needs less of all that.
    const float bob = leg * Mix(0.014f, 0.03f, run) * ease * std::cos(4.0f * kPi * (phase - duty * 0.5f));
    const float hip = leg * Mix(Mix(0.975f, 0.955f, ease), 0.9f, run) + bob;
    const float sway =
        leg * 0.018f * (1.0f - run) * std::sin(2.0f * kPi * (phase - duty * 0.5f) + kPi * 0.5f);
    pose.root = Vector3(sway, body.position[Root].Y + hip - (body.hipY - body.ankleY), 0.0f);

    // Trunk: the pelvis follows the leading leg a little, the chest leans
    // into the pace and turns the other way with the arms.
    pose.rotation[Root] = Yaw(-0.05f * ease * turn);
    TurnInModel(pose, body, BackLower, Pitch(-Mix(0.03f, 0.2f, run)));
    TurnInModel(pose, body, BackUpper, Yaw(Mix(0.11f, 0.2f, run) * ease * turn));
    TurnInModel(pose, body, Neck, Yaw(-Mix(0.06f, 0.12f, run) * ease * turn));
    TurnInModel(pose, body, Neck, Pitch(Mix(0.0f, 0.12f, run)));

    // Arms swing against the legs: the right arm is forward when the left heel strikes.
    const float arm = Mix(0.36f * ease * swing, 0.85f, run);
    const float elbow = Mix(0.18f, 1.25f, run);
    TurnInModel(pose, body, ShoulderLeft, Pitch(-arm * turn));
    TurnInModel(pose, body, ShoulderRight, Pitch(arm * turn));
    TurnInModel(pose, body, ElbowLeft, Pitch(elbow + 0.28f * swing * std::max(0.0f, -turn)));
    TurnInModel(pose, body, ElbowRight, Pitch(elbow + 0.28f * swing * std::max(0.0f, turn)));

    const Footfall left = FootAt(Fraction(phase), duty, reach, lift, leg);
    const Footfall right = FootAt(Fraction(phase + 0.5f), duty, reach, lift, leg);
    PoseLeg(pose, body, kLeft, SolveLeg(body.thigh, body.shin, left.forward, hip - left.lift), left.pitch);
    PoseLeg(pose, body, kRight, SolveLeg(body.thigh, body.shin, right.forward, hip - right.lift),
            right.pitch);
}

namespace {

/// Brings a bone `weight` of the way to the rotation `world` in the avatar's space.
void Bring(AvatarPose& pose, const AvatarBody& body, int bone, const Quaternion& world, float weight) {
    SetWorldRotation(pose, body, bone, Nlerp(WorldRotation(pose, body, bone), world, weight));
}

struct Arm {
    int shoulder, elbow, wrist;
    float inward;  ///< the sign of a turn about the vertical that brings a raised arm across the chest
};
constexpr Arm kLeftArm{ShoulderLeft, ElbowLeft, WristLeft, -1.0f};
constexpr Arm kRightArm{ShoulderRight, ElbowRight, WristRight, 1.0f};

/// An arm, whatever it did: the upper arm swung `upper` forward of hanging and turned
/// `across` towards the chest, the forearm bent `bend` further and turned `fold` across.
void PoseArm(AvatarPose& pose, const AvatarBody& body, const Arm& arm, float upper, float across, float bend,
             float fold, float weight) {
    Bring(pose, body, arm.shoulder, Quaternion::Concatenate(Pitch(upper), Yaw(arm.inward * across)), weight);
    Bring(pose, body, arm.elbow, Quaternion::Concatenate(Pitch(upper + bend), Yaw(arm.inward * fold)), weight);
    const auto wrist = static_cast<std::size_t>(arm.wrist);
    pose.rotation[wrist] = Nlerp(pose.rotation[wrist], Quaternion::Identity, weight);
}

}  // namespace

void ApplyWeight(AvatarPose& pose, const AvatarBody& body, float side) {
    const float leg = body.Leg();
    const float shift = 0.05f * leg * side;
    // The hip of the free leg hangs, the shoulders make up for it. The root turns about
    // the ground: it is moved so that the hips end up `shift` aside and the standing
    // hip no higher than it was.
    const float hang = 0.07f * side;
    pose.root = Vector3(shift + body.hipY * std::sin(hang),
                        body.position[Root].Y - std::abs(body.position[HipRight].X * std::sin(hang)), 0.0f);
    pose.rotation[Root] = Roll(hang);
    TurnInModel(pose, body, BackLower, Roll(-0.06f * side));
    TurnInModel(pose, body, BackUpper, Roll(-0.05f * side));
    // Both feet stay where they stood: the standing leg leans in under the hip, the
    // other is put a little forward on its toes with the knee loose.
    const float in = std::asin(std::clamp(-shift / leg, -0.5f, 0.5f));
    const float drop = body.hipY - body.ankleY;
    const Leg& standing = side >= 0.0f ? kRight : kLeft;
    const Leg& loose = side >= 0.0f ? kLeft : kRight;
    SetWorldRotation(pose, body, standing.hip, Roll(in));
    SetWorldRotation(pose, body, standing.knee, Roll(in));
    SetWorldRotation(pose, body, standing.ankle, Quaternion::Identity);
    const float much = std::abs(side);
    const LegAngles angles =
        SolveLeg(body.thigh, body.shin, 0.1f * leg * much, drop - (0.03f + 0.012f) * leg * much);
    SetWorldRotation(pose, body, loose.hip, Quaternion::Concatenate(Pitch(angles.thigh), Roll(in * 1.6f)));
    SetWorldRotation(pose, body, loose.knee,
                     Quaternion::Concatenate(Pitch(angles.thigh - angles.knee), Roll(in * 1.6f)));
    SetWorldRotation(pose, body, loose.ankle, Pitch(-0.38f * much));
}

void ApplyLook(AvatarPose& pose, const AvatarBody& body, float yaw, float pitch) {
    if (yaw == 0.0f && pitch == 0.0f)
        return;
    TurnInModel(pose, body, BackUpper, Yaw(0.18f * yaw));
    TurnInModel(pose, body, Neck, Quaternion::Concatenate(Pitch(0.4f * pitch), Yaw(0.37f * yaw)));
    TurnInModel(pose, body, Head, Quaternion::Concatenate(Pitch(0.6f * pitch), Yaw(0.45f * yaw)));
}

void ApplyPoint(AvatarPose& pose, const AvatarBody& body, float yaw, float pitch, float weight) {
    if (weight <= 0.0f)
        return;
    // Kept clear of the body: an arm cannot point across its own chest.
    const float across = std::clamp(yaw, -0.7f, 0.5f);
    PoseArm(pose, body, kRightArm, 1.38f + pitch, across, 0.14f, across, weight);
    TurnInModel(pose, body, BackUpper, Yaw(0.12f * weight));
}

void ApplyCamera(AvatarPose& pose, const AvatarBody& body, float weight) {
    if (weight <= 0.0f)
        return;
    PoseArm(pose, body, kRightArm, 0.95f, 0.45f, 1.5f, 1.0f, weight);
    PoseArm(pose, body, kLeftArm, 0.95f, 0.45f, 1.5f, 1.0f, weight);
    TurnInModel(pose, body, Head, Pitch(-0.06f * weight));
}

void ApplyPhone(AvatarPose& pose, const AvatarBody& body, float weight) {
    if (weight <= 0.0f)
        return;
    PoseArm(pose, body, kRightArm, 0.3f, 0.2f, 1.75f, 0.9f, weight);
    TurnInModel(pose, body, Neck, Pitch(-0.2f * weight));
    TurnInModel(pose, body, Head, Pitch(-0.3f * weight));
}

void ApplyLean(AvatarPose& pose, const AvatarBody& body, float weight) {
    if (weight <= 0.0f)
        return;
    // The hips go back as the chest comes forward, or the avatar would topple.
    pose.root.Z += 0.11f * body.Leg() * weight;
    TurnInModel(pose, body, BackLower, Pitch(-0.3f * weight));
    TurnInModel(pose, body, Neck, Pitch(0.12f * weight));
    TurnInModel(pose, body, Head, Pitch(0.12f * weight));
    PoseArm(pose, body, kRightArm, 0.7f, 0.1f, 1.0f, 0.55f, weight);
    PoseArm(pose, body, kLeftArm, 0.7f, 0.1f, 1.0f, 0.55f, weight);
    for (const Leg& leg : {kLeft, kRight}) {
        TurnInModel(pose, body, leg.hip, Pitch(-0.12f * weight));
        TurnInModel(pose, body, leg.knee, Pitch(-0.12f * weight));
    }
}

void ApplyTalk(AvatarPose& pose, const AvatarBody& body, float time, float weight) {
    if (weight <= 0.0f)
        return;
    const float beat = std::sin(time * 5.1f) * std::sin(time * 1.3f);
    PoseArm(pose, body, kRightArm, 0.35f + 0.1f * beat, 0.1f, 1.25f + 0.3f * beat, 0.35f - 0.3f * beat, weight);
    TurnInModel(pose, body, Head, Pitch(0.05f * weight * std::sin(time * 3.3f)));
}

void ApplyCrouch(AvatarPose& pose, const AvatarBody& body) {
    const float leg = body.Leg();
    // On the balls of the feet, the seat near the heels, the knees well forward.
    const float hip = 0.5f * leg;
    pose.root = Vector3(0.0f, body.position[Root].Y + hip - (body.hipY - body.ankleY) + 0.045f * leg,
                        0.05f * leg);
    pose.rotation[Root] = Quaternion::Identity;
    TurnInModel(pose, body, BackLower, Pitch(-0.3f));
    TurnInModel(pose, body, BackUpper, Pitch(-0.14f));
    TurnInModel(pose, body, Neck, Pitch(0.22f));
    const LegAngles angles = SolveLeg(body.thigh, body.shin, -0.08f * leg, hip - 0.045f * leg);
    for (const Leg& one : {kLeft, kRight})
        PoseLeg(pose, body, one, angles, -0.45f);
    PoseArm(pose, body, kRightArm, 0.9f, 0.1f, 0.55f, 0.3f, 1.0f);
    PoseArm(pose, body, kLeftArm, 0.9f, 0.1f, 0.55f, 0.3f, 1.0f);
}

void ApplyStoop(AvatarPose& pose, const AvatarBody& body, float weight) {
    if (weight <= 0.0f)
        return;
    TurnInModel(pose, body, BackLower, Pitch(-0.1f * weight));
    TurnInModel(pose, body, BackUpper, Pitch(-0.16f * weight));
    TurnInModel(pose, body, Neck, Pitch(0.1f * weight));
    TurnInModel(pose, body, Head, Pitch(0.1f * weight));
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
