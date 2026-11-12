// SPDX-License-Identifier: MIT
#include "FenceModels.hpp"

#include <algorithm>
#include <cmath>

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
            w.BoxAt(base.X, base.Z, base.Y - sink, s, h + 0.1f + sink, s);
            break;
        }
        case zoo::FenceStyle::Mesh: {
            MeshWriter& w = model.Use(look.postColor, Pattern::Metal);
            w.Tube(foot, base + Up(h + 0.05f), 0.04f * stout, 0.04f * stout, 6);
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
            const float s = 0.09f * stout;
            w.BoxAt(base.X, base.Z, base.Y - sink, s, h + 0.06f + sink, s);
            break;
        }
        case zoo::FenceStyle::Wall: {
            if (look.perimeter) {
                MeshWriter& w = model.Use(look.color, Pattern::Brick);
                w.BoxAt(base.X, base.Z, base.Y - sink, 0.55f, h + 0.25f + sink, 0.55f);
                MeshWriter& cap = model.Use(look.postColor, Pattern::Concrete);
                cap.BoxAt(base.X, base.Z, base.Y + h + 0.25f, 0.7f, 0.12f, 0.7f);
            } else {
                MeshWriter& w = model.Use(look.postColor, Pattern::Concrete);
                w.BoxAt(base.X, base.Z, base.Y - sink, 0.36f, h + 0.1f + sink, 0.36f);
            }
            break;
        }
        case zoo::FenceStyle::Cable: {
            MeshWriter& w = model.Use(look.postColor, Pattern::Concrete);
            const float r = (h > 3.5f ? 0.14f : 0.1f) * stout;
            w.Tube(foot, base + Up(h + 0.1f), r, r * 0.85f, 8);
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
            const int rails = h > 1.6f ? 4 : 3;
            for (int i = 0; i < rails; ++i) {
                const float y = h * (0.3f + 0.62f * static_cast<float>(i) / static_cast<float>(rails - 1));
                w.Beam(Along(a, b, 0.0f, y), Along(a, b, 1.0f, y), 0.05f, 0.11f);
            }
            break;
        }
        case zoo::FenceStyle::Mesh: {
            MeshWriter& wire = model.Use(look.color, Pattern::WireMesh);
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
                MeshWriter& door = model.Use(kSteel, Pattern::Metal);
                Slab(door, Along(a, b, 0.07f, 0.0f), Along(a, b, 0.93f, 0.0f), 0.14f, h - 0.08f, 0.05f);
                break;
            }
            MeshWriter& kerb = model.Use(kConcrete, Pattern::Concrete);
            Slab(kerb, a, b, -0.3f, 0.35f, 0.22f);
            MeshWriter& glass = model.Use(look.color, Pattern::Glass);
            glass.Quad(a + Up(0.35f), b + Up(0.35f), b + Up(h), a + Up(h));
            MeshWriter& rail = model.Use(look.postColor, Pattern::Metal);
            rail.Beam(a + Up(h + 0.02f), b + Up(h + 0.02f), 0.07f, 0.05f);
            break;
        }
        case zoo::FenceStyle::Wall: {
            if (gate) {
                MeshWriter& door = model.Use(kWeatheredWood, Pattern::Planks);
                Slab(door, Along(a, b, 0.07f, 0.0f), Along(a, b, 0.93f, 0.0f), 0.14f, h - 0.08f, 0.07f);
                break;
            }
            if (look.perimeter) {
                MeshWriter& wall = model.Use(look.color, Pattern::Brick);
                Slab(wall, a, b, -0.4f, h, 0.36f);
                MeshWriter& coping = model.Use(look.postColor, Pattern::Concrete);
                Slab(coping, a, b, h, h + 0.1f, 0.48f);
            } else {
                MeshWriter& wall = model.Use(look.color, Pattern::Concrete);
                Slab(wall, a, b, -0.4f, h, 0.26f);
                MeshWriter& coping = model.Use(look.postColor, Pattern::Concrete);
                Slab(coping, a, b, h, h + 0.07f, 0.34f);
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
            MeshWriter& w = model.Use(look.color, Pattern::Leaves);
            // Clipped, but alive: two lumpy cushions per panel over a solid core.
            Slab(w, a, b, -0.3f, h * 0.85f, 0.6f);
            const Vector3 half = (b - a) * 0.25f;
            for (int i = 0; i < 2; ++i) {
                const Vector3 c = a + half * (i == 0 ? 1.0f : 3.0f) + Up(h * 0.6f);
                const float lengthX = std::abs(half.X) + 0.42f;
                const float lengthZ = std::abs(half.Z) + 0.42f;
                w.Blob(c, Vector3(lengthX, h * 0.48f, lengthZ), seed * 2u + static_cast<std::uint32_t>(i),
                       0.12f, 8, 5);
            }
            break;
        }
        case zoo::FenceStyle::Count:
            break;
    }
}

}  // namespace zoogame
