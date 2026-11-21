// SPDX-License-Identifier: MIT
#include "SignTextures.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <string_view>

namespace zoogame {

namespace {

constexpr int kSheet = 1024;

struct Ink {
    std::uint8_t r, g, b;
};

constexpr Ink kCream{238, 231, 208};
constexpr Ink kWhite{246, 245, 240};
constexpr Ink kInk{40, 36, 30};
constexpr Ink kGreen{28, 84, 48};
constexpr Ink kDeepGreen{20, 62, 38};
constexpr Ink kBlue{30, 76, 132};
constexpr Ink kRed{168, 34, 28};
constexpr Ink kYellow{236, 190, 40};
constexpr Ink kBrown{104, 70, 40};
constexpr Ink kSlate{44, 50, 52};
constexpr Ink kOrange{196, 104, 30};

/// A rectangle of the sheet in texels.
struct Area {
    float x, y, w, h;

    Area Inset(float by) const { return {x + by, y + by, w - 2.0f * by, h - 2.0f * by}; }
    float MidX() const { return x + 0.5f * w; }
    float MidY() const { return y + 0.5f * h; }
};

constexpr std::array<Area, static_cast<std::size_t>(Sign::Count)> kAreas{{
    {0, 0, 768, 192},      // Zoo
    {768, 0, 256, 192},    // Menu
    {0, 192, 512, 128},    // Toilets
    {512, 192, 512, 128},  // GiftShop
    {0, 320, 512, 128},    // Drinks
    {512, 320, 512, 128},  // IceCream
    {0, 448, 512, 128},    // Burgers
    {512, 448, 512, 128},  // Education
    {0, 604, 256, 64},     // Research
    {256, 604, 256, 64},   // Vet
    {0, 704, 256, 128},    // Staff
    {256, 704, 256, 128},  // StaffOnly
    {512, 704, 256, 128},  // Danger
    {768, 704, 256, 128},  // DoNotFeed
    {0, 832, 256, 192},    // Board0
    {256, 832, 256, 192},  // Board1
    {512, 832, 256, 192},  // Board2
    {768, 832, 256, 192},  // Board3
    {512, 604, 256, 64},   // Snake Pavilion
    {768, 604, 256, 64},   // Woodland Aviary
}};

/// The strokes of a capital on a grid 8 wide (some letters more or less) and 12 high, the
/// base line at 0: polylines separated by spaces, each point two characters (0-9, a = 10 ...).
std::string_view Strokes(char letter) {
    switch (letter) {
        case 'A':
            return "00 4c 80|13 73";
        case 'B':
            return "00 0c 5c 7a 78 56 06|56 84 82 60 00";
        case 'C':
            return "8a 6c 2c 0a 02 20 60 82";
        case 'D':
            return "00 0c 5c 89 83 50 00";
        case 'E':
            return "80 00 0c 8c|06 66";
        case 'F':
            return "00 0c 8c|06 66";
        case 'G':
            return "8a 6c 2c 0a 02 20 60 82 86 46";
        case 'H':
            return "00 0c|80 8c|06 86";
        case 'I':
            return "00 0c";
        case 'J':
            return "6c 62 40 20 02";
        case 'K':
            return "00 0c|8c 05|37 80";
        case 'L':
            return "0c 00 70";
        case 'M':
            return "00 0c 55 ac a0";
        case 'N':
            return "00 0c 80 8c";
        case 'O':
            return "2c 6c 8a 82 60 20 02 0a 2c";
        case 'P':
            return "00 0c 6c 8a 87 65 05";
        case 'Q':
            return "2c 6c 8a 82 60 20 02 0a 2c|54 80";
        case 'R':
            return "00 0c 6c 8a 87 65 05|55 80";
        case 'S':
            return "8a 6c 2c 0a 08 26 66 84 82 60 20 02";
        case 'T':
            return "0c 8c|4c 40";
        case 'U':
            return "0c 02 20 60 82 8c";
        case 'V':
            return "0c 40 8c";
        case 'W':
            return "0c 30 68 90 cc";
        case 'X':
            return "00 8c|0c 80";
        case 'Y':
            return "0c 46 8c|46 40";
        case 'Z':
            return "0c 8c 00 80";
        case '-':
            return "26 66";
        default:
            return "";
    }
}

float Grid(char c) {
    return static_cast<float>(c <= '9' ? c - '0' : 10 + (c - 'a'));
}

/// How far a letter reaches to the right, in grid units.
float Reach(char letter) {
    float reach = 0.0f;
    const std::string_view strokes = Strokes(letter);
    for (std::size_t i = 0; i + 1 < strokes.size(); i += 3) {
        reach = std::max(reach, Grid(strokes[i]));
    }
    return reach;
}

constexpr float kLetterGap = 3.4f;  ///< between letters, in grid units (the strokes' width included)
constexpr float kWordGap = 6.0f;

class Sheet {
public:
    Sheet() : rgba_(static_cast<std::size_t>(kSheet * kSheet * 4), 255) {}

