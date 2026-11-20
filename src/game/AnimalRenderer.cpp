// SPDX-License-Identifier: MIT
#include "AnimalRenderer.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <fstream>

#include <nlohmann/json.hpp>

#include "CnaEngine/App/AssetDirectory.hpp"
#include "CnaEngine/Enhanced/ModelImport.hpp"

#include "Microsoft/Xna/Framework/BoundingFrustum.hpp"
#include "Microsoft/Xna/Framework/BoundingSphere.hpp"

#include "AnimalTextures.hpp"

#include "GroundPaint.hpp"

namespace zoogame {

namespace {

using namespace CnaEngine;
namespace Xna = Microsoft::Xna::Framework::Graphics;

constexpr float kPi = 3.14159265f;

constexpr std::array<const char*, static_cast<std::size_t>(AnimalPose::Count)> kClipNames = {
    "idle", "walk", "trot", "eat", "drink", "rest", "sleep", "vocalize", "swim", "float",
    "sentry", "dig", "preen", "dust_bathe", "ruminate", "filter_feed", "bathe"};

/// Up to this share of its growing time an animal is drawn with the model of the young.
constexpr float kJuvenileShare = 0.5f;
/// An animal floats where the water is deeper than this share of its standing height...
constexpr float kFloatDepthShare = 0.62f;
/// ...and then, without a figure from its model, this share of its height is under water.
constexpr float kFloatSunkShare = 0.72f;

/// The model that stands in for a species whose own does not exist yet: the
/// nearest body plan there is, sized to the species' shoulder height.
const char* StandInFor(const zoo::SpeciesDef& def) {
    if (def.group == "big_cat")
        return "lion";
    if (def.group == "bear")
        return "brown_bear";
    if (def.group == "ungulate" || def.group == "pachyderm")
        return "zebra";
    return "wolf";
}

float WrapAngle(float a) {
    while (a > kPi)
        a -= 2.0f * kPi;
    while (a < -kPi)
        a += 2.0f * kPi;
    return a;
}

bool LiesDown(AnimalPose pose) {
    return pose == AnimalPose::Rest || pose == AnimalPose::Sleep;
}

/// A posture the body takes slowly: lying down, wallowing, sitting down to dust-bathe, or
/// rising on the hind feet.
bool SettlesSlowly(AnimalPose pose) {
    return LiesDown(pose) || pose == AnimalPose::DustBathe || pose == AnimalPose::Bathe ||
           pose == AnimalPose::Sentry;
}

using ShownPoses = std::array<AnimalPose, static_cast<std::size_t>(zoo::AnimalActivity::Count)>;
using PoseClips = std::array<const Animation::AnimationClip*, static_cast<std::size_t>(AnimalPose::Count)>;

/// What a still animal plays for each activity when its model says nothing else. Activities
/// without a clip of their own are shown by the nearest posture the standard clips have.
ShownPoses DefaultShownPoses() {
    using zoo::AnimalActivity;
    ShownPoses shown;
    shown.fill(AnimalPose::Idle);
    const auto set = [&shown](AnimalActivity activity, AnimalPose pose) {
        shown[static_cast<std::size_t>(activity)] = pose;
    };
    set(AnimalActivity::Forage, AnimalPose::Eat);
    set(AnimalActivity::Eat, AnimalPose::Eat);
    set(AnimalActivity::Drink, AnimalPose::Drink);
    set(AnimalActivity::Rest, AnimalPose::Rest);
    set(AnimalActivity::Sleep, AnimalPose::Sleep);
    // Nose or bill down: digging for grubs, sieving the shallows.
    set(AnimalActivity::Dig, AnimalPose::Eat);
    set(AnimalActivity::FilterFeed, AnimalPose::Eat);
    // A wallow too shallow to float in: the animal lies in it.
    set(AnimalActivity::Bathe, AnimalPose::Rest);
    return shown;
}

/// `meta.activity_clips` of a model's sidecar: {"huddle": "rest", "preen": "preen", ...}, activity
/// names as the save format writes them, clip names as the models carry them. Unknown names, and
/// clips the model does not have, are left out.
void ReadActivityClips(const nlohmann::json& clips, const PoseClips& present, ShownPoses& shown) {
    for (std::size_t activity = 0; activity < shown.size(); ++activity) {
        const std::string name{zoo::ToString(static_cast<zoo::AnimalActivity>(activity))};
        const auto found = clips.find(name);
        if (found == clips.end() || !found->is_string())
            continue;
        const std::string clip = found->get<std::string>();
        // Only the still clips: a gait or a water clip would be played in place.
        for (const AnimalPose pose :
             {AnimalPose::Idle, AnimalPose::Eat, AnimalPose::Drink, AnimalPose::Rest, AnimalPose::Sleep,
              AnimalPose::Sentry, AnimalPose::Dig, AnimalPose::Preen, AnimalPose::DustBathe,
              AnimalPose::Ruminate, AnimalPose::FilterFeed, AnimalPose::Bathe}) {
            const auto index = static_cast<std::size_t>(pose);
            if (clip == kClipNames[index] && present[index] != nullptr)
                shown[activity] = pose;
        }
    }
}

}  // namespace

AnimalRenderer::AnimalRenderer(Microsoft::Xna::Framework::Content::ContentManager& content,
                               const zoo::SpeciesCatalog& species)
    : content_(content), species_(species), directory_(App::FindAssetDirectory("assets")) {
    looks_.resize(species.All().size());
}

AnimalRenderer::~AnimalRenderer() = default;

void AnimalRenderer::Clear() {
    actors_.clear();
    tick_ = -1;
    drawn_ = 0;
}

const AnimalRenderer::Kind* AnimalRenderer::Load(const std::string& model) {
    if (const auto found = kinds_.find(model); found != kinds_.end())
        return found->second.get();
    // A failure is remembered too: the file is not looked for again every frame.
    auto& slot = kinds_[model];
    if (!directory_ || !std::filesystem::exists(*directory_ / "animals" / (model + ".glb")))
        return nullptr;
    auto kind = std::make_unique<Kind>();
    kind->shown = DefaultShownPoses();
    const auto started = std::chrono::steady_clock::now();
#if defined(__linux__) && !defined(__ANDROID__)
    // Imported animal textures need their level-zero pixels once below to make role-aware
    // mipmaps. Temporarily retain those pixels while the model is imported; mipmapped
    // replacements are uploaded after recovery is disabled again and do not keep that copy.
    content_.getGraphicsDeviceInternal().SetContextRecoveryEnabled(true);
#endif
    try {
        kind->model = std::make_unique<Xna::Model>(content_.Load<Xna::Model>("animals/" + model));
    } catch (const std::exception& error) {
#if defined(__linux__) && !defined(__ANDROID__)
        content_.getGraphicsDeviceInternal().SetContextRecoveryEnabled(false);
#endif
        std::printf("zoo: animals: %s: %s\n", model.c_str(), error.what());
        return nullptr;
    }
    kind->view = std::make_unique<Graphics::ModelView>(*kind->model, Enhanced::PbrMaterialFromEffect);
    const auto loaded = std::chrono::steady_clock::now();
#if defined(__linux__) && !defined(__ANDROID__)
    content_.getGraphicsDeviceInternal().SetContextRecoveryEnabled(false);
#endif
    kind->textures = GiveMipmaps(*kind->view);
    if (std::getenv("ZOO_LOAD_TIMING") != nullptr) {
        std::printf("zoo: model: %s load/view %.3f s, mipmaps %.3f s\n", model.c_str(),
                    std::chrono::duration<double>(loaded - started).count(),
                    std::chrono::duration<double>(std::chrono::steady_clock::now() - loaded).count());
    }
    const auto* skin = Enhanced::FindSkinningData(*kind->model);
    if (skin == nullptr) {
        std::printf("zoo: animals: %s has no skeleton\n", model.c_str());
        return nullptr;
    }
    kind->skeleton = Enhanced::SkeletonFromSkinningData(*skin);
    kind->clips = Enhanced::ClipsFromSkinningData(*skin);
    for (std::size_t i = 0; i < kClipNames.size(); ++i) {
        for (const auto& clip : kind->clips) {
            if (clip.name == kClipNames[i])
                kind->pose[i] = &clip;
        }
    }
    if (kind->pose[static_cast<std::size_t>(AnimalPose::Idle)] == nullptr) {
        std::printf("zoo: animals: %s has no idle clip\n", model.c_str());
        return nullptr;
    }
    // The generator's sidecar says how fast each gait covers ground.
    try {
        std::ifstream file(*directory_ / "animals" / (model + ".json"));
        const nlohmann::json sidecar = nlohmann::json::parse(file);
        for (const auto& clip : sidecar.at("clips")) {
            const std::string name = clip.value("name", "");
            const float speed = clip.value("speed", 0.0f);
            if (name == "walk" && speed > 0.0f)
                kind->walkSpeed = speed;
            if (name == "trot" && speed > 0.0f)
                kind->trotSpeed = speed;
            if (name == "swim" && speed > 0.0f)
                kind->swimSpeed = speed;
        }
        const auto& meta = sidecar.at("meta");
        kind->shoulderHeight = std::max(0.01f, meta.value("shoulder_height_m", 1.0f));
        kind->relativeHeight = std::clamp(meta.value("relative_height", 1.0f), 0.3f, 1.5f);
        kind->swimDepth = meta.value("swim_depth_m", kFloatSunkShare * kind->shoulderHeight);
        // The birds' sidecars give a depth for each of their two water clips.
        if (const auto swim = meta.find("swim"); swim != meta.end() && swim->is_object())
            kind->swimDepth = swim->value("depth_m", kind->swimDepth);
        kind->floatDepth = kind->swimDepth;
        if (const auto still = meta.find("float"); still != meta.end() && still->is_object())
            kind->floatDepth = still->value("depth_m", kind->floatDepth);
        if (const auto shown = meta.find("activity_clips"); shown != meta.end() && shown->is_object())
            ReadActivityClips(*shown, kind->pose, kind->shown);
    } catch (const std::exception& error) {
        std::printf("zoo: animals: %s.json: %s\n", model.c_str(), error.what());
    }
    slot = std::move(kind);
    return slot.get();
}

const AnimalRenderer::Look& AnimalRenderer::LookOf(const zoo::SpeciesDef& def, int index) {
    Look& look = looks_[static_cast<std::size_t>(index)];
    if (!look.resolved) {
        look.resolved = true;
        look.kind = Load(def.model);
        if (look.kind != nullptr) {
            look.female = Load(def.model + "_female");
            look.juvenile = Load(def.model + "_juvenile");
        } else {
            const std::string standIn = StandInFor(def);
            look.kind = Load(standIn);
            if (look.kind != nullptr) {
                standIns_.push_back(def.id);
                std::printf("zoo: animals: %s has no model yet; a %s stands in\n", def.id.c_str(),
                            standIn.c_str());
            } else {
                std::printf("zoo: animals: %s cannot be drawn\n", def.id.c_str());
            }
        }
    }
    return look;
}

AnimalPose AnimalRenderer::Choose(const zoo::Animal& animal, const Actor& actor) const {
    using zoo::AnimalActivity;
    if (actor.afloat > 0.5f) {
        // Under way it swims; lying still on the water it floats, if its model has the clip
        // (and keeps floating until it really moves: no flicker at the boundary).
        const bool still = animal.speed < (actor.pose == AnimalPose::Float ? 0.12f : 0.05f);
        const bool canFloat = actor.kind->pose[static_cast<std::size_t>(AnimalPose::Float)] != nullptr;
        return still && canFloat ? AnimalPose::Float : AnimalPose::Swim;
    }
    if (animal.speed > 0.05f) {
        // Between the two gaits the one in use is kept: no flicker at the boundary.
        const float change = 0.5f * (actor.kind->walkSpeed + actor.kind->trotSpeed) * actor.scale;
        const float margin = actor.pose == AnimalPose::Trot ? 0.9f : 1.1f;
        return animal.speed > change * margin ? AnimalPose::Trot : AnimalPose::Walk;
    }
    if (animal.activity == AnimalActivity::Vocalize) {
        // The call is given once; then the animal stands.
        if (actor.pose == AnimalPose::Vocalize && actor.animator.Finished())
            return AnimalPose::Idle;
        return actor.pose == AnimalPose::Idle && actor.called ? AnimalPose::Idle : AnimalPose::Vocalize;
    }
    const auto activity = static_cast<std::size_t>(animal.activity);
    return activity < actor.kind->shown.size() ? actor.kind->shown[activity] : AnimalPose::Idle;
}

void AnimalRenderer::Update(const zoo::Zoo& zoo, float seconds, float tickFraction) {
    const zoo::World& world = zoo.Map();
    const zoo::GameTime now = zoo.Time();
    const bool stepped = now.tick != tick_;
    tick_ = now.tick;
    const float alpha = std::clamp(tickFraction, 0.0f, 1.0f);
    for (auto& [id, actor] : actors_)
        actor->seen = false;
    for (const zoo::Animal& animal : zoo.Fauna().All()) {
        if (animal.species < 0 || animal.species >= static_cast<int>(looks_.size()))
            continue;
        const zoo::SpeciesDef& def = species_.All()[static_cast<std::size_t>(animal.species)];
        const Look& look = LookOf(def, animal.species);
        if (look.kind == nullptr)
            continue;
        const bool female = animal.sex == zoo::Sex::Female;
        const float grown = zoo::AgeYears(animal, now) / std::max(0.001f, def.maturityYears);
        const Kind* kind = look.kind;
        if (look.juvenile != nullptr && grown < kJuvenileShare)
            kind = look.juvenile;
        else if (female && look.female != nullptr)
            kind = look.female;
        auto& slot = actors_[animal.id];
        // A young animal that has grown up gets its adult body: a new actor in the same place.
        const bool fresh = slot == nullptr || slot->kind != kind;
        if (fresh)
            slot = std::make_unique<Actor>(*kind);
        Actor& actor = *slot;
        actor.seen = true;
        const bool visible = animal.Visible();
        const bool appeared = fresh || (visible && !actor.visible);
        actor.visible = visible;
        if (!visible)
            continue;
        const float femaleShare = female && look.female != nullptr ? look.female->relativeHeight : 1.0f;
        const float bodyScale = zoo::BodyScale(animal, def, now);
        const float standing = def.shoulderHeightM * femaleShare * bodyScale;
        actor.scale = standing / kind->shoulderHeight;
        actor.standing = standing;
        const float length = def.bodyLengthM * bodyScale;
        // Half the body's diagonal, plus enough for a tall animal's morning shadow. This is
        // deliberately generous: submitting a marginal animal is cheap; losing its visible
        // nose or shadow at a screen edge is not.
        actor.bounds = 0.5f * std::hypot(length, standing) + standing;

        // Where it stands: between the last two places the simulation gave it.
        if (appeared) {
            actor.from = actor.to = actor.shown = animal.position;
            actor.heading = animal.heading;
            actor.pitch = 0.0f;
        } else if (stepped) {
            actor.from = actor.to;
            actor.to = animal.position;
            const zoo::Vec2 jump = actor.to - actor.from;
            if (jump.x * jump.x + jump.z * jump.z > 16.0f) {
                actor.from = actor.to;  // carried, not walked
                actor.heading = animal.heading;
            }
        }
        actor.shown = actor.from + (actor.to - actor.from) * alpha;

        // In water deeper than it can stand in, it floats (and stops floating a little later
        // than it started, so that a shore does not make it bob between the two).
        const float depth = world.WaterDepthAt(actor.shown);
        const bool floats = depth > (actor.afloat > 0.5f ? 0.8f : 1.0f) * kFloatDepthShare * standing;
        if (appeared)
            actor.afloat = floats ? 1.0f : 0.0f;
        else
            actor.afloat += ((floats ? 1.0f : 0.0f) - actor.afloat) * std::min(1.0f, 2.5f * seconds);
        const float turn = WrapAngle(animal.heading - actor.heading);
        const float most = (2.5f + 1.5f * animal.speed) * seconds;
        actor.heading = WrapAngle(actor.heading + std::clamp(turn, -most, most));

        // What it does.
        if (animal.activity != zoo::AnimalActivity::Vocalize)
            actor.called = false;
        const AnimalPose pose = Choose(animal, actor);
        const auto* clip = kind->pose[static_cast<std::size_t>(pose)];
        const bool paddles = pose == AnimalPose::Swim && clip == nullptr;
        if (paddles)
            clip = kind->pose[static_cast<std::size_t>(AnimalPose::Walk)];  // legs working under water
        if (clip == nullptr)
            clip = kind->pose[static_cast<std::size_t>(AnimalPose::Idle)];
        Animation::PlayOptions options;
        options.loop = pose != AnimalPose::Vocalize;
        if (pose == AnimalPose::Swim) {
            const float gait = (paddles ? kind->walkSpeed : kind->swimSpeed) * actor.scale;
            options.speed = std::clamp(animal.speed / std::max(gait, 0.05f), 0.5f, paddles ? 1.3f : 2.0f);
        } else if (pose == AnimalPose::Walk || pose == AnimalPose::Trot) {
            const float gait = (pose == AnimalPose::Walk ? kind->walkSpeed : kind->trotSpeed) * actor.scale;
            options.speed = std::clamp(animal.speed / std::max(gait, 0.05f), 0.4f, 2.2f);
        }
        if (pose != actor.pose) {
            const bool slow = SettlesSlowly(pose) || SettlesSlowly(actor.pose);
            options.fadeSeconds = fresh ? 0.0f : slow ? 0.9f : 0.35f;
            options.restart = pose == AnimalPose::Vocalize;
            if (pose == AnimalPose::Vocalize)
                actor.called = true;
            actor.animator.Play(*clip, options);
            if (fresh) {
                // A herd bought together does not breathe in step.
                actor.animator.SetTime(clip->duration * static_cast<float>(animal.id.value % 17) / 17.0f);
            }
            actor.pose = pose;
        } else if (options.loop) {
            options.fadeSeconds = 0.35f;
            actor.animator.Play(*clip, options);
        }
        actor.animator.Update(seconds);

        // On the ground, following its slope.
        const float reach = 0.4f * def.bodyLengthM * actor.scale;
        const zoo::Vec2 along = zoo::HeadingVector(actor.heading) * reach;
        const float front = DrawnGroundHeight(world, actor.shown + along);
        const float rear = DrawnGroundHeight(world, actor.shown - along);
        const float slope =
            LiesDown(pose) ? 0.0f : (1.0f - actor.afloat) * std::atan2(front - rear, 2.0f * reach);
        actor.pitch += (slope - actor.pitch) * std::min(1.0f, 6.0f * seconds);
        const float bed = std::max(DrawnGroundHeight(world, actor.shown), 0.5f * (front + rear));
        const float surface = world.HeightAt(actor.shown) + depth;
        const float sinks = pose == AnimalPose::Float ? kind->floatDepth : kind->swimDepth;
        actor.sunk = appeared ? sinks : actor.sunk + (sinks - actor.sunk) * std::min(1.0f, 1.5f * seconds);
        const float floating = std::max(bed, surface - actor.sunk * actor.scale);
        const float ground = bed + (floating - bed) * actor.afloat;
        actor.world = Matrix::CreateScale(actor.scale) * Matrix::CreateRotationX(-actor.pitch) *
                      Matrix::CreateRotationY(actor.heading) *
                      Matrix::CreateTranslation(actor.shown.x, ground, actor.shown.z);
    }
    std::erase_if(actors_, [](const auto& entry) { return !entry.second->seen; });
}

void AnimalRenderer::Submit(CnaEngine::App::Frame& frame, const CnaEngine::Graphics::Camera& camera) const {
    using Microsoft::Xna::Framework::BoundingSphere;
    using Microsoft::Xna::Framework::ContainmentType;
    const auto& frustum = camera.Frustum();
    drawn_ = 0;
    for (const auto& [id, actor] : actors_) {
        if (!actor->visible)
            continue;
        const Vector3 feet = actor->world.getTranslationProperty();
        const BoundingSphere bounds(feet + Vector3(0.0f, 0.5f * actor->standing, 0.0f), actor->bounds);
        if (frustum.Contains(bounds) == ContainmentType::Disjoint)
            continue;
        frame.Add(*actor->kind->view, actor->world, &actor->animator.SkinPalette());
        ++drawn_;
    }
}

std::optional<Vector3> AnimalRenderer::PositionOf(zoo::AnimalId id) const {
    const auto found = actors_.find(id);
    if (found == actors_.end() || !found->second->visible)
        return std::nullopt;
    return found->second->world.getTranslationProperty();
}

}  // namespace zoogame
