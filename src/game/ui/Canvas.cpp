// SPDX-License-Identifier: MIT
#include "Canvas.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <stdexcept>

#include <nlohmann/json.hpp>

#include "Microsoft/Xna/Framework/Graphics/BlendState.hpp"
#include "Microsoft/Xna/Framework/Graphics/DepthStencilState.hpp"
#include "Microsoft/Xna/Framework/Graphics/SamplerState.hpp"
#include "Microsoft/Xna/Framework/Graphics/SpriteEffects.hpp"
#include "Microsoft/Xna/Framework/Graphics/SpriteSortMode.hpp"
#include "Microsoft/Xna/Framework/Graphics/Viewport.hpp"
#include "Microsoft/Xna/Framework/Matrix.hpp"
#include "Microsoft/Xna/Framework/Rectangle.hpp"

namespace zoogame::ui {

namespace {

namespace Xna = Microsoft::Xna::Framework::Graphics;
using Microsoft::Xna::Framework::Rectangle;

constexpr float kVirtualHeight = 720.0f;
constexpr int kDisc = 64;  ///< side of the disc and glow textures, texels
constexpr float kHalf = 0.5f * static_cast<float>(kDisc);

std::unique_ptr<Xna::Texture2D> MakeTexture(Xna::GraphicsDevice& device, int size, float (*alpha)(float d)) {
    std::vector<Rgba> pixels(static_cast<std::size_t>(size * size));
    const float center = 0.5f * static_cast<float>(size);
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            const float dx = static_cast<float>(x) + 0.5f - center;
            const float dy = static_cast<float>(y) + 0.5f - center;
            const float a = std::clamp(alpha(std::sqrt(dx * dx + dy * dy)), 0.0f, 1.0f);
            pixels[static_cast<std::size_t>(y * size + x)] =
                Rgba(255, 255, 255, static_cast<int>(std::lround(a * 255.0f)));
        }
    }
    auto texture = std::make_unique<Xna::Texture2D>(device, size, size);
    texture->SetData(pixels.data(), static_cast<int>(pixels.size()));
    return texture;
}

}  // namespace

Box Intersect(const Box& a, const Box& b) {
    const float x0 = std::max(a.x, b.x);
    const float y0 = std::max(a.y, b.y);
    const float x1 = std::min(a.Right(), b.Right());
    const float y1 = std::min(a.Bottom(), b.Bottom());
    return {x0, y0, std::max(0.0f, x1 - x0), std::max(0.0f, y1 - y0)};
}

Rgba Faded(Rgba color, float alpha) {
    return Rgba(static_cast<int>(color.getRProperty()), static_cast<int>(color.getGProperty()),
                static_cast<int>(color.getBProperty()),
                static_cast<int>(
                    std::lround(static_cast<float>(color.getAProperty()) * std::clamp(alpha, 0.0f, 1.0f))));
}

Rgba Blend(Rgba a, Rgba b, float t) {
    const auto mix = [t](std::uint8_t from, std::uint8_t to) {
        return static_cast<int>(
            std::lround(static_cast<float>(from) + (static_cast<float>(to) - static_cast<float>(from)) * t));
    };
    return Rgba(mix(a.getRProperty(), b.getRProperty()), mix(a.getGProperty(), b.getGProperty()),
                mix(a.getBProperty(), b.getBProperty()), mix(a.getAProperty(), b.getAProperty()));
}

std::uint32_t NextCodepoint(std::string_view text, std::size_t& index) {
    const auto byte = [&](std::size_t i) { return static_cast<std::uint8_t>(text[i]); };
    const std::uint8_t first = byte(index++);
    if (first < 0x80)
        return first;
    int more = 0;
    std::uint32_t value = 0;
    if ((first & 0xE0) == 0xC0) {
        more = 1;
        value = first & 0x1Fu;
    } else if ((first & 0xF0) == 0xE0) {
        more = 2;
        value = first & 0x0Fu;
    } else if ((first & 0xF8) == 0xF0) {
        more = 3;
        value = first & 0x07u;
    } else {
        return 0xFFFD;
    }
    for (int i = 0; i < more; ++i) {
        if (index >= text.size() || (byte(index) & 0xC0) != 0x80)
            return 0xFFFD;
        value = (value << 6) | (byte(index++) & 0x3Fu);
    }
    return value;
}

