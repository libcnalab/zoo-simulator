// SPDX-License-Identifier: MIT
#include "VisitorRenderer.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "Microsoft/Xna/Framework/BoundingFrustum.hpp"
#include "Microsoft/Xna/Framework/BoundingSphere.hpp"
#include "Microsoft/Xna/Framework/GamerServices/AvatarBodyType.hpp"
#include "Microsoft/Xna/Framework/GamerServices/AvatarRendererState.hpp"

#include "zoo/base/Rng.hpp"

namespace zoogame {

namespace {

using Microsoft::Xna::Framework::BoundingSphere;
using Microsoft::Xna::Framework::ContainmentType;
using Microsoft::Xna::Framework::GamerServices::AvatarBodyType;
using Microsoft::Xna::Framework::GamerServices::AvatarDescription;
using Microsoft::Xna::Framework::GamerServices::AvatarRenderer;
using Microsoft::Xna::Framework::GamerServices::AvatarRendererState;
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
    cast_.reserve(kCast);
    GrowCast();
}

VisitorRenderer::~VisitorRenderer() {
    if (std::getenv("ZOO_AVATAR_DEBUG") != nullptr && frames_ > 0) {
        const double frames = static_cast<double>(frames_);
        std::printf(
            "zoo: visitors: %d of the cast ready; a frame drew %.1f people, posing %.3f ms, drawing %.3f "
            "ms\n",
            ready_, static_cast<double>(drawnInAll_) / frames, poseSeconds_ * 1000.0 / frames,
            drawSeconds_ * 1000.0 / frames);
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

    for (const zoo::Visitor& visitor : zoo.Guests().All()) {
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
        const float scale = member != nullptr ? actor.heightM / member->height : 1.0f;
        const float stride = (member != nullptr ? StrideLength(member->body, actor.run) : 1.2f) * scale;
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
        } else if (!moving && !actor.seated && visitor.activity != VisitorActivity::Reading &&
                   visitor.activity != VisitorActivity::Eating) {
            actor.fidgetIn -= seconds;
            if (actor.fidgetIn <= 0.0f) {
                actor.shot = Fidget(actor, visitor.id, member != nullptr && member->female);
                actor.shotTime = 0.0f;
                ++actor.fidgets;
            }
        }
    }
    std::erase_if(actors_, [](const auto& entry) { return !entry.second.seen; });
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
    if (actor.gait > 0.001f) {
        AvatarPose step = pose_;
        ApplyGait(step, body, actor.phase, actor.run);
        Blend(pose_, step, SmoothStep(actor.gait));
    }
    if (actor.sit > 0.001f) {
        AvatarPose seated = pose_;
        ApplySit(seated, body, (kSeatHeight + kHipAboveSeat) / scale);
        Blend(pose_, seated, SmoothStep(actor.sit));
    }
    ApplyEating(pose_, body, actor.eatTime, actor.eat);
    ApplyHeadDown(pose_, body, 0.32f * actor.read);
    ToMatrices(pose_, bones_);
}

void VisitorRenderer::Draw(const CnaEngine::Graphics::Camera& camera,
                           const CnaEngine::Graphics::ForwardLighting& lighting) {
    drawn_ = 0;
    inRange_ = 0;
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
    for (auto& [id, actor] : actors_) {
        if (actor.hidden || Playing(actor) == nullptr)
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
        inView_.emplace_back(distance, &actor);
    }
    inViewCount_ = inView_.size();
    if (inView_.size() > budget_) {
        std::nth_element(inView_.begin(), inView_.begin() + static_cast<std::ptrdiff_t>(budget_), inView_.end(),
                         [](const auto& a, const auto& b) { return a.first < b.first; });
        inView_.resize(budget_);
    }

    for (const auto& [distance, seen] : inView_) {
        Actor& actor = *seen;
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
    ++frames_;
    drawnInAll_ += drawn_;
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
