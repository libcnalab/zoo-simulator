// SPDX-License-Identifier: MIT
#include "BuildingModels.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "RockModels.hpp"
#include "SignTextures.hpp"
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
const zoo::Color3 kStraw{0.66f, 0.55f, 0.24f};

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
    // Somebody has pulled a blind half down in one window out of three, so that a row of
    // them is not a row of the same hole.
    const int which = static_cast<int>(std::floor(x * 1.7f + k.wx * 3.1f + 40.0f)) % 3;
    if (which == 0 && h > 0.8f) {
        const float drop =
            h * (0.25f + 0.1f * static_cast<float>(static_cast<int>(std::floor(x + 40.0f)) % 3));
        k.m.Use(kCream, Pattern::Plain)
            .Quad({x - w * 0.5f, y0 + h - drop, z + 0.01f}, {x + w * 0.5f, y0 + h - drop, z + 0.01f},
                  {x + w * 0.5f, y0 + h, z + 0.01f}, {x - w * 0.5f, y0 + h, z + 0.01f});
    }
}

/// A printed sign on a wall: its middle at x, its foot at y0, `w` wide, on a board of the
/// building's colour that stands out from the wall.
void NameSign(const Shell& k, Sign sign, float x, float y0, float w, const zoo::Color3& board) {
    const float h = w / SignAspect(sign);
    const float out = kPlinthOut + 0.05f;
    const float face = out + 0.004f;
    k.m.Use(board, Pattern::Plain)
        .Box({x - w * 0.5f - 0.05f, y0 - 0.05f, 0.0f}, {x + w * 0.5f + 0.05f, y0 + h + 0.05f, out});
    WriteSign(k.m, sign, {x - w * 0.5f, y0, face}, {x + w * 0.5f, y0, face}, {x + w * 0.5f, y0 + h, face},
              {x - w * 0.5f, y0 + h, face});
}

/// A lantern on a bracket over a door.
void Lantern(const Shell& k, float x, float y) {
    MeshWriter& iron = k.m.Use(kSteel, Pattern::Metal);
    iron.Box({x - 0.02f, y + 0.2f, 0.0f}, {x + 0.02f, y + 0.24f, 0.26f});
    iron.Box({x - 0.09f, y + 0.16f, 0.14f}, {x + 0.09f, y + 0.2f, 0.32f});
    k.m.Use(kCream, Pattern::Plain).Box({x - 0.06f, y, 0.17f}, {x + 0.06f, y + 0.16f, 0.29f});
}

