// SPDX-License-Identifier: MIT
#include "FenceModels.hpp"

#include <algorithm>
#include <cmath>

#include "PlantModels.hpp"
#include "SignTextures.hpp"

namespace zoogame {

namespace {

const zoo::Color3 kSteel{0.36f, 0.38f, 0.4f};
const zoo::Color3 kDarkSteel{0.1f, 0.11f, 0.12f};
const zoo::Color3 kGateGreen{0.07f, 0.2f, 0.12f};
const zoo::Color3 kConcrete{0.5f, 0.49f, 0.46f};
const zoo::Color3 kBrick{0.42f, 0.2f, 0.13f};
const zoo::Color3 kWeatheredWood{0.3f, 0.22f, 0.15f};

Vector3 Up(float h) {
    return Vector3(0.0f, h, 0.0f);
}

/// A wall slab from a to b following the slope: `y0`..`y1` above the ground line,
/// `thickness` across.
void Slab(MeshWriter& w, const Vector3& a, const Vector3& b, float y0, float y1, float thickness) {
    const Vector3 along = b - a;
    Vector3 across = Vector3::Cross(Vector3(0.0f, 1.0f, 0.0f), along);
    across = Vector3::Normalize(across) * (thickness * 0.5f);
    const Vector3 a0 = a + Up(y0), a1 = a + Up(y1), b0 = b + Up(y0), b1 = b + Up(y1);
    w.Quad(b0 + across, a0 + across, a1 + across, b1 + across);
    w.Quad(a0 - across, b0 - across, b1 - across, a1 - across);
    w.Quad(a1 - across, b1 - across, b1 + across, a1 + across);  // top
    w.Quad(a0 + across, a0 - across, a1 - across, a1 + across);  // end at a
    w.Quad(b0 - across, b0 + across, b1 + across, b1 - across);  // end at b
}

/// A thin round bar (four sides, no caps): fence bars, cables, wires.
void Rod(MeshWriter& w, const Vector3& from, const Vector3& to, float radius) {
    w.Tube(from, to, radius, radius, 4, false, false);
}

Vector3 Along(const Vector3& a, const Vector3& b, float t, float h) {
    return a + (b - a) * t + Up(h);
}

/// A plate fixed to a panel and read from both sides: its middle at `t` along the panel and
/// `y` above the ground, `width` wide.
void PlateOn(ModelBuilder& model, Sign sign, const Vector3& a, const Vector3& b, float t, float y,
             float width) {
    const Vector3 along = Vector3::Normalize(b - a);
    const Vector3 across = Vector3::Normalize(Vector3::Cross(Vector3(0.0f, 1.0f, 0.0f), along));
    const Vector3 mid = Along(a, b, t, y);
    const Vector3 hu = along * (width * 0.5f);
    const Vector3 hv = Up(width / SignAspect(sign) * 0.5f);
    const Vector3 off = across * 0.03f;
    model.Use(kSteel, Pattern::Metal).Beam(mid - hu * 1.04f, mid + hu * 1.04f, 0.05f, hv.Y * 2.08f);
    WriteSign(model, sign, mid + hu - hv + off, mid - hu - hv + off, mid - hu + hv + off,
              mid + hu + hv + off);
    WriteSign(model, sign, mid - hu - hv - off, mid + hu - hv - off, mid + hu + hv - off,
              mid - hu + hv - off);
}

/// The frame of a gate leaf with a diagonal brace, a hand-width inside the posts.
void GateFrame(ModelBuilder& model, const Vector3& a, const Vector3& b, float height) {
    MeshWriter& frame = model.Use(kGateGreen, Pattern::Metal);
    const float lo = 0.12f;
    const float hi = height - 0.05f;
    const float t0 = 0.06f, t1 = 0.94f;
    frame.Beam(Along(a, b, t0, lo), Along(a, b, t1, lo), 0.07f, 0.07f);
    frame.Beam(Along(a, b, t0, hi), Along(a, b, t1, hi), 0.07f, 0.07f);
    frame.Beam(Along(a, b, t0, lo), Along(a, b, t0, hi), 0.07f, 0.07f);
    frame.Beam(Along(a, b, t1, lo), Along(a, b, t1, hi), 0.07f, 0.07f);
    frame.Beam(Along(a, b, t0, lo), Along(a, b, t1, hi), 0.05f, 0.05f);
    // Lock box and hinges.
    MeshWriter& dark = model.Use(kDarkSteel, Pattern::Metal);
    dark.Beam(Along(a, b, 0.86f, 1.0f), Along(a, b, 0.93f, 1.0f), 0.12f, 0.2f);
    dark.Beam(Along(a, b, 0.02f, 0.5f), Along(a, b, 0.08f, 0.5f), 0.1f, 0.1f);
    dark.Beam(Along(a, b, 0.02f, hi - 0.4f), Along(a, b, 0.08f, hi - 0.4f), 0.1f, 0.1f);
    // A drop bolt at the foot of the closing side, and what every keeper's gate says.
    Rod(dark, Along(a, b, 0.9f, 0.02f), Along(a, b, 0.9f, 0.5f), 0.012f);
    if (height > 1.7f) {
        PlateOn(model, Sign::StaffOnly, a, b, 0.5f, 1.5f, 0.46f);
    }
}

/// A point on a square post, so that rain runs off the end grain or the tube's cap.
void Pyramid(MeshWriter& w, const Vector3& base, float half, float rise) {
    const Vector3 top = base + Up(rise);
    const Vector3 corners[4] = {base + Vector3(-half, 0.0f, half), base + Vector3(half, 0.0f, half),
                                base + Vector3(half, 0.0f, -half), base + Vector3(-half, 0.0f, -half)};
    for (int i = 0; i < 4; ++i) {
        w.Tri(corners[i], corners[(i + 1) % 4], top);
    }
}

/// The concrete a post is set in: a block showing a few centimetres above the ground (and
/// reaching into it, so that a slope shows no gap).
void Footing(ModelBuilder& model, const Vector3& base, float size, float above) {
    model.Use(kConcrete, Pattern::Concrete).BoxAt(base.X, base.Z, base.Y - 0.3f, size, 0.3f + above, size);
}

/// The unit vector across a panel, to the left of someone walking from a to b.
Vector3 Across(const Vector3& a, const Vector3& b) {
    return Vector3::Normalize(Vector3::Cross(Vector3(0.0f, 1.0f, 0.0f), b - a));
}

/// A number in 0..1 from a panel's seed, another for every `salt`.
float Chance(std::uint32_t seed, std::uint32_t salt) {
    std::uint32_t h = (seed + salt * 0x9E3779B9u) * 2246822519u;
    h = (h ^ (h >> 15)) * 2654435761u;
    return static_cast<float>((h ^ (h >> 13)) & 0xFFFFu) / 65535.0f;
}

/// A number in 0..1 that belongs to a place on the ground: two panels that meet there agree
/// on it (the height a rockwork wall has at their common end).
float PlaceChance(const Vector3& at, std::uint32_t salt) {
    const auto x = static_cast<std::uint32_t>(static_cast<std::int32_t>(std::lround(at.X * 4.0f)));
    const auto z = static_cast<std::uint32_t>(static_cast<std::int32_t>(std::lround(at.Z * 4.0f)));
    return Chance(x * 73856093u ^ z * 19349663u, salt);
}

/// Welded mesh for bears and big cats: the panel in its frame of flat bar and, above the
/// posts, the overhang no climber gets round, cranked towards the animals (to both sides
/// where the look does not say which side they are on).
void HeavyMeshPanel(ModelBuilder& model, const FenceLook& look, const Vector3& a, const Vector3& b,
                    std::uint32_t seed) {
    const float h = look.height;
    MeshWriter& mesh = model.Use(look.color, Pattern::WeldMesh);
    mesh.Quad(a + Up(0.03f), b + Up(0.03f), b + Up(h - 0.02f), a + Up(h - 0.02f));
    MeshWriter& frame = model.Use(look.postColor, Pattern::Metal);
    const int bays = std::max(2, static_cast<int>(std::lround(h / 1.5f)));
    for (int i = 0; i <= bays; ++i) {
        const float y = 0.05f + (h - 0.05f) * static_cast<float>(i) / static_cast<float>(bays);
        frame.Beam(a + Up(y), b + Up(y), 0.05f, 0.06f);
    }
    frame.Beam(Along(a, b, 0.5f, 0.05f), Along(a, b, 0.5f, h), 0.04f, 0.04f);
    const Vector3 across = Across(a, b);
    for (const float side : {1.0f, -1.0f}) {
        if (look.inward != 0 && side * static_cast<float>(look.inward) < 0.0f) {
            continue;
        }
        const Vector3 reach = across * (side * 0.62f) + Up(0.46f);
        mesh.Quad(a + Up(h), b + Up(h), b + Up(h) + reach, a + Up(h) + reach);
        for (const float t : {0.25f, 0.75f}) {
            frame.Beam(Along(a, b, t, h), Along(a, b, t, h) + reach, 0.045f, 0.045f);
        }
        Rod(frame, a + Up(h) + reach, b + Up(h) + reach, 0.014f);
    }
    // Now and then a plate says what the mesh is for.
    if (Chance(seed, 1) < 0.14f) {
        PlateOn(model, Sign::Danger, a, b, 0.5f, 1.55f, 0.5f);
    }
}

/// A viewing window: the pane stands in a channel on a low kerb and under a top rail,
/// between the posts.
void GlassPanel(ModelBuilder& model, const FenceLook& look, const Vector3& a, const Vector3& b,
                std::uint32_t seed) {
    const float h = look.height;
    const float sill = 0.35f;
    MeshWriter& kerb = model.Use(kConcrete, Pattern::Concrete);
    Slab(kerb, a, b, -0.3f, sill, 0.24f);
    MeshWriter& frame = model.Use(look.postColor, Pattern::Metal);
    frame.Beam(a + Up(sill + 0.025f), b + Up(sill + 0.025f), 0.07f, 0.05f);
    frame.Beam(a + Up(h + 0.02f), b + Up(h + 0.02f), 0.08f, 0.06f);
    // No two panes are dirty alike: every other one shows the picture the other way round.
    const bool turned = Chance(seed, 2) < 0.5f;
    const float lo = sill + 0.05f;
    const float hi = h - 0.01f;
    model.Use(look.color, Pattern::Pane)
        .Picture(Along(a, b, 0.02f, lo), Along(a, b, 0.98f, lo), Along(a, b, 0.98f, hi),
                 Along(a, b, 0.02f, hi), turned ? 1.0f : 0.0f, 0.0f, turned ? 0.0f : 1.0f, 1.0f);
}

/// A keeper's door in a solid front: a steel leaf in its frame, a handle, a kick plate.
void ServiceDoor(ModelBuilder& model, const Vector3& a, const Vector3& b, float height,
                 const zoo::Color3& leaf, Pattern pattern) {
    const float top = std::min(height - 0.08f, 2.1f);
    Slab(model.Use(leaf, pattern), Along(a, b, 0.1f, 0.0f), Along(a, b, 0.9f, 0.0f), 0.05f, top, 0.05f);
    if (height - 0.08f > top + 0.05f) {
        // What is above the door is closed with the fence's own steel.
        Slab(model.Use(kSteel, Pattern::Metal), Along(a, b, 0.07f, 0.0f), Along(a, b, 0.93f, 0.0f),
             top + 0.06f, height - 0.08f, 0.04f);
    }
    MeshWriter& dark = model.Use(kDarkSteel, Pattern::Metal);
    const Vector3 out = Across(a, b) * 0.05f;
    for (const float side : {1.0f, -1.0f}) {
        dark.Beam(Along(a, b, 0.8f, 1.02f) + out * side, Along(a, b, 0.88f, 1.02f) + out * side, 0.03f,
                  0.03f);
        dark.Beam(Along(a, b, 0.14f, 0.2f) + out * (side * 0.6f),
                  Along(a, b, 0.86f, 0.2f) + out * (side * 0.6f), 0.012f, 0.22f);
    }
}

/// Rockwork as zoos build it: sprayed concrete over a frame, carved into beds. Thicker at
/// the foot than at the top, the top line rising and falling from end to end.
void RockworkPanel(ModelBuilder& model, const FenceLook& look, const Vector3& a, const Vector3& b,
                   std::uint32_t seed) {
    MeshWriter& rock = model.Use(look.color, Pattern::Rock);
    const float h = look.height;
    const Vector3 across = Across(a, b);
    const int steps = 4;
    // Front and back profiles at every step along the panel: foot, belly, shoulder, crest.
    Vector3 front[steps + 1][4];
    Vector3 back[steps + 1][4];
    for (int i = 0; i <= steps; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(steps);
        const Vector3 at = Along(a, b, t, 0.0f);
        const float crest = h + 0.55f * PlaceChance(at, 3);
        const float heights[4] = {-0.4f, 0.32f * crest, 0.74f * crest, crest};
        const float halves[4] = {0.5f, 0.42f, 0.3f, 0.14f};
        for (int k = 0; k < 4; ++k) {
            const float bulge = k == 0 || k == 3
                                    ? 0.0f
                                    : 0.12f * (PlaceChance(at, 4u + static_cast<std::uint32_t>(k)) - 0.5f);
            const float lean =
                k == 0 ? 0.0f : 0.1f * (PlaceChance(at, 9u + static_cast<std::uint32_t>(k)) - 0.5f);
            front[i][k] = at + Up(heights[k]) + across * (halves[k] + bulge + lean);
            back[i][k] = at + Up(heights[k]) - across * (halves[k] + bulge - lean);
        }
    }
    for (int i = 0; i < steps; ++i) {
        for (int k = 0; k < 3; ++k) {
            rock.Quad(front[i + 1][k], front[i][k], front[i][k + 1], front[i + 1][k + 1]);
            rock.Quad(back[i][k], back[i + 1][k], back[i + 1][k + 1], back[i][k + 1]);
        }
        rock.Quad(back[i][3], back[i + 1][3], front[i + 1][3], front[i][3]);
    }
    (void)seed;
}

}  // namespace

bool LookOf(const zoo::Catalog& catalog, zoo::Fence fence, FenceLook& out) {
    if (!fence.Exists()) {
        return false;
    }
    if (fence.kind == zoo::kPerimeterFence) {
        out.style = zoo::FenceStyle::Wall;
        out.height = zoo::kPerimeterHeightM;
        out.color = kBrick;
        out.postColor = kConcrete;
        out.perimeter = true;
        return true;
    }
    const zoo::FenceDef* def = catalog.FenceByKind(fence.kind);
    if (def == nullptr) {
        return false;
    }
    out.style = def->style;
    out.height = def->heightM;
    out.color = def->color;
    out.postColor = def->postColor;
    out.perimeter = false;
    out.heavy = def->style == zoo::FenceStyle::Mesh && def->strength >= 4;
    out.handrail = def->id == "stand_off_rail";
    return true;
}

void BuildFencePost(ModelBuilder& model, const FenceLook& look, const Vector3& base, bool gatePost) {
    const float sink = 0.3f;  // posts reach into the ground, so slopes show no gap
    const float h = look.height;
    const float stout = gatePost ? 1.5f : 1.0f;
    const Vector3 foot = base - Up(sink);
    switch (look.style) {
        case zoo::FenceStyle::Rail: {
            MeshWriter& w = model.Use(look.postColor, Pattern::Wood);
            const float s = 0.13f * stout;
            // A handrail's posts end under its top board.
            w.BoxAt(base.X, base.Z, base.Y - sink, s, h + (look.handrail ? -0.02f : 0.1f) + sink, s);
            if (!look.handrail) {
                Pyramid(w, base + Up(h + 0.1f), s * 0.5f, 0.05f);
            }
            break;
        }
        case zoo::FenceStyle::Mesh: {
            MeshWriter& w = model.Use(look.postColor, Pattern::Metal);
            if (look.heavy) {
                const float s = 0.14f * stout;
                w.BoxAt(base.X, base.Z, base.Y - sink, s, h + 0.08f + sink, s);
                w.BoxAt(base.X, base.Z, base.Y + h + 0.08f, s + 0.05f, 0.03f, s + 0.05f);
                // A base plate bolted to a block of concrete.
                w.BoxAt(base.X, base.Z, base.Y + 0.1f, s + 0.14f, 0.02f, s + 0.14f);
                Footing(model, base, s + 0.3f, 0.1f);
                break;
            }
            // Galvanised tube, thicker for the taller fence, under a pressed cap.
            const float r = (h > 2.6f ? 0.045f : 0.034f) * stout;
            w.Tube(foot, base + Up(h + 0.05f), r, r, 6, false, false);
            w.Tube(base + Up(h + 0.05f), base + Up(h + 0.08f), r + 0.008f, r + 0.008f, 6, true, true);
            Footing(model, base, 0.24f * stout, 0.05f);
            break;
        }
        case zoo::FenceStyle::Bars: {
            MeshWriter& w = model.Use(look.postColor, Pattern::Metal);
            const float s = 0.11f * stout;
            w.BoxAt(base.X, base.Z, base.Y - sink, s, h + 0.12f + sink, s);
            // A small finial.
            w.Tube(base + Up(h + 0.12f), base + Up(h + 0.3f), s * 0.5f, 0.0f, 4, false, false);
            break;
        }
        case zoo::FenceStyle::Glass: {
            MeshWriter& w = model.Use(look.postColor, Pattern::Metal);
            const float s = 0.1f * stout;
            w.BoxAt(base.X, base.Z, base.Y - sink, s, h + 0.07f + sink, s);
            // The post's cap, and its shoe on the kerb the panes stand on.
            w.BoxAt(base.X, base.Z, base.Y + h + 0.07f, s + 0.04f, 0.02f, s + 0.04f);
            w.BoxAt(base.X, base.Z, base.Y + 0.35f, s + 0.08f, 0.03f, s + 0.08f);
            Footing(model, base, 0.3f * stout, 0.35f);
            break;
        }
        case zoo::FenceStyle::Wall: {
            if (look.perimeter) {
                MeshWriter& w = model.Use(look.color, Pattern::Brick);
                w.BoxAt(base.X, base.Z, base.Y - sink, 0.55f, h + 0.25f + sink, 0.55f);
                MeshWriter& cap = model.Use(look.postColor, Pattern::Concrete);
                cap.BoxAt(base.X, base.Z, base.Y + h + 0.25f, 0.7f, 0.12f, 0.7f);
                Pyramid(cap, base + Up(h + 0.37f), 0.35f, 0.14f);
                // The pier's plinth, a course proud of the brick.
                cap.BoxAt(base.X, base.Z, base.Y - sink, 0.64f, 0.32f + sink, 0.64f);
            }
            // Rockwork has no posts: the panels run into each other.
            break;
        }
        case zoo::FenceStyle::Cable: {
            // Steel tube filled with concrete, as thick as a thigh, under a welded cap; the
            // cables run through it.
            MeshWriter& w = model.Use(look.postColor, Pattern::Metal);
            const float r = (h > 3.5f ? 0.14f : 0.1f) * stout;
            w.Tube(foot, base + Up(h + 0.1f), r, r, 8, false, false);
            w.Tube(base + Up(h + 0.1f), base + Up(h + 0.13f), r + 0.015f, r + 0.015f, 8, true, true);
            Footing(model, base, 2.0f * r + 0.24f, 0.08f);
            break;
        }
        case zoo::FenceStyle::Hedge:
        case zoo::FenceStyle::Count:
            break;
    }
}

void BuildFencePanel(ModelBuilder& model, const FenceLook& look, const Vector3& a, const Vector3& b,
                     bool gate, std::uint32_t seed) {
    const float h = look.height;
    if (gate) {
        GateFrame(model, a, b, h);
    }
    switch (look.style) {
        case zoo::FenceStyle::Rail: {
            MeshWriter& w = model.Use(look.color, Pattern::Wood);
            if (gate) {
                // A boarded gate.
                for (int i = 0; i < 9; ++i) {
                    const float t = 0.1f + 0.1f * static_cast<float>(i);
                    w.Beam(Along(a, b, t, 0.15f), Along(a, b, t, h - 0.08f), 0.16f, 0.03f);
                }
                break;
            }
            if (look.handrail) {
                // A broad board to lean on, a rail under it and one at a child's height.
                w.Beam(Along(a, b, 0.0f, h), Along(a, b, 1.0f, h), 0.17f, 0.045f);
                w.Beam(Along(a, b, 0.0f, h * 0.72f), Along(a, b, 1.0f, h * 0.72f), 0.045f, 0.09f);
                w.Beam(Along(a, b, 0.0f, h * 0.4f), Along(a, b, 1.0f, h * 0.4f), 0.045f, 0.09f);
                if (Chance(seed, 3) < 0.12f) {
                    PlateOn(model, Sign::DoNotFeed, a, b, 0.5f, h * 0.56f, 0.5f);
                }
                break;
            }
            const int rails = h > 1.6f ? 4 : 3;
            for (int i = 0; i < rails; ++i) {
                const float y = h * (0.3f + 0.62f * static_cast<float>(i) / static_cast<float>(rails - 1));
                // Sawn rails nailed by hand: none sits quite level.
                const float y0 = y + 0.03f * (Chance(seed, 10u + static_cast<std::uint32_t>(i)) - 0.5f);
                const float y1 = y + 0.03f * (Chance(seed, 20u + static_cast<std::uint32_t>(i)) - 0.5f);
                w.Beam(Along(a, b, 0.0f, y0), Along(a, b, 1.0f, y1), 0.05f, 0.11f);
            }
            break;
        }
        case zoo::FenceStyle::Mesh: {
            if (!gate && look.heavy) {
                HeavyMeshPanel(model, look, a, b, seed);
                break;
            }
            MeshWriter& wire = model.Use(look.color, look.heavy ? Pattern::WeldMesh : Pattern::WireMesh);
            const float t0 = gate ? 0.07f : 0.0f;
            const float t1 = gate ? 0.93f : 1.0f;
            const float lo = gate ? 0.14f : 0.03f;
            wire.Quad(Along(a, b, t0, lo), Along(a, b, t1, lo), Along(a, b, t1, h - 0.06f),
                      Along(a, b, t0, h - 0.06f));
            if (!gate) {
                MeshWriter& rail = model.Use(look.postColor, Pattern::Metal);
                Rod(rail, a + Up(h), b + Up(h), 0.025f);
                Rod(rail, a + Up(0.05f), b + Up(0.05f), 0.02f);
                if (h > 2.6f) {
                    Rod(rail, a + Up(h * 0.5f), b + Up(h * 0.5f), 0.02f);
                }
            }
            break;
        }
        case zoo::FenceStyle::Bars: {
            MeshWriter& w = model.Use(look.color, Pattern::Metal);
            if (!gate) {
                w.Beam(a + Up(0.18f), b + Up(0.18f), 0.05f, 0.06f);
                w.Beam(a + Up(h - 0.15f), b + Up(h - 0.15f), 0.05f, 0.06f);
                if (h > 2.8f) {
                    w.Beam(a + Up(h * 0.5f), b + Up(h * 0.5f), 0.04f, 0.05f);
                }
            }
            const int bars = 14;
            for (int i = 0; i < bars; ++i) {
                const float t = (static_cast<float>(i) + 0.5f) / static_cast<float>(bars);
                if (gate && (t < 0.08f || t > 0.92f)) {
                    continue;
                }
                Rod(w, Along(a, b, t, 0.1f), Along(a, b, t, gate ? h - 0.08f : h + 0.05f), 0.016f);
            }
            break;
        }
        case zoo::FenceStyle::Glass: {
            if (gate) {
                // A steel service door in the glass front.
                ServiceDoor(model, a, b, h, kSteel, Pattern::Metal);
                break;
            }
            GlassPanel(model, look, a, b, seed);
            break;
        }
        case zoo::FenceStyle::Wall: {
            if (gate) {
                ServiceDoor(model, a, b, h, kWeatheredWood, Pattern::Planks);
                break;
            }
            if (look.perimeter) {
                MeshWriter& wall = model.Use(look.color, Pattern::Brick);
                Slab(wall, a, b, 0.3f, h, 0.36f);
                MeshWriter& coping = model.Use(look.postColor, Pattern::Concrete);
                Slab(coping, a, b, h, h + 0.1f, 0.48f);
                // A plinth of concrete under the brick, and the coping's drip above it.
                Slab(coping, a, b, -0.4f, 0.3f, 0.44f);
            } else {
                RockworkPanel(model, look, a, b, seed);
            }
            break;
        }
        case zoo::FenceStyle::Cable: {
            MeshWriter& w = model.Use(look.color, Pattern::Metal);
            if (gate) {
                const int bars = 9;
                for (int i = 1; i < bars; ++i) {
                    const float t = 0.06f + 0.88f * static_cast<float>(i) / static_cast<float>(bars);
                    Rod(w, Along(a, b, t, 0.12f), Along(a, b, t, h - 0.05f), 0.02f);
                }
                break;
            }
            const int cables = std::max(4, static_cast<int>(std::lround(h / 0.32f)));
            for (int i = 0; i < cables; ++i) {
                const float y = 0.25f + (h - 0.35f) * static_cast<float>(i) / static_cast<float>(cables - 1);
                Rod(w, a + Up(y), b + Up(y), 0.014f);
            }
            break;
        }
        case zoo::FenceStyle::Hedge: {
            if (gate) {
                MeshWriter& w = model.Use(kWeatheredWood, Pattern::Planks);
                Slab(w, Along(a, b, 0.07f, 0.0f), Along(a, b, 0.93f, 0.0f), 0.14f, h - 0.08f, 0.05f);
                break;
            }
            BuildHedge(model, a, b, h, look.color, seed);
            break;
        }
        case zoo::FenceStyle::Count:
            break;
    }
}

}  // namespace zoogame
