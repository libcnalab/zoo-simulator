// SPDX-License-Identifier: MIT
#pragma once

#include <vector>

#include "Saves.hpp"
#include "Visit.hpp"
#include "ui/Ui.hpp"

namespace zoogame {

/// What a visitor of the zoo sees over the world: the title, the pause menu and,
/// on a walk, next to nothing (a dot to aim with, the time, what E would do).
/// None of the management game's panels: those belong to the editor (--editor).
class VisitorHud {
public:
    void Build(ui::Ui& ui, Visit& visit);

private:
    void Title(ui::Ui& ui, Visit& visit);
    void Pause(ui::Ui& ui, Visit& visit);
    void Walking(ui::Ui& ui, Visit& visit);
    void Settings(ui::Ui& ui, Visit& visit, const ui::Box& box);
    void Load(ui::Ui& ui, Visit& visit, const ui::Box& box);
    void Card(ui::Ui& ui, const Visit& visit);
    void Board(ui::Ui& ui, const Visit& visit);
    void Map(ui::Ui& ui, Visit& visit);
    void Diagnostics(ui::Ui& ui, const Visit& visit);
    void Notice(ui::Ui& ui, Visit& visit);

    VisitWindow listedFor_ = VisitWindow::None;
    std::vector<SaveEntry> saves_;
};

}  // namespace zoogame
