// SPDX-License-Identifier: MIT
#include "FoliageTextures.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace zoogame {

namespace {

constexpr float kPi = std::numbers::pi_v<float>;
constexpr int kCell = 256;  ///< texels along a cell
constexpr int kCells = 2;
constexpr int kMargin = 4;  ///< clear texels round every cell

/// A small generator of its own: the pictures must not change with anything else.
class Dice {
public:
    explicit Dice(std::uint32_t seed) : state_(seed * 2654435761u + 0x9E3779B9u) {}
    float Next() {
        state_ ^= state_ << 13;
        state_ ^= state_ >> 17;
        state_ ^= state_ << 5;
        return static_cast<float>(state_ >> 8) * (1.0f / 16777216.0f);
    }
    float Range(float lo, float hi) { return lo + (hi - lo) * Next(); }

private:
    std::uint32_t state_;
};

struct Rgb {
    float r, g, b;
};

/// One picture being painted: linear colour and coverage, later things over earlier ones.
///
/// Positions are in the picture, 0..1 both ways; lengths are in picture widths and widths of
/// strokes in texels. A picture that goes on a card longer than wide is painted `stretch`
/// times as long as wide (in its own measure), so that angles and shapes come out true on
/// the card although the picture is stored square.
class Canvas {
public:
    explicit Canvas(int cell = kCell, float stretch = 1.0f)
        : n_(cell),
          stretch_(stretch),
          color_(static_cast<std::size_t>(cell * cell), Rgb{0, 0, 0}),
          alpha_(color_.size(), 0.0f) {}

    /// Whether a disc lies wholly inside the margin (coordinates in cells, 0..1).
    static bool Inside(float x, float y, float radius) {
        const float m = static_cast<float>(kMargin) / static_cast<float>(kCell) + radius;
        return x > m && x < 1.0f - m && y > m && y < 1.0f - m;
    }

    /// A straight stroke with round ends; widths in texels.
    void Stroke(float x0, float y0, float x1, float y1, float halfWidth, const Rgb& color) {
        const float n = static_cast<float>(n_);
        const float ax = x0 * n, ay = y0 * n * stretch_, bx = x1 * n, by = y1 * n * stretch_;
        const float dx = bx - ax, dy = by - ay;
        const float lengthSq = std::max(dx * dx + dy * dy, 1e-6f);
        const float reach = halfWidth + 1.0f;
        Box(std::min(ax, bx) - reach, std::min(ay, by) - reach, std::max(ax, bx) + reach,
            std::max(ay, by) + reach, [&](float px, float py) {
                const float t = std::clamp(((px - ax) * dx + (py - ay) * dy) / lengthSq, 0.0f, 1.0f);
                const float ex = px - (ax + dx * t), ey = py - (ay + dy * t);
                return Hit{halfWidth - std::sqrt(ex * ex + ey * ey), color};
            });
    }

    /// A leaf from its stalk at (x, y) towards `angle`: pointed at both ends, widest a little
    /// below the middle, a pale midrib and one half in the shade of the other.
    void Leaf(float x, float y, float angle, float length, float width, const Rgb& color) {
        const float n = static_cast<float>(n_);
        const float ax = x * n, ay = y * n * stretch_;
        const float ux = std::cos(angle), uy = std::sin(angle);
        const float L = length * n;
        const float W = width * n * 0.5f;
        const float cx = ax + ux * L * 0.5f, cy = ay + uy * L * 0.5f;
        const float reach = L * 0.5f + 1.5f;
        Box(cx - reach, cy - reach, cx + reach, cy + reach, [&](float px, float py) {
            const float along = ((px - ax) * ux + (py - ay) * uy) / L;
            const float across = -(px - ax) * uy + (py - ay) * ux;
            if (along <= 0.0f || along >= 1.0f) {
                return Hit{-1.0f, color};
            }
            const float half = W * std::pow(std::sin(kPi * std::pow(along, 0.8f)), 0.85f);
            float tone = 0.86f + 0.22f * along;
            if (across > 0.0f) {
                tone *= 0.84f;
            }
            if (std::abs(across) < 0.6f && along < 0.9f) {
                tone *= 1.22f;
            }
            return Hit{half - std::abs(across), Rgb{color.r * tone, color.g * tone, color.b * tone}};
        });
    }

