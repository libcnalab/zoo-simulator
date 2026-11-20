// SPDX-License-Identifier: MIT
#include "KeeperRenderer.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>

#include "Microsoft/Xna/Framework/BoundingFrustum.hpp"
#include "Microsoft/Xna/Framework/BoundingSphere.hpp"
#include "Microsoft/Xna/Framework/GamerServices/AvatarRendererState.hpp"

#include "GroundPaint.hpp"
#include "KeeperAppearance.hpp"
#include "zoo/base/Rng.hpp"

namespace zoogame {

namespace {

using Microsoft::Xna::Framework::BoundingSphere;
using Microsoft::Xna::Framework::ContainmentType;
using Microsoft::Xna::Framework::GamerServices::AvatarDescription;
using Microsoft::Xna::Framework::GamerServices::AvatarRenderer;
using Microsoft::Xna::Framework::GamerServices::AvatarRendererState;
using Microsoft::Xna::Framework::Graphics::GraphicsDevice;
using Microsoft::Xna::Framework::Graphics::VertexPositionColor;
using zoo::StaffActivity;
using namespace avatar_bone;

constexpr float kPi = 3.14159265f;
/// How many different people the crew is played by.
constexpr std::size_t kCast = 4;
/// Nobody is drawn farther from the camera than this, metres: a keeper is then a dozen
/// pixels tall and the tools cannot be told any more.
constexpr float kFarthest = 150.0f;
/// How many are drawn in one frame at most (the nearest).
constexpr std::size_t kMost = 8;

float Ease(float value, float goal, float rate, float seconds) {
    return goal + (value - goal) * std::exp(-rate * seconds);
}

float SmoothStep(float v) {
    v = std::clamp(v, 0.0f, 1.0f);
    return v * v * (3.0f - 2.0f * v);
}

/// 0 below `from`, 1 above `to`, smooth between.
float Ramp(float from, float to, float v) {
    return SmoothStep((v - from) / (to - from));
}

float Fraction(float v) {
    return v - std::floor(v);
}

float Length(const Vector3& v) {
    return std::sqrt(v.X * v.X + v.Y * v.Y + v.Z * v.Z);
}

float Dot(const Vector3& a, const Vector3& b) {
    return a.X * b.X + a.Y * b.Y + a.Z * b.Z;
}

Vector3 Unit(const Vector3& v, const Vector3& otherwise) {
    const float length = Length(v);
    return length > 1.0e-5f ? v * (1.0f / length) : otherwise;
}

Vector3 Lerp(const Vector3& a, const Vector3& b, float t) {
    return a + (b - a) * t;
}

const Vector3 kUp(0.0f, 1.0f, 0.0f);
const Vector3 kForward(0.0f, 0.0f, -1.0f);  // an avatar looks along -Z
const Vector3 kRight(1.0f, 0.0f, 0.0f);

// ---------------------------------------------------------------------------------------
// The uniform
// ---------------------------------------------------------------------------------------

/// CNA's avatar description as docs/avatars.md of CNA lays it out (formats 1 to 3 keep
/// these bytes): colours and catalog items of the clothes, and a checksum over all of it.
constexpr std::size_t kDescriptionBytes = 1021;
constexpr std::size_t kTopColour = 19;
constexpr std::size_t kBottomColour = 22;
constexpr std::size_t kShoeColour = 25;
constexpr std::size_t kAccessoryColour = 28;
constexpr std::size_t kTopItem = 33;
constexpr std::size_t kBottomItem = 35;
constexpr std::size_t kShoeItem = 37;
constexpr std::size_t kHatItem = 41;
constexpr std::size_t kChecksum = 1017;
/// Items of the baseline catalog (v3), which every later catalog keeps.
constexpr std::uint16_t kPolo = 27;
constexpr std::uint16_t kCargo = 43;
constexpr std::uint16_t kBoots = 61;
constexpr std::uint16_t kBucketHat = 102;

std::uint32_t Crc32(const std::uint8_t* data, std::size_t size) {
    std::uint32_t crc = 0xffffffffu;
    for (std::size_t i = 0; i < size; ++i) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}

/// The same person in the zoo's work clothes: a green polo shirt, khaki cargo trousers,
/// brown boots and a khaki bucket hat. With `items` false only the colours are changed
/// (the clothes stay what the person wore). Nothing when CNA does not take the result
/// for an avatar: the description is CNA's own format, and a build of CNA that lays it
/// out differently refuses it by the checksum or the catalog.
std::optional<AvatarDescription> Uniformed(const AvatarDescription& person, bool items) {
    std::vector<std::uint8_t> bytes = person.getDescriptionProperty();
    if (bytes.size() != kDescriptionBytes)
        return std::nullopt;
    const auto colour = [&](std::size_t at, int r, int g, int b) {
        bytes[at] = static_cast<std::uint8_t>(r);
        bytes[at + 1] = static_cast<std::uint8_t>(g);
        bytes[at + 2] = static_cast<std::uint8_t>(b);
    };
    const auto item = [&](std::size_t at, std::uint16_t id) {
        bytes[at] = static_cast<std::uint8_t>(id & 0xffu);
        bytes[at + 1] = static_cast<std::uint8_t>(id >> 8);
    };
    colour(kTopColour, 44, 112, 70);
    colour(kBottomColour, 152, 134, 96);
    colour(kShoeColour, 72, 52, 38);
    colour(kAccessoryColour, 170, 152, 108);
    if (items) {
        item(kTopItem, kPolo);
        item(kBottomItem, kCargo);
        item(kShoeItem, kBoots);
        item(kHatItem, kBucketHat);
    }
    const std::uint32_t crc = Crc32(bytes.data(), kChecksum);
    for (std::size_t i = 0; i < 4; ++i)
        bytes[kChecksum + i] = static_cast<std::uint8_t>((crc >> (8 * i)) & 0xffu);
    AvatarDescription dressed(bytes);
    if (!dressed.getIsValidProperty() || dressed.getHeightProperty() <= 0.0f)
        return std::nullopt;
    return dressed;
}

/// A cast member in uniform if CNA takes it (first with the clothes, then with the colours
/// alone), otherwise as they came.
AvatarDescription Engage(AvatarDescription person, bool& uniformed) {
    for (const bool items : {true, false}) {
        if (std::optional<AvatarDescription> dressed = Uniformed(person, items)) {
            uniformed = true;
            return std::move(*dressed);
        }
    }
    uniformed = false;
    return person;
}

// ---------------------------------------------------------------------------------------
// The body
// ---------------------------------------------------------------------------------------

/// What of a cast member the posing needs.
struct Build {
    const AvatarBody& body;
    float upper;  ///< shoulder to elbow
    float fore;   ///< elbow to wrist
    float Arm() const { return upper + fore; }
};

Vector3 Rotated(const Vector3& v, const Quaternion& q) {
    return Vector3::Transform(v, q);
}

/// Where a bone's joint is in the avatar's space, in this pose.
Vector3 JointAt(const AvatarPose& pose, const AvatarBody& body, int bone) {
    const int parent = body.parent[static_cast<std::size_t>(bone)];
    if (bone == Root || parent < 0)
        return pose.root;
    return JointAt(pose, body, parent) +
           Rotated(body.offset[static_cast<std::size_t>(bone)], WorldRotation(pose, body, parent));
}

/// The shortest turn that takes the direction `from` to the direction `to`.
Quaternion Between(const Vector3& from, const Vector3& to) {
    const Vector3 a = Unit(from, kUp);
    const Vector3 b = Unit(to, kUp);
    const float cosine = Dot(a, b);
    if (cosine < -0.9999f) {
        // Opposite: half a turn about anything across.
        const Vector3 across = Unit(Vector3::Cross(a, std::abs(a.Y) < 0.9f ? kUp : kRight), kRight);
        return Quaternion(across.X, across.Y, across.Z, 0.0f);
    }
    const Vector3 axis = Vector3::Cross(a, b);
    return Quaternion::Normalize(Quaternion(axis.X, axis.Y, axis.Z, 1.0f + cosine));
}

/// Where the hand holds things: a little beyond the wrist.
Vector3 Grip(const AvatarPose& pose, const AvatarBody& body, bool right) {
    const Vector3 wrist = JointAt(pose, body, right ? WristRight : WristLeft);
    const Vector3 elbow = JointAt(pose, body, right ? ElbowRight : ElbowLeft);
    return wrist + Unit(wrist - elbow, kUp * -1.0f) * 0.07f;
}

/// Brings the wrist of an arm to `target` (as near as the arm reaches), the elbow bent
/// towards `pole`. The trunk must have its final pose already.
void Reach(AvatarPose& pose, const Build& build, bool right, const Vector3& target, const Vector3& pole) {
    const AvatarBody& body = build.body;
    const int shoulder = right ? ShoulderRight : ShoulderLeft;
    const int elbow = right ? ElbowRight : ElbowLeft;
    const int wrist = right ? WristRight : WristLeft;
    const Vector3 from = JointAt(pose, body, shoulder);
    const Vector3 along = Unit(target - from, kUp * -1.0f);
    const float distance = std::clamp(Length(target - from), std::abs(build.upper - build.fore) + 0.02f,
                                      build.Arm() * 0.995f);
    // The elbow lies on a circle about the line from shoulder to hand; the pole says where on it.
    const float toElbow =
        (build.upper * build.upper - build.fore * build.fore + distance * distance) / (2.0f * distance);
    const float out = std::sqrt(std::max(build.upper * build.upper - toElbow * toElbow, 0.0f));
    const Vector3 aside = Unit(pole - along * Dot(pole, along), kUp * -1.0f);
    const Vector3 elbowAt = from + along * toElbow + aside * out;
    const Vector3 handAt = from + along * distance;
    TurnInModel(pose, body, shoulder,
                Between(Rotated(body.offset[static_cast<std::size_t>(elbow)], WorldRotation(pose, body, shoulder)),
                        elbowAt - from));
    TurnInModel(pose, body, elbow,
                Between(Rotated(body.offset[static_cast<std::size_t>(wrist)], WorldRotation(pose, body, elbow)),
                        handAt - elbowAt));
}

/// Bends the trunk forward: at the waist and between the shoulders, radians.
void Lean(AvatarPose& pose, const AvatarBody& body, float waist, float chest) {
    TurnInModel(pose, body, BackLower, Pitch(-waist));
    TurnInModel(pose, body, BackUpper, Pitch(-chest));
}

/// Turns the trunk about the vertical (positive: the right shoulder comes forward).
void Twist(AvatarPose& pose, const AvatarBody& body, float angle) {
    TurnInModel(pose, body, BackLower, Yaw(angle * 0.5f));
    TurnInModel(pose, body, BackUpper, Yaw(angle * 0.5f));
}

void LookDown(AvatarPose& pose, const AvatarBody& body, float angle) {
    TurnInModel(pose, body, Neck, Pitch(-angle * 0.5f));
    TurnInModel(pose, body, Head, Pitch(-angle * 0.5f));
}

/// Squats: the hip joints come down by bending hips and knees, the feet stay where they
/// are and flat on the ground. `bend` is the angle of the thighs from the vertical.
void Squat(AvatarPose& pose, const AvatarBody& body, float bend) {
    const float shinBend = std::asin(std::clamp(body.thigh * std::sin(bend) / body.shin, -1.0f, 1.0f));
    pose.root.Y -= body.thigh * (1.0f - std::cos(bend)) + body.shin * (1.0f - std::cos(shinBend));
    for (const bool right : {false, true}) {
        TurnInModel(pose, body, right ? HipRight : HipLeft, Pitch(bend));
        TurnInModel(pose, body, right ? KneeRight : KneeLeft, Pitch(-(bend + shinBend)));
        TurnInModel(pose, body, right ? AnkleRight : AnkleLeft, Pitch(shinBend));
    }
}

/// The elbow hangs down, out and a little back unless a pose says otherwise.
Vector3 ElbowDown(bool right) {
    return Vector3(right ? 0.45f : -0.45f, -0.75f, 0.45f);
}

Vector3 Shoulder(const AvatarPose& pose, const Build& build, bool right) {
    return JointAt(pose, build.body, right ? ShoulderRight : ShoulderLeft);
}

Vector3 BetweenShoulders(const AvatarPose& pose, const Build& build) {
    return (Shoulder(pose, build, false) + Shoulder(pose, build, true)) * 0.5f;
}

// -- The moments of the jobs: the same functions pose the body and place the tools. --

/// 0 .. 1 .. 0: the bucket tipped over the feeder and back, once every few seconds.
float Tip(float time) {
    const float t = Fraction(time / 4.2f);
    return Ramp(0.08f, 0.3f, t) * (1.0f - Ramp(0.72f, 0.95f, t));
}

/// The stroke of a rake: 0 reached out, 1 drawn in.
float Stroke(float time, float period) {
    const float t = Fraction(time / period);
    return Ramp(0.1f, 0.62f, t) * (1.0f - Ramp(0.7f, 0.98f, t));
}

/// A throw of feed, `t` 0 .. 1 through it: the hand dips into the bucket, swings back,
/// throws (the feed leaves the hand at kThrowAt) and comes down.
constexpr float kThrowPeriod = 2.6f;
constexpr float kThrowAt = 0.6f;

/// The sweep of the hose from side to side, radians.
float HoseSwing(float time) {
    return 0.42f * std::sin(time * 0.9f);
}

/// The lifting of the sack out of the bin: 0 hands in the bin, 1 the sack held up.
float Lift(float time) {
    const float t = Fraction(time / 5.0f);
    return Ramp(0.25f, 0.5f, t) * (1.0f - Ramp(0.82f, 0.98f, t));
}

/// 1 while the hand is at the fence, 0 while it writes on the clipboard.
float AtFence(float time) {
    const float t = Fraction(time / 7.0f);
    return Ramp(0.05f, 0.18f, t) * (1.0f - Ramp(0.55f, 0.68f, t));
}

/// The hand of a throw, relative to the right shoulder, in arm lengths.
Vector3 ThrowHand(float t) {
    const Vector3 inBucket(-0.50f, -0.80f, -0.25f);
    const Vector3 back(0.20f, -0.72f, 0.42f);
    const Vector3 out(0.05f, 0.22f, -0.93f);
    const Vector3 down(0.10f, -0.90f, -0.10f);
    if (t < 0.3f)
        return Lerp(down, inBucket, SmoothStep(t / 0.2f));
    if (t < 0.5f)
        return Lerp(inBucket, back, SmoothStep((t - 0.3f) / 0.2f));
    if (t < 0.64f)
        return Lerp(back, out, SmoothStep((t - 0.5f) / 0.14f));
    return Lerp(out, down, SmoothStep((t - 0.72f) / 0.25f));
}

/// Where an arm hangs with something heavy in the hand.
Vector3 Hanging(const AvatarPose& pose, const Build& build, bool right) {
    return Shoulder(pose, build, right) + Vector3(right ? 0.07f : -0.07f, -0.95f * build.Arm(), -0.03f);
}

/// The two hands on a long tool worked on the ground in front: the upper (left) hand
/// near the chest, the lower (right) one down the shaft. `draw` 0 .. 1 pulls the tool
/// in, `aside` swings its lower end sideways (metres).
void HoldShaft(AvatarPose& pose, const Build& build, float draw, float aside) {
    const float arm = build.Arm();
    const Vector3 chest = BetweenShoulders(pose, build);
    const Vector3 upper = chest + Vector3(-0.03f, (-0.30f + 0.10f * draw) * arm, (-0.36f + 0.20f * draw) * arm);
    const Vector3 lower = upper + Vector3(0.16f + aside, -0.50f * arm, (-0.56f + 0.14f * draw) * arm);
    Reach(pose, build, false, upper, ElbowDown(false));
    Reach(pose, build, true, lower, ElbowDown(true));
}

/// What is in the hands on the way to a job.
enum class Held { Nothing, Bucket, Can, Barrow, Rake, Broom, Board };

Held HeldFor(StaffActivity approach, StaffActivity work) {
    if (approach == StaffActivity::Push)
        return Held::Barrow;
    if (approach == StaffActivity::Carry)
        return work == StaffActivity::Water ? Held::Can : Held::Bucket;
    switch (work) {
        case StaffActivity::Rake:
            return Held::Rake;
        case StaffActivity::Sweep:
            return Held::Broom;
        case StaffActivity::Inspect:
            return Held::Board;
        default:
            return Held::Nothing;
    }
}

/// The body on the way with something: a step of the gait is there already.
void PoseHold(AvatarPose& pose, const Build& build, Held held) {
    const AvatarBody& body = build.body;
    const float arm = build.Arm();
    switch (held) {
        case Held::Bucket:
        case Held::Can:
            // The loaded arm does not swing.
            Reach(pose, build, true, Hanging(pose, build, true), ElbowDown(true));
            break;
        case Held::Barrow: {
            Lean(pose, body, 0.12f, 0.06f);
            for (const bool right : {false, true}) {
                const Vector3 grip =
                    Shoulder(pose, build, right) + Vector3(right ? 0.08f : -0.08f, -0.84f * arm, -0.42f * arm);
                Reach(pose, build, right, grip, ElbowDown(right));
            }
            break;
        }
        case Held::Rake:
        case Held::Broom:
            // Held upright at the side, the hand half way up the body.
            Reach(pose, build, true, Shoulder(pose, build, true) + Vector3(0.10f, -0.70f * arm, -0.42f * arm),
                  ElbowDown(true));
            break;
        case Held::Board:
        case Held::Nothing:
            break;
    }
}

/// The body at a job, `time` seconds into it. `pose` is somebody standing.
void PoseWork(AvatarPose& pose, const Build& build, StaffActivity work, float time) {
    const AvatarBody& body = build.body;
    const float arm = build.Arm();
    switch (work) {
        case StaffActivity::FillFeeder:
        case StaffActivity::FillTrough: {
            // Bent over it, the bucket in both hands, tipped and righted.
            const bool low = work == StaffActivity::FillTrough;
            const float tip = Tip(time);
            Lean(pose, body, (low ? 0.52f : 0.36f) + 0.06f * tip, low ? 0.34f : 0.24f);
            LookDown(pose, body, 0.2f);
            const Vector3 bucket = BetweenShoulders(pose, build) +
                                   Vector3(0.0f, (low ? -0.82f : -0.60f) * arm + 0.04f * tip, -0.70f * arm);
            Reach(pose, build, false, bucket + Vector3(-0.17f, 0.0f, 0.03f), ElbowDown(false));
            Reach(pose, build, true, bucket + Vector3(0.17f, 0.0f, 0.03f), ElbowDown(true));
            break;
        }
        case StaffActivity::ThrowFeed: {
            const float t = Fraction(time / kThrowPeriod);
            const float swing = Ramp(0.3f, 0.5f, t) * (1.0f - Ramp(0.5f, 0.66f, t));
            Twist(pose, body, -0.35f * swing + 0.25f * Ramp(0.5f, 0.66f, t) * (1.0f - Ramp(0.7f, 0.95f, t)));
            Reach(pose, build, false, Hanging(pose, build, false) + Vector3(0.0f, 0.12f * arm, -0.12f * arm),
                  ElbowDown(false));
            Reach(pose, build, true, Shoulder(pose, build, true) + ThrowHand(t) * arm, ElbowDown(true));
            break;
        }
        case StaffActivity::Rake: {
            const float draw = Stroke(time, 1.9f);
            Lean(pose, body, 0.34f - 0.14f * draw, 0.2f - 0.06f * draw);
            LookDown(pose, body, 0.15f);
            HoldShaft(pose, build, draw, 0.0f);
            break;
        }
        case StaffActivity::Sweep: {
            // Short strokes from side to side, the body turning with them.
            const float side = std::sin(time * 4.4f);
            Lean(pose, body, 0.2f, 0.12f);
            Twist(pose, body, 0.16f * side);
            LookDown(pose, body, 0.2f);
            HoldShaft(pose, build, 0.5f + 0.2f * std::cos(time * 4.4f), 0.07f * side);
            break;
        }
        case StaffActivity::Hose: {
            // The nozzle in the right hand, the left steadying the hose; the jet swings.
            Twist(pose, body, HoseSwing(time));
            Reach(pose, build, true, Shoulder(pose, build, true) + Vector3(0.0f, -0.62f * arm, -0.6f * arm),
                  ElbowDown(true));
            Reach(pose, build, false, Shoulder(pose, build, false) + Vector3(0.16f, -0.78f * arm, -0.3f * arm),
                  ElbowDown(false));
            break;
        }
        case StaffActivity::Inspect: {
            // A hand on the fence, a look along it, a note on the clipboard.
            const float fence = AtFence(time);
            TurnInModel(pose, body, Head, Yaw(0.5f * fence * std::sin(time * 0.9f)));
            LookDown(pose, body, 0.4f * (1.0f - fence));
            const Vector3 board = Shoulder(pose, build, false) + Vector3(0.14f, -0.58f * arm, -0.56f * arm);
            Reach(pose, build, false, board, ElbowDown(false));
            const Vector3 onFence = Shoulder(pose, build, true) + Vector3(0.0f, -0.28f * arm, -0.92f * arm);
            const Vector3 writing = board + Vector3(0.14f, 0.07f, 0.0f) + kRight * (0.02f * std::sin(time * 9.0f));
            Reach(pose, build, true, Lerp(writing, onFence, fence), ElbowDown(true));
            break;
        }
        case StaffActivity::EmptyBin: {
            const float lift = Lift(time);
            Lean(pose, body, 0.55f - 0.4f * lift, 0.3f - 0.2f * lift);
            LookDown(pose, body, 0.2f);
            const Vector3 sack =
                BetweenShoulders(pose, build) + Vector3(0.0f, (-0.86f + 0.3f * lift) * arm, -0.62f * arm);
            Reach(pose, build, false, sack + Vector3(-0.1f, 0.0f, 0.0f), ElbowDown(false));
            Reach(pose, build, true, sack + Vector3(0.1f, 0.0f, 0.0f), ElbowDown(true));
            break;
        }
        case StaffActivity::Tend: {
            // Down at the bed, a hand on the knee, the other digging with a trowel.
            Squat(pose, body, 1.2f);
            Lean(pose, body, 0.5f, 0.3f);
            LookDown(pose, body, 0.1f);
            const float dig = 0.5f + 0.5f * std::sin(time * 3.1f);
            Reach(pose, build, false, JointAt(pose, body, KneeLeft) + Vector3(-0.02f, 0.09f, -0.03f),
                  ElbowDown(false));
            Reach(pose, build, true,
                  Shoulder(pose, build, true) +
                      Vector3(-0.08f + 0.05f * std::sin(time * 0.7f), (-0.82f + 0.1f * dig) * arm,
                              (-0.5f - 0.08f * dig) * arm),
                  ElbowDown(true));
            break;
        }
        case StaffActivity::Water: {
            Lean(pose, body, 0.16f, 0.1f);
            LookDown(pose, body, 0.25f);
            Twist(pose, body, 0.2f * std::sin(time * 0.8f));
            Reach(pose, build, true, Shoulder(pose, build, true) + Vector3(0.02f, -0.7f * arm, -0.62f * arm),
                  ElbowDown(true));
            break;
        }
        default:
            break;
    }
}

// ---------------------------------------------------------------------------------------
// The tools
// ---------------------------------------------------------------------------------------

/// Builds coloured triangles in an avatar's own space and writes them out in the world's,
/// lit as the avatars are: the scene's sun and its ambient light, flat on every face.
class Painter {
public:
    Painter(std::vector<VertexPositionColor>& solid, std::vector<VertexPositionColor>& sheer, const Matrix& world,
            const Vector3& toSun, const Vector3& sun, const Vector3& ambient)
        : solid_(solid), sheer_(sheer), world_(world), toSun_(toSun), sun_(sun), ambient_(ambient) {}

