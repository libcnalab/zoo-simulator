// SPDX-License-Identifier: MIT
#include "Walker.hpp"

#include <algorithm>
#include <cmath>

#include "Microsoft/Xna/Framework/BoundingBox.hpp"
#include "Microsoft/Xna/Framework/Ray.hpp"

#include "GroundPaint.hpp"

namespace zoogame {

namespace {

using CnaEngine::Collision::CharacterSettings;
using CnaEngine::Collision::HeightField;
using Microsoft::Xna::Framework::BoundingBox;

constexpr float kPi = 3.14159265358979f;
/// Deeper water than this is not waded.
constexpr float kWadeDepth = 0.35f;
/// The eyes look this far up and down at most.
constexpr float kMaxPitch = 1.35f;

CharacterSettings Body() {
    CharacterSettings settings;
    settings.radius = Walker::kBodyRadius;
    settings.height = 1.75f;
    settings.eyeHeight = Walker::kEyeHeight;
    // No ledges: the ground is a height field (a facet steeper than a body climbs rises two
    // metres), and fences, walls, trunks and posts all stand higher than a step. With a step
    // height cna-engine's controller, stopped by a wall, tries a hop of the body's radius and
    // keeps it wherever the ground beyond lies a millimetre higher: on any rising ground the
    // body shot along a fence at 0.29 m a step (26 m/s) and through gaps narrower than itself.
    settings.stepHeight = 0.0f;
    settings.stepDown = 0.45f;
    settings.acceleration = 10.0f;
    settings.deceleration = 12.0f;
    // A footstep and a sway of the eyes every 1.5 m: at the fast walk of 4.4 m/s about three
    // steps a second (with the stroll's 0.72 m it would have been six), at the run six.
    settings.strideLength = 1.5f;
    return settings;
}

/// The body stands deeper in a wall, a trunk or a post than being pushed out of it leaves it.
bool Wedged(const CnaEngine::Collision::StaticWorld& world, const CnaEngine::Collision::CharacterController& body) {
    for (const auto& contact : world.Contacts(body.Body())) {
        if (std::abs(contact.normal.Y) < 0.7f && contact.depth > 0.005f)
            return true;
    }
    return false;
}

float WrapAngle(float a) {
    // A finite but damaged saved heading can be so large that subtracting one turn
    // rounds back to the same float forever. Reduction must take bounded time.
    if (!std::isfinite(a))
        return 0.0f;
    if (std::abs(a) > 2.0f * kPi)
        a = std::fmod(a, 2.0f * kPi);
    while (a > kPi)
        a -= 2.0f * kPi;
    while (a < -kPi)
        a += 2.0f * kPi;
    return a;
}

/// Radius of the part the player collides with, including furniture simulation visitors pass.
float PlayerPropRadius(const zoo::PropDef& def, float scale) {
    if (def.blocksWalking)
        return def.radiusM * scale;
    switch (def.role) {
        case zoo::PropRole::Bench:
            return 0.55f;
        case zoo::PropRole::Bin:
            return 0.3f;
        case zoo::PropRole::Lamp:
            return 0.14f;
        case zoo::PropRole::InfoBoard:
            return 0.25f;
        default:
            return 0.0f;  // flower beds are stepped over
    }
}

}  // namespace

bool ClearOfPlayerProps(const zoo::Zoo& zoo, zoo::Vec2 at) {
    return std::ranges::none_of(zoo.Objects().Props(), [&](const zoo::Prop& prop) {
        if (prop.def < 0)
            return false;
        const auto& def = zoo.Definitions().props[static_cast<std::size_t>(prop.def)];
        const float radius = PlayerPropRadius(def, prop.scale);
        const float clearance = radius + Walker::kBodyRadius;
        return radius > 0.0f && zoo::DistanceSquared(at, prop.position) < clearance * clearance;
    });
}

bool CanLandOnPath(const zoo::Zoo& zoo, zoo::TileCoord tile) {
    const auto& world = zoo.Map();
    if (!world.InBounds(tile))
        return false;
    const auto* home = zoo.Habitats().Find(zoo.Habitats().At(tile));
    return world.At(tile).path != zoo::PathKind::None && !world.At(tile).building &&
           !world.At(tile).standOff && !world.HasWater(tile) &&
           (zoo.Habitats().RegionAt(tile) == zoo::Region::Public || (home && home->walkThrough)) &&
           zoo.Routes().Reachable(world.EntranceTile(), tile, zoo::NavProfile::Visitor()) &&
           ClearOfPlayerProps(zoo, zoo::World::TileCenter(tile)) &&
           !zoo.Fauna().CrowdsResident(zoo.Habitats(), zoo.SpeciesList(), zoo.Time(),
                                       zoo::World::TileCenter(tile));
}

Walker::Walker() : world_(4.0f), body_(Body()) {}

void Walker::Rebuild(const zoo::Zoo& zoo) {
    const zoo::World& map = zoo.Map();
    const zoo::Catalog& catalog = zoo.Definitions();
    world_.Clear();

    HeightField ground;
    ground.origin = Vector3::Zero;
    ground.cellSize = zoo::kTileSize;
    ground.columns = map.Width() + 1;
    ground.rows = map.Depth() + 1;
    ground.heights.resize(static_cast<std::size_t>(ground.columns) * static_cast<std::size_t>(ground.rows));
    for (int vz = 0; vz < ground.rows; ++vz) {
        for (int vx = 0; vx < ground.columns; ++vx) {
            // As drawn: dry land beside a pool is not dug down to its bed.
            ground.heights[static_cast<std::size_t>(vz * ground.columns + vx)] =
                DrawnVertexHeight(map, vx, vz);
        }
    }
    world_.SetHeightField(std::move(ground));

    const float tile = zoo::kTileSize;
    const auto wall = [&](float x0, float z0, float x1, float z1, float height) {
        // From under the lower end's ground to `height` above the higher end's.
        const float a = DrawnGroundHeight(map, {x0, z0});
        const float b = DrawnGroundHeight(map, {x1, z1});
        constexpr float kHalf = 0.11f;
        world_.AddBox(BoundingBox(Vector3(std::min(x0, x1) - kHalf, std::min(a, b) - 1.0f, std::min(z0, z1) - kHalf),
                                  Vector3(std::max(x0, x1) + kHalf, std::max(a, b) + height,
                                          std::max(z0, z1) + kHalf)));
    };
    const auto heightOf = [&](zoo::Fence fence) {
        if (fence.kind == zoo::kPerimeterFence)
            return zoo::kPerimeterHeightM;
        const zoo::FenceDef* def = catalog.FenceByKind(fence.kind);
        // Nobody climbs over a fence here, however low it is.
        return std::max(def != nullptr ? def->heightM : 1.2f, 1.0f);
    };
    // Keeper gates stay shut. Visitors can pass through an aviary curtain.
    for (int z = 0; z <= map.Depth(); ++z) {
        for (int x = 0; x < map.Width(); ++x) {
            const zoo::Fence fence = map.FenceAt({x, z, true});
            if (fence.Exists() && !fence.visitorGate) {
                const float fx = static_cast<float>(x) * tile;
                const float fz = static_cast<float>(z) * tile;
                wall(fx, fz, fx + tile, fz, heightOf(fence));
            }
        }
    }
    for (int z = 0; z < map.Depth(); ++z) {
        for (int x = 0; x <= map.Width(); ++x) {
            const zoo::Fence fence = map.FenceAt({x, z, false});
            if (fence.Exists() && !fence.visitorGate) {
                const float fx = static_cast<float>(x) * tile;
                const float fz = static_cast<float>(z) * tile;
                wall(fx, fz, fx, fz + tile, heightOf(fence));
            }
        }
    }

    // Water one cannot wade: a tile of it is a block as high as a person.
    for (int z = 0; z < map.Depth(); ++z) {
        for (int x = 0; x < map.Width(); ++x) {
            const zoo::Vec2 centre = zoo::World::TileCenter({x, z});
            if (!map.HasWater({x, z}) || map.WaterDepthAt(centre) < kWadeDepth)
                continue;
            const float floor = DrawnGroundHeight(map, centre);
            const float surface = map.At({x, z}).waterLevel;
            const float x0 = static_cast<float>(x) * tile;
            const float z0 = static_cast<float>(z) * tile;
            world_.AddBox(BoundingBox(Vector3(x0 + 0.2f, floor - 2.0f, z0 + 0.2f),
                                      Vector3(x0 + tile - 0.2f, surface + 3.0f, z0 + tile - 0.2f)));
        }
    }

    for (const zoo::Building& building : zoo.Objects().Buildings()) {
        const zoo::BuildingDef& def = catalog.buildings[static_cast<std::size_t>(building.def)];
        const zoo::TileCoord size = zoo::FootprintSize(def, building.facing);
        const float x0 = static_cast<float>(building.origin.x) * tile;
        const float z0 = static_cast<float>(building.origin.z) * tile;
        const float x1 = x0 + static_cast<float>(size.x) * tile;
        const float z1 = z0 + static_cast<float>(size.z) * tile;
        const float floor = DrawnGroundHeight(map, {(x0 + x1) * 0.5f, (z0 + z1) * 0.5f});
        // A little inside the footprint: eaves and counters reach out to it, walls do not.
        constexpr float kInset = 0.25f;
        world_.AddBox(BoundingBox(Vector3(x0 + kInset, floor - 2.0f, z0 + kInset),
                                  Vector3(x1 - kInset, floor + std::max(def.heightM, 2.2f), z1 - kInset)));
    }

    for (const zoo::Prop& prop : zoo.Objects().Props()) {
        const zoo::PropDef& def = catalog.props[static_cast<std::size_t>(prop.def)];
        const float radius = PlayerPropRadius(def, prop.scale);
        if (radius <= 0.0f)
            continue;
        const float floor = DrawnGroundHeight(map, prop.position);
        world_.AddCylinder(Vector3(prop.position.x, floor - 0.5f, prop.position.z), radius,
                           std::max(def.heightM * prop.scale, 0.6f) + 0.5f);
    }

    // The edge of the land.
    const float sx = map.SizeX();
    const float sz = map.SizeZ();
    constexpr float kEdge = 1.0f;
    constexpr float kLow = -200.0f;
    constexpr float kHigh = 400.0f;
    world_.AddBox(BoundingBox(Vector3(-10.0f, kLow, -10.0f), Vector3(kEdge, kHigh, sz + 10.0f)));
    world_.AddBox(BoundingBox(Vector3(sx - kEdge, kLow, -10.0f), Vector3(sx + 10.0f, kHigh, sz + 10.0f)));
    world_.AddBox(BoundingBox(Vector3(-10.0f, kLow, -10.0f), Vector3(sx + 10.0f, kHigh, kEdge)));
    world_.AddBox(BoundingBox(Vector3(-10.0f, kLow, sz - kEdge), Vector3(sx + 10.0f, kHigh, sz + 10.0f)));

    objectsRevision_ = zoo.Objects().Revision();
    fenceRevision_ = map.FenceRevision();
    navRevision_ = map.NavRevision();
    built_ = true;
}

void Walker::Enter(const zoo::Zoo& zoo, zoo::Vec2 at, float yaw, float pitch) {
    if (!built_ || objectsRevision_ != zoo.Objects().Revision() ||
        fenceRevision_ != zoo.Map().FenceRevision() || navRevision_ != zoo.Map().NavRevision())
        Rebuild(zoo);
    const float ground = DrawnGroundHeight(zoo.Map(), at);
    body_ = CnaEngine::Collision::CharacterController(Body(), Vector3(at.x, ground, at.z));
    body_.Teleport(world_, Vector3(at.x, ground + 0.05f, at.z));
    yaw_ = WrapAngle(yaw);
    pitch_ = std::clamp(pitch, -kMaxPitch, kMaxPitch);
    eyeY_ = body_.Eye().Y;
    stepped_ = false;
}

void Walker::Update(const zoo::Zoo& zoo, const Input& input, float seconds) {
    if (!built_ || objectsRevision_ != zoo.Objects().Revision() ||
        fenceRevision_ != zoo.Map().FenceRevision() || navRevision_ != zoo.Map().NavRevision())
        Rebuild(zoo);

    // To the right is clockwise seen from above: the heading falls.
    yaw_ = WrapAngle(yaw_ - input.turn);
    pitch_ = std::clamp(pitch_ + input.tilt, -kMaxPitch, kMaxPitch);

    const Vector3 ahead(std::sin(yaw_), 0.0f, std::cos(yaw_));
    const Vector3 right(-std::cos(yaw_), 0.0f, std::sin(yaw_));
    Vector3 wish = right * std::clamp(input.right, -1.0f, 1.0f) + ahead * std::clamp(input.ahead, -1.0f, 1.0f);
    const float push = wish.Length();
    if (push > 1.0f)
        wish = wish * (1.0f / push);
    // Backwards and sideways nobody runs.
    const bool running = input.run && input.ahead > 0.1f;
    wish = wish * (running ? kRunSpeed : kWalkSpeed);

    // The body moves in steps of its own, so that a slow frame cannot carry it through a fence.
    stepped_ = false;
    float left = std::min(seconds, 0.25f);
    // cna-engine's controller pushes a body out of what it meets in four passes and keeps the
    // place it has then, also when that is still inside something. Between two solids a
    // little closer to each other than the body is wide (a bench and the lamp before it:
    // 0.51 m for 0.56 m) the passes push it from the one into the other, and step by step it
    // was carried through, into a corner with no way out. A step that ends so is not taken.
    bool clear = !Wedged(world_, body_);
    while (left > 1.0e-5f) {
        const float step = std::min(left, 1.0f / 90.0f);
        const Vector3 before = body_.Feet();
        body_.Step(world_, wish, false, step);
        const auto here = zoo::World::TileAt({before.X, before.Z});
        const auto next = zoo::World::TileAt(Position());
        const auto& map = zoo.Map();
        // The public walk in a shared exhibit ends at the boardwalk's edge.
        const zoo::Enclosure* bank = nullptr;
        if (here != next && map.InBounds(here) && map.InBounds(next) &&
            map.At(here).path != zoo::PathKind::None && map.At(next).path == zoo::PathKind::None) {
            bank = zoo.Habitats().Find(zoo.Habitats().At(next));
        }
        if ((bank && bank->walkThrough) || zoo.Fauna().CrowdsResident(zoo.Habitats(), zoo.SpeciesList(), zoo.Time(), Position(),
                                       zoo::Vec2{before.X, before.Z})) {
            body_.Teleport(world_, before);
        } else if (Wedged(world_, body_)) {
            if (clear)
                body_.Teleport(world_, before);
        } else {
            clear = true;
        }
        stepped_ = stepped_ || body_.StrideTaken();
        left -= step;
    }

    const float eye = body_.Eye().Y;
    // Steps, kerbs and the facets of the ground are taken with the knees, not the eyes.
    eyeY_ = eye + (eyeY_ - eye) * std::exp(-16.0f * seconds);
    if (std::abs(eyeY_ - eye) > 1.5f)
        eyeY_ = eye;
}

Vector3 Walker::Eye() const {
    const Vector3 feet = body_.Feet();
    return Vector3(feet.X, eyeY_ + (headBob_ > 0.0f ? body_.HeadBob(headBob_) : 0.0f), feet.Z);
}

Vector3 Walker::Gaze() const {
    return Vector3(std::sin(yaw_) * std::cos(pitch_), -std::sin(pitch_), std::cos(yaw_) * std::cos(pitch_));
}

float Walker::Speed() const {
    const Vector3 v = body_.Velocity();
    return std::sqrt(v.X * v.X + v.Z * v.Z);
}

float Walker::SightTo(const Vector3& direction, float reach) const {
    const auto hit = world_.Raycast(Microsoft::Xna::Framework::Ray(Eye(), direction), reach);
    return hit ? hit->distance : reach;
}

}  // namespace zoogame
