// SPDX-License-Identifier: MIT
#include "AnimalRenderer.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <exception>
#include <fstream>

#include <nlohmann/json.hpp>

#include "CnaEngine/App/AssetDirectory.hpp"
#include "CnaEngine/Enhanced/ModelImport.hpp"

namespace zoogame {

namespace {

using namespace CnaEngine;
namespace Xna = Microsoft::Xna::Framework::Graphics;

constexpr float kPi = 3.14159265f;

constexpr std::array<const char*, static_cast<std::size_t>(AnimalPose::Count)> kClipNames = {
    "idle", "walk", "trot", "eat", "drink", "rest", "sleep", "vocalize"};

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

}  // namespace

AnimalRenderer::AnimalRenderer(Microsoft::Xna::Framework::Content::ContentManager& content,
                               const zoo::SpeciesCatalog& species)
    : content_(content), species_(species), directory_(App::FindAssetDirectory("assets")) {
    looks_.resize(species.All().size());
}

AnimalRenderer::~AnimalRenderer() = default;

const AnimalRenderer::Kind* AnimalRenderer::Load(const std::string& model) {
    if (const auto found = kinds_.find(model); found != kinds_.end())
        return found->second.get();
    // A failure is remembered too: the file is not looked for again every frame.
    auto& slot = kinds_[model];
    if (!directory_ || !std::filesystem::exists(*directory_ / "animals" / (model + ".glb")))
        return nullptr;
    auto kind = std::make_unique<Kind>();
    try {
        kind->model = std::make_unique<Xna::Model>(content_.Load<Xna::Model>("animals/" + model));
    } catch (const std::exception& error) {
        std::printf("zoo: animals: %s: %s\n", model.c_str(), error.what());
        return nullptr;
    }
    kind->view = std::make_unique<Graphics::ModelView>(*kind->model, Enhanced::PbrMaterialFromEffect);
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
        }
        kind->shoulderHeight = sidecar.at("meta").value("shoulder_height_m", 1.0f);
    } catch (const std::exception& error) {
        std::printf("zoo: animals: %s.json: %s\n", model.c_str(), error.what());
    }
    slot = std::move(kind);
    return slot.get();
}

const AnimalRenderer::Kind* AnimalRenderer::KindOf(const zoo::SpeciesDef& def, int index) {
    Look& look = looks_[static_cast<std::size_t>(index)];
    if (!look.resolved) {
        look.resolved = true;
        look.kind = Load(def.model);
        if (look.kind == nullptr) {
            const std::string standIn = StandInFor(def);
            look.kind = Load(standIn);
            if (look.kind != nullptr) {
                look.scale = def.shoulderHeightM / look.kind->shoulderHeight;
                standIns_.push_back(def.id);
                std::printf("zoo: animals: %s has no model yet; a %s stands in\n", def.id.c_str(),
                            standIn.c_str());
            } else {
                std::printf("zoo: animals: %s cannot be drawn\n", def.id.c_str());
            }
        }
    }
    return look.kind;
}

AnimalPose AnimalRenderer::Choose(const zoo::Animal& animal, const Actor& actor) const {
    using zoo::AnimalActivity;
    if (animal.speed > 0.05f) {
        // Between the two gaits the one in use is kept: no flicker at the boundary.
        const float change = 0.5f * (actor.kind->walkSpeed + actor.kind->trotSpeed) * actor.scale;
        const float margin = actor.pose == AnimalPose::Trot ? 0.9f : 1.1f;
        return animal.speed > change * margin ? AnimalPose::Trot : AnimalPose::Walk;
    }
    switch (animal.activity) {
        case AnimalActivity::Forage:
        case AnimalActivity::Eat:
            return AnimalPose::Eat;
        case AnimalActivity::Drink:
            return AnimalPose::Drink;
        case AnimalActivity::Rest:
            return AnimalPose::Rest;
        case AnimalActivity::Sleep:
            return AnimalPose::Sleep;
        case AnimalActivity::Vocalize:
            // The call is given once; then the animal stands.
            if (actor.pose == AnimalPose::Vocalize && actor.animator.Finished())
                return AnimalPose::Idle;
            return actor.pose == AnimalPose::Idle && actor.called ? AnimalPose::Idle : AnimalPose::Vocalize;
        default:
            return AnimalPose::Idle;
    }
}

