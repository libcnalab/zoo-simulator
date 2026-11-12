// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Play.hpp"
#include "ui/Ui.hpp"

namespace zoogame {

/// The interface of a game in progress: the bar along the top, the toolbar
/// along the bottom, the palette of things to build, the inspector of what is
/// selected, the windows, the messages.
class Hud {
public:
    /// Describes this frame's interface and applies what the player did in it.
    void Build(ui::Ui& ui, Play& play);

private:
    void TopBar(ui::Ui& ui, Play& play);
    void Toasts(ui::Ui& ui, Play& play);
    void Toolbar(ui::Ui& ui, Play& play);
    void HintLine(ui::Ui& ui, Play& play);
    void PalettePanel(ui::Ui& ui, Play& play);
    void PathPalette(ui::Ui& ui, Play& play, ui::Box body);
    void FencePalette(ui::Ui& ui, Play& play, ui::Box body);
    void GroundPalette(ui::Ui& ui, Play& play, ui::Box body);
    void WaterPalette(ui::Ui& ui, Play& play, ui::Box body);
    void BuildingPalette(ui::Ui& ui, Play& play, ui::Box body);
    void SceneryPalette(ui::Ui& ui, Play& play, ui::Box body);
    void AnimalPalette(ui::Ui& ui, Play& play, ui::Box body);

    struct Toast {
        std::uint32_t id = 0;
        float age = 0.0f;  ///< seconds on screen
    };
    std::vector<Toast> toasts_;
    /// Messages up to this id were shown (or predate this session).
    std::uint32_t seen_ = 0;
    bool primed_ = false;
    /// The tab of the scenery palette: plants, rocks, habitat, visitors.
    int sceneryTab_ = 0;
};

/// Opens a palette of things to build (or closes them all with Palette::None)
/// and puts the matching tool in the player's hand.
void OpenPalette(Play& play, Palette palette);

/// "08:15"
std::string ClockText(zoo::GameTime time);
/// "2 March, year 1"
std::string DateText(zoo::GameTime time);
std::string_view SeasonName(zoo::Season season);
std::string_view WeatherName(zoo::WeatherKind kind);
ui::Rgba SeverityColor(zoo::Severity severity);
/// Green for a good welfare, amber for a middling one, red for a poor one.
ui::Rgba WelfareColor(float welfare);
/// "72 %"
std::string Percent(float fraction);

namespace hud {
inline constexpr float kBarHeight = 38.0f;      ///< the bar along the top
inline constexpr float kToolbarHeight = 44.0f;  ///< the toolbar along the bottom
inline constexpr float kMargin = 10.0f;
}  // namespace hud

}  // namespace zoogame
