// SPDX-License-Identifier: MIT
#include "FoliageTextures.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace zoogame {

namespace {

constexpr float kPi = std::numbers::pi_v<float>;
constexpr int kCell = 256;   ///< texels along a cell
constexpr int kCells = 2;
constexpr int kMargin = 4;   ///< clear texels round every cell

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

/// One cell being painted: linear colour and coverage, later things over earlier ones.
class Canvas {
public:
    Canvas() : color_(static_cast<std::size_t>(kCell * kCell), Rgb{0, 0, 0}), alpha_(color_.size(), 0.0f) {}

    /// Whether a disc lies wholly inside the margin (coordinates in cells, 0..1).
    static bool Inside(float x, float y, float radius) {
        const float m = static_cast<float>(kMargin) / static_cast<float>(kCell) + radius;
        return x > m && x < 1.0f - m && y > m && y < 1.0f - m;
    }

    /// A straight stroke with round ends; widths in texels.
    void Stroke(float x0, float y0, float x1, float y1, float halfWidth, const Rgb& color) {
        const float ax = x0 * kCell, ay = y0 * kCell, bx = x1 * kCell, by = y1 * kCell;
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
        const float ax = x * kCell, ay = y * kCell;
        const float ux = std::cos(angle), uy = std::sin(angle);
        const float L = length * kCell;
        const float W = width * kCell * 0.5f;
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

    /// Gives the clear texels the colour of the nearest painted ones and writes the cell
    /// into the atlas.
    void Write(SprayAtlas& atlas, int cellX, int cellY) {
        std::vector<float> known = alpha_;
        for (int pass = 0; pass < 24; ++pass) {
            std::vector<Rgb> nextColor = color_;
            std::vector<float> nextKnown = known;
            for (int y = 0; y < kCell; ++y) {
                for (int x = 0; x < kCell; ++x) {
                    const std::size_t i = At(x, y);
                    if (known[i] > 0.0f) {
                        continue;
                    }
                    Rgb sum{0, 0, 0};
                    float weight = 0.0f;
                    for (int oy = -1; oy <= 1; ++oy) {
                        for (int ox = -1; ox <= 1; ++ox) {
                            const int nx = x + ox, ny = y + oy;
                            if (nx < 0 || ny < 0 || nx >= kCell || ny >= kCell || known[At(nx, ny)] <= 0.0f) {
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
        for (int y = 0; y < kCell; ++y) {
            for (int x = 0; x < kCell; ++x) {
                const std::size_t i = At(x, y);
                const Rgb& c = known[i] > 0.0f ? color_[i] : mean;
                const std::size_t o =
                    (static_cast<std::size_t>(cellY * kCell + y) * static_cast<std::size_t>(atlas.size) +
                     static_cast<std::size_t>(cellX * kCell + x)) *
                    4u;
                atlas.rgba[o] = encode(c.r);
                atlas.rgba[o + 1] = encode(c.g);
                atlas.rgba[o + 2] = encode(c.b);
                atlas.rgba[o + 3] = static_cast<std::uint8_t>(std::lround(std::clamp(alpha_[i], 0.0f, 1.0f) * 255.0f));
            }
        }
    }

private:
    struct Hit {
        float inside;  ///< texels inside the shape's edge (negative: outside)
        Rgb color;
    };

    static std::size_t At(int x, int y) {
        return static_cast<std::size_t>(y) * static_cast<std::size_t>(kCell) + static_cast<std::size_t>(x);
    }

    template <typename Shape>
    void Box(float minX, float minY, float maxX, float maxY, Shape&& shape) {
        const int x0 = std::max(kMargin, static_cast<int>(std::floor(minX)));
        const int y0 = std::max(kMargin, static_cast<int>(std::floor(minY)));
        const int x1 = std::min(kCell - 1 - kMargin, static_cast<int>(std::ceil(maxX)));
        const int y1 = std::min(kCell - 1 - kMargin, static_cast<int>(std::ceil(maxY)));
        for (int y = y0; y <= y1; ++y) {
            for (int x = x0; x <= x1; ++x) {
                const Hit hit = shape(static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f);
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
            for (float s = reach * dice.Range(0.25f, 0.4f); s < reach * 0.9f; s += reach * dice.Range(0.22f, 0.34f)) {
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
        leaves.push_back({0.5f + std::cos(a) * r, 0.48f + std::sin(a) * r * 1.05f, dice.Range(0.0f, 2.0f * kPi),
                          size, size * dice.Range(0.42f, 0.56f), -1.0f, 0.0f});
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
            canvas.Write(atlas, cx, cy);
        }
    }
    return atlas;
}

}  // namespace

SprayAtlas LeafSprays() {
    return Paint(11u, PaintLeafSpray);
}

SprayAtlas NeedleSprays() {
    return Paint(23u, PaintNeedleSpray);
}

}  // namespace zoogame
