// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Content/ContentManager.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Graphics/RasterizerState.hpp"
#include "Microsoft/Xna/Framework/Graphics/SpriteBatch.hpp"
#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"
#include "Microsoft/Xna/Framework/Vector2.hpp"

namespace zoogame::ui {

using Microsoft::Xna::Framework::Vector2;
using Rgba = Microsoft::Xna::Framework::Color;

/// A rectangle of the interface, in its own units (see Canvas).
struct Box {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;

    float Right() const { return x + w; }
    float Bottom() const { return y + h; }
    float MidX() const { return x + 0.5f * w; }
    float MidY() const { return y + 0.5f * h; }
    bool Contains(const Vector2& p) const { return p.X >= x && p.X < x + w && p.Y >= y && p.Y < y + h; }
    /// Shrunk by `d` on every side (grown when negative).
    Box Inset(float d) const { return {x + d, y + d, w - 2.0f * d, h - 2.0f * d}; }
    Box Inset(float dx, float dy) const { return {x + dx, y + dy, w - 2.0f * dx, h - 2.0f * dy}; }
    Box Moved(float dx, float dy) const { return {x + dx, y + dy, w, h}; }

    /// The layout cuts: each takes a slice off this box and returns it.
    Box CutTop(float height) {
        const Box slice{x, y, w, height};
        y += height;
        h -= height;
        return slice;
    }
    Box CutBottom(float height) {
        h -= height;
        return {x, y + h, w, height};
    }
    Box CutLeft(float width) {
        const Box slice{x, y, width, h};
        x += width;
        w -= width;
        return slice;
    }
    Box CutRight(float width) {
        w -= width;
        return {x + w, y, width, h};
    }
};

/// What is cut off the box intersection of two boxes (empty boxes have w or h <= 0).
Box Intersect(const Box& a, const Box& b);

/// One baked size of a typeface: an atlas texture and where its glyphs are.
struct FontStrike {
    struct Glyph {
        std::int16_t x = 0, y = 0, w = 0, h = 0;  ///< in the atlas
        std::int16_t left = 0, top = 0;           ///< from the pen (the line's top left) to the bitmap
        float advance = 0.0f;
    };
    int size = 0;  ///< pixels per em
    float ascent = 0.0f;
    float line = 0.0f;
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> texture;
    std::unordered_map<std::uint32_t, Glyph> glyphs;

    const Glyph* Find(std::uint32_t codepoint) const;
};

/// A typeface baked by tools/fonts/make_font_atlas.py at several sizes.
class FontFace {
public:
    /// Reads fonts/<name>.json and its atlases; throws when they are missing.
    FontFace(Microsoft::Xna::Framework::Content::ContentManager& content, const std::filesystem::path& assets,
             const std::string& name);
    /// The strike to draw text `pixels` high with: the smallest that is not
    /// magnified, or the largest there is.
    const FontStrike& Pick(float pixels) const;

private:
    std::vector<FontStrike> strikes_;  // ascending size
};

/// Draws the interface over the finished frame.
///
/// Coordinates are interface units: the screen is always 720 units high, and as
/// wide as its shape makes it (1280 for 16:9). The interface is described while
/// the game updates (every call records a command) and drawn by Flush() from
/// EngineGame::DrawOverlay. Shapes are snapped to device pixels, text is drawn
/// from the atlas nearest to its size on screen.
class Canvas {
public:
    Canvas(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device,
           Microsoft::Xna::Framework::Content::ContentManager& content, const std::filesystem::path& assets);
    ~Canvas();

    /// Forgets the recorded commands and measures the screen.
    void Begin();
    /// Draws what was recorded since Begin (callable more than once: a frame
    /// drawn again without an update shows the same interface).
    void Flush();

    Vector2 Size() const { return size_; }
    /// Device pixels per interface unit.
    float Scale() const { return scale_; }
    Vector2 ToUnits(const Vector2& pixels) const { return Vector2(pixels.X / scale_, pixels.Y / scale_); }

    void Fill(const Box& box, Rgba color);
    /// A rectangle with rounded corners.
    void Round(const Box& box, float radius, Rgba color);
    /// A rounded rectangle with a border `thickness` wide inside its edge.
    void Frame(const Box& box, float radius, Rgba fill, Rgba border, float thickness = 1.0f);
    /// The four sides of a box, inside its edge.
    void Outline(const Box& box, float thickness, Rgba color);
    void Disc(const Vector2& center, float radius, Rgba color);
    /// A soft shadow under a box, reaching `spread` beyond it.
    void Shadow(const Box& box, float spread, float alpha);
    void Image(const Microsoft::Xna::Framework::Graphics::Texture2D& texture, const Box& box, Rgba tint);

    /// One line of UTF-8 text with its top left at `at`; returns its width.
    float Text(std::string_view text, Vector2 at, float size, Rgba color, bool bold = false);
    float TextWidth(std::string_view text, float size, bool bold = false) const;
    /// The distance between the tops of two lines of text of `size`.
    float LineHeight(float size) const;
    /// Breaks text into lines no wider than `width` (at spaces; '\n' forces a break).
    std::vector<std::string> Wrap(std::string_view text, float width, float size, bool bold = false) const;

    /// Nothing is drawn outside `box` (intersected with the clip in force) until PopClip.
    void PushClip(const Box& box);
    void PopClip();
    /// The clip in force; the whole screen when there is none.
    Box Clip() const;

private:
    struct Command {
        enum class Kind : std::uint8_t { Quad, Clip };
        Kind kind = Kind::Quad;
        const Microsoft::Xna::Framework::Graphics::Texture2D* texture = nullptr;
        float sx = 0, sy = 0, sw = 0, sh = 0;  ///< source rectangle, texels
        Box box;                               ///< destination; for Clip: the area, w < 0 = no clip
        Rgba color;
    };

    void Quad(const Microsoft::Xna::Framework::Graphics::Texture2D& texture, float sx, float sy, float sw,
              float sh, const Box& box, Rgba color);
    float Snap(float v) const;
    Box Snapped(const Box& box) const;
    const FontFace& Face(bool bold) const { return bold ? *bold_ : *regular_; }
    void BeginBatch(bool clipped);

    Microsoft::Xna::Framework::Graphics::GraphicsDevice& device_;
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::SpriteBatch> batch_;
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> white_;
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> disc_;
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> glow_;
    Microsoft::Xna::Framework::Graphics::RasterizerState scissor_;
    std::unique_ptr<FontFace> regular_;
    std::unique_ptr<FontFace> bold_;
    std::vector<Command> commands_;
    std::vector<Box> clips_;
    Vector2 size_ = Vector2(1280.0f, 720.0f);
    float scale_ = 1.0f;
};

/// Decodes the UTF-8 character at `index` and advances past it; U+FFFD for bytes that are not UTF-8.
std::uint32_t NextCodepoint(std::string_view text, std::size_t& index);
/// Removes the last UTF-8 character of a string.
void PopCodepoint(std::string& text);
void AppendCodepoint(std::string& text, std::uint32_t codepoint);

/// A colour from 0..255 parts.
inline Rgba Rgb(int r, int g, int b, int a = 255) {
    return Rgba(r, g, b, a);
}
/// The same colour with its opacity multiplied by `alpha` (0..1).
Rgba Faded(Rgba color, float alpha);
Rgba Blend(Rgba a, Rgba b, float t);

}  // namespace zoogame::ui