    /// A face seen from the side away from `inside`.
    void Triangle(const Vector3& a, const Vector3& b, const Vector3& c, const Vector3& base,
                  const Vector3& inside) {
        Vector3 normal = Unit(Vector3::Cross(b - a, c - a), kUp);
        if (Dot(normal, (a + b + c) * (1.0f / 3.0f) - inside) < 0.0f)
            normal = normal * -1.0f;
        const Color colour = Lit(base, normal, 1.0f);
        solid_.emplace_back(Vector3::Transform(a, world_), colour);
        solid_.emplace_back(Vector3::Transform(b, world_), colour);
        solid_.emplace_back(Vector3::Transform(c, world_), colour);
    }

    void Quad(const Vector3& a, const Vector3& b, const Vector3& c, const Vector3& d, const Vector3& base,
              const Vector3& inside) {
        Triangle(a, b, c, base, inside);
        Triangle(a, c, d, base, inside);
    }

    /// A box by its centre and its three half extents.
    void Box(const Vector3& centre, const Vector3& x, const Vector3& y, const Vector3& z, const Vector3& base) {
        const auto corner = [&](int i) {
            return centre + x * ((i & 1) != 0 ? 1.0f : -1.0f) + y * ((i & 2) != 0 ? 1.0f : -1.0f) +
                   z * ((i & 4) != 0 ? 1.0f : -1.0f);
        };
        static constexpr int kFaces[6][4] = {{0, 1, 3, 2}, {4, 5, 7, 6}, {0, 1, 5, 4},
                                             {2, 3, 7, 6}, {0, 2, 6, 4}, {1, 3, 7, 5}};
        for (const auto& face : kFaces)
            Quad(corner(face[0]), corner(face[1]), corner(face[2]), corner(face[3]), base, centre);
    }

