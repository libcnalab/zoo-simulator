// SPDX-License-Identifier: MIT
#include "PropModels.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <string_view>

#include "zoo/base/Rng.hpp"

namespace zoogame {

namespace {

constexpr float kPi = std::numbers::pi_v<float>;

const zoo::Color3 kTimber{0.36f, 0.24f, 0.14f};
const zoo::Color3 kPaleTimber{0.5f, 0.38f, 0.24f};
const zoo::Color3 kCutWood{0.62f, 0.48f, 0.3f};
const zoo::Color3 kSteel{0.36f, 0.38f, 0.4f};
const zoo::Color3 kDarkSteel{0.08f, 0.09f, 0.1f};
const zoo::Color3 kRope{0.55f, 0.44f, 0.27f};
const zoo::Color3 kStone{0.5f, 0.5f, 0.47f};
const zoo::Color3 kSoil{0.16f, 0.11f, 0.07f};
const zoo::Color3 kHay{0.66f, 0.55f, 0.24f};
const zoo::Color3 kWater{0.1f, 0.28f, 0.36f};

/// What the look functions share.
struct Maker {
    ModelBuilder& m;
    const zoo::PropDef& def;
    zoo::Rng rng;
    float H;  ///< the definition's height in metres

    Maker(ModelBuilder& model, const zoo::PropDef& definition, std::uint32_t seed)
        : m(model), def(definition), rng(seed, 5), H(definition.heightM) {}

