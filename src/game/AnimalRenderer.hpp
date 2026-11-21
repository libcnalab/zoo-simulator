// SPDX-License-Identifier: MIT
#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "CnaEngine/Animation/Animator.hpp"
#include "CnaEngine/App/EngineGame.hpp"
#include "CnaEngine/Graphics/Camera.hpp"
#include "CnaEngine/Graphics/GpuMesh.hpp"
#include "CnaEngine/Graphics/Material.hpp"
#include "CnaEngine/Graphics/ModelView.hpp"

#include "Microsoft/Xna/Framework/Content/ContentManager.hpp"
#include "Microsoft/Xna/Framework/Graphics/Model.hpp"
#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"

#include "Common.hpp"
#include "zoo/sim/Zoo.hpp"

namespace zoogame {

/// What an animal's body shows: one clip of its model each. The poses after `Float` are activity
/// clips that only some models have, played where the model's sidecar maps an activity to them
/// (`meta.activity_clips`).
enum class AnimalPose : std::uint8_t {
    Idle, Walk, Trot, Eat, Drink, Rest, Sleep, Vocalize, Swim, Float, Sentry, Dig, Preen,
    DustBathe, Ruminate, FilterFeed, Bathe, Huddle, Browse, Nuzzle, Groom, Count
};

/// Draws the zoo's animals: one skinned model per species (assets/animals,
/// loaded when the first animal of the species appears), one animator per
/// animal. A species may have two more models beside `<model>.glb`, loaded
/// only when an animal that needs that body appears:
/// `<model>_female.glb` for its adult females and `<model>_juvenile.glb` for
/// its young (their own proportions, not the adult scaled down); without
/// them the one model serves all. An animal in water deeper than it can
/// stand in floats at the surface, with the model's `swim` clip if it has one. The simulation moves animals twenty times a second; between its
/// steps the bodies glide, turn at a believable rate, and their gaits play at
/// the pace of the ground they cover, so the feet do not slide. Bodies wholly
/// outside the camera are not submitted to either render pass.
class AnimalRenderer {
public:
    AnimalRenderer(Microsoft::Xna::Framework::Content::ContentManager& content,
                   const zoo::SpeciesCatalog& species);
    ~AnimalRenderer();

    /// `seconds` of animal time have passed (real time times the game speed;
    /// 0 while paused); `tickFraction` is how far the game is into the next
    /// simulation step (0..1).
    void Update(const zoo::Zoo& zoo, float seconds, float tickFraction);
    /// For a newly adopted zoo, forget the old actors but retain the expensive species models.
    void Clear();
    void Submit(CnaEngine::App::Frame& frame, const CnaEngine::Graphics::Camera& camera) const;

    /// Where the animal's body stands (its feet); nothing for an animal without
    /// a body in the world (unknown, crated, inside a shelter).
    std::optional<Vector3> PositionOf(zoo::AnimalId id) const;
    /// Bodies submitted for the last frame, after camera culling.
    std::size_t Drawn() const { return drawn_; }
    /// Species drawn with a stand-in because their own model does not exist yet.
    const std::vector<std::string>& StandIns() const { return standIns_; }

private:
    struct Kind {
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::Model> model;
        std::unique_ptr<CnaEngine::Graphics::ModelView> view;
        /// The model's textures with mip levels (AnimalTextures.hpp); the view's materials use them.
        std::vector<std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D>> textures;
        CnaEngine::Animation::Skeleton skeleton;
        std::vector<CnaEngine::Animation::AnimationClip> clips;
        std::array<const CnaEngine::Animation::AnimationClip*, static_cast<std::size_t>(AnimalPose::Count)>
            pose{};
        /// Ground speed (m/s) each gait covers at its own playback rate.
        float walkSpeed = 1.0f;
        float trotSpeed = 2.5f;
        float swimSpeed = 0.8f;
        float shoulderHeight = 1.0f;
        /// A female model's adult height as a fraction of the species' (the male's).
        float relativeHeight = 1.0f;
        /// How far under the surface the feet of the floating body are, in model metres: under
        /// way (`swim`), and lying still on the water (`float`, for a model that has the clip).
        float swimDepth = 0.0f;
        float floatDepth = 0.0f;
        /// The clip a still animal plays for each activity: the defaults, changed by the model's
        /// sidecar (`meta.activity_clips`) where its clips suit a species better.
        std::array<AnimalPose, static_cast<std::size_t>(zoo::AnimalActivity::Count)> shown{};
    };
    struct Actor {
        explicit Actor(const Kind& k) : kind(&k), animator(k.skeleton) {}
        const Kind* kind;
        CnaEngine::Animation::Animator animator;
        AnimalPose pose = AnimalPose::Count;
        zoo::Vec2 from;
        zoo::Vec2 to;
        zoo::Vec2 shown;
        float heading = 0.0f;
        float pitch = 0.0f;
        float scale = 1.0f;
        float standing = 1.0f;  ///< height in the world, for conservative camera culling
        float bounds = 2.0f;    ///< body and the shadow it may cast, metres from its middle
        float afloat = 0.0f;  ///< 0 standing on the ground .. 1 floating at the surface
        float sunk = 0.0f;    ///< how deep it lies in the water now, model metres (Kind::swimDepth)
        float water = 0.0f;   ///< 0 dry/still .. 1 making a visible disturbance
        float waterY = 0.0f;
        float waterPhase = 0.0f;
        float waterWidth = 0.0f;
        float waterLength = 0.0f;
        Matrix world;
        bool visible = false;
        bool seen = false;
        bool called = false;  ///< has given the call of its current Vocalize activity
    };

    struct Look;
    /// The models of a species: its own, or a stand-in; no kind when not even that loads.
    const Look& LookOf(const zoo::SpeciesDef& def, int index, bool needsFemale, bool needsJuvenile);
    const Kind* Load(const std::string& model);
    AnimalPose Choose(const zoo::Animal& animal, const Actor& actor) const;

    Microsoft::Xna::Framework::Content::ContentManager& content_;
    const zoo::SpeciesCatalog& species_;
    std::optional<std::filesystem::path> directory_;
    std::unordered_map<std::string, std::unique_ptr<Kind>> kinds_;
    /// Per species index: the kinds it is drawn with.
    struct Look {
        const Kind* kind = nullptr;      ///< adults (the males, where there is a female model)
        const Kind* female = nullptr;    ///< adult females, or nullptr
        const Kind* juvenile = nullptr;  ///< the young, or nullptr
        bool resolved = false;
        bool own = false;  ///< kind is this species' model, so its named alternates may exist
        bool femaleResolved = false;
        bool juvenileResolved = false;
    };
    std::vector<Look> looks_;
    std::unordered_map<zoo::AnimalId, std::unique_ptr<Actor>> actors_;
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> waterTexture_;
    CnaEngine::Graphics::Material waterMaterial_;
    CnaEngine::Graphics::GpuMesh waterMesh_;
    std::vector<std::string> standIns_;
    std::int64_t tick_ = -1;
    mutable std::size_t drawn_ = 0;
};

}  // namespace zoogame
