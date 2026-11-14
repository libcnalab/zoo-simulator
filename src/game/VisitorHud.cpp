// SPDX-License-Identifier: MIT
#include "VisitorHud.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <initializer_list>

#include "HudWidgets.hpp"
#include "zoo/base/Version.hpp"

namespace zoogame {

namespace {

namespace theme = ui::theme;
using ui::Align;
using ui::Box;

/// Text over the world itself: a dark copy behind it keeps it legible on a bright sky.
void Shadowed(ui::Ui& ui, std::string_view text, float x, float y, float size, ui::Rgba color,
              bool bold = false) {
    ui.Paint().Text(text, Vector2(x + 1.0f, y + 1.0f), size, ui::Rgb(0, 0, 0, 130), bold);
    ui.Paint().Text(text, Vector2(x, y), size, color, bold);
}

struct KeyHint {
    std::string_view key;
    std::string_view does;
};

/// "[W A S D] Walk   [Mouse] Look ...", centred on `midX`.
void KeyHints(ui::Ui& ui, float midX, float y, std::initializer_list<KeyHint> hints, float alpha) {
    constexpr float kSize = theme::kSmall + 1.0f;
    constexpr float kPad = 7.0f;
    constexpr float kGap = 18.0f;
    float width = -kGap;
    for (const KeyHint& hint : hints)
        width += ui.Paint().TextWidth(hint.key, kSize, true) + 2.0f * kPad + 7.0f +
                 ui.Paint().TextWidth(hint.does, kSize) + kGap;
    float x = std::round(midX - 0.5f * width);
    const Box plate{x - 14.0f, y - 7.0f, width + 28.0f, 34.0f};
    ui.Paint().Round(plate, 8.0f, ui::Rgb(10, 14, 18, static_cast<int>(150.0f * alpha)));
    for (const KeyHint& hint : hints) {
        const float keyWidth = ui.Paint().TextWidth(hint.key, kSize, true) + 2.0f * kPad;
        const Box cap{x, y, keyWidth, 20.0f};
        ui.Paint().Frame(cap, 4.0f, ui::Faded(theme::kButton, alpha), ui::Faded(theme::kPanelEdge, alpha));
        ui.LabelIn(cap, hint.key, kSize, ui::Faded(theme::kText, alpha), Align::Center, true);
        x += keyWidth + 7.0f;
        ui.LabelIn({x, y, 200.0f, 20.0f}, hint.does, kSize, ui::Faded(theme::kText, 0.9f * alpha));
        x += ui.Paint().TextWidth(hint.does, kSize) + kGap;
    }
}

/// "label        [-]  value  [+]": returns -1, 0 or +1.
int Stepper(ui::Ui& ui, Box row, std::string_view label, std::string_view value, bool canLess, bool canMore) {
    ui.LabelIn(row.CutLeft(std::round(row.w * 0.46f)), label, theme::kBody, theme::kMuted);
    int step = 0;
    if (ui.Button(row.CutLeft(30.0f), "-", {.enabled = canLess, .bold = true}))
        step = -1;
    if (ui.Button(row.CutRight(30.0f), "+", {.enabled = canMore, .bold = true}))
        step = 1;
    ui.LabelIn(row, value, theme::kBody, theme::kText, Align::Center);
    return step;
}

/// "label        [Off][On]": true when the switch was thrown.
bool Switch(ui::Ui& ui, Box row, std::string_view label, bool& on) {
    ui.LabelIn(row.CutLeft(std::round(row.w * 0.46f)), label, theme::kBody, theme::kMuted);
    const int picked = hud::Choice(ui, row, {"Off", "On"}, on ? 1 : 0);
    if (picked < 0 || (picked == 1) == on)
        return false;
    on = picked == 1;
    return true;
}

std::string Written(const SaveEntry& entry) {
    const auto system = std::chrono::clock_cast<std::chrono::system_clock>(entry.written);
    const std::time_t seconds = std::chrono::system_clock::to_time_t(system);
    std::tm local{};
    localtime_r(&seconds, &local);
    char text[40];
    std::strftime(text, sizeof text, "%e %b %Y, %H:%M", &local);
    return text;
}

}  // namespace

void VisitorHud::Build(ui::Ui& ui, Visit& visit) {
    if (visit.window != listedFor_) {
        listedFor_ = visit.window;
        if (visit.window == VisitWindow::Load)
            saves_ = ListSaves();
    }
    visit.flashLeft = std::max(0.0f, visit.flashLeft - ui.Seconds());
    switch (visit.screen) {
        case VisitScreen::Title:
            Title(ui, visit);
            break;
        case VisitScreen::Walking:
            Walking(ui, visit);
            break;
        case VisitScreen::Paused:
            Pause(ui, visit);
            break;
    }
    if (visit.settings.diagnostics)
        Diagnostics(ui, visit);
    Notice(ui, visit);
}

void VisitorHud::Title(ui::Ui& ui, Visit& visit) {
    const Box screen{0.0f, 0.0f, ui.Screen().X, ui.Screen().Y};
    ui.Claim(screen);
    // A band down the left: the zoo goes on behind it.
    const float width = 380.0f;
    const Box band{std::round(0.09f * screen.w), 0.0f, width, screen.h};
    ui.Paint().Fill(band, ui::Rgb(14, 20, 24, 214));
    ui.Paint().Fill({band.x, 0.0f, 1.0f, band.h}, ui::Faded(theme::kPanelEdge, 0.6f));
    ui.Paint().Fill({band.Right() - 1.0f, 0.0f, 1.0f, band.h}, ui::Faded(theme::kPanelEdge, 0.6f));

    Box body = band.Inset(40.0f, 0.0f);
    body.CutTop(std::round(0.16f * screen.h));
    ui.LabelIn(body.CutTop(44.0f), "Zoo Simulator", 36.0f, theme::kText, Align::Left, true);
    ui.Paint().Fill({body.x, body.y + 4.0f, 64.0f, 3.0f}, theme::kAccent);
    body.CutTop(18.0f);
    ui.LabelIn(body.CutTop(20.0f), "A day among the animals, on foot.", theme::kBody, theme::kMuted);
    body.CutTop(34.0f);

    const auto button = [&](std::string_view text, bool primary = false, bool enabled = true) {
        const Box box = body.CutTop(primary ? 46.0f : 40.0f);
        body.CutTop(8.0f);
        return ui.Button(box, text,
                         {.selected = primary,
                          .enabled = enabled,
                          .bold = primary,
                          .size = theme::kBody + (primary ? 3.0f : 1.0f)});
    };
    if (button("Visit Zoo", true)) {
        visit.request = VisitRequest::NewVisit;
        visit.window = VisitWindow::None;
    }
    if (button("Continue Visit", false, visit.canContinue)) {
        visit.request = VisitRequest::Continue;
        visit.window = VisitWindow::None;
    }
    if (button("Load Visit"))
        visit.window = visit.window == VisitWindow::Load ? VisitWindow::None : VisitWindow::Load;
    if (button("Settings"))
        visit.window = visit.window == VisitWindow::Settings ? VisitWindow::None : VisitWindow::Settings;
    if (button("Quit"))
        visit.request = VisitRequest::Quit;

    ui.LabelIn({body.x, screen.h - 40.0f, body.w, 20.0f}, Format("Version %s", zoo::VersionString()),
               theme::kSmall, theme::kFaint);

    const float left = band.Right() + 36.0f;
    const float top = std::round(0.16f * screen.h);
    if (visit.window == VisitWindow::Settings)
        Settings(ui, visit, {left, top, 440.0f, 372.0f});
    else if (visit.window == VisitWindow::Load)
        Load(ui, visit, {left, top, std::min(560.0f, screen.w - left - 24.0f), 420.0f});
}

void VisitorHud::Pause(ui::Ui& ui, Visit& visit) {
    const Box screen{0.0f, 0.0f, ui.Screen().X, ui.Screen().Y};
    ui.Claim(screen);
    ui.Paint().Fill(screen, ui::Rgb(6, 10, 14, 120));
    if (visit.window == VisitWindow::Settings) {
        Settings(ui, visit,
                 {std::round(screen.MidX() - 220.0f), std::round(screen.MidY() - 197.0f), 440.0f, 372.0f});
        return;
    }
    if (visit.window == VisitWindow::Load) {
        Load(ui, visit,
             {std::round(screen.MidX() - 280.0f), std::round(screen.MidY() - 210.0f), 560.0f, 420.0f});
        return;
    }
    const Box panel{std::round(screen.MidX() - 160.0f), std::round(screen.MidY() - 190.0f), 320.0f, 366.0f};
    ui.Panel(panel);
    Box body = panel.Inset(28.0f, 22.0f);
    ui.LabelIn(body.CutTop(32.0f), "Paused", 24.0f, theme::kText, Align::Left, true);
    ui.Paint().Fill({body.x, body.y + 2.0f, 44.0f, 3.0f}, theme::kAccent);
    body.CutTop(20.0f);
    const auto button = [&](std::string_view text, bool primary = false) {
        const Box box = body.CutTop(38.0f);
        body.CutTop(8.0f);
        return ui.Button(box, text, {.selected = primary, .bold = primary, .size = theme::kBody + 1.0f});
    };
    if (button("Resume", true))
        visit.screen = VisitScreen::Walking;
    if (button("Save Visit"))
        visit.request = VisitRequest::Save;
    if (button("Load Visit"))
        visit.window = VisitWindow::Load;
    if (button("Settings"))
        visit.window = VisitWindow::Settings;
    if (button("Back to the Title"))
        visit.request = VisitRequest::ToTitle;
    if (button("Quit"))
        visit.request = VisitRequest::Quit;
}

void VisitorHud::Settings(ui::Ui& ui, Visit& visit, const Box& box) {
    bool closed = false;
    Box body = ui.Window(box, "Settings", &closed);
    if (closed)
        visit.window = VisitWindow::None;
    VisitSettings& s = visit.settings;
    const auto row = [&] {
        const Box line = body.CutTop(30.0f);
        body.CutTop(8.0f);
        return line;
    };
    if (const int step = Stepper(ui, row(), "Mouse speed", Format("%.1f", static_cast<double>(s.sensitivity)),
                                 s.sensitivity > 0.25f, s.sensitivity < 2.95f)) {
        s.sensitivity = std::clamp(
            std::round((s.sensitivity + 0.1f * static_cast<float>(step)) * 10.0f) / 10.0f, 0.2f, 3.0f);
        visit.settingsChanged = true;
    }
    visit.settingsChanged |= Switch(ui, row(), "Invert the mouse up and down", s.invertY);
    if (const int step =
            Stepper(ui, row(), "Field of view", Format("%.0f°", static_cast<double>(s.fieldOfView)),
                    s.fieldOfView > 45.5f, s.fieldOfView < 74.5f)) {
        s.fieldOfView = std::clamp(s.fieldOfView + 2.0f * static_cast<float>(step), 45.0f, 75.0f);
        visit.settingsChanged = true;
    }
    visit.settingsChanged |= Switch(ui, row(), "Sway with the steps", s.headBob);
    if (const int step = Stepper(ui, row(), "Sound", Format("%.0f %%", static_cast<double>(s.volume) * 100.0),
                                 s.volume > 0.05f, s.volume < 0.95f)) {
        s.volume =
            std::clamp(std::round((s.volume + 0.1f * static_cast<float>(step)) * 10.0f) / 10.0f, 0.0f, 1.0f);
        visit.settingsChanged = true;
    }
    if (const int step = Stepper(ui, row(), "Visitors", kCrowdLevels[static_cast<std::size_t>(s.crowd)].name,
                                 s.crowd > 0, s.crowd + 1 < static_cast<int>(kCrowdLevels.size()))) {
        s.crowd = std::clamp(s.crowd + step, 0, static_cast<int>(kCrowdLevels.size()) - 1);
        visit.settingsChanged = true;
    }
    visit.settingsChanged |= Switch(ui, row(), "Diagnostics (F4)", s.diagnostics);
    hud::Note(ui, body, "F11 switches between a window and the full screen, F12 saves a picture.");
}

void VisitorHud::Load(ui::Ui& ui, Visit& visit, const Box& box) {
    bool closed = false;
    Box body = ui.Window(box, "Load Visit", &closed);
    if (closed)
        visit.window = VisitWindow::None;
    if (saves_.empty()) {
        hud::Note(ui, body,
                  Format("No saved visits yet. They are kept in %s", SaveDirectory().string().c_str()));
        return;
    }
    constexpr float kLine = 34.0f;
    const float offset = ui.ScrollBegin("visit-list", body, kLine * static_cast<float>(saves_.size()));
    float y = body.y - offset;
    for (const SaveEntry& entry : saves_) {
        const Box row{body.x, y, body.w - 10.0f, kLine - 4.0f};
        y += kLine;
        if (row.Bottom() < body.y || row.y > body.Bottom())
            continue;
        if (ui.Button(row, "", {.enabled = entry.readable})) {
            visit.request = VisitRequest::Load;
            visit.requestPath = entry.path.string();
            visit.window = VisitWindow::None;
        }
        Box inner = row.Inset(10.0f, 0.0f);
        ui.LabelIn(inner.CutRight(250.0f),
                   entry.readable ? Format("%s · %d animals", Written(entry).c_str(), entry.summary.animals)
                                  : std::string("cannot be read"),
                   theme::kSmall, entry.readable ? theme::kMuted : theme::kAlarm, Align::Right);
        ui.LabelIn(inner, entry.title, theme::kBody, theme::kText);
    }
    ui.ScrollEnd();
}

void VisitorHud::Walking(ui::Ui& ui, Visit& visit) {
    const Vector2 screen = ui.Screen();
    // A dot to aim with: where the eyes look.
    const Vector2 middle(std::round(0.5f * screen.X), std::round(0.5f * screen.Y));
    ui.Paint().Disc(middle, 2.6f, ui::Rgb(0, 0, 0, 70));
    ui.Paint().Disc(middle, 1.7f, ui::Rgb(255, 255, 255, 170));

    if (!visit.clock.empty()) {
        const float width = ui.Paint().TextWidth(visit.clock, theme::kTitle, true);
        Shadowed(ui, visit.clock, screen.X - width - 22.0f, 16.0f, theme::kTitle, ui::Rgb(255, 255, 255, 215),
                 true);
    }
    if (!visit.prompt.empty())
        KeyHints(ui, middle.X, std::round(screen.Y * 0.62f), {{"E", visit.prompt}}, 1.0f);
    if (visit.window == VisitWindow::Board)
        Board(ui, visit);
    else if (visit.window == VisitWindow::Map)
        Map(ui, visit);
    else
        Card(ui, visit);

    visit.hintLeft = std::max(0.0f, visit.hintLeft - ui.Seconds());
    if (visit.hintLeft > 0.0f && visit.window == VisitWindow::None)
        KeyHints(ui, middle.X, screen.Y - 52.0f,
                 {{"W A S D", "Walk"},
                  {"Mouse", "Look around"},
                  {"Shift", "Walk / run"},
                  {"M", "Map"},
                  {"Esc", "Menu"}},
                 std::min(1.0f, visit.hintLeft / 1.5f));
}

void VisitorHud::Notice(ui::Ui& ui, Visit& visit) {
    if (visit.flashLeft <= 0.0f || visit.flash.empty())
        return;
    const float alpha = std::min(1.0f, visit.flashLeft / 0.6f);
    const float width = ui.Paint().TextWidth(visit.flash, theme::kBody) + 36.0f;
    const Box plate{std::round(0.5f * (ui.Screen().X - width)), 22.0f, width, 32.0f};
    ui.Paint().Round(plate, 8.0f, ui::Rgb(10, 14, 18, static_cast<int>(190.0f * alpha)));
    ui.LabelIn(plate, visit.flash, theme::kBody, ui::Faded(theme::kText, alpha), Align::Center);
}

void VisitorHud::Card(ui::Ui& ui, const Visit& visit) {
    const AnimalCard& card = visit.card;
    if (card.shown <= 0.0f || card.species.empty())
        return;
    // Low in the view, out of the way of what one looks at.
    const float alpha = card.shown;
    const std::string first = card.name.empty() ? card.species : card.name + ", " + card.species;
    const std::string second = card.about + "  -  " + card.doing;
    const float width = std::max({ui.Paint().TextWidth(first, theme::kBody + 1.0f, true),
                                  ui.Paint().TextWidth(card.latin, theme::kSmall),
                                  ui.Paint().TextWidth(second, theme::kSmall)}) +
                        36.0f;
    const Vector2 screen = ui.Screen();
    const Box plate{std::round(0.5f * (screen.X - width)), std::round(screen.Y * 0.74f), width, 74.0f};
    ui.Paint().Round(plate, 8.0f, ui::Rgb(10, 14, 18, static_cast<int>(165.0f * alpha)));
    Box body = plate.Inset(18.0f, 9.0f);
    ui.LabelIn(body.CutTop(22.0f), first, theme::kBody + 1.0f, ui::Faded(theme::kText, alpha), Align::Center,
               true);
    ui.LabelIn(body.CutTop(16.0f), card.latin, theme::kSmall, ui::Faded(theme::kMuted, alpha), Align::Center);
    ui.LabelIn(body.CutTop(18.0f), second, theme::kSmall, ui::Faded(theme::kText, 0.92f * alpha),
               Align::Center);
}

void VisitorHud::Board(ui::Ui& ui, const Visit& visit) {
    const BoardText& text = visit.board;
    const Vector2 screen = ui.Screen();
    constexpr float kWidth = 520.0f;
    // As tall as what it says: measured first.
    const float inner = kWidth - 52.0f;
    float height = 26.0f + 30.0f + 20.0f + 14.0f + 21.0f * static_cast<float>(text.rows.size()) + 12.0f;
    for (const std::string& fact : text.facts)
        height += static_cast<float>(ui.Paint().Wrap(fact, inner - 14.0f, theme::kSmall + 1.0f).size()) *
                      ui.Paint().LineHeight(theme::kSmall + 1.0f) +
                  7.0f;
    height += (text.residents.empty() ? 0.0f : 24.0f) + (text.others.empty() ? 0.0f : 20.0f) + 44.0f;
    const Box panel{std::round(0.5f * (screen.X - kWidth)),
                    std::round(std::max(20.0f, 0.5f * (screen.Y - height))), kWidth, height};
    ui.Paint().Shadow(panel, 18.0f, 0.5f);
    ui.Paint().Frame(panel, 8.0f, ui::Rgb(238, 232, 214, 246), ui::Rgb(94, 72, 44, 255), 3.0f);
    const ui::Rgba ink = ui::Rgb(38, 34, 28, 255);
    const ui::Rgba soft = ui::Rgb(104, 92, 74, 255);
    Box body = panel.Inset(26.0f, 13.0f);
    ui.LabelIn(body.CutTop(30.0f), text.title, theme::kTitle, ink, Align::Left, true);
    ui.LabelIn(body.CutTop(20.0f), text.latin, theme::kBody, soft);
    body.CutTop(6.0f);
    ui.Paint().Fill({body.x, body.y, body.w, 2.0f}, ui::Rgb(94, 72, 44, 120));
    body.CutTop(8.0f);
    for (const auto& [label, value] : text.rows) {
        Box row = body.CutTop(21.0f);
        ui.LabelIn(row.CutLeft(132.0f), label, theme::kSmall + 1.0f, soft);
        ui.LabelIn(row, value, theme::kSmall + 1.0f, ink);
    }
    body.CutTop(12.0f);
    for (const std::string& fact : text.facts) {
        ui.Paint().Disc(Vector2(body.x + 4.0f, body.y + 9.0f), 2.5f, ui::Rgb(94, 72, 44, 255));
        Box line = body;
        line.CutLeft(14.0f);
        const float used = ui.Paragraph(line, fact, theme::kSmall + 1.0f, ink);
        body.CutTop(used + 7.0f);
    }
    if (!text.residents.empty())
        ui.LabelIn(body.CutTop(24.0f), text.residents, theme::kSmall + 1.0f, ink, Align::Left, true);
    if (!text.others.empty())
        ui.LabelIn(body.CutTop(20.0f), text.others, theme::kSmall + 1.0f, soft);
    KeyHints(ui, panel.MidX(), panel.Bottom() - 36.0f, {{"E", "Step back"}}, 1.0f);
}

void VisitorHud::Map(ui::Ui& ui, Visit& visit) {
    using Kind = VisitMap::Kind;
    const VisitMap& map = visit.map;
    const Vector2 screen = ui.Screen();
    const float spanX = std::max(1.0f, map.maxX - map.minX);
    const float spanZ = std::max(1.0f, map.maxZ - map.minZ);
    const float scale = std::min((screen.X - 120.0f) / spanX, (screen.Y - 150.0f) / spanZ);
    const Box plan{std::round(0.5f * (screen.X - spanX * scale)),
                   std::round(0.5f * (screen.Y - spanZ * scale)) + 8.0f, std::round(spanX * scale),
                   std::round(spanZ * scale)};
    const Box panel{plan.x - 16.0f, plan.y - 46.0f, plan.w + 32.0f, plan.h + 62.0f};
    ui.Paint().Fill({0.0f, 0.0f, screen.X, screen.Y}, ui::Rgb(0, 0, 0, 90));
    ui.Paint().Shadow(panel, 18.0f, 0.5f);
    ui.Paint().Frame(panel, 8.0f, ui::Rgb(238, 232, 214, 250), ui::Rgb(94, 72, 44, 255), 3.0f);
    ui.LabelIn({panel.x + 18.0f, panel.y + 8.0f, 300.0f, 30.0f}, "Zoo map", theme::kTitle,
               ui::Rgb(38, 34, 28, 255), Align::Left, true);
    ui.LabelIn({panel.Right() - 318.0f, panel.y + 10.0f, 300.0f, 26.0f}, "North is up   -   M closes",
               theme::kSmall, ui::Rgb(104, 92, 74, 255), Align::Right);

    // North (-z) is up: the plan reads like the ground one walks, the entrance at the foot.
    ui.Paint().PushClip(plan);
    for (const VisitMap::Shape& shape : map.shapes) {
        ui::Rgba color = ui::Rgb(150, 186, 118, 255);
        switch (shape.kind) {
            case Kind::Ground:
                break;
            case Kind::Water:
                color = ui::Rgb(104, 158, 204, 255);
                break;
            case Kind::Habitat:
                color = ui::Rgb(206, 190, 128, 255);
                break;
            case Kind::Path:
                color = ui::Rgb(232, 226, 210, 255);
                break;
            case Kind::Building:
                color = ui::Rgb(146, 98, 72, 255);
                break;
            case Kind::Entrance:
                color = ui::Rgb(198, 62, 50, 255);
                break;
        }
        const float x0 = std::floor(plan.x + (shape.x - map.minX) * scale);
        const float y0 = std::floor(plan.y + (shape.z - map.minZ) * scale);
        const float x1 = std::ceil(plan.x + (shape.x + shape.w - map.minX) * scale);
        const float y1 = std::ceil(plan.y + (shape.z + shape.d - map.minZ) * scale);
        ui.Paint().Fill({x0, y0, x1 - x0, y1 - y0}, color);
    }
    // The names: the animals first, a house's name moves down or gives way where it would cover one.
    std::vector<Box> placed;
    for (const VisitMap::Label& label : map.labels) {
        const float size = label.habitat ? theme::kSmall + 1.0f : theme::kSmall - 1.0f;
        const float width = ui.Paint().TextWidth(label.text, size, label.habitat);
        // A name near the plan's edge moves in rather than being cut.
        const float x = std::clamp(std::round(plan.x + (label.x - map.minX) * scale - 0.5f * width),
                                   plan.x + 7.0f, std::max(plan.x + 7.0f, plan.Right() - width - 7.0f));
        float y = std::round(plan.y + (label.z - map.minZ) * scale - 8.0f);
        const auto covered = [&] {
            return std::ranges::any_of(placed, [&](const Box& other) {
                return x - 5.0f < other.Right() && x + width + 5.0f > other.x && y - 2.0f < other.Bottom() &&
                       y + 17.0f > other.y;
            });
        };
        int moves = 0;
        while (covered() && moves < 3) {
            y += 20.0f;
            ++moves;
        }
        if (covered())
            continue;
        placed.push_back({x - 5.0f, y - 2.0f, width + 10.0f, 19.0f});
        ui.Paint().Round({x - 5.0f, y - 2.0f, width + 10.0f, 19.0f}, 4.0f,
                         label.habitat ? ui::Rgb(38, 34, 28, 200) : ui::Rgb(255, 255, 255, 190));
        ui.Paint().Text(label.text, Vector2(x, y), size,
                        label.habitat ? ui::Rgb(250, 244, 226, 255) : ui::Rgb(60, 50, 40, 255),
                        label.habitat);
    }
    // Places to go to: a click on one takes you there. The one under the mouse is the nearest
    // within reach, so that markers close together still each have their turn.
    int hovered = -1;
    float nearest = 13.0f * 13.0f;
    const auto onPlan = [&](const VisitMap::Place& place) {
        return Vector2(plan.x + (place.x - map.minX) * scale, plan.y + (place.z - map.minZ) * scale);
    };
    if (ui.Hovered(plan)) {
        for (std::size_t i = 0; i < map.places.size(); ++i) {
            const Vector2 at = onPlan(map.places[i]);
            const float dx = at.X - ui.Mouse().X;
            const float dy = at.Y - ui.Mouse().Y;
            if (dx * dx + dy * dy < nearest) {
                nearest = dx * dx + dy * dy;
                hovered = static_cast<int>(i);
            }
        }
    }
    for (std::size_t i = 0; i < map.places.size(); ++i) {
        const VisitMap::Place& place = map.places[i];
        const bool lit = static_cast<int>(i) == hovered;
        const Vector2 at = onPlan(place);
        const float radius = (place.habitat ? 6.5f : 5.5f) + (lit ? 3.0f : 0.0f);
        ui.Paint().Disc(at, radius + 2.0f, ui::Rgb(255, 255, 255, 235));
        ui.Paint().Disc(at, radius, place.habitat ? ui::Rgb(34, 110, 62, 255) : ui::Rgb(38, 84, 160, 255));
    }
    if (hovered >= 0 && ui.Clicked(plan))
        visit.travelTo = hovered;

    // You are here: a dot and, ahead of it, where you look.
    const Vector2 you(plan.x + (map.you.X - map.minX) * scale, plan.y + (map.you.Y - map.minZ) * scale);
    const Vector2 ahead(std::sin(map.yaw), std::cos(map.yaw));
    for (int step = 3; step >= 1; --step)
        ui.Paint().Disc(you + ahead * (5.0f * static_cast<float>(step)), 4.2f - static_cast<float>(step),
                        ui::Rgb(200, 40, 40, 255));
    ui.Paint().Disc(you, 7.0f, ui::Rgb(255, 255, 255, 255));
    ui.Paint().Disc(you, 5.0f, ui::Rgb(214, 40, 40, 255));
    if (hovered >= 0) {
        const std::string text = "Go to " + map.places[static_cast<std::size_t>(hovered)].name;
        const float size = theme::kSmall + 1.0f;
        const float wide = ui.Paint().TextWidth(text, size, true) + 16.0f;
        const Vector2 at = onPlan(map.places[static_cast<std::size_t>(hovered)]);
        const Box tip{std::clamp(std::round(at.X - 0.5f * wide), plan.x + 4.0f, plan.Right() - wide - 4.0f),
                      at.Y - 40.0f < plan.y + 4.0f ? at.Y + 14.0f : at.Y - 40.0f, wide, 26.0f};
        ui.Paint().Round(tip, 5.0f, ui::Rgb(38, 34, 28, 235));
        ui.Paint().Text(text, Vector2(tip.x + 8.0f, tip.y + 4.0f), size, ui::Rgb(250, 244, 226, 255), true);
    }
    ui.Paint().PopClip();
    const float width = ui.Paint().TextWidth("You are here", theme::kSmall, true);
    ui.Paint().Disc(Vector2(panel.x + 26.0f, panel.Bottom() - 14.0f), 5.0f, ui::Rgb(214, 40, 40, 255));
    ui.Paint().Text("You are here", Vector2(panel.x + 38.0f, panel.Bottom() - 22.0f), theme::kSmall,
                    ui::Rgb(38, 34, 28, 255), true);
    // The legend of the places one can go to.
    float x = panel.x + 54.0f + width;
    ui.Paint().Disc(Vector2(x, panel.Bottom() - 14.0f), 6.0f, ui::Rgb(34, 110, 62, 255));
    x += ui.Paint().Text("Habitat", Vector2(x + 10.0f, panel.Bottom() - 22.0f), theme::kSmall,
                         ui::Rgb(38, 34, 28, 255)) +
         28.0f;
    ui.Paint().Disc(Vector2(x, panel.Bottom() - 14.0f), 5.0f, ui::Rgb(38, 84, 160, 255));
    x += ui.Paint().Text("Building", Vector2(x + 10.0f, panel.Bottom() - 22.0f), theme::kSmall,
                         ui::Rgb(38, 34, 28, 255)) +
         28.0f;
    ui.Paint().Text("Click a place to go there", Vector2(x, panel.Bottom() - 22.0f), theme::kSmall,
                    ui::Rgb(104, 92, 74, 255));
}

void VisitorHud::Diagnostics(ui::Ui& ui, const Visit& visit) {
    const VisitFigures& f = visit.figures;
    const Box panel{14.0f, 14.0f, 292.0f, 292.0f};
    ui.Paint().Round(panel, 6.0f, ui::Rgb(10, 14, 18, 190));
    Box body = panel.Inset(12.0f, 9.0f);
    hud::Heading(ui, body, "VISITORS");
    hud::Fact(ui, body, "In the zoo (simulated)", Format("%d", f.visitors));
    hud::Fact(ui, body, "Near enough to draw", Format("%d", f.inRange));
    hud::Fact(ui, body, "Drawn of those in view", Format("%d of %d", f.drawn, f.inView),
              f.inRange > 0 && f.castReady == 0 ? theme::kAlarm : theme::kText);
    hud::Fact(ui, body, "Avatars ready (CNA)", Format("%d of %d", f.castReady, f.castSize),
              f.castReady == 0 ? theme::kAlarm : theme::kText);
    hud::Fact(ui, body, "Avatars that failed", Format("%d", f.castFailed),
              f.castFailed > 0 ? theme::kAlarm : theme::kText);
    body.CutTop(4.0f);
    hud::Heading(ui, body, "WORLD");
    hud::Fact(ui, body, "Animals (drawn)", Format("%d (%d)", f.animals, f.animalsDrawn));
    hud::Fact(ui, body, "Here",
              Format("%.1f, %.1f  heading %.0f°", static_cast<double>(f.feet.X),
                     static_cast<double>(f.feet.Z), static_cast<double>(f.yawDegrees)));
    hud::Fact(ui, body, "Frames a second", Format("%.0f", static_cast<double>(f.framesPerSecond)));
    body.CutTop(4.0f);
    hud::Heading(ui, body, "SOUND");
    hud::Fact(ui, body, "Recordings",
              f.sound.failed > 0 ? Format("%d, %d failed", f.sound.files, f.sound.failed)
                                 : Format("%d", f.sound.files),
              !f.sound.device || f.sound.files == 0 ? theme::kAlarm : theme::kText);
    hud::Fact(ui, body, "Heard now", Format("%d of %d", f.sound.audible, f.sound.voices));
    hud::Fact(ui, body, "Last call", f.sound.lastCall.empty() ? std::string("-") : f.sound.lastCall);
}

}  // namespace zoogame
