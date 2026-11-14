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

#include "Microsoft/Xna/Framework/Graphics/AlphaTestEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/BasicEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Graphics/RenderTarget2D.hpp"
#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexPositionColorTexture.hpp"

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
/// the scene's light. They cast no shadow in the shadow map; everybody gets a
/// soft patch of shade on the ground under them instead.
///
/// An avatar is some thirty draw calls, each with its own effect state: about
/// 0.17 ms of the processor's time a person (measured, docs/PERFORMANCE.md).
/// So only the people near the eyes are drawn as avatars. Everybody farther
/// away is drawn flat: each member of the cast is drawn once into a sheet of
/// small pictures (standing, sitting and eight moments of a step, each from
/// eight sides), and a distant person is one upright rectangle with the picture
/// that fits what they do and where they are seen from. A whole avenue of them
/// costs a few draw calls.
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
    /// Makes what drawing needs and is not made yet (the sheet of pictures of one member
    /// of the cast at most a call). Called between frames: it draws into its own target.
    void Prepare(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device);
    /// Draws everybody the camera sees.
    void Draw(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device,
              const CnaEngine::Graphics::Camera& camera,
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
    /// The most people a frame draws as avatars: the nearest of those in view.
    void SetBudget(std::size_t people) { budget_ = std::max<std::size_t>(people, 1); }
    std::size_t Budget() const { return budget_; }
    /// Whether the people beyond the nearest are drawn flat (true: a visit on foot) or not
    /// at all (a camera that looks down on the zoo would see the rectangles for what they are).
    void SetFlat(bool flat) { flat_ = flat; }
    /// Of the people drawn on the last frame, those drawn flat.
    std::size_t DrawnFlat() const { return drawnFlat_; }
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
        /// The member's pictures for the distance; null until Prepare() has drawn them.
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::RenderTarget2D> sheet;
        /// The size of what one picture shows, metres.
        float cellWide = 1.0f;
        float cellTall = 2.0f;
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
        /// Drawn as an avatar on the last frame (kept a little beyond the distance at which
        /// one becomes one, so that nobody flickers between the two at the boundary).
        bool close = false;
        /// Whether it was one when this frame began (the rank of the nearest uses it).
        bool wasAvatar = false;
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
        // How it carries itself. The step is everybody's; the rest is kept up only while
        // it is drawn as an avatar (a picture in the distance shows none of it).
        float stride = 1.0f;  ///< the length of its step against its body's own
        float swing = 1.0f;   ///< how freely its arms go with the step
        float stoop = 0.0f;   ///< an old back
        float weight = 0.0f;  ///< standing: on the left leg (-1) .. on the right (1)
        float lookYaw = 0.0f;  ///< where the head is turned against the body, to the left
        float lookPitch = 0.0f;
        float speak = 0.0f;  ///< speaking with a hand
        /// What a standing person does with its hands for a while.
        enum class Manner : std::uint8_t { None, Lean, Point, Camera, Phone, Crouch };
        Manner manner = Manner::None;
        float mannerWeight = 0.0f;
        float pointYaw = 0.0f;
        float pointPitch = 0.0f;
    };

    void GrowCast();
    void PollCast();
    /// The member that plays an actor now: its own, or another while its own is not ready.
    const Member* Playing(const Actor& actor) const;
    const AvatarClip& Clip(Preset preset);
    void React(Actor& actor, zoo::VisitorId id, const Member* member, bool fresh);
    Preset Fidget(const Actor& actor, zoo::VisitorId id, bool female);
    /// What a person near the eyes does besides its activity: where it looks, whom it
    /// talks to, on which leg it stands, what its hands are busy with. All of it follows
    /// from the clock and from who it is, so it is the same on every run.
    void Carry(Actor& actor, const std::vector<zoo::Visitor>& all, std::size_t index, bool moving, double clock,
               float seconds);
    void Pose(Actor& actor, const Member& member, float scale);
    void Capture(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device, Member& member);
    void DrawShade(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device,
                   const CnaEngine::Graphics::Camera& camera,
                   const CnaEngine::Graphics::ForwardLighting& lighting);
    void DrawFlat(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device,
                  const CnaEngine::Graphics::Camera& camera,
                  const CnaEngine::Graphics::ForwardLighting& lighting);

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
    bool flat_ = false;
    std::size_t drawnFlat_ = 0;
    std::vector<std::pair<float, Actor*>> inView_;
    /// Of those in view: who is drawn flat, and where everybody drawn stands.
    struct Far {
        const Member* member;
        const Actor* actor;
        Vector3 feet;
    };
    std::vector<Far> far_;
    std::vector<Vector3> shades_;
    std::vector<Microsoft::Xna::Framework::Graphics::VertexPositionColorTexture> quads_;
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::AlphaTestEffect> flatEffect_;
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::BasicEffect> shadeEffect_;
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> shadeTexture_;
    int ready_ = 0;
    int failed_ = 0;
    bool available_ = true;
    bool reported_ = false;
    bool censused_ = false;  ///< ZOO_VISITOR_DEBUG has printed who is where
    /// ZOO_VISITOR_POSE (lean, point, camera, phone, crouch, old): everybody who can, does
    /// that or is that, all the time. For looking at one pose; None and false in the game.
    Actor::Manner show_ = Actor::Manner::None;
    bool showOld_ = false;
    // What it costs (reported at the end of a run with ZOO_AVATAR_DEBUG set).
    std::uint64_t frames_ = 0;
    std::uint64_t drawnInAll_ = 0;
    std::size_t closeInAll_ = 0;
    double poseSeconds_ = 0.0;
    double drawSeconds_ = 0.0;
    double flatSeconds_ = 0.0;
};

}  // namespace zoogame