    /// Gives the clear texels the colour of the nearest painted ones (as far as `passes`
    /// texels away; the rest take the picture's mean) and writes the picture into the atlas
    /// with its top left texel at (originX, originY).
    void Write(SprayAtlas& atlas, int originX, int originY, int passes) {
        std::vector<float> known = alpha_;
        for (int pass = 0; pass < passes; ++pass) {
            std::vector<Rgb> nextColor = color_;
            std::vector<float> nextKnown = known;
            for (int y = 0; y < n_; ++y) {
                for (int x = 0; x < n_; ++x) {
                    const std::size_t i = At(x, y);
                    if (known[i] > 0.0f) {
                        continue;
                    }
                    Rgb sum{0, 0, 0};
                    float weight = 0.0f;
                    for (int oy = -1; oy <= 1; ++oy) {
                        for (int ox = -1; ox <= 1; ++ox) {
                            const int nx = x + ox, ny = y + oy;
                            if (nx < 0 || ny < 0 || nx >= n_ || ny >= n_ || known[At(nx, ny)] <= 0.0f) {
                                continue;
                            }
                            const Rgb& c = color_[At(nx, ny)];
                            sum.r += c.r;
                            sum.g += c.g;
                            sum.b += c.b;
                            weight += 1.0f;
                        }
                    }
                    if (weight > 0.0f) {
                        nextColor[i] = Rgb{sum.r / weight, sum.g / weight, sum.b / weight};
                        nextKnown[i] = 1.0f;
                    }
                }
            }
            color_.swap(nextColor);
            known.swap(nextKnown);
        }
        // What no leaf is near takes the picture's mean.
        Rgb mean{0, 0, 0};
        float weight = 0.0f;
        for (std::size_t i = 0; i < color_.size(); ++i) {
            if (alpha_[i] > 0.5f) {
                mean.r += color_[i].r;
                mean.g += color_[i].g;
                mean.b += color_[i].b;
                weight += 1.0f;
            }
        }
        if (weight > 0.0f) {
            mean = Rgb{mean.r / weight, mean.g / weight, mean.b / weight};
        }
        const auto encode = [](float linear) {
            const float c = std::clamp(linear, 0.0f, 1.0f);
            const float s = c <= 0.0031308f ? c * 12.92f : 1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f;
            return static_cast<std::uint8_t>(std::lround(s * 255.0f));
        };
        for (int y = 0; y < n_; ++y) {
            for (int x = 0; x < n_; ++x) {
                const std::size_t i = At(x, y);
                const Rgb& c = known[i] > 0.0f ? color_[i] : mean;
                const std::size_t o =
                    (static_cast<std::size_t>(originY + y) * static_cast<std::size_t>(atlas.size) +
                     static_cast<std::size_t>(originX + x)) *
                    4u;
                atlas.rgba[o] = encode(c.r);
                atlas.rgba[o + 1] = encode(c.g);
                atlas.rgba[o + 2] = encode(c.b);
                atlas.rgba[o + 3] =
                    static_cast<std::uint8_t>(std::lround(std::clamp(alpha_[i], 0.0f, 1.0f) * 255.0f));
            }
        }
    }

private:
    struct Hit {
        float inside;  ///< texels inside the shape's edge (negative: outside)
        Rgb color;
    };

    std::size_t At(int x, int y) const {
        return static_cast<std::size_t>(y) * static_cast<std::size_t>(n_) + static_cast<std::size_t>(x);
    }

    /// Paints `shape` within a box given, like the shape's own arguments, in texels of the
    /// stretched picture.
    template <typename Shape>
    void Box(float minX, float minY, float maxX, float maxY, Shape&& shape) {
        const int x0 = std::max(kMargin, static_cast<int>(std::floor(minX)));
        const int y0 = std::max(kMargin, static_cast<int>(std::floor(minY / stretch_)));
        const int x1 = std::min(n_ - 1 - kMargin, static_cast<int>(std::ceil(maxX)));
        const int y1 = std::min(n_ - 1 - kMargin, static_cast<int>(std::ceil(maxY / stretch_)));
        for (int y = y0; y <= y1; ++y) {
            for (int x = x0; x <= x1; ++x) {
                const Hit hit =
                    shape(static_cast<float>(x) + 0.5f, (static_cast<float>(y) + 0.5f) * stretch_);
                const float cover = std::clamp(hit.inside + 0.5f, 0.0f, 1.0f);
                if (cover <= 0.0f) {
                    continue;
                }
                const std::size_t i = At(x, y);
                const float under = alpha_[i] * (1.0f - cover);
                const float total = cover + under;
                color_[i] = Rgb{(hit.color.r * cover + color_[i].r * under) / total,
                                (hit.color.g * cover + color_[i].g * under) / total,
                                (hit.color.b * cover + color_[i].b * under) / total};
                alpha_[i] = total;
            }
        }
    }

