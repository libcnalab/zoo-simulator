// SPDX-License-Identifier: MIT
#include "PlantModels.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <string_view>

#include "FoliageTextures.hpp"
#include "zoo/base/Rng.hpp"

namespace zoogame {

namespace {

constexpr float kPi = std::numbers::pi_v<float>;
constexpr float kGolden = 2.39996323f;  ///< the golden angle: what turns leaf after leaf round a stem

const zoo::Color3 kStone{0.5f, 0.5f, 0.47f};
const zoo::Color3 kSoil{0.16f, 0.11f, 0.07f};
const zoo::Color3 kDryLeaf{0.5f, 0.36f, 0.18f};

Vector3 Up(float y = 1.0f) {
    return Vector3(0.0f, y, 0.0f);
}

Vector3 Polar(float angle, float distance, float y = 0.0f) {
    return Vector3(std::cos(angle) * distance, y, std::sin(angle) * distance);
}

/// What the plants share.
///
/// Both details of a plant are written by the same code drawing the same random numbers in
/// the same order: that is what makes the far one the same individual. So nothing here may
/// draw a number only when near; what differs is decided after the numbers are drawn.
struct Maker {
    ModelBuilder& m;
    const zoo::PropDef& def;
    zoo::Rng rng;
    float H;   ///< this individual's height in metres
    bool far;  ///< PlantDetail::Far
    int run = 0;

    Maker(ModelBuilder& model, const zoo::PropDef& definition, std::uint32_t seed, PlantDetail detail)
        : m(model), def(definition), rng(seed, 5), H(definition.heightM), far(detail == PlantDetail::Far) {}

    float R(float lo, float hi) { return rng.Range(lo, hi); }
    std::uint32_t Seed() { return rng.NextU32(); }
    /// Whether the next of many small things is written: every one near, one in `every` far.
    bool Keep(int every) { return !far || run++ % every == 0; }
    MeshWriter& Bark() { return m.Use(def.color2, Pattern::Bark); }
};

/// What a crown is written into: its dark heart, which hides what is behind it and casts
/// the shadow (cut-out cards cast none), and the sprays of leaves round it.
struct Foliage {
    MeshWriter& heart;
    MeshWriter& sprays;
};

/// The finish of whatever stands for leaves one cannot make out: the inside of a crown, the
/// dense middle of a frond.
MeshWriter& Shade(Maker& k, const zoo::Color3& color) {
    return k.m.Use(Scaled(color, 0.8f), Pattern::LeafShade);
}

Foliage Broadleaves(Maker& k) {
    return {Shade(k, k.def.color), k.m.Use(k.def.color, Pattern::LeafSpray)};
}

Foliage Conifer(Maker& k) {
    return {k.m.Use(Scaled(k.def.color, 0.6f), Pattern::Needles), k.m.Use(k.def.color, Pattern::NeedleSpray)};
}

/// A branch without end caps. Far away it has three sides, and one thinner than an arm is
/// left out.
void Limb(Maker& k, MeshWriter& w, const Vector3& from, const Vector3& to, float r0, float r1,
          int sides = 6) {
    if (k.far && r0 < 0.07f) {
        return;
    }
    w.Tube(from, to, r0, r1, k.far ? 3 : sides, false, false);
}

/// A trunk from its flared foot in the ground up to `top`, wandering a little on the way.
/// `girth` is its radius at breast height, `taper` what is left of it at the top.
void Trunk(Maker& k, MeshWriter& bark, const Vector3& top, float girth, float taper, int sides = 8,
           int pieces = 2, int farSides = 4) {
    std::array<Vector3, 10> points;
    std::array<float, 10> radii;
    std::size_t count = 0;
    const auto add = [&](const Vector3& point, float radius) {
        points[count] = point;
        radii[count++] = radius;
    };
    add(Up(-0.4f), girth * 1.9f);
    if (!k.far) {
        add(Up(0.1f), girth * 1.5f);
        add(Up(0.45f), girth * 1.16f);
    }
    const Vector3 breast(top.X * 0.05f, 1.2f, top.Z * 0.05f);
    add(breast, girth);
    pieces = std::min(pieces, 5);
    for (int i = 1; i <= pieces; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(pieces);
        Vector3 next = breast + (top - breast) * t;
        if (i < pieces) {
            next = next + Vector3(k.R(-1.0f, 1.0f), 0.0f, k.R(-1.0f, 1.0f)) * (girth * 0.55f);
        }
        add(next, girth * (1.0f + (taper - 1.0f) * t));
    }
    bark.Pipe(std::span(points.data(), count), std::span(radii.data(), count), k.far ? farSides : sides);
}

/// The roots that run out of a trunk's foot into the ground.
void Roots(Maker& k, MeshWriter& bark, float girth, int count) {
    const float start = k.R(0.0f, 2.0f * kPi);
    for (int i = 0; i < count; ++i) {
        const float angle =
            start + 2.0f * kPi * static_cast<float>(i) / static_cast<float>(count) + k.R(-0.35f, 0.35f);
        const float reach = girth * k.R(2.3f, 3.4f);
        const float high = k.R(0.4f, 0.75f);
        if (!k.far) {
            bark.Tube(Polar(angle, girth * 0.6f, high), Polar(angle, reach, -0.25f), girth * 0.42f,
                      girth * 0.13f, 5, false, false);
        }
    }
}

/// One spray out of the four of an atlas, sometimes mirrored, on a card `width` by `height`
/// metres centred on `center`; the twig's foot is at `center - tip * height / 2`.
///
/// Far away one card in `every` is written (none for 0), and larger to cover what the others
/// did; of four, twice the size and with all four sprays on it: the same leaves at the same
/// size cover the same area with a quarter of the triangles.
void SprayCard(Maker& k, MeshWriter& sprays, const Vector3& center, const Vector3& side, const Vector3& tip,
               float width, float height, const Vector3& shade, int every = 4) {
    const int cell = static_cast<int>(k.R(0.0f, 3.999f));
    const bool mirror = k.R(0.0f, 1.0f) < 0.5f;
    float u0 = static_cast<float>(cell % 2) * 0.5f;
    float u1 = u0 + 0.5f;
    float v0 = static_cast<float>(cell / 2) * 0.5f;
    float v1 = v0 + 0.5f;
    float grow = 0.5f;
    if (k.far) {
        if (every <= 0 || !k.Keep(every)) {
            return;
        }
        if (every >= 4) {
            u0 = v0 = 0.0f;
            u1 = v1 = 1.0f;
            grow = 0.92f;
        } else {
            grow = 0.5f * std::sqrt(static_cast<float>(every));
        }
    }
    if (mirror) {
        std::swap(u0, u1);
    }
    sprays.Card(center, side * (width * grow), tip * (height * grow), shade, u0, v0, u1, v1);
}

Vector3 AnyDirection(Maker& k) {
    const float y = k.R(-1.0f, 1.0f);
    const float ring = std::sqrt(std::max(0.0f, 1.0f - y * y));
    const float angle = k.R(0.0f, 2.0f * kPi);
    return Vector3(ring * std::cos(angle), y, ring * std::sin(angle));
}

struct CrownStyle {
    float lumpiness = 0.2f;
    float flatBase = 1.0f;  ///< as for MeshWriter::Blob
    float cover = 1.7f;     ///< the cards' area to the ellipsoid's
    float heart = 0.52f;    ///< the heart's radii to the crown's
    bool hanging = false;   ///< the sprays hang
};

/// A mass of foliage filling an ellipsoid: a dark heart and, all over it, sprays on cards of
/// `card` metres, turned every way but mostly outwards, each lit as the place of the crown it
/// sits on. One in five sits deeper, in front of the heart, so that this is not seen bare.
void Crown(Maker& k, const Foliage& f, const Vector3& center, const Vector3& radii, float card,
           const CrownStyle& style = {}) {
    const bool small = std::max(radii.X, radii.Z) < 1.0f;
    const std::uint32_t seed = k.Seed();
    const std::size_t first = f.heart.mesh.normals.size();
    if (k.far) {
        f.heart.Blob(center, radii * style.heart, seed, style.lumpiness, small ? 4 : 5, small ? 2 : 3,
                     style.flatBase);
    } else {
        f.heart.Blob(center, radii * style.heart, seed, style.lumpiness, small ? 6 : 8, small ? 4 : 5,
                     style.flatBase);
    }
    // Lit as the sprays in front of it are (see `shade` below), or its underside would show
    // through them as a black ball.
    for (std::size_t i = first; i < f.heart.mesh.normals.size(); ++i) {
        f.heart.mesh.normals[i] = Vector3::Normalize(f.heart.mesh.normals[i] + Up(0.3f));
    }
    const float a = radii.X, b = radii.Y, c = radii.Z;
    const float area =
        4.0f * kPi *
        std::pow((std::pow(a * b, 1.6f) + std::pow(a * c, 1.6f) + std::pow(b * c, 1.6f)) / 3.0f,
                 1.0f / 1.6f) *
        (1.0f + style.flatBase) * 0.5f;
    const int count = std::max(6, static_cast<int>(std::lround(style.cover * area / (card * card))));
    const float turn = k.R(0.0f, 2.0f * kPi);
    // A far card is twice the size: it sits that much deeper, or the crown would grow.
    const float sink = k.far ? std::min(0.2f, 0.4f * card / std::max(a, c)) : 0.0f;
    for (int i = 0; i < count; ++i) {
        // Evenly over the ellipsoid (a spiral from the top down), then shaken.
        const float y =
            1.0f - (1.0f + style.flatBase) * (static_cast<float>(i) + 0.5f) / static_cast<float>(count);
        const float ring = std::sqrt(std::max(0.0f, 1.0f - y * y));
        const float angle = turn + kGolden * static_cast<float>(i) + k.R(-0.3f, 0.3f);
        const Vector3 dir(ring * std::cos(angle), y, ring * std::sin(angle));
        const float depth = i % 5 == 4 ? k.R(0.52f, 0.7f) : k.R(0.74f, 0.96f);
        const Vector3 at = center + radii * dir * (depth - sink);
        const Vector3 normal = Vector3::Normalize(Vector3(dir.X / a, dir.Y / b, dir.Z / c));
        // The underside of a crown is in its own shade but not black: the light of the sky
        // and of the ground reaches it, which a normal straight down would deny it.
        const Vector3 shade = Vector3::Normalize(normal + Up(0.3f));
        Vector3 face, tip;
        if (style.hanging) {
            const float round = k.R(0.0f, 2.0f * kPi);
            face = Vector3(std::cos(round), 0.0f, std::sin(round));
            tip = Vector3::Normalize(Vector3(k.R(-0.15f, 0.15f), -1.0f, k.R(-0.15f, 0.15f)));
        } else {
            face = Vector3::Normalize(normal * 0.55f + AnyDirection(k));
            tip = normal + AnyDirection(k) * 0.6f;
        }
        tip = tip - face * Vector3::Dot(tip, face);
        if (tip.LengthSquared() < 1e-4f) {
            tip = Vector3::Cross(face, Vector3(0.3f, 0.9f, 0.1f));
        }
        tip = Vector3::Normalize(tip);
        const float size = card * k.R(0.8f, 1.2f);
        SprayCard(k, f.sprays, at, Vector3::Cross(tip, face), tip, size, size, shade);
    }
}

/// A spray hanging from `top`, `width` across and `length` down: a birch's twig ends, a
/// willow's curtain. Stretched lengthways the leaves grow long and narrow, which is what a
/// willow's are.
void HangingSpray(Maker& k, MeshWriter& sprays, const Vector3& top, float facing, float width, float length,
                  const Vector3& shade, int every = 2) {
    const Vector3 side = Polar(facing, 1.0f);
    const Vector3 down = Vector3::Normalize(Vector3(k.R(-0.1f, 0.1f), -1.0f, k.R(-0.1f, 0.1f)));
    SprayCard(k, sprays, top + down * (length * 0.5f), side, down, width, length, shade, every);
}

/// A picture of the plant-card atlas standing on `foot`: `width` across (along `side`) and
/// `height` towards `tip`.
void PlantCardAt(MeshWriter& cards, PlantCard picture, const Vector3& foot, const Vector3& side,
                 const Vector3& tip, float width, float height, const Vector3& shade, bool mirror) {
    const CardRect r = RectOf(picture);
    cards.Card(foot + tip * (height * 0.5f), side * (width * 0.5f), tip * (height * 0.5f), shade,
               mirror ? r.u1 : r.u0, r.v0, mirror ? r.u0 : r.u1, r.v1);
}

/// The line of an arching leaf: out of `base` towards `out` (level, unit) at `elevation`,
/// bending down by `droop` times its length at the tip.
struct Arch {
    Vector3 base;
    Vector3 out;
    float length = 1.0f;
    float elevation = 0.5f;
    float droop = 0.5f;

