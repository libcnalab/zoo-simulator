// SPDX-License-Identifier: MIT
#include "Tools.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdio>
#include <span>

#include "BuildingModels.hpp"
#include "FenceModels.hpp"
#include "PropModels.hpp"

namespace zoogame {

namespace {

using CnaEngine::Graphics::GpuMesh;
using CnaEngine::Graphics::VertexLayout;

constexpr float kTile = zoo::kTileSize;
constexpr float kLift = 0.12f;  ///< marks float this far above the ground
constexpr float kPi = 3.14159265f;

struct Hasher {
    std::uint64_t value = 0xcbf29ce484222325ull;

    void Add(std::uint64_t v) {
        value ^= v;
        value *= 0x100000001b3ull;
        value ^= value >> 29;
    }
    void Add(float v) { Add(static_cast<std::uint64_t>(std::bit_cast<std::uint32_t>(v))); }
    void Add(int v) { Add(static_cast<std::uint64_t>(static_cast<std::uint32_t>(v))); }
};

Vector3 VertexAt(const zoo::World& world, int vx, int vz, float lift = 0.0f) {
    return Vector3(static_cast<float>(vx) * kTile, world.VertexHeight(vx, vz) + lift,
                   static_cast<float>(vz) * kTile);
}

/// The grid vertex nearest to a point.
zoo::TileCoord NearestVertex(const zoo::World& world, zoo::Vec2 at) {
    return {std::clamp(static_cast<int>(std::lround(at.x / kTile)), 0, world.Width()),
            std::clamp(static_cast<int>(std::lround(at.z / kTile)), 0, world.Depth())};
}

/// The edge of the tile under a point that the point is nearest to, and how far it is.
zoo::EdgeRef NearestEdge(zoo::Vec2 at, float* distance = nullptr) {
    const zoo::TileCoord tile = zoo::World::TileAt(at);
    const float fx = at.x - static_cast<float>(tile.x) * kTile;
    const float fz = at.z - static_cast<float>(tile.z) * kTile;
    const float toSide[4] = {fz, kTile - fx, kTile - fz, fx};  // north, east, south, west
    int best = 0;
    for (int i = 1; i < 4; ++i) {
        if (toSide[i] < toSide[best])
            best = i;
    }
    if (distance != nullptr)
        *distance = toSide[best];
    return zoo::EdgeOf(tile, static_cast<zoo::Dir>(best));
}

/// "12 tiles, $240" or why not.
void Explain(Play& play, const zoo::BuildPlan& plan, const char* unit, const char* units) {
    if (!plan.Ok()) {
        play.hint = std::string(zoo::Describe(plan.error));
        play.hintBad = true;
        return;
    }
    play.hint = Format("%d %s", plan.count, plan.count == 1 ? unit : units);
    if (plan.cost > 0)
        play.hint += ", " + zoo::FormatMoney(plan.cost);
    else if (plan.cost < 0)
        play.hint += ", " + zoo::FormatMoney(-plan.cost) + " back";
    if (plan.skipped > 0)
        play.hint += Format(" (%d left out: %s)", plan.skipped,
                            std::string(zoo::Describe(plan.skippedBecause)).c_str());
    play.hintBad = false;
}

/// Tells the player why what they just clicked did not happen.
bool Done(Play& play, const zoo::BuildPlan& plan) {
    if (plan.Ok())
        return true;
    if (plan.error != zoo::BuildError::NothingToDo)
        play.Flash(std::string(zoo::Describe(plan.error)));
    return false;
}

float BuildingGround(const zoo::World& world, zoo::TileCoord origin, zoo::TileCoord size) {
    float ground = world.VertexHeight(origin.x, origin.z);
    for (int z = 0; z <= size.z; ++z) {
        for (int x = 0; x <= size.x; ++x)
            ground = std::max(ground, world.VertexHeight(origin.x + x, origin.z + z));
    }
    return ground;
}

}  // namespace

zoo::AnimalId PickAnimal(const Play& play, const Pointer& pointer) {
    zoo::AnimalId best;
    float bestAlong = pointer.onGround ? pointer.distance + 1.0f : 1e9f;
    const auto& species = play.species->All();
    for (const zoo::Animal& animal : play.zoo->Fauna().All()) {
        const auto position = play.animals->PositionOf(animal.id);
        if (!position)
            continue;
        const zoo::SpeciesDef& def = species[static_cast<std::size_t>(animal.species)];
        // A ball about the body, generous enough to hit from far away.
        const float radius = std::max({0.5f * def.bodyLengthM, 0.5f * def.shoulderHeightM, 0.45f}) * 1.15f;
        const zoo::Vec3 center{position->X, position->Y + std::max(0.5f * def.shoulderHeightM, 0.3f),
                               position->Z};
        const zoo::Vec3 to = center - pointer.origin;
        const float along = zoo::Dot(to, pointer.direction);
        if (along <= 0.0f || along >= bestAlong)
            continue;
        const zoo::Vec3 nearest = pointer.origin + pointer.direction * along;
        // Far animals are small on screen: the ball grows a little with distance.
        const float reach = radius + along * 0.006f;
        if (zoo::Dot(nearest - center, nearest - center) <= reach * reach) {
            best = animal.id;
            bestAlong = along;
        }
    }
    return best;
}

WorldTools::WorldTools(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device, Surfaces& surfaces,
                       const zoo::Catalog& catalog)
    : device_(device), surfaces_(surfaces), catalog_(catalog) {}

void WorldTools::MarkTile(Mark mark, const zoo::World& world, zoo::TileCoord tile) {
    if (!world.InBounds(tile))
        return;
    Layer(mark).Quad(VertexAt(world, tile.x, tile.z, kLift), VertexAt(world, tile.x, tile.z + 1, kLift),
                     VertexAt(world, tile.x + 1, tile.z + 1, kLift),
                     VertexAt(world, tile.x + 1, tile.z, kLift));
}

void WorldTools::MarkEdge(Mark mark, const zoo::World& world, zoo::EdgeRef edge, float height) {
    if (!world.EdgeInBounds(edge))
        return;
    const Vector3 a = VertexAt(world, edge.x, edge.z);
    const Vector3 b =
        edge.horizontal ? VertexAt(world, edge.x + 1, edge.z) : VertexAt(world, edge.x, edge.z + 1);
    const Vector3 up(0.0f, height, 0.0f);
    Layer(mark).Quad(a, b, b + up, a + up);
}

void WorldTools::MarkBand(Mark mark, const zoo::World& world, zoo::TileCoord tile, zoo::Dir side,
                          float width) {
    // A strip lying on the ground just inside the tile's edge (a fence would hide one under it).
    const zoo::TileCoord out = zoo::Step(side);
    const zoo::Vec2 center = zoo::World::TileCenter(tile);
    const float half = 0.5f * zoo::kTileSize;
    const zoo::Vec2 toEdge{static_cast<float>(out.x), static_cast<float>(out.z)};
    const zoo::Vec2 along{-toEdge.z, toEdge.x};
    const auto point = [&](float across, float lengthwise) {
        const zoo::Vec2 p{center.x + toEdge.x * across + along.x * lengthwise,
                          center.z + toEdge.z * across + along.z * lengthwise};
        return Vector3(p.x, world.HeightAt(p) + kLift, p.z);
    };
    const float outer = half - 0.08f;
    const float inner = outer - width;
    // Both windings: seen from above whichever way the side faces.
    Layer(mark).Quad(point(inner, -half), point(outer, -half), point(outer, half), point(inner, half));
    Layer(mark).Quad(point(inner, half), point(outer, half), point(outer, -half), point(inner, -half));
}

void WorldTools::MarkRing(Mark mark, const zoo::World& world, zoo::Vec2 center, float radius, float width) {
    const int sides = std::clamp(static_cast<int>(radius * 6.0f), 16, 64);
    const float inner = std::max(0.0f, radius - width);
    const auto point = [&](int i, float r) {
        const float angle = 2.0f * kPi * static_cast<float>(i) / static_cast<float>(sides);
        const zoo::Vec2 p{center.x + std::sin(angle) * r, center.z + std::cos(angle) * r};
        return Vector3(p.x, world.HeightAt(p) + kLift, p.z);
    };
    for (int i = 0; i < sides; ++i)
        Layer(mark).Quad(point(i, inner), point(i, radius), point(i + 1, radius), point(i + 1, inner));
}

void WorldTools::UploadLayers() {
    for (LayerMesh& layer : layers_) {
        const auto& mesh = layer.writer.mesh;
        layer.shown = !mesh.indices.empty();
        if (!layer.shown)
            continue;
        Hasher h;
        h.Add(static_cast<int>(mesh.positions.size()));
        for (const Vector3& p : mesh.positions) {
            h.Add(p.X);
            h.Add(p.Y);
            h.Add(p.Z);
        }
        if (h.value != layer.hash) {
            layer.mesh = GpuMesh::Upload(device_, mesh, VertexLayout::PositionNormalTexture());
            layer.hash = h.value;
        }
    }
}

void WorldTools::ShowFences(const zoo::World& world, const std::vector<zoo::EdgeRef>& edges, int kind,
                            bool gate) {
    if (edges.empty())
        return;
    Hasher h;
    h.Add(kind);
    h.Add(gate ? 1 : 0);
    h.Add(static_cast<int>(world.Revision()));
    for (const zoo::EdgeRef& edge : edges) {
        h.Add(edge.x);
        h.Add(edge.z);
        h.Add(edge.horizontal ? 1 : 0);
    }
    fencesShown_ = true;
    if (h.value == fencesKey_)
        return;
    fencesKey_ = h.value;
    ModelBuilder model;
    FenceLook look;
    zoo::Fence fence;
    fence.kind = static_cast<std::uint8_t>(kind);
    fence.gate = gate;
    fence.condition = 1.0f;
    if (!LookOf(catalog_, fence, look)) {
        fences_.Clear();
        return;
    }
    std::vector<zoo::TileCoord> posts;
    for (const zoo::EdgeRef& edge : edges) {
        const zoo::TileCoord from{edge.x, edge.z};
        const zoo::TileCoord to =
            edge.horizontal ? zoo::TileCoord{edge.x + 1, edge.z} : zoo::TileCoord{edge.x, edge.z + 1};
        BuildFencePanel(model, look, VertexAt(world, from.x, from.z), VertexAt(world, to.x, to.z), gate,
                        static_cast<std::uint32_t>(edge.x * 73 + edge.z * 31 + (edge.horizontal ? 1 : 2)));
        for (const zoo::TileCoord post : {from, to}) {
            if (std::find(posts.begin(), posts.end(), post) == posts.end())
                posts.push_back(post);
        }
    }
    if (look.style != zoo::FenceStyle::Hedge) {
        for (const zoo::TileCoord post : posts)
            BuildFencePost(model, look, VertexAt(world, post.x, post.z), gate);
    }
    fences_.Build(device_, surfaces_, model);
}

void WorldTools::ShowGhost(std::uint64_t key, const Matrix& world, auto&& build) {
    if (key != ghostKey_ || ghost_.Empty()) {
        ModelBuilder model;
        build(model);
        ghost_.Build(device_, surfaces_, model);
        ghostKey_ = key;
    }
    ghostWorld_ = world;
    ghostShown_ = true;
}

void WorldTools::Update(Play& play, const Controls& controls, const CnaEngine::Graphics::Camera& camera,
                        Vector2 viewport, float seconds) {
    for (LayerMesh& layer : layers_)
        layer.writer = MeshWriter();
    fencesShown_ = false;
    ghostShown_ = false;
    play.hint.clear();
    play.hintBad = false;
    play.flashLeft = std::max(0.0f, play.flashLeft - seconds);

    const zoo::World& world = play.zoo->Map();
    Tool& tool = play.tool;

    // Putting the tool down: Escape, or a right click that was not a drag of the map.
    if (controls.Pressed(MouseKey::Right))
        rightTravel_ = 0.0f;
    rightTravel_ += std::abs(controls.Drag(MouseKey::Right).X) + std::abs(controls.Drag(MouseKey::Right).Y);
    const bool rightClick =
        controls.Released(MouseKey::Right) && rightTravel_ < 6.0f && controls.PointerFree();
    const bool escape = controls.KeyboardFree() && controls.Pressed(Keys::Escape);
    if (rightClick || escape) {
        if (dragging_) {
            dragging_ = false;
        } else if (escape && IsModal(play.dialog)) {
            play.dialog = Dialog::None;
        } else if (tool.kind != ToolKind::Inspect) {
            play.Use(ToolKind::Inspect);
            play.palette = Palette::None;
        } else if (play.dialog == Dialog::None && play.selection.kind != Picked::None) {
            play.selection.Clear();
            play.follow = false;
        } else if (escape && play.dialog == Dialog::None) {
            play.dialog = Dialog::Menu;
        } else if (escape) {
            play.dialog = Dialog::None;
        }
    }
    if (controls.KeyboardFree()) {
        // Turning what is about to be placed.
        const int turn = (controls.Pressed(Keys::X) ? 1 : 0) - (controls.Pressed(Keys::Z) ? 1 : 0);
        if (turn != 0) {
            tool.facing = zoo::RotatedClockwise(tool.facing, turn > 0 ? 1 : 3);
            tool.heading = zoo::WrapAngle(tool.heading + static_cast<float>(turn) * kPi * 0.25f);
        }
    }

    Pointer pointer;
    if (controls.PointerFree()) {
        pointer.origin = FromXna(play.camera->Position());
        pointer.direction = FromXna(play.camera->PixelDirection(camera, controls.Mouse(), viewport));
        if (const auto hit = world.Raycast(pointer.origin, pointer.direction, 3000.0f)) {
            pointer.onGround = true;
            pointer.ground = *hit;
            pointer.at = {hit->x, hit->z};
            pointer.tile = zoo::World::TileAt(pointer.at);
            pointer.distance = zoo::Length(*hit - pointer.origin);
        }
    } else if (!controls.Down(MouseKey::Left)) {
        dragging_ = false;
    }
    if (!controls.Down(MouseKey::Left)) {
        lastPainted_.reset();
        lastProp_.reset();
        lastBulldozed_ = 0;
    }

    switch (tool.kind) {
        case ToolKind::Inspect:
            Inspect(play, controls, pointer);
            break;
        case ToolKind::Path:
            Paths(play, controls, pointer);
            break;
        case ToolKind::Fence:
            Fences(play, controls, pointer);
            break;
        case ToolKind::Terrain:
            Terrain(play, controls, pointer);
            break;
        case ToolKind::Water:
            Water(play, controls, pointer);
            break;
        case ToolKind::Terraform:
            Terraform(play, controls, pointer, seconds);
            break;
        case ToolKind::Building:
            Buildings(play, controls, pointer);
            break;
        case ToolKind::Prop:
            Props(play, controls, pointer);
            break;
        case ToolKind::Animal:
        case ToolKind::MoveAnimal:
            Animals(play, controls, pointer);
            break;
        case ToolKind::Bulldoze:
            Bulldoze(play, controls, pointer);
            break;
    }
    ShowSelection(play);
    UploadLayers();
}

void WorldTools::Submit(CnaEngine::App::Frame& frame) const {
    const Matrix identity = Matrix::getIdentityProperty();
    for (std::size_t i = 0; i < layers_.size(); ++i) {
        if (layers_[i].shown)
            frame.Add(layers_[i].mesh, surfaces_.Marker(static_cast<Mark>(i)), identity);
    }
    if (fencesShown_)
        fences_.Submit(frame);
    if (ghostShown_)
        ghost_.Submit(frame, ghostWorld_);
}

void WorldTools::Inspect(Play& play, const Controls& controls, const Pointer& pointer) {
    if (!controls.PointerFree() || !controls.Pressed(MouseKey::Left))
        return;
    Selection picked;
    if (const zoo::AnimalId animal = PickAnimal(play, pointer)) {
        picked.kind = Picked::Animal;
        picked.animal = animal;
    } else if (pointer.onGround) {
        const zoo::World& world = play.zoo->Map();
        if (const zoo::PropId prop = play.zoo->Build().PropAt(pointer.at, 0.35f)) {
            picked.kind = Picked::Prop;
            picked.prop = prop;
        } else if (world.InBounds(pointer.tile) && world.At(pointer.tile).building) {
            picked.kind = Picked::Building;
            picked.building = world.At(pointer.tile).building;
        } else if (const zoo::EnclosureId enclosure = play.zoo->Habitats().At(pointer.tile)) {
            picked.kind = Picked::Enclosure;
            picked.enclosure = enclosure;
        }
    }
    play.Select(picked);
}

void WorldTools::ShowSelection(const Play& play) {
    const zoo::Zoo& zoo = *play.zoo;
    const zoo::World& world = zoo.Map();
    const Selection& s = play.selection;
    switch (s.kind) {
        case Picked::None:
            break;
        case Picked::Animal: {
            const zoo::Animal* animal = zoo.Fauna().Find(s.animal);
            const auto position = play.animals->PositionOf(s.animal);
            if (animal != nullptr && position) {
                const zoo::SpeciesDef& def = play.species->All()[static_cast<std::size_t>(animal->species)];
                MarkRing(Mark::Select, world, {position->X, position->Z},
                         std::max(0.6f, 0.65f * def.bodyLengthM), 0.14f);
            }
            break;
        }
        case Picked::Enclosure: {
            const zoo::Enclosure* enclosure = zoo.Habitats().Find(s.enclosure);
            if (enclosure == nullptr)
                break;
            // Its outline: every edge between a tile of it and a tile that is not.
            for (int z = enclosure->min.z; z <= enclosure->max.z; ++z) {
                for (int x = enclosure->min.x; x <= enclosure->max.x; ++x) {
                    const zoo::TileCoord tile{x, z};
                    if (zoo.Habitats().At(tile) != s.enclosure)
                        continue;
                    for (const zoo::Dir side :
                         {zoo::Dir::North, zoo::Dir::East, zoo::Dir::South, zoo::Dir::West}) {
                        if (zoo.Habitats().At(tile + zoo::Step(side)) != s.enclosure)
                            MarkBand(Mark::Select, world, tile, side, 0.45f);
                    }
                }
            }
            break;
        }
        case Picked::Building: {
            const zoo::Building* building = zoo.Objects().FindBuilding(s.building);
            if (building == nullptr)
                break;
            const zoo::BuildingDef& def = catalog_.buildings[static_cast<std::size_t>(building->def)];
            const zoo::TileCoord size = zoo::FootprintSize(def, building->facing);
            const zoo::TileCoord last{building->origin.x + size.x - 1, building->origin.z + size.z - 1};
            for (const zoo::EdgeRef& edge : zoo::EdgesAround(building->origin, last))
                MarkEdge(Mark::Select, world, edge, 0.5f);
            break;
        }
        case Picked::Prop: {
            const zoo::Prop* prop = zoo.Objects().FindProp(s.prop);
            if (prop == nullptr)
                break;
            const zoo::PropDef& def = catalog_.props[static_cast<std::size_t>(prop->def)];
            MarkRing(Mark::Select, world, prop->position, std::max(0.5f, def.radiusM * prop->scale + 0.2f),
                     0.14f);
            break;
        }
    }
}

void WorldTools::Paths(Play& play, const Controls& controls, const Pointer& pointer) {
    if (!pointer.onGround && !dragging_)
        return;
    zoo::Builder& builder = play.zoo->Build();
    const zoo::World& world = play.zoo->Map();
    const zoo::PathKind kind = play.tool.path;
    if (pointer.onGround && controls.Pressed(MouseKey::Left)) {
        dragging_ = true;
        anchorTile_ = pointer.tile;
    }
    if (!pointer.onGround)
        return;
    const std::vector<zoo::TileCoord> tiles =
        dragging_ ? zoo::TilesInLine(anchorTile_, pointer.tile) : std::vector<zoo::TileCoord>{pointer.tile};
    for (const zoo::TileCoord tile : tiles) {
        const zoo::BuildError error = builder.CheckPath(tile, kind);
        if (error == zoo::BuildError::None)
            MarkTile(kind == zoo::PathKind::None ? Mark::Bad : Mark::Good, world, tile);
        else if (error != zoo::BuildError::NothingToDo)
            MarkTile(Mark::Bad, world, tile);
        else
            MarkTile(Mark::Note, world, tile);
    }
    Explain(play, builder.PlanPath(tiles, kind), "tile", "tiles");
    if (dragging_ && controls.Released(MouseKey::Left)) {
        dragging_ = false;
        Done(play, builder.PlacePath(tiles, kind));
    }
}

void WorldTools::Fences(Play& play, const Controls& controls, const Pointer& pointer) {
    if (!pointer.onGround)
        return;
    zoo::Builder& builder = play.zoo->Build();
    const zoo::World& world = play.zoo->Map();
    const Tool& tool = play.tool;
    std::vector<zoo::EdgeRef> edges;
    int kind = tool.fence;
    if (tool.gate) {
        // A gate goes into the one edge the pointer is nearest to; into a
        // fence that stands there already, whatever it is made of.
        edges.push_back(NearestEdge(pointer.at));
        if (world.EdgeInBounds(edges[0]) && world.FenceAt(edges[0]).Exists())
            kind = world.FenceAt(edges[0]).kind;
    } else {
        const zoo::TileCoord vertex = NearestVertex(world, pointer.at);
        if (controls.Pressed(MouseKey::Left)) {
            dragging_ = true;
            anchorVertex_ = vertex;
            anchorTile_ = pointer.tile;
        }
        if (dragging_)
            edges = controls.Shift() ? zoo::EdgesAround(anchorTile_, pointer.tile)
                                     : zoo::EdgesAlong(anchorVertex_, vertex);
        if (!dragging_ || edges.empty()) {
            // Before the drag: a post where the fence would begin.
            MarkRing(Mark::Good, world,
                     {static_cast<float>(vertex.x) * kTile, static_cast<float>(vertex.z) * kTile}, 0.55f,
                     0.3f);
        }
    }
    std::vector<zoo::EdgeRef> good;
    for (const zoo::EdgeRef& edge : edges) {
        const zoo::BuildError error = builder.CheckFence(edge, kind, tool.gate);
        if (error == zoo::BuildError::None)
            good.push_back(edge);
        else if (error != zoo::BuildError::FenceExists)
            MarkEdge(Mark::Bad, world, edge);
    }
    ShowFences(world, good, kind, tool.gate);
    if (!edges.empty())
        Explain(play, builder.PlanFence(edges, kind, tool.gate), tool.gate ? "gate" : "panel",
                tool.gate ? "gates" : "panels");
    else
        play.hint = "Drag to build a fence; hold Shift to fence a rectangle";
    if (tool.gate) {
        if (controls.Pressed(MouseKey::Left))
            Done(play, builder.PlaceFence(edges, kind, true));
    } else if (dragging_ && controls.Released(MouseKey::Left)) {
        dragging_ = false;
        if (!edges.empty())
            Done(play, builder.PlaceFence(edges, kind, false));
    }
}

void WorldTools::Terrain(Play& play, const Controls& controls, const Pointer& pointer) {
    if (!pointer.onGround)
        return;
    zoo::Builder& builder = play.zoo->Build();
    const zoo::World& world = play.zoo->Map();
    const int reach = play.tool.brushTiles;
    const std::vector<zoo::TileCoord> tiles = zoo::TilesInRect(
        {pointer.tile.x - reach, pointer.tile.z - reach}, {pointer.tile.x + reach, pointer.tile.z + reach});
    for (const zoo::TileCoord tile : tiles)
        MarkTile(world.InBounds(tile) && world.IsProperty(tile) ? Mark::Note : Mark::Bad, world, tile);
    const zoo::BuildPlan plan = builder.PlanTerrain(tiles, play.tool.terrain);
    if (plan.error == zoo::BuildError::NothingToDo)
        play.hint = "Hold the button and paint";
    else
        Explain(play, plan, "tile", "tiles");
    if (controls.Down(MouseKey::Left) && (!lastPainted_ || !(*lastPainted_ == pointer.tile))) {
        lastPainted_ = pointer.tile;
        Done(play, builder.PaintTerrain(tiles, play.tool.terrain));
    }
}

void WorldTools::Water(Play& play, const Controls& controls, const Pointer& pointer) {
    if (!pointer.onGround)
        return;
    zoo::Builder& builder = play.zoo->Build();
    const zoo::World& world = play.zoo->Map();
    if (controls.Pressed(MouseKey::Left)) {
        dragging_ = true;
        anchorTile_ = pointer.tile;
    }
    const std::vector<zoo::TileCoord> tiles =
        zoo::TilesInRect(dragging_ ? anchorTile_ : pointer.tile, pointer.tile);
    const bool drain = play.tool.drain;
    for (const zoo::TileCoord tile : tiles) {
        const bool fine = drain ? world.InBounds(tile) && world.HasWater(tile)
                                : builder.CheckWater(tile) == zoo::BuildError::None;
        MarkTile(fine ? (drain ? Mark::Bad : Mark::Note) : (drain ? Mark::Note : Mark::Bad), world, tile);
    }
    Explain(play, drain ? builder.PlanRemoveWater(tiles) : builder.PlanWater(tiles, play.tool.waterDepth),
            "tile", "tiles");
    if (dragging_ && controls.Released(MouseKey::Left)) {
        dragging_ = false;
        Done(play, drain ? builder.RemoveWater(tiles) : builder.PlaceWater(tiles, play.tool.waterDepth));
    }
}

void WorldTools::Terraform(Play& play, const Controls& controls, const Pointer& pointer, float seconds) {
    if (!pointer.onGround)
        return;
    zoo::Builder& builder = play.zoo->Build();
    const zoo::World& world = play.zoo->Map();
    const Tool& tool = play.tool;
    MarkRing(Mark::Note, world, pointer.at, tool.radius, 0.3f);
    MarkRing(Mark::Note, world, pointer.at, 0.3f, 0.3f);
    float amount = 0.3f;
    if (tool.brush == zoo::Builder::Brush::Raise)
        amount = tool.lower ? -0.2f : 0.2f;
    else if (tool.brush == zoo::Builder::Brush::Smooth)
        amount = 0.5f;
    const zoo::BuildPlan plan = builder.PlanTerraform(tool.brush, pointer.at, tool.radius, amount);
    if (plan.Ok() || plan.error == zoo::BuildError::NothingToDo) {
        play.hint = plan.cost > 0 ? "Hold the button: " + zoo::FormatMoney(plan.cost) + " a stroke"
                                  : std::string("Hold the button to shape the ground");
    } else {
        play.hint = std::string(zoo::Describe(plan.error));
        play.hintBad = true;
    }
    terraformClock_ -= seconds;
    if (controls.Down(MouseKey::Left) && terraformClock_ <= 0.0f) {
        terraformClock_ = 0.07f;
        Done(play, builder.Terraform(tool.brush, pointer.at, tool.radius, amount));
    }
    if (!controls.Down(MouseKey::Left))
        terraformClock_ = 0.0f;
}

void WorldTools::Buildings(Play& play, const Controls& controls, const Pointer& pointer) {
    const Tool& tool = play.tool;
    if (!pointer.onGround || tool.building < 0 ||
        tool.building >= static_cast<int>(catalog_.buildings.size()))
        return;
    zoo::Builder& builder = play.zoo->Build();
    const zoo::World& world = play.zoo->Map();
    const zoo::BuildingDef& def = catalog_.buildings[static_cast<std::size_t>(tool.building)];
    const zoo::TileCoord size = zoo::FootprintSize(def, tool.facing);
    const zoo::TileCoord origin{
        static_cast<int>(std::lround(pointer.at.x / kTile - 0.5f * static_cast<float>(size.x))),
        static_cast<int>(std::lround(pointer.at.z / kTile - 0.5f * static_cast<float>(size.z)))};
    const zoo::BuildPlan plan = builder.PlanBuilding(tool.building, origin, tool.facing);
    const Mark mark = plan.Ok() ? Mark::Good : Mark::Bad;
    const zoo::TileCoord last{origin.x + size.x - 1, origin.z + size.z - 1};
    for (const zoo::TileCoord tile : zoo::TilesInRect(origin, last))
        MarkTile(mark, world, tile);
    // Where the door opens to.
    MarkTile(Mark::Note, world, zoo::FrontTile(def, origin, tool.facing));
    if (world.InBounds(origin) && world.InBounds(last)) {
        const Vector3 centre((static_cast<float>(origin.x) + 0.5f * static_cast<float>(size.x)) * kTile,
                             BuildingGround(world, origin, size),
                             (static_cast<float>(origin.z) + 0.5f * static_cast<float>(size.z)) * kTile);
        ShowGhost(0x1000u + static_cast<std::uint64_t>(tool.building),
                  Placement(centre, FacingAngle(tool.facing)),
                  [&](ModelBuilder& model) { BuildBuilding(model, def); });
    }
    if (plan.Ok())
        play.hint = def.name + ", " + zoo::FormatMoney(plan.cost) + "  ·  Z / X turn it";
    else
        Explain(play, plan, "building", "buildings");
    if (controls.Pressed(MouseKey::Left))
        Done(play, builder.PlaceBuilding(tool.building, origin, tool.facing));
}

void WorldTools::Props(Play& play, const Controls& controls, const Pointer& pointer) {
    const Tool& tool = play.tool;
    if (!pointer.onGround || tool.prop < 0 || tool.prop >= static_cast<int>(catalog_.props.size()))
        return;
    zoo::Builder& builder = play.zoo->Build();
    const zoo::World& world = play.zoo->Map();
    const zoo::PropDef& def = catalog_.props[static_cast<std::size_t>(tool.prop)];
    const zoo::BuildPlan plan = builder.PlanProp(tool.prop, pointer.at);
    MarkRing(plan.Ok() ? Mark::Good : Mark::Bad, world, pointer.at, std::max(def.radiusM, 0.35f), 0.15f);
    const bool natural =
        def.role == zoo::PropRole::Tree || def.role == zoo::PropRole::Bush || def.role == zoo::PropRole::Rock;
    ShowGhost(0x2000u + static_cast<std::uint64_t>(tool.prop),
              Placement(Vector3(pointer.at.x, world.HeightAt(pointer.at), pointer.at.z), tool.heading),
              [&](ModelBuilder& model) { BuildProp(model, def, 7u); });
    if (plan.Ok())
        play.hint = def.name + ", " + zoo::FormatMoney(plan.cost) +
                    (natural ? "  ·  hold the button to plant many" : "  ·  Z / X turn it");
    else
        Explain(play, plan, "item", "items");
    // One per click; plants and rocks also along a drag, at a natural spacing.
    const bool click = controls.Pressed(MouseKey::Left);
    const bool stroke = natural && controls.Down(MouseKey::Left) && lastProp_ &&
                        zoo::Distance(*lastProp_, pointer.at) > std::max(1.6f, def.radiusM * 3.0f);
    if (click || stroke) {
        float heading = tool.heading;
        float scale = 1.0f;
        if (natural) {
            // No two alike.
            scatter_ = scatter_ * 1664525u + 1013904223u;
            heading = static_cast<float>(scatter_ >> 8) / static_cast<float>(1u << 24) * 2.0f * kPi - kPi;
            scatter_ = scatter_ * 1664525u + 1013904223u;
            scale = 0.82f + static_cast<float>(scatter_ >> 8) / static_cast<float>(1u << 24) * 0.4f;
        }
        const zoo::BuildPlan placed = builder.PlaceProp(tool.prop, pointer.at, heading, scale);
        if (click)
            Done(play, placed);
        if (placed.Ok() || click)
            lastProp_ = pointer.at;
    }
}

void WorldTools::Animals(Play& play, const Controls& controls, const Pointer& pointer) {
    if (!pointer.onGround)
        return;
    zoo::Zoo& zoo = *play.zoo;
    const zoo::World& world = zoo.Map();
    Tool& tool = play.tool;
    const bool moving = tool.kind == ToolKind::MoveAnimal;
    const zoo::Animal* carried = moving ? zoo.Fauna().Find(tool.moving) : nullptr;
    if (moving && carried == nullptr) {
        play.Use(ToolKind::Inspect);
        return;
    }
    const int speciesIndex = moving ? carried->species : tool.species;
    if (speciesIndex < 0 || speciesIndex >= static_cast<int>(play.species->All().size()))
        return;
    const zoo::SpeciesDef& def = play.species->All()[static_cast<std::size_t>(speciesIndex)];
    const zoo::EnclosureId enclosure = zoo.Habitats().At(pointer.tile);
    const bool fine = static_cast<bool>(enclosure);
    MarkRing(fine ? Mark::Good : Mark::Bad, world, pointer.at, std::max(0.6f, 0.6f * def.bodyLengthM), 0.18f);
    if (!fine) {
        play.hint = "Animals are released inside an enclosure: fence one in first";
        play.hintBad = true;
    } else if (moving) {
        play.hint = "Release " + carried->name + " here";
    } else {
        play.hint = Format("Release a %s %s here, %s", tool.sex == zoo::Sex::Male ? "male" : "female",
                           def.commonName.c_str(), zoo::FormatMoney(zoo.PriceOf(speciesIndex)).c_str());
    }
    if (!controls.Pressed(MouseKey::Left))
        return;
    if (!fine) {
        play.Flash("Animals are released inside an enclosure");
        return;
    }
    if (moving) {
        const zoo::ActionError error = zoo.MoveAnimal(tool.moving, pointer.at);
        if (error != zoo::ActionError::None) {
            play.Flash(std::string(zoo::Describe(error)));
            return;
        }
        Selection selection;
        selection.kind = Picked::Animal;
        selection.animal = tool.moving;
        play.Select(selection);
        play.Use(ToolKind::Inspect);
        return;
    }
    const zoo::BuyResult bought = zoo.BuyAnimal(tool.species, tool.sex, pointer.at);
    if (!bought.Ok())
        play.Flash(std::string(zoo::Describe(bought.error)));
}

void WorldTools::Bulldoze(Play& play, const Controls& controls, const Pointer& pointer) {
    if (!pointer.onGround)
        return;
    zoo::Zoo& zoo = *play.zoo;
    zoo::Builder& builder = zoo.Build();
    const zoo::World& world = zoo.Map();
    const bool press = controls.Pressed(MouseKey::Left);
    const bool sweep = controls.Down(MouseKey::Left);
    // What the pointer is on, the smallest thing first.
    if (const zoo::PropId prop = builder.PropAt(pointer.at, 0.3f)) {
        const zoo::Prop* found = zoo.Objects().FindProp(prop);
        const zoo::PropDef& def = catalog_.props[static_cast<std::size_t>(found->def)];
        MarkRing(Mark::Bad, world, found->position, std::max(0.5f, def.radiusM * found->scale + 0.15f), 0.3f);
        Explain(play, builder.PlanRemoveProp(prop), def.name.c_str(), def.name.c_str());
        const std::uint64_t key = 0x100000000ull | prop.value;
        if (press || (sweep && key != lastBulldozed_)) {
            lastBulldozed_ = key;
            Done(play, builder.RemoveProp(prop));
        }
        return;
    }
    if (world.InBounds(pointer.tile) && world.At(pointer.tile).building) {
        const zoo::BuildingId id = world.At(pointer.tile).building;
        const zoo::Building* building = zoo.Objects().FindBuilding(id);
        const zoo::BuildingDef& def = catalog_.buildings[static_cast<std::size_t>(building->def)];
        const zoo::TileCoord size = zoo::FootprintSize(def, building->facing);
        for (const zoo::TileCoord tile : zoo::TilesInRect(
                 building->origin, {building->origin.x + size.x - 1, building->origin.z + size.z - 1}))
            MarkTile(Mark::Bad, world, tile);
        Explain(play, builder.PlanDemolish(id), def.name.c_str(), def.name.c_str());
        if (press)  // never by sweeping over it
            Done(play, builder.Demolish(id));
        return;
    }
    float toEdge = 0.0f;
    const zoo::EdgeRef edge = NearestEdge(pointer.at, &toEdge);
    const zoo::Fence fence = world.EdgeInBounds(edge) ? world.FenceAt(edge) : zoo::Fence{};
    if (fence.Exists() && toEdge < 0.55f) {
        MarkEdge(Mark::Bad, world, edge, 2.0f);
        const zoo::EdgeRef one[1] = {edge};
        Explain(play, builder.PlanRemoveFence(one), "fence panel", "fence panels");
        const std::uint64_t key = 0x200000000ull | (static_cast<std::uint64_t>(edge.x) << 17) |
                                  (static_cast<std::uint64_t>(edge.z) << 1) | (edge.horizontal ? 1u : 0u);
        if (press || (sweep && key != lastBulldozed_)) {
            lastBulldozed_ = key;
            Done(play, builder.RemoveFence(one));
        }
        return;
    }
    if (world.InBounds(pointer.tile) && world.At(pointer.tile).path != zoo::PathKind::None) {
        MarkTile(Mark::Bad, world, pointer.tile);
        const zoo::TileCoord one[1] = {pointer.tile};
        Explain(play, builder.PlanPath(one, zoo::PathKind::None), "path tile", "path tiles");
        const std::uint64_t key = 0x300000000ull | (static_cast<std::uint64_t>(pointer.tile.x) << 16) |
                                  static_cast<std::uint64_t>(pointer.tile.z);
        if (press || (sweep && key != lastBulldozed_)) {
            lastBulldozed_ = key;
            Done(play, builder.PlacePath(one, zoo::PathKind::None));
        }
        return;
    }
    play.hint = "Click a fence, a path, a building or an object to remove it";
}

}  // namespace zoogame
