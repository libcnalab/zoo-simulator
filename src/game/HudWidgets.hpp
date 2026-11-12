// SPDX-License-Identifier: MIT
#pragma once

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <string>
#include <string_view>

#include "Hud.hpp"

/// The small composite widgets the palettes, the inspector and the windows share.
namespace zoogame::hud {

inline constexpr float kRow = 28.0f;  ///< a row of a list, with its gap
inline constexpr float kGap = 3.0f;

/// A row of a list: a name on the left, a figure on the right.
inline bool ItemRow(ui::Ui& ui, const ui::Box& box, std::string_view name, std::string_view figure,
                    bool selected, std::string_view tooltip = {}) {
    namespace theme = ui::theme;
    const bool clicked = ui.Button(box, "", {.selected = selected, .tooltip = tooltip});
    ui::Box inner = box.Inset(9.0f, 0.0f);
    if (!figure.empty()) {
        const float width = ui.Paint().TextWidth(figure, theme::kSmall);
        ui.LabelIn(inner.CutRight(width + 2.0f), figure, theme::kSmall,
                   selected ? theme::kText : theme::kMoney, ui::Align::Right);
        inner.CutRight(6.0f);
    }
    ui.LabelIn(inner, name, theme::kBody, theme::kText);
    return clicked;
}

/// A row of equal buttons, one of which is chosen; returns the one clicked or -1.
inline int Choice(ui::Ui& ui, ui::Box row, std::initializer_list<std::string_view> labels, int chosen) {
    namespace theme = ui::theme;
    const float count = static_cast<float>(labels.size());
    const float width = (row.w - kGap * (count - 1.0f)) / count;
    int clicked = -1;
    int index = 0;
    for (const std::string_view label : labels) {
        const ui::Box box{std::round(row.x + (width + kGap) * static_cast<float>(index)), row.y,
                          std::round(width), row.h};
        if (ui.Button(box, label, {.selected = index == chosen, .size = theme::kSmall + 1.0f}))
            clicked = index;
        ++index;
    }
    return clicked;
}

inline void Heading(ui::Ui& ui, ui::Box& body, std::string_view text) {
    namespace theme = ui::theme;
    ui.LabelIn(body.CutTop(20.0f), text, theme::kSmall, theme::kMuted, ui::Align::Left, true);
    body.CutTop(2.0f);
}

inline void Note(ui::Ui& ui, ui::Box& body, std::string_view text, ui::Rgba color = ui::theme::kMuted) {
    namespace theme = ui::theme;
    body.CutTop(4.0f);
    const float used = ui.Paragraph(body, text, theme::kSmall, color);
    body.CutTop(used + 2.0f);
}

/// "label ...... value" in small print.
inline void Fact(ui::Ui& ui, ui::Box& body, std::string_view label, std::string_view value,
                 ui::Rgba color = ui::theme::kText) {
    namespace theme = ui::theme;
    const ui::Box row = body.CutTop(19.0f);
    ui.LabelIn(row, label, theme::kSmall, theme::kMuted);
    ui::Box right = row;
    right.CutLeft(std::min(row.w * 0.42f, ui.Paint().TextWidth(label, theme::kSmall) + 10.0f));
    ui.LabelIn(right, value, theme::kSmall, color, ui::Align::Right);
}

/// "label [=====   ] 62 %": how full, how good.
inline void Meter(ui::Ui& ui, ui::Box& body, std::string_view label, float fraction, ui::Rgba color,
                  std::string_view tooltip = {}) {
    namespace theme = ui::theme;
    const ui::Box row = body.CutTop(19.0f);
    ui::Box rest = row;
    ui.LabelIn(rest.CutLeft(std::round(row.w * 0.36f)), label, theme::kSmall, theme::kMuted);
    ui.LabelIn(rest.CutRight(40.0f), Percent(fraction), theme::kSmall, theme::kText, ui::Align::Right);
    rest.CutRight(4.0f);
    ui.Bar({rest.x, std::round(rest.MidY() - 3.5f), rest.w, 7.0f}, fraction, color);
    if (!tooltip.empty())
        ui.Tooltip(row, tooltip);
}

/// A narrow meter for two to a row: "label [bar]", the figure in the tooltip.
inline void SmallMeter(ui::Ui& ui, ui::Box row, std::string_view label, float fraction, ui::Rgba color) {
    namespace theme = ui::theme;
    ui::Box rest = row;
    ui.LabelIn(rest.CutLeft(std::round(row.w * 0.54f)), label, theme::kSmall, theme::kMuted);
    ui.Bar({rest.x, std::round(rest.MidY() - 3.5f), rest.w, 7.0f}, fraction, color);
    ui.Tooltip(row,
               Format("%.*s: %s", static_cast<int>(label.size()), label.data(), Percent(fraction).c_str()));
}

}  // namespace zoogame::hud