void PopCodepoint(std::string& text) {
    while (!text.empty()) {
        const auto last = static_cast<std::uint8_t>(text.back());
        text.pop_back();
        if ((last & 0xC0) != 0x80)
            break;
    }
}

void AppendCodepoint(std::string& text, std::uint32_t cp) {
    if (cp < 0x80) {
        text.push_back(static_cast<char>(cp));
    } else if (cp < 0x800) {
        text.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        text.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
        text.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        text.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        text.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        text.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        text.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        text.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        text.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

const FontStrike::Glyph* FontStrike::Find(std::uint32_t codepoint) const {
    const auto found = glyphs.find(codepoint);
    if (found != glyphs.end())
        return &found->second;
    const auto fallback = glyphs.find('?');
    return fallback != glyphs.end() ? &fallback->second : nullptr;
}

FontFace::FontFace(Microsoft::Xna::Framework::Content::ContentManager& content,
                   const std::filesystem::path& assets, const std::string& name) {
    std::ifstream file(assets / "fonts" / (name + ".json"));
    if (!file)
        throw std::runtime_error("zoo: the font fonts/" + name + ".json is missing");
    const nlohmann::json table = nlohmann::json::parse(file);
    for (const auto& entry : table.at("sizes")) {
        FontStrike strike;
        strike.size = entry.at("size").get<int>();
        strike.ascent = entry.at("ascent").get<float>();
        strike.line = entry.at("line").get<float>();
        strike.texture = std::make_unique<Xna::Texture2D>(
            content.Load<Xna::Texture2D>("fonts/" + entry.at("image").get<std::string>()));
        for (const auto& [key, g] : entry.at("glyphs").items()) {
            FontStrike::Glyph glyph;
            glyph.x = g.at(0).get<std::int16_t>();
            glyph.y = g.at(1).get<std::int16_t>();
            glyph.w = g.at(2).get<std::int16_t>();
            glyph.h = g.at(3).get<std::int16_t>();
            glyph.left = g.at(4).get<std::int16_t>();
            glyph.top = g.at(5).get<std::int16_t>();
            glyph.advance = g.at(6).get<float>();
            strike.glyphs.emplace(static_cast<std::uint32_t>(std::stoul(key)), glyph);
        }
        strikes_.push_back(std::move(strike));
    }
    if (strikes_.empty())
        throw std::runtime_error("zoo: the font " + name + " has no sizes");
    std::sort(strikes_.begin(), strikes_.end(),
              [](const FontStrike& a, const FontStrike& b) { return a.size < b.size; });
}

const FontStrike& FontFace::Pick(float pixels) const {
    for (const FontStrike& strike : strikes_) {
        // A strike a twentieth too small is still sharper than the next one shrunk.
        if (static_cast<float>(strike.size) >= pixels * 0.95f)
            return strike;
    }
    return strikes_.back();
}

Canvas::Canvas(Xna::GraphicsDevice& device, Microsoft::Xna::Framework::Content::ContentManager& content,
               const std::filesystem::path& assets)
    : device_(device),
      batch_(std::make_unique<Xna::SpriteBatch>(device)),
      white_(std::make_unique<Xna::Texture2D>(device, 1, 1)) {
    const Rgba white(255, 255, 255, 255);
    white_->SetData(&white, 1);
    // A disc with a soft edge one texel wide, and a glow that fades from its centre.
    disc_ = MakeTexture(device, kDisc, [](float d) { return kHalf - 0.5f - d + 0.5f; });
    glow_ = MakeTexture(device, kDisc, [](float d) {
        const float t = std::clamp(1.0f - d / kHalf, 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    });
    scissor_.setCullModeProperty(Xna::CullMode::None);
    scissor_.setScissorTestEnableProperty(true);
    regular_ = std::make_unique<FontFace>(content, assets, "sans");
    bold_ = std::make_unique<FontFace>(content, assets, "sansb");
}

Canvas::~Canvas() = default;

void Canvas::Begin() {
    commands_.clear();
    clips_.clear();
    const auto& viewport = device_.getViewportProperty();
    const float height = static_cast<float>(std::max(1, viewport.getHeightProperty()));
    const float width = static_cast<float>(std::max(1, viewport.getWidthProperty()));
    scale_ = height / kVirtualHeight;
    size_ = Vector2(width / scale_, kVirtualHeight);
}

float Canvas::Snap(float v) const {
    return std::round(v * scale_) / scale_;
}

Box Canvas::Snapped(const Box& box) const {
    const float x0 = Snap(box.x);
    const float y0 = Snap(box.y);
    return {x0, y0, Snap(box.Right()) - x0, Snap(box.Bottom()) - y0};
}

void Canvas::Quad(const Xna::Texture2D& texture, float sx, float sy, float sw, float sh, const Box& box,
                  Rgba color) {
    if (box.w <= 0.0f || box.h <= 0.0f || color.getAProperty() == 0)
        return;
    Command command;
    command.texture = &texture;
    command.sx = sx;
    command.sy = sy;
    command.sw = sw;
    command.sh = sh;
    command.box = box;
    command.color = color;
    commands_.push_back(command);
}

void Canvas::Fill(const Box& box, Rgba color) {
    Quad(*white_, 0, 0, 1, 1, Snapped(box), color);
}

void Canvas::Round(const Box& raw, float radius, Rgba color) {
    const Box box = Snapped(raw);
    const float r = Snap(std::min({radius, 0.5f * box.w, 0.5f * box.h}));
    if (r <= 0.0f) {
        Fill(box, color);
        return;
    }
    const float right = box.Right() - r;
    const float bottom = box.Bottom() - r;
    Quad(*disc_, 0, 0, kHalf, kHalf, {box.x, box.y, r, r}, color);
    Quad(*disc_, kHalf, 0, kHalf, kHalf, {right, box.y, r, r}, color);
    Quad(*disc_, 0, kHalf, kHalf, kHalf, {box.x, bottom, r, r}, color);
    Quad(*disc_, kHalf, kHalf, kHalf, kHalf, {right, bottom, r, r}, color);
    Quad(*white_, 0, 0, 1, 1, {box.x + r, box.y, box.w - 2.0f * r, r}, color);
    Quad(*white_, 0, 0, 1, 1, {box.x, box.y + r, box.w, box.h - 2.0f * r}, color);
    Quad(*white_, 0, 0, 1, 1, {box.x + r, bottom, box.w - 2.0f * r, r}, color);
}

void Canvas::Frame(const Box& box, float radius, Rgba fill, Rgba border, float thickness) {
    Round(box, radius, border);
    Round(box.Inset(thickness), std::max(0.0f, radius - thickness), fill);
}

void Canvas::Outline(const Box& raw, float thickness, Rgba color) {
    const Box box = Snapped(raw);
    const float t = std::max(Snap(thickness), 1.0f / scale_);
    Quad(*white_, 0, 0, 1, 1, {box.x, box.y, box.w, t}, color);
    Quad(*white_, 0, 0, 1, 1, {box.x, box.Bottom() - t, box.w, t}, color);
    Quad(*white_, 0, 0, 1, 1, {box.x, box.y + t, t, box.h - 2.0f * t}, color);
    Quad(*white_, 0, 0, 1, 1, {box.Right() - t, box.y + t, t, box.h - 2.0f * t}, color);
}

void Canvas::Disc(const Vector2& center, float radius, Rgba color) {
    Quad(*disc_, 0, 0, static_cast<float>(kDisc), static_cast<float>(kDisc),
         {center.X - radius, center.Y - radius, 2.0f * radius, 2.0f * radius}, color);
}

void Canvas::Shadow(const Box& raw, float spread, float alpha) {
    const Box box = Snapped(raw);
    const float s = Snap(spread);
    if (s <= 0.0f)
        return;
    const Rgba color = Rgba(0, 0, 0, static_cast<int>(std::lround(std::clamp(alpha, 0.0f, 1.0f) * 255.0f)));
    // The glow's quadrants at the corners (their centres a spread inside the
    // box), its middle row and column stretched along the sides.
    const float x0 = box.x - s;
    const float y0 = box.y - s;
    const float x1 = box.x + s;
    const float y1 = box.y + s;
    const float x2 = box.Right() - s;
    const float y2 = box.Bottom() - s;
    const float d = 2.0f * s;
    if (x2 <= x1 || y2 <= y1)
        return;
    Quad(*glow_, 0, 0, kHalf, kHalf, {x0, y0, d, d}, color);
    Quad(*glow_, kHalf, 0, kHalf, kHalf, {x2, y0, d, d}, color);
    Quad(*glow_, 0, kHalf, kHalf, kHalf, {x0, y2, d, d}, color);
    Quad(*glow_, kHalf, kHalf, kHalf, kHalf, {x2, y2, d, d}, color);
    Quad(*glow_, kHalf - 0.5f, 0, 1, kHalf, {x1, y0, x2 - x1, d}, color);
    Quad(*glow_, kHalf - 0.5f, kHalf, 1, kHalf, {x1, y2, x2 - x1, d}, color);
    Quad(*glow_, 0, kHalf - 0.5f, kHalf, 1, {x0, y1, d, y2 - y1}, color);
    Quad(*glow_, kHalf, kHalf - 0.5f, kHalf, 1, {x2, y1, d, y2 - y1}, color);
    Quad(*white_, 0, 0, 1, 1, {x1, y1, x2 - x1, y2 - y1}, color);
}

void Canvas::Image(const Xna::Texture2D& texture, const Box& box, Rgba tint) {
    Quad(texture, 0, 0, static_cast<float>(texture.getWidthProperty()),
         static_cast<float>(texture.getHeightProperty()), box, tint);
}

float Canvas::LineHeight(float size) const {
    const FontStrike& strike = regular_->Pick(size * scale_);
    return strike.line * size / static_cast<float>(strike.size);
}

float Canvas::TextWidth(std::string_view text, float size, bool bold) const {
    const FontStrike& strike = Face(bold).Pick(size * scale_);
    const float k = size / static_cast<float>(strike.size);
    float width = 0.0f;
    for (std::size_t i = 0; i < text.size();) {
        if (const auto* glyph = strike.Find(NextCodepoint(text, i)))
            width += glyph->advance * k;
    }
    return width;
}

float Canvas::Text(std::string_view text, Vector2 at, float size, Rgba color, bool bold) {
    const FontStrike& strike = Face(bold).Pick(size * scale_);
    const float k = size / static_cast<float>(strike.size);
    float pen = at.X;
    const float top = Snap(at.Y);
    for (std::size_t i = 0; i < text.size();) {
        const auto* glyph = strike.Find(NextCodepoint(text, i));
        if (glyph == nullptr)
            continue;
        if (glyph->w > 0 && glyph->h > 0) {
            // A texel of margin on every side keeps the glyph's soft edge when it is scaled.
            const Box box{Snap(pen + (static_cast<float>(glyph->left) - 1.0f) * k),
                          top + (static_cast<float>(glyph->top) - 1.0f) * k,
                          (static_cast<float>(glyph->w) + 2.0f) * k,
                          (static_cast<float>(glyph->h) + 2.0f) * k};
            Quad(*strike.texture, static_cast<float>(glyph->x) - 1.0f, static_cast<float>(glyph->y) - 1.0f,
                 static_cast<float>(glyph->w) + 2.0f, static_cast<float>(glyph->h) + 2.0f, box, color);
        }
        pen += glyph->advance * k;
    }
    return pen - at.X;
}

std::vector<std::string> Canvas::Wrap(std::string_view text, float width, float size, bool bold) const {
    std::vector<std::string> lines;
    std::string line;
    std::size_t i = 0;
    const auto flush = [&] {
        lines.push_back(line);
        line.clear();
    };
    while (i <= text.size()) {
        // The next word: up to a space, a line break or the end.
        std::size_t end = i;
        while (end < text.size() && text[end] != ' ' && text[end] != '\n')
            ++end;
        const std::string_view word = text.substr(i, end - i);
        if (!word.empty()) {
            std::string candidate = line.empty() ? std::string(word) : line + " " + std::string(word);
            if (!line.empty() && TextWidth(candidate, size, bold) > width) {
                flush();
                candidate = std::string(word);
            }
            line = std::move(candidate);
        }
        if (end >= text.size())
            break;
        if (text[end] == '\n')
            flush();
        i = end + 1;
    }
    if (!line.empty() || lines.empty())
        lines.push_back(line);
    return lines;
}

Box Canvas::Clip() const {
    return clips_.empty() ? Box{0.0f, 0.0f, size_.X, size_.Y} : clips_.back();
}

void Canvas::PushClip(const Box& box) {
    const Box clip = Intersect(Snapped(box), Clip());
    clips_.push_back(clip);
    Command command;
    command.kind = Command::Kind::Clip;
    command.box = clip;
    commands_.push_back(command);
}

void Canvas::PopClip() {
    if (clips_.empty())
        return;
    clips_.pop_back();
    Command command;
    command.kind = Command::Kind::Clip;
    command.box = clips_.empty() ? Box{0.0f, 0.0f, -1.0f, -1.0f} : clips_.back();
    commands_.push_back(command);
}

void Canvas::BeginBatch(bool clipped) {
    batch_->Begin(Xna::SpriteSortMode::Deferred, &Xna::BlendState::NonPremultiplied,
                  &Xna::SamplerState::LinearClamp, &Xna::DepthStencilState::None,
                  clipped ? &scissor_ : &Xna::RasterizerState::CullNone, nullptr,
                  Microsoft::Xna::Framework::Matrix::CreateScale(scale_));
}

void Canvas::Flush() {
    BeginBatch(false);
    for (const Command& command : commands_) {
        if (command.kind == Command::Kind::Clip) {
            batch_->End();
            const bool clipped = command.box.w >= 0.0f;
            if (clipped) {
                device_.setScissorRectangleProperty(
                    Rectangle(static_cast<int>(std::lround(command.box.x * scale_)),
                              static_cast<int>(std::lround(command.box.y * scale_)),
                              static_cast<int>(std::lround(command.box.w * scale_)),
                              static_cast<int>(std::lround(command.box.h * scale_))));
            }
            BeginBatch(clipped);
            continue;
        }
        // Whole texels as the source; the fraction a strip asks for goes into the scale.
        const Rectangle source(static_cast<int>(std::floor(command.sx)),
                               static_cast<int>(std::floor(command.sy)),
                               std::max(1, static_cast<int>(std::lround(command.sw))),
                               std::max(1, static_cast<int>(std::lround(command.sh))));
        batch_->Draw(*command.texture, Vector2(command.box.x, command.box.y), source, command.color, 0.0f,
                     Vector2(0.0f, 0.0f),
                     Vector2(command.box.w / static_cast<float>(source.Width),
                             command.box.h / static_cast<float>(source.Height)),
                     Xna::SpriteEffects::None, 0.0f);
    }
    batch_->End();
}

}  // namespace zoogame::ui