    int n_;
    float stretch_;
    std::vector<Rgb> color_;
    std::vector<float> alpha_;
};

/// The colour of bare twig as the texture holds it: the material multiplies by a leaf
/// green, so "brown" here is whatever comes out dark and dull after that.
constexpr Rgb kTwig{0.5f, 0.2f, 0.22f};

struct LeafMark {
    float x, y, angle, length, width, tone, warm;
};

/// A leaf's colour: its tone, and how much yellower (warm > 0) or bluer than the rest.
Rgb LeafColor(float tone, float warm) {
    return Rgb{tone * (1.0f + 0.22f * warm), tone, tone * (1.0f - 0.3f * warm)};
}

/// Lines a twig with leaves on alternating sides and one at its tip.
void LeafTwig(Canvas& canvas, Dice& dice, std::vector<LeafMark>& leaves, float x, float y, float angle,
              float length, float leafLength, float gap) {
    const float ux = std::cos(angle), uy = std::sin(angle);
    canvas.Stroke(x, y, x + ux * length, y + uy * length, 0.7f, kTwig);
    int side = dice.Next() < 0.5f ? 1 : -1;
    for (float s = gap * dice.Range(0.3f, 1.0f); s < length; s += gap * dice.Range(0.75f, 1.25f)) {
        const float size = leafLength * dice.Range(0.8f, 1.2f);
        leaves.push_back({x + ux * s, y + uy * s, angle + static_cast<float>(side) * dice.Range(0.55f, 1.25f),
                          size, size * dice.Range(0.42f, 0.56f), 0.0f, 0.0f});
        side = -side;
    }
    const float size = leafLength * dice.Range(0.9f, 1.25f);
    leaves.push_back({x + ux * length, y + uy * length, angle + dice.Range(-0.25f, 0.25f), size,
                      size * dice.Range(0.42f, 0.56f), 0.0f, 0.0f});
}

void PaintLeafSpray(Canvas& canvas, std::uint32_t seed) {
    Dice dice(seed);
    std::vector<LeafMark> leaves;
    const float leafLength = 0.066f;
    const float gap = 0.03f;
    // The main stem, in a few pieces that wander a little.
    float x = 0.5f, y = 0.985f;
    float angle = -kPi * 0.5f + dice.Range(-0.1f, 0.1f);
    const int pieces = 7;
    const float piece = 0.74f / static_cast<float>(pieces);
    int side = dice.Next() < 0.5f ? 1 : -1;
    for (int i = 0; i < pieces; ++i) {
        const float nx = x + std::cos(angle) * piece, ny = y + std::sin(angle) * piece;
        canvas.Stroke(x, y, nx, ny, 1.6f - 0.13f * static_cast<float>(i), kTwig);
        if (i > 0) {
            LeafTwig(canvas, dice, leaves, x, y, angle, piece, leafLength, gap * 1.4f);
        }
        // A side branch from every joint, and from each of those two or three twigs.
        const float t = static_cast<float>(i) / static_cast<float>(pieces);
        const float wanted = (0.44f - 0.2f * t) * dice.Range(0.8f, 1.1f) * (i == 0 ? 0.7f : 1.0f);
        for (int both = 0; both < (i % 2 == 0 ? 2 : 1); ++both) {
            const float out = angle + static_cast<float>(side) * dice.Range(0.6f, 1.05f);
            const float bx = nx, by = ny;
            // No branch runs out of the picture: a twig cut off straight would show on every card.
            float reach = wanted;
            while (reach > 0.05f &&
                   !Canvas::Inside(bx + std::cos(out) * reach, by + std::sin(out) * reach, leafLength)) {
                reach *= 0.85f;
            }
            const float ex = bx + std::cos(out) * reach, ey = by + std::sin(out) * reach;
            canvas.Stroke(bx, by, ex, ey, 1.0f, kTwig);
            LeafTwig(canvas, dice, leaves, bx, by, out, reach, leafLength, gap);
            int twigSide = side;
            for (float s = reach * dice.Range(0.25f, 0.4f); s < reach * 0.9f;
                 s += reach * dice.Range(0.22f, 0.34f)) {
                const float twig = out + static_cast<float>(twigSide) * dice.Range(0.55f, 0.95f);
                LeafTwig(canvas, dice, leaves, bx + std::cos(out) * s, by + std::sin(out) * s, twig,
                         reach * dice.Range(0.3f, 0.5f), leafLength, gap);
                twigSide = -twigSide;
            }
            side = -side;
        }
        x = nx;
        y = ny;
        angle += dice.Range(-0.14f, 0.14f);
    }
    // Leaves of the twigs behind, which the picture does not draw.
    for (int i = 0; i < 90; ++i) {
        const float a = dice.Range(0.0f, 2.0f * kPi);
        const float r = 0.4f * std::sqrt(dice.Next());
        const float size = leafLength * dice.Range(0.8f, 1.2f);
        leaves.push_back({0.5f + std::cos(a) * r, 0.48f + std::sin(a) * r * 1.05f,
                          dice.Range(0.0f, 2.0f * kPi), size, size * dice.Range(0.42f, 0.56f), -1.0f, 0.0f});
    }
    for (LeafMark& leaf : leaves) {
        // Those behind are the darker ones.
        leaf.tone = leaf.tone < 0.0f ? dice.Range(0.4f, 0.62f) : dice.Range(0.58f, 1.0f);
        leaf.warm = dice.Range(-0.35f, 1.0f) * dice.Next();
    }
    std::stable_sort(leaves.begin(), leaves.end(),
                     [](const LeafMark& a, const LeafMark& b) { return a.tone < b.tone; });
    for (const LeafMark& leaf : leaves) {
        const float mx = leaf.x + std::cos(leaf.angle) * leaf.length * 0.5f;
        const float my = leaf.y + std::sin(leaf.angle) * leaf.length * 0.5f;
        if (!Canvas::Inside(mx, my, leaf.length * 0.5f)) {
            continue;
        }
        canvas.Leaf(leaf.x, leaf.y, leaf.angle, leaf.length, leaf.width, LeafColor(leaf.tone, leaf.warm));
    }
}

/// A twig lined with needles on both sides, shorter towards its tip.
void NeedleTwig(Canvas& canvas, Dice& dice, float x, float y, float angle, float length, float needle) {
    const float ux = std::cos(angle), uy = std::sin(angle);
    canvas.Stroke(x, y, x + ux * length, y + uy * length, 0.9f, kTwig);
    const float gap = 0.0085f;
    for (float s = 0.0f; s < length + needle * 0.3f; s += gap) {
        const float taper = s > length * 0.8f ? 0.75f : 1.0f;
        for (int side = -1; side <= 1; side += 2) {
            const float lean = angle + static_cast<float>(side) * dice.Range(0.55f, 1.0f);
            const float size = needle * taper * dice.Range(0.75f, 1.15f);
            const float bx = x + ux * std::min(s, length), by = y + uy * std::min(s, length);
            const float ex = bx + std::cos(lean) * size, ey = by + std::sin(lean) * size;
            if (!Canvas::Inside((bx + ex) * 0.5f, (by + ey) * 0.5f, size * 0.5f)) {
                continue;
            }
            canvas.Stroke(bx, by, ex, ey, 0.75f, LeafColor(dice.Range(0.5f, 1.0f), dice.Range(-0.4f, 0.5f)));
        }
    }
}

void PaintNeedleSpray(Canvas& canvas, std::uint32_t seed) {
    Dice dice(seed);
    const float needle = 0.034f;
    float x = 0.5f, y = 0.985f;
    float angle = -kPi * 0.5f + dice.Range(-0.06f, 0.06f);
    const int pieces = 11;
    const float piece = 0.9f / static_cast<float>(pieces);
    for (int i = 0; i < pieces; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(pieces);
        // Side twigs in pairs, long at the foot of the bough and short at its tip; the long
        // ones fork once more.
        for (int side = -1; side <= 1; side += 2) {
            const float out = angle + static_cast<float>(side) * dice.Range(0.75f, 1.0f);
            const float reach = (0.42f * (1.0f - t) + 0.05f) * dice.Range(0.85f, 1.1f);
            NeedleTwig(canvas, dice, x, y, out, reach, needle);
            for (float s = reach * dice.Range(0.3f, 0.45f); s < reach * 0.85f && reach > 0.16f;
                 s += reach * dice.Range(0.25f, 0.35f)) {
                for (int fork = -1; fork <= 1; fork += 2) {
                    NeedleTwig(canvas, dice, x + std::cos(out) * s, y + std::sin(out) * s,
                               out + static_cast<float>(fork) * dice.Range(0.5f, 0.8f),
                               (reach - s) * dice.Range(0.4f, 0.6f), needle);
                }
            }
        }
        NeedleTwig(canvas, dice, x, y, angle, piece, needle);
        x += std::cos(angle) * piece;
        y += std::sin(angle) * piece;
        angle += dice.Range(-0.05f, 0.05f);
    }
}

/// The colour of a leaf, a stalk or a petal.
Rgb Tone(float tone, float warm = 0.0f) {
    return LeafColor(tone, warm);
}

constexpr float kPalmStretch = 2.6f;
constexpr float kFernStretch = 2.4f;

/// A feather palm's frond from its foot at the bottom to its tip: a bare stalk, then the
/// rib with a leaflet every few centimetres on both sides, long in the lower half and
/// swept towards the tip further up. Some are torn off short, as on every palm.
void PaintPalmFrond(Canvas& canvas, std::uint32_t seed) {
    Dice dice(seed);
    const float foot = 0.988f, tip = 0.085f;
    const auto rib = [&](float t) { return foot + (tip - foot) * t; };
    for (int side = -1; side <= 1; side += 2) {
        for (float t = 0.12f; t < 0.995f; t += 0.0105f * dice.Range(0.8f, 1.2f)) {
            const float reach = 0.035f + 0.43f * std::pow(std::sin(kPi * std::pow(t, 0.55f)), 0.8f);
            const float sweep = 1.3f - 0.85f * t + dice.Range(-0.08f, 0.08f);  // from the rib's direction
            float length = reach / std::sin(sweep) * dice.Range(0.86f, 1.05f);
            if (dice.Next() < 0.07f) {
                length *= dice.Range(0.3f, 0.7f);
            }
            const float dx = static_cast<float>(side) * std::sin(sweep), dy = -std::cos(sweep);
            if (!Canvas::Inside(0.5f + dx * length, rib(t) + dy * length / kPalmStretch, 0.012f)) {
                continue;
            }
            canvas.Leaf(0.5f, rib(t), std::atan2(dy, dx), length, 0.021f * dice.Range(0.85f, 1.15f),
                        Tone(dice.Range(0.58f, 1.0f), dice.Range(-0.2f, 0.5f)));
        }
    }
    const int pieces = 10;
    for (int i = 0; i < pieces; ++i) {
        const float t0 = static_cast<float>(i) / static_cast<float>(pieces);
        const float t1 = static_cast<float>(i + 1) / static_cast<float>(pieces);
        canvas.Stroke(0.5f, rib(t0), 0.5f, rib(t1), 4.6f - 3.4f * t0, Tone(0.98f, 0.9f));
    }
}

/// A fern's frond: twice divided, widest a quarter of the way up and tapering to its tip.
void PaintFernFrond(Canvas& canvas, std::uint32_t seed) {
    Dice dice(seed);
    const float foot = 0.988f, tip = 0.05f;
    const auto rib = [&](float t) { return foot + (tip - foot) * t; };
    for (int side = -1; side <= 1; side += 2) {
        for (float t = 0.1f; t < 0.985f; t += 0.03f * dice.Range(0.9f, 1.1f)) {
            const float profile =
                t < 0.3f ? 0.6f + 0.4f * (t - 0.1f) / 0.2f : 1.0f - 0.9f * (t - 0.3f) / 0.7f;
            const float sweep = 1.25f - 0.4f * t + dice.Range(-0.06f, 0.06f);
            const float length = 0.44f * profile / std::sin(sweep) * dice.Range(0.92f, 1.04f);
            const float dx = static_cast<float>(side) * std::sin(sweep), dy = -std::cos(sweep);
            const float angle = std::atan2(dy, dx);
            const float y = rib(t);
            const Rgb color = Tone(dice.Range(0.62f, 1.0f), dice.Range(-0.1f, 0.5f));
            // The little leaves of this one, shorter towards its tip.
            for (float s = 0.012f; s < length; s += 0.0165f) {
                const float size = 0.062f * (1.0f - 0.72f * s / length) * std::sqrt(std::max(profile, 0.2f));
                const float px = 0.5f + dx * s, py = y + dy * s / kFernStretch;
                for (int both = -1; both <= 1; both += 2) {
                    const float turn = angle + static_cast<float>(both) * dice.Range(1.0f, 1.25f);
                    const float ex = px + std::cos(turn) * size,
                                ey = py + std::sin(turn) * size / kFernStretch;
                    if (!Canvas::Inside(ex, ey, 0.004f)) {
                        continue;
                    }
                    canvas.Leaf(px, py, turn, size, size * 0.46f, color);
                }
            }
            canvas.Stroke(0.5f, y, 0.5f + dx * length * 0.92f, y + dy * length * 0.92f / kFernStretch, 0.8f,
                          Tone(0.8f, 0.7f));
        }
    }
    canvas.Stroke(0.5f, foot, 0.5f, tip, 2.4f, Tone(0.62f, 1.0f));
    canvas.Stroke(0.5f, foot, 0.5f, rib(0.5f), 3.4f, Tone(0.62f, 1.0f));
}

/// A blade of grass or reed from (x, 0.985): `lean` (radians from upright) grows by `bend`
/// towards the tip.
void Blade(Canvas& canvas, float x, float height, float lean, float bend, float halfWidth, const Rgb& color) {
    constexpr int kPieces = 8;
    float px = x, py = 0.985f;
    for (int i = 0; i < kPieces; ++i) {
        const float u = (static_cast<float>(i) + 0.5f) / static_cast<float>(kPieces);
        const float angle = lean + bend * u * u;
        const float nx = px + std::sin(angle) * height / static_cast<float>(kPieces);
        const float ny = py - std::cos(angle) * height / static_cast<float>(kPieces);
        if (!Canvas::Inside(nx, ny, 0.012f)) {
            break;
        }
        canvas.Stroke(px, py, nx, ny, std::max(0.55f, halfWidth * (1.0f - 0.8f * u)), color);
        px = nx;
        py = ny;
    }
}

struct BladeMark {
    float x, height, lean, bend, width, tone, warm;
};

void PaintBlades(Canvas& canvas, Dice& dice, int count, float spread, float fan, float minHeight,
                 float width) {
    std::vector<BladeMark> blades;
    for (int i = 0; i < count; ++i) {
        const float side = dice.Range(-1.0f, 1.0f);
        blades.push_back({0.5f + side * spread, dice.Range(minHeight, 0.95f),
                          side * fan * dice.Range(0.5f, 1.2f),
                          (side < 0.0f ? -1.0f : 1.0f) * dice.Range(0.1f, 1.0f) * fan * 2.0f,
                          width * dice.Range(0.75f, 1.25f), dice.Range(0.5f, 1.0f),
                          dice.Range(-0.3f, 0.9f) * dice.Next()});
    }
    std::stable_sort(blades.begin(), blades.end(),
                     [](const BladeMark& a, const BladeMark& b) { return a.tone < b.tone; });
    for (const BladeMark& b : blades) {
        Blade(canvas, b.x, b.height, b.lean, b.bend, b.width, Tone(b.tone, b.warm));
    }
}

/// A tuft of grass: blades fanning out of one foot, the outer ones bowing over.
void PaintGrassTuft(Canvas& canvas, std::uint32_t seed) {
    Dice dice(seed);
    PaintBlades(canvas, dice, 54, 0.1f, 0.5f, 0.42f, 2.3f);
}

/// Reeds: stiffer, broader blades and a few stalks with the brown cobs of a bulrush.
void PaintReeds(Canvas& canvas, std::uint32_t seed) {
    Dice dice(seed);
    PaintBlades(canvas, dice, 22, 0.2f, 0.24f, 0.6f, 3.0f);
    // What the material's green makes a warm brown of.
    const Rgb cob{0.95f, 0.3f, 0.22f};
    for (int i = 0; i < 5; ++i) {
        const float x = 0.5f + dice.Range(-0.24f, 0.24f);
        const float lean = dice.Range(-0.12f, 0.12f);
        const float height = dice.Range(0.68f, 0.88f);
        const auto at = [&](float h, float& ox, float& oy) {
            ox = x + std::sin(lean) * h;
            oy = 0.985f - std::cos(lean) * h;
        };
        float x0, y0, x1, y1;
        at(0.0f, x0, y0);
        at(height, x1, y1);
        canvas.Stroke(x0, y0, x1, y1, 1.2f, Tone(0.8f, 0.6f));
        at(height - 0.2f, x0, y0);
        at(height - 0.06f, x1, y1);
        canvas.Stroke(x0, y0, x1, y1, dice.Range(4.2f, 5.4f), cob);
    }
}

/// A twig of bamboo: thin side twigs, each ending in a fan of long narrow leaves.
void PaintBambooTwig(Canvas& canvas, std::uint32_t seed) {
    Dice dice(seed);
    const Rgb cane = Tone(0.9f, 1.0f);
    const auto fan = [&](float x, float y, float towards, int leaves) {
        for (int i = 0; i < leaves; ++i) {
            const float angle =
                towards +
                (static_cast<float>(i) / static_cast<float>(std::max(1, leaves - 1)) - 0.5f) * 1.5f +
                dice.Range(-0.12f, 0.12f);
            float length = dice.Range(0.2f, 0.27f);
            while (length > 0.08f &&
                   !Canvas::Inside(x + std::cos(angle) * length, y + std::sin(angle) * length, 0.01f)) {
                length *= 0.85f;
            }
            if (length > 0.08f) {
                canvas.Leaf(x, y, angle, length, length * dice.Range(0.17f, 0.22f),
                            Tone(dice.Range(0.6f, 1.0f), dice.Range(-0.2f, 0.6f)));
            }
        }
    };
    float x = 0.5f, y = 0.985f;
    float angle = -kPi * 0.5f + dice.Range(-0.08f, 0.08f);
    int side = dice.Next() < 0.5f ? 1 : -1;
    const int nodes = 6;
    for (int i = 0; i < nodes; ++i) {
        const float piece = 0.12f;
        const float nx = x + std::cos(angle) * piece, ny = y + std::sin(angle) * piece;
        canvas.Stroke(x, y, nx, ny, 1.5f - 0.15f * static_cast<float>(i), cane);
        x = nx;
        y = ny;
        const float out = angle + static_cast<float>(side) * dice.Range(0.7f, 1.1f);
        const float reach = dice.Range(0.1f, 0.2f);
        const float ex = x + std::cos(out) * reach, ey = y + std::sin(out) * reach;
        if (Canvas::Inside(ex, ey, 0.02f)) {
            canvas.Stroke(x, y, ex, ey, 0.8f, cane);
            // The leaves hang a little: towards the twig's direction, turned downwards.
            fan(ex, ey, out + static_cast<float>(side) * 0.35f,
                3 + static_cast<int>(dice.Range(0.0f, 2.99f)));
        }
        side = -side;
        angle += dice.Range(-0.12f, 0.12f);
    }
    fan(x, y, angle, 4);
}

/// Flower heads over thin stalks, white so that the material gives them their colour:
/// daisies of some seven petals (`umbels` false) or domes of small four-petalled florets.
void PaintBlossoms(Canvas& canvas, std::uint32_t seed, bool umbels) {
    Dice dice(seed);
    struct Head {
        float x, y;
    };
    std::vector<Head> heads;
    const float apart = umbels ? 0.2f : 0.135f;
    for (int tries = 0; tries < 400 && heads.size() < (umbels ? 11u : 24u); ++tries) {
        const Head h{dice.Range(0.1f, 0.9f), dice.Range(0.1f, 0.74f)};
        bool clear = true;
        for (const Head& other : heads) {
            clear = clear && std::hypot(h.x - other.x, h.y - other.y) > apart;
        }
        if (clear) {
            heads.push_back(h);
        }
    }
    const Rgb stalk{0.1f, 0.22f, 0.08f};
    const Rgb eye{0.3f, 0.2f, 0.04f};
    for (const Head& h : heads) {
        canvas.Stroke(h.x, h.y, h.x + dice.Range(-0.03f, 0.03f),
                      std::min(0.97f, h.y + dice.Range(0.16f, 0.26f)), 0.9f, stalk);
    }
    for (const Head& h : heads) {
        if (umbels) {
            for (int i = 0; i < 12; ++i) {
                const float a = dice.Range(0.0f, 2.0f * kPi);
                const float r = 0.07f * std::sqrt(dice.Next());
                const float fx = h.x + std::cos(a) * r, fy = h.y + std::sin(a) * r * 0.7f;
                const float turn = dice.Range(0.0f, kPi);
                const Rgb petal = Tone(dice.Range(0.7f, 1.0f));
                for (int p = 0; p < 4; ++p) {
                    canvas.Leaf(fx, fy, turn + kPi * 0.5f * static_cast<float>(p), 0.026f, 0.02f, petal);
                }
            }
        } else {
            const int petals = 6 + static_cast<int>(dice.Range(0.0f, 2.99f));
            const float turn = dice.Range(0.0f, kPi);
            const float size = dice.Range(0.048f, 0.062f);
            for (int p = 0; p < petals; ++p) {
                canvas.Leaf(h.x, h.y, turn + 2.0f * kPi * static_cast<float>(p) / static_cast<float>(petals),
                            size, size * 0.55f, Tone(dice.Range(0.82f, 1.0f)));
            }
            canvas.Stroke(h.x, h.y, h.x, h.y, 2.6f, eye);
        }
    }
}

template <typename Painter>
SprayAtlas Paint(std::uint32_t seed, Painter&& painter) {
    SprayAtlas atlas;
    atlas.size = kCell * kCells;
    atlas.cells = kCells;
    atlas.rgba.assign(static_cast<std::size_t>(atlas.size) * static_cast<std::size_t>(atlas.size) * 4u, 0);
    for (int cy = 0; cy < kCells; ++cy) {
        for (int cx = 0; cx < kCells; ++cx) {
            Canvas canvas;
            painter(canvas, seed + static_cast<std::uint32_t>(cy * kCells + cx) * 7919u);
            canvas.Write(atlas, cx * kCell, cy * kCell, 24);
        }
    }
    return atlas;
}

constexpr int kPlantAtlas = 1024;

/// Where each plant card lies in its atlas, in texels: x, y, edge.
constexpr int kPlantCells[static_cast<std::size_t>(PlantCard::Count)][3] = {
    {0, 0, 512},     {512, 0, 512}, {0, 512, 256},   {256, 512, 256}, {512, 512, 256},
    {768, 512, 256}, {0, 768, 256}, {256, 768, 256}, {512, 768, 256}, {768, 768, 256},
};

}  // namespace

