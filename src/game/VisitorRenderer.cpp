// SPDX-License-Identifier: MIT
#include "VisitorRenderer.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>

#include "Microsoft/Xna/Framework/BoundingFrustum.hpp"
#include "Microsoft/Xna/Framework/BoundingSphere.hpp"
#include "Microsoft/Xna/Framework/GamerServices/AvatarBodyType.hpp"
#include "Microsoft/Xna/Framework/GamerServices/AvatarRendererState.hpp"
#include "Microsoft/Xna/Framework/Vector2.hpp"
#include "Microsoft/Xna/Framework/Vector4.hpp"

#include "zoo/base/Rng.hpp"

namespace zoogame {

namespace {

using Microsoft::Xna::Framework::BoundingSphere;
using Microsoft::Xna::Framework::ContainmentType;
using Microsoft::Xna::Framework::GamerServices::AvatarBodyType;
using Microsoft::Xna::Framework::GamerServices::AvatarDescription;
using Microsoft::Xna::Framework::GamerServices::AvatarRenderer;
using Microsoft::Xna::Framework::GamerServices::AvatarRendererState;
using Microsoft::Xna::Framework::Graphics::GraphicsDevice;
using Microsoft::Xna::Framework::Vector2;
using Microsoft::Xna::Framework::Vector4;
using zoo::VisitorActivity;
using zoo::VisitorThought;

constexpr float kPi = 3.14159265f;
/// How many different people there are. Each takes CNA a moment to assemble
/// and holds its own meshes and textures.
constexpr std::size_t kCast = 28;
/// How many are assembled at the same time.
constexpr int kAssembling = 4;
/// Nobody is drawn farther from the camera than this, metres.
constexpr float kFarthest = 260.0f;
/// A person nearer than this is drawn as an avatar; one who was an avatar stays one up to
/// the second distance. At 16 m a person stands some 70 pixels tall on a 720-line screen
/// and a picture of 96 lines is as sharp as the avatar.
constexpr float kCloseNear = 16.0f;
constexpr float kCloseFar = 20.0f;
/// How many people a frame draws as avatars at most (the nearest), when it can draw pictures.
constexpr std::size_t kAvatars = 18;
/// The patch of shade under a person is not drawn farther away than this.
constexpr float kShadeFarthest = 90.0f;
/// A member's sheet of pictures: one column a pose (standing, sitting, the moments of a
/// step), one row a side it is seen from.
constexpr int kCellWide = 64;
constexpr int kCellTall = 96;
constexpr int kSheetSteps = 8;
constexpr int kSheetStand = 0;
constexpr int kSheetSit = 1;
constexpr int kSheetWalk = 2;
constexpr int kSheetPoses = kSheetWalk + kSheetSteps;
constexpr int kSheetSides = 8;
/// The light the pictures are drawn in.
constexpr float kSheetAmbient = 0.62f;
constexpr float kSheetLight = 0.5f;
/// The seat of a bench above the ground, and the hip joint above the seat, metres.
constexpr float kSeatHeight = 0.43f;
constexpr float kHipAboveSeat = 0.1f;

/// `value` moved towards `goal` at `rate` per second.
float Ease(float value, float goal, float rate, float seconds) {
    const float most = rate * seconds;
    return value + std::clamp(goal - value, -most, most);
}

float SmoothStep(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

bool OnTheMove(VisitorActivity activity) {
    return activity == VisitorActivity::Arriving || activity == VisitorActivity::Walking ||
           activity == VisitorActivity::Leaving || activity == VisitorActivity::Fleeing;
}

}  // namespace

VisitorRenderer::VisitorRenderer() {
    if (const char* pose = std::getenv("ZOO_VISITOR_POSE"); pose != nullptr) {
        const std::string_view name(pose);
        using Manner = Actor::Manner;
        show_ = name == "lean"     ? Manner::Lean
                : name == "point"  ? Manner::Point
                : name == "camera" ? Manner::Camera
                : name == "phone"  ? Manner::Phone
                : name == "crouch" ? Manner::Crouch
                                   : Manner::None;
        showOld_ = name == "old";
    }
    cast_.reserve(kCast);
    GrowCast();
}

VisitorRenderer::~VisitorRenderer() {
    if (std::getenv("ZOO_AVATAR_DEBUG") != nullptr && frames_ > 0) {
        const double frames = static_cast<double>(frames_);
        std::printf(
            "zoo: visitors: %d of the cast ready; a frame drew %.1f people, %.1f of them as avatars; posing "
            "%.3f ms, drawing avatars %.3f ms, flat people %.3f ms\n",
            ready_, static_cast<double>(drawnInAll_) / frames, static_cast<double>(closeInAll_) / frames,
            poseSeconds_ * 1000.0 / frames,
            drawSeconds_ * 1000.0 / frames, flatSeconds_ * 1000.0 / frames);
    }
}

void VisitorRenderer::Clear() {
    actors_.clear();
    tick_ = -1;
    drawn_ = 0;
    inView_.clear();
}

void VisitorRenderer::GrowCast() {
    int assembling = 0;
    for (const auto& member : cast_)
        assembling += !member->ready && !member->failed;
    while (cast_.size() < kCast && assembling < kAssembling) {
        // Men and women in turn; everything else about them is CNA's choice.
        const AvatarBodyType type = cast_.size() % 2 == 0 ? AvatarBodyType::Female : AvatarBodyType::Male;
        auto member = std::make_unique<Member>(Member{.description = AvatarDescription::CreateRandom(type),
                                                      .renderer = nullptr,
                                                      .body = {},
                                                      .height = 1.7f,
                                                      .female = type == AvatarBodyType::Female,
                                                      .ready = false,
                                                      .failed = false});
        member->renderer = std::make_unique<AvatarRenderer>(&member->description, false);
        cast_.push_back(std::move(member));
        ++assembling;
    }
}

void VisitorRenderer::PollCast() {
    ready_ = 0;
    int failed = 0;
    for (const auto& slot : cast_) {
        Member& member = *slot;
        if (member.failed) {
            ++failed;
            continue;
        }
        if (member.ready) {
            ++ready_;
            continue;
        }
        const AvatarRendererState state = member.renderer->getStateProperty();
        if (state == AvatarRendererState::Unavailable) {
            member.failed = true;
            ++failed;
            continue;
        }
        if (state != AvatarRendererState::Ready)
            continue;
        const auto parents = member.renderer->getParentBonesProperty();
        const auto bind = member.renderer->getBindPoseProperty();
        if (parents.getCountProperty() != kAvatarBones || bind.getCountProperty() != kAvatarBones) {
            member.failed = true;
            ++failed;
            continue;
        }
        std::array<int, kAvatarBones> parent{};
        std::array<Vector3, kAvatarBones> offset{};
        for (int b = 0; b < kAvatarBones; ++b) {
            parent[static_cast<std::size_t>(b)] = parents[b];
            offset[static_cast<std::size_t>(b)] = bind[b].getTranslationProperty();
        }
        member.body.Measure(parent, offset);
        const float described = member.description.getHeightProperty();
        member.height = described > 0.8f && described < 2.5f ? described : member.body.top;
        member.ready = true;
        ++ready_;
        if (std::getenv("ZOO_AVATAR_DEBUG") != nullptr) {
            const AvatarBody& body = member.body;
            std::printf(
                "zoo: avatar %zu %s height %.3f top %.3f root %.3f hip %.3f ankle %.3f thigh %.3f shin "
                "%.3f\n",
                static_cast<std::size_t>(&slot - cast_.data()), member.female ? "f" : "m",
                static_cast<double>(member.height), static_cast<double>(body.top),
                static_cast<double>(body.position[avatar_bone::Root].Y), static_cast<double>(body.hipY),
                static_cast<double>(body.ankleY), static_cast<double>(body.thigh),
                static_cast<double>(body.shin));
            if (ready_ == 1) {
                for (int b = 0; b < kAvatarBones; ++b) {
                    const Vector3& o = body.offset[static_cast<std::size_t>(b)];
                    const Vector3& p = body.position[static_cast<std::size_t>(b)];
                    std::printf("zoo: avatar bone %d parent %d offset %.3f %.3f %.3f at %.3f %.3f %.3f\n", b,
                                body.parent[static_cast<std::size_t>(b)], static_cast<double>(o.X),
                                static_cast<double>(o.Y), static_cast<double>(o.Z), static_cast<double>(p.X),
                                static_cast<double>(p.Y), static_cast<double>(p.Z));
                }
                const AvatarClip& stand = Clip(Preset::Stand0);
                const AvatarPose& first = stand.frames.front();
                std::printf(
                    "zoo: avatar Stand0 length %.2f root %.3f %.3f %.3f rootq %.3f %.3f %.3f %.3f\n",
                    static_cast<double>(stand.length), static_cast<double>(first.root.X),
                    static_cast<double>(first.root.Y), static_cast<double>(first.root.Z),
                    static_cast<double>(first.rotation[0].X), static_cast<double>(first.rotation[0].Y),
                    static_cast<double>(first.rotation[0].Z), static_cast<double>(first.rotation[0].W));
            }
        }
    }
    failed_ = failed;
    if (failed > 0 && ready_ == 0 && static_cast<std::size_t>(failed) == cast_.size() && !reported_) {
        reported_ = true;
        available_ = false;
        std::printf("zoo: visitors cannot be drawn: CNA provides no avatars here\n");
    }
    if (available_)
        GrowCast();
}

const VisitorRenderer::Member* VisitorRenderer::Playing(const Actor& actor) const {
    for (std::size_t i = 0; i < cast_.size(); ++i) {
        const Member& member = *cast_[(actor.member + i) % cast_.size()];
        if (member.ready)
            return &member;
    }
    return nullptr;
}

const AvatarClip& VisitorRenderer::Clip(Preset preset) {
    auto& slot = clips_[static_cast<std::size_t>(preset) % clips_.size()];
    if (!slot)
        slot = AvatarClip::Bake(preset);
    return *slot;
}

VisitorRenderer::Preset VisitorRenderer::Fidget(const Actor& actor, zoo::VisitorId id, bool female) {
    const std::uint32_t roll = zoo::HashU32(id.value, 0x51d0u + actor.fidgets);
    const auto pick = [&](std::initializer_list<Preset> of) { return *(of.begin() + roll % of.size()); };
    const Preset look = female ? Preset::FemaleIdleLookAround : Preset::MaleIdleLookAround;
    const Preset shift = female ? Preset::FemaleIdleShiftWeight : Preset::MaleIdleShiftWeight;
    const Preset yawn = female ? Preset::FemaleYawn : Preset::MaleYawn;
    const Preset laugh = female ? Preset::FemaleLaugh : Preset::MaleLaugh;
    const Preset hands = female ? Preset::FemaleIdleCheckNails : Preset::MaleIdleCheckHand;
    const Preset confused = female ? Preset::FemaleConfused : Preset::MaleConfused;
    switch (actor.activity) {
        case VisitorActivity::Watching:
            if (actor.thought == VisitorThought::LovedAnimal)
                return pick({Preset::Clap, Preset::Wave, laugh, Preset::Celebrate, look});
            if (actor.thought == VisitorThought::CouldNotSeeAnimals ||
                actor.thought == VisitorThought::AnimalsLookUnhappy)
                return pick({confused, look, shift});
            return pick({look, shift, laugh, Preset::Wave});
        case VisitorActivity::Queueing:
            return pick({shift, yawn, hands, look, shift});
        default:
            return pick({look, shift, hands, female ? Preset::FemaleIdleFixShoe : Preset::MaleIdleStretch});
    }
}

void VisitorRenderer::React(Actor& actor, zoo::VisitorId id, const Member* member, bool fresh) {
    // A thought the visitor has just had shows at once, if it stands to show it.
    const bool female = member != nullptr && member->female;
    if (fresh || actor.gait > 0.2f || actor.sit > 0.2f)
        return;
    const std::uint32_t roll =
        zoo::HashU32(id.value, 0x7e11u + static_cast<std::uint32_t>(actor.thoughtTick));
    std::optional<Preset> shot;
    switch (actor.thought) {
        case VisitorThought::LovedAnimal:
            shot = roll % 3 == 0   ? Preset::Clap
                   : roll % 3 == 1 ? (female ? Preset::FemaleLaugh : Preset::MaleLaugh)
                                   : Preset::Celebrate;
            break;
        case VisitorThought::GreatZoo:
            shot = Preset::Celebrate;
            break;
        case VisitorThought::Scared:
            shot = female ? Preset::FemaleShocked : Preset::MaleSurprised;
            break;
        case VisitorThought::TooExpensive:
        case VisitorThought::Litter:
        case VisitorThought::TooCrowded:
            shot = female ? Preset::FemaleAngry : Preset::MaleAngry;
            break;
        case VisitorThought::AnimalsLookUnhappy:
            shot = female ? Preset::FemaleCry : Preset::MaleCry;
            break;
        case VisitorThought::CouldNotSeeAnimals:
        case VisitorThought::NothingToSee:
        case VisitorThought::LostWay:
            shot = female ? Preset::FemaleConfused : Preset::MaleConfused;
            break;
        case VisitorThought::Tired:
            shot = female ? Preset::FemaleYawn : Preset::MaleYawn;
            break;
        default:
            break;
    }
    if (shot) {
        actor.shot = shot;
        actor.shotTime = 0.0f;
    }
}

void VisitorRenderer::Update(const zoo::Zoo& zoo, float seconds, float tickFraction) {
    PollCast();
    const zoo::World& world = zoo.Map();
    const zoo::GameTime now = zoo.Time();
    const bool stepped = now.tick != tick_;
    tick_ = now.tick;
    const float alpha = std::clamp(tickFraction, 0.0f, 1.0f);
    for (auto& [id, actor] : actors_)
        actor.seen = false;
    if (!censused_ && std::getenv("ZOO_VISITOR_DEBUG") != nullptr && zoo.Guests().Count() > 0) {
        // Who is where and does what, once: for finding a scene to look at.
        censused_ = true;
        for (const zoo::Visitor& visitor : zoo.Guests().All())
            std::printf("zoo: visitor %u group %u %s%s at %.2f,%.2f heading %.0f speed %.2f height %.2f\n",
                        visitor.id.value, visitor.group, std::string(zoo::ToString(visitor.activity)).c_str(),
                        visitor.child ? " child" : "", static_cast<double>(visitor.position.x),
                        static_cast<double>(visitor.position.z),
                        static_cast<double>(visitor.heading * 57.29578f), static_cast<double>(visitor.speed),
                        static_cast<double>(visitor.heightM));
    }

    const std::vector<zoo::Visitor>& all = zoo.Guests().All();
    const double clock = (static_cast<double>(now.tick) + static_cast<double>(alpha)) * zoo::kTickSeconds;
    for (std::size_t index = 0; index < all.size(); ++index) {
        const zoo::Visitor& visitor = all[index];
        const auto [found, fresh] = actors_.try_emplace(visitor.id);
        Actor& actor = found->second;
        actor.seen = true;
        const bool hidden = visitor.activity == VisitorActivity::Inside;
        const bool appeared = fresh || (actor.hidden && !hidden);
        actor.hidden = hidden;
        actor.heightM = visitor.heightM;
        actor.activity = visitor.activity;
        actor.seated = static_cast<bool>(visitor.seat) && (visitor.activity == VisitorActivity::Sitting ||
                                                           visitor.activity == VisitorActivity::Eating);
        const Member* member = Playing(actor);
        if (fresh) {
            actor.member = visitor.look % kCast;
            actor.base = static_cast<Preset>(static_cast<int>(Preset::Stand0) +
                                             static_cast<int>(visitor.look / 7 % 8));
            // People do not breathe, step or fidget in time with each other.
            actor.baseTime = 20.0f * zoo::HashFloat(visitor.id.value, 1);
            actor.phase = zoo::HashFloat(visitor.id.value, 2);
            actor.fidgetIn = 1.0f + 8.0f * zoo::HashFloat(visitor.id.value, 3);
            actor.eatTime = 4.0f * zoo::HashFloat(visitor.id.value, 4);
            // Nobody walks like the next one: longer or shorter steps for the same pace,
            // arms that swing or hardly move; the old take short steps and stoop.
            const bool old = zoo::Elderly(visitor) || (showOld_ && !visitor.child);
            actor.stride = (old ? 0.8f : 0.92f) + 0.14f * zoo::HashFloat(visitor.id.value, 5);
            actor.swing = (old ? 0.45f : 0.65f) + 0.7f * zoo::HashFloat(visitor.id.value, 6);
            actor.stoop = old ? 0.6f + 0.4f * zoo::HashFloat(visitor.id.value, 7) : 0.0f;
            actor.thought = visitor.thought;
            actor.thoughtTick = visitor.thoughtTick;
        }
        if (visitor.thought != actor.thought || visitor.thoughtTick != actor.thoughtTick) {
            actor.thought = visitor.thought;
            actor.thoughtTick = visitor.thoughtTick;
            React(actor, visitor.id, member, fresh);
        }
        if (hidden)
            continue;

        // Where it is: between the last two places the simulation gave it.
        const zoo::Vec2 before = actor.shown;
        if (appeared) {
            actor.from = actor.to = actor.shown = visitor.position;
            actor.heading = visitor.heading;
        } else if (stepped) {
            actor.from = actor.to;
            actor.to = visitor.position;
            const zoo::Vec2 jump = actor.to - actor.from;
            if (jump.x * jump.x + jump.z * jump.z > 16.0f) {
                actor.from = actor.to;
                actor.heading = visitor.heading;
            }
        }
        actor.shown = actor.from + (actor.to - actor.from) * alpha;
        actor.ground = world.HeightAt(actor.shown);
        const float turn = zoo::WrapAngle(visitor.heading - actor.heading);
        const float most = (actor.seated ? 9.0f : 5.0f) * seconds;
        actor.heading = zoo::WrapAngle(actor.heading + std::clamp(turn, -most, most));
        if (seconds <= 0.0f)
            continue;

        // What the body does: the weights follow the simulation, the steps the ground covered.
        const zoo::Vec2 step = appeared ? zoo::Vec2{} : actor.shown - before;
        const float covered = std::sqrt(step.x * step.x + step.z * step.z);
        const float pace = covered / seconds;
        const bool moving = OnTheMove(visitor.activity) && visitor.speed > 0.05f;
        actor.gait = Ease(actor.gait, moving ? 1.0f : 0.0f, 4.5f, seconds);
        actor.run = Ease(actor.run, std::clamp((pace - 2.1f) / 1.2f, 0.0f, 1.0f), 3.0f, seconds);
        actor.sit = Ease(actor.sit, actor.seated ? 1.0f : 0.0f, 2.8f, seconds);
        actor.eat = Ease(actor.eat, visitor.activity == VisitorActivity::Eating ? 1.0f : 0.0f, 3.0f, seconds);
        actor.read =
            Ease(actor.read, visitor.activity == VisitorActivity::Reading ? 1.0f : 0.0f, 2.5f, seconds);
        // Who goes slowly (an old person, somebody braking or squeezing past) takes shorter
        // steps, not the same ones in slow motion: the step goes with the root of the pace.
        const float scale = member != nullptr ? actor.heightM / member->height : 1.0f;
        const bool old = actor.stoop > 0.0f;
        const float own = (old ? 0.8f : 0.92f) + 0.14f * zoo::HashFloat(visitor.id.value, 5);
        if (moving) {
            const float natural = 1.45f * std::sqrt(std::max(scale, 0.3f));
            const float wanted = own * std::clamp(std::sqrt(pace / natural), 0.5f, 1.06f);
            actor.stride = Ease(actor.stride, wanted, 1.2f, seconds);
        }
        const float stride =
            (member != nullptr ? StrideLength(member->body, actor.run) : 1.2f) * scale * actor.stride;
        actor.phase += covered / std::max(stride, 0.2f);
        actor.phase -= std::floor(actor.phase);
        actor.baseTime += seconds;
        actor.eatTime += seconds;

        if (actor.shot) {
            actor.shotTime += seconds;
            if (actor.shotTime >= Clip(*actor.shot).length || actor.gait > 0.6f) {
                actor.shot.reset();
                actor.fidgetIn = 3.0f + 7.0f * zoo::HashFloat(visitor.id.value, 0x90u + actor.fidgets);
            }
        } else if (!moving && !actor.seated && actor.mannerWeight < 0.05f &&
                   visitor.activity != VisitorActivity::Reading && visitor.activity != VisitorActivity::Eating) {
            actor.fidgetIn -= seconds;
            if (actor.fidgetIn <= 0.0f) {
                actor.shot = Fidget(actor, visitor.id, member != nullptr && member->female);
                actor.shotTime = 0.0f;
                ++actor.fidgets;
            }
        }
        if (actor.close) {
            Carry(actor, all, index, moving, clock, seconds);
        } else {
            actor.weight = 0.0f;
            actor.lookYaw = 0.0f;
            actor.lookPitch = 0.0f;
            actor.speak = 0.0f;
            actor.mannerWeight = 0.0f;
            actor.manner = Actor::Manner::None;
        }
    }
    std::erase_if(actors_, [](const auto& entry) { return !entry.second.seen; });
}

void VisitorRenderer::Carry(Actor& actor, const std::vector<zoo::Visitor>& all, std::size_t index, bool moving,
                            double clock, float seconds) {
    using Manner = Actor::Manner;
    const zoo::Visitor& visitor = all[index];
    const std::uint32_t id = visitor.id.value;
    const VisitorActivity activity = visitor.activity;
    const bool standing = !moving && !actor.seated;
    const auto wave = [&](float rate, std::uint32_t salt) {
        return std::sin(static_cast<float>(std::fmod(clock * static_cast<double>(rate), 2.0 * kPi)) +
                        2.0f * kPi * zoo::HashFloat(id, salt));
    };
    /// A stretch of time of the person's own: which one it is in, and how far into it.
    const auto spell = [&](float length, std::uint32_t salt, float& into) {
        const double at = clock / static_cast<double>(length) + static_cast<double>(zoo::HashFloat(id, salt));
        const double whole = std::floor(at);
        into = static_cast<float>(at - whole) * length;
        return static_cast<std::uint32_t>(static_cast<std::int64_t>(whole) & 0xffffff);
    };

    // The weight on one leg, and now and then on the other; one in five stands square.
    float side = 0.0f;
    if (standing && activity != VisitorActivity::Reading) {
        float into = 0.0f;
        const std::uint32_t turn = spell(9.0f + 9.0f * zoo::HashFloat(id, 21), 22, into);
        const float roll = zoo::HashFloat(id, 0x600u + turn);
        side = roll < 0.4f ? -1.0f : roll < 0.8f ? 1.0f : 0.0f;
        side *= 0.55f + 0.45f * zoo::HashFloat(id, 23);
    }
    actor.weight = Ease(actor.weight, side, 1.1f, seconds);

    // The group: its members follow one another in the list.
    std::size_t first = index;
    while (first > 0 && all[first - 1].group == visitor.group)
        --first;
    std::size_t end = index + 1;
    while (end < all.size() && all[end].group == visitor.group)
        ++end;
    const auto size = static_cast<std::uint32_t>(end - first);
    const auto self = static_cast<std::uint32_t>(index - first);

    // Talking: for some seconds one of a group speaks to another and the rest look at
    // the speaker. Everybody works that out alike, so the heads turn to each other.
    float yaw = 0.0f;
    float pitch = 0.0f;
    bool speaking = false;
    bool listening = false;
    if (size > 1 && activity != VisitorActivity::Reading && activity != VisitorActivity::Fleeing) {
        constexpr double kEvery = 11.0;
        const double at = clock / kEvery + static_cast<double>(zoo::HashFloat(visitor.group, 41));
        const double whole = std::floor(at);
        const auto turn = static_cast<std::uint32_t>(static_cast<std::int64_t>(whole) & 0xffffff);
        const float into = static_cast<float>((at - whole) * kEvery);
        if (zoo::HashFloat(visitor.group, 0x900u + turn) < 0.55f && into < 7.0f) {
            const std::uint32_t speaker = zoo::HashU32(visitor.group, 0xa00u + turn) % size;
            const std::uint32_t heard =
                (speaker + 1u + zoo::HashU32(visitor.group, 0xb00u + turn) % (size - 1u)) % size;
            const zoo::Visitor& other = all[first + (self == speaker ? heard : speaker)];
            const zoo::Vec2 to = other.position - visitor.position;
            const float apart = std::sqrt(to.x * to.x + to.z * to.z);
            if (other.activity != VisitorActivity::Inside && apart > 0.2f && apart < 3.0f) {
                const float turnTo = zoo::WrapAngle(zoo::HeadingOf(to) - actor.heading);
                // Nobody talks over a shoulder for long; walking, one glances sideways.
                const float most = moving ? 0.7f : 1.2f;
                if (std::abs(turnTo) < (moving ? 1.9f : 2.3f)) {
                    listening = true;
                    yaw = std::clamp(turnTo, -most, most);
                    const float seatedDrop = actor.seated ? 0.4f : 0.0f;
                    const float theirs = other.heightM - (other.seat ? 0.4f : 0.0f);
                    pitch = std::clamp(std::atan2(0.9f * (theirs - (visitor.heightM - seatedDrop)), apart), -0.45f,
                                       0.4f);
                    speaking = self == speaker && standing && activity != VisitorActivity::Eating;
                }
            }
        }
    }
    if (!listening) {
        // Nobody keeps its head still: the animals are followed, a waiting person looks about.
        switch (activity) {
            case VisitorActivity::Watching:
                yaw = 0.2f * wave(0.31f, 24) + 0.1f * wave(0.83f, 25);
                pitch = -0.05f + 0.05f * wave(0.27f, 26);
                break;
            case VisitorActivity::Idle:
            case VisitorActivity::Queueing:
            case VisitorActivity::Sitting:
                yaw = 0.55f * wave(0.23f, 24) * wave(0.11f, 25);
                pitch = 0.04f * wave(0.19f, 26);
                break;
            case VisitorActivity::Reading:
            case VisitorActivity::Eating:
                break;
            default:
                yaw = 0.14f * wave(0.37f, 24);
                break;
        }
    }

    // What the hands are busy with, for a part of every spell of some twelve to twenty seconds.
    Manner wanted = Manner::None;
    if (!moving && actor.gait < 0.2f && !actor.shot && !speaking) {
        float into = 0.0f;
        const float length = 12.0f + 8.0f * zoo::HashFloat(id, 31);
        const std::uint32_t turn = spell(length, 32, into);
        const float roll = zoo::HashFloat(id, 0x700u + turn);
        float lasts = 0.0f;
        if (activity == VisitorActivity::Watching) {
            const bool rail = visitor.slot < 4 && !visitor.child && visitor.heightM > 1.5f;
            bool withChild = false;
            if (!visitor.child) {
                for (std::size_t i = first; i < end; ++i) {
                    const zoo::Vec2 to = all[i].position - visitor.position;
                    withChild = withChild || (all[i].child && to.x * to.x + to.z * to.z < 1.2f * 1.2f);
                }
            }
            if (visitor.child) {
                wanted = roll < 0.35f ? Manner::Point : Manner::None;
                lasts = 2.6f;
            } else if (rail && roll < 0.36f) {
                wanted = Manner::Lean;
                lasts = 0.8f * length;
            } else if (roll < 0.5f) {
                wanted = Manner::Point;
                lasts = 2.6f;
            } else if (roll < 0.68f) {
                wanted = Manner::Camera;
                lasts = 5.5f;
            } else if (withChild && roll < 0.84f) {
                wanted = Manner::Crouch;
                lasts = 0.6f * length;
            }
        } else if (activity == VisitorActivity::Idle || activity == VisitorActivity::Queueing ||
                   activity == VisitorActivity::Sitting) {
            if (!visitor.child && roll < 0.4f) {
                wanted = Manner::Phone;
                lasts = 0.7f * length;
            }
        }
        if (into > lasts)
            wanted = Manner::None;
        if (show_ == Manner::Phone ? !visitor.child
                                   : show_ != Manner::None && activity == VisitorActivity::Watching &&
                                         (show_ == Manner::Point || !visitor.child))
            wanted = show_;
        if (wanted == Manner::Point) {
            actor.pointYaw = 0.5f * (zoo::HashFloat(id, 0x800u + turn) - 0.5f);
            actor.pointPitch = (visitor.child ? 0.12f : -0.08f) + 0.14f * (zoo::HashFloat(id, 0x880u + turn) - 0.5f);
        }
    }
    if (actor.manner != wanted && actor.mannerWeight > 0.0f) {
        // Out of the old one first.
        actor.mannerWeight = Ease(actor.mannerWeight, 0.0f, moving ? 5.0f : 1.8f, seconds);
    } else {
        actor.manner = wanted;
        actor.mannerWeight =
            Ease(actor.mannerWeight, wanted == Manner::None ? 0.0f : 1.0f, wanted == Manner::Crouch ? 1.1f : 1.6f,
                 seconds);
    }
    // Who holds a telephone looks at it; who points or takes a picture looks where the hand is.
    if (actor.manner == Manner::Phone || actor.manner == Manner::Camera) {
        yaw *= 1.0f - actor.mannerWeight;
        pitch *= 1.0f - actor.mannerWeight;
    } else if (actor.manner == Manner::Point) {
        yaw += (actor.pointYaw - yaw) * actor.mannerWeight;
    }
    actor.lookYaw = Ease(actor.lookYaw, yaw, 1.9f, seconds);
    actor.lookPitch = Ease(actor.lookPitch, pitch, 1.3f, seconds);
    actor.speak = Ease(actor.speak, speaking && actor.mannerWeight < 0.05f ? 1.0f : 0.0f, 2.2f, seconds);
}

void VisitorRenderer::Pose(Actor& actor, const Member& member, float scale) {
    const AvatarBody& body = member.body;
    Clip(actor.base).Sample(actor.baseTime, true, pose_, expression_);
    if (actor.shot) {
        const AvatarClip& clip = Clip(*actor.shot);
        const float weight = SmoothStep(actor.shotTime / 0.3f) *
                             SmoothStep((clip.length - actor.shotTime) / 0.35f) * (1.0f - actor.gait);
        if (weight > 0.0f) {
            AvatarPose shot;
            AvatarExpression face;
            clip.Sample(actor.shotTime, false, shot, face);
            Blend(pose_, shot, weight);
            if (weight > 0.5f)
                expression_ = face;
        }
    }
    // Standing: on one leg rather than on both, and what the hands are busy with.
    const float still = 1.0f - SmoothStep(actor.sit);
    if (std::abs(actor.weight) > 0.001f && still > 0.0f) {
        AvatarPose eased = pose_;
        ApplyWeight(eased, body, actor.weight < 0.0f ? -1.0f : 1.0f);
        Blend(pose_, eased, SmoothStep(std::abs(actor.weight)) * still);
    }
    const float manner = SmoothStep(actor.mannerWeight);
    if (manner > 0.0f) {
        switch (actor.manner) {
            case Actor::Manner::Lean:
                ApplyLean(pose_, body, manner * still);
                break;
            case Actor::Manner::Point:
                ApplyPoint(pose_, body, actor.pointYaw, actor.pointPitch, manner);
                break;
            case Actor::Manner::Camera:
                ApplyCamera(pose_, body, manner);
                break;
            case Actor::Manner::Phone:
                ApplyPhone(pose_, body, manner);
                break;
            case Actor::Manner::Crouch: {
                AvatarPose down = pose_;
                ApplyCrouch(down, body);
                Blend(pose_, down, manner * still);
                break;
            }
            default:
                break;
        }
    }
    ApplyTalk(pose_, body, actor.baseTime, SmoothStep(actor.speak));
    if (actor.gait > 0.001f) {
        AvatarPose step = pose_;
        ApplyGait(step, body, actor.phase, actor.run, actor.stride, actor.swing);
        Blend(pose_, step, SmoothStep(actor.gait));
    }
    if (actor.sit > 0.001f) {
        AvatarPose seated = pose_;
        ApplySit(seated, body, (kSeatHeight + kHipAboveSeat) / scale);
        Blend(pose_, seated, SmoothStep(actor.sit));
    }
    ApplyEating(pose_, body, actor.eatTime, actor.eat);
    ApplyHeadDown(pose_, body, 0.32f * actor.read);
    ApplyStoop(pose_, body, actor.stoop);
    ApplyLook(pose_, body, actor.lookYaw, actor.lookPitch);
    ToMatrices(pose_, bones_);
}

void VisitorRenderer::Prepare(GraphicsDevice& device) {
    if (!flat_)
        return;
    for (const auto& slot : cast_) {
        if (slot->ready && slot->sheet == nullptr) {
            Capture(device, *slot);
            return;  // one a frame: a sheet is eighty avatars drawn
        }
    }
}

void VisitorRenderer::Capture(GraphicsDevice& device, Member& member) {
    namespace Xna = Microsoft::Xna::Framework::Graphics;
    member.sheet = std::make_unique<Xna::RenderTarget2D>(device, kSheetPoses * kCellWide, kSheetSides * kCellTall,
                                                         true, Xna::SurfaceFormat::Color,
                                                         Xna::DepthFormat::Depth24);
    // A picture shows the member with some room about them: a step and a swung arm reach out.
    member.cellTall = member.height * 1.1f;
    member.cellWide = member.cellTall * static_cast<float>(kCellWide) / static_cast<float>(kCellTall);
    const Xna::Viewport before = device.getViewportProperty();
    device.SetRenderTarget(member.sheet.get());
    // Nothing where nobody is; the colour is what the edge of a body fades into when the
    // picture is drawn small, so it is a middling cloth colour and not black.
    device.Clear(Xna::ClearOptions::Target | Xna::ClearOptions::DepthBuffer, Vector4(0.36f, 0.33f, 0.31f, 0.0f),
                 1.0f, 0);
    AvatarRenderer& renderer = *member.renderer;
    renderer.setWorldProperty(Matrix::getIdentityProperty());
    renderer.setProjectionProperty(Matrix::CreateOrthographic(member.cellWide, member.cellTall, 1.0f, 20.0f));
    // Lit as on a dull day, a little from above and the side: the form shows, and the
    // picture is tinted with the light of the hour when it is drawn.
    renderer.setLightColorProperty(Vector3(kSheetLight, kSheetLight, kSheetLight));
    renderer.setAmbientLightColorProperty(Vector3(kSheetAmbient, kSheetAmbient, kSheetAmbient));
    const Vector3 middle(0.0f, 0.5f * member.cellTall, 0.0f);
    for (int pose = 0; pose < kSheetPoses; ++pose) {
        Actor actor;
        actor.base = Preset::Stand0;
        actor.baseTime = 0.4f;
        if (pose == kSheetSit) {
            actor.sit = 1.0f;
        } else if (pose >= kSheetWalk) {
            actor.gait = 1.0f;
            actor.phase = static_cast<float>(pose - kSheetWalk) / static_cast<float>(kSheetSteps);
        }
        Pose(actor, member, 1.0f);
        for (int side = 0; side < kSheetSides; ++side) {
            // An avatar looks along -Z: side 0 is seen from the front, and the eye goes round.
            const float angle = 2.0f * kPi * static_cast<float>(side) / static_cast<float>(kSheetSides);
            const Vector3 from(std::sin(angle), 0.0f, -std::cos(angle));
            const Vector3 eye = middle + from * 10.0f;
            const Vector3 right = Vector3::Cross(from * -1.0f, Vector3(0.0f, 1.0f, 0.0f));
            Vector3 light = from * -1.0f + Vector3(0.0f, -0.7f, 0.0f) + right * 0.45f;
            light.Normalize();
            device.setViewportProperty(Xna::Viewport(pose * kCellWide, side * kCellTall, kCellWide, kCellTall));
            renderer.setViewProperty(Matrix::CreateLookAt(eye, middle, Vector3(0.0f, 1.0f, 0.0f)));
            renderer.setLightDirectionProperty(light);
            renderer.Draw(bones_, expression_);
        }
    }
    device.SetRenderTarget(nullptr);
    device.setViewportProperty(before);
}

void VisitorRenderer::Draw(GraphicsDevice& device, const CnaEngine::Graphics::Camera& camera,
                           const CnaEngine::Graphics::ForwardLighting& lighting) {
    drawn_ = 0;
    drawnFlat_ = 0;
    inRange_ = 0;
    inViewCount_ = 0;
    if (ready_ == 0)
        return;
    const auto& frustum = camera.Frustum();
    const Vector3 eye = camera.Position();
    // The scene's sun, and its sky as light from everywhere (the avatars' own
    // effect knows one directional light and an ambient colour).
    const Vector3 sun = lighting.lightColor * 0.9f;
    Vector3 ambient = lighting.ambientColor;
    if (!lighting.fillLights.empty())
        ambient = ambient + lighting.fillLights.front().diffuse * 0.55f;

    // Who is in view, and how far away: when that is more people than a frame may
    // draw (from the ground one looks along a whole avenue of them), the nearest are drawn.
    // People make room for one another, and so they do for the observer: who would walk
    // straight through the eyes is drawn stepping round them at arm's length.
    constexpr float kRoom = 0.95f;
    const auto standing = [&](const Actor& actor) {
        Vector3 feet(actor.shown.x, actor.ground, actor.shown.z);
        const float dx = feet.X - eye.X;
        const float dz = feet.Z - eye.Z;
        const float apart = std::sqrt(dx * dx + dz * dz);
        if (apart < kRoom && eye.Y - feet.Y < 2.5f && eye.Y - feet.Y > 0.5f) {
            const float push = (kRoom - apart) / std::max(apart, 0.05f);
            feet.X += (apart > 0.01f ? dx : 0.05f) * push;
            feet.Z += dz * push;
        }
        return feet;
    };
    inView_.clear();
    far_.clear();
    shades_.clear();
    for (auto& [id, actor] : actors_) {
        const bool wasClose = actor.close;
        actor.wasAvatar = wasClose;
        actor.close = false;
        const Member* member = actor.hidden ? nullptr : Playing(actor);
        if (member == nullptr)
            continue;
        const Vector3 feet = standing(actor);
        const Vector3 away = feet - eye;
        const float distance = away.X * away.X + away.Y * away.Y + away.Z * away.Z;
        if (distance > kFarthest * kFarthest)
            continue;
        ++inRange_;
        if (frustum.Contains(BoundingSphere(feet + Vector3(0.0f, 0.9f, 0.0f), 1.4f)) ==
            ContainmentType::Disjoint)
            continue;
        ++inViewCount_;
        // Flat beyond the distance at which a picture is as good as the avatar on the screen;
        // who was an avatar on the last frame stays one a few metres longer.
        const float limit = wasClose ? kCloseFar : kCloseNear;
        if (flat_ && distance > limit * limit)
            far_.push_back(Far{member, &actor, feet});
        else
            inView_.emplace_back(distance, &actor);
    }
    // An avatar is some forty draws of its own (0.45 ms on the machine this was measured on,
    // docs/PERFORMANCE.md): a crowd at a fence is drawn as avatars only in its nearest rows,
    // the rows behind as pictures. Who was an avatar on the last frame counts as a little
    // nearer, so that two people at the same distance do not swap every frame.
    const std::size_t avatars = flat_ ? std::min(budget_, kAvatars) : budget_;
    if (inView_.size() > avatars) {
        const auto rank = [](const std::pair<float, Actor*>& one) {
            return one.second->wasAvatar ? one.first * 0.8f : one.first;
        };
        std::nth_element(inView_.begin(), inView_.begin() + static_cast<std::ptrdiff_t>(avatars), inView_.end(),
                         [&](const auto& a, const auto& b) { return rank(a) < rank(b); });
        if (flat_) {
            for (std::size_t i = avatars; i < inView_.size(); ++i) {
                const Actor& actor = *inView_[i].second;
                far_.push_back(Far{Playing(actor), &actor, standing(actor)});
            }
        }
        inView_.resize(avatars);
    }
    // Whose pictures are not drawn yet is not seen for the frames that takes.
    std::erase_if(far_, [](const Far& one) { return one.member->sheet == nullptr; });
    for (const Far& one : far_)
        shades_.push_back(one.feet);
    for (const auto& [distance, seen] : inView_)
        shades_.push_back(standing(*seen));

    DrawShade(device, camera, lighting);

    for (const auto& [distance, seen] : inView_) {
        Actor& actor = *seen;
        actor.close = true;
        const Member* member = Playing(actor);
        const Vector3 feet = standing(actor);
        const float scale = actor.heightM / member->height;
        const auto before = std::chrono::steady_clock::now();
        Pose(actor, *member, scale);
        const auto posed = std::chrono::steady_clock::now();
        AvatarRenderer& renderer = *member->renderer;
        // An avatar looks along -Z; a heading of 0 looks along +Z.
        renderer.setWorldProperty(Matrix::CreateScale(scale) * Matrix::CreateRotationY(actor.heading + kPi) *
                                  Matrix::CreateTranslation(feet));
        renderer.setViewProperty(camera.View());
        renderer.setProjectionProperty(camera.Projection());
        renderer.setLightDirectionProperty(lighting.lightDirection);
        renderer.setLightColorProperty(sun);
        renderer.setAmbientLightColorProperty(ambient);
        renderer.Draw(bones_, expression_);
        ++drawn_;
        poseSeconds_ += std::chrono::duration<double>(posed - before).count();
        drawSeconds_ += std::chrono::duration<double>(std::chrono::steady_clock::now() - posed).count();
    }

    const auto before = std::chrono::steady_clock::now();
    DrawFlat(device, camera, lighting);
    flatSeconds_ += std::chrono::duration<double>(std::chrono::steady_clock::now() - before).count();
    closeInAll_ += drawn_;
    drawnFlat_ = far_.size();
    drawn_ += drawnFlat_;
    ++frames_;
    drawnInAll_ += drawn_;
}

namespace {

/// Restores what the engine's renderer had set when a pass of the game's own is over.
struct DeviceState {
    explicit DeviceState(GraphicsDevice& of)
        : device(of), blend(of.getBlendStateProperty()), depth(of.getDepthStencilStateProperty()),
          rasterizer(of.getRasterizerStateProperty()), sampler(of.getSamplerStatesProperty()[0]) {}
    ~DeviceState() {
        device.setBlendStateProperty(blend);
        device.setDepthStencilStateProperty(depth);
        device.setRasterizerStateProperty(rasterizer);
        device.getSamplerStatesProperty()[0] = sampler;
    }
    GraphicsDevice& device;
    Microsoft::Xna::Framework::Graphics::BlendState blend;
    Microsoft::Xna::Framework::Graphics::DepthStencilState depth;
    Microsoft::Xna::Framework::Graphics::RasterizerState rasterizer;
    Microsoft::Xna::Framework::Graphics::SamplerState sampler;
};

}  // namespace

void VisitorRenderer::DrawShade(GraphicsDevice& device, const CnaEngine::Graphics::Camera& camera,
                                const CnaEngine::Graphics::ForwardLighting& lighting) {
    namespace Xna = Microsoft::Xna::Framework::Graphics;
    using Microsoft::Xna::Framework::Color;
    if (shades_.empty())
        return;
    if (shadeEffect_ == nullptr) {
        // A round patch, dark in the middle and gone at the rim (premultiplied: black with alpha).
        constexpr int kSize = 32;
        std::vector<Color> pixels(static_cast<std::size_t>(kSize * kSize));
        for (int y = 0; y < kSize; ++y) {
            for (int x = 0; x < kSize; ++x) {
                const float u = (static_cast<float>(x) + 0.5f) / kSize * 2.0f - 1.0f;
                const float v = (static_cast<float>(y) + 0.5f) / kSize * 2.0f - 1.0f;
                const float alpha = SmoothStep(1.0f - std::sqrt(u * u + v * v));
                pixels[static_cast<std::size_t>(y * kSize + x)] = Color(0.0f, 0.0f, 0.0f, alpha);
            }
        }
        shadeTexture_ = std::make_unique<Xna::Texture2D>(device, kSize, kSize);
        shadeTexture_->SetData(pixels.data(), static_cast<int>(pixels.size()));
        shadeEffect_ = std::make_unique<Xna::BasicEffect>(device);
        shadeEffect_->setLightingEnabledProperty(false);
        shadeEffect_->setTextureEnabledProperty(true);
        shadeEffect_->setVertexColorEnabledProperty(true);
        shadeEffect_->setTextureProperty(shadeTexture_.get());
    }
    // Under a high sun the patch is short and dark, towards evening longer and fainter; it
    // lies where the light throws it. Under cloud or at dusk only a faint patch is left.
    const Vector3 ray = lighting.lightDirection;
    const float flatLength = std::sqrt(ray.X * ray.X + ray.Z * ray.Z);
    const float bright = std::clamp((lighting.lightColor.X + lighting.lightColor.Y + lighting.lightColor.Z) / 3.0f,
                                    0.0f, 1.0f);
    const float low = ray.Y < -0.05f ? std::clamp(flatLength / -ray.Y, 0.0f, 2.5f) : 2.5f;
    const Vector3 along = flatLength > 0.001f ? Vector3(ray.X / flatLength, 0.0f, ray.Z / flatLength)
                                               : Vector3(1.0f, 0.0f, 0.0f);
    const Vector3 across(-along.Z, 0.0f, along.X);
    const float reach = 0.34f + 0.42f * low;
    const float strength = (0.22f + 0.33f * bright) / (1.0f + 0.25f * low);
    const Color tint(strength, strength, strength, strength);
    const Vector3 eye = camera.Position();
    quads_.clear();
    for (const Vector3& feet : shades_) {
        const float dx = feet.X - eye.X;
        const float dz = feet.Z - eye.Z;
        if (dx * dx + dz * dz > kShadeFarthest * kShadeFarthest)
            continue;
        const Vector3 middle = feet + Vector3(0.0f, 0.035f, 0.0f) + along * (reach - 0.3f);
        const Vector3 a = middle - along * reach - across * 0.34f;
        const Vector3 b = middle + along * reach - across * 0.34f;
        const Vector3 c = middle + along * reach + across * 0.34f;
        const Vector3 d = middle - along * reach + across * 0.34f;
        quads_.emplace_back(a, tint, Vector2(0.0f, 0.0f));
        quads_.emplace_back(b, tint, Vector2(1.0f, 0.0f));
        quads_.emplace_back(c, tint, Vector2(1.0f, 1.0f));
        quads_.emplace_back(a, tint, Vector2(0.0f, 0.0f));
        quads_.emplace_back(c, tint, Vector2(1.0f, 1.0f));
        quads_.emplace_back(d, tint, Vector2(0.0f, 1.0f));
    }
    if (quads_.empty())
        return;
    const DeviceState restore(device);
    device.setBlendStateProperty(Xna::BlendState::AlphaBlend);
    device.setDepthStencilStateProperty(Xna::DepthStencilState::DepthRead);
    device.setRasterizerStateProperty(Xna::RasterizerState::CullNone);
    device.getSamplerStatesProperty()[0] = Xna::SamplerState::LinearClamp;
    shadeEffect_->setWorldProperty(Matrix::getIdentityProperty());
    shadeEffect_->setViewProperty(camera.View());
    shadeEffect_->setProjectionProperty(camera.Projection());
    shadeEffect_->Apply();
    device.DrawUserPrimitives(Xna::PrimitiveType::TriangleList, quads_.data(), 0,
                              static_cast<int>(quads_.size() / 3));
}

void VisitorRenderer::DrawFlat(GraphicsDevice& device, const CnaEngine::Graphics::Camera& camera,
                               const CnaEngine::Graphics::ForwardLighting& lighting) {
    namespace Xna = Microsoft::Xna::Framework::Graphics;
    using Microsoft::Xna::Framework::Color;
    if (far_.empty())
        return;
    if (flatEffect_ == nullptr) {
        flatEffect_ = std::make_unique<Xna::AlphaTestEffect>(device);
        flatEffect_->setVertexColorEnabledProperty(true);
        flatEffect_->setAlphaFunctionProperty(Xna::CompareFunction::Greater);
        flatEffect_->setReferenceAlphaProperty(96);
    }
    // One sheet after another: each is one draw call.
    std::sort(far_.begin(), far_.end(), [](const Far& a, const Far& b) { return a.member < b.member; });
    const Vector3 eye = camera.Position();
    const Vector3 sun = lighting.lightColor * 0.9f;
    Vector3 ambient = lighting.ambientColor;
    if (!lighting.fillLights.empty())
        ambient = ambient + lighting.fillLights.front().diffuse * 0.55f;
    const DeviceState restore(device);
    device.setBlendStateProperty(Xna::BlendState::Opaque);
    device.setDepthStencilStateProperty(Xna::DepthStencilState::Default);
    device.setRasterizerStateProperty(Xna::RasterizerState::CullNone);
    device.getSamplerStatesProperty()[0] = Xna::SamplerState::LinearClamp;
    flatEffect_->setWorldProperty(Matrix::getIdentityProperty());
    flatEffect_->setViewProperty(camera.View());
    flatEffect_->setProjectionProperty(camera.Projection());

    std::size_t at = 0;
    while (at < far_.size()) {
        const Member& member = *far_[at].member;
        quads_.clear();
        for (; at < far_.size() && far_[at].member == &member; ++at) {
            const Actor& actor = *far_[at].actor;
            const Vector3& feet = far_[at].feet;
            // The rectangle stands upright and turns about its foot to face the eye.
            Vector3 toEye(eye.X - feet.X, 0.0f, eye.Z - feet.Z);
            const float apart = std::sqrt(toEye.X * toEye.X + toEye.Z * toEye.Z);
            if (apart < 0.01f)
                continue;
            toEye = toEye * (1.0f / apart);
            const Vector3 right = Vector3::Cross(toEye * -1.0f, Vector3(0.0f, 1.0f, 0.0f));
            // Which picture: the side the eye is on, as the avatar itself would see it
            // (drawn, it is turned by heading + pi about the vertical).
            const float turn = -(actor.heading + kPi);
            const float localX = toEye.X * std::cos(turn) + toEye.Z * std::sin(turn);
            const float localZ = -toEye.X * std::sin(turn) + toEye.Z * std::cos(turn);
            const float angle = std::atan2(localX, -localZ);
            int side = static_cast<int>(std::lround(angle / (2.0f * kPi) * kSheetSides));
            side = ((side % kSheetSides) + kSheetSides) % kSheetSides;
            int pose = kSheetStand;
            if (actor.sit > 0.5f)
                pose = kSheetSit;
            else if (actor.gait > 0.5f)
                pose = kSheetWalk + static_cast<int>(actor.phase * kSheetSteps) % kSheetSteps;

            const float scale = actor.heightM / member.height;
            const float halfWide = 0.5f * member.cellWide * scale;
            const float tall = member.cellTall * scale;
            // The sun on the side one sees, or on the far one.
            const float facing = -(toEye.X * lighting.lightDirection.X + toEye.Z * lighting.lightDirection.Z);
            const float lit = 0.38f + 0.30f * std::clamp(facing, -1.0f, 1.0f);
            const Vector3 light = (ambient + sun * lit) * (1.0f / (kSheetAmbient + 0.45f * kSheetLight));
            const Color tint(std::min(light.X, 1.0f), std::min(light.Y, 1.0f), std::min(light.Z, 1.0f), 1.0f);

            // Half a texel in from the picture's edge: its neighbours do not show.
            const float u0 = (static_cast<float>(pose * kCellWide) + 0.5f) / (kSheetPoses * kCellWide);
            const float u1 = (static_cast<float>((pose + 1) * kCellWide) - 0.5f) / (kSheetPoses * kCellWide);
            const float v0 = (static_cast<float>(side * kCellTall) + 0.5f) / (kSheetSides * kCellTall);
            const float v1 = (static_cast<float>((side + 1) * kCellTall) - 0.5f) / (kSheetSides * kCellTall);
            const Vector3 up(0.0f, tall, 0.0f);
            const Vector3 a = feet - right * halfWide + up;
            const Vector3 b = feet + right * halfWide + up;
            const Vector3 c = feet + right * halfWide;
            const Vector3 d = feet - right * halfWide;
            quads_.emplace_back(a, tint, Vector2(u0, v0));
            quads_.emplace_back(b, tint, Vector2(u1, v0));
            quads_.emplace_back(c, tint, Vector2(u1, v1));
            quads_.emplace_back(a, tint, Vector2(u0, v0));
            quads_.emplace_back(c, tint, Vector2(u1, v1));
            quads_.emplace_back(d, tint, Vector2(u0, v1));
        }
        if (quads_.empty())
            continue;
        flatEffect_->setTextureProperty(member.sheet.get());
        flatEffect_->Apply();
        device.DrawUserPrimitives(Xna::PrimitiveType::TriangleList, quads_.data(), 0,
                                  static_cast<int>(quads_.size() / 3));
    }
}

std::optional<Vector3> VisitorRenderer::PositionOf(zoo::VisitorId id) const {
    const auto found = actors_.find(id);
    if (found == actors_.end() || found->second.hidden)
        return std::nullopt;
    const Actor& actor = found->second;
    return Vector3(actor.shown.x, actor.ground, actor.shown.z);
}

float VisitorRenderer::HeightOf(zoo::VisitorId id) const {
    const auto found = actors_.find(id);
    return found == actors_.end() ? 1.7f : found->second.heightM;
}

}  // namespace zoogame