void AnimalRenderer::Update(const zoo::Zoo& zoo, float seconds, float tickFraction) {
    const zoo::World& world = zoo.Map();
    const zoo::GameTime now = zoo.Time();
    const bool stepped = now.tick != tick_;
    tick_ = now.tick;
    const float alpha = std::clamp(tickFraction, 0.0f, 1.0f);
    for (auto& [id, actor] : actors_)
        actor->seen = false;
    drawn_ = 0;

    for (const zoo::Animal& animal : zoo.Fauna().All()) {
        if (animal.species < 0 || animal.species >= static_cast<int>(looks_.size()))
            continue;
        const zoo::SpeciesDef& def = species_.All()[static_cast<std::size_t>(animal.species)];
        const Kind* kind = KindOf(def, animal.species);
        if (kind == nullptr)
            continue;
        auto& slot = actors_[animal.id];
        const bool fresh = slot == nullptr;
        if (fresh)
            slot = std::make_unique<Actor>(*kind);
        Actor& actor = *slot;
        actor.seen = true;
        const bool visible = animal.Visible();
        const bool appeared = fresh || (visible && !actor.visible);
        actor.visible = visible;
        if (!visible)
            continue;
        ++drawn_;
        actor.scale =
            looks_[static_cast<std::size_t>(animal.species)].scale * zoo::BodyScale(animal, def, now);

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
        const float turn = WrapAngle(animal.heading - actor.heading);
        const float most = (2.5f + 1.5f * animal.speed) * seconds;
        actor.heading = WrapAngle(actor.heading + std::clamp(turn, -most, most));

        // What it does.
        if (animal.activity != zoo::AnimalActivity::Vocalize)
            actor.called = false;
        const AnimalPose pose = Choose(animal, actor);
        const auto* clip = kind->pose[static_cast<std::size_t>(pose)];
        if (clip == nullptr)
            clip = kind->pose[static_cast<std::size_t>(AnimalPose::Idle)];
        Animation::PlayOptions options;
        options.loop = pose != AnimalPose::Vocalize;
        if (pose == AnimalPose::Walk || pose == AnimalPose::Trot) {
            const float gait = (pose == AnimalPose::Walk ? kind->walkSpeed : kind->trotSpeed) * actor.scale;
            options.speed = std::clamp(animal.speed / std::max(gait, 0.05f), 0.4f, 2.2f);
        }
        if (pose != actor.pose) {
            options.fadeSeconds = fresh ? 0.0f : LiesDown(pose) || LiesDown(actor.pose) ? 0.9f : 0.35f;
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
        const float front = world.HeightAt(actor.shown + along);
        const float rear = world.HeightAt(actor.shown - along);
        const float slope = LiesDown(pose) ? 0.0f : std::atan2(front - rear, 2.0f * reach);
        actor.pitch += (slope - actor.pitch) * std::min(1.0f, 6.0f * seconds);
        const float ground = std::max(world.HeightAt(actor.shown), 0.5f * (front + rear));
        actor.world = Matrix::CreateScale(actor.scale) * Matrix::CreateRotationX(-actor.pitch) *
                      Matrix::CreateRotationY(actor.heading) *
                      Matrix::CreateTranslation(actor.shown.x, ground, actor.shown.z);
    }
    std::erase_if(actors_, [](const auto& entry) { return !entry.second->seen; });
}

void AnimalRenderer::Submit(CnaEngine::App::Frame& frame) const {
    for (const auto& [id, actor] : actors_) {
        if (actor->visible)
            frame.Add(*actor->kind->view, actor->world, &actor->animator.SkinPalette());
    }
}

std::optional<Vector3> AnimalRenderer::PositionOf(zoo::AnimalId id) const {
    const auto found = actors_.find(id);
    if (found == actors_.end() || !found->second->visible)
        return std::nullopt;
    return found->second->world.getTranslationProperty();
}

}  // namespace zoogame
