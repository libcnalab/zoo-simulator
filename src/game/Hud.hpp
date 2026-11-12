// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Play.hpp"
#include "Saves.hpp"
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
    void Inspector(ui::Ui& ui, Play& play);
    void AnimalInspector(ui::Ui& ui, Play& play, ui::Box body);
    void EnclosureInspector(ui::Ui& ui, Play& play, ui::Box body);
    void BuildingInspector(ui::Ui& ui, Play& play, ui::Box body);
    void PropInspector(ui::Ui& ui, Play& play, ui::Box body);
    void Windows(ui::Ui& ui, Play& play);
    void ZooWindow(ui::Ui& ui, Play& play);
    void AnimalsWindow(ui::Ui& ui, Play& play);
    void FinancesWindow(ui::Ui& ui, Play& play);
    void MessagesWindow(ui::Ui& ui, Play& play);
    void HelpWindow(ui::Ui& ui, Play& play);
    void MenuWindow(ui::Ui& ui, Play& play, bool opened);
    void SaveWindow(ui::Ui& ui, Play& play, bool opened);
    void LoadWindow(ui::Ui& ui, Play& play, bool opened);
    void NewGameWindow(ui::Ui& ui, Play& play, bool opened);
    void TitleScreen(ui::Ui& ui, Play& play);

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
    /// The name being typed in the inspector, and whose it is.
    std::string nameEdit_;
    std::uint32_t nameEditFor_ = 0;

    // The windows.
    Dialog shown_ = Dialog::None;  ///< last frame's, to notice one opening
    std::string zooName_;
    int animalSort_ = 0;
    Dialog helpFrom_ = Dialog::None;   ///< where closing the help leads back to
    Request confirm_ = Request::None;  ///< what the menu asked "really?" about
    std::vector<SaveEntry> saves_;
    int savePicked_ = -1;
    std::string saveName_;
    std::string newName_;
    std::string newSeed_;
    int newLand_ = 0;   ///< 0 the starter zoo, 1 bare land
    int newFunds_ = 1;  ///< 0..2 the starting money, 3 a sandbox
};

/// Opens a palette of things to build (or closes them all with Palette::None)
/// and puts the matching tool in the player's hand.
void OpenPalette(Play& play, Palette palette);

/// What an animal is doing, in words: "Going to feed".
std::string ActivityText(const zoo::Animal& animal);
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