/// The building's name as its board prints it; false for a building without one.
bool NameOf(const zoo::BuildingDef& def, Sign& sign) {
    if (def.id == "ice_cream_stand") {
        sign = Sign::IceCream;
        return true;
    }
    switch (def.role) {
        case zoo::BuildingRole::FoodStall:
            sign = Sign::Burgers;
            return true;
        case zoo::BuildingRole::DrinkStall:
            sign = Sign::Drinks;
            return true;
        case zoo::BuildingRole::Restroom:
            sign = Sign::Toilets;
            return true;
        case zoo::BuildingRole::GiftShop:
            sign = Sign::GiftShop;
            return true;
        case zoo::BuildingRole::StaffRoom:
            sign = Sign::Staff;
            return true;
        case zoo::BuildingRole::VetClinic:
            sign = Sign::Vet;
            return true;
        case zoo::BuildingRole::EducationCenter:
            sign = Sign::Education;
            return true;
        case zoo::BuildingRole::ResearchCenter:
            sign = Sign::Research;
            return true;
        case zoo::BuildingRole::AnimalShelter:
        case zoo::BuildingRole::Count:
            break;
    }
    return false;
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

/// A board without lettering: a coloured panel with a pale band.
void Board(const Shell& k, float x, float y0, float w, float h, const zoo::Color3& color) {
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
    // Along the ridge `a`, across it `c`: the same carpentry whichever way the roof runs.
    const float length = (alongX ? k.wx : k.wz) + overhang;
    const auto at = [alongX](float a, float y, float c) {
        return alongX ? Vector3(a, y, c) : Vector3(c, y, a);
    };
    MeshWriter& gutter = k.m.Use(kSteel, Pattern::Metal);
    for (const float s : {-1.0f, 1.0f}) {
        // Barge boards close the roof's edge over the gables.
        for (const float end : {-1.0f, 1.0f}) {
            cap.Beam(at(end * length, eaves - 0.05f, s * (span + overhang)),
                     at(end * length, ridge - 0.02f, 0.0f), 0.045f, 0.2f);
        }
        // A gutter under each eaves, and a pipe from it back to the wall and down to the ground.
        const float edge = s * (span + overhang + 0.06f);
        gutter.Beam(at(-length, eaves - 0.07f, edge), at(length, eaves - 0.07f, edge), 0.12f, 0.08f);
        const float pipe = s * (length - overhang - 0.14f);
        const float wall = s * (span + kPlinthOut + 0.05f);
        gutter.Tube(at(pipe, eaves - 0.1f, edge), at(pipe, eaves - 0.1f - overhang * 0.8f, wall), 0.035f,
                    0.035f, 6, false, false);
        gutter.Tube(at(pipe, eaves - 0.1f - overhang * 0.8f, wall), at(pipe, 0.12f, wall), 0.035f, 0.035f, 6,
                    false, false);
    }
    return ridge;
}

/// Straw trodden out of a doorway.
void StrawAt(const Shell& k, float x, float reach) {
    k.m.Use(kStraw, Pattern::Thatch)
        .Blob({x, -0.06f, k.wz + reach * 0.5f}, {reach * 1.2f, 0.17f, reach}, 31u, 0.25f, 8, 4, 0.2f);
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
    // A toilet block has its windows small and high.
    const bool closet = def.role == zoo::BuildingRole::Restroom;
    const float winY = shop ? 0.75f : closet ? 1.6f : 1.0f;
    const float winW = shop ? 1.5f : closet ? 0.8f : 1.1f;
    const float winH = shop ? 1.5f : closet ? 0.55f : 1.15f;
    const float topH = std::min(winH, k.H - winY - 0.45f);
    Sign name = Sign::Staff;
    const bool named = NameOf(def, name);

    k.Enter(Side::Front);
    for (int i = 0; i < k.Slots(Side::Front); ++i) {
        const float x = k.SlotX(Side::Front, i);
        if (i == def.doorOffset) {
            Door(k, x, 1.05f, 2.1f, kDoorWood);
            const float signH = std::min(0.42f, k.H - 2.45f);
            if (named) {
                NameSign(k, name, x, 2.3f, std::min(1.9f, signH * SignAspect(name)), RoleColor(def.role));
            } else {
                Board(k, x, 2.28f, 1.5f, signH, RoleColor(def.role));
            }
            Lantern(k, x - 0.85f, 1.95f);
            if (def.role == zoo::BuildingRole::VetClinic) {
                // A red cross on a white plate beside the door.
                const float out = kPlinthOut + 0.02f;
                k.m.Use(kTrim, Pattern::Plain).Box({x + 0.72f, 1.35f, 0.0f}, {x + 1.12f, 1.75f, out});
                MeshWriter& red = k.m.Use({0.6f, 0.03f, 0.03f}, Pattern::Plain);
                red.Box({x + 0.88f, 1.4f, out}, {x + 0.96f, 1.7f, out + 0.01f});
                red.Box({x + 0.77f, 1.51f, out}, {x + 1.07f, 1.59f, out + 0.01f});
            }
        } else {
            Window(k, x, winY, winW, topH);
            if (shop) {
                // What the shop sells, on a shelf behind the glass.
                const float z0 = 0.025f;
                k.m.Use(kTimber, Pattern::Wood)
                    .Box({x - winW * 0.5f, winY + 0.42f, z0}, {x + winW * 0.5f, winY + 0.46f, z0 + 0.03f});
                for (int item = 0; item < 5; ++item) {
                    const float ix = x - winW * 0.5f + 0.18f + 0.28f * static_cast<float>(item);
                    const float tall = 0.16f + 0.07f * static_cast<float>((item * 7 + i * 3) % 4);
                    MeshWriter& toy = k.m.Use(item % 3 == 0   ? RoleColor(def.role)
                                              : item % 3 == 1 ? kCream
                                                              : kStraw,
                                              Pattern::Plain);
                    toy.Box({ix - 0.08f, winY + 0.46f, z0}, {ix + 0.08f, winY + 0.46f + tall, z0 + 0.03f});
                    toy.Box({ix - 0.1f, winY + 0.04f, z0}, {ix + 0.1f, winY + 0.1f + tall, z0 + 0.03f});
                }
            }
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
            Window(k, k.SlotX(side, i), closet ? 1.6f : 1.0f, closet ? 0.8f : 1.1f,
                   closet ? 0.55f : std::min(1.15f, k.H - 1.45f));
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
    // Animal houses are roofed with corrugated sheet.
    const float ridge =
        Gable(k, def.roofColor, Pattern::Sheeting, 0.55f, 0.45f, def.wallColor, Pattern::Planks);
    const float slitY = k.H - 1.15f;

    k.Enter(Side::Front);
    for (int i = 0; i < k.Slots(Side::Front); ++i) {
        const float x = k.SlotX(Side::Front, i);
        if (i == def.doorOffset) {
            const float doorH = std::min(k.H - 0.6f, 3.0f);
            Opening(k, x, 0.0f, 1.7f, doorH, kTimber, Pattern::Wood);
            // The rail the sliding door hangs from, the door itself pushed in behind the
            // boards; steel angles where hides rub the jambs.
            MeshWriter& steel = k.m.Use(kSteel, Pattern::Metal);
            steel.Box({x - 1.05f, doorH + 0.16f, 0.0f}, {x + 1.05f, doorH + 0.22f, kPlinthOut + 0.12f});
            for (const float s : {-1.0f, 1.0f}) {
                steel.Box({x + s * 0.85f - 0.03f, 0.0f, kPlinthOut + 0.07f},
                          {x + s * 0.85f + 0.03f, 1.3f, kPlinthOut + 0.1f});
            }
        } else {
            Window(k, x, slitY, 0.95f, 0.5f);
        }
    }
    k.Leave();
    StrawAt(k, k.doorX, 0.8f);
    for (const Side side : {Side::Back, Side::Right, Side::Left}) {
        k.Enter(side);
        for (int i = 0; i < k.Slots(side); ++i) {
            const float x = k.SlotX(side, i);
            Window(k, x, slitY, 0.95f, 0.5f);
            if (side == Side::Back && i == 0) {
                // The keepers' door.
                Door(k, x, 0.9f, 2.05f, kSteel);
            }
        }
        k.Leave();
    }
    // A loading hatch into the loft in one gable, under the beam of its hoist.
    const bool ridgeAlongX = k.wx >= k.wz;
    const float span = ridgeAlongX ? k.wz : k.wx;
    if (span > 2.2f && k.H > 3.2f) {
        k.Enter(ridgeAlongX ? Side::Right : Side::Back);
        const float out = kPlinthOut + 0.03f;
        k.m.Use(kTimber, Pattern::Wood).Box({-0.5f, k.H - 0.1f, 0.0f}, {0.5f, k.H + 0.8f, out});
        k.m.Use(Scaled(def.wallColor, 0.7f), Pattern::Planks)
            .Box({-0.42f, k.H - 0.03f, out}, {0.42f, k.H + 0.72f, out + 0.02f});
        k.m.Use(kTimber, Pattern::Wood).Box({-0.07f, k.H + 0.92f, -0.3f}, {0.07f, k.H + 1.06f, 0.8f});
        k.Leave();
    }
    if (k.wx > 3.0f || k.wz > 3.0f) {
        // Ventilators ride the ridge of the bigger houses.
        MeshWriter& vent = k.m.Use(kSteel, Pattern::Metal);
        for (const float t : {-0.45f, 0.45f}) {
            const float vx = ridgeAlongX ? t * k.wx : 0.0f;
            const float vz = ridgeAlongX ? 0.0f : t * k.wz;
            vent.BoxAt(vx, vz, ridge - 0.05f, 0.5f, 0.3f, 0.5f);
            vent.HipRoof(vx - 0.34f, vx + 0.34f, vz - 0.34f, vz + 0.34f, ridge + 0.25f, ridge + 0.42f);
        }
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
    StrawAt(k, 0.0f, 0.9f);
    // The thatch is bound into a knot at the top, and a pole ties the eaves together.
    const float top = k.H + std::min(k.wx, k.wz) * 0.8f + 0.25f;
    k.m.Use(k.def.roofColor, Pattern::Thatch)
        .Tube({0.0f, top - 0.25f, 0.0f}, {0.0f, top + 0.22f, 0.0f}, 0.2f, 0.09f, 7);
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
    {
        // What stands behind the hatch: a shelf of cups and bottles, the till on the counter.
        const float z0 = kPlinthOut + 0.025f;
        MeshWriter& steel = k.m.Use(kSteel, Pattern::Metal);
        steel.Box({-hatch * 0.5f, 1.62f, z0}, {hatch * 0.5f, 1.65f, z0 + 0.03f});
        steel.Box({hatch * 0.5f - 0.42f, 1.02f, z0}, {hatch * 0.5f - 0.1f, 1.3f, z0 + 0.04f});
        const int items = std::max(3, static_cast<int>(hatch / 0.2f));
        for (int i = 0; i < items; ++i) {
            const float ix = -hatch * 0.5f + 0.12f +
                             (hatch - 0.24f) * static_cast<float>(i) / static_cast<float>(items - 1);
            const float tall = 0.13f + 0.05f * static_cast<float>((i * 5) % 3);
            k.m.Use(i % 3 == 0   ? def.roofColor
                    : i % 3 == 1 ? kCream
                                 : kStraw,
                    Pattern::Plain)
                .Box({ix - 0.05f, 1.65f, z0}, {ix + 0.05f, 1.65f + tall, z0 + 0.03f});
        }
    }
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
    // The sign on the roof says what the stall sells.
    const float signW = 2.0f * k.wx * 0.85f;
    Sign name = Sign::Menu;
    if (NameOf(def, name)) {
        const float w = std::min(signW - 0.12f, 2.6f);
        const float h = w / SignAspect(name);
        k.m.Use(def.roofColor, Pattern::Plain)
            .Box({-signW * 0.5f, k.H + 0.18f, -0.14f}, {signW * 0.5f, k.H + 0.32f + h, 0.0f});
        WriteSign(k.m, name, {-w * 0.5f, k.H + 0.25f, 0.006f}, {w * 0.5f, k.H + 0.25f, 0.006f},
                  {w * 0.5f, k.H + 0.25f + h, 0.006f}, {-w * 0.5f, k.H + 0.25f + h, 0.006f});
    }
    k.Leave();

    // The prices hang on both flanks, where the queue stands.
    for (const Side side : {Side::Right, Side::Left}) {
        k.Enter(side);
        const float out = kPlinthOut + 0.03f;
        k.m.Use(kTimber, Pattern::Wood).Box({-0.42f, 1.22f, 0.0f}, {0.42f, 1.9f, out});
        WriteSign(k.m, Sign::Menu, {-0.39f, 1.27f, out + 0.004f}, {0.39f, 1.27f, out + 0.004f},
                  {0.39f, 1.855f, out + 0.004f}, {-0.39f, 1.855f, out + 0.004f});
        k.Leave();
    }

    k.Enter(Side::Back);
    Door(k, 0.0f, 0.85f, 2.0f, kSteel);
    {
        const float out = kPlinthOut + 0.065f;
        const float w = 0.4f;
        const float h = w / SignAspect(Sign::StaffOnly);
        WriteSign(k.m, Sign::StaffOnly, {-w * 0.5f, 1.45f, out}, {w * 0.5f, 1.45f, out},
                  {w * 0.5f, 1.45f + h, out}, {-w * 0.5f, 1.45f + h, out});
    }
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
        const auto panel = static_cast<Sign>(static_cast<int>(Sign::Board0) + i % 4);
        const float w = 1.36f;
        const float h = w / SignAspect(panel);
        const float y0 = floor + 1.03f;
        WriteSign(k.m, panel, {x - w * 0.5f, y0, z + 0.045f}, {x + w * 0.5f, y0, z + 0.045f},
                  {x + w * 0.5f, y0 + h, z + 0.045f}, {x - w * 0.5f, y0 + h, z + 0.045f});
    }
    // The hall's name hangs from the front beam, over the way in.
    {
        const float w = std::min(3.0f, 2.0f * k.wx - 1.0f);
        const float h = w / SignAspect(Sign::Education);
        const float y0 = k.H - 0.3f - h;
        const float z = k.wz + 0.03f;
        k.m.Use(RoleColor(def.role), Pattern::Plain)
            .Box({k.doorX - w * 0.5f - 0.06f, y0 - 0.06f, k.wz - 0.04f},
                 {k.doorX + w * 0.5f + 0.06f, k.H - 0.26f, z});
        WriteSign(k.m, Sign::Education, {k.doorX - w * 0.5f, y0, z + 0.004f},
                  {k.doorX + w * 0.5f, y0, z + 0.004f}, {k.doorX + w * 0.5f, y0 + h, z + 0.004f},
                  {k.doorX - w * 0.5f, y0 + h, z + 0.004f});
    }
    for (const float sx : {-1.0f, 1.0f}) {
        const float x = sx * k.wx * 0.45f;
        timber.Box({x - 0.9f, floor + 0.38f, -0.2f}, {x + 0.9f, floor + 0.45f, 0.22f});
        timber.Box({x - 0.8f, floor, -0.12f}, {x - 0.7f, floor + 0.38f, 0.14f});
        timber.Box({x + 0.7f, floor, -0.12f}, {x + 0.8f, floor + 0.38f, 0.14f});
    }
}

/// A den of rockwork with a dark mouth: cool in summer. Built as zoos build them, in beds
/// of sprayed concrete over a chamber, so it is a crag and not a heap of boulders.
void Cave(const Shell& k) {
    MeshWriter& rock = k.m.Use(k.def.wallColor, Pattern::Rock);
    const float w = k.W * 0.5f;
    const float d = k.D * 0.5f;
    Crag(rock, {0.0f, 0.0f, -d * 0.12f}, {w * 0.95f, k.H, d * 0.86f}, 811u,
         {.sides = 11, .tiers = 4, .rugged = 0.12f, .dip = 0.06f, .taper = 0.5f, .sink = 1.2f});
    Crag(rock, {-w * 0.6f, 0.0f, d * 0.25f}, {w * 0.45f, k.H * 0.55f, d * 0.6f}, 812u,
         {.sides = 7, .tiers = 3, .rugged = 0.16f, .dip = -0.08f, .taper = 0.5f, .sink = 1.2f});
    Crag(rock, {w * 0.62f, 0.0f, -d * 0.2f}, {w * 0.42f, k.H * 0.66f, d * 0.6f}, 813u,
         {.sides = 7, .tiers = 3, .rugged = 0.16f, .dip = 0.1f, .taper = 0.45f, .sink = 1.2f});
    // The mouth: two piers and a slab laid over them, around darkness.
    const float x = k.doorX;
    const float z = d - 0.55f;
    const float mouthW = 1.9f;
    const float mouthH = std::min(1.9f, k.H * 0.55f);
    for (const float s : {-1.0f, 1.0f}) {
        Crag(rock, {x + s * (mouthW * 0.5f + 0.5f), 0.0f, z - 0.2f}, {0.6f, mouthH + 0.25f, 0.85f},
             815u + static_cast<std::uint32_t>(s > 0.0f ? 1 : 0),
             {.sides = 6, .tiers = 3, .rugged = 0.12f, .dip = 0.0f, .taper = 0.75f, .sink = 1.2f});
    }
    rock.Beam({x - mouthW * 0.5f - 1.1f, mouthH + 0.4f, z - 0.25f},
              {x + mouthW * 0.5f + 1.1f, mouthH + 0.52f, z - 0.3f}, 1.5f, 0.6f);
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

BuildingSight::BuildingSight(const zoo::BuildingDef& def) {
    using Microsoft::Xna::Framework::BoundingBox;
    ModelBuilder model;
    BuildBuilding(model, def);
    for (const ModelPart& part : model.Parts()) {
        const auto pattern = part.finish.pattern;
        if (part.writer.Empty() || pattern == Pattern::Glass || pattern == Pattern::Pane ||
            pattern == Pattern::WireMesh || pattern == Pattern::WeldMesh)
            continue;
        const auto& mesh = part.writer.mesh;
        bounds_ = BoundingBox::CreateMerged(bounds_, mesh.Bounds());
        solids_.AddTriangles(mesh.positions, mesh.indices, Matrix::getIdentityProperty());
    }
}

std::optional<float> BuildingSight::Distance(const Microsoft::Xna::Framework::Ray& localRay, float reach) const {
    const auto entry = localRay.Intersects(bounds_);
    if (!entry || *entry > reach)
        return std::nullopt;
    const auto hit = solids_.Raycast(localRay, reach);
    return hit ? std::optional<float>(hit->distance) : std::nullopt;
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
    {
        // The zoo's name, to those who come and to those who leave.
        const float h = top - boardLo - 0.24f;
        const float w = std::min(h * SignAspect(Sign::Zoo), 2.0f * half * 0.8f);
        const float y0 = boardLo + 0.12f;
        const float y1 = y0 + w / SignAspect(Sign::Zoo);
        WriteSign(model, Sign::Zoo, {-w * 0.5f, y0, 0.226f}, {w * 0.5f, y0, 0.226f}, {w * 0.5f, y1, 0.226f},
                  {-w * 0.5f, y1, 0.226f});
        WriteSign(model, Sign::Zoo, {w * 0.5f, y0, -0.226f}, {-w * 0.5f, y0, -0.226f},
                  {-w * 0.5f, y1, -0.226f}, {w * 0.5f, y1, -0.226f});
    }
    // A lantern on each face of each pillar.
    MeshWriter& lantern = model.Use(kGateGreen, Pattern::Metal);
    MeshWriter& glow = model.Use(kCream, Pattern::Plain);
    for (const float s : {-1.0f, 1.0f}) {
        for (const float f : {-1.0f, 1.0f}) {
            const float z = f * (pillar * 0.5f + 0.2f);
            lantern.Beam({s * px, 3.52f, f * pillar * 0.5f}, {s * px, 3.52f, z}, 0.04f, 0.04f);
            lantern.BoxAt(s * px, z, 3.44f, 0.24f, 0.05f, 0.24f);
            glow.BoxAt(s * px, z, 3.14f, 0.17f, 0.3f, 0.17f);
            lantern.BoxAt(s * px, z, 3.1f, 0.2f, 0.04f, 0.2f);
        }
    }
    // The ticket office: a lodge built against the wall outside, beside the gateway, its
    // window to the forecourt under a tiled lean-to.
    {
        const float x0 = px + pillar * 0.5f + 0.05f;
        const float x1 = x0 + 2.9f;
        const float z0 = 0.16f;
        const float z1 = 2.1f;
        const float eave = 2.55f;
        // The walls stop under the roof's lower edge; the cheeks above them follow its fall.
        const float wallTop = eave + 0.17f;
        const float lean = eave + 0.95f;
        brick.Box({x0, 0.4f, z0}, {x1, wallTop, z1});
        brick.Tri({x1, wallTop, z1}, {x1, wallTop, z0}, {x1, lean - 0.03f, z0});
        brick.Tri({x0, wallTop, z0}, {x0, wallTop, z1}, {x0, lean - 0.03f, z0});
        cap.Box({x0 - 0.05f, -1.2f, z0}, {x1 + 0.05f, 0.4f, z1 + 0.05f});
        MeshWriter& tiles = model.Use({0.3f, 0.12f, 0.08f}, Pattern::RoofTiles);
        tiles.QuadTwoSided({x0 - 0.3f, eave, z1 + 0.55f}, {x1 + 0.3f, eave, z1 + 0.55f},
                           {x1 + 0.3f, lean, z0}, {x0 - 0.3f, lean, z0});
        // A fascia and a gutter along the drip edge, and the flashing where the roof meets the wall.
        model.Use(kTrim, Pattern::Plain)
            .Box({x0 - 0.3f, eave - 0.13f, z1 + 0.5f}, {x1 + 0.3f, eave - 0.01f, z1 + 0.54f});
        MeshWriter& gutter = model.Use(kSteel, Pattern::Metal);
        gutter.Box({x0 - 0.3f, eave - 0.1f, z1 + 0.54f}, {x1 + 0.3f, eave - 0.02f, z1 + 0.63f});
        gutter.Box({x0 - 0.3f, lean - 0.02f, z0 - 0.01f}, {x1 + 0.3f, lean + 0.1f, z0 + 0.05f});
        const float mid = (x0 + x1) * 0.5f;
        const float face = z1 + 0.012f;
        model.Use(kDark, Pattern::Plain)
            .Quad({mid - 0.75f, 1.05f, face}, {mid + 0.75f, 1.05f, face}, {mid + 0.75f, 1.95f, face},
                  {mid - 0.75f, 1.95f, face});
        MeshWriter& trim = model.Use(kTrim, Pattern::Plain);
        trim.Box({mid - 0.85f, 0.95f, z1}, {mid + 0.85f, 1.05f, z1 + 0.3f});
        trim.Box({mid - 0.85f, 1.95f, z1}, {mid + 0.85f, 2.05f, z1 + 0.06f});
        trim.Box({mid - 0.85f, 1.05f, z1}, {mid - 0.75f, 1.95f, z1 + 0.06f});
        trim.Box({mid + 0.75f, 1.05f, z1}, {mid + 0.85f, 1.95f, z1 + 0.06f});
        trim.Box({mid - 0.03f, 1.05f, z1}, {mid + 0.03f, 1.95f, z1 + 0.05f});
        // The keepers of the till come and go by the end.
        model.Use(kGateGreen, Pattern::Planks).Box({x1, 0.42f, z0 + 0.5f}, {x1 + 0.04f, 2.4f, z0 + 1.4f});
    }
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

void BuildHoldingGate(ModelBuilder& model) {
    constexpr float halfWidth = 0.84f;
    constexpr float bottom = 0.08f;
    constexpr float top = 2.52f;
    constexpr float rail = 0.055f;

    // The mesh is a little proud of the frame so its transparency cannot fight
    // the metal in the depth buffer. It remains legible from inside and outside.
    MeshWriter& mesh = model.Use({0.22f, 0.25f, 0.25f}, Pattern::WeldMesh);
    mesh.QuadTwoSided({-halfWidth, bottom, 0.025f}, {halfWidth, bottom, 0.025f},
                      {halfWidth, top, 0.025f}, {-halfWidth, top, 0.025f});

    MeshWriter& steel = model.Use(kSteel, Pattern::Metal);
    steel.Box({-halfWidth - rail, bottom, -0.025f}, {-halfWidth + rail, top, 0.055f});
    steel.Box({halfWidth - rail, bottom, -0.025f}, {halfWidth + rail, top, 0.055f});
    steel.Box({-halfWidth, bottom, -0.025f}, {halfWidth, bottom + rail * 2.0f, 0.055f});
    steel.Box({-halfWidth, top - rail * 2.0f, -0.025f}, {halfWidth, top, 0.055f});
    steel.Beam({-halfWidth + 0.08f, bottom + 0.12f, 0.065f},
               {halfWidth - 0.08f, top - 0.12f, 0.065f}, 0.055f, 0.045f);

    // A high-contrast operating plate distinguishes it from ordinary exhibit mesh.
    model.Use({0.88f, 0.61f, 0.07f}, Pattern::Metal)
        .Box({-0.17f, 1.25f, 0.075f}, {0.17f, 1.47f, 0.115f});
}

void BuildHoldingGateGuides(ModelBuilder& model) {
    MeshWriter& steel = model.Use(kSteel, Pattern::Metal);
    steel.Box({-1.85f, 2.58f, -0.055f}, {1.85f, 2.69f, 0.075f});
    for (const float x : {-1.55f, 0.0f, 1.55f}) {
        steel.Box({x - 0.04f, 2.69f, -0.025f}, {x + 0.04f, 2.94f, 0.065f});
    }
}

}  // namespace zoogame