    /// A round body from `a` (radius `ra`) to `b` (radius `rb`), closed at the ends asked for.
    void Round(const Vector3& a, const Vector3& b, float ra, float rb, const Vector3& base, int sides,
               bool closedAtA, bool closedAtB) {
        const Vector3 axis = Unit(b - a, kUp);
        const Vector3 u = Unit(Vector3::Cross(axis, std::abs(axis.Y) < 0.9f ? kUp : kRight), kRight);
        const Vector3 v = Vector3::Cross(axis, u);
        const Vector3 middle = (a + b) * 0.5f;
        const auto spoke = [&](int i) {
            const float angle = 2.0f * kPi * static_cast<float>(i) / static_cast<float>(sides);
            return u * std::cos(angle) + v * std::sin(angle);
        };
        for (int i = 0; i < sides; ++i) {
            const Vector3 one = spoke(i);
            const Vector3 next = spoke(i + 1);
            Quad(a + one * ra, a + next * ra, b + next * rb, b + one * rb, base, middle);
            if (closedAtA)
                Triangle(a, a + one * ra, a + next * ra, base, middle);
            if (closedAtB)
                Triangle(b, b + one * rb, b + next * rb, base, middle);
        }
    }

    /// A thin bar: a shaft, a handle, a leg.
    void Rod(const Vector3& a, const Vector3& b, float radius, const Vector3& base) {
        Round(a, b, radius, radius, base, 5, false, false);
    }

