// SPDX-License-Identifier: MIT
#include "Ui.hpp"

#include <algorithm>
#include <cmath>

#include "Microsoft/Xna/Framework/Input/TextInputEXT.hpp"

namespace zoogame::ui {

namespace {

using Microsoft::Xna::Framework::Input::TextInputEXT;

/// What the keyboard typed since the interface last looked (UTF-16 code units,
/// as the platform delivers them). The subscription outlives any one Ui.
std::u16string& Typed() {
    static std::u16string typed;
    return typed;
}

void ListenToKeyboard() {
    static bool listening = false;
    if (listening)
        return;
    listening = true;
    TextInputEXT::TextInput += [](char16_t unit) { Typed().push_back(unit); };
}

}  // namespace

Ui::Ui(Canvas& canvas, Controls& controls) : canvas_(canvas), controls_(controls) {
    ListenToKeyboard();
}

Ui::~Ui() = default;

void Ui::Begin(float seconds) {
    canvas_.Begin();
    seconds_ = seconds;
    mouse_ = canvas_.ToUnits(controls_.Mouse());
    pressed_ = controls_.Pressed(MouseKey::Left);
    enabled_ = true;
    tooltip_.clear();
    scrolls_.clear();
    blink_ = std::fmod(blink_ + seconds, 1.0f);
    if (focus_.empty())
        Typed().clear();
    else
        controls_.ConsumeKeyboard();
}

void Ui::End() {
    if (!focus_.empty() && pressed_ && controls_.PointerFree())
        Focus({});  // a click on the world ends the typing
    if (tooltip_.empty())
        return;
    constexpr float kWidth = 260.0f;
    constexpr float kPad = 8.0f;
    const auto lines = canvas_.Wrap(tooltip_, kWidth, theme::kSmall);
    float widest = 0.0f;
    for (const auto& line : lines)
        widest = std::max(widest, canvas_.TextWidth(line, theme::kSmall));
    const float lineHeight = canvas_.LineHeight(theme::kSmall);
    Box box{mouse_.X + 16.0f, mouse_.Y + 18.0f, widest + 2.0f * kPad,
            lineHeight * static_cast<float>(lines.size()) + 2.0f * kPad};
    box.x = std::min(box.x, Screen().X - box.w - 4.0f);
    if (box.Bottom() > Screen().Y - 4.0f)
        box.y = mouse_.Y - box.h - 8.0f;
    canvas_.Shadow(box, 8.0f, 0.35f);
    canvas_.Frame(box, 4.0f, Rgb(12, 16, 20, 245), theme::kPanelEdge);
    float y = box.y + kPad;
    for (const auto& line : lines) {
        canvas_.Text(line, Vector2(box.x + kPad, y), theme::kSmall, theme::kText);
        y += lineHeight;
    }
}

bool Ui::Hovered(const Box& box) const {
    return enabled_ && box.Contains(mouse_) && canvas_.Clip().Contains(mouse_);
}

bool Ui::Clicked(const Box& box) const {
    return pressed_ && Hovered(box);
}

void Ui::Claim(const Box& box) {
    if (box.Contains(mouse_))
        controls_.ConsumePointer();
}

void Ui::Panel(const Box& box) {
    Claim(box);
    canvas_.Shadow(box, 14.0f, 0.4f);
    canvas_.Frame(box, theme::kRadius, theme::kPanel, theme::kPanelEdge);
}

Box Ui::Window(const Box& box, std::string_view title, bool* closed) {
    Panel(box);
    Box body = box.Inset(1.0f);
    const Box bar = body.CutTop(30.0f);
    canvas_.Round(bar, theme::kRadius - 1.0f, theme::kHeader);
    canvas_.Fill({bar.x, bar.y + bar.h - 6.0f, bar.w, 6.0f}, theme::kHeader);
    LabelIn(bar.Inset(12.0f, 0.0f), title, theme::kTitle - 2.0f, theme::kText, Align::Left, true);
    if (closed != nullptr) {
        const Box close{bar.Right() - 28.0f, bar.y + 3.0f, 24.0f, 24.0f};
        if (Button(close, "✗", {.tooltip = "Close"}))
            *closed = true;
    }
    return body.Inset(12.0f, 10.0f);
}

float Ui::Label(std::string_view text, float x, float y, float size, Rgba color, bool bold) {
    return canvas_.Text(text, Vector2(x, y), size, color, bold);
}

void Ui::LabelIn(const Box& box, std::string_view text, float size, Rgba color, Align align, bool bold) {
    std::string shown(text);
    float width = canvas_.TextWidth(shown, size, bold);
    if (width > box.w && box.w > 0.0f) {
        // Too long: characters come off the end until it fits with an ellipsis.
        while (!shown.empty() && canvas_.TextWidth(shown + "…", size, bold) > box.w)
            PopCodepoint(shown);
        shown += "…";
        width = canvas_.TextWidth(shown, size, bold);
    }
    float x = box.x;
    if (align == Align::Center)
        x = box.x + 0.5f * (box.w - width);
    else if (align == Align::Right)
        x = box.Right() - width;
    const float y = box.y + 0.5f * (box.h - canvas_.LineHeight(size));
    canvas_.Text(shown, Vector2(std::round(x), y), size, color, bold);
}

float Ui::Paragraph(const Box& box, std::string_view text, float size, Rgba color) {
    const float lineHeight = canvas_.LineHeight(size);
    float y = box.y;
    for (const auto& line : canvas_.Wrap(text, box.w, size)) {
        canvas_.Text(line, Vector2(box.x, y), size, color);
        y += lineHeight;
    }
    return y - box.y;
}

bool Ui::Button(const Box& box, std::string_view text, const ButtonStyle& style) {
    const bool hover = style.enabled && Hovered(box);
    const bool down = hover && controls_.Down(MouseKey::Left);
    Rgba fill = style.selected ? theme::kAccent : theme::kButton;
    if (hover)
        fill = down ? theme::kButtonDown : style.selected ? theme::kAccentHover : theme::kButtonHover;
    if (!style.enabled)
        fill = Faded(theme::kButton, 0.5f);
    canvas_.Round(box, 4.0f, fill);
    const Rgba ink = style.enabled ? style.text : theme::kFaint;
    const float pad = style.align == Align::Center ? 2.0f : 8.0f;
    LabelIn(box.Inset(pad, 0.0f), text, style.size, ink, style.align, style.bold);
    if (!style.tooltip.empty())
        Tooltip(box, style.tooltip);
    return hover && pressed_;
}

void Ui::Bar(const Box& box, float fraction, Rgba color) {
    canvas_.Round(box, 0.5f * box.h, Rgb(10, 14, 18, 255));
    const float f = std::clamp(fraction, 0.0f, 1.0f);
    if (f > 0.0f) {
        const Box inner = box.Inset(1.0f);
        canvas_.Round({inner.x, inner.y, std::max(inner.h, inner.w * f), inner.h}, 0.5f * inner.h, color);
    }
}

void Ui::Row(const Box& box, std::string_view label, std::string_view value, Rgba valueColor) {
    LabelIn(box, label, theme::kBody, theme::kMuted, Align::Left);
    LabelIn(box, value, theme::kBody, valueColor, Align::Right);
}

void Ui::Separator(const Box& box) {
    canvas_.Fill({box.x, std::round(box.MidY()), box.w, 1.0f}, Faded(theme::kPanelEdge, 0.7f));
}

void Ui::Focus(std::string_view id) {
    const bool had = !focus_.empty();
    focus_ = std::string(id);
    Typed().clear();
    if (!focus_.empty() && !had)
        TextInputEXT::StartTextInput();
    if (focus_.empty() && had)
        TextInputEXT::StopTextInput();
}

bool Ui::TextField(std::string_view id, const Box& box, std::string& text, std::size_t maxLength) {
    const bool focused = focus_ == id;
    if (Clicked(box) && !focused)
        Focus(id);
    else if (focused && pressed_ && !box.Contains(mouse_))
        Focus({});
    bool confirmed = false;
    if (focus_ == id) {
        for (const char16_t unit : Typed()) {
            // Control characters are keys, not text; surrogate halves are dropped.
            if (unit < 0x20 || unit == 0x7F || (unit >= 0xD800 && unit <= 0xDFFF))
                continue;
            if (text.size() < maxLength)
                AppendCodepoint(text, unit);
        }
        Typed().clear();
        if (controls_.Down(Keys::Back)) {
            // Once at once, then repeating after a pause.
            if (backspaceHeld_ == 0.0f || backspaceHeld_ > 0.45f) {
                PopCodepoint(text);
                if (backspaceHeld_ > 0.45f)
                    backspaceHeld_ = 0.41f;
            }
            backspaceHeld_ += std::max(seconds_, 1e-4f);
        } else {
            backspaceHeld_ = 0.0f;
        }
        if (controls_.Pressed(Keys::Enter)) {
            confirmed = true;
            Focus({});
        } else if (controls_.Pressed(Keys::Escape)) {
            Focus({});
        }
    }
    const bool now = focus_ == id;
    canvas_.Frame(box, 4.0f, Rgb(12, 16, 20, 255), now ? theme::kAccent : theme::kPanelEdge);
    const Box inner = box.Inset(8.0f, 0.0f);
    LabelIn(inner, text, theme::kBody, theme::kText);
    if (now && blink_ < 0.5f) {
        const float x = std::min(inner.x + canvas_.TextWidth(text, theme::kBody) + 1.0f, inner.Right());
        canvas_.Fill({x, box.y + 5.0f, 1.5f, box.h - 10.0f}, theme::kText);
    }
    return confirmed;
}

float Ui::ScrollBegin(std::string_view id, const Box& box, float contentHeight) {
    float& offset = scroll_[std::string(id)];
    const float most = std::max(0.0f, contentHeight - box.h);
    if (Hovered(box)) {
        offset -= controls_.Wheel() * 48.0f;
    }
    offset = std::clamp(offset, 0.0f, most);
    if (most > 0.0f) {
        // The thumb, in a track along the right edge; it can be dragged.
        const Box track{box.Right() - 5.0f, box.y, 5.0f, box.h};
        const float thumb = std::max(24.0f, box.h * box.h / contentHeight);
        const Box grab{track.x - 6.0f, track.y, 14.0f, track.h};
        if (enabled_ && grab.Contains(mouse_) && controls_.Down(MouseKey::Left)) {
            const float t = std::clamp((mouse_.Y - track.y - 0.5f * thumb) / (track.h - thumb), 0.0f, 1.0f);
            offset = t * most;
        }
        canvas_.Round(track, 2.5f, Rgb(10, 14, 18, 200));
        canvas_.Round({track.x, track.y + (track.h - thumb) * offset / most, track.w, thumb}, 2.5f,
                      theme::kFaint);
    }
    scrolls_.push_back({std::string(id), box, contentHeight});
    canvas_.PushClip({box.x, box.y, box.w - (most > 0.0f ? 8.0f : 0.0f), box.h});
    return std::round(offset);
}

void Ui::ScrollEnd() {
    if (scrolls_.empty())
        return;
    canvas_.PopClip();
    scrolls_.pop_back();
}

void Ui::Tooltip(const Box& box, std::string_view text) {
    if (Hovered(box))
        tooltip_ = std::string(text);
}

}  // namespace zoogame::ui
