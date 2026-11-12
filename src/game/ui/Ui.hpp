// SPDX-License-Identifier: MIT
#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "../Controls.hpp"
#include "Canvas.hpp"

namespace zoogame::ui {

/// The interface's colours and sizes.
namespace theme {
inline const Rgba kPanel = Rgb(22, 28, 34, 236);
inline const Rgba kPanelEdge = Rgb(70, 84, 96, 255);
inline const Rgba kHeader = Rgb(34, 44, 54, 255);
inline const Rgba kButton = Rgb(48, 60, 72, 255);
inline const Rgba kButtonHover = Rgb(66, 82, 98, 255);
inline const Rgba kButtonDown = Rgb(38, 48, 58, 255);
inline const Rgba kAccent = Rgb(74, 160, 92, 255);
inline const Rgba kAccentHover = Rgb(92, 184, 110, 255);
inline const Rgba kText = Rgb(236, 240, 242, 255);
inline const Rgba kMuted = Rgb(160, 172, 182, 255);
inline const Rgba kFaint = Rgb(108, 120, 130, 255);
inline const Rgba kGood = Rgb(110, 200, 120, 255);
inline const Rgba kWarning = Rgb(236, 184, 78, 255);
inline const Rgba kAlarm = Rgb(232, 96, 86, 255);
inline const Rgba kInfo = Rgb(120, 176, 232, 255);
inline const Rgba kMoney = Rgb(240, 214, 120, 255);
inline constexpr float kRadius = 6.0f;
inline constexpr float kSmall = 12.0f;
inline constexpr float kBody = 14.0f;
inline constexpr float kTitle = 17.0f;
}  // namespace theme

enum class Align { Left, Center, Right };

struct ButtonStyle {
    bool selected = false;  ///< drawn in the accent colour
    bool enabled = true;
    bool bold = false;
    float size = theme::kBody;
    Align align = Align::Center;
    std::string_view tooltip;
    Rgba text = theme::kText;
};

/// An immediate-mode interface: every frame the game calls a function per
/// widget, which draws it (recording into the Canvas) and answers what the
/// player did with it. Built before the world reads the input, so a click on a
/// panel never reaches the ground below.
class Ui {
public:
    Ui(Canvas& canvas, Controls& controls);
    ~Ui();

    void Begin(float seconds);
    /// Draws what floats above everything (the tooltip).
    void End();

    Canvas& Paint() { return canvas_; }
    Vector2 Screen() const { return canvas_.Size(); }
    Vector2 Mouse() const { return mouse_; }
    float Seconds() const { return seconds_; }
    /// True when the pointer is over `box`, inside the clip, and the interface listens.
    bool Hovered(const Box& box) const;
    bool Clicked(const Box& box) const;
    /// Widgets that follow do not react (those behind a dialog).
    void SetEnabled(bool enabled) { enabled_ = enabled; }
    bool Enabled() const { return enabled_; }

    /// A panel: shadow, body, border. The pointer over it belongs to the interface.
    void Panel(const Box& box);
    /// A panel with a title bar; returns the area below the bar, padded.
    /// `closed` is set when its close button is clicked (no button when null).
    Box Window(const Box& box, std::string_view title, bool* closed = nullptr);
    /// Marks an area as the interface's without drawing it.
    void Claim(const Box& box);

    float Label(std::string_view text, float x, float y, float size = theme::kBody, Rgba color = theme::kText,
                bool bold = false);
    /// Text inside a box: aligned along x, centred along y, cut off with an ellipsis when too long.
    void LabelIn(const Box& box, std::string_view text, float size = theme::kBody, Rgba color = theme::kText,
                 Align align = Align::Left, bool bold = false);
    /// Wrapped text from the top of `box`; returns the height used.
    float Paragraph(const Box& box, std::string_view text, float size = theme::kBody,
                    Rgba color = theme::kText);

    bool Button(const Box& box, std::string_view text, const ButtonStyle& style = {});
    /// A bar filled to `fraction` (0..1).
    void Bar(const Box& box, float fraction, Rgba color);
    /// "label ........ value" on one row.
    void Row(const Box& box, std::string_view label, std::string_view value, Rgba valueColor = theme::kText);
    void Separator(const Box& box);
    /// A box to type into; true when Enter confirmed the text.
    bool TextField(std::string_view id, const Box& box, std::string& text, std::size_t maxLength = 32);
    bool Typing() const { return !focus_.empty(); }
    void Focus(std::string_view id);

    /// A scrolled area: draw the content between Begin and End at `y - offset`.
    /// Returns the offset; `contentHeight` is last frame's (or a known) height.
    float ScrollBegin(std::string_view id, const Box& box, float contentHeight);
    void ScrollEnd();

    /// Shows `text` beside the pointer while it rests on `box`.
    void Tooltip(const Box& box, std::string_view text);

private:
    Canvas& canvas_;
    Controls& controls_;
    Vector2 mouse_ = Vector2(0.0f, 0.0f);
    float seconds_ = 0.0f;
    bool enabled_ = true;
    bool pressed_ = false;
    std::string tooltip_;
    std::string focus_;
    float blink_ = 0.0f;
    float backspaceHeld_ = 0.0f;
    std::unordered_map<std::string, float> scroll_;
    struct Scroll {
        std::string id;
        Box box;
        float content = 0.0f;
    };
    std::vector<Scroll> scrolls_;
};

}  // namespace zoogame::ui
