// SPDX-License-Identifier: MIT
#include "BuildingModels.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "zoo/world/World.hpp"

namespace zoogame {

namespace {

constexpr float kPi = std::numbers::pi_v<float>;
constexpr float kTile = zoo::kTileSize;
/// Walls stand this far inside the footprint, so that eaves stay over the
/// building's own ground and neighbours do not touch.
constexpr float kInset = 0.3f;
/// How far the plinth stands out from the wall; everything fixed to a wall is
/// thicker than this.
constexpr float kPlinthOut = 0.06f;

const zoo::Color3 kStone{0.4f, 0.38f, 0.35f};
const zoo::Color3 kConcrete{0.55f, 0.54f, 0.5f};
const zoo::Color3 kTrim{0.8f, 0.78f, 0.72f};
const zoo::Color3 kPane{0.035f, 0.06f, 0.085f};
const zoo::Color3 kDark{0.012f, 0.011f, 0.01f};
const zoo::Color3 kTimber{0.27f, 0.18f, 0.11f};
const zoo::Color3 kDoorWood{0.2f, 0.12f, 0.07f};
const zoo::Color3 kSteel{0.36f, 0.38f, 0.4f};
const zoo::Color3 kBrick{0.42f, 0.2f, 0.13f};
const zoo::Color3 kGateGreen{0.07f, 0.2f, 0.12f};
const zoo::Color3 kCream{0.85f, 0.8f, 0.62f};

enum class Side { Front, Back, Right, Left };

/// A building under construction.
struct Shell {
    ModelBuilder& m;
    const zoo::BuildingDef& def;
    float W;      ///< footprint along x, metres
    float D;      ///< footprint along z
    float H;      ///< eaves height
    float wx;     ///< half extent of the walls along x
    float wz;     ///< half extent of the walls along z
    float doorX;  ///< centre of the door slot on the front

    /// Enters the frame of a wall: x along the wall (to the right for somebody
    /// looking at it from outside), y up from the ground, +z out of the wall.
    void Enter(Side side) const {
        switch (side) {
            case Side::Front:
                m.SetLocal(Matrix::CreateTranslation(0.0f, 0.0f, wz));
                break;
            case Side::Back:
                m.SetLocal(Matrix::CreateRotationY(kPi) * Matrix::CreateTranslation(0.0f, 0.0f, -wz));
                break;
            case Side::Right:
                m.SetLocal(Matrix::CreateRotationY(kPi * 0.5f) * Matrix::CreateTranslation(wx, 0.0f, 0.0f));
                break;
            case Side::Left:
                m.SetLocal(Matrix::CreateRotationY(-kPi * 0.5f) * Matrix::CreateTranslation(-wx, 0.0f, 0.0f));
                break;
        }
    }
    void Leave() const { m.ClearLocal(); }

