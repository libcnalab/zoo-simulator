// SPDX-License-Identifier: MIT
#include "Hud.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace zoogame {

namespace {

using ui::Box;
using ui::Rgb;
namespace theme = ui::theme;

using hud::kBarHeight;

/// The pause sign, drawn (the font has none).
void PauseSign(ui::Ui& ui, const Box& box, ui::Rgba color) {
    const float h = 11.0f;
    const float y = std::round(box.MidY() - 0.5f * h);
    ui.Paint().Fill({std::round(box.MidX()) - 5.0f, y, 3.5f, h}, color);
    ui.Paint().Fill({std::round(box.MidX()) + 1.5f, y, 3.5f, h}, color);
}

}  // namespace

std::string ClockText(zoo::GameTime time) {
    const float hour = time.Hour();
    const int h = static_cast<int>(hour);
    const int m = static_cast<int>((hour - static_cast<float>(h)) * 60.0f);
    return Format("%02d:%02d", h, m);
}

std::string DateText(zoo::GameTime time) {
    static constexpr const char* kMonths[12] = {"January",   "February", "March",    "April",
                                                "May",       "June",     "July",     "August",
                                                "September", "October",  "November", "December"};
    return Format("%d %s, year %d", time.DayOfMonth(), kMonths[(time.Month() + 11) % 12], time.Year());
}

std::string_view SeasonName(zoo::Season season) {
    switch (season) {
        case zoo::Season::Spring:
            return "Spring";
        case zoo::Season::Summer:
            return "Summer";
        case zoo::Season::Autumn:
            return "Autumn";
        case zoo::Season::Winter:
            return "Winter";
    }
    return "";
}

std::string_view WeatherName(zoo::WeatherKind kind) {
    switch (kind) {
        case zoo::WeatherKind::Clear:
            return "Clear";
        case zoo::WeatherKind::Cloudy:
            return "Cloudy";
        case zoo::WeatherKind::Overcast:
            return "Overcast";
        case zoo::WeatherKind::Rain:
            return "Rain";
        case zoo::WeatherKind::Storm:
            return "Storm";
        case zoo::WeatherKind::Snow:
            return "Snow";
        case zoo::WeatherKind::Count:
            break;
    }
    return "";
}

ui::Rgba SeverityColor(zoo::Severity severity) {
    switch (severity) {
        case zoo::Severity::Info:
            return theme::kInfo;
        case zoo::Severity::Good:
            return theme::kGood;
        case zoo::Severity::Warning:
            return theme::kWarning;
        case zoo::Severity::Alarm:
            return theme::kAlarm;
    }
    return theme::kInfo;
}

ui::Rgba WelfareColor(float welfare) {
    return welfare >= 0.7f ? theme::kGood : welfare >= 0.45f ? theme::kWarning : theme::kAlarm;
}

std::string Percent(float fraction) {
    return Format("%d %%", static_cast<int>(std::lround(std::clamp(fraction, 0.0f, 1.0f) * 100.0f)));
}

void Hud::Build(ui::Ui& ui, Play& play) {
    if (play.mainMenu) {
        // The title screen; its windows (a new zoo, loading, the help) lie over it.
        ui.SetEnabled(play.dialog == Dialog::None);
        TitleScreen(ui, play);
        ui.SetEnabled(true);
        Windows(ui, play);
        HintLine(ui, play);  // "The zoo could not be loaded"
        return;
    }
    // A modal window stops everything beneath it.
    const bool modal = IsModal(play.dialog);
    ui.SetEnabled(!modal);
    TopBar(ui, play);
    Toolbar(ui, play);
    HintLine(ui, play);
    PalettePanel(ui, play);
    Inspector(ui, play);
    Toasts(ui, play);
    ui.SetEnabled(true);
    if (modal) {
        const Box screen{0.0f, 0.0f, ui.Screen().X, ui.Screen().Y};
        ui.Claim(screen);
        ui.Paint().Fill(screen, Rgb(6, 10, 12, 130));
    }
    Windows(ui, play);
}