    /// A flat round face looking along `axis`.
    void Disc(const Vector3& centre, const Vector3& axis, float radius, const Vector3& base, int sides) {
        const Vector3 along = Unit(axis, kUp);
        const Vector3 u = Unit(Vector3::Cross(along, std::abs(along.Y) < 0.9f ? kUp : kRight), kRight);
        const Vector3 v = Vector3::Cross(along, u);
        for (int i = 0; i < sides; ++i) {
            const float one = 2.0f * kPi * static_cast<float>(i) / static_cast<float>(sides);
            const float next = 2.0f * kPi * static_cast<float>(i + 1) / static_cast<float>(sides);
            Triangle(centre, centre + (u * std::cos(one) + v * std::sin(one)) * radius,
                     centre + (u * std::cos(next) + v * std::sin(next)) * radius, base, centre - along);
        }
    }

    /// See-through things (water): no face has a side, the light is what falls from above.
    void Sheer(const Vector3& a, const Vector3& b, const Vector3& c, const Vector3& base, float alphaA,
               float alphaB, float alphaC) {
        sheer_.emplace_back(Vector3::Transform(a, world_), Lit(base, kUp, alphaA));
        sheer_.emplace_back(Vector3::Transform(b, world_), Lit(base, kUp, alphaB));
        sheer_.emplace_back(Vector3::Transform(c, world_), Lit(base, kUp, alphaC));
    }

    /// The patch of shade under something standing at `feet`: dark in the middle, gone at the
    /// rim, lying where the light throws it (longer and fainter under a low sun).
    void Shade(const Vector3& feet, float radius) {
        const Vector3 at = Vector3::Transform(feet, world_) + Vector3(0.0f, 0.04f, 0.0f);
        const float flat = std::sqrt(toSun_.X * toSun_.X + toSun_.Z * toSun_.Z);
        const float bright = std::clamp((sun_.X + sun_.Y + sun_.Z) / 3.0f, 0.0f, 1.0f);
        const float low = toSun_.Y > 0.05f ? std::clamp(flat / toSun_.Y, 0.0f, 2.5f) : 2.5f;
        const Vector3 along =
            flat > 0.001f ? Vector3(-toSun_.X / flat, 0.0f, -toSun_.Z / flat) : Vector3(1.0f, 0.0f, 0.0f);
        const Vector3 across(-along.Z, 0.0f, along.X);
        const float reach = radius * (1.0f + 1.2f * low);
        const float strength = (0.2f + 0.3f * bright) / (1.0f + 0.25f * low);
        const Vector3 middle = at + along * (reach - radius);
        constexpr int kSides = 14;
        const auto rim = [&](int i, float share) {
            const float angle = 2.0f * kPi * static_cast<float>(i) / kSides;
            return middle + (along * (std::cos(angle) * reach) + across * (std::sin(angle) * radius)) * share;
        };
        const Color dark(0.0f, 0.0f, 0.0f, strength);
        const Color half(0.0f, 0.0f, 0.0f, strength * 0.55f);
        const Color none(0.0f, 0.0f, 0.0f, 0.0f);
        for (int i = 0; i < kSides; ++i) {
            sheer_.emplace_back(middle, dark);
            sheer_.emplace_back(rim(i, 0.5f), half);
            sheer_.emplace_back(rim(i + 1, 0.5f), half);
            sheer_.emplace_back(rim(i, 0.5f), half);
            sheer_.emplace_back(rim(i, 1.0f), none);
            sheer_.emplace_back(rim(i + 1, 1.0f), none);
            sheer_.emplace_back(rim(i, 0.5f), half);
            sheer_.emplace_back(rim(i + 1, 1.0f), none);
            sheer_.emplace_back(rim(i + 1, 0.5f), half);
        }
    }

private:
    /// Premultiplied, as BlendState::AlphaBlend takes it.
    Color Lit(const Vector3& base, const Vector3& normal, float alpha) const {
        const float facing = std::max(Dot(Vector3::TransformNormal(normal, world_), toSun_), 0.0f);
        const auto channel = [&](float albedo, float light, float sky) {
            return std::clamp(albedo * (sky + light * facing), 0.0f, 1.0f) * alpha;
        };
        return Color(channel(base.X, sun_.X, ambient_.X), channel(base.Y, sun_.Y, ambient_.Y),
                     channel(base.Z, sun_.Z, ambient_.Z), alpha);
    }