    /// Tiles along a wall and the centre of slot `i` in the wall's frame.
    int Slots(Side side) const { return side == Side::Front || side == Side::Back ? def.width : def.depth; }
    float SlotX(Side side, int i) const {
        const float length = static_cast<float>(Slots(side)) * kTile;
        return -length * 0.5f + (static_cast<float>(i) + 0.5f) * kTile;
    }
};

// ---- things on a wall (in the wall's frame) ---------------------------------------------

void Window(const Shell& k, float x, float y0, float w, float h) {
    const float f = 0.07f;
    const float out = kPlinthOut + 0.03f;
    MeshWriter& trim = k.m.Use(kTrim, Pattern::Plain);
    trim.Box({x - w * 0.5f - f, y0 - f, 0.0f}, {x + w * 0.5f + f, y0, out + 0.04f});  // sill
    trim.Box({x - w * 0.5f - f, y0 + h, 0.0f}, {x + w * 0.5f + f, y0 + h + f, out});
    trim.Box({x - w * 0.5f - f, y0, 0.0f}, {x - w * 0.5f, y0 + h, out});
    trim.Box({x + w * 0.5f, y0, 0.0f}, {x + w * 0.5f + f, y0 + h, out});
    trim.Box({x - 0.02f, y0, 0.0f}, {x + 0.02f, y0 + h, out - 0.02f});
    if (h > 0.8f) {
        trim.Box({x - w * 0.5f, y0 + h * 0.55f - 0.02f, 0.0f},
                 {x + w * 0.5f, y0 + h * 0.55f + 0.02f, out - 0.02f});
    }
    const float z = 0.02f;
    k.m.Use(kPane, Pattern::Plain)
        .Quad({x - w * 0.5f, y0, z}, {x + w * 0.5f, y0, z}, {x + w * 0.5f, y0 + h, z},
              {x - w * 0.5f, y0 + h, z});
}

/// A dark hole in a wall with a frame around it: a doorway one cannot look
/// into, a serving hatch.
void Opening(const Shell& k, float x, float y0, float w, float h, const zoo::Color3& frameColor,
             Pattern frame) {
    const float z = kPlinthOut + 0.02f;
    k.m.Use(kDark, Pattern::Plain)
        .Quad({x - w * 0.5f, y0, z}, {x + w * 0.5f, y0, z}, {x + w * 0.5f, y0 + h, z},
              {x - w * 0.5f, y0 + h, z});
    const float f = 0.13f;
    const float out = kPlinthOut + 0.07f;
    MeshWriter& wood = k.m.Use(frameColor, frame);
    wood.Box({x - w * 0.5f - f, y0, 0.0f}, {x - w * 0.5f, y0 + h, out});
    wood.Box({x + w * 0.5f, y0, 0.0f}, {x + w * 0.5f + f, y0 + h, out});
    wood.Box({x - w * 0.5f - f, y0 + h, 0.0f}, {x + w * 0.5f + f, y0 + h + f, out});
    if (y0 > 0.2f) {
        wood.Box({x - w * 0.5f - f, y0 - f * 0.6f, 0.0f}, {x + w * 0.5f + f, y0, out});
    }
}

void Door(const Shell& k, float x, float w, float h, const zoo::Color3& color) {
    const float f = 0.08f;
    const float out = kPlinthOut + 0.06f;
    MeshWriter& trim = k.m.Use(kTrim, Pattern::Plain);
    trim.Box({x - w * 0.5f - f, 0.0f, 0.0f}, {x - w * 0.5f, h, out});
    trim.Box({x + w * 0.5f, 0.0f, 0.0f}, {x + w * 0.5f + f, h, out});
    trim.Box({x - w * 0.5f - f, h, 0.0f}, {x + w * 0.5f + f, h + f, out});
    k.m.Use(color, Pattern::Planks).Box({x - w * 0.5f, 0.05f, 0.0f}, {x + w * 0.5f, h, out - 0.03f});
    k.m.Use(kSteel, Pattern::Metal)
        .Box({x + w * 0.5f - 0.17f, 0.98f, out - 0.03f}, {x + w * 0.5f - 0.1f, 1.1f, out + 0.03f});
    k.m.Use(kConcrete, Pattern::Concrete)
        .Box({x - w * 0.5f - 0.25f, -0.6f, 0.0f}, {x + w * 0.5f + 0.25f, 0.06f, 0.55f});
}

/// A sign board: a coloured panel with a pale band.
void Sign(const Shell& k, float x, float y0, float w, float h, const zoo::Color3& color) {
    const float out = kPlinthOut + 0.04f;
    k.m.Use(color, Pattern::Plain).Box({x - w * 0.5f, y0, 0.0f}, {x + w * 0.5f, y0 + h, out});
    k.m.Use(kCream, Pattern::Plain)
        .Box({x - w * 0.36f, y0 + h * 0.3f, out}, {x + w * 0.36f, y0 + h * 0.7f, out + 0.012f});
}

// ---- shells and roofs -------------------------------------------------------------------

void Walls(const Shell& k, const zoo::Color3& color, Pattern pattern, float plinthTop) {
    k.m.Use(color, pattern).Box({-k.wx, plinthTop, -k.wz}, {k.wx, k.H, k.wz});
    k.m.Use(kStone, Pattern::Stone)
        .Box({-k.wx - kPlinthOut, -1.2f, -k.wz - kPlinthOut},
             {k.wx + kPlinthOut, plinthTop, k.wz + kPlinthOut});
}

/// A gable roof with its ridge along the longer side; returns the ridge height.
float Gable(const Shell& k, const zoo::Color3& color, Pattern pattern, float pitch, float overhang,
            const zoo::Color3& gableColor, Pattern gablePattern) {
    const bool alongX = k.wx >= k.wz;
    const float span = alongX ? k.wz : k.wx;
    const float ridge = k.H + span * pitch;
    const float eaves = k.H - overhang * pitch;
    k.m.Use(color, pattern)
        .GableRoof(-k.wx - overhang, k.wx + overhang, -k.wz - overhang, k.wz + overhang, eaves, ridge, alongX,
                   0.14f);
    k.m.Use(gableColor, gablePattern).GableEnds(-k.wx, k.wx, -k.wz, k.wz, k.H, ridge, alongX);
    // A ridge beam hides the seam.
    MeshWriter& cap = k.m.Use(Scaled(color, 0.7f), Pattern::Plain);
    if (alongX) {
        cap.Beam({-k.wx - overhang, ridge, 0.0f}, {k.wx + overhang, ridge, 0.0f}, 0.2f, 0.1f);
    } else {
        cap.Beam({0.0f, ridge, -k.wz - overhang}, {0.0f, ridge, k.wz + overhang}, 0.2f, 0.1f);
    }
    return ridge;
}

void Hip(const Shell& k, const zoo::Color3& color, Pattern pattern, float pitch, float overhang) {
    const float eaves = k.H - overhang * pitch;
    const float top = k.H + std::min(k.wx, k.wz) * pitch + 0.25f;
    k.m.Use(color, pattern)
        .HipRoof(-k.wx - overhang, k.wx + overhang, -k.wz - overhang, k.wz + overhang, eaves, top);
}

zoo::Color3 RoleColor(zoo::BuildingRole role) {
    switch (role) {
        case zoo::BuildingRole::FoodStall:
            return {0.6f, 0.08f, 0.05f};
        case zoo::BuildingRole::DrinkStall:
            return {0.06f, 0.25f, 0.55f};
        case zoo::BuildingRole::Restroom:
            return {0.05f, 0.16f, 0.45f};
        case zoo::BuildingRole::GiftShop:
            return {0.45f, 0.07f, 0.3f};
        case zoo::BuildingRole::StaffRoom:
            return {0.2f, 0.22f, 0.2f};
        case zoo::BuildingRole::VetClinic:
            return {0.75f, 0.75f, 0.73f};
        case zoo::BuildingRole::EducationCenter:
            return {0.07f, 0.28f, 0.12f};
        case zoo::BuildingRole::ResearchCenter:
            return {0.05f, 0.28f, 0.32f};
        case zoo::BuildingRole::AnimalShelter:
        case zoo::BuildingRole::Count:
            break;
    }
    return {0.25f, 0.25f, 0.25f};
}

// ---- the looks --------------------------------------------------------------------------

/// Plastered walls, a tiled gable roof, a door with a sign and a window in
/// every other bay: shops, clinics, offices, restrooms.
void House(const Shell& k) {
    const zoo::BuildingDef& def = k.def;
    Walls(k, def.wallColor, Pattern::Plaster, 0.35f);
    const float ridge =
        Gable(k, def.roofColor, Pattern::RoofTiles, 0.62f, 0.4f, def.wallColor, Pattern::Plaster);
    const bool shop = def.role == zoo::BuildingRole::GiftShop;
    const float winY = shop ? 0.75f : 1.0f;
    const float winW = shop ? 1.5f : 1.1f;
    const float winH = shop ? 1.5f : 1.15f;
    const float topH = std::min(winH, k.H - winY - 0.45f);

    k.Enter(Side::Front);
    for (int i = 0; i < k.Slots(Side::Front); ++i) {
        const float x = k.SlotX(Side::Front, i);
        if (i == def.doorOffset) {
            Door(k, x, 1.05f, 2.1f, kDoorWood);
            Sign(k, x, 2.28f, 1.5f, std::min(0.42f, k.H - 2.4f), RoleColor(def.role));
            if (def.role == zoo::BuildingRole::VetClinic) {
                // A red cross on the white board.
                const float out = kPlinthOut + 0.055f;
                MeshWriter& red = k.m.Use({0.6f, 0.03f, 0.03f}, Pattern::Plain);
                red.Box({x - 0.05f, 2.32f, out}, {x + 0.05f, 2.66f, out + 0.01f});
                red.Box({x - 0.17f, 2.44f, out}, {x + 0.17f, 2.54f, out + 0.01f});
            }
        } else {
            Window(k, x, winY, winW, topH);
        }
    }
    k.Leave();
    for (const Side side : {Side::Back, Side::Right, Side::Left}) {
        k.Enter(side);
        for (int i = 0; i < k.Slots(side); ++i) {
            // A blank bay now and then looks less like a barracks.
            if (side == Side::Back && k.Slots(side) > 3 && i % 2 == 1) {
                continue;
            }
            Window(k, k.SlotX(side, i), 1.0f, 1.1f, std::min(1.15f, k.H - 1.45f));
        }
        k.Leave();
    }
    // A chimney through the ridge.
    const bool alongX = k.wx >= k.wz;
    const float cx = alongX ? k.wx * 0.5f : 0.0f;
    const float cz = alongX ? 0.0f : -k.wz * 0.5f;
    k.m.Use(kBrick, Pattern::Brick).BoxAt(cx, cz, ridge - 0.5f, 0.5f, 1.05f, 0.5f);
    k.m.Use(kConcrete, Pattern::Concrete).BoxAt(cx, cz, ridge + 0.55f, 0.62f, 0.08f, 0.62f);
}

/// Boarded walls on a stone plinth, a wide dark doorway the animals walk
/// through, slit windows under the eaves.
void Barn(const Shell& k) {
    const zoo::BuildingDef& def = k.def;
    Walls(k, def.wallColor, Pattern::Planks, 0.5f);
    const float ridge =
        Gable(k, def.roofColor, Pattern::RoofTiles, 0.55f, 0.45f, def.wallColor, Pattern::Planks);
    const float slitY = k.H - 1.15f;

    k.Enter(Side::Front);
    for (int i = 0; i < k.Slots(Side::Front); ++i) {
        const float x = k.SlotX(Side::Front, i);
        if (i == def.doorOffset) {
            Opening(k, x, 0.0f, 1.7f, std::min(k.H - 0.6f, 3.0f), kTimber, Pattern::Wood);
        } else {
            Window(k, x, slitY, 0.95f, 0.5f);
        }
    }
    k.Leave();
    for (const Side side : {Side::Back, Side::Right, Side::Left}) {
        k.Enter(side);
        for (int i = 0; i < k.Slots(side); ++i) {
            Window(k, k.SlotX(side, i), slitY, 0.95f, 0.5f);
        }
        k.Leave();
    }
    // Corner boards.
    MeshWriter& timber = k.m.Use(kTimber, Pattern::Wood);
    for (const float sx : {-1.0f, 1.0f}) {
        for (const float sz : {-1.0f, 1.0f}) {
            timber.BoxAt(sx * k.wx, sz * k.wz, 0.5f, 0.2f, k.H - 0.5f, 0.2f);
        }
    }
    if (def.heated) {
        // The stove's flue.
        MeshWriter& steel = k.m.Use(kSteel, Pattern::Metal);
        const bool alongX = k.wx >= k.wz;
        const Vector3 base(alongX ? -k.wx * 0.5f : k.wx * 0.45f, ridge - 0.6f,
                           alongX ? -k.wz * 0.45f : k.wz * 0.5f);
        steel.Tube(base, base + Vector3(0.0f, 1.5f, 0.0f), 0.14f, 0.14f, 8);
        steel.Tube(base + Vector3(0.0f, 1.5f, 0.0f), base + Vector3(0.0f, 1.62f, 0.0f), 0.24f, 0.05f, 8);
    }
}

/// A small boarded shelter, open to the front, under thatch.
void Hut(const Shell& k) {
    Walls(k, k.def.wallColor, Pattern::Planks, 0.3f);
    Hip(k, k.def.roofColor, Pattern::Thatch, 0.8f, 0.5f);
    k.Enter(Side::Front);
    Opening(k, 0.0f, 0.0f, 2.0f * k.wx - 0.8f, std::min(k.H - 0.5f, 2.1f), kTimber, Pattern::Wood);
    k.Leave();
    MeshWriter& timber = k.m.Use(kTimber, Pattern::Wood);
    for (const float sx : {-1.0f, 1.0f}) {
        for (const float sz : {-1.0f, 1.0f}) {
            timber.BoxAt(sx * k.wx, sz * k.wz, 0.3f, 0.18f, k.H - 0.3f, 0.18f);
        }
    }
}

/// A stall: a serving hatch with a counter under a striped awning, a sign on
/// the flat roof, the staff door at the back.
void Kiosk(const Shell& k) {
    const zoo::BuildingDef& def = k.def;
    Walls(k, def.wallColor, Pattern::Plaster, 0.25f);
    k.m.Use(Scaled(def.roofColor, 0.55f), Pattern::Plain)
        .Box({-k.wx - 0.15f, k.H, -k.wz - 0.15f}, {k.wx + 0.15f, k.H + 0.18f, k.wz + 0.15f});

    k.Enter(Side::Front);
    const float hatch = 2.0f * k.wx - 0.5f;
    Opening(k, 0.0f, 1.02f, hatch, 1.1f, kSteel, Pattern::Metal);
    k.m.Use(kTimber, Pattern::Wood).Box({-k.wx + 0.05f, 0.93f, 0.0f}, {k.wx - 0.05f, 1.01f, 0.42f});
    // The awning in stripes of the stall's colour and cream.
    const float y0 = std::min(k.H - 0.25f, 2.65f);
    const int stripes = std::max(3, static_cast<int>(std::lround(2.0f * k.wx / 0.42f)) | 1);
    const float step = 2.0f * k.wx / static_cast<float>(stripes);
    for (int i = 0; i < stripes; ++i) {
        const float x0 = -k.wx + step * static_cast<float>(i);
        MeshWriter& cloth = k.m.Use(i % 2 == 0 ? def.roofColor : kCream, Pattern::Plain);
        cloth.QuadTwoSided({x0, y0 - 0.38f, 1.05f}, {x0 + step, y0 - 0.38f, 1.05f}, {x0 + step, y0, 0.02f},
                           {x0, y0, 0.02f});
        cloth.QuadTwoSided({x0, y0 - 0.52f, 1.05f}, {x0 + step, y0 - 0.52f, 1.05f},
                           {x0 + step, y0 - 0.38f, 1.05f}, {x0, y0 - 0.38f, 1.05f});
    }
    MeshWriter& steel = k.m.Use(kSteel, Pattern::Metal);
    for (const float sx : {-1.0f, 1.0f}) {
        steel.Tube({sx * (k.wx - 0.04f), y0 - 0.9f, 0.04f}, {sx * (k.wx - 0.04f), y0 - 0.4f, 1.03f}, 0.015f,
                   0.015f, 4, false, false);
    }
    // The sign on the roof.
    const float signW = 2.0f * k.wx * 0.85f;
    k.m.Use(def.roofColor, Pattern::Plain)
        .Box({-signW * 0.5f, k.H + 0.18f, -0.14f}, {signW * 0.5f, k.H + 0.95f, 0.0f});
    k.m.Use(kCream, Pattern::Plain)
        .Box({-signW * 0.38f, k.H + 0.4f, 0.0f}, {signW * 0.38f, k.H + 0.75f, 0.012f});
    k.Leave();

    k.Enter(Side::Back);
    Door(k, 0.0f, 0.85f, 2.0f, kSteel);
    k.Leave();
}

/// An open hall on a raised floor: posts, half-height side walls, a back wall
/// with display boards, a hipped roof. The education centre.
void Pavilion(const Shell& k) {
    const zoo::BuildingDef& def = k.def;
    const float floor = 0.3f;
    k.m.Use(kConcrete, Pattern::Concrete).Box({-k.wx, -1.2f, -k.wz}, {k.wx, floor, k.wz});
    k.m.Use(kConcrete, Pattern::Concrete)
        .Box({k.doorX - 1.3f, -0.6f, k.wz}, {k.doorX + 1.3f, floor * 0.5f, k.wz + 0.45f});
    MeshWriter& wall = k.m.Use(def.wallColor, Pattern::Plaster);
    wall.Box({-k.wx, floor, -k.wz}, {k.wx, k.H, -k.wz + 0.2f});
    wall.Box({-k.wx, floor, -k.wz}, {-k.wx + 0.16f, floor + 1.0f, k.wz});
    wall.Box({k.wx - 0.16f, floor, -k.wz}, {k.wx, floor + 1.0f, k.wz});

    MeshWriter& timber = k.m.Use(kTimber, Pattern::Wood);
    const float post = 0.22f;
    for (int i = 0; i <= def.width; ++i) {
        const float x =
            std::clamp(-k.W * 0.5f + static_cast<float>(i) * kTile, -k.wx + post * 0.5f, k.wx - post * 0.5f);
        timber.BoxAt(x, k.wz - post * 0.5f, floor, post, k.H - floor, post);
    }
    for (int i = 1; i < def.depth; ++i) {
        const float z = -k.D * 0.5f + static_cast<float>(i) * kTile;
        timber.BoxAt(-k.wx + post * 0.5f, z, floor, post, k.H - floor, post);
        timber.BoxAt(k.wx - post * 0.5f, z, floor, post, k.H - floor, post);
    }
    // The beam the roof rests on.
    timber.Box({-k.wx, k.H - 0.26f, k.wz - post}, {k.wx, k.H, k.wz});
    timber.Box({-k.wx, k.H - 0.26f, -k.wz + 0.2f}, {-k.wx + post, k.H, k.wz - post});
    timber.Box({k.wx - post, k.H - 0.26f, -k.wz + 0.2f}, {k.wx, k.H, k.wz - post});
    Hip(k, def.roofColor, Pattern::RoofTiles, 0.5f, 0.7f);

    // Display boards on the back wall, and two benches to read them from.
    const zoo::Color3 boards[3] = {{0.06f, 0.2f, 0.4f}, {0.07f, 0.28f, 0.12f}, {0.5f, 0.3f, 0.06f}};
    for (int i = 0; i < def.width; ++i) {
        const float x = -k.W * 0.5f + (static_cast<float>(i) + 0.5f) * kTile;
        const float z = -k.wz + 0.2f;
        k.m.Use(boards[i % 3], Pattern::Plain)
            .Box({x - 0.75f, floor + 0.9f, z}, {x + 0.75f, floor + 2.2f, z + 0.04f});
        k.m.Use(kCream, Pattern::Plain)
            .Box({x - 0.6f, floor + 1.05f, z + 0.04f}, {x + 0.6f, floor + 1.7f, z + 0.05f});
    }
    for (const float sx : {-1.0f, 1.0f}) {
        const float x = sx * k.wx * 0.45f;
        timber.Box({x - 0.9f, floor + 0.38f, -0.2f}, {x + 0.9f, floor + 0.45f, 0.22f});
        timber.Box({x - 0.8f, floor, -0.12f}, {x - 0.7f, floor + 0.38f, 0.14f});
        timber.Box({x + 0.7f, floor, -0.12f}, {x + 0.8f, floor + 0.38f, 0.14f});
    }
}

/// A den of piled rock with a dark mouth: cool in summer.
void Cave(const Shell& k) {
    MeshWriter& rock = k.m.Use(k.def.wallColor, Pattern::Stone);
    const float w = k.W * 0.5f;
    const float d = k.D * 0.5f;
    rock.Blob({0.0f, 0.0f, -d * 0.12f}, {w * 0.95f, k.H, d * 0.86f}, 811u, 0.2f, 14, 9, 0.3f);
    rock.Blob({-w * 0.55f, 0.0f, d * 0.2f}, {w * 0.45f, k.H * 0.55f, d * 0.6f}, 812u, 0.25f, 9, 6, 0.4f);
    rock.Blob({w * 0.6f, 0.0f, -d * 0.3f}, {w * 0.42f, k.H * 0.62f, d * 0.55f}, 813u, 0.25f, 9, 6, 0.4f);
    rock.Blob({w * 0.1f, k.H * 0.55f, -d * 0.35f}, {w * 0.5f, k.H * 0.6f, d * 0.5f}, 814u, 0.22f, 9, 6, 1.0f);
    // The mouth: a porch of three rocks around darkness.
    const float x = k.doorX;
    const float z = d - 0.55f;
    const float mouthW = 1.9f;
    const float mouthH = std::min(1.9f, k.H * 0.55f);
    rock.Blob({x - mouthW * 0.5f - 0.45f, 0.5f, z - 0.25f}, {0.75f, mouthH * 0.95f, 0.85f}, 815u, 0.22f, 8, 6,
              0.6f);
    rock.Blob({x + mouthW * 0.5f + 0.45f, 0.5f, z - 0.25f}, {0.75f, mouthH * 0.9f, 0.85f}, 816u, 0.22f, 8, 6,
              0.6f);
    rock.Blob({x, mouthH + 0.45f, z - 0.3f}, {mouthW * 0.5f + 1.0f, 0.75f, 0.95f}, 817u, 0.2f, 9, 5, 1.0f);
    // Whatever shows between the porch and the hill behind it is rock too.
    rock.Box({x - mouthW * 0.5f - 0.5f, -0.5f, -d * 0.2f},
             {x + mouthW * 0.5f + 0.5f, mouthH + 0.5f, z - 0.3f});
    k.m.Use(kDark, Pattern::Plain)
        .Quad({x - mouthW * 0.5f - 0.3f, -0.1f, z}, {x + mouthW * 0.5f + 0.3f, -0.1f, z},
              {x + mouthW * 0.5f + 0.3f, mouthH + 0.3f, z}, {x - mouthW * 0.5f - 0.3f, mouthH + 0.3f, z});
}

}  // namespace

void BuildBuilding(ModelBuilder& model, const zoo::BuildingDef& def) {
    Shell k{model, def, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    k.W = static_cast<float>(def.width) * kTile;
    k.D = static_cast<float>(def.depth) * kTile;
    k.H = std::max(def.heightM, 2.2f);
    k.wx = k.W * 0.5f - kInset;
    k.wz = k.D * 0.5f - kInset;
    k.doorX = -k.W * 0.5f + (static_cast<float>(std::clamp(def.doorOffset, 0, def.width - 1)) + 0.5f) * kTile;

    model.ClearLocal();
    if (def.look == "barn") {
        Barn(k);
    } else if (def.look == "hut") {
        Hut(k);
    } else if (def.look == "kiosk") {
        Kiosk(k);
    } else if (def.look == "pavilion") {
        Pavilion(k);
    } else if (def.look == "cave") {
        Cave(k);
    } else {
        House(k);
    }
    model.ClearLocal();
}

void BuildEntranceArch(ModelBuilder& model, float width) {
    const float half = width * 0.5f;
    const float pillar = 1.0f;
    const float px = half + pillar * 0.5f - 0.1f;
    const float top = 5.2f;
    MeshWriter& brick = model.Use(kBrick, Pattern::Brick);
    MeshWriter& cap = model.Use(kConcrete, Pattern::Concrete);
    for (const float s : {-1.0f, 1.0f}) {
        brick.BoxAt(s * px, 0.0f, -1.2f, pillar, top + 1.2f, pillar);
        cap.BoxAt(s * px, 0.0f, 0.0f, pillar + 0.16f, 0.5f, pillar + 0.16f);
        cap.BoxAt(s * px, 0.0f, top, pillar + 0.2f, 0.14f, pillar + 0.2f);
    }
    // The board between the pillars, the same on both faces.
    const float boardLo = 4.0f;
    model.Use(kGateGreen, Pattern::Plain).Box({-px, boardLo, -0.22f}, {px, top, 0.22f});
    MeshWriter& cream = model.Use(kCream, Pattern::Plain);
    cream.Box({-half * 0.72f, boardLo + 0.3f, 0.22f}, {half * 0.72f, top - 0.3f, 0.235f});
    cream.Box({-half * 0.72f, boardLo + 0.3f, -0.235f}, {half * 0.72f, top - 0.3f, -0.22f});
    // A tiled roof over all of it.
    const float eaves = top + 0.14f;
    const float ridge = eaves + 0.75f;
    model.Use({0.3f, 0.12f, 0.08f}, Pattern::RoofTiles)
        .GableRoof(-px - 0.9f, px + 0.9f, -1.0f, 1.0f, eaves, ridge, true, 0.12f);
    model.Use(kGateGreen, Pattern::Plain)
        .GableEnds(-px - 0.6f, px + 0.6f, -0.75f, 0.75f, eaves, ridge - 0.19f, true);

    // The leaves, open, along the inside of the gateway.
    MeshWriter& iron = model.Use(kGateGreen, Pattern::Metal);
    const float leaf = std::min(half - 0.15f, 3.2f);
    const float lo = 0.15f;
    const float hi = 2.7f;
    for (const float s : {-1.0f, 1.0f}) {
        const float x = s * (half - 0.14f);
        const float z0 = -pillar * 0.5f;
        const float z1 = z0 - leaf;
        iron.Beam({x, lo, z0}, {x, lo, z1}, 0.06f, 0.07f);
        iron.Beam({x, hi, z0}, {x, hi, z1}, 0.06f, 0.07f);
        iron.Beam({x, lo, z0}, {x, hi + 0.25f, z0}, 0.08f, 0.08f);
        iron.Beam({x, lo, z1}, {x, hi, z1}, 0.07f, 0.07f);
        const int bars = std::max(4, static_cast<int>(std::lround(leaf / 0.16f)));
        for (int i = 1; i < bars; ++i) {
            const float z = z0 + (z1 - z0) * static_cast<float>(i) / static_cast<float>(bars);
            iron.Tube({x, lo, z}, {x, hi + 0.18f, z}, 0.014f, 0.014f, 4, false, false);
        }
    }
}

}  // namespace zoogame
