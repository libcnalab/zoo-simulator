// SPDX-License-Identifier: MIT
#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

#include "CnaEngine/Graphics/Camera.hpp"
#include "CnaEngine/Graphics/ForwardRenderer.hpp"

#include "Microsoft/Xna/Framework/GamerServices/AvatarDescription.hpp"
#include "Microsoft/Xna/Framework/GamerServices/AvatarRenderer.hpp"

#include "AvatarPose.hpp"
#include "Common.hpp"
#include "zoo/sim/Zoo.hpp"

namespace zoogame {

/// Draws the zoo's visitors as CNA avatars.
///
/// A cast of avatars is made once (random people; assembling one takes CNA a
/// moment on a thread of its own) and every visitor is played by the member
/// its `look` picks, sized to the visitor's height. The simulation moves
/// people twenty times a second; between its steps they glide and turn, their
/// steps follow the ground they cover (AvatarPose: the feet do not slide),
/// and what they do otherwise comes from the simulation: standing people
/// play CNA's own idle animations and react to what they think, people on
/// benches sit, people with food eat.
///
/// Avatars draw themselves with CNA's own effect, not through the engine's
/// renderer: Draw() is called from a render pass after the opaque scene, with
/// the scene's light. They cast no shadow in the shadow map.
class VisitorRenderer {
public:
    VisitorRenderer();
    ~VisitorRenderer();

    /// Forgets everybody (another zoo is about to be shown). The cast stays.
    void Clear();
    /// `seconds` of visitor time have passed (real time times the game speed;
    /// 0 while paused); `tickFraction` is how far the game is into the next
    /// simulation step (0..1).
    void Update(const zoo::Zoo& zoo, float seconds, float tickFraction);
    /// Draws everybody the camera sees.
    void Draw(const CnaEngine::Graphics::Camera& camera,
              const CnaEngine::Graphics::ForwardLighting& lighting);

    /// Where a visitor is drawn (the ground under it); nothing for one that is not drawn.
    std::optional<Vector3> PositionOf(zoo::VisitorId id) const;
    /// The height a visitor is drawn at, metres.
    float HeightOf(zoo::VisitorId id) const;
    std::size_t Drawn() const { return drawn_; }
    /// People near enough to be drawn at the last Draw(), whichever way the camera looked.
    std::size_t InRange() const { return inRange_; }
    /// Of them those the camera looked at; Drawn() is less when they were more than the budget.
    std::size_t InView() const { return inViewCount_; }
    /// The most people a frame draws: the nearest of those in view.
    void SetBudget(std::size_t people) { budget_ = std::max<std::size_t>(people, 1); }
    std::size_t Budget() const { return budget_; }
    /// People the simulation has in the zoo (those inside buildings too).
    std::size_t Known() const { return actors_.size(); }
    /// The cast: how many avatars there are to play the visitors, and how many
    /// of them CNA could not make.
    std::size_t CastSize() const { return cast_.size(); }
    int Failed() const { return failed_; }
    /// Members of the cast that can be drawn by now.
    int Ready() const { return ready_; }
    /// False once it is known that CNA cannot provide avatars at all.
    bool Available() const { return available_; }

private:
    using Preset = AvatarAnimationPreset;

    struct Member {
        Microsoft::Xna::Framework::GamerServices::AvatarDescription description;
        std::unique_ptr<Microsoft::Xna::Framework::GamerServices::AvatarRenderer> renderer;
        AvatarBody body;
        float height = 1.7f;
        bool female = false;
        bool ready = false;
        bool failed = false;
    };
    struct Actor {
        std::size_t member = 0;
        zoo::Vec2 from;
        zoo::Vec2 to;
        zoo::Vec2 shown;
        float ground = 0.0f;
        float heading = 0.0f;
        float heightM = 1.7f;
        zoo::VisitorActivity activity = zoo::VisitorActivity::Idle;
        zoo::VisitorThought thought = zoo::VisitorThought::None;
        std::int64_t thoughtTick = -1;
        bool seated = false;
        bool hidden = false;
        bool seen = false;
        // Weights of the things a body can be doing, eased towards 0 or 1.
        float gait = 0.0f;
        float run = 0.0f;
        float sit = 0.0f;
        float eat = 0.0f;
        float read = 0.0f;
        float phase = 0.0f;  ///< through the cycle of the gait
        Preset base = Preset::Stand0;
        float baseTime = 0.0f;
        /// An animation played once on top of the standing one.
        std::optional<Preset> shot;
        float shotTime = 0.0f;
        float fidgetIn = 3.0f;  ///< seconds until the next one, while standing
        std::uint32_t fidgets = 0;
        float eatTime = 0.0f;
    };

    void GrowCast();
    void PollCast();
    /// The member that plays an actor now: its own, or another while its own is not ready.
    const Member* Playing(const Actor& actor) const;
    const AvatarClip& Clip(Preset preset);
    void React(Actor& actor, zoo::VisitorId id, const Member* member, bool fresh);
    Preset Fidget(const Actor& actor, zoo::VisitorId id, bool female);
    void Pose(Actor& actor, const Member& member, float scale);

    std::vector<std::unique_ptr<Member>> cast_;
    std::array<std::optional<AvatarClip>, 64> clips_;
    std::unordered_map<zoo::VisitorId, Actor> actors_;
    std::vector<Matrix> bones_;
    AvatarPose pose_;
    AvatarExpression expression_;
    std::int64_t tick_ = -1;
    std::size_t drawn_ = 0;
    std::size_t inRange_ = 0;
    std::size_t inViewCount_ = 0;
    std::size_t budget_ = 400;
    std::vector<std::pair<float, Actor*>> inView_;
    int ready_ = 0;
    int failed_ = 0;
    bool available_ = true;
    bool reported_ = false;
    // What it costs (reported at the end of a run with ZOO_AVATAR_DEBUG set).
    std::uint64_t frames_ = 0;
    std::uint64_t drawnInAll_ = 0;
    double poseSeconds_ = 0.0;
    double drawSeconds_ = 0.0;
};

}  // namespace zoogame