void Hud::TopBar(ui::Ui& ui, Play& play) {
    const zoo::Zoo& zoo = *play.zoo;
    Box bar{0.0f, 0.0f, ui.Screen().X, kBarHeight};
    ui.Claim(bar);
    ui.Paint().Fill(bar, theme::kPanel);
    ui.Paint().Fill({0.0f, kBarHeight - 1.0f, bar.w, 1.0f}, theme::kPanelEdge);
    bar = bar.Inset(10.0f, 5.0f);

    // Right to left: the speed of time, then the figures.
    struct Speed {
        int speed;
        const char* label;
        const char* tip;
    };
    static constexpr Speed kSpeeds[] = {{8, "▶▶▶", "Very fast (4)"},
                                        {3, "▶▶", "Fast (3)"},
                                        {1, "▶", "Normal speed (2)"},
                                        {0, "", "Pause (space)"}};
    for (const Speed& s : kSpeeds) {
        const Box button = bar.CutRight(s.speed == 8 ? 44.0f : 34.0f);
        bar.CutRight(3.0f);
        if (ui.Button(button, s.label, {.selected = play.speed == s.speed, .size = 10.0f, .tooltip = s.tip}))
            play.speed = s.speed;
        if (s.speed == 0)
            PauseSign(ui, button, theme::kText);
    }
    bar.CutRight(10.0f);

    const zoo::GameTime now = zoo.Time();
    ui.LabelIn(bar.CutRight(ui.Paint().TextWidth("00:00", theme::kTitle, true) + 4.0f), ClockText(now),
               theme::kTitle, theme::kText, ui::Align::Right, true);
    bar.CutRight(12.0f);
    const std::string date =
        Format("%s · %s", std::string(SeasonName(now.SeasonNow())).c_str(), DateText(now).c_str());
    ui.LabelIn(bar.CutRight(ui.Paint().TextWidth(date, theme::kBody) + 4.0f), date, theme::kBody,
               theme::kMuted, ui::Align::Right);
    bar.CutRight(16.0f);

    const zoo::WeatherNow& weather = zoo.Weather();
    const char* icon = "☀";
    if (weather.kind == zoo::WeatherKind::Cloudy || weather.kind == zoo::WeatherKind::Overcast)
        icon = "☁";
    else if (weather.kind == zoo::WeatherKind::Rain || weather.kind == zoo::WeatherKind::Storm)
        icon = "☂";
    else if (weather.kind == zoo::WeatherKind::Snow)
        icon = "❄";
    const std::string sky = Format("%s %s, %d °C", icon, std::string(WeatherName(weather.kind)).c_str(),
                                   static_cast<int>(std::lround(weather.temperatureC)));
    const Box skyBox = bar.CutRight(ui.Paint().TextWidth(sky, theme::kBody) + 4.0f);
    ui.LabelIn(skyBox, sky, theme::kBody, theme::kMuted, ui::Align::Right);
    ui.Tooltip(skyBox, Format("Wind %d %%, cloud %d %%", static_cast<int>(std::lround(weather.wind * 100.0f)),
                              static_cast<int>(std::lround(weather.cloud * 100.0f))));

    // Left to right: the zoo, its money, its animals.
    const float nameWidth = std::min(ui.Paint().TextWidth(zoo.Name(), theme::kTitle, true) + 8.0f, 260.0f);
    ui.LabelIn(bar.CutLeft(nameWidth), zoo.Name(), theme::kTitle, theme::kText, ui::Align::Left, true);
    bar.CutLeft(14.0f);

    const zoo::Money balance = zoo.Accounts().Balance();
    const std::string money = zoo::FormatMoney(balance);
    const Box moneyBox = bar.CutLeft(ui.Paint().TextWidth(money, theme::kTitle, true) + 16.0f);
    if (ui.Button(moneyBox, money,
                  {.selected = play.dialog == Dialog::Finances,
                   .bold = true,
                   .size = theme::kTitle,
                   .tooltip = "Finances",
                   .text = balance < 0 ? theme::kAlarm : theme::kMoney}))
        play.dialog = play.dialog == Dialog::Finances ? Dialog::None : Dialog::Finances;
    bar.CutLeft(14.0f);

    const int animals = zoo.AnimalCount();
    const std::string count = Format("%d animal%s", animals, animals == 1 ? "" : "s");
    const Box countBox = bar.CutLeft(ui.Paint().TextWidth(count, theme::kBody) + 16.0f);
    if (ui.Button(countBox, count,
                  {.selected = play.dialog == Dialog::Animals, .tooltip = "The list of animals"}))
        play.dialog = play.dialog == Dialog::Animals ? Dialog::None : Dialog::Animals;
    bar.CutLeft(12.0f);

    if (animals > 0) {
        const float welfare = zoo.MeanWelfare();
        const Box label = bar.CutLeft(ui.Paint().TextWidth("Welfare", theme::kBody) + 8.0f);
        ui.LabelIn(label, "Welfare", theme::kBody, theme::kMuted);
        Box meter = bar.CutLeft(70.0f);
        ui.Bar({meter.x, std::round(meter.MidY() - 4.0f), meter.w, 8.0f}, welfare, WelfareColor(welfare));
        ui.Tooltip({label.x, label.y, label.w + meter.w, label.h},
                   Format("Mean welfare of the animals: %s", Percent(welfare).c_str()));
    }
}

