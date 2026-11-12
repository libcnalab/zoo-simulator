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
#include "CnaEngine/Graphics/ModelView.hpp"

#include "Microsoft/Xna/Framework/Content/ContentManager.hpp"
#include "Microsoft/Xna/Framework/Graphics/Model.hpp"

#include "Common.hpp"
#include "zoo/sim/Zoo.hpp"

namespace zoogame {

/// What an animal's body shows: one clip of its model each.
enum class AnimalPose : std::uint8_t { Idle, Walk, Trot, Eat, Drink, Rest, Sleep, Vocalize, Count };

/// Draws the zoo's animals: one skinned model per species (assets/animals,
/// loaded when the first animal of the species appears), one animator per
/// animal. The simulation moves animals twenty times a second; between its
/// steps the bodies glide, turn at a believable rate, and their gaits play at
/// the pace of the ground they cover, so the feet do not slide.
class AnimalRenderer {
public:
    AnimalRenderer(Microsoft::Xna::Framework::Content::ContentManager& content,
                   const zoo::SpeciesCatalog& species);
    ~AnimalRenderer();

    /// `seconds` of animal time have passed (real time times the game speed;
    /// 0 while paused); `tickFraction` is how far the game is into the next
    /// simulation step (0..1).
    void Update(const zoo::Zoo& zoo, float seconds, float tickFraction);
    void Submit(CnaEngine::App::Frame& frame) const;

    /// Where the animal's body is drawn (its feet); nothing for an animal
    /// that is not drawn (unknown, crated, inside a shelter).
    std::optional<Vector3> PositionOf(zoo::AnimalId id) const;
    std::size_t Drawn() const { return drawn_; }
    /// Species drawn with a stand-in because their own model does not exist yet.
    const std::vector<std::string>& StandIns() const { return standIns_; }

private:
    struct Kind {
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::Model> model;
        std::unique_ptr<CnaEngine::Graphics::ModelView> view;
        CnaEngine::Animation::Skeleton skeleton;
        std::vector<CnaEngine::Animation::AnimationClip> clips;
        std::array<const CnaEngine::Animation::AnimationClip*, static_cast<std::size_t>(AnimalPose::Count)>
            pose{};
        /// Ground speed (m/s) each gait covers at its own playback rate.
        float walkSpeed = 1.0f;
        float trotSpeed = 2.5f;
        float shoulderHeight = 1.0f;
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
        Matrix world;
        bool visible = false;
        bool seen = false;
        bool called = false;  ///< has given the call of its current Vocalize activity
    };

    /// The model of a species: its own, or a stand-in; nullptr when not even that loads.
    const Kind* KindOf(const zoo::SpeciesDef& def, int index);
    const Kind* Load(const std::string& model);
    AnimalPose Choose(const zoo::Animal& animal, const Actor& actor) const;

    Microsoft::Xna::Framework::Content::ContentManager& content_;
    const zoo::SpeciesCatalog& species_;
    std::optional<std::filesystem::path> directory_;
    std::unordered_map<std::string, std::unique_ptr<Kind>> kinds_;
    /// Per species index: the kind it is drawn with and the factor that sizes it.
    struct Look {
        const Kind* kind = nullptr;
        float scale = 1.0f;
        bool resolved = false;
    };
    std::vector<Look> looks_;
    std::unordered_map<zoo::AnimalId, std::unique_ptr<Actor>> actors_;
    std::vector<std::string> standIns_;
    std::int64_t tick_ = -1;
    std::size_t drawn_ = 0;
};

}  // namespace zoogame