    Vector3 At(float t) const {
        const float s = t * length;
        return base + out * (s * std::cos(elevation)) + Up(s * std::sin(elevation) - droop * s * t);
    }
};

/// A frond of the plant-card atlas laid along an arch: two rows of cards, one each side of
/// the midrib, which is the middle of the picture. `keel` tilts the two halves up from the
/// midrib (a young frond's V) or, negative, lets them hang. Under the cards, when `under` is
/// given, runs an opaque strip a fifth as wide: seen from below it is the dense middle of the
/// frond, and it is what casts the frond's shadow.
void FrondStrip(Maker& k, MeshWriter& cards, MeshWriter* under, PlantCard picture, const Arch& arch,
                float width, float keel, int segments) {
    const CardRect r = RectOf(picture);
    const float uMid = (r.u0 + r.u1) * 0.5f;
    const Vector3 side = Vector3::Cross(Up(), arch.out);
    const Vector3 left = (side * -std::cos(keel) + Up(std::sin(keel))) * (width * 0.5f);
    const Vector3 right = (side * std::cos(keel) + Up(std::sin(keel))) * (width * 0.5f);
    const int n = k.far ? std::max(2, segments / 3) : segments;
    // How wide the frond is at `t` of its length, as a share of its widest.
    const auto wide = [](float t) { return 0.2f * std::pow(std::sin(kPi * std::pow(t, 0.6f)), 0.8f); };
    for (int i = 0; i < n; ++i) {
        const float t0 = static_cast<float>(i) / static_cast<float>(n);
        const float t1 = static_cast<float>(i + 1) / static_cast<float>(n);
        const Vector3 p0 = arch.At(t0), p1 = arch.At(t1);
        const Vector3 mid = (p0 + p1) * 0.5f, half = (p1 - p0) * 0.5f;
        const Vector3 shade = Vector3::Normalize(Vector3::Normalize(Vector3::Cross(half, side)) + Up(0.6f));
        const float vTip = r.v1 + (r.v0 - r.v1) * t1;
        const float vFoot = r.v1 + (r.v0 - r.v1) * t0;
        cards.Card(mid + left * 0.5f, left * -0.5f, half, shade, r.u0, vTip, uMid, vFoot);
        cards.Card(mid + right * 0.5f, right * 0.5f, half, shade, uMid, vTip, r.u1, vFoot);
        if (under != nullptr) {
            const Vector3 sunk = Up(-0.015f);
            const float w0 = std::max(0.02f, wide(t0)), w1 = i + 1 == n ? 0.0f : std::max(0.02f, wide(t1));
            under->QuadTwoSided(p0 + sunk, p1 + sunk, p1 + sunk + left * w1, p0 + sunk + left * w0);
            under->QuadTwoSided(p1 + sunk, p0 + sunk, p0 + sunk + right * w0, p1 + sunk + right * w1);
        }
    }
}

/// A scaffold limb from `from` by way of an elbow to `to`: it rises first and reaches after,
/// as a limb that has carried its own weight for decades does.
void Scaffold(Maker& k, MeshWriter& bark, const Vector3& from, const Vector3& to, float radius, float rise) {
    const Vector3 run = to - from;
    const Vector3 elbow = from + run * k.R(0.42f, 0.58f) + Up(rise) +
                          Vector3(k.R(-1.0f, 1.0f), 0.0f, k.R(-1.0f, 1.0f)) * (run.Length() * 0.07f);
    if (k.far && radius < 0.07f) {
        return;
    }
    const std::array<Vector3, 3> points{from, elbow, to};
    const std::array<float, 3> radii{radius, radius * 0.62f, radius * 0.22f};
    bark.Pipe(points, radii, k.far ? 3 : 6);
}

/// Dry leaves caught around the foot of a broadleaf tree. Every random value is still consumed for
/// the far form, but leaves this small are omitted there: at the 48-56 m switch they are below a
/// pixel.
void FallenLeaves(Maker& k) {
    MeshWriter& leaves = k.m.Use(Scaled(kDryLeaf, 0.65f), Pattern::Leaves);
    constexpr int count = 32;
    const float reach = std::clamp(k.def.radiusM * 1.8f, 1.15f, 2.4f);
    for (int i = 0; i < count; ++i) {
        const float around = k.R(0.0f, 2.0f * kPi);
        const float out = reach * std::sqrt(k.R(0.08f, 1.0f));
        const float turn = k.R(0.0f, 2.0f * kPi);
        const float length = k.R(0.18f, 0.34f);
        const float width = length * k.R(0.36f, 0.55f);
        const float lift = k.R(0.012f, 0.032f);
        const float fold = k.R(0.0f, 0.018f);
        if (k.far) {
            continue;
        }
        const Vector3 center = Polar(around, out, lift);
        const Vector3 along = Polar(turn, length * 0.5f);
        const Vector3 across = Polar(turn + kPi * 0.5f, width * 0.5f);
        // Counter-clockwise from above: one cheap, gently folded leaf lying on the ground.
        leaves.Quad(center - along, center + across + Up(fold), center + along,
                    center - across + Up(fold));
    }
}

/// Broad and heavy: a short thick trunk, limbs that reach out almost level, and a crown
/// wider than it is high.
void Oak(Maker& k) {
    MeshWriter& bark = k.Bark();
    const Foliage leaves = Broadleaves(k);
    const float H = k.H * k.R(0.88f, 1.1f);
    const float girth = k.R(0.38f, 0.5f);
    const Vector3 fork = Polar(k.R(0.0f, 2.0f * kPi), H * k.R(0.0f, 0.035f), H * k.R(0.24f, 0.32f));
    Trunk(k, bark, fork, girth, 0.8f, 9, 2);
    Roots(k, bark, girth, 5);

    const float reach = H * k.R(0.38f, 0.46f);
    const float tall = H * k.R(0.2f, 0.25f);
    const Vector3 heart = fork + Up(tall * 1.1f);
    Limb(k, bark, fork, heart + Up(tall * 0.2f), girth * 0.62f, girth * 0.18f);
    Crown(k, leaves, heart + Up(tall * 0.3f), Vector3(reach * 0.62f, tall * 0.85f, reach * 0.62f), 1.7f,
          {.lumpiness = 0.24f, .cover = 2.0f});
    const int limbs = 6 + static_cast<int>(k.Seed() % 2u);
    const float start = k.R(0.0f, 2.0f * kPi);
    for (int i = 0; i < limbs; ++i) {
        const float angle =
            start + 2.0f * kPi * static_cast<float>(i) / static_cast<float>(limbs) + k.R(-0.3f, 0.3f);
        const float radius = reach * k.R(0.36f, 0.5f);
        const Vector3 c = heart + Polar(angle, reach - radius * k.R(0.75f, 1.0f), tall * k.R(-0.3f, 0.45f));
        Scaffold(k, bark, fork - Up(k.R(0.0f, 0.1f) * H), c - Up(radius * 0.25f), girth * k.R(0.34f, 0.46f),
                 H * k.R(0.0f, 0.05f));
        Crown(k, leaves, c, Vector3(radius, radius * k.R(0.6f, 0.76f), radius), 1.7f,
              {.lumpiness = 0.24f, .cover = 2.0f});
    }
}

/// Light and hanging: a slender white stem to the very top, thin ascending branches, small
/// airy masses of leaves with twig ends hanging under them.
void Birch(Maker& k) {
    MeshWriter& bark = k.m.Use({0.86f, 0.85f, 0.8f}, Pattern::BirchBark);
    const Foliage leaves = Broadleaves(k);
    const float H = k.H * k.R(0.86f, 1.12f);
    const float girth = k.R(0.11f, 0.17f);
    const Vector3 top = Polar(k.R(0.0f, 2.0f * kPi), H * k.R(0.0f, 0.06f), H * 0.94f);
    Trunk(k, bark, top, girth, 0.18f, 7, 3);
    const Vector3 breast(0.0f, 1.2f, 0.0f);

    const int branches = 9 + static_cast<int>(k.Seed() % 3u);
    const float start = k.R(0.0f, 2.0f * kPi);
    const float lowest = k.R(0.36f, 0.48f);
    const CrownStyle airy{.lumpiness = 0.3f, .cover = 2.0f, .heart = 0.34f};
    for (int i = 0; i < branches; ++i) {
        const float t = lowest + (0.95f - lowest) * static_cast<float>(i) / static_cast<float>(branches - 1);
        const Vector3 from = breast + (top - breast) * t;
        const float angle = start + kGolden * static_cast<float>(i) + k.R(-0.3f, 0.3f);
        const float length = H * (0.19f - 0.11f * t) * k.R(0.8f, 1.2f);
        const float elevation = k.R(0.55f, 0.95f);
        const Vector3 tip = from + Polar(angle, length * std::cos(elevation), length * std::sin(elevation));
        Limb(k, bark, from, tip, girth * (0.42f - 0.25f * t), 0.015f, 5);
        const float radius = H * (0.1f - 0.04f * t) * k.R(0.85f, 1.15f);
        Crown(k, leaves, tip, Vector3(radius, radius * 1.05f, radius), 1.0f, airy);
        for (int j = 0; j < 4; ++j) {
            const Vector3 from2 =
                tip + Polar(k.R(0.0f, 2.0f * kPi), radius * k.R(0.3f, 1.0f), -radius * 0.3f);
            HangingSpray(k, leaves.sprays, from2, k.R(0.0f, 2.0f * kPi), 0.75f, k.R(1.0f, 1.7f),
                         Vector3::Normalize(Polar(angle, 0.6f, 0.8f)), 3);
        }
    }
    Crown(k, leaves, top + Up(0.1f), Vector3(0.55f, 0.85f, 0.55f), 1.0f, airy);
}

/// Weeping: a short stout trunk, a low dome, and round it curtains of long narrow leaves
/// hanging almost to the ground.
void Willow(Maker& k) {
    MeshWriter& bark = k.Bark();
    const Foliage leaves = Broadleaves(k);
    const float H = k.H * k.R(0.9f, 1.1f);
    const float girth = k.R(0.4f, 0.54f);
    const Vector3 fork = Polar(k.R(0.0f, 2.0f * kPi), H * k.R(0.0f, 0.05f), H * k.R(0.2f, 0.27f));
    Trunk(k, bark, fork, girth, 0.85f, 9, 1);
    Roots(k, bark, girth, 5);

    const float reach = H * k.R(0.36f, 0.43f);
    const float tall = H * 0.2f;
    const Vector3 heart = fork + Up(H * 0.34f);
    Limb(k, bark, fork, heart, girth * 0.6f, girth * 0.2f);
    Crown(k, leaves, heart + Up(tall * 0.2f), Vector3(reach * 0.6f, tall, reach * 0.6f), 1.6f);
    const int limbs = 6 + static_cast<int>(k.Seed() % 2u);
    const float start = k.R(0.0f, 2.0f * kPi);
    for (int i = 0; i < limbs; ++i) {
        const float angle =
            start + 2.0f * kPi * static_cast<float>(i) / static_cast<float>(limbs) + k.R(-0.3f, 0.3f);
        const float radius = reach * k.R(0.36f, 0.46f);
        const Vector3 c = heart + Polar(angle, reach - radius * k.R(0.85f, 1.05f), tall * k.R(-0.35f, 0.2f));
        // A willow's limbs go up before they arch over.
        Scaffold(k, bark, fork - Up(k.R(0.0f, 0.08f) * H), c, girth * k.R(0.34f, 0.44f),
                 H * k.R(0.08f, 0.14f));
        Crown(k, leaves, c, Vector3(radius, radius * 0.62f, radius), 1.6f);
    }
    // The curtains: strands side by side all round the dome and some under it. They have no
    // heart, a curtain is to be seen through; the dome above casts the tree's shadow.
    const int strands = 46;
    const float turn = k.R(0.0f, 2.0f * kPi);
    for (int i = 0; i < strands; ++i) {
        const float angle = turn + kGolden * static_cast<float>(i);
        const float out = i % 4 == 3 ? k.R(0.45f, 0.7f) : k.R(0.78f, 1.04f);
        const float from = heart.Y + tall * (k.R(-0.25f, 0.2f) - 0.55f * out * out);
        const float foot = k.R(0.5f, 2.3f);
        const float width = k.R(1.1f, 1.7f);
        const float facing = angle + kPi * 0.5f + k.R(-0.9f, 0.9f);
        const Vector3 shade = Vector3::Normalize(Polar(angle, 0.75f, 0.65f));
        // Two cards one under the other: one would stretch its leaves past belief.
        const float length = std::max(1.0f, from - foot) * 0.5f;
        const Vector3 top = Vector3(heart.X, from, heart.Z) + Polar(angle, reach * out);
        HangingSpray(k, leaves.sprays, top, facing, width, length * 1.08f, shade);
        HangingSpray(k, leaves.sprays, top - Up(length * 0.92f), facing + k.R(-0.3f, 0.3f), width * 0.9f,
                     length * 1.08f, shade);
    }
}

/// Flat-topped: a leaning stem that forks into a few limbs, each carrying a thin wide table
/// of leaves, all at one height.
void Acacia(Maker& k) {
    MeshWriter& bark = k.Bark();
    const Foliage leaves = Broadleaves(k);
    const float H = k.H * k.R(0.88f, 1.1f);
    const float girth = k.R(0.19f, 0.26f);
    const Vector3 fork(k.R(-0.07f, 0.07f) * H, H * k.R(0.34f, 0.46f), k.R(-0.07f, 0.07f) * H);
    Trunk(k, bark, fork, girth, 0.85f, 7, 2);
    const int limbs = 4 + static_cast<int>(k.Seed() % 2u);
    const float start = k.R(0.0f, 2.0f * kPi);
    const float canopy = H * 0.86f;
    const CrownStyle table{.lumpiness = 0.16f};
    for (int i = 0; i < limbs; ++i) {
        const float angle =
            start + 2.0f * kPi * static_cast<float>(i) / static_cast<float>(limbs) + k.R(-0.3f, 0.3f);
        const Vector3 tip = Vector3(fork.X, 0.0f, fork.Z) +
                            Polar(angle, H * k.R(0.2f, 0.32f), canopy - H * k.R(0.02f, 0.07f));
        const Vector3 elbow = fork + (tip - fork) * 0.55f + Up(H * 0.06f);
        Limb(k, bark, fork, elbow, girth * 0.72f, girth * 0.48f);
        Limb(k, bark, elbow, tip, girth * 0.48f, girth * 0.2f, 5);
        const float radius = H * k.R(0.21f, 0.3f);
        Crown(k, leaves, tip + Up(H * 0.035f), Vector3(radius, H * 0.055f, radius), 1.1f, table);
    }
    const float radius = H * k.R(0.26f, 0.32f);
    Crown(k, leaves, Vector3(fork.X, canopy + H * 0.05f, fork.Z), Vector3(radius, H * 0.07f, radius), 1.1f,
          table);
}

/// A bottle of a trunk with a few short thick limbs and tufts of leaves at their ends.
void Baobab(Maker& k) {
    MeshWriter& bark = k.m.Use({0.4f, 0.34f, 0.3f}, Pattern::Bark);
    const Foliage leaves = Broadleaves(k);
    const float H = k.H * k.R(0.9f, 1.08f);
    const float girth = k.def.radiusM * k.R(0.82f, 1.0f);
    const int round = k.far ? 6 : 11;
    const float belly = k.R(0.95f, 1.08f);
    bark.Tube(Vector3(0.0f, -0.4f, 0.0f), Up(H * 0.07f), girth * 1.22f, girth, round, false, false);
    bark.Tube(Up(H * 0.07f), Up(H * 0.24f), girth, girth * belly, round, false, false);
    bark.Tube(Up(H * 0.24f), Up(H * 0.42f), girth * belly, girth * 0.88f, round, false, false);
    const Vector3 shoulder = Up(H * 0.6f);
    bark.Tube(Up(H * 0.42f), shoulder, girth * 0.88f, girth * 0.5f, round, false, true);
    const int limbs = 6 + static_cast<int>(k.Seed() % 3u);
    const float start = k.R(0.0f, 2.0f * kPi);
    const CrownStyle tuft{.lumpiness = 0.28f, .cover = 1.6f, .heart = 0.42f};
    for (int i = 0; i < limbs; ++i) {
        const float angle =
            start + 2.0f * kPi * static_cast<float>(i) / static_cast<float>(limbs) + k.R(-0.25f, 0.25f);
        const Vector3 root = shoulder + Polar(angle, girth * 0.3f, -H * 0.03f);
        const Vector3 tip = shoulder + Polar(angle, H * k.R(0.2f, 0.3f), H * k.R(0.14f, 0.3f));
        Limb(k, bark, root, tip, girth * 0.22f, girth * 0.07f);
        const float radius = H * k.R(0.08f, 0.11f);
        Crown(k, leaves, tip + Up(radius * 0.4f), Vector3(radius * 1.3f, radius * 0.75f, radius * 1.3f), 1.0f,
              tuft);
        // A side branch with its own tuft.
        const Vector3 twig = root + (tip - root) * 0.6f;
        const Vector3 twigTip = twig + Polar(angle + k.R(0.6f, 1.2f), H * 0.1f, H * 0.08f);
        Limb(k, bark, twig, twigTip, girth * 0.08f, girth * 0.03f, 4);
        Crown(k, leaves, twigTip, Vector3(radius, radius * 0.6f, radius), 1.0f, tuft);
    }
}

/// A cone to the ground: whorl on whorl of boughs that droop under their own weight.
void Spruce(Maker& k) {
    MeshWriter& bark = k.Bark();
    const Foliage needles = Conifer(k);
    const float H = k.H * k.R(0.88f, 1.12f);
    const Vector3 top = Polar(k.R(0.0f, 2.0f * kPi), H * k.R(0.0f, 0.025f), H * 0.95f);
    Trunk(k, bark, top, k.R(0.17f, 0.23f), 0.12f, 7, 1);
    const int tiers = 7 + static_cast<int>(k.Seed() % 3u);
    const float bottom = H * k.R(0.07f, 0.16f);
    const float widest = H * k.R(0.18f, 0.25f);
    for (int i = 0; i < tiers; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(tiers);
        const float y0 = bottom + (H - bottom) * t * 0.93f;
        const float radius = widest * (1.0f - t * 0.82f) * k.R(0.9f, 1.1f);
        const float tall = (H - bottom) / static_cast<float>(tiers) * 1.9f;
        const Vector3 axis = top * (y0 / top.Y);
        // Small opaque hearts follow the whorls, providing broken shade without a visible
        // continuous cone inside the open boughs. Keep them in both distance forms.
        const std::uint32_t heartSeed = k.Seed();
        needles.heart.Blob(Vector3(axis.X, y0 + tall * 0.28f, axis.Z),
                           Vector3(radius * 0.45f, tall * 0.24f, radius * 0.45f), heartSeed, 0.2f,
                           k.far ? 4 : 6, k.far ? 2 : 3);
        // The boughs are cards from the trunk outwards and down, every other one on edge so
        // that some face every onlooker.
        const int boughs = 8 + static_cast<int>(std::lround(6.0f * (1.0f - t)));
        const float start = k.R(0.0f, 2.0f * kPi);
        for (int j = 0; j < boughs; ++j) {
            const float angle =
                start + 2.0f * kPi * static_cast<float>(j) / static_cast<float>(boughs) + k.R(-0.2f, 0.2f);
            const Vector3 out = Polar(angle, 1.0f);
            const Vector3 tip = Vector3::Normalize(out + Up(-k.R(0.25f, 0.65f)));
            const Vector3 across = Polar(angle + kPi * 0.5f, 1.0f);
            const Vector3 over = Vector3::Cross(across, tip);
            const float roll =
                (j % 2 == 0 ? k.R(0.1f, 0.5f) : k.R(1.0f, 1.5f)) * (k.R(0.0f, 1.0f) < 0.5f ? -1.0f : 1.0f);
            const Vector3 side = across * std::cos(roll) + over * std::sin(roll);
            const float length = std::max(0.7f, radius * k.R(1.05f, 1.4f));
            const Vector3 foot(axis.X, y0 + tall * k.R(0.3f, 0.6f), axis.Z);
            // Far away only the boughs on edge stay, half as wide again.
            SprayCard(k, needles.sprays, foot + tip * (length * 0.5f), side, tip,
                      length * (k.far ? 1.5f : 1.0f), length, Vector3::Normalize(out * 0.7f + Up(0.7f)),
                      j % 2);
            // Between two boughs a short one close to the stem, steeper: what fills the tree
            // for whoever stands under it. Too small to matter from far away.
            const float between = angle + kPi / static_cast<float>(boughs);
            const Vector3 inOut = Polar(between, 1.0f);
            const Vector3 inTip = Vector3::Normalize(inOut + Up(-k.R(0.7f, 1.1f)));
            const float inLength = length * k.R(0.5f, 0.65f);
            SprayCard(k, needles.sprays,
                      Vector3(axis.X, y0 + tall * k.R(0.45f, 0.8f), axis.Z) + inTip * (inLength * 0.5f),
                      Polar(between + kPi * 0.5f, 1.0f), inTip, inLength, inLength,
                      Vector3::Normalize(inOut * 0.7f + Up(0.7f)), 0);
        }
    }
    // The leader.
    for (int j = 0; j < 2; ++j) {
        const float angle = k.R(0.0f, kPi) + kPi * 0.5f * static_cast<float>(j);
        SprayCard(k, needles.sprays, top - Up(0.3f), Polar(angle, 1.0f), Up(), 1.3f, 1.3f,
                  Vector3::Normalize(Polar(angle + kPi * 0.5f, 0.5f, 0.85f)), 1);
    }
}

/// A Scots pine: a bare, slightly crooked stem, grey below and fox-red above, and a flat
/// crown of pads on a few crooked limbs.
void Pine(Maker& k) {
    MeshWriter& bark = k.m.Use({0.3f, 0.21f, 0.16f}, Pattern::Bark);
    MeshWriter& red = k.m.Use({0.6f, 0.3f, 0.15f}, Pattern::Bark);
    const Foliage needles = Conifer(k);
    const float H = k.H * k.R(0.86f, 1.12f);
    const float girth = k.R(0.2f, 0.28f);
    const float lean = k.R(0.0f, 2.0f * kPi);
    const Vector3 half = Polar(lean, H * k.R(0.0f, 0.04f), H * k.R(0.4f, 0.5f));
    const Vector3 top = Polar(lean + k.R(1.5f, 4.5f), H * k.R(0.0f, 0.05f), H * 0.82f);
    Trunk(k, bark, half, girth, 0.72f, 8, 2);
    const Vector3 bend = half + (top - half) * 0.5f + Polar(k.R(0.0f, 2.0f * kPi), girth * 0.8f);
    const std::array<Vector3, 3> stem{half, bend, top};
    const std::array<float, 3> girths{girth * 0.72f, girth * 0.55f, girth * 0.3f};
    red.Pipe(stem, girths, k.far ? 4 : 7);

    const int pads = 5 + static_cast<int>(k.Seed() % 3u);
    const float start = k.R(0.0f, 2.0f * kPi);
    const float spread = H * k.R(0.15f, 0.21f);
    for (int i = 0; i < pads; ++i) {
        const float angle = start + kGolden * static_cast<float>(i) + k.R(-0.3f, 0.3f);
        const bool crown = i == 0;
        const float radius = H * (crown ? 0.16f : k.R(0.1f, 0.15f));
        const float out = crown ? 0.0f : spread * k.R(0.6f, 1.1f);
        const float drop = crown ? -H * 0.04f : H * k.R(0.0f, 0.16f);
        const Vector3 c = top + Polar(angle, out, -drop);
        Crown(k, needles, c, Vector3(radius, radius * 0.42f, radius), 1.5f, {.lumpiness = 0.22f});
        const Vector3 root =
            half + (top - half) * std::clamp(1.0f - (drop + H * 0.1f) / (top.Y - half.Y), 0.2f, 0.95f);
        Scaffold(k, red, root, c - Up(radius * 0.15f), girth * 0.3f, H * 0.02f);
    }
    // A dead stub or two below the crown.
    for (int i = 0; i < 2; ++i) {
        const float t = k.R(0.3f, 0.9f);
        const Vector3 from = half + (bend - half) * t;
        Limb(k, red, from, from + Polar(k.R(0.0f, 2.0f * kPi), k.R(0.6f, 1.2f), 0.3f), 0.05f, 0.02f, 4);
    }
}

/// A feather palm: a ringed stem that leans and straightens, a fibrous head, and from it a
/// fountain of fronds, the young ones upright, the old ones hanging, a few of them dead.
void Palm(Maker& k) {
    MeshWriter& bark = k.m.Use({0.4f, 0.34f, 0.26f}, Pattern::Bark);
    MeshWriter& fibre = k.m.Use({0.36f, 0.27f, 0.16f}, Pattern::Thatch);
    MeshWriter& fronds = k.m.Use(k.def.color, Pattern::Fronds);
    MeshWriter& under = k.m.Use(Scaled(k.def.color, 0.62f), Pattern::Leaves);
    MeshWriter& dead = k.m.Use(kDryLeaf, Pattern::Fronds);
    const float H = k.H * k.R(0.68f, 0.84f);  // the stem; the fronds rise above it
    const float bendAngle = k.R(0.0f, 2.0f * kPi);
    const float bend = k.R(0.04f, 0.16f) * H;
    const float girth = k.R(0.16f, 0.2f);
    // The stem in rings, each a little wider at its foot than the one below is at its top:
    // the scars the fallen fronds have left.
    const int rings = k.far ? 4 : std::max(8, static_cast<int>(std::lround(H / 0.42f)));
    const auto spine = [&](float t) { return Polar(bendAngle, bend * t * t, -0.4f + (H + 0.4f) * t); };
    for (int i = 0; i < rings; ++i) {
        const float t0 = static_cast<float>(i) / static_cast<float>(rings);
        const float t1 = static_cast<float>(i + 1) / static_cast<float>(rings);
        // Swollen at the foot, slimmest two thirds up, thicker again under the head.
        const auto radius = [&](float t) {
            return girth *
                   (1.0f + 0.9f * std::exp(-t * 14.0f) - 0.22f * std::sin(kPi * std::min(1.0f, t * 1.2f)));
        };
        const float step = k.far ? 1.0f : 1.07f;
        // Closed underneath: whoever looks up the stem looks at the step of every ring.
        bark.Tube(spine(t0), spine(t1), radius(t0) * step, radius(t1) * (2.0f - step), k.far ? 4 : 8,
                  !k.far && i > 0, false);
    }
    const Vector3 top = spine(1.0f);
    fibre.Blob(top + Up(0.1f), Vector3(girth * 1.7f, 0.55f, girth * 1.7f), k.Seed(), 0.16f, k.far ? 5 : 8,
               k.far ? 3 : 5);

    const int count = 19 + static_cast<int>(k.Seed() % 5u);
    const float start = k.R(0.0f, 2.0f * kPi);
    for (int i = 0; i < count; ++i) {
        // From the youngest, upright in the middle, to the oldest, hanging below the head.
        const float age = static_cast<float>(i) / static_cast<float>(count - 1);
        const float angle = start + kGolden * static_cast<float>(i) + k.R(-0.2f, 0.2f);
        Arch arch;
        arch.out = Polar(angle, 1.0f);
        arch.length = k.H * k.R(0.3f, 0.38f) * (0.8f + 0.2f * std::sin(kPi * age));
        arch.elevation = 1.3f - 1.75f * age + k.R(-0.12f, 0.12f);
        arch.droop = k.R(0.5f, 0.75f) - 0.25f * age;
        arch.base = top + Up(0.35f - 0.3f * age) + arch.out * (girth * 0.9f);
        const float keel = 0.45f - 0.9f * age + k.R(-0.1f, 0.1f);
        // The stalk, and under it the stub of its sheath on the head.
        Limb(k, fibre, top + Up(0.1f - 0.2f * age), arch.At(0.14f), 0.06f, 0.03f, 4);
        FrondStrip(k, fronds, &under, PlantCard::PalmFrond, arch, arch.length / 2.5f, keel, 7);
    }
    // Dead fronds hang down the stem until they fall or are cut.
    const int withered = 2 + static_cast<int>(k.Seed() % 3u);
    for (int i = 0; i < withered; ++i) {
        Arch arch;
        arch.out = Polar(k.R(0.0f, 2.0f * kPi), 1.0f);
        arch.length = k.H * k.R(0.24f, 0.3f);
        arch.elevation = k.R(-1.15f, -0.8f);
        arch.droop = 0.25f;
        arch.base = top - Up(0.15f) + arch.out * (girth * 1.2f);
        FrondStrip(k, dead, nullptr, PlantCard::PalmFrond, arch, arch.length / 3.2f, -0.8f, 4);
    }
}

/// A clump of bamboo: canes in joints, close together at the foot and leaning apart, their
/// thin tops arching over under fans of narrow leaves.
void Bamboo(Maker& k) {
    MeshWriter& cane = k.m.Use(Mix(k.def.color, {0.55f, 0.6f, 0.2f}, 0.45f), Pattern::Plain);
    MeshWriter& leaves = k.m.Use(k.def.color, Pattern::Fronds);
    MeshWriter& blades = k.m.Use(Scaled(k.def.color, 0.8f), Pattern::Plain);
    const int culms = 13 + static_cast<int>(k.Seed() % 4u);
    for (int i = 0; i < culms; ++i) {
        const Vector3 base = Polar(k.R(0.0f, 2.0f * kPi), k.def.radiusM * std::sqrt(k.R(0.0f, 0.7f)), -0.3f);
        const float tall = k.H * k.R(0.6f, 1.0f);
        const float away = k.R(0.0f, 2.0f * kPi);
        const float lean = tall * k.R(0.02f, 0.1f);
        const float nod = tall * k.R(0.08f, 0.2f);
        const float thick = 0.026f + 0.02f * tall / k.H;
        // Straight for most of its height, then nodding over.
        const auto at = [&](float t) {
            const float over = std::max(0.0f, t - 0.5f) * 2.0f;
            return base + Polar(away, lean * t + nod * over * over, tall * (t - 0.12f * over * over));
        };
        const int joints = k.far ? 2 : std::max(6, static_cast<int>(std::lround(tall / 0.55f)));
        for (int j = 0; j < joints; ++j) {
            const float t0 = static_cast<float>(j) / static_cast<float>(joints);
            const float t1 = static_cast<float>(j + 1) / static_cast<float>(joints);
            const float r0 = thick * (1.0f - 0.75f * t0), r1 = thick * (1.0f - 0.75f * t1);
            const Vector3 p0 = at(t0), p1 = at(t1);
            if (k.far) {
                cane.Tube(p0, p1, r0 * 1.2f, r1 * 1.2f, 3, false, false);
            } else if (t1 < 0.6f) {
                // Each joint ends in a swollen node, the next begins slimmer. Only as high as
                // anyone can make a node out.
                const Vector3 node = p0 + (p1 - p0) * 0.95f;
                cane.Tube(p0, node, r0 * 0.94f, r1, 6, false, false);
                cane.Tube(node, p1, r1 * 1.2f, r1 * 1.2f, 6, false, false);
            } else {
                cane.Tube(p0, p1, r0, r1, 4, false, false);
            }
        }
        // Twigs of leaves from the nodes of the upper half, hanging out and down.
        const int twigs = 9;
        for (int j = 0; j < twigs; ++j) {
            const float t =
                0.42f + 0.58f * (static_cast<float>(j) + k.R(0.0f, 0.8f)) / static_cast<float>(twigs);
            const float round = k.R(0.0f, 2.0f * kPi);
            const Vector3 out = Polar(round, 1.0f);
            const Vector3 tip = Vector3::Normalize(out + Up(k.R(-0.9f, 0.3f)));
            const float size = k.R(0.75f, 1.2f);
            const bool mirror = k.R(0.0f, 1.0f) < 0.5f;
            const float turn = k.R(-1.2f, 1.2f);
            if (!k.Keep(3)) {
                continue;
            }
            const Vector3 across = Polar(round + kPi * 0.5f, 1.0f);
            const Vector3 side = across * std::cos(turn) + Vector3::Cross(across, tip) * std::sin(turn);
            const float grow = k.far ? 1.7f : 1.0f;
            const Vector3 foot = at(std::min(t, 1.0f));
            PlantCardAt(leaves, j % 2 == 0 ? PlantCard::BambooA : PlantCard::BambooB, foot, side, tip,
                        size * grow, size * grow, Vector3::Normalize(out * 0.5f + Up(0.85f)), mirror);
            // Cut-out leaves cast no shadow: two solid blades lie among the leaves of every
            // other fan, and it is these that dapple the ground under the clump.
            if (j % 2 == 0) {
                for (const float bladeLean : {-0.45f, 0.4f}) {
                    const Vector3 along = Vector3::Normalize(tip + side * bladeLean);
                    const Vector3 a = foot + along * (size * 0.3f), b = foot + along * (size * 0.72f);
                    const Vector3 wide = Vector3::Cross(along, Vector3::Cross(side, tip)) * (size * 0.035f);
                    blades.QuadTwoSided(a, (a + b) * 0.5f - wide, b, (a + b) * 0.5f + wide);
                }
            }
        }
    }
}

/// A shuttlecock of fronds: the young ones stand, the old ones arch out to the ground.
void Fern(Maker& k) {
    MeshWriter& fronds = k.m.Use(k.def.color, Pattern::Fronds);
    MeshWriter& under = Shade(k, k.def.color);
    const int count = 15 + static_cast<int>(k.Seed() % 5u);
    const float start = k.R(0.0f, 2.0f * kPi);
    for (int i = 0; i < count; ++i) {
        const float age = static_cast<float>(i) / static_cast<float>(count - 1);
        Arch arch;
        arch.out = Polar(start + kGolden * static_cast<float>(i) + k.R(-0.25f, 0.25f), 1.0f);
        arch.length = k.H * k.R(0.62f, 0.85f) * (0.75f + 0.25f * age);
        arch.elevation = 1.3f - 0.75f * age + k.R(-0.1f, 0.1f);
        arch.droop = k.R(0.3f, 0.5f) + 0.15f * age;
        arch.base = Up(0.08f) + arch.out * 0.06f;
        const float keel = k.R(-0.1f, 0.25f);
        if (k.far && i % 2 == 1) {
            continue;
        }
        FrondStrip(k, fronds, &under, PlantCard::FernFrond, arch, arch.length / (k.far ? 1.6f : 2.3f), keel,
                   5);
    }
    under.Blob(Up(0.12f), Vector3(0.22f, 0.26f, 0.22f), k.Seed(), 0.25f, k.far ? 4 : 6, k.far ? 3 : 4, 0.5f);
}

/// A tuft of the plant-card atlas: two cards crossed on `foot`.
void Tuft(Maker& k, MeshWriter& cards, PlantCard a, PlantCard b, const Vector3& foot, float width,
          float height) {
    const float turn = k.R(0.0f, kPi);
    const Vector3 tip = Vector3::Normalize(Vector3(k.R(-0.12f, 0.12f), 1.0f, k.R(-0.12f, 0.12f)));
    const bool mirror = k.R(0.0f, 1.0f) < 0.5f;
    if (!k.Keep(2)) {
        return;
    }
    const float grow = k.far ? 1.35f : 1.0f;
    const Vector3 shade = Vector3::Normalize(Polar(turn, 0.25f, 1.0f));
    PlantCardAt(cards, a, foot, Polar(turn, 1.0f), tip, width * grow, height, shade, mirror);
    PlantCardAt(cards, b, foot, Polar(turn + kPi * 0.5f, 1.0f), tip, width * grow, height, shade, !mirror);
}

void Bush(Maker& k, const Foliage& foliage, int blobs, float spread, float squash, bool twigs, bool grass) {
    const float H = k.H * k.R(0.85f, 1.15f);
    const int count = blobs + static_cast<int>(k.Seed() % 2u);
    for (int i = 0; i < count; ++i) {
        const float angle = k.R(0.0f, 2.0f * kPi);
        const float distance = i == 0 ? 0.0f : spread * k.R(0.5f, 1.1f);
        const float tall = H * (i == 0 ? 0.5f : k.R(0.28f, 0.46f)) * squash;
        const float wide = k.def.radiusM * k.R(0.7f, 1.12f);
        // At arm's length a bush is many twig-end sprays, not four coarse green cushions. Keep
        // the former card size in the far form: its atlas cell already represents four sprays.
        const float coarseCard = std::clamp(wide * 1.1f, 0.5f, 0.8f);
        const float card = k.far ? coarseCard : std::clamp(wide * 0.88f, 0.42f, 0.64f);
        Crown(k, foliage, Polar(angle, distance, tall * 0.8f), Vector3(wide, tall, wide), card,
              {.lumpiness = 0.25f, .flatBase = 0.75f, .cover = k.far ? 1.5f : 1.2f});
    }
    if (twigs) {
        MeshWriter& bark = k.Bark();
        // Stems from the root, which show through the leaves; none stands out above them.
        for (int i = 0; i < (k.far ? 4 : 7); ++i) {
            const float angle = k.R(0.0f, 2.0f * kPi);
            Limb(k, bark, Polar(angle, 0.1f, -0.1f),
                 Polar(angle, k.R(0.3f, 0.6f), H * squash * k.R(0.4f, 0.6f)), 0.03f, 0.012f, 4);
        }
    }
    if (grass) {
        // Dry grass that the grazers leave standing under a thorny bush.
        MeshWriter& blades = k.m.Use(Mix(k.def.color, {0.62f, 0.56f, 0.3f}, 0.55f), Pattern::Fronds);
        for (int i = 0; i < 3; ++i) {
            const Vector3 foot = Polar(k.R(0.0f, 2.0f * kPi), k.def.radiusM * k.R(0.7f, 1.3f), -0.04f);
            const float tall = k.R(0.4f, 0.7f);
            Tuft(k, blades, PlantCard::GrassA, PlantCard::GrassB, foot, tall * 1.2f, tall);
        }
    }
}

/// A stand of reeds or a clump of tall grass: tufts close together over a low dark heart.
void Tufts(Maker& k, PlantCard a, PlantCard b, int count) {
    MeshWriter& cards = k.m.Use(k.def.color, Pattern::Fronds);
    MeshWriter& heart = Shade(k, k.def.color);
    for (int i = 0; i < count; ++i) {
        const Vector3 foot =
            i == 0 ? Up(-0.04f)
                   : Polar(k.R(0.0f, 2.0f * kPi), k.def.radiusM * std::sqrt(k.R(0.05f, 1.0f)), -0.04f);
        const float tall = k.H * k.R(0.75f, 1.1f);
        Tuft(k, cards, a, b, foot, tall * k.R(0.8f, 1.0f), tall);
    }
    heart.Blob(Up(0.0f), Vector3(k.def.radiusM * 0.7f, k.H * 0.3f, k.def.radiusM * 0.7f), k.Seed(), 0.3f,
               k.far ? 5 : 7, k.far ? 3 : 4, 0.0f);
}

void FlowerBed(Maker& k) {
    const float r = k.def.radiusM;
    const int sides = k.far ? 8 : 14;
    MeshWriter& kerb = k.m.Use(kStone, Pattern::Stone);
    for (int i = 0; i < sides; ++i) {
        const float a0 = 2.0f * kPi * static_cast<float>(i) / static_cast<float>(sides);
        const float a1 = 2.0f * kPi * static_cast<float>(i + 1) / static_cast<float>(sides);
        const Vector3 o0 = Polar(a0, r), o1 = Polar(a1, r);
        const Vector3 i0 = Polar(a0, r - 0.14f), i1 = Polar(a1, r - 0.14f);
        const Vector3 lo = Up(-0.2f), hi = Up(0.2f);
        kerb.Quad(o1 + lo, o0 + lo, o0 + hi, o1 + hi);
        kerb.Quad(i0 + lo, i1 + lo, i1 + hi, i0 + hi);
        kerb.Quad(o1 + hi, o0 + hi, i0 + hi, i1 + hi);
    }
    k.m.Use(kSoil, Pattern::Concrete).Disc(Up(0.15f), r - 0.12f, sides);

    // A cushion of leaves fills the bed; the flowers stand out of it, leaning every way, so
    // that whoever looks down on the bed looks into some of their faces.
    const zoo::Color3 herb{0.2f, 0.42f, 0.16f};
    const Foliage green{Shade(k, herb), k.m.Use(herb, Pattern::LeafSpray)};
    Crown(k, green, Up(0.12f), Vector3(r - 0.2f, 0.3f, r - 0.2f), 0.42f,
          {.lumpiness = 0.2f, .flatBase = 0.0f, .cover = 1.5f, .heart = 0.8f});
    const zoo::Color3 petals[3] = {k.def.color, {0.9f, 0.7f, 0.1f}, {0.9f, 0.88f, 0.82f}};
    MeshWriter* heads[3] = {&k.m.Use(petals[0], Pattern::Fronds), &k.m.Use(petals[1], Pattern::Fronds),
                            &k.m.Use(petals[2], Pattern::Fronds)};
    const int clumps = 30;
    for (int i = 0; i < clumps; ++i) {
        const float angle = kGolden * static_cast<float>(i) + k.R(-0.3f, 0.3f);
        const float out = (r - 0.3f) * std::sqrt((static_cast<float>(i) + 0.5f) / static_cast<float>(clumps));
        const float lean = k.R(0.25f, 0.95f);
        const float toward = angle + k.R(-1.0f, 1.0f);
        const float size = k.R(0.38f, 0.52f);
        const bool mirror = k.R(0.0f, 1.0f) < 0.5f;
        // Patches of one colour, as a gardener plants them.
        const int colour = static_cast<int>(std::floor(angle / (2.0f * kPi) * 6.0f + 0.5f * out)) % 3;
        if (!k.Keep(3)) {
            continue;
        }
        const Vector3 tip = Vector3::Normalize(Polar(toward, std::sin(lean), std::cos(lean)));
        const Vector3 side = Polar(toward + kPi * 0.5f, 1.0f);
        const float grow = k.far ? 1.6f : 1.0f;
        PlantCardAt(*heads[colour < 0 ? 0 : colour], i % 2 == 0 ? PlantCard::Daisies : PlantCard::Umbels,
                    Polar(angle, out, 0.14f), side, tip, size * grow, size * grow,
                    Vector3::Normalize(Up() + tip * 0.3f), mirror);
    }
}

}  // namespace

bool IsPlant(const zoo::PropDef& def) {
    static constexpr std::string_view kLooks[] = {"oak",  "birch",  "pine",   "spruce",    "acacia", "baobab",
                                                  "palm", "bamboo", "willow", "juniper",   "shrub",  "scrub",
                                                  "fern", "reeds",  "grass",  "flower_bed"};
    return std::ranges::find(kLooks, std::string_view(def.look)) != std::end(kLooks);
}

bool BuildPlant(ModelBuilder& model, const zoo::PropDef& def, std::uint32_t seed, PlantDetail detail) {
    const std::string_view look = def.look;
    Maker k(model, def, seed, detail);
    bool dropsLeaves = false;
    if (look == "oak") {
        Oak(k);
        dropsLeaves = true;
    } else if (look == "birch") {
        Birch(k);
        dropsLeaves = true;
    } else if (look == "pine") {
        Pine(k);
    } else if (look == "spruce") {
        Spruce(k);
    } else if (look == "acacia") {
        Acacia(k);
        dropsLeaves = true;
    } else if (look == "baobab") {
        Baobab(k);
        dropsLeaves = true;
    } else if (look == "palm") {
        Palm(k);
    } else if (look == "bamboo") {
        Bamboo(k);
    } else if (look == "willow") {
        Willow(k);
        dropsLeaves = true;
    } else if (look == "juniper") {
        Bush(k, Conifer(k), 3, 0.3f, 1.15f, false, false);
    } else if (look == "shrub") {
        Bush(k, Broadleaves(k), 4, 0.45f, 0.9f, true, true);
    } else if (look == "scrub") {
        Bush(k, Broadleaves(k), 2, 0.4f, 0.6f, true, true);
    } else if (look == "fern") {
        Fern(k);
    } else if (look == "reeds") {
        Tufts(k, PlantCard::ReedsA, PlantCard::ReedsB, 7);
    } else if (look == "grass") {
        Tufts(k, PlantCard::GrassA, PlantCard::GrassB, 9);
    } else if (look == "flower_bed") {
        FlowerBed(k);
    } else {
        return false;
    }
    if (dropsLeaves) {
        FallenLeaves(k);
    }
    return true;
}

void BuildHedge(ModelBuilder& model, const Vector3& a, const Vector3& b, float height,
                const zoo::Color3& color, std::uint32_t seed) {
    MeshWriter& core = model.Use(Scaled(color, 0.8f), Pattern::LeafShade);
    MeshWriter& sprays = model.Use(color, Pattern::LeafSpray);
    zoo::Rng rng(seed, 9);
    const Vector3 run = b - a;
    const float length = run.Length();
    if (length < 1e-4f) {
        return;
    }
    const Vector3 along = run / length;
    const Vector3 across(-along.Z, 0.0f, along.X);
    constexpr float kHalf = 0.3f;  // half the hedge's thickness
    // The core: a box a hand's breadth inside the leaves, into the ground on a slope.
    const float inside = kHalf - 0.09f, top = height - 0.09f;
    const Vector3 lo = Up(-0.3f), hi = Up(top);
    const Vector3 a0 = a - across * inside, a1 = a + across * inside;
    const Vector3 b0 = b - across * inside, b1 = b + across * inside;
    core.Quad(a1 + lo, b1 + lo, b1 + hi, a1 + hi);
    core.Quad(b0 + lo, a0 + lo, a0 + hi, b0 + hi);
    core.Quad(a1 + hi, b1 + hi, b0 + hi, a0 + hi);
    core.Quad(a0 + lo, a1 + lo, a1 + hi, a0 + hi);
    core.Quad(b1 + lo, b0 + lo, b0 + hi, b1 + hi);

    // The coat: sprays lying against the two faces and the top like tiles, every one a
    // little askew, and those of the top standing a little proud: clipped, but alive.
    constexpr float kCard = 0.52f;
    const auto spray = [&](const Vector3& center, const Vector3& normal, const Vector3& upward) {
        const Vector3 jitter(rng.Range(-0.5f, 0.5f), rng.Range(-0.5f, 0.5f), rng.Range(-0.5f, 0.5f));
        const Vector3 face = Vector3::Normalize(normal + jitter * 0.7f);
        Vector3 tip = upward + jitter * 0.8f;
        tip = Vector3::Normalize(tip - face * Vector3::Dot(tip, face));
        const int cell = static_cast<int>(rng.Range(0.0f, 3.999f));
        float u0 = static_cast<float>(cell % 2) * 0.5f, u1 = u0 + 0.5f;
        const float v0 = static_cast<float>(cell / 2) * 0.5f;
        if (rng.Chance(0.5f)) {
            std::swap(u0, u1);
        }
        const float half = kCard * 0.5f * rng.Range(0.85f, 1.2f);
        sprays.Card(center, Vector3::Cross(tip, face) * half, tip * half,
                    Vector3::Normalize(normal + Up(0.35f)), u0, v0, u1, v0 + 0.5f);
    };
    const int columns = std::max(2, static_cast<int>(std::lround(length / 0.3f)));
    const int rows = std::max(2, static_cast<int>(std::lround(height / 0.34f)));
    for (int i = 0; i < columns; ++i) {
        const float t = (static_cast<float>(i) + 0.5f) / static_cast<float>(columns);
        const Vector3 base = a + run * t;
        for (int j = 0; j < rows; ++j) {
            const float y = 0.12f + (height - 0.3f) * (static_cast<float>(j) + rng.Range(0.2f, 0.8f)) /
                                        static_cast<float>(rows);
            for (const float way : {-1.0f, 1.0f}) {
                spray(base + along * rng.Range(-0.12f, 0.12f) + across * (way * (kHalf - 0.03f)) + Up(y),
                      across * way, Up());
            }
        }
        for (int j = 0; j < 2; ++j) {
            const float off = (static_cast<float>(j) - 0.5f) * kHalf + rng.Range(-0.08f, 0.08f);
            spray(base + along * rng.Range(-0.12f, 0.12f) + across * off +
                      Up(height - 0.06f + rng.Range(0.0f, 0.07f)),
                  Up(), along * (rng.Chance(0.5f) ? 1.0f : -1.0f));
        }
    }
}

}  // namespace zoogame