void Hud::Toasts(ui::Ui& ui, Play& play) {
    const zoo::Zoo& zoo = *play.zoo;
    const auto& all = zoo.Notifications();
    if (!primed_) {
        // What happened before this session began is history, not news.
        seen_ = zoo.NextNotificationId() - 1;
        primed_ = true;
    }
    for (const zoo::Notification& n : all) {
        if (n.id > seen_) {
            toasts_.push_back({n.id, 0.0f});
            seen_ = n.id;
        }
    }
    constexpr float kLife = 9.0f;
    constexpr std::size_t kMost = 5;
    for (Toast& toast : toasts_)
        toast.age += ui.Seconds();
    std::erase_if(toasts_, [](const Toast& t) { return t.age > kLife; });
    if (toasts_.size() > kMost)
        toasts_.erase(toasts_.begin(), toasts_.end() - static_cast<std::ptrdiff_t>(kMost));

    constexpr float kWidth = 330.0f;
    // Beside the inspector when something is selected.
    const float shift = play.selection.kind != Picked::None ? 310.0f : 0.0f;
    float y = kBarHeight + 10.0f;
    std::uint32_t dismissed = 0;
    for (auto it = toasts_.rbegin(); it != toasts_.rend(); ++it) {
        const auto found =
            std::find_if(all.begin(), all.end(), [&](const zoo::Notification& n) { return n.id == it->id; });
        if (found == all.end())
            continue;
        const zoo::Notification& n = *found;
        const auto lines = ui.Paint().Wrap(n.text, kWidth - 30.0f, theme::kSmall + 1.0f);
        const float lineHeight = ui.Paint().LineHeight(theme::kSmall + 1.0f);
        const Box box{ui.Screen().X - kWidth - 10.0f - shift, y, kWidth,
                      lineHeight * static_cast<float>(lines.size()) + 14.0f};
        // The last second fades it out.
        const float fade = std::clamp(kLife - it->age, 0.0f, 1.0f);
        ui.Claim(box);
        ui.Paint().Shadow(box, 10.0f, 0.3f * fade);
        ui.Paint().Frame(box, 5.0f, ui::Faded(theme::kPanel, fade), ui::Faded(theme::kPanelEdge, fade));
        ui.Paint().Round({box.x + 5.0f, box.y + 5.0f, 4.0f, box.h - 10.0f}, 2.0f,
                         ui::Faded(SeverityColor(n.severity), fade));
        float lineY = box.y + 7.0f;
        for (const std::string& line : lines) {
            ui.Paint().Text(line, ui::Vector2(box.x + 17.0f, lineY), theme::kSmall + 1.0f,
                            ui::Faded(theme::kText, fade));
            lineY += lineHeight;
        }
        if (ui.Clicked(box)) {
            if (n.located)
                play.camera->GoTo(n.position);
            dismissed = n.id;
        } else if (n.located) {
            ui.Tooltip(box, "Click to look there");
        }
        y += box.h + 6.0f;
    }
    if (dismissed != 0)
        std::erase_if(toasts_, [&](const Toast& t) { return t.id == dismissed; });
}

}  // namespace zoogame