    std::vector<std::uint8_t> Take() { return std::move(rgba_); }

    void Fill(const Area& area, Ink ink) {
        Cover(area.x, area.y, area.x + area.w, area.y + area.h, [&](float, float) { return Hit{1.0f, ink}; });
    }

    /// A straight stroke with round ends.
    void Line(float x0, float y0, float x1, float y1, float halfWidth, Ink ink) {
        const float dx = x1 - x0, dy = y1 - y0;
        const float lengthSq = std::max(dx * dx + dy * dy, 1e-6f);
        const float reach = halfWidth + 1.0f;
        Cover(std::min(x0, x1) - reach, std::min(y0, y1) - reach, std::max(x0, x1) + reach,
              std::max(y0, y1) + reach, [&](float px, float py) {
                  const float t = std::clamp(((px - x0) * dx + (py - y0) * dy) / lengthSq, 0.0f, 1.0f);
                  const float ex = px - (x0 + dx * t), ey = py - (y0 + dy * t);
                  return Hit{halfWidth - std::sqrt(ex * ex + ey * ey) + 0.5f, ink};
              });
    }

    void Disc(float x, float y, float radius, Ink ink) { Line(x, y, x, y, radius, ink); }

    static float TextWidth(std::string_view text, float height) {
        float width = 0.0f;
        for (const char letter : text) {
            width += letter == ' ' ? kWordGap : Reach(letter) + kLetterGap;
        }
        return (width - kLetterGap) * height / 12.0f;
    }

    /// Capitals `height` tall with their base line at `base`, from `x` on.
    void Text(std::string_view text, float x, float base, float height, Ink ink, float weight = 0.085f) {
        const float unit = height / 12.0f;
        for (const char letter : text) {
            if (letter == ' ') {
                x += kWordGap * unit;
                continue;
            }
            const std::string_view strokes = Strokes(letter);
            for (std::size_t i = 0; i + 4 < strokes.size(); i += 3) {
                if (strokes[i + 2] == '|') {
                    continue;
                }
                Line(x + Grid(strokes[i]) * unit, base - Grid(strokes[i + 1]) * unit,
                     x + Grid(strokes[i + 3]) * unit, base - Grid(strokes[i + 4]) * unit, weight * height,
                     ink);
            }
            x += (Reach(letter) + kLetterGap) * unit;
        }
    }

    /// Centred on (midX, midY), and no wider than `room`.
    void Centred(std::string_view text, float midX, float midY, float height, float room, Ink ink,
                 float weight = 0.085f) {
        const float wide = TextWidth(text, height);
        if (wide > room) {
            height *= room / wide;
        }
        Text(text, midX - 0.5f * TextWidth(text, height), midY + 0.5f * height, height, ink, weight);
    }

    /// A panel in one colour with a thin line a little inside its edge. The colour runs to
    /// the edge of the area, so that the smaller mip levels do not pull a neighbour in.
    void Plate(const Area& area, Ink ground, Ink line) {
        Fill(area, ground);
        const Area in = area.Inset(7.0f);
        Outline(in, 1.4f, line);
    }

    void Outline(const Area& in, float halfWidth, Ink line) {
        Line(in.x, in.y, in.x + in.w, in.y, halfWidth, line);
        Line(in.x, in.y + in.h, in.x + in.w, in.y + in.h, halfWidth, line);
        Line(in.x, in.y, in.x, in.y + in.h, halfWidth, line);
        Line(in.x + in.w, in.y, in.x + in.w, in.y + in.h, halfWidth, line);
    }

    /// A paw print about `size` across.
    void Paw(float x, float y, float size, Ink ink) {
        Disc(x, y + 0.14f * size, 0.27f * size, ink);
        Disc(x - 0.16f * size, y + 0.26f * size, 0.17f * size, ink);
        Disc(x + 0.16f * size, y + 0.26f * size, 0.17f * size, ink);
        const float toes[4][2] = {{-0.36f, -0.08f}, {-0.13f, -0.3f}, {0.13f, -0.3f}, {0.36f, -0.08f}};
        for (const auto& toe : toes) {
            Disc(x + toe[0] * size, y + toe[1] * size, 0.115f * size, ink);
        }
    }