    float R(float lo, float hi) { return rng.Range(lo, hi); }
    std::uint32_t Seed() { return rng.NextU32(); }
    MeshWriter& Leaves() { return m.Use(def.color, Pattern::Leaves); }
    MeshWriter& Needles() { return m.Use(def.color, Pattern::Needles); }
    MeshWriter& Bark() { return m.Use(def.color2, Pattern::Bark); }
    MeshWriter& Timber() { return m.Use(kTimber, Pattern::Wood); }
    MeshWriter& Planks() { return m.Use(kPaleTimber, Pattern::Planks); }
    MeshWriter& Steel() { return m.Use(kSteel, Pattern::Metal); }
};

Vector3 Polar(float angle, float distance, float y = 0.0f) {
    return Vector3(std::cos(angle) * distance, y, std::sin(angle) * distance);
}

/// A branch or log without end caps.
void Limb(MeshWriter& w, const Vector3& from, const Vector3& to, float r0, float r1, int sides = 6) {
    w.Tube(from, to, r0, r1, sides, false, false);
}

/// A kerb ring with inner and outer faces and a flat top.
void Ring(MeshWriter& w, const Vector3& center, float radius, float y0, float y1, float thickness,
          int sides = 14) {
    const float outer = radius;
    const float inner = radius - thickness;
    for (int i = 0; i < sides; ++i) {
        const float a0 = 2.0f * kPi * static_cast<float>(i) / static_cast<float>(sides);
        const float a1 = 2.0f * kPi * static_cast<float>(i + 1) / static_cast<float>(sides);
        const Vector3 o0 = center + Polar(a0, outer), o1 = center + Polar(a1, outer);
        const Vector3 i0 = center + Polar(a0, inner), i1 = center + Polar(a1, inner);
        const Vector3 lo(0.0f, y0, 0.0f), hi(0.0f, y1, 0.0f);
        w.Quad(o1 + lo, o0 + lo, o0 + hi, o1 + hi);  // outside
        w.Quad(i0 + lo, i1 + lo, i1 + hi, i0 + hi);  // inside
        w.Quad(o1 + hi, o0 + hi, i0 + hi, i1 + hi);  // top
    }
}

/// An open box (four walls and a floor): troughs, planters.
void Tub(MeshWriter& w, float base, float sizeX, float sizeY, float sizeZ, float wall) {
    w.BoxAt(0.0f, 0.0f, base, sizeX, wall, sizeZ);
    w.BoxAt(-(sizeX - wall) * 0.5f, 0.0f, base, wall, sizeY, sizeZ);
    w.BoxAt((sizeX - wall) * 0.5f, 0.0f, base, wall, sizeY, sizeZ);
    w.BoxAt(0.0f, -(sizeZ - wall) * 0.5f, base, sizeX - 2.0f * wall, sizeY, wall);
    w.BoxAt(0.0f, (sizeZ - wall) * 0.5f, base, sizeX - 2.0f * wall, sizeY, wall);
}

/// An arching leaf: a V-shaped ribbon from `base` outwards in direction
/// `angle`, rising first and drooping towards its tip.
void Frond(MeshWriter& w, const Vector3& base, float angle, float length, float rise, float droop,
           float width) {
    const Vector3 out = Polar(angle, 1.0f);
    const Vector3 side = Polar(angle + kPi * 0.5f, 1.0f);
    constexpr int kSegments = 5;
    Vector3 spine[kSegments + 1];
    float half[kSegments + 1];
    for (int i = 0; i <= kSegments; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(kSegments);
        const float s = t * length;
        spine[i] = base + out * s + Vector3(0.0f, rise * s - droop * s * s / length, 0.0f);
        half[i] = width * 0.5f * (0.35f + 0.65f * std::sin(kPi * std::min(1.0f, t * 1.25f + 0.12f)));
        if (i == kSegments) {
            half[i] = 0.03f;
        }
    }
    const Vector3 keel(0.0f, 0.3f, 0.0f);
    for (int i = 0; i < kSegments; ++i) {
        const Vector3 l0 = spine[i] - side * half[i] + keel * half[i];
        const Vector3 l1 = spine[i + 1] - side * half[i + 1] + keel * half[i + 1];
        const Vector3 r0 = spine[i] + side * half[i] + keel * half[i];
        const Vector3 r1 = spine[i + 1] + side * half[i + 1] + keel * half[i + 1];
        w.QuadTwoSided(spine[i], spine[i + 1], l1, l0);
        w.QuadTwoSided(spine[i + 1], spine[i], r0, r1);
    }
}

// ---- trees ----------------------------------------------------------------------------

struct Broadleaf {
    float trunkHeight = 0.36f;  ///< where the trunk forks, as a share of the height
    float trunkRadius = 0.35f;  ///< metres at breast height
    float crownRadius = 0.3f;   ///< horizontal reach of the crown, share of the height
    float crownHeight = 0.24f;  ///< vertical half-size of the crown, share of the height
    int blobs = 5;
    float lumpiness = 0.2f;
    zoo::Color3 bark{0.3f, 0.22f, 0.15f};
};

/// Returns the crown's centre.
Vector3 BroadleafTree(Maker& k, const Broadleaf& p) {
    MeshWriter& bark = k.m.Use(p.bark, Pattern::Bark);
    MeshWriter& leaves = k.Leaves();
    const float H = k.H;
    const Vector3 fork(k.R(-0.03f, 0.03f) * H, H * p.trunkHeight, k.R(-0.03f, 0.03f) * H);
    bark.Tube(Vector3(0.0f, -0.4f, 0.0f), Vector3(0.0f, 0.6f, 0.0f), p.trunkRadius * 1.5f, p.trunkRadius, 8,
              false, false);
    bark.Tube(Vector3(0.0f, 0.6f, 0.0f), fork, p.trunkRadius, p.trunkRadius * 0.72f, 8, false, false);

    const float reach = p.crownRadius * H;
    const float tall = p.crownHeight * H;
    const Vector3 heart = fork + Vector3(0.0f, tall * 0.95f, 0.0f);
    Limb(bark, fork, heart, p.trunkRadius * 0.7f, p.trunkRadius * 0.2f);
    leaves.Blob(heart + Vector3(0.0f, tall * 0.15f, 0.0f), Vector3(reach * 0.72f, tall, reach * 0.72f),
                k.Seed(), p.lumpiness, 10, 7);
    const float start = k.R(0.0f, 2.0f * kPi);
    for (int i = 0; i < p.blobs; ++i) {
        const float angle =
            start + 2.0f * kPi * static_cast<float>(i) / static_cast<float>(p.blobs) + k.R(-0.25f, 0.25f);
        const Vector3 c = heart + Polar(angle, reach * k.R(0.5f, 0.66f), tall * k.R(-0.55f, 0.3f));
        const float radius = reach * k.R(0.44f, 0.58f);
        leaves.Blob(c, Vector3(radius, radius * k.R(0.68f, 0.85f), radius), k.Seed(), p.lumpiness);
        Limb(bark, fork - Vector3(0.0f, k.R(0.0f, 0.12f) * H, 0.0f), c - Vector3(0.0f, radius * 0.3f, 0.0f),
             p.trunkRadius * 0.38f, p.trunkRadius * 0.12f, 5);
    }
    return heart;
}

void Oak(Maker& k) {
    Broadleaf p;
    p.trunkHeight = k.R(0.3f, 0.38f);
    p.trunkRadius = 0.4f;
    p.crownRadius = 0.34f;
    p.crownHeight = 0.24f;
    p.blobs = 6;
    p.bark = k.def.color2;
    BroadleafTree(k, p);
}

void Birch(Maker& k) {
    Broadleaf p;
    p.trunkHeight = k.R(0.46f, 0.54f);
    p.trunkRadius = 0.15f;
    p.crownRadius = 0.2f;
    p.crownHeight = 0.24f;
    p.blobs = 4;
    p.lumpiness = 0.24f;
    p.bark = {0.74f, 0.73f, 0.68f};
    BroadleafTree(k, p);
}

void Willow(Maker& k) {
    Broadleaf p;
    p.trunkHeight = 0.3f;
    p.trunkRadius = 0.42f;
    p.crownRadius = 0.36f;
    p.crownHeight = 0.22f;
    p.blobs = 5;
    p.bark = k.def.color2;
    const Vector3 heart = BroadleafTree(k, p);
    // The hanging curtains.
    MeshWriter& leaves = k.Leaves();
    const float reach = p.crownRadius * k.H;
    const int curtains = 9;
    const float start = k.R(0.0f, 2.0f * kPi);
    for (int i = 0; i < curtains; ++i) {
        const float angle = start + 2.0f * kPi * static_cast<float>(i) / static_cast<float>(curtains);
        const float drop = k.H * k.R(0.26f, 0.34f);
        const Vector3 c =
            Vector3(heart.X, 0.0f, heart.Z) + Polar(angle, reach * k.R(0.8f, 0.95f), heart.Y - drop * 0.7f);
        leaves.Blob(c, Vector3(reach * 0.3f, drop, reach * 0.3f), k.Seed(), 0.22f, 7, 6);
    }
}

void Spruce(Maker& k) {
    MeshWriter& bark = k.Bark();
    MeshWriter& needles = k.Needles();
    const float H = k.H;
    bark.Tube(Vector3(0.0f, -0.4f, 0.0f), Vector3(0.0f, H * 0.95f, 0.0f), 0.24f, 0.03f, 7, false, false);
    const int tiers = 7;
    const float bottom = H * 0.1f;
    const float widest = H * k.R(0.19f, 0.23f);
    for (int i = 0; i < tiers; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(tiers);
        const float y0 = bottom + (H - bottom) * t * 0.93f;
        const float radius = widest * (1.0f - t * 0.82f) * k.R(0.92f, 1.08f);
        const float tall = (H - bottom) / static_cast<float>(tiers) * 1.9f;
        const float y1 = std::min(H, y0 + tall);
        // Each tier is a skirt: a cone with a turned-down hem, closed underneath.
        needles.Tube(Vector3(0.0f, y0 - tall * 0.12f, 0.0f), Vector3(0.0f, y0, 0.0f), radius * 0.86f, radius,
                     9, true, false);
        needles.Tube(Vector3(0.0f, y0, 0.0f), Vector3(0.0f, y1, 0.0f), radius, 0.0f, 9, false, false);
    }
}

void Pine(Maker& k) {
    MeshWriter& bark = k.m.Use({0.36f, 0.2f, 0.12f}, Pattern::Bark);
    MeshWriter& needles = k.Needles();
    const float H = k.H;
    const Vector3 top(k.R(-0.04f, 0.04f) * H, H * 0.8f, k.R(-0.04f, 0.04f) * H);
    bark.Tube(Vector3(0.0f, -0.4f, 0.0f), Vector3(0.0f, 0.6f, 0.0f), 0.42f, 0.28f, 8, false, false);
    bark.Tube(Vector3(0.0f, 0.6f, 0.0f), top, 0.28f, 0.1f, 8, false, false);
    const int pads = 5;
    const float start = k.R(0.0f, 2.0f * kPi);
    for (int i = 0; i < pads; ++i) {
        const float angle = start + 2.0f * kPi * static_cast<float>(i) / static_cast<float>(pads - 1);
        const bool crown = i == pads - 1;
        const float radius = H * (crown ? 0.17f : k.R(0.12f, 0.16f));
        const Vector3 c = crown ? top + Vector3(0.0f, H * 0.05f, 0.0f)
                                : top + Polar(angle, H * k.R(0.12f, 0.17f), -H * k.R(0.02f, 0.2f));
        needles.Blob(c, Vector3(radius, radius * 0.5f, radius), k.Seed(), 0.22f, 9, 5);
        if (!crown) {
            const Vector3 root(top.X * 0.8f, c.Y - H * 0.06f, top.Z * 0.8f);
            Limb(bark, root, c - Vector3(0.0f, radius * 0.2f, 0.0f), 0.09f, 0.04f, 5);
        }
    }
    // A dead stub or two below the crown.
    for (int i = 0; i < 2; ++i) {
        const float y = H * k.R(0.35f, 0.55f);
        Limb(bark, Vector3(0.0f, y, 0.0f), Polar(k.R(0.0f, 6.28f), k.R(0.7f, 1.3f), y + 0.3f), 0.05f, 0.02f,
             4);
    }
}

void Acacia(Maker& k) {
    MeshWriter& bark = k.Bark();
    MeshWriter& leaves = k.Leaves();
    const float H = k.H;
    const Vector3 fork(k.R(-0.06f, 0.06f) * H, H * k.R(0.36f, 0.45f), k.R(-0.06f, 0.06f) * H);
    bark.Tube(Vector3(0.0f, -0.4f, 0.0f), fork, 0.3f, 0.2f, 7, false, false);
    const int limbs = 4;
    const float start = k.R(0.0f, 2.0f * kPi);
    const float canopy = H * 0.86f;
    for (int i = 0; i < limbs; ++i) {
        const float angle =
            start + 2.0f * kPi * static_cast<float>(i) / static_cast<float>(limbs) + k.R(-0.3f, 0.3f);
        const Vector3 tip =
            Vector3(fork.X, 0.0f, fork.Z) + Polar(angle, H * k.R(0.2f, 0.3f), canopy - H * 0.04f);
        const Vector3 elbow = fork + (tip - fork) * 0.55f + Vector3(0.0f, H * 0.06f, 0.0f);
        Limb(bark, fork, elbow, 0.16f, 0.1f);
        Limb(bark, elbow, tip, 0.1f, 0.04f, 5);
        const float radius = H * k.R(0.22f, 0.3f);
        leaves.Blob(tip + Vector3(0.0f, H * 0.035f, 0.0f), Vector3(radius, H * 0.055f, radius), k.Seed(),
                    0.16f, 10, 5);
    }
    const float radius = H * 0.3f;
    leaves.Blob(Vector3(fork.X, canopy + H * 0.05f, fork.Z), Vector3(radius, H * 0.07f, radius), k.Seed(),
                0.16f, 10, 5);
}

void Baobab(Maker& k) {
    MeshWriter& bark = k.m.Use({0.4f, 0.34f, 0.3f}, Pattern::Bark);
    MeshWriter& leaves = k.Leaves();
    const float H = k.H;
    const float girth = k.def.radiusM * 0.95f;
    bark.Tube(Vector3(0.0f, -0.4f, 0.0f), Vector3(0.0f, H * 0.08f, 0.0f), girth * 1.15f, girth, 10, false,
              false);
    bark.Tube(Vector3(0.0f, H * 0.08f, 0.0f), Vector3(0.0f, H * 0.4f, 0.0f), girth, girth * 0.9f, 10, false,
              false);
    const Vector3 shoulder(0.0f, H * 0.6f, 0.0f);
    bark.Tube(Vector3(0.0f, H * 0.4f, 0.0f), shoulder, girth * 0.9f, girth * 0.55f, 10, false, true);
    const int limbs = 7;
    const float start = k.R(0.0f, 2.0f * kPi);
    for (int i = 0; i < limbs; ++i) {
        const float angle =
            start + 2.0f * kPi * static_cast<float>(i) / static_cast<float>(limbs) + k.R(-0.2f, 0.2f);
        const Vector3 root = shoulder + Polar(angle, girth * 0.3f, -H * 0.03f);
        const Vector3 tip = shoulder + Polar(angle, H * k.R(0.2f, 0.3f), H * k.R(0.14f, 0.3f));
        Limb(bark, root, tip, girth * 0.22f, girth * 0.07f);
        const float radius = H * k.R(0.08f, 0.11f);
        leaves.Blob(tip + Vector3(0.0f, radius * 0.4f, 0.0f),
                    Vector3(radius * 1.3f, radius * 0.75f, radius * 1.3f), k.Seed(), 0.22f, 8, 5);
        // A side twig with its own tuft.
        const Vector3 twig = root + (tip - root) * 0.6f;
        const Vector3 twigTip = twig + Polar(angle + k.R(0.6f, 1.2f), H * 0.1f, H * 0.08f);
        Limb(bark, twig, twigTip, girth * 0.08f, girth * 0.03f, 4);
        leaves.Blob(twigTip, Vector3(radius, radius * 0.6f, radius), k.Seed(), 0.22f, 7, 4);
    }
}

void Palm(Maker& k) {
    MeshWriter& bark = k.m.Use({0.36f, 0.31f, 0.24f}, Pattern::Bark);
    MeshWriter& leaves = k.Leaves();
    const float H = k.H * 0.8f;  // the fronds rise above the trunk
    const float bendAngle = k.R(0.0f, 2.0f * kPi);
    const float bend = k.R(0.05f, 0.16f) * H;
    constexpr int kSegments = 6;
    Vector3 previous(0.0f, -0.4f, 0.0f);
    for (int i = 1; i <= kSegments; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(kSegments);
        const Vector3 next = Polar(bendAngle, bend * t * t, H * t);
        const float r0 = 0.24f - 0.1f * static_cast<float>(i - 1) / static_cast<float>(kSegments);
        const float r1 = 0.24f - 0.1f * t;
        bark.Tube(previous, next, i == 1 ? 0.34f : r0, r1, 7, false, false);
        previous = next;
    }
    const Vector3 top = previous;
    leaves.Blob(top, Vector3(0.4f, 0.45f, 0.4f), k.Seed(), 0.1f, 7, 4);
    const int fronds = 11;
    const float start = k.R(0.0f, 2.0f * kPi);
    for (int i = 0; i < fronds; ++i) {
        const float angle =
            start + 2.0f * kPi * static_cast<float>(i) / static_cast<float>(fronds) + k.R(-0.15f, 0.15f);
        const float length = k.H * k.R(0.26f, 0.33f);
        const bool upper = i % 2 == 0;
        Frond(leaves, top + Vector3(0.0f, 0.2f, 0.0f), angle, length,
              upper ? k.R(0.75f, 1.0f) : k.R(0.2f, 0.45f), upper ? 1.25f : 1.0f, 0.95f);
    }
}

void Bamboo(Maker& k) {
    MeshWriter& cane = k.m.Use(Mix(k.def.color, {0.55f, 0.6f, 0.2f}, 0.45f), Pattern::Plain);
    MeshWriter& leaves = k.Leaves();
    const int culms = 12;
    for (int i = 0; i < culms; ++i) {
        const Vector3 base = Polar(k.R(0.0f, 2.0f * kPi), k.def.radiusM * std::sqrt(k.R(0.0f, 0.8f)), -0.3f);
        const float tall = k.H * k.R(0.65f, 1.0f);
        const Vector3 tip = base + Polar(k.R(0.0f, 2.0f * kPi), tall * k.R(0.03f, 0.16f), tall);
        cane.Tube(base, tip, 0.045f, 0.02f, 5, false, false);
        for (int j = 0; j < 2; ++j) {
            const float t = j == 0 ? k.R(0.55f, 0.75f) : k.R(0.82f, 0.98f);
            const Vector3 c = base + (tip - base) * t + Polar(k.R(0.0f, 2.0f * kPi), 0.25f);
            leaves.Blob(c, Vector3(k.R(0.45f, 0.7f), k.R(0.7f, 1.1f), k.R(0.45f, 0.7f)), k.Seed(), 0.25f, 6,
                        4);
        }
    }
}

// ---- bushes and rocks -----------------------------------------------------------------

void Bush(Maker& k, MeshWriter& foliage, int blobs, float spread, float squash, bool twigs) {
    const float H = k.H;
    for (int i = 0; i < blobs; ++i) {
        const float angle = k.R(0.0f, 2.0f * kPi);
        const float distance = i == 0 ? 0.0f : spread * k.R(0.5f, 1.0f);
        const float tall = H * (i == 0 ? 0.5f : k.R(0.3f, 0.45f)) * squash;
        const float wide = k.def.radiusM * k.R(0.75f, 1.1f);
        foliage.Blob(Polar(angle, distance, tall * 0.8f), Vector3(wide, tall, wide), k.Seed(), 0.25f, 8, 5,
                     0.75f);
    }
    if (twigs) {
        MeshWriter& bark = k.Bark();
        for (int i = 0; i < 5; ++i) {
            const float angle = k.R(0.0f, 2.0f * kPi);
            Limb(bark, Polar(angle, 0.1f, -0.1f), Polar(angle, k.R(0.4f, 0.9f), H * k.R(0.6f, 1.0f)), 0.035f,
                 0.012f, 4);
        }
    }
}

void Fern(Maker& k) {
    MeshWriter& leaves = k.Leaves();
    const int fronds = 10;
    const float start = k.R(0.0f, 2.0f * kPi);
    for (int i = 0; i < fronds; ++i) {
        const float angle =
            start + 2.0f * kPi * static_cast<float>(i) / static_cast<float>(fronds) + k.R(-0.2f, 0.2f);
        const bool upper = i % 2 == 0;
        Frond(leaves, Vector3(0.0f, 0.05f, 0.0f), angle, k.H * k.R(0.7f, 0.95f), upper ? 1.3f : 0.7f,
              upper ? 1.3f : 0.9f, 0.5f);
    }
    leaves.Blob(Vector3(0.0f, 0.15f, 0.0f), Vector3(0.3f, 0.3f, 0.3f), k.Seed(), 0.2f, 6, 4, 0.5f);
}

void Rocks(Maker& k, Pattern pattern, float squash, float lumpiness, int extra) {
    MeshWriter& stone = k.m.Use(k.def.color, pattern);
    const float r = k.def.radiusM;
    const float H = k.H * squash;
    stone.Blob(Vector3(0.0f, H * 0.3f, 0.0f), Vector3(r * k.R(0.85f, 1.0f), H * 0.7f, r * k.R(0.7f, 0.95f)),
               k.Seed(), lumpiness, 9, 6, 0.45f);
    for (int i = 0; i < extra; ++i) {
        const float angle = k.R(0.0f, 2.0f * kPi);
        const float size = r * k.R(0.35f, 0.55f);
        stone.Blob(Polar(angle, r * k.R(0.6f, 0.85f), size * 0.25f),
                   Vector3(size, size * k.R(0.6f, 0.9f) * squash, size), k.Seed(), lumpiness, 7, 5, 0.4f);
    }
}

/// A lying log with cut ends.
void Log(Maker& k, const Vector3& from, const Vector3& to, float radius) {
    MeshWriter& bark = k.Bark();
    bark.Tube(from, to, radius, radius * 0.9f, 8, false, false);
    MeshWriter& cut = k.m.Use(kCutWood, Pattern::Wood);
    const Vector3 axis = Vector3::Normalize(to - from);
    cut.Tube(to - axis * 0.01f, to, radius * 0.9f, radius * 0.9f, 8, false, true);
    cut.Tube(from + axis * 0.01f, from, radius, radius, 8, false, true);
}

void FallenLog(Maker& k) {
    const float r = 0.32f;
    Log(k, Vector3(-1.4f, r * 0.85f, 0.0f), Vector3(1.4f, r * 0.8f, 0.1f), r);
    MeshWriter& bark = k.Bark();
    Limb(bark, Vector3(0.5f, r, 0.0f), Vector3(0.9f, r + 0.6f, -0.45f), 0.09f, 0.05f, 5);
    Limb(bark, Vector3(-0.6f, r, 0.0f), Vector3(-0.8f, r + 0.35f, 0.5f), 0.08f, 0.05f, 5);
}

void LogPile(Maker& k) {
    const float r = 0.2f;
    const float half = 0.75f;
    for (int row = 0; row < 3; ++row) {
        const int count = 3 - row;
        for (int i = 0; i < count; ++i) {
            const float z = (static_cast<float>(i) - static_cast<float>(count - 1) * 0.5f) * r * 2.05f;
            const float y = r + static_cast<float>(row) * r * 1.75f;
            const float shift = k.R(-0.08f, 0.08f);
            Log(k, Vector3(-half + shift, y, z), Vector3(half + shift, y, z), r * k.R(0.88f, 1.0f));
        }
    }
}

// ---- feeders and troughs --------------------------------------------------------------

/// A table on four legs; returns the writer of the top.
void Table(Maker& k, float sizeX, float sizeZ, float height, float leg = 0.1f) {
    MeshWriter& legs = k.Timber();
    for (int i = 0; i < 4; ++i) {
        const float x = (i % 2 == 0 ? -1.0f : 1.0f) * (sizeX * 0.5f - leg);
        const float z = (i < 2 ? -1.0f : 1.0f) * (sizeZ * 0.5f - leg);
        legs.BoxAt(x, z, -0.3f, leg, height + 0.3f, leg);
    }
    k.Planks().BoxAt(0.0f, 0.0f, height, sizeX, 0.06f, sizeZ);
}

void MeatPlatform(Maker& k) {
    Table(k, 1.3f, 1.3f, 0.75f, 0.12f);
    MeshWriter& rim = k.Timber();
    rim.BoxAt(0.0f, 0.62f, 0.81f, 1.3f, 0.07f, 0.06f);
    rim.BoxAt(0.0f, -0.62f, 0.81f, 1.3f, 0.07f, 0.06f);
    // What is left of the last meal.
    MeshWriter& meat = k.m.Use({0.42f, 0.09f, 0.07f}, Pattern::Plain);
    meat.Blob(Vector3(0.15f, 0.93f, 0.05f), Vector3(0.3f, 0.13f, 0.2f), k.Seed(), 0.2f, 7, 4);
    MeshWriter& bone = k.m.Use({0.8f, 0.76f, 0.66f}, Pattern::Plain);
    Limb(bone, Vector3(-0.4f, 0.86f, -0.2f), Vector3(-0.05f, 0.86f, -0.35f), 0.03f, 0.03f, 5);
    bone.Blob(Vector3(-0.4f, 0.86f, -0.2f), Vector3(0.055f, 0.05f, 0.055f), 1, 0.0f, 5, 3);
    bone.Blob(Vector3(-0.05f, 0.86f, -0.35f), Vector3(0.055f, 0.05f, 0.055f), 2, 0.0f, 5, 3);
}

void HayRack(Maker& k) {
    MeshWriter& timber = k.Timber();
    for (int i = 0; i < 4; ++i) {
        const float x = (i % 2 == 0 ? -0.7f : 0.7f);
        const float z = (i < 2 ? -0.45f : 0.45f);
        timber.BoxAt(x, z, -0.3f, 0.1f, 2.3f, 0.1f);
    }
    // The rack: a V of rails holding the hay.
    for (int side = -1; side <= 1; side += 2) {
        const float s = static_cast<float>(side);
        for (int i = 0; i < 6; ++i) {
            const float x = -0.6f + 0.24f * static_cast<float>(i);
            Limb(timber, Vector3(x, 0.75f, s * 0.08f), Vector3(x, 1.5f, s * 0.42f), 0.02f, 0.02f, 4);
        }
        timber.Beam(Vector3(-0.7f, 1.5f, s * 0.43f), Vector3(0.7f, 1.5f, s * 0.43f), 0.06f, 0.06f);
    }
    timber.Beam(Vector3(-0.7f, 0.75f, 0.0f), Vector3(0.7f, 0.75f, 0.0f), 0.2f, 0.06f);
    MeshWriter& hay = k.m.Use(kHay, Pattern::Thatch);
    hay.Blob(Vector3(0.0f, 1.25f, 0.0f), Vector3(0.62f, 0.42f, 0.3f), k.Seed(), 0.25f, 8, 5);
    // A little roof keeps the hay dry.
    MeshWriter& roof = k.m.Use({0.3f, 0.2f, 0.14f}, Pattern::Planks);
    roof.GableRoof(-0.95f, 0.95f, -0.7f, 0.7f, 2.0f, 2.4f, true, 0.05f);
}

void Trough(Maker& k) {
    const bool water = k.def.role == zoo::PropRole::WaterTrough;
    if (water) {
        MeshWriter& tub = k.m.Use(k.def.color, Pattern::Metal);
        Tub(tub, 0.12f, 1.7f, 0.5f, 0.75f, 0.05f);
        MeshWriter& feet = k.m.Use(kDarkSteel, Pattern::Metal);
        feet.BoxAt(-0.65f, 0.0f, -0.2f, 0.08f, 0.34f, 0.7f);
        feet.BoxAt(0.65f, 0.0f, -0.2f, 0.08f, 0.34f, 0.7f);
        MeshWriter& surface = k.m.Use(kWater, Pattern::Metal);
        surface.Quad(Vector3(-0.8f, 0.52f, 0.33f), Vector3(0.8f, 0.52f, 0.33f), Vector3(0.8f, 0.52f, -0.33f),
                     Vector3(-0.8f, 0.52f, -0.33f));
        return;
    }
    MeshWriter& tub = k.m.Use(k.def.color, Pattern::Planks);
    Tub(tub, 0.3f, 1.5f, 0.32f, 0.6f, 0.05f);
    MeshWriter& legs = k.Timber();
    for (int i = 0; i < 4; ++i) {
        legs.BoxAt(i % 2 == 0 ? -0.62f : 0.62f, i < 2 ? -0.22f : 0.22f, -0.3f, 0.09f, 0.62f, 0.09f);
    }
    // Pellets with a few pieces of fruit.
    MeshWriter& feed = k.m.Use({0.45f, 0.33f, 0.16f}, Pattern::Concrete);
    feed.Quad(Vector3(-0.7f, 0.55f, 0.25f), Vector3(0.7f, 0.55f, 0.25f), Vector3(0.7f, 0.55f, -0.25f),
              Vector3(-0.7f, 0.55f, -0.25f));
    const zoo::Color3 fruit[3] = {{0.7f, 0.12f, 0.08f}, {0.85f, 0.5f, 0.06f}, {0.4f, 0.6f, 0.1f}};
    for (int i = 0; i < 6; ++i) {
        MeshWriter& piece = k.m.Use(fruit[i % 3], Pattern::Plain);
        piece.Blob(Vector3(-0.55f + 0.22f * static_cast<float>(i), 0.58f, k.R(-0.15f, 0.15f)),
                   Vector3(0.07f, 0.06f, 0.07f), k.Seed(), 0.05f, 6, 4);
    }
}

void HighBrowse(Maker& k) {
    // A mast with a basket of leafy branches at giraffe height.
    const float top = std::max(4.2f, k.def.feedHeightM + 0.4f);
    MeshWriter& timber = k.Timber();
    timber.Tube(Vector3(0.0f, -0.4f, 0.0f), Vector3(0.0f, top, 0.0f), 0.13f, 0.1f, 8, false, true);
    timber.Beam(Vector3(-0.7f, top - 0.25f, 0.0f), Vector3(0.7f, top - 0.25f, 0.0f), 0.1f, 0.1f);
    MeshWriter& steel = k.Steel();
    MeshWriter& leaves = k.m.Use({0.2f, 0.4f, 0.14f}, Pattern::Leaves);
    for (int side = -1; side <= 1; side += 2) {
        const float x = 0.62f * static_cast<float>(side);
        Limb(steel, Vector3(x, top - 0.25f, 0.0f), Vector3(x, top - 0.6f, 0.0f), 0.012f, 0.012f, 4);
        Ring(steel, Vector3(x, 0.0f, 0.0f), 0.3f, top - 1.15f, top - 0.6f, 0.03f, 8);
        leaves.Blob(Vector3(x, top - 0.75f, 0.0f), Vector3(0.42f, 0.5f, 0.42f), k.Seed(), 0.3f, 7, 5);
    }
}

void FishBucket(Maker& k) {
    Table(k, 0.9f, 0.6f, 0.5f, 0.08f);
    MeshWriter& steel = k.Steel();
    steel.Tube(Vector3(0.0f, 0.56f, 0.0f), Vector3(0.0f, 0.9f, 0.0f), 0.17f, 0.21f, 10, false, false);
    Ring(steel, Vector3(0.0f, 0.0f, 0.0f), 0.2f, 0.56f, 0.9f, 0.02f, 10);
    MeshWriter& fish = k.m.Use({0.5f, 0.56f, 0.6f}, Pattern::Metal);
    fish.Blob(Vector3(0.0f, 0.86f, 0.0f), Vector3(0.17f, 0.06f, 0.17f), k.Seed(), 0.3f, 7, 3);
}

void InsectFeeder(Maker& k) {
    // A hollow log on a post: the animals fish the insects out of its holes.
    MeshWriter& timber = k.Timber();
    timber.Tube(Vector3(0.0f, -0.3f, 0.0f), Vector3(0.0f, 0.55f, 0.0f), 0.07f, 0.07f, 6, false, false);
    Log(k, Vector3(-0.5f, 0.75f, 0.0f), Vector3(0.5f, 0.75f, 0.0f), 0.2f);
    MeshWriter& hole = k.m.Use({0.03f, 0.02f, 0.02f}, Pattern::Plain);
    for (int i = 0; i < 5; ++i) {
        const float x = -0.36f + 0.18f * static_cast<float>(i);
        const float angle = 0.5f + 0.5f * static_cast<float>(i % 3);
        const Vector3 c(x, 0.75f + std::sin(angle) * 0.2f, std::cos(angle) * 0.2f);
        hole.Blob(c, Vector3(0.035f, 0.035f, 0.035f), 3, 0.0f, 5, 3);
    }
}

// ---- enrichment -----------------------------------------------------------------------

void Ball(Maker& k) {
    MeshWriter& ball = k.m.Use(k.def.color, Pattern::Plain);
    ball.Blob(Vector3(0.0f, 0.36f, 0.0f), Vector3(0.36f, 0.36f, 0.36f), 1, 0.0f, 12, 8);
}

void ScratchingPost(Maker& k) {
    MeshWriter& base = k.Timber();
    base.BoxAt(0.0f, 0.0f, -0.05f, 0.9f, 0.13f, 0.9f);
    MeshWriter& post = k.m.Use(k.def.color, Pattern::Bark);
    post.Tube(Vector3(0.0f, 0.08f, 0.0f), Vector3(0.0f, 1.9f, 0.0f), 0.17f, 0.15f, 9, false, true);
    MeshWriter& rope = k.m.Use(kRope, Pattern::Thatch);
    rope.Tube(Vector3(0.0f, 0.55f, 0.0f), Vector3(0.0f, 1.35f, 0.0f), 0.19f, 0.19f, 9, true, true);
}

void ClimbingFrame(Maker& k) {
    MeshWriter& timber = k.m.Use(k.def.color, Pattern::Wood);
    const float w = 1.0f;
    const float tall[4] = {2.6f, 2.6f, 1.6f, 1.6f};
    const Vector3 corner[4] = {{-w, 0.0f, -w}, {w, 0.0f, -w}, {w, 0.0f, w}, {-w, 0.0f, w}};
    for (int i = 0; i < 4; ++i) {
        timber.Tube(corner[i] - Vector3(0.0f, 0.4f, 0.0f), corner[i] + Vector3(0.0f, tall[i], 0.0f), 0.1f,
                    0.09f, 7, false, true);
    }
    // Two decks and the beams that carry them.
    k.Planks().BoxAt(0.0f, -w * 0.45f, 2.1f, 2.3f, 0.07f, 1.2f);
    k.Planks().BoxAt(0.0f, w * 0.5f, 1.15f, 2.3f, 0.07f, 1.1f);
    timber.Beam(corner[0] + Vector3(0.0f, 2.05f, 0.0f), corner[1] + Vector3(0.0f, 2.05f, 0.0f), 0.09f, 0.09f);
    timber.Beam(corner[3] + Vector3(0.0f, 1.1f, 0.0f), corner[2] + Vector3(0.0f, 1.1f, 0.0f), 0.09f, 0.09f);
    timber.Beam(corner[0] + Vector3(0.0f, 1.1f, 0.0f), corner[3] + Vector3(0.0f, 1.1f, 0.0f), 0.09f, 0.09f);
    timber.Beam(corner[1] + Vector3(0.0f, 1.1f, 0.0f), corner[2] + Vector3(0.0f, 1.1f, 0.0f), 0.09f, 0.09f);
    // A ramp up to the lower deck and a hanging rope.
    k.Planks().Beam(Vector3(0.0f, 0.0f, 2.3f), Vector3(0.0f, 1.15f, 1.0f), 0.7f, 0.06f);
    MeshWriter& rope = k.m.Use(kRope, Pattern::Thatch);
    Limb(rope, Vector3(0.4f, 2.1f, -0.1f), Vector3(0.45f, 0.5f, -0.05f), 0.03f, 0.03f, 5);
    rope.Blob(Vector3(0.45f, 0.45f, -0.05f), Vector3(0.08f, 0.1f, 0.08f), 4, 0.0f, 5, 3);
}

void TyreSwing(Maker& k) {
    MeshWriter& timber = k.Timber();
    const float top = 2.6f;
    for (int side = -1; side <= 1; side += 2) {
        const float x = 1.1f * static_cast<float>(side);
        Limb(timber, Vector3(x, -0.4f, -0.7f), Vector3(x * 0.9f, top, 0.0f), 0.08f, 0.07f);
        Limb(timber, Vector3(x, -0.4f, 0.7f), Vector3(x * 0.9f, top, 0.0f), 0.08f, 0.07f);
    }
    timber.Tube(Vector3(-1.15f, top, 0.0f), Vector3(1.15f, top, 0.0f), 0.08f, 0.08f, 7, true, true);
    MeshWriter& chain = k.Steel();
    Limb(chain, Vector3(-0.25f, top, 0.0f), Vector3(-0.3f, 0.95f, 0.0f), 0.012f, 0.012f, 4);
    Limb(chain, Vector3(0.25f, top, 0.0f), Vector3(0.3f, 0.95f, 0.0f), 0.012f, 0.012f, 4);
    MeshWriter& tyre = k.m.Use(k.def.color, Pattern::Plain);
    Ring(tyre, Vector3(0.0f, 0.0f, 0.0f), 0.42f, 0.75f, 0.95f, 0.17f, 12);
    for (int i = 0; i < 12; ++i) {  // the tyre's underside
        const float a0 = 2.0f * kPi * static_cast<float>(i) / 12.0f;
        const float a1 = 2.0f * kPi * static_cast<float>(i + 1) / 12.0f;
        tyre.Quad(Polar(a0, 0.42f, 0.75f), Polar(a1, 0.42f, 0.75f), Polar(a1, 0.25f, 0.75f),
                  Polar(a0, 0.25f, 0.75f));
    }
}

void TermiteMound(Maker& k) {
    MeshWriter& earth = k.m.Use(k.def.color, Pattern::Plaster);
    earth.Blob(Vector3(0.0f, 0.5f, 0.0f), Vector3(0.6f, 1.5f, 0.6f), k.Seed(), 0.3f, 9, 7, 0.35f);
    earth.Blob(Vector3(0.35f, 0.3f, 0.2f), Vector3(0.32f, 0.9f, 0.32f), k.Seed(), 0.3f, 7, 5, 0.35f);
    earth.Blob(Vector3(-0.3f, 0.25f, -0.25f), Vector3(0.28f, 0.7f, 0.28f), k.Seed(), 0.3f, 7, 5, 0.35f);
}

void IceBlock(Maker& k) {
    MeshWriter& ice = k.m.Use(k.def.color, Pattern::Metal);
    ice.BoxAt(0.0f, 0.0f, -0.03f, 0.9f, 0.6f, 0.65f, 0.2f);
    ice.BoxAt(0.15f, 0.1f, 0.57f, 0.5f, 0.3f, 0.45f, -0.4f);
    // Fish frozen into it.
    MeshWriter& fish = k.m.Use({0.45f, 0.5f, 0.55f}, Pattern::Plain);
    fish.Blob(Vector3(-0.1f, 0.3f, 0.34f), Vector3(0.16f, 0.05f, 0.02f), 5, 0.0f, 6, 3);
}

// ---- for visitors ---------------------------------------------------------------------

void Bench(Maker& k) {
    MeshWriter& slats = k.m.Use(k.def.color, Pattern::Wood);
    for (int i = 0; i < 4; ++i) {
        slats.BoxAt(0.0f, -0.16f + 0.11f * static_cast<float>(i), 0.43f, 1.5f, 0.035f, 0.09f);
    }
    for (int i = 0; i < 3; ++i) {
        const float y = 0.6f + 0.13f * static_cast<float>(i);
        slats.Beam(Vector3(-0.75f, y, -0.22f - 0.03f * static_cast<float>(i)),
                   Vector3(0.75f, y, -0.22f - 0.03f * static_cast<float>(i)), 0.03f, 0.1f);
    }
    MeshWriter& iron = k.m.Use(kDarkSteel, Pattern::Metal);
    for (int side = -1; side <= 1; side += 2) {
        const float x = 0.62f * static_cast<float>(side);
        iron.BoxAt(x, 0.17f, -0.2f, 0.05f, 0.63f, 0.05f);
        iron.BoxAt(x, -0.2f, -0.2f, 0.05f, 1.15f, 0.05f);
        iron.Beam(Vector3(x, 0.41f, -0.22f), Vector3(x, 0.41f, 0.2f), 0.05f, 0.04f);
        iron.Beam(Vector3(x, 0.64f, -0.2f), Vector3(x, 0.64f, 0.2f), 0.05f, 0.03f);  // arm rest
    }
}

void PicnicTable(Maker& k) {
    MeshWriter& wood = k.m.Use(k.def.color, Pattern::Planks);
    wood.BoxAt(0.0f, 0.0f, 0.72f, 1.8f, 0.05f, 0.8f);
    wood.BoxAt(0.0f, -0.72f, 0.42f, 1.8f, 0.05f, 0.28f);
    wood.BoxAt(0.0f, 0.72f, 0.42f, 1.8f, 0.05f, 0.28f);
    MeshWriter& frame = k.Timber();
    for (int side = -1; side <= 1; side += 2) {
        const float x = 0.7f * static_cast<float>(side);
        frame.Beam(Vector3(x, -0.3f, -0.8f), Vector3(x, 0.72f, -0.22f), 0.06f, 0.1f);
        frame.Beam(Vector3(x, -0.3f, 0.8f), Vector3(x, 0.72f, 0.22f), 0.06f, 0.1f);
        frame.Beam(Vector3(x, 0.4f, -0.85f), Vector3(x, 0.4f, 0.85f), 0.06f, 0.08f);
    }
}

void Bin(Maker& k) {
    MeshWriter& body = k.m.Use(k.def.color, Pattern::Metal);
    body.Tube(Vector3(0.0f, -0.1f, 0.0f), Vector3(0.0f, 0.8f, 0.0f), 0.24f, 0.27f, 10, false, true);
    MeshWriter& dark = k.m.Use(kDarkSteel, Pattern::Metal);
    dark.Tube(Vector3(0.0f, 0.8f, 0.0f), Vector3(0.0f, 0.86f, 0.0f), 0.29f, 0.29f, 10, true, true);
    dark.Disc(Vector3(0.0f, 0.865f, 0.0f), 0.17f, 10);
    // A roof against rain on two stays.
    Limb(dark, Vector3(-0.22f, 0.86f, 0.0f), Vector3(-0.22f, 1.05f, 0.0f), 0.015f, 0.015f, 4);
    Limb(dark, Vector3(0.22f, 0.86f, 0.0f), Vector3(0.22f, 1.05f, 0.0f), 0.015f, 0.015f, 4);
    body.Tube(Vector3(0.0f, 1.05f, 0.0f), Vector3(0.0f, 1.14f, 0.0f), 0.31f, 0.05f, 10, true, true);
}

void Lamp(Maker& k) {
    MeshWriter& iron = k.m.Use(k.def.color, Pattern::Metal);
    const float H = k.H;
    iron.Tube(Vector3(0.0f, -0.3f, 0.0f), Vector3(0.0f, 0.5f, 0.0f), 0.09f, 0.06f, 8, false, false);
    iron.Tube(Vector3(0.0f, 0.5f, 0.0f), Vector3(0.0f, H - 0.45f, 0.0f), 0.05f, 0.04f, 8, false, false);
    iron.Tube(Vector3(0.0f, H - 0.45f, 0.0f), Vector3(0.0f, H - 0.38f, 0.0f), 0.04f, 0.14f, 8, false, false);
    MeshWriter& glass = k.m.Use({0.95f, 0.9f, 0.7f}, Pattern::Plain);
    glass.Tube(Vector3(0.0f, H - 0.38f, 0.0f), Vector3(0.0f, H - 0.1f, 0.0f), 0.13f, 0.17f, 8, true, false);
    iron.Tube(Vector3(0.0f, H - 0.1f, 0.0f), Vector3(0.0f, H, 0.0f), 0.21f, 0.03f, 8, true, true);
}

void InfoBoard(Maker& k) {
    MeshWriter& wood = k.m.Use(k.def.color, Pattern::Wood);
    wood.BoxAt(-0.42f, 0.0f, -0.3f, 0.08f, 1.55f, 0.08f);
    wood.BoxAt(0.42f, 0.0f, -0.3f, 0.08f, 1.55f, 0.08f);
    // A sloping reading desk towards +z.
    k.m.SetLocal(Matrix::CreateRotationX(0.6f) * Matrix::CreateTranslation(0.0f, 1.12f, 0.08f));
    wood.Box(Vector3(-0.55f, -0.03f, -0.36f), Vector3(0.55f, 0.03f, 0.36f));
    k.m.Use({0.85f, 0.82f, 0.7f}, Pattern::Plain)
        .Quad(Vector3(-0.5f, 0.035f, 0.31f), Vector3(0.5f, 0.035f, 0.31f), Vector3(0.5f, 0.035f, -0.2f),
              Vector3(-0.5f, 0.035f, -0.2f));
    k.m.Use({0.1f, 0.3f, 0.16f}, Pattern::Plain)
        .Quad(Vector3(-0.5f, 0.035f, -0.2f), Vector3(0.5f, 0.035f, -0.2f), Vector3(0.5f, 0.035f, -0.32f),
              Vector3(-0.5f, 0.035f, -0.32f));
    k.m.ClearLocal();
}

void FlowerBed(Maker& k) {
    const float r = k.def.radiusM;
    MeshWriter& kerb = k.m.Use(kStone, Pattern::Stone);
    Ring(kerb, Vector3(0.0f, 0.0f, 0.0f), r, -0.2f, 0.2f, 0.14f, 14);
    k.m.Use(kSoil, Pattern::Concrete).Disc(Vector3(0.0f, 0.15f, 0.0f), r - 0.12f, 14);
    MeshWriter& green = k.m.Use({0.13f, 0.32f, 0.1f}, Pattern::Leaves);
    const zoo::Color3 petals[3] = {k.def.color, {0.9f, 0.7f, 0.1f}, {0.9f, 0.88f, 0.82f}};
    for (int i = 0; i < 16; ++i) {
        const Vector3 c = Polar(k.R(0.0f, 2.0f * kPi), (r - 0.3f) * std::sqrt(k.R(0.0f, 1.0f)), 0.0f);
        const float tall = k.R(0.25f, 0.42f);
        green.Blob(c + Vector3(0.0f, 0.15f + tall * 0.4f, 0.0f), Vector3(0.17f, tall * 0.55f, 0.17f),
                   k.Seed(), 0.25f, 6, 4);
        MeshWriter& flower = k.m.Use(petals[i % 3], Pattern::Plain);
        flower.Blob(c + Vector3(0.0f, 0.15f + tall, 0.0f), Vector3(0.11f, 0.07f, 0.11f), k.Seed(), 0.3f, 6,
                    3);
    }
}

void Fountain(Maker& k) {
    const float r = k.def.radiusM;
    MeshWriter& stone = k.m.Use(kStone, Pattern::Stone);
    Ring(stone, Vector3(0.0f, 0.0f, 0.0f), r, -0.3f, 0.5f, 0.2f, 16);
    stone.Disc(Vector3(0.0f, 0.05f, 0.0f), r - 0.1f, 16);
    stone.Tube(Vector3(0.0f, 0.05f, 0.0f), Vector3(0.0f, 1.3f, 0.0f), 0.2f, 0.13f, 10, false, false);
    stone.Tube(Vector3(0.0f, 1.3f, 0.0f), Vector3(0.0f, 1.45f, 0.0f), 0.13f, 0.6f, 12, false, false);
    Ring(stone, Vector3(0.0f, 0.0f, 0.0f), 0.6f, 1.4f, 1.5f, 0.06f, 12);
    stone.Tube(Vector3(0.0f, 1.42f, 0.0f), Vector3(0.0f, 1.9f, 0.0f), 0.07f, 0.05f, 8, false, true);
    MeshWriter& water = k.m.Use(k.def.color, Pattern::Metal);
    water.Disc(Vector3(0.0f, 0.38f, 0.0f), r - 0.19f, 16);
    water.Disc(Vector3(0.0f, 1.47f, 0.0f), 0.54f, 12);
    // The jet and what falls from the bowl.
    MeshWriter& spray = k.m.Use({0.8f, 0.9f, 0.93f}, Pattern::Plain);
    spray.Tube(Vector3(0.0f, 1.9f, 0.0f), Vector3(0.0f, k.H + 0.25f, 0.0f), 0.035f, 0.01f, 5, false, false);
    spray.Blob(Vector3(0.0f, k.H + 0.2f, 0.0f), Vector3(0.14f, 0.1f, 0.14f), k.Seed(), 0.3f, 6, 3);
    for (int i = 0; i < 6; ++i) {
        const float angle = 2.0f * kPi * static_cast<float>(i) / 6.0f;
        Limb(spray, Polar(angle, 0.58f, 1.47f), Polar(angle, 0.66f, 0.4f), 0.02f, 0.035f, 4);
    }
}

void Statue(Maker& k) {
    MeshWriter& plinth = k.m.Use(kStone, Pattern::Stone);
    plinth.BoxAt(0.0f, 0.0f, -0.2f, 1.2f, 0.4f, 1.2f);
    plinth.BoxAt(0.0f, 0.0f, 0.2f, 0.95f, 0.65f, 0.95f);
    plinth.BoxAt(0.0f, 0.0f, 0.85f, 1.05f, 0.08f, 1.05f);
    // A seated big cat in bronze, looking along +z.
    MeshWriter& bronze = k.m.Use({0.3f, 0.2f, 0.09f}, Pattern::Metal);
    const float y = 0.93f;
    bronze.Blob(Vector3(0.0f, y + 0.22f, -0.18f), Vector3(0.24f, 0.24f, 0.3f), 11, 0.05f, 8, 5);  // haunches
    bronze.Blob(Vector3(0.0f, y + 0.5f, 0.05f), Vector3(0.2f, 0.36f, 0.22f), 12, 0.05f, 8, 6);    // chest
    bronze.Blob(Vector3(0.0f, y + 0.9f, 0.14f), Vector3(0.22f, 0.24f, 0.22f), 13, 0.2f, 8, 6);    // mane
    bronze.Blob(Vector3(0.0f, y + 0.92f, 0.3f), Vector3(0.12f, 0.12f, 0.14f), 14, 0.0f, 7, 5);    // muzzle
    Limb(bronze, Vector3(-0.11f, y + 0.5f, 0.2f), Vector3(-0.11f, y, 0.26f), 0.06f, 0.055f);
    Limb(bronze, Vector3(0.11f, y + 0.5f, 0.2f), Vector3(0.11f, y, 0.26f), 0.06f, 0.055f);
    bronze.Blob(Vector3(-0.11f, y + 0.03f, 0.3f), Vector3(0.07f, 0.04f, 0.1f), 15, 0.0f, 6, 3);
    bronze.Blob(Vector3(0.11f, y + 0.03f, 0.3f), Vector3(0.07f, 0.04f, 0.1f), 16, 0.0f, 6, 3);
    Limb(bronze, Vector3(0.0f, y + 0.06f, -0.4f), Vector3(0.3f, y + 0.04f, -0.2f), 0.035f, 0.03f, 5);  // tail
    bronze.Blob(Vector3(-0.13f, y + 1.1f, 0.12f), Vector3(0.045f, 0.06f, 0.03f), 17, 0.0f, 5, 3);
    bronze.Blob(Vector3(0.13f, y + 1.1f, 0.12f), Vector3(0.045f, 0.06f, 0.03f), 18, 0.0f, 5, 3);
}

void Marker(Maker& k) {
    MeshWriter& w = k.m.Use({0.9f, 0.1f, 0.6f}, Pattern::Plain);
    w.BoxAt(0.0f, 0.0f, 0.0f, 0.3f, std::max(0.5f, k.H), 0.3f);
}

}  // namespace

void BuildProp(ModelBuilder& model, const zoo::PropDef& def, std::uint32_t seed) {
    Maker k(model, def, seed);
    const std::string_view look = def.look;
    if (look == "oak") {
        Oak(k);
    } else if (look == "birch") {
        Birch(k);
    } else if (look == "pine") {
        Pine(k);
    } else if (look == "spruce") {
        Spruce(k);
    } else if (look == "acacia") {
        Acacia(k);
    } else if (look == "baobab") {
        Baobab(k);
    } else if (look == "palm") {
        Palm(k);
    } else if (look == "bamboo") {
        Bamboo(k);
    } else if (look == "willow") {
        Willow(k);
    } else if (look == "juniper") {
        Bush(k, k.Needles(), 3, 0.3f, 1.15f, false);
    } else if (look == "shrub") {
        Bush(k, k.Leaves(), 4, 0.45f, 0.9f, true);
    } else if (look == "scrub") {
        Bush(k, k.Leaves(), 2, 0.4f, 0.6f, true);
    } else if (look == "fern") {
        Fern(k);
    } else if (look == "boulder") {
        Rocks(k, Pattern::Stone, 1.0f, 0.24f, 2);
    } else if (look == "rock") {
        Rocks(k, Pattern::Stone, 0.7f, 0.16f, 3);
    } else if (look == "ice_rock") {
        Rocks(k, Pattern::Plaster, 0.9f, 0.3f, 2);
    } else if (look == "log") {
        FallenLog(k);
    } else if (look == "meat_platform") {
        MeatPlatform(k);
    } else if (look == "hay_rack") {
        HayRack(k);
    } else if (look == "trough") {
        Trough(k);
    } else if (look == "high_browse") {
        HighBrowse(k);
    } else if (look == "fish_bucket") {
        FishBucket(k);
    } else if (look == "insect_feeder") {
        InsectFeeder(k);
    } else if (look == "ball") {
        Ball(k);
    } else if (look == "scratching_post") {
        ScratchingPost(k);
    } else if (look == "climbing_frame") {
        ClimbingFrame(k);
    } else if (look == "tyre_swing") {
        TyreSwing(k);
    } else if (look == "log_pile") {
        LogPile(k);
    } else if (look == "termite_mound") {
        TermiteMound(k);
    } else if (look == "ice_block") {
        IceBlock(k);
    } else if (look == "bench") {
        Bench(k);
    } else if (look == "picnic_table") {
        PicnicTable(k);
    } else if (look == "bin") {
        Bin(k);
    } else if (look == "lamp") {
        Lamp(k);
    } else if (look == "info_board") {
        InfoBoard(k);
    } else if (look == "flower_bed") {
        FlowerBed(k);
    } else if (look == "fountain") {
        Fountain(k);
    } else if (look == "statue") {
        Statue(k);
    } else {
        Marker(k);
    }
}

}  // namespace zoogame
