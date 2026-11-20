// SPDX-License-Identifier: MIT
#include "SceneryRenderer.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <numbers>

#include "BuildingModels.hpp"
#include "FenceModels.hpp"
#include "GroundPaint.hpp"
#include "PlantModels.hpp"
#include "PropModels.hpp"
#include "zoo/sim/Zoo.hpp"

namespace zoogame {

namespace {

constexpr float kPi = std::numbers::pi_v<float>;
constexpr float kTile = zoo::kTileSize;

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

std::uint32_t Scramble(std::uint32_t a, std::uint32_t b, std::uint32_t c) {
    std::uint32_t h = a * 0x9e3779b1u ^ (b + 0x7f4a7c15u) * 0x85ebca6bu ^ (c + 0x165667b1u) * 0xc2b2ae35u;
    h ^= h >> 15;
    h *= 0x2c1b3c6du;
    h ^= h >> 12;
    return h;
}

Vector3 VertexAt(const zoo::World& world, int vx, int vz) {
    return Vector3(static_cast<float>(vx) * kTile, world.VertexHeight(vx, vz),
                   static_cast<float>(vz) * kTile);
}

bool IsEntrance(zoo::Fence fence) {
    return fence.kind == zoo::kPerimeterFence && fence.gate;
}

/// Where the public stands: a path or the planted strip before a barrier.
bool PublicSide(const zoo::World& world, zoo::TileCoord tile) {
    if (!world.InBounds(tile)) {
        return false;
    }
    const zoo::Tile& t = world.At(tile);
    return t.path != zoo::PathKind::None || t.standOff;
}

/// FenceLook::inward for the panel of `edge`: the overhang leans away from the public side,
/// 0 when both or neither side is public. Across(a, b) points to the tile north of a
/// horizontal edge and east of a vertical one.
int AnimalSide(const zoo::World& world, const zoo::EdgeRef& edge) {
    const zoo::TileCoord across = edge.horizontal ? zoo::TileCoord{edge.x, edge.z - 1} : zoo::TileCoord{edge.x, edge.z};
    const zoo::TileCoord behind = edge.horizontal ? zoo::TileCoord{edge.x, edge.z} : zoo::TileCoord{edge.x - 1, edge.z};
    const bool publicAcross = PublicSide(world, across);
    const bool publicBehind = PublicSide(world, behind);
    if (publicAcross == publicBehind) {
        return 0;
    }
    return publicBehind ? 1 : -1;
}

}  // namespace

struct SceneryRenderer::Cell {
    // Tiles [x0, x1) x [z0, z1). The cells along the far rims also own the fence
    // edges and posts on the rim itself.
    int x0 = 0, z0 = 0, x1 = 0, z1 = 0;
    bool lastX = false, lastZ = false;
    StaticBatch fences;
    StaticBatch objects;
    std::uint64_t fenceHash = 0, fenceWanted = 0;
    std::uint64_t objectHash = 0, objectWanted = 0;
    bool fencesBuilt = false, objectsBuilt = false;
};

struct SceneryRenderer::Grove {
    int x0 = 0, z0 = 0, x1 = 0, z1 = 0;  // tiles [x0, x1) x [z0, z1)
    float ground = 0.0f;                 // the height its plants stand at, on average
    StaticBatch near;
    StaticBatch far;
    std::uint64_t hash = 0, wanted = 0;
    bool built = false;
    bool showsFar = false;
};

struct SceneryRenderer::HoldingGate {
    Matrix placement;
    float closed = 0.0f;
    float slideDirection = 1.0f;
    bool active = false;
    bool wantsClosed = false;
};

SceneryRenderer::SceneryRenderer(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device,
                                 Surfaces& surfaces, const zoo::Catalog& catalog, int cellTiles)
    : device_(device),
      surfaces_(surfaces),
      catalog_(catalog),
      palette_(device),
      cellTiles_(std::max(cellTiles, 8)) {
    fullDetail_ = std::getenv("ZOO_PLANTS_FULL_DETAIL") != nullptr;
    ModelBuilder model;
    BuildHoldingGate(model);
    holdingGateModel_.Build(device_, surfaces_, model);
    ModelBuilder guides;
    BuildHoldingGateGuides(guides);
    holdingGateGuides_.Build(device_, surfaces_, guides);
}

SceneryRenderer::~SceneryRenderer() = default;

void SceneryRenderer::Layout(const zoo::World& world) {
    cells_.clear();
    holdingGates_.clear();
    width_ = world.Width();
    depth_ = world.Depth();
    for (int z = 0; z < depth_; z += cellTiles_) {
        for (int x = 0; x < width_; x += cellTiles_) {
            auto cell = std::make_unique<Cell>();
            cell->x0 = x;
            cell->z0 = z;
            cell->x1 = std::min(x + cellTiles_, width_);
            cell->z1 = std::min(z + cellTiles_, depth_);
            cell->lastX = cell->x1 == width_;
            cell->lastZ = cell->z1 == depth_;
            cells_.push_back(std::move(cell));
        }
    }
    groves_.clear();
    const int groveTiles = std::min(kGroveTiles, cellTiles_);
    for (int z = 0; z < depth_; z += groveTiles) {
        for (int x = 0; x < width_; x += groveTiles) {
            auto grove = std::make_unique<Grove>();
            grove->x0 = x;
            grove->z0 = z;
            grove->x1 = std::min(x + groveTiles, width_);
            grove->z1 = std::min(z + groveTiles, depth_);
            groves_.push_back(std::move(grove));
        }
    }
    worldRevision_ = 0;
    objectsRevision_ = 0;
}

void SceneryRenderer::UpdateHoldingGates(const zoo::Zoo& zoo, double seconds) {
    constexpr float kTravelSeconds = 1.2f;

    for (auto& [id, gate] : holdingGates_) {
        (void)id;
        gate.active = false;
        gate.wantsClosed = false;
    }

    for (const zoo::StaffMember& member : zoo.Keepers().All()) {
        if (!member.lockout) {
            continue;
        }
        const zoo::Enclosure* enclosure = zoo.Habitats().Find(member.lockout);
        if (enclosure == nullptr) {
            continue;
        }
        const zoo::StaffTask* task = member.Task();
        const bool releasing = member.working && task != nullptr &&
                               task->lockout == zoo::StaffLockoutAction::Release &&
                               task->enclosure == member.lockout;
        for (const zoo::BuildingId id : enclosure->shelters) {
            const zoo::Building* building = zoo.Objects().FindBuilding(id);
            if (building == nullptr || !building->open || building->def < 0 ||
                static_cast<std::size_t>(building->def) >= catalog_.buildings.size()) {
                continue;
            }
            const zoo::BuildingDef& def = catalog_.buildings[static_cast<std::size_t>(building->def)];
            if (def.role != zoo::BuildingRole::AnimalShelter) {
                continue;
            }
            const zoo::Vec2 front = zoo::World::TileCenter(
                zoo::FrontTile(def, building->origin, building->facing));
            bool assigned = false;
            bool housed = true;
            for (const zoo::Animal& animal : zoo.Fauna().All()) {
                if (!animal.InWorld() ||
                    (animal.home != member.lockout &&
                     zoo.Habitats().At(animal.position) != member.lockout)) {
                    continue;
                }
                if (animal.shelter == id) {
                    assigned = true;
                } else if (animal.activity == zoo::AnimalActivity::Shelter &&
                           zoo::DistanceSquared(animal.target, front) < 0.01f) {
                    assigned = true;
                    housed = false;
                }
            }

            const bool wantsClosed = assigned && housed && !releasing;
            auto [at, inserted] = holdingGates_.try_emplace(id);
            HoldingGate& gate = at->second;
            if (inserted) {
                // A visit may start from a warmed-up or loaded mid-lockout state. Match
                // that state immediately; ordinary live recalls still start raised.
                gate.closed = wantsClosed ? 1.0f : 0.0f;
                gate.slideDirection = def.doorOffset * 2 < def.width ? 1.0f : -1.0f;
                const zoo::TileCoord size = zoo::FootprintSize(def, building->facing);
                float ground = zoo.Map().VertexHeight(building->origin.x, building->origin.z);
                for (int z = 0; z <= size.z; ++z) {
                    for (int x = 0; x <= size.x; ++x) {
                        ground = std::max(
                            ground, zoo.Map().VertexHeight(building->origin.x + x, building->origin.z + z));
                    }
                }
                const zoo::Vec2 inside = zoo::World::TileCenter(
                    zoo::DoorTile(def, building->origin, building->facing));
                const zoo::Vec2 doorway = (inside + front) * 0.5f;
                gate.placement = Placement({doorway.x, ground, doorway.z}, FacingAngle(building->facing));
            }
            gate.active = true;
            gate.wantsClosed = gate.wantsClosed || wantsClosed;
        }
    }

    const float travel = static_cast<float>(std::max(seconds, 0.0)) / kTravelSeconds;
    for (auto at = holdingGates_.begin(); at != holdingGates_.end();) {
        HoldingGate& gate = at->second;
        gate.closed = std::clamp(gate.closed + (gate.wantsClosed ? travel : -travel), 0.0f, 1.0f);
        if (!gate.active && gate.closed <= 0.0f) {
            at = holdingGates_.erase(at);
        } else {
            ++at;
        }
    }
}

std::uint64_t SceneryRenderer::FenceHash(const zoo::World& world, const Cell& cell) const {
    Hasher h;
    // Posts on the cell's near rims look at the edges just outside it.
    for (int z = cell.z0 - 1; z <= cell.z1; ++z) {
        for (int x = cell.x0 - 1; x <= cell.x1; ++x) {
            for (const bool horizontal : {true, false}) {
                const zoo::EdgeRef edge{x, z, horizontal};
                if (!world.EdgeInBounds(edge)) {
                    continue;
                }
                const zoo::Fence fence = world.FenceAt(edge);
                if (fence.Exists()) {
                    h.Add(x);
                    h.Add(z);
                    h.Add(static_cast<int>(fence.kind) * 4 + (fence.gate ? 2 : 0) + (horizontal ? 1 : 0));
                    h.Add(world.VertexHeight(x, z));
                    h.Add(horizontal ? world.VertexHeight(x + 1, z) : world.VertexHeight(x, z + 1));
                }
            }
        }
    }
    return h.value;
}

std::uint64_t SceneryRenderer::ObjectHash(const zoo::World& world, const zoo::ZooObjects& objects,
                                          const Cell& cell) const {
    Hasher h;
    std::vector<zoo::PropId> ids;
    objects.PropsInTiles({cell.x0, cell.z0}, {cell.x1 - 1, cell.z1 - 1}, ids);
    for (const zoo::PropId id : ids) {
        const zoo::Prop* prop = objects.FindProp(id);
        if (prop == nullptr || PlantDef(prop) != nullptr) {
            continue;
        }
        h.Add(static_cast<int>(id.value));
        h.Add(prop->def);
        h.Add(prop->position.x);
        h.Add(prop->position.z);
        h.Add(prop->heading);
        h.Add(prop->scale);
        h.Add(DrawnGroundHeight(world, prop->position));
    }
    for (const zoo::Building& building : objects.Buildings()) {
        if (building.def < 0 || static_cast<std::size_t>(building.def) >= catalog_.buildings.size()) {
            continue;
        }
        const zoo::TileCoord size =
            zoo::FootprintSize(catalog_.buildings[static_cast<std::size_t>(building.def)], building.facing);
        const int cx = building.origin.x + size.x / 2;
        const int cz = building.origin.z + size.z / 2;
        if (cx < cell.x0 || cx >= cell.x1 || cz < cell.z0 || cz >= cell.z1) {
            continue;
        }
        h.Add(static_cast<int>(building.id.value));
        h.Add(building.def);
        h.Add(building.origin.x);
        h.Add(building.origin.z);
        h.Add(static_cast<int>(building.facing));
        h.Add(world.VertexHeight(building.origin.x, building.origin.z));
    }
    return h.value;
}

void SceneryRenderer::BuildFences(const zoo::World& world, Cell& cell) {
    ModelBuilder model;
    const int xEnd = cell.lastX ? cell.x1 + 1 : cell.x1;
    const int zEnd = cell.lastZ ? cell.z1 + 1 : cell.z1;

    // Panels.
    for (int z = cell.z0; z < zEnd; ++z) {
        for (int x = cell.x0; x < xEnd; ++x) {
            for (const bool horizontal : {true, false}) {
                const zoo::EdgeRef edge{x, z, horizontal};
                if (!world.EdgeInBounds(edge)) {
                    continue;
                }
                const zoo::Fence fence = world.FenceAt(edge);
                FenceLook look;
                if (!LookOf(catalog_, fence, look)) {
                    continue;
                }
                if (IsEntrance(fence)) {
                    // The entrance stands open under its arch, built once per gateway.
                    if (horizontal && !IsEntrance(world.FenceAt({x - 1, z, true}))) {
                        int run = 1;
                        while (world.EdgeInBounds({x + run, z, true}) &&
                               IsEntrance(world.FenceAt({x + run, z, true}))) {
                            ++run;
                        }
                        float ground = world.VertexHeight(x, z);
                        for (int i = 1; i <= run; ++i) {
                            ground = std::min(ground, world.VertexHeight(x + i, z));
                        }
                        const float width = static_cast<float>(run) * kTile;
                        model.SetTransform(
                            Matrix::CreateTranslation(static_cast<float>(x) * kTile + width * 0.5f, ground,
                                                      static_cast<float>(z) * kTile));
                        BuildEntranceArch(model, width);
                        model.ClearTransform();
                    }
                    continue;
                }
                const Vector3 a = VertexAt(world, x, z);
                const Vector3 b = horizontal ? VertexAt(world, x + 1, z) : VertexAt(world, x, z + 1);
                if (look.heavy || look.style == zoo::FenceStyle::Moat) {
                    look.inward = AnimalSide(world, edge);
                }
                BuildFencePanel(model, look, a, b, fence.gate,
                                Scramble(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(z),
                                         horizontal ? 1u : 2u));
            }
        }
    }

    // Posts, where fences meet a grid vertex.
    for (int vz = cell.z0; vz < zEnd; ++vz) {
        for (int vx = cell.x0; vx < xEnd; ++vx) {
            const zoo::EdgeRef around[4] = {
                {vx - 1, vz, true}, {vx, vz, true}, {vx, vz - 1, false}, {vx, vz, false}};
            FenceLook best;
            bool any = false;
            bool gatePost = false;
            bool entrance = false;
            for (const zoo::EdgeRef& edge : around) {
                if (!world.EdgeInBounds(edge)) {
                    continue;
                }
                const zoo::Fence fence = world.FenceAt(edge);
                FenceLook look;
                if (!LookOf(catalog_, fence, look) || look.style == zoo::FenceStyle::Hedge) {
                    continue;
                }
                entrance = entrance || IsEntrance(fence);
                gatePost = gatePost || fence.gate;
                if (!any || look.height > best.height) {
                    best = look;
                }
                any = true;
            }
            // The arch has pillars of its own, and no posts stand in the gateway.
            if (any && !entrance) {
                BuildFencePost(model, best, VertexAt(world, vx, vz), gatePost);
            }
        }
    }
    cell.fences.Build(device_, surfaces_, model);
}

void SceneryRenderer::BuildObjects(const zoo::World& world, const zoo::ZooObjects& objects, Cell& cell) {
    ModelBuilder model;
    std::vector<zoo::PropId> ids;
    objects.PropsInTiles({cell.x0, cell.z0}, {cell.x1 - 1, cell.z1 - 1}, ids);
    for (const zoo::PropId id : ids) {
        const zoo::Prop* prop = objects.FindProp(id);
        if (prop == nullptr || prop->def < 0 ||
            static_cast<std::size_t>(prop->def) >= catalog_.props.size() || PlantDef(prop) != nullptr) {
            continue;
        }
        const Vector3 position(prop->position.x, DrawnGroundHeight(world, prop->position), prop->position.z);
        model.SetTransform(Placement(position, prop->heading, prop->scale));
        BuildProp(model, catalog_.props[static_cast<std::size_t>(prop->def)], Scramble(id.value, 17u, 3u));
    }
    for (const zoo::Building& building : objects.Buildings()) {
        if (building.def < 0 || static_cast<std::size_t>(building.def) >= catalog_.buildings.size()) {
            continue;
        }
        const zoo::BuildingDef& def = catalog_.buildings[static_cast<std::size_t>(building.def)];
        const zoo::TileCoord size = zoo::FootprintSize(def, building.facing);
        const int cx = building.origin.x + size.x / 2;
        const int cz = building.origin.z + size.z / 2;
        if (cx < cell.x0 || cx >= cell.x1 || cz < cell.z0 || cz >= cell.z1) {
            continue;
        }
        // The builder levelled the footprint; the highest corner is the floor.
        float ground = world.VertexHeight(building.origin.x, building.origin.z);
        for (int z = 0; z <= size.z; ++z) {
            for (int x = 0; x <= size.x; ++x) {
                ground = std::max(ground, world.VertexHeight(building.origin.x + x, building.origin.z + z));
            }
        }
        const Vector3 centre(
            (static_cast<float>(building.origin.x) + static_cast<float>(size.x) * 0.5f) * kTile, ground,
            (static_cast<float>(building.origin.z) + static_cast<float>(size.z) * 0.5f) * kTile);
        model.SetTransform(Placement(centre, FacingAngle(building.facing)));
        BuildBuilding(model, def);
    }
    model.ClearTransform();
    cell.objects.Build(device_, surfaces_, model);
}

const zoo::PropDef* SceneryRenderer::PlantDef(const zoo::Prop* prop) const {
    if (prop == nullptr || prop->def < 0 || static_cast<std::size_t>(prop->def) >= catalog_.props.size()) {
        return nullptr;
    }
    const zoo::PropDef& def = catalog_.props[static_cast<std::size_t>(prop->def)];
    return IsPlant(def) ? &def : nullptr;
}

std::uint64_t SceneryRenderer::GroveHash(const zoo::World& world, const zoo::ZooObjects& objects,
                                         const Grove& grove) const {
    Hasher h;
    std::vector<zoo::PropId> ids;
    objects.PropsInTiles({grove.x0, grove.z0}, {grove.x1 - 1, grove.z1 - 1}, ids);
    for (const zoo::PropId id : ids) {
        const zoo::Prop* prop = objects.FindProp(id);
        if (PlantDef(prop) == nullptr) {
            continue;
        }
        h.Add(static_cast<int>(id.value));
        h.Add(prop->def);
        h.Add(prop->position.x);
        h.Add(prop->position.z);
        h.Add(prop->heading);
        h.Add(prop->scale);
        h.Add(DrawnGroundHeight(world, prop->position));
    }
    return h.value;
}

void SceneryRenderer::BuildGrove(const zoo::World& world, const zoo::ZooObjects& objects, Grove& grove) {
    ModelBuilder near, far;
    std::vector<zoo::PropId> ids;
    objects.PropsInTiles({grove.x0, grove.z0}, {grove.x1 - 1, grove.z1 - 1}, ids);
    float heights = 0.0f;
    int count = 0;
    for (const zoo::PropId id : ids) {
        const zoo::Prop* prop = objects.FindProp(id);
        const zoo::PropDef* def = PlantDef(prop);
        if (def == nullptr) {
            continue;
        }
        const Vector3 position(prop->position.x, DrawnGroundHeight(world, prop->position), prop->position.z);
        const Matrix placement = Placement(position, prop->heading, prop->scale);
        const std::uint32_t seed = Scramble(id.value, 17u, 3u);
        near.SetTransform(placement);
        BuildPlant(near, *def, seed, PlantDetail::Near);
        if (!fullDetail_) {
            far.SetTransform(placement);
            BuildPlant(far, *def, seed, PlantDetail::Far);
        }
        heights += position.Y;
        ++count;
    }
    grove.ground = count > 0 ? heights / static_cast<float>(count) : 0.0f;
    grove.near.Build(device_, surfaces_, near);
    // The comparison mode never submits the far form; avoid making an unused second zoo.
    grove.far.Clear();
    if (fullDetail_) {
        return;
    }
    // The far form in the palette's two materials, whatever grows here.
    CnaEngine::Graphics::MeshData solid, cards;
    palette_.Merge(far, solid, cards);
    grove.far.Add(device_, solid, palette_.Solid());
    grove.far.Add(device_, cards, palette_.Cards());
}

void SceneryRenderer::Update(const zoo::World& world, const zoo::ZooObjects& objects, int maxCells) {
    if (editCacheDiscarded_)
        return;
    if (cells_.empty() || world.Width() != width_ || world.Depth() != depth_) {
        Layout(world);
    }
    const bool worldChanged = world.Revision() != worldRevision_;
    const bool objectsChanged = objects.Revision() != objectsRevision_;
    if (worldChanged || objectsChanged) {
        for (auto& cell : cells_) {
            if (worldChanged) {
                cell->fenceWanted = FenceHash(world, *cell);
            }
            cell->objectWanted = ObjectHash(world, objects, *cell);
        }
        for (auto& grove : groves_) {
            grove->wanted = GroveHash(world, objects, *grove);
        }
        worldRevision_ = world.Revision();
        objectsRevision_ = objects.Revision();
        pending_ = true;
    }
    if (!pending_) {
        return;
    }
    int budget = maxCells;
    pending_ = false;
    for (auto& cell : cells_) {
        const bool fences = !cell->fencesBuilt || cell->fenceHash != cell->fenceWanted;
        const bool things = !cell->objectsBuilt || cell->objectHash != cell->objectWanted;
        if (!fences && !things) {
            continue;
        }
        if (maxCells >= 0 && budget <= 0) {
            pending_ = true;
            break;
        }
        if (fences) {
            BuildFences(world, *cell);
            cell->fenceHash = cell->fenceWanted;
            cell->fencesBuilt = true;
        }
        if (things) {
            BuildObjects(world, objects, *cell);
            cell->objectHash = cell->objectWanted;
            cell->objectsBuilt = true;
        }
        --budget;
    }
    for (auto& grove : groves_) {
        if (grove->built && grove->hash == grove->wanted) {
            continue;
        }
        if (maxCells >= 0 && budget <= 0) {
            pending_ = true;
            break;
        }
        BuildGrove(world, objects, *grove);
        grove->hash = grove->wanted;
        grove->built = true;
        --budget;
    }
    palette_.Upload();
}

void SceneryRenderer::DiscardEditCache(bool discardCpuTextureCopies) {
    palette_.DiscardEditCache(discardCpuTextureCopies);
    editCacheDiscarded_ = true;
}

void SceneryRenderer::Submit(CnaEngine::App::Frame& frame, const Vector3& eye) {
    if (fullDetail_) {
        static_cast<const SceneryRenderer&>(*this).Submit(frame);
        return;
    }
    for (const auto& cell : cells_) {
        cell->fences.Submit(frame);
        cell->objects.Submit(frame);
    }
    for (const auto& grove : groves_) {
        if (grove->near.Empty()) {
            continue;
        }
        // From the eye to the nearest point of the grove's square on the ground.
        const float dx = std::max({static_cast<float>(grove->x0) * kTile - eye.X, 0.0f,
                                   eye.X - static_cast<float>(grove->x1) * kTile});
        const float dz = std::max({static_cast<float>(grove->z0) * kTile - eye.Z, 0.0f,
                                   eye.Z - static_cast<float>(grove->z1) * kTile});
        const float dy = eye.Y - grove->ground;
        const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (distance > kFarBeyond) {
            grove->showsFar = true;
        } else if (distance < kNearWithin) {
            grove->showsFar = false;
        }
        (grove->showsFar ? grove->far : grove->near).Submit(frame);
    }
    constexpr float kSlide = 1.75f;
    constexpr float kRailOffset = 0.85f;
    for (const auto& [id, gate] : holdingGates_) {
        (void)id;
        holdingGateGuides_.Submit(
            frame, Matrix::CreateTranslation(kRailOffset * gate.slideDirection, 0.0f, 0.0f) *
                       gate.placement);
        holdingGateModel_.Submit(
            frame, Matrix::CreateTranslation((1.0f - gate.closed) * kSlide * gate.slideDirection,
                                             0.0f, 0.0f) *
                       gate.placement);
    }
}

void SceneryRenderer::Submit(CnaEngine::App::Frame& frame) const {
    for (const auto& cell : cells_) {
        cell->fences.Submit(frame);
        cell->objects.Submit(frame);
    }
    for (const auto& grove : groves_) {
        grove->near.Submit(frame);
    }
    constexpr float kSlide = 1.75f;
    constexpr float kRailOffset = 0.85f;
    for (const auto& [id, gate] : holdingGates_) {
        (void)id;
        holdingGateGuides_.Submit(
            frame, Matrix::CreateTranslation(kRailOffset * gate.slideDirection, 0.0f, 0.0f) *
                       gate.placement);
        holdingGateModel_.Submit(
            frame, Matrix::CreateTranslation((1.0f - gate.closed) * kSlide * gate.slideDirection,
                                             0.0f, 0.0f) *
                       gate.placement);
    }
}

std::size_t SceneryRenderer::Triangles() const {
    std::size_t total = 0;
    for (const auto& cell : cells_) {
        total += cell->fences.Triangles() + cell->objects.Triangles();
    }
    for (const auto& grove : groves_) {
        total += grove->near.Triangles();
    }
    total += holdingGates_.size() *
             (holdingGateModel_.Triangles() + holdingGateGuides_.Triangles());
    return total;
}

std::string SceneryRenderer::Tally(const zoo::ZooObjects& objects) const {
    struct Line {
        std::string name;
        std::size_t each = 0;
        std::size_t count = 0;
        std::size_t far = 0;  // a plant's triangles in the far form
    };
    const auto triangles = [](const ModelBuilder& model) {
        std::size_t total = 0;
        for (const ModelPart& part : model.Parts()) {
            total += part.writer.mesh.TriangleCount();
        }
        return total;
    };
    std::vector<Line> lines;
    Line fences{"(all fences)", 0, 1, 0};
    for (const auto& cell : cells_) {
        fences.each += cell->fences.Triangles();
    }
    lines.push_back(fences);
    std::vector<std::size_t> props(catalog_.props.size(), 0);
    for (const zoo::Prop& prop : objects.Props()) {
        if (prop.def >= 0 && static_cast<std::size_t>(prop.def) < props.size()) {
            ++props[static_cast<std::size_t>(prop.def)];
        }
    }
    for (std::size_t i = 0; i < props.size(); ++i) {
        if (props[i] == 0) {
            continue;
        }
        // One of them stands for all: the seed changes a tree's shape, hardly its size.
        ModelBuilder model, far;
        BuildProp(model, catalog_.props[i], 1u);
        BuildPlant(far, catalog_.props[i], 1u, PlantDetail::Far);
        lines.push_back({catalog_.props[i].id, triangles(model), props[i], triangles(far)});
    }
    std::vector<std::size_t> buildings(catalog_.buildings.size(), 0);
    for (const zoo::Building& building : objects.Buildings()) {
        if (building.def >= 0 && static_cast<std::size_t>(building.def) < buildings.size()) {
            ++buildings[static_cast<std::size_t>(building.def)];
        }
    }
    for (std::size_t i = 0; i < buildings.size(); ++i) {
        if (buildings[i] == 0) {
            continue;
        }
        ModelBuilder model;
        BuildBuilding(model, catalog_.buildings[i]);
        lines.push_back({catalog_.buildings[i].id, triangles(model), buildings[i], 0});
    }
    std::sort(lines.begin(), lines.end(),
              [](const Line& a, const Line& b) { return a.each * a.count > b.each * b.count; });
    std::string text = "  draw calls a cell (fences+objects):";
    for (const auto& cell : cells_) {
        text += " " + std::to_string(cell->fences.Pieces()) + "+" + std::to_string(cell->objects.Pieces());
    }
    text += "\n";
    std::size_t planted = 0, pieces = 0, near = 0, far = 0;
    for (const auto& grove : groves_) {
        planted += grove->near.Empty() ? 0 : 1;
        pieces += grove->near.Pieces();
        near += grove->near.Triangles();
        far += grove->far.Triangles();
    }
    text += "  plants: " + std::to_string(planted) + " groves of " + std::to_string(groves_.size()) +
            " planted, " + std::to_string(pieces) + " draw calls and " + std::to_string(near) +
            " triangles all near, " + std::to_string(far) + " triangles all far (two draw calls a grove)\n";
    for (const Line& line : lines) {
        char row[200];
        if (line.far > 0) {
            std::snprintf(row, sizeof row, "  %-24s %5zu x %6zu = %8zu  (far %zu each)\n", line.name.c_str(),
                          line.count, line.each, line.each * line.count, line.far);
        } else {
            std::snprintf(row, sizeof row, "  %-24s %5zu x %6zu = %8zu\n", line.name.c_str(), line.count,
                          line.each, line.each * line.count);
        }
        text += row;
    }
    return text;
}

std::size_t SceneryRenderer::DrawCalls() const {
    std::size_t total = 0;
    for (const auto& cell : cells_) {
        total += cell->fences.Pieces() + cell->objects.Pieces();
    }
    for (const auto& grove : groves_) {
        total += grove->near.Pieces();
    }
    total += holdingGates_.size() * (holdingGateModel_.Pieces() + holdingGateGuides_.Pieces());
    return total;
}

}  // namespace zoogame