    /// Lines of print too small to read: grey bars of uneven length.
    void Print(const Area& area, float pitch, std::uint32_t seed, Ink ink) {
        std::uint32_t state = seed * 747796405u + 2891336453u;
        for (float y = area.y + 0.5f * pitch; y < area.y + area.h; y += pitch) {
            state = state * 1664525u + 1013904223u;
            const float share = 0.62f + 0.38f * static_cast<float>(state >> 24) / 255.0f;
            // A paragraph ends now and then.
            const bool last = (state >> 16) % 5u == 0u;
            Line(area.x, y, area.x + area.w * (last ? 0.45f * share : share), y, 0.22f * pitch, ink);
        }
    }

private:
    struct Hit {
        float cover;  ///< 1 inside, 0 outside, between at the edge
        Ink ink;
    };

    template <typename Shape>
    void Cover(float x0, float y0, float x1, float y1, Shape&& shape) {
        const int left = std::max(0, static_cast<int>(std::floor(x0)));
        const int top = std::max(0, static_cast<int>(std::floor(y0)));
        const int right = std::min(kSheet, static_cast<int>(std::ceil(x1)));
        const int bottom = std::min(kSheet, static_cast<int>(std::ceil(y1)));
        for (int y = top; y < bottom; ++y) {
            for (int x = left; x < right; ++x) {
                const Hit hit = shape(static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f);
                const float cover = std::clamp(hit.cover, 0.0f, 1.0f);
                if (cover <= 0.0f) {
                    continue;
                }
                std::uint8_t* texel = &rgba_[static_cast<std::size_t>((y * kSheet + x) * 4)];
                const std::uint8_t ink[3] = {hit.ink.r, hit.ink.g, hit.ink.b};
                for (int c = 0; c < 3; ++c) {
                    texel[c] = static_cast<std::uint8_t>(std::lround(
                        static_cast<float>(texel[c]) * (1.0f - cover) + static_cast<float>(ink[c]) * cover));
                }
            }
        }
    }