SprayAtlas LeafSprays() {
    return Paint(11u, PaintLeafSpray);
}

SprayAtlas NeedleSprays() {
    return Paint(23u, PaintNeedleSpray);
}

SprayAtlas PlantCards() {
    SprayAtlas atlas;
    atlas.size = kPlantAtlas;
    atlas.cells = 0;
    atlas.rgba.assign(static_cast<std::size_t>(atlas.size) * static_cast<std::size_t>(atlas.size) * 4u, 0);
    for (std::size_t i = 0; i < static_cast<std::size_t>(PlantCard::Count); ++i) {
        const auto card = static_cast<PlantCard>(i);
        const std::uint32_t seed = 101u + static_cast<std::uint32_t>(i) * 7919u;
        const float stretch = card == PlantCard::PalmFrond   ? kPalmStretch
                              : card == PlantCard::FernFrond ? kFernStretch
                                                             : 1.0f;
        Canvas canvas(kPlantCells[i][2], stretch);
        switch (card) {
            case PlantCard::PalmFrond:
                PaintPalmFrond(canvas, seed);
                break;
            case PlantCard::FernFrond:
                PaintFernFrond(canvas, seed);
                break;
            case PlantCard::GrassA:
            case PlantCard::GrassB:
                PaintGrassTuft(canvas, seed);
                break;
            case PlantCard::ReedsA:
            case PlantCard::ReedsB:
                PaintReeds(canvas, seed);
                break;
            case PlantCard::BambooA:
            case PlantCard::BambooB:
                PaintBambooTwig(canvas, seed);
                break;
            case PlantCard::Daisies:
            case PlantCard::Umbels:
                PaintBlossoms(canvas, seed, card == PlantCard::Umbels);
                break;
            case PlantCard::Count:
                break;
        }
        canvas.Write(atlas, kPlantCells[i][0], kPlantCells[i][1], 10);
    }
    return atlas;
}

CardRect RectOf(PlantCard card) {
    const int* cell = kPlantCells[static_cast<std::size_t>(card)];
    const float size = static_cast<float>(kPlantAtlas);
    return {static_cast<float>(cell[0]) / size, static_cast<float>(cell[1]) / size,
            static_cast<float>(cell[0] + cell[2]) / size, static_cast<float>(cell[1] + cell[2]) / size};
}

}  // namespace zoogame
