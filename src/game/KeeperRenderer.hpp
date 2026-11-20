// SPDX-License-Identifier: MIT
#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "CnaEngine/Graphics/Camera.hpp"
#include "CnaEngine/Graphics/ForwardRenderer.hpp"

#include "Microsoft/Xna/Framework/Graphics/BasicEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexPositionColor.hpp"

#include "Microsoft/Xna/Framework/GamerServices/AvatarDescription.hpp"
#include "Microsoft/Xna/Framework/GamerServices/AvatarRenderer.hpp"

#include "AvatarPose.hpp"
#include "Common.hpp"
#include "zoo/sim/Zoo.hpp"

namespace zoogame {

/// Draws the zoo's staff (zoo::Staff) at their work, as CNA avatars in the zoo's uniform.
///
/// A small repeatable cast of avatars is made once, all in the same work clothes (a green
/// polo shirt, khaki cargo trousers, boots and a bucket hat, see Uniformed in the .cpp),
/// so that a keeper is told from a visitor at a glance and looks the same on another run.
/// Every member of staff is played by the cast member their `look` picks.
///
/// What they do comes from the simulation: where they are, which way they face, whether
/// they walk, carry, push or are at a job, and which job. This class turns that into a
/// body (the gait of AvatarPose, a pose of its own for every kind of work: the hands are
/// placed and the arms solved) and puts the tools of the job into the hands: bucket,
/// wheelbarrow, rake, broom, hose with its jet, clipboard, sack, trowel, watering can.
/// The tools are a few hundred coloured triangles a keeper, built in place and lit on the
/// processor every frame for those in view only, and drawn in two calls for the whole
/// crew (the solid ones, then water and the patches of shade). Small projected patches
/// join each tool to the ground without another draw call.
///
/// The crew is at most a dozen people; everybody in view within kFarthest is drawn as a
/// full avatar (there are no pictures for the distance as the visitors have).
class KeeperRenderer {
public:
    using GraphicsDevice = Microsoft::Xna::Framework::Graphics::GraphicsDevice;

    KeeperRenderer();
    ~KeeperRenderer();

    /// Forgets everybody (another zoo was loaded). The cast is kept.
    void Clear();

    /// Follows the simulation: `seconds` of time lived, `tickFraction` of the way from the
    /// last simulation step to the next. Also where the cast is polled as CNA assembles it.
    void Update(const zoo::Zoo& zoo, float seconds, float tickFraction);

    void Draw(GraphicsDevice& device, const CnaEngine::Graphics::Camera& camera,
              const CnaEngine::Graphics::ForwardLighting& lighting);

    /// Staff drawn by the last Draw().
    std::size_t Drawn() const { return drawn_; }
    /// Where a member of staff is shown (the feet), once the renderer has met them.
    std::optional<Vector3> PositionOf(zoo::StaffId id) const;

private:
    using AvatarDescription = Microsoft::Xna::Framework::GamerServices::AvatarDescription;
    using AvatarRenderer = Microsoft::Xna::Framework::GamerServices::AvatarRenderer;
    using Vertex = Microsoft::Xna::Framework::Graphics::VertexPositionColor;

    /// One avatar of the cast.
    struct Member {
        AvatarDescription description;
        std::unique_ptr<AvatarRenderer> renderer;
        AvatarBody body;
        float height = 1.7f;
        /// Shoulder to elbow and elbow to wrist, 0 when the skeleton is not laid out as
        /// this class assumes (then the arms are left to the standing pose).
        float upperArm = 0.0f;
        float foreArm = 0.0f;
        bool ready = false;
        bool failed = false;
    };

    /// One member of staff as shown.
    struct Actor {
        zoo::StaffId id;
        std::size_t member = 0;
        zoo::Vec2 from, to, shown;
        float ground = 0.0f;
        float heading = 0.0f;
        float gait = 0.0f;   ///< 0 standing .. 1 walking
        float phase = 0.0f;  ///< of the gait
        float baseTime = 0.0f;
        /// What is carried on the way: Walk (nothing in particular), Carry or Push.
        zoo::StaffActivity approach = zoo::StaffActivity::Walk;
        /// The job at hand, or the one being walked to.
        zoo::StaffActivity work = zoo::StaffActivity::Rest;
        bool working = false;
        float workTime = 0.0f;    ///< seconds at the job
        float workWeight = 0.0f;  ///< 0 .. 1 into the job's pose
        float holdWeight = 0.0f;  ///< 0 .. 1 into the pose of carrying or pushing
        /// Where the wheelbarrow stands while they rake: on their left (-1) or right (1),
        /// whichever is clear of fences and water; 0: nowhere (not drawn).
        float barrowSide = -1.0f;
        bool seen = false;
    };

    void PollCast();
    const Member* Playing(const Actor& actor) const;
    /// Fills pose_ and bones_ for the actor.
    void Pose(const Actor& actor, const Member& member);

    std::vector<std::unique_ptr<Member>> cast_;
    bool castMade_ = false;
    bool reported_ = false;
    std::vector<Actor> actors_;
    std::int64_t tick_ = -1;

    AvatarClip stand_;
    bool standBaked_ = false;
    AvatarPose pose_;
    AvatarExpression expression_{};
    std::vector<Matrix> bones_;

    std::vector<std::pair<float, Actor*>> inView_;
    std::vector<Vertex> solid_;
    std::vector<Vertex> sheer_;
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::BasicEffect> effect_;
    std::size_t drawn_ = 0;
};

}  // namespace zoogame