    std::vector<std::uint8_t> rgba_;
};

const Area& AreaOf(Sign sign) {
    return kAreas[static_cast<std::size_t>(sign)];
}

/// A building's name on a board of its trade's colour.
void NameBoard(Sheet& sheet, Sign sign, std::string_view name, Ink ground) {
    const Area& area = AreaOf(sign);
    sheet.Plate(area, ground, kCream);
    sheet.Centred(name, area.MidX(), area.MidY(), 0.46f * area.h, area.w - 56.0f, kCream);
}

/// The little man and woman of a toilet door, feet at `base`.
void Figures(Sheet& sheet, float x, float base, float height, Ink ink) {
    const float u = height / 10.0f;
    // He: head, shoulders, legs.
    sheet.Disc(x, base - 9.0f * u, 1.0f * u, ink);
    sheet.Line(x - 1.2f * u, base - 7.0f * u, x + 1.2f * u, base - 7.0f * u, 0.8f * u, ink);
    sheet.Line(x - 1.2f * u, base - 7.0f * u, x - 1.2f * u, base - 4.4f * u, 0.5f * u, ink);
    sheet.Line(x + 1.2f * u, base - 7.0f * u, x + 1.2f * u, base - 4.4f * u, 0.5f * u, ink);
    sheet.Line(x - 0.6f * u, base - 6.6f * u, x - 0.6f * u, base - 0.6f * u, 0.6f * u, ink);
    sheet.Line(x + 0.6f * u, base - 6.6f * u, x + 0.6f * u, base - 0.6f * u, 0.6f * u, ink);
    // She: head, a dress widening to the knee, legs.
    const float s = x + 5.4f * u;
    sheet.Disc(s, base - 9.0f * u, 1.0f * u, ink);
    for (int i = 0; i <= 8; ++i) {
        const float t = static_cast<float>(i) / 8.0f;
        const float y = base - (7.4f - 3.8f * t) * u;
        const float half = (0.7f + 1.5f * t) * u;
        sheet.Line(s - half, y, s + half, y, 0.35f * u, ink);
    }
    sheet.Line(s - 0.6f * u, base - 3.6f * u, s - 0.6f * u, base - 0.6f * u, 0.5f * u, ink);
    sheet.Line(s + 0.6f * u, base - 3.6f * u, s + 0.6f * u, base - 0.6f * u, 0.5f * u, ink);
}

/// A plate with one or two lines of capitals.
void Plaque(Sheet& sheet, Sign sign, std::string_view first, std::string_view second, Ink ground, Ink ink) {
    const Area& area = AreaOf(sign);
    sheet.Plate(area, ground, ink);
    if (second.empty()) {
        sheet.Centred(first, area.MidX(), area.MidY(), 0.4f * area.h, area.w - 44.0f, ink);
        return;
    }
    sheet.Centred(first, area.MidX(), area.y + 0.34f * area.h, 0.27f * area.h, area.w - 40.0f, ink);
    sheet.Centred(second, area.MidX(), area.y + 0.69f * area.h, 0.2f * area.h, area.w - 40.0f, ink);
}

/// The panel of an information board: a band with the heading, a picture, a map of where
/// the animal lives, and the text. Four layouts in four colours.
void BoardPanel(Sheet& sheet, int variant) {
    const Area& area = AreaOf(static_cast<Sign>(static_cast<int>(Sign::Board0) + variant));
    const Ink bands[4] = {kGreen, kBlue, kBrown, kOrange};
    const Ink band = bands[variant];
    sheet.Fill(area, kCream);
    // The band: a paw, the name (a bar where the name would be) and the smaller Latin one.
    sheet.Fill({area.x, area.y, area.w, 44.0f}, band);
    sheet.Paw(area.x + 26.0f, area.y + 21.0f, 24.0f, kCream);
    sheet.Line(area.x + 52.0f, area.y + 17.0f, area.x + 150.0f + 18.0f * static_cast<float>(variant),
               area.y + 17.0f, 4.2f, kCream);
    sheet.Line(area.x + 52.0f, area.y + 32.0f, area.x + 122.0f, area.y + 32.0f, 1.6f, kCream);
    // The picture: sky, a far hill, the ground, a sun.
    const bool left = variant % 2 == 0;
    const Area picture{left ? area.x + 12.0f : area.x + 132.0f, area.y + 54.0f, 112.0f, 74.0f};
    sheet.Fill(picture, Ink{150, 186, 214});
    sheet.Disc(picture.x + (left ? 86.0f : 24.0f), picture.y + 18.0f, 8.0f, Ink{244, 232, 170});
    for (int i = 0; i <= 16; ++i) {
        const float t = static_cast<float>(i) / 16.0f;
        const float rise =
            10.0f * std::sin(3.1416f * t) + 4.0f * std::sin(9.0f * t + static_cast<float>(variant));
        sheet.Line(picture.x + 4.0f + t * (picture.w - 8.0f), picture.y + 46.0f - rise,
                   picture.x + 4.0f + t * (picture.w - 8.0f), picture.y + picture.h - 6.0f, 4.0f,
                   variant == 3 ? Ink{196, 170, 110} : Ink{96, 132, 84});
    }
    sheet.Fill({picture.x, picture.y + 54.0f, picture.w, 20.0f},
               variant == 3 ? Ink{210, 184, 124} : (variant == 1 ? Ink{214, 226, 232} : Ink{128, 150, 84}));
    sheet.Outline(picture, 1.2f, kInk);
    // The map: sea, land, a mark where the animal is at home.
    const Area map{left ? area.x + 12.0f : area.x + 132.0f, area.y + 136.0f, 112.0f, 46.0f};
    sheet.Fill(map, Ink{176, 204, 220});
    const float lands[5][3] = {{20, 16, 11}, {30, 30, 8}, {58, 14, 10}, {62, 30, 9}, {90, 22, 11}};
    for (const auto& land : lands) {
        sheet.Disc(map.x + land[0], map.y + land[1], land[2], Ink{150, 170, 118});
    }
    const float home[4][2] = {{60, 28}, {22, 14}, {88, 24}, {58, 16}};
    sheet.Disc(map.x + home[variant][0], map.y + home[variant][1], 5.0f, kRed);
    sheet.Outline(map, 1.0f, kInk);
    // The text beside them: a sub-heading and paragraphs.
    const Area text{left ? area.x + 136.0f : area.x + 12.0f, area.y + 56.0f, 108.0f, 126.0f};
    sheet.Line(text.x, text.y + 4.0f, text.x + 64.0f, text.y + 4.0f, 2.4f, band);
    sheet.Print({text.x, text.y + 12.0f, text.w, text.h - 12.0f}, 6.0f,
                17u + static_cast<std::uint32_t>(variant), Ink{96, 90, 80});
}

}  // namespace

SignSheet PaintSigns() {
    Sheet sheet;
    sheet.Fill({0, 0, kSheet, kSheet}, kCream);

    // Over the gate: the word, a paw on either side.
    {
        const Area& area = AreaOf(Sign::Zoo);
        sheet.Fill(area, kDeepGreen);
        sheet.Outline(area.Inset(10.0f), 2.4f, kCream);
        sheet.Centred("ZOO", area.MidX(), area.MidY(), 104.0f, 400.0f, kCream, 0.1f);
        sheet.Paw(area.x + 150.0f, area.MidY(), 84.0f, kCream);
        sheet.Paw(area.x + area.w - 150.0f, area.MidY(), 84.0f, kCream);
    }
    // A stall's board: what there is and what it costs, chalked.
    {
        const Area& area = AreaOf(Sign::Menu);
        sheet.Plate(area, kSlate, Ink{150, 150, 140});
        sheet.Centred("MENU", area.MidX(), area.y + 34.0f, 26.0f, 200.0f, kWhite);
        for (int row = 0; row < 5; ++row) {
            const float y = area.y + 70.0f + 24.0f * static_cast<float>(row);
            sheet.Line(area.x + 26.0f, y, area.x + 118.0f + 14.0f * static_cast<float>((row * 7) % 4), y,
                       3.0f, Ink{226, 224, 214});
            sheet.Line(area.x + 196.0f, y, area.x + 228.0f, y, 3.0f, kYellow);
        }
    }
    NameBoard(sheet, Sign::GiftShop, "GIFT SHOP", kRed);
    NameBoard(sheet, Sign::Drinks, "DRINKS", kBlue);
    NameBoard(sheet, Sign::IceCream, "ICE CREAM", Ink{186, 62, 110});
    NameBoard(sheet, Sign::Burgers, "BURGERS", kOrange);
    NameBoard(sheet, Sign::WoodlandAviary, "WOODLAND AVIARY", kGreen);
    NameBoard(sheet, Sign::SnakePavilion, "SNAKE PAVILION", kGreen);
    NameBoard(sheet, Sign::Education, "EDUCATION CENTRE", kGreen);
    NameBoard(sheet, Sign::Research, "RESEARCH CENTRE", kSlate);
    NameBoard(sheet, Sign::Vet, "VETERINARY CLINIC", Ink{30, 110, 110});
    {
        // Toilets: the two figures before the word.
        const Area& area = AreaOf(Sign::Toilets);
        sheet.Plate(area, kBlue, kCream);
        Figures(sheet, area.x + 56.0f, area.y + 104.0f, 80.0f, kCream);
        sheet.Centred("TOILETS", area.x + 0.5f * (area.w + 110.0f), area.MidY(), 0.46f * area.h,
                      area.w - 190.0f, kCream);
    }
    Plaque(sheet, Sign::Staff, "STAFF", "", kGreen, kCream);
    Plaque(sheet, Sign::StaffOnly, "STAFF ONLY", "NO ENTRY", kWhite, kRed);
    Plaque(sheet, Sign::Danger, "DANGER", "KEEP BACK", kYellow, kInk);
    Plaque(sheet, Sign::DoNotFeed, "PLEASE DO NOT", "FEED THE ANIMALS", kGreen, kCream);
    for (int variant = 0; variant < 4; ++variant) {
        BoardPanel(sheet, variant);
    }

    SignSheet out;
    out.size = kSheet;
    out.rgba = sheet.Take();
    return out;
}

float SignAspect(Sign sign) {
    const Area& area = AreaOf(sign);
    return area.w / area.h;
}

void WriteSign(ModelBuilder& model, Sign sign, const Vector3& a, const Vector3& b, const Vector3& c,
               const Vector3& d) {
    // Half a texel inside the sign's own area: its neighbours stay out of the filtering.
    const Area area = AreaOf(sign).Inset(0.5f);
    const float scale = 1.0f / static_cast<float>(kSheet);
    model.Use(zoo::Color3{1.0f, 1.0f, 1.0f}, Pattern::Signs)
        .Picture(a, b, c, d, area.x * scale, area.y * scale, (area.x + area.w) * scale,
                 (area.y + area.h) * scale);
}

}  // namespace zoogame