    std::vector<VertexPositionColor>& solid_;
    std::vector<VertexPositionColor>& sheer_;
    Matrix world_;
    Vector3 toSun_;
    Vector3 sun_;
    Vector3 ambient_;
};

const Vector3 kWood(0.62f, 0.47f, 0.28f);
const Vector3 kSteel(0.42f, 0.44f, 0.47f);
const Vector3 kBucketBlue(0.24f, 0.4f, 0.62f);
const Vector3 kFeed(0.66f, 0.5f, 0.24f);
const Vector3 kWater(0.72f, 0.85f, 0.95f);
const Vector3 kBarrowRed(0.6f, 0.2f, 0.13f);
const Vector3 kStraw(0.74f, 0.62f, 0.32f);
const Vector3 kRubber(0.1f, 0.1f, 0.11f);
const Vector3 kHoseGreen(0.16f, 0.42f, 0.2f);
const Vector3 kBrass(0.78f, 0.62f, 0.22f);
const Vector3 kPaper(0.93f, 0.93f, 0.9f);
const Vector3 kBoard(0.4f, 0.27f, 0.16f);
const Vector3 kSack(0.13f, 0.14f, 0.16f);
const Vector3 kCanGrey(0.6f, 0.65f, 0.68f);

/// A bucket by the middle of its rim; `up` is the way its mouth looks.
void Bucket(Painter& paint, const Vector3& rim, const Vector3& up, const Vector3& content) {
    const Vector3 bottom = rim - up * 0.26f;
    paint.Round(bottom, rim, 0.1f, 0.135f, kBucketBlue, 10, true, false);
    paint.Disc(rim - up * 0.035f, up, 0.127f, content, 10);
}

/// A bucket hanging from the hand by its handle.
void HangingBucket(Painter& paint, const Vector3& hand, const Vector3& content) {
    const Vector3 rim = hand - kUp * 0.17f;
    Bucket(paint, rim, kUp, content);
    paint.Rod(hand, rim + kForward * 0.13f, 0.006f, kSteel);
    paint.Rod(hand, rim - kForward * 0.13f, 0.006f, kSteel);
}

/// A rake or a broom: the shaft from `butt` along `along`, the head at its far end.
void LongTool(Painter& paint, const Vector3& butt, const Vector3& along, bool broom) {
    const float length = broom ? 1.45f : 1.7f;
    const Vector3 end = butt + along * length;
    paint.Rod(butt, end, 0.014f, kWood);
    const Vector3 across = Unit(Vector3::Cross(along, kUp), kRight);
    if (broom) {
        // The bristles go on the way the shaft points.
        paint.Box(end + along * 0.06f, across * 0.17f, along * 0.08f, Vector3::Cross(across, along) * 0.03f,
                  kStraw);
    } else {
        // The tines stand across the shaft, towards the ground when the head is down.
        Vector3 tines = Vector3::Cross(across, along);
        if (tines.Y * along.Y < 0.0f)
            tines = tines * -1.0f;
        paint.Box(end, across * 0.24f, along * 0.012f, tines * 0.012f, kSteel);
        paint.Box(end + tines * 0.045f, across * 0.24f, along * 0.004f, tines * 0.045f, kSteel);
    }
}

/// A wheelbarrow by the middle between its grips, looking along `ahead` (level, unit).
/// The wheel stands on the ground (y = 0 of the avatar's space).
void Barrow(Painter& paint, const Vector3& grips, const Vector3& ahead, bool withRake) {
    const Vector3 right = Vector3::Cross(ahead, kUp);
    const Vector3 axle(grips.X + ahead.X * 1.34f, 0.19f, grips.Z + ahead.Z * 1.34f);
    const Vector3 tray = Lerp(grips, axle, 0.56f) + kUp * 0.1f;
    // The tray tilts with the shafts: its floor runs from the grips' height down to the axle.
    const Vector3 slope = Unit(axle + kUp * 0.2f - grips, ahead);
    const Vector3 lift = Vector3::Cross(right, slope);
    paint.Box(tray, right * 0.29f, lift * 0.13f, slope * 0.38f, kBarrowRed);
    paint.Box(tray + lift * 0.15f, right * 0.25f, lift * 0.05f, slope * 0.32f, kStraw * 0.8f);
    paint.Round(axle - right * 0.04f, axle + right * 0.04f, 0.19f, 0.19f, kRubber, 12, true, true);
    for (const float side : {-1.0f, 1.0f}) {
        const Vector3 grip = grips + right * (0.27f * side);
        const Vector3 under = tray + right * (0.24f * side) - slope * 0.36f - lift * 0.14f;
        paint.Rod(grip, under, 0.016f, kSteel);
        paint.Rod(under, axle + right * (0.06f * side), 0.016f, kSteel);
        // The legs: a little off the ground while it is pushed.
        const Vector3 knee = tray + right * (0.22f * side) - slope * 0.2f - lift * 0.13f;
        paint.Rod(knee, Vector3(knee.X - ahead.X * 0.06f, std::max(grips.Y - 0.66f, 0.0f), knee.Z - ahead.Z * 0.06f),
                  0.014f, kSteel);
    }
    if (withRake)
        LongTool(paint, tray - slope * 0.75f + lift * 0.24f + right * 0.1f, slope, false);
}

/// Feed or water falling from `from`: a few bits of it, each `fall` metres on its way.
void Falling(Painter& paint, const Vector3& from, float time, float fall, bool water) {
    constexpr int kBits = 9;
    for (int i = 0; i < kBits; ++i) {
        const float t = Fraction(time * 1.7f + static_cast<float>(i) / kBits);
        const float wobble = zoo::HashFloat(static_cast<std::uint32_t>(i), 0x51u) - 0.5f;
        const Vector3 at = from + Vector3(0.05f * wobble, -fall * t * t, -0.07f * t + 0.03f * wobble);
        if (water) {
            const Vector3 wide(0.014f, 0.0f, 0.0f);
            const Vector3 tall(0.0f, 0.05f + 0.06f * t, 0.0f);
            paint.Sheer(at - wide, at + wide, at + wide - tall, kWater, 0.6f, 0.6f, 0.4f);
            paint.Sheer(at - wide, at + wide - tall, at - wide - tall, kWater, 0.6f, 0.4f, 0.4f);
        } else {
            paint.Box(at, kRight * 0.012f, kUp * 0.012f, kForward * 0.012f, kFeed);
        }
    }
}

/// A jet of water from `from`, leaving at `velocity` (m/s) and falling to the ground: two
/// ribbons crossed, widening on the way, and the splash where it lands.
void Jet(Painter& paint, const Vector3& from, const Vector3& velocity, float time) {
    constexpr float kGravity = 9.8f;
    // When it is down: from.Y + v t - g t^2 / 2 = 0.
    const float flight =
        (velocity.Y + std::sqrt(velocity.Y * velocity.Y + 2.0f * kGravity * std::max(from.Y, 0.05f))) / kGravity;
    const Vector3 level = Unit(Vector3(velocity.X, 0.0f, velocity.Z), kForward);
    const Vector3 across = Vector3::Cross(level, kUp);
    constexpr int kParts = 9;
    const auto at = [&](int i) {
        const float t = flight * static_cast<float>(i) / kParts;
        return from + velocity * t - kUp * (0.5f * kGravity * t * t);
    };
    for (int i = 0; i < kParts; ++i) {
        const float one = static_cast<float>(i) / kParts;
        const float next = static_cast<float>(i + 1) / kParts;
        const float wideOne = 0.012f + 0.07f * one;
        const float wideNext = 0.012f + 0.07f * next;
        // The water breaks up as it goes; it flickers a little.
        const float alphaOne = (0.62f - 0.3f * one) * (0.85f + 0.15f * std::sin(40.0f * one - 23.0f * time));
        const float alphaNext = (0.62f - 0.3f * next) * (0.85f + 0.15f * std::sin(40.0f * next - 23.0f * time));
        const Vector3 a = at(i);
        const Vector3 b = at(i + 1);
        for (const Vector3& wide : {across, kUp}) {
            paint.Sheer(a - wide * wideOne, a + wide * wideOne, b + wide * wideNext, kWater, alphaOne, alphaOne,
                        alphaNext);
            paint.Sheer(a - wide * wideOne, b + wide * wideNext, b - wide * wideNext, kWater, alphaOne, alphaNext,
                        alphaNext);
        }
    }
    const Vector3 landing(at(kParts).X, 0.03f, at(kParts).Z);
    constexpr int kSides = 10;
    const float spread = 0.32f + 0.06f * std::sin(time * 17.0f);
    for (int i = 0; i < kSides; ++i) {
        const float one = 2.0f * kPi * static_cast<float>(i) / kSides;
        const float next = 2.0f * kPi * static_cast<float>(i + 1) / kSides;
        paint.Sheer(landing, landing + Vector3(std::cos(one), 0.0f, std::sin(one)) * spread,
                    landing + Vector3(std::cos(next), 0.0f, std::sin(next)) * spread, kWater, 0.45f, 0.0f, 0.0f);
    }
}

/// Where the wheelbarrow is set down beside somebody raking at `feet`, facing `heading`, in
/// the avatar's space: the grips, and the way the wheel points.
Vector3 BarrowGrips(float side) { return Vector3(1.05f * side, 0.6f, 0.25f); }
Vector3 BarrowAhead(float side) { return Unit(Vector3(-0.35f * side, 0.0f, -1.0f), kForward); }

/// The side the wheelbarrow can stand on beside somebody raking at `feet` facing `heading`:
/// the left (-1) or the right (1), whichever keeps it on dry ground and on this side of every
/// fence, gates included (it is not parked in the keeper's doorway); 0 when neither does.
float BarrowSide(const zoo::Zoo& zoo, zoo::Vec2 feet, float heading) {
    constexpr zoo::NavProfile profile{.keepOffWater = true};
    const float c = std::cos(heading);
    const float s = std::sin(heading);
    // The avatar's space turned by heading + pi, as Draw() places the avatar.
    const auto placed = [&](const Vector3& at) { return feet + zoo::Vec2{-at.X * c - at.Z * s, at.X * s - at.Z * c}; };
    for (const float side : {-1.0f, 1.0f}) {
        // The corners of what it covers, with a hand's breadth to spare: both grips, the
        // wheel. Every one must be reached from the feet without crossing a fence.
        const Vector3 grips = BarrowGrips(side);
        const Vector3 ahead = BarrowAhead(side);
        const Vector3 across = Vector3::Cross(ahead, kUp);
        const std::array<Vector3, 4> corners = {grips + across * 0.4f - ahead * 0.15f,
                                                grips - across * 0.4f - ahead * 0.15f,
                                                grips + ahead * 1.6f + across * 0.15f,
                                                grips + ahead * 1.6f - across * 0.15f};
        const bool clear = std::ranges::all_of(
            corners, [&](const Vector3& corner) { return zoo.Routes().Walkable(feet, placed(corner), profile); });
        if (clear)
            return side;
    }
    return 0.0f;
}

/// The tools of a job in the hands of somebody posed for it.
void WorkTools(Painter& paint, const AvatarPose& pose, const Build& build, StaffActivity work,
               StaffActivity approach, float time, float barrowSide) {
    const AvatarBody& body = build.body;
    const Vector3 left = Grip(pose, body, false);
    const Vector3 right = Grip(pose, body, true);
    const Vector3 between = (left + right) * 0.5f;
    switch (work) {
        case StaffActivity::FillFeeder:
        case StaffActivity::FillTrough: {
            const bool water = work == StaffActivity::FillTrough;
            const float tip = Tip(time);
            const float angle = 0.25f + 1.2f * tip;
            const Vector3 up(0.0f, std::cos(angle), -std::sin(angle));
            const Vector3 out(0.0f, -std::sin(angle), -std::cos(angle));
            Bucket(paint, between, up, water ? kWater * 0.8f : kFeed);
            if (tip > 0.55f)
                Falling(paint, between + out * 0.13f, time, water ? 0.5f : 0.4f, water);
            break;
        }
        case StaffActivity::ThrowFeed: {
            HangingBucket(paint, left, kFeed);
            // What was thrown flies in a low arc from where the hand let go.
            const float t = Fraction(time / kThrowPeriod);
            const float flown = (t - kThrowAt) * kThrowPeriod;
            if (flown > 0.0f && flown < 0.95f) {
                const Vector3 from = Shoulder(pose, build, true) + ThrowHand(kThrowAt) * build.Arm();
                for (int i = 0; i < 12; ++i) {
                    const float a = zoo::HashFloat(static_cast<std::uint32_t>(i), 0x71u) - 0.5f;
                    const float b = zoo::HashFloat(static_cast<std::uint32_t>(i), 0x72u);
                    const Vector3 velocity(1.6f * a, 2.2f + 1.2f * b, -4.6f - 1.5f * b);
                    const Vector3 at = from + velocity * flown - kUp * (4.9f * flown * flown);
                    if (at.Y > 0.02f)
                        paint.Box(at, kRight * 0.016f, kUp * 0.016f, kForward * 0.016f, kFeed);
                }
            }
            break;
        }
        case StaffActivity::Rake:
        case StaffActivity::Sweep: {
            const Vector3 along = Unit(right - left, Vector3(0.0f, -0.7f, -0.7f));
            LongTool(paint, left - along * 0.12f, along, work == StaffActivity::Sweep);
            if (approach == StaffActivity::Push && barrowSide != 0.0f)
                Barrow(paint, BarrowGrips(barrowSide), BarrowAhead(barrowSide), false);
            break;
        }
        case StaffActivity::Hose: {
            const float swing = HoseSwing(time);
            // The trunk's turn carries the hand; the nozzle points the way the chest does.
            const Vector3 aim = Unit(Vector3(-std::sin(swing), 0.16f, -std::cos(swing)), kForward);
            paint.Rod(right - aim * 0.05f, right + aim * 0.17f, 0.02f, kBrass);
            const Vector3 nozzle = right + aim * 0.17f;
            Jet(paint, nozzle, aim * 6.2f, time);
            const Vector3 sag(left.X - 0.1f, 0.45f, left.Z + 0.35f);
            const Vector3 ground(sag.X - 0.1f, 0.02f, sag.Z + 0.5f);
            const Vector3 reel(ground.X + 0.25f, 0.02f, ground.Z + 1.2f);
            paint.Rod(right - aim * 0.05f, left, 0.015f, kHoseGreen);
            paint.Rod(left, sag, 0.015f, kHoseGreen);
            paint.Rod(sag, ground, 0.015f, kHoseGreen);
            paint.Rod(ground, reel, 0.015f, kHoseGreen);
            paint.Round(reel, reel + kUp * 0.1f, 0.2f, 0.2f, kHoseGreen, 10, false, true);
            break;
        }
        case StaffActivity::Inspect: {
            // The clipboard lies on the forearm, its face up and towards the reader.
            const Vector3 face = Unit(Vector3(0.0f, 0.8f, 0.6f), kUp);
            const Vector3 tall = Vector3::Cross(kRight, face);
            const Vector3 centre = left + Vector3(0.05f, 0.05f, -0.06f);
            paint.Box(centre, kRight * 0.11f, tall * 0.15f, face * 0.006f, kBoard);
            paint.Box(centre + face * 0.008f, kRight * 0.095f, tall * 0.13f, face * 0.002f, kPaper);
            break;
        }
        case StaffActivity::EmptyBin: {
            const Vector3 neck = between - kUp * 0.03f;
            paint.Round(neck, neck - kUp * 0.2f, 0.05f, 0.2f, kSack, 8, false, false);
            paint.Round(neck - kUp * 0.2f, neck - kUp * 0.46f, 0.2f, 0.12f, kSack, 8, false, true);
            break;
        }
        case StaffActivity::Tend: {
            const Vector3 along = Unit(Vector3(0.0f, -0.75f, -0.65f), kForward);
            paint.Rod(right - along * 0.04f, right + along * 0.1f, 0.016f, kWood);
            paint.Box(right + along * 0.19f, kRight * 0.035f, along * 0.09f, Vector3::Cross(kRight, along) * 0.004f,
                      kSteel);
            break;
        }
        case StaffActivity::Water: {
            // The can tipped forward, water falling from its rose.
            const float angle = 0.45f + 0.1f * std::sin(time * 1.3f);
            const Vector3 up(0.0f, std::cos(angle), -std::sin(angle));
            const Vector3 out(0.0f, -std::sin(angle), -std::cos(angle));
            const Vector3 can = right - up * 0.2f;
            paint.Round(can - up * 0.13f, can + up * 0.13f, 0.11f, 0.11f, kCanGrey, 10, true, true);
            paint.Rod(right - out * 0.09f, can + up * 0.13f - out * 0.08f, 0.012f, kCanGrey);
            paint.Rod(right + out * 0.06f, can + up * 0.13f + out * 0.06f, 0.012f, kCanGrey);
            const Vector3 rose = can + out * 0.44f + up * 0.1f;
            paint.Rod(can + out * 0.1f - up * 0.08f, rose, 0.02f, kCanGrey);
            paint.Disc(rose, out, 0.045f, kCanGrey, 8);
            Falling(paint, rose + out * 0.02f, time, std::max(rose.Y - 0.05f, 0.1f), true);
            break;
        }
        default:
            break;
    }
}

/// What is carried on the way, in the hands of somebody posed for it.
void HeldTools(Painter& paint, const AvatarPose& pose, const Build& build, Held held) {
    const AvatarBody& body = build.body;
    const Vector3 left = Grip(pose, body, false);
    const Vector3 right = Grip(pose, body, true);
    switch (held) {
        case Held::Bucket:
            HangingBucket(paint, right, kFeed);
            break;
        case Held::Can: {
            const Vector3 can = right - kUp * 0.2f;
            paint.Round(can - kUp * 0.13f, can + kUp * 0.13f, 0.11f, 0.11f, kCanGrey, 10, true, true);
            paint.Rod(right - kForward * 0.08f, can + kUp * 0.13f - kForward * 0.08f, 0.012f, kCanGrey);
            paint.Rod(right + kForward * 0.06f, can + kUp * 0.13f + kForward * 0.06f, 0.012f, kCanGrey);
            const Vector3 rose = can + kForward * 0.44f + kUp * 0.1f;
            paint.Rod(can + kForward * 0.1f - kUp * 0.08f, rose, 0.02f, kCanGrey);
            paint.Disc(rose, kForward, 0.045f, kCanGrey, 8);
            break;
        }
        case Held::Barrow:
            Barrow(paint, (left + right) * 0.5f, kForward, true);
            paint.Shade(Vector3(0.0f, 0.0f, (left.Z + right.Z) * 0.5f - 0.8f), 0.42f);
            break;
        case Held::Rake:
        case Held::Broom: {
            // Upright in the hand, the head up.
            const Vector3 along = Unit(Vector3(0.0f, 1.0f, -0.1f), kUp);
            LongTool(paint, right - along * std::min(0.8f, std::max(right.Y - 0.06f, 0.0f)), along,
                     held == Held::Broom);
            break;
        }
        case Held::Board: {
            // Down at the side, flat against the leg.
            const Vector3 centre = left - kUp * 0.1f - kRight * 0.02f;
            paint.Box(centre, kForward * 0.11f, kUp * 0.15f, kRight * 0.006f, kBoard);
            paint.Box(centre - kRight * 0.008f, kForward * 0.095f, kUp * 0.13f, kRight * 0.002f, kPaper);
            break;
        }
        case Held::Nothing:
            break;
    }
}

/// Restores what the engine's renderer had set when a pass of the game's own is over.
struct DeviceState {
    explicit DeviceState(GraphicsDevice& of)
        : device(of), blend(of.getBlendStateProperty()), depth(of.getDepthStencilStateProperty()),
          rasterizer(of.getRasterizerStateProperty()) {}
    ~DeviceState() {
        device.setBlendStateProperty(blend);
        device.setDepthStencilStateProperty(depth);
        device.setRasterizerStateProperty(rasterizer);
    }
    GraphicsDevice& device;
    Microsoft::Xna::Framework::Graphics::BlendState blend;
    Microsoft::Xna::Framework::Graphics::DepthStencilState depth;
    Microsoft::Xna::Framework::Graphics::RasterizerState rasterizer;
};

bool IsWalking(StaffActivity activity) {
    return activity == StaffActivity::Walk || activity == StaffActivity::Carry ||
           activity == StaffActivity::Push;
}

}  // namespace

KeeperRenderer::KeeperRenderer() = default;
KeeperRenderer::~KeeperRenderer() = default;

void KeeperRenderer::Clear() {
    actors_.clear();
    inView_.clear();
    tick_ = -1;
    drawn_ = 0;
}

void KeeperRenderer::PollCast() {
    if (!castMade_) {
        castMade_ = true;
        std::size_t uniformed = 0;
        for (std::size_t i = 0; i < kCast; ++i) {
            // Women and men in turn. The fixed cast makes repeated evidence and saved zoos look alike.
            bool dressed = false;
            auto member = std::make_unique<Member>(Member{.description = Engage(StaffAppearance(i), dressed),
                                                          .renderer = nullptr,
                                                          .body = {},
                                                          .height = 1.7f,
                                                          .upperArm = 0.0f,
                                                          .foreArm = 0.0f,
                                                          .ready = false,
                                                          .failed = false});
            member->renderer = std::make_unique<AvatarRenderer>(&member->description, false);
            cast_.push_back(std::move(member));
            uniformed += dressed ? 1u : 0u;
        }
        if (uniformed < kCast)
            std::printf("zoo: %zu of %zu keepers wear their own clothes: CNA did not take the uniform\n",
                        kCast - uniformed, kCast);
    }
    for (const auto& slot : cast_) {
        Member& member = *slot;
        if (member.ready || member.failed)
            continue;
        const AvatarRendererState state = member.renderer->getStateProperty();
        if (state == AvatarRendererState::Unavailable) {
            member.failed = true;
            if (!reported_) {
                reported_ = true;
                std::printf("zoo: a keeper cannot be drawn: CNA has no avatar for them\n");
            }
            continue;
        }
        if (state != AvatarRendererState::Ready)
            continue;
        const auto& parents = member.renderer->getParentBonesProperty();
        const auto& bind = member.renderer->getBindPoseProperty();
        if (parents.getCountProperty() != kAvatarBones || bind.getCountProperty() != kAvatarBones) {
            member.failed = true;
            continue;
        }
        std::array<int, kAvatarBones> parent{};
        std::array<Vector3, kAvatarBones> offset{};
        for (int b = 0; b < kAvatarBones; ++b) {
            parent[static_cast<std::size_t>(b)] = parents[b];
            offset[static_cast<std::size_t>(b)] = bind[b].getTranslationProperty();
        }
        member.body.Measure(parent, offset);
        const float height = member.description.getHeightProperty();
        member.height = height > 0.8f && height < 2.5f ? height : member.body.top;
        // The arms are solved only on a skeleton whose arm is shoulder, elbow, wrist in a row.
        const auto chained = [&](int shoulder, int elbow, int wrist) {
            return parent[static_cast<std::size_t>(elbow)] == shoulder &&
                   parent[static_cast<std::size_t>(wrist)] == elbow;
        };
        if (chained(ShoulderLeft, ElbowLeft, WristLeft) && chained(ShoulderRight, ElbowRight, WristRight)) {
            member.upperArm = Length(offset[static_cast<std::size_t>(ElbowRight)]);
            member.foreArm = Length(offset[static_cast<std::size_t>(WristRight)]);
        }
        if (member.upperArm < 0.05f || member.foreArm < 0.05f) {
            member.upperArm = member.foreArm = 0.0f;
            std::printf("zoo: a keeper's arms are not as expected: tools are not put in the hands\n");
        }
        member.ready = true;
    }
}

const KeeperRenderer::Member* KeeperRenderer::Playing(const Actor& actor) const {
    for (std::size_t i = 0; i < cast_.size(); ++i) {
        const Member& member = *cast_[(actor.member + i) % cast_.size()];
        if (member.ready)
            return &member;
    }
    return nullptr;
}

void KeeperRenderer::Update(const zoo::Zoo& zoo, float seconds, float tickFraction) {
    PollCast();
    if (!standBaked_) {
        standBaked_ = true;
        stand_ = AvatarClip::Bake(AvatarAnimationPreset::Stand0);
    }
    const zoo::World& world = zoo.Map();
    const zoo::GameTime now = zoo.Time();
    const bool stepped = now.tick != tick_;
    tick_ = now.tick;
    const float alpha = std::clamp(tickFraction, 0.0f, 1.0f);
    for (Actor& actor : actors_)
        actor.seen = false;

    for (const zoo::StaffMember& staff : zoo.Keepers().All()) {
        auto found = std::find_if(actors_.begin(), actors_.end(),
                                  [&](const Actor& one) { return one.id == staff.id; });
        const bool fresh = found == actors_.end();
        if (fresh) {
            actors_.emplace_back();
            found = actors_.end() - 1;
            found->id = staff.id;
            found->member = staff.look % kCast;
            // The crew does not breathe or step in time with one another.
            found->baseTime = 20.0f * zoo::HashFloat(staff.id.value, 1);
            found->phase = zoo::HashFloat(staff.id.value, 2);
        }
        Actor& actor = *found;
        actor.seen = true;

        // Where they are: between the last two places the simulation gave them.
        const zoo::Vec2 before = actor.shown;
        if (fresh) {
            actor.from = actor.to = actor.shown = staff.position;
            actor.heading = staff.heading;
        } else if (stepped) {
            actor.from = actor.to;
            actor.to = staff.position;
            const zoo::Vec2 jump = actor.to - actor.from;
            if (jump.x * jump.x + jump.z * jump.z > 16.0f) {
                actor.from = actor.to;
                actor.heading = staff.heading;
            }
        }
        actor.shown = actor.from + (actor.to - actor.from) * alpha;
        actor.ground = DrawnGroundHeight(world, actor.shown);  // the ground as drawn (docs/GROUND.md)
        const float turn = zoo::WrapAngle(staff.heading - actor.heading);
        const float most = 4.0f * seconds;
        actor.heading = zoo::WrapAngle(actor.heading + std::clamp(turn, -most, most));

        // What they do. A pose is left before the next one is taken, so that a rake does
        // not turn into a bucket in the hands.
        const zoo::StaffTask* task = staff.Task();
        const bool working = staff.working && task != nullptr && !IsWalking(staff.activity);
        const StaffActivity work = task != nullptr ? task->work : StaffActivity::Rest;
        const StaffActivity approach = task != nullptr ? task->approach : StaffActivity::Walk;
        if (fresh) {
            actor.work = work;
            actor.approach = approach;
            actor.working = working;
            actor.workWeight = working ? 1.0f : 0.0f;
            actor.holdWeight = working ? 0.0f : 1.0f;
            // In the middle of it, not all of the crew at the first moment of their jobs.
            actor.workTime = 9.0f * zoo::HashFloat(staff.id.value, 3);
            if (task != nullptr)
                actor.barrowSide = BarrowSide(zoo, task->place, task->facing);
        }
        if (seconds <= 0.0f)
            continue;
        const bool sameWork = working && actor.working && actor.work == work;
        const bool sameHold = !working && !actor.working && actor.work == work && actor.approach == approach;
        actor.workWeight = Ease(actor.workWeight, sameWork ? 1.0f : 0.0f, 5.0f, seconds);
        actor.holdWeight = Ease(actor.holdWeight, sameHold ? 1.0f : 0.0f, 5.0f, seconds);
        if (!sameWork && !sameHold && actor.workWeight < 0.04f && actor.holdWeight < 0.04f) {
            actor.work = work;
            actor.approach = approach;
            actor.working = working;
            actor.workTime = 0.0f;
            // Where the job is done and which way they will face there, not where they are now.
            if (task != nullptr)
                actor.barrowSide = BarrowSide(zoo, task->place, task->facing);
        }
        actor.workTime += seconds;
        actor.baseTime += seconds;

        const zoo::Vec2 step = fresh ? zoo::Vec2{} : actor.shown - before;
        const float covered = std::sqrt(step.x * step.x + step.z * step.z);
        const bool moving = IsWalking(staff.activity) && !staff.working && staff.speed > 0.05f;
        actor.gait = Ease(actor.gait, moving ? 1.0f : 0.0f, 4.5f, seconds);
        const Member* member = Playing(actor);
        const float stride = member != nullptr ? StrideLength(member->body, 0.0f) : 1.2f;
        actor.phase += covered / std::max(stride, 0.2f);
        actor.phase -= std::floor(actor.phase);
    }
    std::erase_if(actors_, [](const Actor& actor) { return !actor.seen; });
}

void KeeperRenderer::Pose(const Actor& actor, const Member& member) {
    const AvatarBody& body = member.body;
    stand_.Sample(actor.baseTime, true, pose_, expression_);
    if (actor.gait > 0.001f) {
        AvatarPose step = pose_;
        ApplyGait(step, body, actor.phase, 0.0f);
        Blend(pose_, step, SmoothStep(actor.gait));
    }
    if (member.upperArm > 0.0f) {
        const Build build{body, member.upperArm, member.foreArm};
        if (actor.holdWeight > 0.001f) {
            AvatarPose held = pose_;
            PoseHold(held, build, HeldFor(actor.approach, actor.work));
            Blend(pose_, held, SmoothStep(actor.holdWeight));
        }
        if (actor.workWeight > 0.001f) {
            AvatarPose work = pose_;
            PoseWork(work, build, actor.work, actor.workTime);
            Blend(pose_, work, SmoothStep(actor.workWeight));
        }
    }
    ToMatrices(pose_, bones_);
}

void KeeperRenderer::Draw(GraphicsDevice& device, const CnaEngine::Graphics::Camera& camera,
                          const CnaEngine::Graphics::ForwardLighting& lighting) {
    namespace Xna = Microsoft::Xna::Framework::Graphics;
    drawn_ = 0;
    const auto& frustum = camera.Frustum();
    const Vector3 eye = camera.Position();
    inView_.clear();
    for (Actor& actor : actors_) {
        if (Playing(actor) == nullptr)
            continue;
        const Vector3 feet(actor.shown.x, actor.ground, actor.shown.z);
        const Vector3 away = feet - eye;
        const float distance = away.X * away.X + away.Y * away.Y + away.Z * away.Z;
        if (distance > kFarthest * kFarthest)
            continue;
        // Room for the tools as well: a rake reaches out, a jet of water lands metres away.
        if (frustum.Contains(BoundingSphere(feet + Vector3(0.0f, 0.9f, 0.0f), 2.6f)) == ContainmentType::Disjoint)
            continue;
        inView_.emplace_back(distance, &actor);
    }
    if (inView_.empty())
        return;
    std::sort(inView_.begin(), inView_.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    if (inView_.size() > kMost)
        inView_.resize(kMost);

    // The scene's sun, and its sky as light from everywhere, as the visitors are lit.
    const Vector3 sun = lighting.lightColor * 0.9f;
    Vector3 ambient = lighting.ambientColor;
    if (!lighting.fillLights.empty())
        ambient = ambient + lighting.fillLights.front().diffuse * 0.55f;
    const Vector3 toSun = Unit(lighting.lightDirection * -1.0f, kUp);

    solid_.clear();
    sheer_.clear();
    for (const auto& [distance, seen] : inView_) {
        const Actor& actor = *seen;
        const Member& member = *Playing(actor);
        const Vector3 feet(actor.shown.x, actor.ground, actor.shown.z);
        Pose(actor, member);
        // An avatar looks along -Z; a heading of 0 looks along +Z.
        const Matrix world = Matrix::CreateRotationY(actor.heading + kPi) * Matrix::CreateTranslation(feet);
        AvatarRenderer& renderer = *member.renderer;
        renderer.setWorldProperty(world);
        renderer.setViewProperty(camera.View());
        renderer.setProjectionProperty(camera.Projection());
        renderer.setLightDirectionProperty(lighting.lightDirection);
        renderer.setLightColorProperty(sun);
        renderer.setAmbientLightColorProperty(ambient);
        renderer.Draw(bones_, expression_);
        ++drawn_;

        Painter paint(solid_, sheer_, world, toSun, sun, ambient);
        paint.Shade(Vector3(0.0f, 0.0f, 0.0f), 0.34f);
        if (member.upperArm <= 0.0f)
            continue;
        const Build build{member.body, member.upperArm, member.foreArm};
        if (actor.working && actor.workWeight > 0.3f)
            WorkTools(paint, pose_, build, actor.work, actor.approach, actor.workTime, actor.barrowSide);
        else if (!actor.working && actor.holdWeight > 0.3f)
            HeldTools(paint, pose_, build, HeldFor(actor.approach, actor.work));
    }

    if (effect_ == nullptr) {
        effect_ = std::make_unique<Xna::BasicEffect>(device);
        effect_->setLightingEnabledProperty(false);
        effect_->setTextureEnabledProperty(false);
        effect_->setVertexColorEnabledProperty(true);
    }
    const DeviceState restore(device);
    effect_->setWorldProperty(Matrix::getIdentityProperty());
    effect_->setViewProperty(camera.View());
    effect_->setProjectionProperty(camera.Projection());
    device.setRasterizerStateProperty(Xna::RasterizerState::CullNone);
    if (!solid_.empty()) {
        device.setBlendStateProperty(Xna::BlendState::Opaque);
        device.setDepthStencilStateProperty(Xna::DepthStencilState::Default);
        effect_->Apply();
        device.DrawUserPrimitives(Xna::PrimitiveType::TriangleList, solid_.data(), 0,
                                  static_cast<int>(solid_.size() / 3));
    }
    if (!sheer_.empty()) {
        device.setBlendStateProperty(Xna::BlendState::AlphaBlend);
        device.setDepthStencilStateProperty(Xna::DepthStencilState::DepthRead);
        effect_->Apply();
        device.DrawUserPrimitives(Xna::PrimitiveType::TriangleList, sheer_.data(), 0,
                                  static_cast<int>(sheer_.size() / 3));
    }
}

std::optional<Vector3> KeeperRenderer::PositionOf(zoo::StaffId id) const {
    for (const Actor& actor : actors_) {
        if (actor.id == id)
            return Vector3(actor.shown.x, actor.ground, actor.shown.z);
    }
    return std::nullopt;
}

}  // namespace zoogame
