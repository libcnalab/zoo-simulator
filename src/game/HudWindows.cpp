// SPDX-License-Identifier: MIT
// The windows in the middle of the screen: the zoo at a glance, the list of
// animals, the books, the messages, the menu, saving and loading, a new zoo,
// the title screen.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "HudWidgets.hpp"
#include "zoo/base/Version.hpp"

namespace zoogame {

namespace {

using hud::Choice;
using hud::Fact;
using hud::Heading;
using hud::kGap;
using hud::Meter;
using hud::Note;
using ui::Align;
using ui::Box;
namespace theme = ui::theme;

/// A window in the middle of what the bars leave free.
Box Centered(ui::Ui& ui, float width, float height) {
    const float top = hud::kBarHeight + hud::kMargin;
    const float bottom = ui.Screen().Y - hud::kToolbarHeight - 8.0f - hud::kMargin;
    const float w = std::min(width, ui.Screen().X - 2.0f * hud::kMargin);
    const float h = std::min(height, bottom - top);
    return {std::round(0.5f * (ui.Screen().X - w)), std::round(top + 0.5f * (bottom - top - h)), w, h};
}

/// A window in the middle of the whole screen (the modal ones).
Box Middle(ui::Ui& ui, float width, float height) {
    const float w = std::min(width, ui.Screen().X - 2.0f * hud::kMargin);
    const float h = std::min(height, ui.Screen().Y - 2.0f * hud::kMargin);
    return {std::round(0.5f * (ui.Screen().X - w)), std::round(0.5f * (ui.Screen().Y - h)), w, h};
}

std::string AgeShort(float years) {
    if (years < 1.0f)
        return Format("%d mo", std::max(1, static_cast<int>(std::lround(years * 12.0f))));
    return Format("%.1f y", static_cast<double>(years));
}

std::string StampText(std::int64_t tick) {
    static constexpr const char* kMonths[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                                "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    const zoo::GameTime time{tick};
    return Format("%d %s, year %d · %s", time.DayOfMonth(), kMonths[(time.Month() + 11) % 12], time.Year(),
                  ClockText(time).c_str());
}

/// The money of a table cell: nothing for nothing.
std::string Cell(zoo::Money amount) {
    return amount == 0 ? std::string("·") : zoo::FormatMoney(amount);
}

/// A row that lights up under the pointer and can be clicked.
bool HotRow(ui::Ui& ui, const Box& row, bool selected = false) {
    if (selected)
        ui.Paint().Round(row, 4.0f, ui::Faded(theme::kAccent, 0.55f));
    else if (ui.Hovered(row))
        ui.Paint().Round(row, 4.0f, theme::kButton);
    return ui.Clicked(row);
}

std::uint64_t FreshSeed() {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(now).count()) %
               899999ULL +
           100000ULL;
}

}  // namespace

void Hud::Windows(ui::Ui& ui, Play& play) {
    const bool opened = play.dialog != shown_;
    shown_ = play.dialog;
    switch (play.dialog) {
        case Dialog::None:
            break;
        case Dialog::Zoo:
            ZooWindow(ui, play);
            break;
        case Dialog::Finances:
            FinancesWindow(ui, play);
            break;
        case Dialog::Animals:
            AnimalsWindow(ui, play);
            break;
        case Dialog::Messages:
            MessagesWindow(ui, play);
            break;
        case Dialog::Menu:
            MenuWindow(ui, play, opened);
            break;
        case Dialog::Save:
            SaveWindow(ui, play, opened);
            break;
        case Dialog::Load:
            LoadWindow(ui, play, opened);
            break;
        case Dialog::NewGame:
            NewGameWindow(ui, play, opened);
            break;
        case Dialog::Help:
            HelpWindow(ui, play);
            break;
    }
}

void Hud::ZooWindow(ui::Ui& ui, Play& play) {
    zoo::Zoo& zoo = *play.zoo;
    bool closed = false;
    Box body = ui.Window(Centered(ui, 620.0f, 470.0f), "The zoo", &closed);
    if (closed)
        play.dialog = Dialog::None;

    if (!ui.Typing())
        zooName_ = zoo.Name();
    {
        Box row = body.CutTop(28.0f);
        ui.LabelIn(row.CutLeft(50.0f), "Name", theme::kBody, theme::kMuted);
        ui.TextField("zoo-name", row.CutLeft(300.0f), zooName_, 40);
        if (!zooName_.empty() && zooName_ != zoo.Name())
            zoo.Rename(zooName_);
        ui.LabelIn(row, DateText(zoo.Time()), theme::kBody, theme::kMuted, Align::Right);
    }
    body.CutTop(8.0f);

    Box left = body.CutLeft(270.0f);
    body.CutLeft(18.0f);

    // What each enclosure holds.
    struct Tally {
        int animals = 0;
        float welfare = 0.0f;
    };
    std::map<std::uint32_t, Tally> tallies;
    std::set<int> kept;
    int loose = 0;
    int sick = 0;
    for (const zoo::Animal& animal : zoo.Fauna().All()) {
        kept.insert(animal.species);
        loose += animal.loose ? 1 : 0;
        sick += animal.sick ? 1 : 0;
        if (const zoo::EnclosureId home = zoo.Habitats().At(animal.position)) {
            Tally& tally = tallies[home.value];
            ++tally.animals;
            tally.welfare += animal.welfare;
        }
    }

    Heading(ui, left, "THE ANIMALS");
    const int animals = zoo.AnimalCount();
    Fact(ui, left, "Animals", Format("%d of %d species", animals, static_cast<int>(kept.size())));
    if (animals > 0) {
        const float welfare = zoo.MeanWelfare();
        Meter(ui, left, "Mean welfare", welfare, WelfareColor(welfare));
    }
    Fact(ui, left, "Born here", Format("%d", zoo.Fauna().Births()));
    Fact(ui, left, "Died", Format("%d", zoo.Fauna().Deaths()));
    Fact(ui, left, "Escapes", Format("%d", zoo.Fauna().Escapes()),
         zoo.Fauna().Escapes() > 0 ? theme::kWarning : theme::kText);
    if (sick > 0)
        Fact(ui, left, "Sick now", Format("%d", sick), theme::kWarning);
    if (loose > 0)
        Fact(ui, left, "On the loose", Format("%d", loose), theme::kAlarm);
    left.CutTop(6.0f);

    Heading(ui, left, "THE MONEY");
    Fact(ui, left, "In the bank", zoo::FormatMoney(zoo.Accounts().Balance()),
         zoo.Accounts().Balance() < 0 ? theme::kAlarm : theme::kMoney);
    if (zoo.Accounts().Loan() > 0)
        Fact(ui, left, "Owed to the bank", zoo::FormatMoney(zoo.Accounts().Loan()), theme::kWarning);
    Fact(ui, left, "Running costs a day", zoo::FormatMoney(zoo.DailyUpkeep()));
    if (zoo.Sandbox())
        Note(ui, left, "A sandbox: nothing costs anything.", theme::kInfo);
    left.CutTop(6.0f);

    Heading(ui, left, "CONSERVATION");
    {
        std::map<zoo::ConservationStatus, int> byStatus;
        for (const int index : kept)
            ++byStatus[play.species->All()[static_cast<std::size_t>(index)].status];
        int threatened = 0;
        for (const auto& [status, count] : byStatus) {
            const bool atRisk = status >= zoo::ConservationStatus::Vulnerable &&
                                status <= zoo::ConservationStatus::ExtinctInTheWild;
            threatened += atRisk ? count : 0;
            Fact(ui, left, zoo::DisplayName(status), Format("%d species", count),
                 atRisk ? theme::kWarning : theme::kText);
        }
        if (kept.empty())
            Note(ui, left, "No animals yet.");
        else if (threatened > 0)
            Note(ui, left,
                 Format("The zoo keeps %d threatened species. Young born here matter.", threatened));
    }

    Heading(ui, body, "ENCLOSURES");
    const auto& enclosures = zoo.Habitats().All();
    if (enclosures.empty()) {
        Note(ui, body, "None yet: close a ring of fence and it becomes an enclosure.");
        return;
    }
    constexpr float kLine = 30.0f;
    const float offset =
        ui.ScrollBegin("zoo-enclosures", body, kLine * static_cast<float>(enclosures.size()));
    float y = body.y - offset;
    for (const zoo::Enclosure& enclosure : enclosures) {
        const Box row{body.x, y, body.w - 10.0f, kLine - 3.0f};
        y += kLine;
        if (row.Bottom() < body.y || row.y > body.Bottom())
            continue;
        const auto found = tallies.find(enclosure.id.value);
        const Tally tally = found != tallies.end() ? found->second : Tally{};
        if (HotRow(ui, row)) {
            play.Select({.kind = Picked::Enclosure, .enclosure = enclosure.id});
            const zoo::Vec2 a = zoo::World::TileCenter(enclosure.min);
            const zoo::Vec2 b = zoo::World::TileCenter(enclosure.max);
            play.camera->GoTo({0.5f * (a.x + b.x), 0.5f * (a.z + b.z)});
            play.dialog = Dialog::None;
        }
        Box inner = row.Inset(8.0f, 0.0f);
        if (tally.animals > 0) {
            const float welfare = tally.welfare / static_cast<float>(tally.animals);
            const Box bar = inner.CutRight(60.0f);
            ui.Bar({bar.x, std::round(bar.MidY() - 3.5f), bar.w, 7.0f}, welfare, WelfareColor(welfare));
            inner.CutRight(8.0f);
        }
        ui.LabelIn(inner.CutRight(34.0f),
                   tally.animals > 0 ? Format("%d", tally.animals) : std::string("empty"), theme::kSmall,
                   tally.animals > 0 ? theme::kText : theme::kFaint, Align::Right);
        ui.LabelIn(inner, enclosure.name, theme::kBody, theme::kText);
    }
    ui.ScrollEnd();
}

void Hud::AnimalsWindow(ui::Ui& ui, Play& play) {
    zoo::Zoo& zoo = *play.zoo;
    bool closed = false;
    Box body = ui.Window(Centered(ui, 820.0f, 480.0f), "The animals", &closed);
    if (closed)
        play.dialog = Dialog::None;

    {
        Box row = body.CutTop(26.0f);
        ui.LabelIn(row.CutLeft(60.0f), "Order", theme::kSmall, theme::kMuted);
        const int picked =
            Choice(ui, row.CutLeft(420.0f), {"By species", "Worst welfare first", "By name"}, animalSort_);
        if (picked >= 0)
            animalSort_ = picked;
        ui.LabelIn(row, Format("%d animals", zoo.AnimalCount()), theme::kSmall, theme::kMuted, Align::Right);
    }
    body.CutTop(6.0f);

    const auto& species = play.species->All();
    std::vector<const zoo::Animal*> list;
    for (const zoo::Animal& animal : zoo.Fauna().All())
        list.push_back(&animal);
    std::stable_sort(list.begin(), list.end(), [&](const zoo::Animal* a, const zoo::Animal* b) {
        if (animalSort_ == 1)
            return a->welfare < b->welfare;
        if (animalSort_ == 0 && a->species != b->species)
            return species[static_cast<std::size_t>(a->species)].commonName <
                   species[static_cast<std::size_t>(b->species)].commonName;
        return a->name < b->name;
    });

    // Columns: name, species, sex, age, welfare, what it does, where it lives.
    constexpr float kName = 130.0f, kSpecies = 150.0f, kSex = 54.0f, kAge = 54.0f, kWelfare = 118.0f,
                    kDoing = 150.0f;
    {
        Box head = body.CutTop(20.0f).Inset(8.0f, 0.0f);
        const auto title = [&](float width, const char* text) {
            ui.LabelIn(head.CutLeft(width), text, theme::kSmall, theme::kMuted, Align::Left, true);
        };
        title(kName, "NAME");
        title(kSpecies, "SPECIES");
        title(kSex, "SEX");
        title(kAge, "AGE");
        title(kWelfare, "WELFARE");
        title(kDoing, "DOING");
        ui.LabelIn(head, "LIVES IN", theme::kSmall, theme::kMuted, Align::Left, true);
    }
    ui.Separator(body.CutTop(1.0f));
    body.CutTop(3.0f);
    if (list.empty()) {
        Note(ui, body, "The zoo has no animals yet. Open the Animals palette to buy some.");
        return;
    }

    constexpr float kLine = 25.0f;
    const zoo::GameTime now = zoo.Time();
    const float offset = ui.ScrollBegin("animal-list", body, kLine * static_cast<float>(list.size()));
    float y = body.y - offset;
    for (const zoo::Animal* animal : list) {
        const Box row{body.x, y, body.w - 10.0f, kLine - 2.0f};
        y += kLine;
        if (row.Bottom() < body.y || row.y > body.Bottom())
            continue;
        const bool selected = play.selection.kind == Picked::Animal && play.selection.animal == animal->id;
        if (HotRow(ui, row, selected)) {
            play.Select({.kind = Picked::Animal, .animal = animal->id});
            play.camera->GoTo(animal->position);
            play.dialog = Dialog::None;
        }
        Box inner = row.Inset(8.0f, 0.0f);
        ui.LabelIn(inner.CutLeft(kName), animal->name, theme::kBody, theme::kText);
        ui.LabelIn(inner.CutLeft(kSpecies), species[static_cast<std::size_t>(animal->species)].commonName,
                   theme::kSmall + 1.0f, theme::kMuted);
        ui.LabelIn(inner.CutLeft(kSex), animal->sex == zoo::Sex::Female ? "female" : "male", theme::kSmall,
                   theme::kMuted);
        ui.LabelIn(inner.CutLeft(kAge), AgeShort(zoo::AgeYears(*animal, now)), theme::kSmall, theme::kMuted);
        {
            Box cell = inner.CutLeft(kWelfare);
            cell.CutRight(14.0f);
            ui.LabelIn(cell.CutRight(38.0f), Percent(animal->welfare), theme::kSmall, theme::kText,
                       Align::Right);
            cell.CutRight(4.0f);
            ui.Bar({cell.x, std::round(cell.MidY() - 3.5f), cell.w, 7.0f}, animal->welfare,
                   WelfareColor(animal->welfare));
        }
        const bool alarm = animal->loose || animal->activity == zoo::AnimalActivity::BreakOut;
        std::string doing = ActivityText(*animal);
        if (animal->sick)
            doing = "Sick";
        ui.LabelIn(inner.CutLeft(kDoing), doing, theme::kSmall,
                   alarm || animal->sick ? theme::kAlarm : theme::kMuted);
        const zoo::Enclosure* home = zoo.Habitats().Find(zoo.Habitats().At(animal->position));
        ui.LabelIn(inner, home != nullptr ? home->name : std::string("no enclosure"), theme::kSmall,
                   home != nullptr ? theme::kMuted : theme::kAlarm);
    }
    ui.ScrollEnd();
}

void Hud::FinancesWindow(ui::Ui& ui, Play& play) {
    zoo::Zoo& zoo = *play.zoo;
    zoo::Finance& books = zoo.Accounts();
    bool closed = false;
    Box body = ui.Window(Centered(ui, 700.0f, 560.0f), "The books", &closed);
    if (closed)
        play.dialog = Dialog::None;

    // The bank.
    {
        Box row = body.CutTop(30.0f);
        ui.LabelIn(row.CutLeft(90.0f), "In the bank", theme::kBody, theme::kMuted);
        const std::string balance = zoo::FormatMoney(books.Balance());
        ui.LabelIn(row.CutLeft(ui.Paint().TextWidth(balance, 20.0f, true) + 24.0f), balance, 20.0f,
                   books.Balance() < 0 ? theme::kAlarm : theme::kMoney, Align::Left, true);
        const std::string step = zoo::FormatMoney(zoo::Finance::kLoanStep);
        if (ui.Button(row.CutRight(120.0f), Format("Repay %s", step.c_str()),
                      {.enabled = books.Loan() > 0 && books.Balance() > 0, .size = theme::kSmall + 1.0f}))
            books.RepayLoan(zoo::Finance::kLoanStep);
        row.CutRight(kGap);
        if (ui.Button(row.CutRight(120.0f), Format("Borrow %s", step.c_str()),
                      {.enabled = books.Loan() + zoo::Finance::kLoanStep <= zoo::Finance::kLoanLimit,
                       .size = theme::kSmall + 1.0f,
                       .tooltip = "The bank lends up to $100,000 at 0.25 % a day"})) {
            if (!books.TakeLoan(zoo::Finance::kLoanStep))
                play.Flash("The bank lends no more");
        }
        row.CutRight(10.0f);
        ui.LabelIn(row, Format("Loan %s", zoo::FormatMoney(books.Loan()).c_str()), theme::kBody,
                   books.Loan() > 0 ? theme::kWarning : theme::kMuted, Align::Right);
    }
    body.CutTop(8.0f);

    // The last days as bars: profit up, loss down.
    const auto& history = books.History();
    {
        Box chart = body.CutBottom(96.0f);
        Heading(ui, chart, "PROFIT OF THE LAST DAYS");
        const std::size_t shown = std::min<std::size_t>(history.size(), 28);
        if (shown == 0) {
            Note(ui, chart, "The first day is not over yet.");
        } else {
            zoo::Money most = 1;
            for (std::size_t i = history.size() - shown; i < history.size(); ++i)
                most = std::max<zoo::Money>(most, std::abs(history[i].Profit()));
            const float middle = std::round(chart.MidY());
            const float reach = 0.5f * chart.h - 3.0f;
            const float pitch = std::min(22.0f, chart.w / static_cast<float>(shown));
            ui.Paint().Fill({chart.x, middle, chart.w, 1.0f}, theme::kPanelEdge);
            for (std::size_t i = 0; i < shown; ++i) {
                const zoo::DayReport& day = history[history.size() - shown + i];
                const zoo::Money profit = day.Profit();
                const float tall = std::max(1.0f, std::round(reach * static_cast<float>(std::abs(profit)) /
                                                             static_cast<float>(most)));
                const Box bar{std::round(chart.x + pitch * static_cast<float>(i)) + 1.0f,
                              profit >= 0 ? middle - tall : middle + 1.0f, std::max(2.0f, pitch - 3.0f),
                              tall};
                ui.Paint().Fill(bar, profit >= 0 ? theme::kGood : theme::kAlarm);
                ui.Tooltip({bar.x, chart.y, bar.w + 2.0f, chart.h},
                           Format("Day %d: %s%s", static_cast<int>(day.day) + 1, profit >= 0 ? "+" : "-",
                                  zoo::FormatMoney(std::abs(profit)).c_str()));
            }
        }
    }
    body.CutBottom(8.0f);

    // The table: accounts down, periods across.
    const zoo::DayReport& today = books.Today();
    const zoo::DayReport yesterday = history.empty() ? zoo::DayReport{} : history.back();
    const zoo::DayReport week = books.Summary(7);
    const auto& life = books.Lifetime();
    constexpr float kColumn = 122.0f;
    const auto line = [&](Box row, std::string_view label, zoo::Money a, zoo::Money b, zoo::Money c,
                          zoo::Money d, ui::Rgba color, bool bold) {
        row = row.Inset(8.0f, 0.0f);
        const float size = bold ? theme::kSmall + 1.0f : theme::kSmall;
        const zoo::Money cells[4] = {d, c, b, a};
        for (const zoo::Money amount : cells)
            ui.LabelIn(row.CutRight(kColumn), Cell(amount), size, amount == 0 ? theme::kFaint : color,
                       Align::Right, bold);
        ui.LabelIn(row, label, size, bold ? theme::kText : theme::kMuted, Align::Left, bold);
    };
    {
        Box head = body.CutTop(20.0f).Inset(8.0f, 0.0f);
        for (const char* title : {"SINCE OPENING", "LAST 7 DAYS", "YESTERDAY", "TODAY"})
            ui.LabelIn(head.CutRight(kColumn), title, theme::kSmall, theme::kMuted, Align::Right, true);
    }
    ui.Separator(body.CutTop(1.0f));
    body.CutTop(2.0f);

    std::vector<zoo::Account> incomes;
    std::vector<zoo::Account> expenses;
    zoo::Money lifeIncome = 0;
    zoo::Money lifeExpenses = 0;
    for (int i = 0; i < zoo::kAccountCount; ++i) {
        const auto account = static_cast<zoo::Account>(i);
        const auto at = static_cast<std::size_t>(i);
        (zoo::IsIncome(account) ? lifeIncome : lifeExpenses) += life[at];
        // Accounts nothing ever went through stay out of the way.
        if (life[at] == 0 && today.amounts[at] == 0)
            continue;
        (zoo::IsIncome(account) ? incomes : expenses).push_back(account);
    }
    constexpr float kLine = 19.0f;
    const float content = kLine * static_cast<float>(incomes.size() + expenses.size() + 3) + 16.0f;
    const float offset = ui.ScrollBegin("books", body, content);
    Box rows{body.x, body.y - offset, body.w - 10.0f, content};
    const auto account = [&](zoo::Account a, ui::Rgba color) {
        const auto at = static_cast<std::size_t>(a);
        line(rows.CutTop(kLine), zoo::DisplayName(a), today.amounts[at], yesterday.amounts[at],
             week.amounts[at], life[at], color, false);
    };
    for (const zoo::Account a : incomes)
        account(a, theme::kText);
    line(rows.CutTop(kLine), "Income", today.Income(), yesterday.Income(), week.Income(), lifeIncome,
         theme::kGood, true);
    rows.CutTop(8.0f);
    for (const zoo::Account a : expenses)
        account(a, theme::kText);
    line(rows.CutTop(kLine), "Costs", today.Expenses(), yesterday.Expenses(), week.Expenses(), lifeExpenses,
         theme::kWarning, true);
    rows.CutTop(8.0f);
    {
        // Profit can be a loss: its own formatting.
        Box row = rows.CutTop(kLine).Inset(8.0f, 0.0f);
        const zoo::Money cells[4] = {lifeIncome - lifeExpenses, week.Profit(), yesterday.Profit(),
                                     today.Profit()};
        for (const zoo::Money amount : cells)
            ui.LabelIn(row.CutRight(kColumn),
                       amount == 0 ? std::string("·")
                                   : Format("%s%s", amount < 0 ? "-" : "+",
                                            zoo::FormatMoney(std::abs(amount)).c_str()),
                       theme::kSmall + 1.0f,
                       amount == 0  ? theme::kFaint
                       : amount < 0 ? theme::kAlarm
                                    : theme::kGood,
                       Align::Right, true);
        ui.LabelIn(row, "Profit", theme::kSmall + 1.0f, theme::kText, Align::Left, true);
    }
    ui.ScrollEnd();
}

void Hud::MessagesWindow(ui::Ui& ui, Play& play) {
    zoo::Zoo& zoo = *play.zoo;
    bool closed = false;
    Box body = ui.Window(Centered(ui, 600.0f, 460.0f), "Messages", &closed);
    if (closed)
        play.dialog = Dialog::None;
    const auto& all = zoo.Notifications();
    if (all.empty()) {
        Note(ui, body, "Nothing has happened yet.");
        return;
    }
    const float lineHeight = ui.Paint().LineHeight(theme::kSmall + 1.0f);
    const float stampHeight = ui.Paint().LineHeight(theme::kSmall);
    const float width = body.w - 10.0f;
    struct Entry {
        const zoo::Notification* note;
        std::vector<std::string> lines;
        float height;
    };
    std::vector<Entry> entries;
    float content = 0.0f;
    for (auto it = all.rbegin(); it != all.rend(); ++it) {
        Entry entry{&*it, ui.Paint().Wrap(it->text, width - 30.0f, theme::kSmall + 1.0f), 0.0f};
        entry.height = stampHeight + lineHeight * static_cast<float>(entry.lines.size()) + 12.0f;
        content += entry.height + 3.0f;
        entries.push_back(std::move(entry));
    }
    const float offset = ui.ScrollBegin("messages", body, content);
    float y = body.y - offset;
    for (const Entry& entry : entries) {
        const Box row{body.x, y, width, entry.height};
        y += entry.height + 3.0f;
        if (row.Bottom() < body.y || row.y > body.Bottom())
            continue;
        const zoo::Notification& note = *entry.note;
        if (note.located) {
            if (HotRow(ui, row)) {
                play.camera->GoTo(note.position);
                if (note.animal && zoo.Fauna().Find(note.animal) != nullptr)
                    play.Select({.kind = Picked::Animal, .animal = note.animal});
                else if (note.enclosure && zoo.Habitats().Find(note.enclosure) != nullptr)
                    play.Select({.kind = Picked::Enclosure, .enclosure = note.enclosure});
                play.dialog = Dialog::None;
            }
            ui.Tooltip(row, "Click to look there");
        }
        ui.Paint().Round({row.x + 5.0f, row.y + 5.0f, 4.0f, row.h - 10.0f}, 2.0f,
                         SeverityColor(note.severity));
        float lineY = row.y + 5.0f;
        ui.Label(StampText(note.tick), row.x + 17.0f, lineY, theme::kSmall, theme::kFaint);
        lineY += stampHeight;
        for (const std::string& text : entry.lines) {
            ui.Label(text, row.x + 17.0f, lineY, theme::kSmall + 1.0f, theme::kText);
            lineY += lineHeight;
        }
    }
    ui.ScrollEnd();
}

void Hud::HelpWindow(ui::Ui& ui, Play& play) {
    bool closed = false;
    Box body = ui.Window(Middle(ui, 660.0f, 520.0f), "How to play", &closed);
    if (closed)
        play.dialog = play.mainMenu ? Dialog::None : helpFrom_;

    struct Line {
        const char* keys;
        const char* what;
    };
    static constexpr Line kCamera[] = {
        {"W A S D, arrows", "Move over the zoo"},
        {"Right drag", "Slide the map"},
        {"Middle drag, Q E", "Turn the view"},
        {"R F", "Tilt the view"},
        {"Wheel", "Come closer, go further"},
        {"Shift", "Move faster"},
        {"Tab", "Walk through the zoo at eye height, and back"},
    };
    static constexpr Line kGame[] = {
        {"Left click", "Select an animal, an enclosure, a building; use the tool in hand"},
        {"Right click, Esc", "Put the tool down; close; open the menu"},
        {"Z X", "Turn what is about to be placed"},
        {"Shift + drag", "Fence around a rectangle"},
        {"Space", "Pause and resume"},
        {"1 2 3 4", "Pause, normal, fast, very fast"},
        {"F5", "Save quickly"},
    };
    Box left = body.CutLeft(std::round(0.5f * body.w - 10.0f));
    body.CutLeft(20.0f);
    const auto table = [&](Box& column, const char* heading, std::span<const Line> lines) {
        Heading(ui, column, heading);
        for (const Line& line : lines) {
            ui.LabelIn(column.CutTop(18.0f), line.keys, theme::kSmall + 1.0f, theme::kMoney, Align::Left,
                       true);
            const float used = ui.Paragraph(column, line.what, theme::kSmall, theme::kText);
            column.CutTop(used + 6.0f);
        }
    };
    table(left, "LOOKING AROUND", kCamera);
    table(body, "RUNNING THE ZOO", kGame);
    body.CutTop(6.0f);
    Heading(ui, body, "A FIRST ZOO");
    Note(ui, body,
         "Close a ring of fence with a gate in it: that is an enclosure. Give it the ground, the plants, the "
         "water and the shelter its animals come from, a feeder and a trough, then release a group. Click an "
         "animal to see what it misses. Lay paths from the entrance so that visitors can come and look.",
         theme::kText);
}

void Hud::MenuWindow(ui::Ui& ui, Play& play, bool opened) {
    if (opened)
        confirm_ = Request::None;
    bool closed = false;
    Box body = ui.Window(Middle(ui, 300.0f, 348.0f), "Menu", &closed);
    if (closed)
        play.dialog = Dialog::None;
    const auto button = [&](std::string_view text, std::string_view tip = {}, bool bold = false) {
        const Box box = body.CutTop(34.0f);
        body.CutTop(6.0f);
        return ui.Button(box, text, {.bold = bold, .tooltip = tip});
    };
    if (button("Back to the zoo", {}, true))
        play.dialog = Dialog::None;
    if (button("Save the zoo"))
        play.dialog = Dialog::Save;
    if (button("Load a zoo"))
        play.dialog = Dialog::Load;
    if (button("A new zoo"))
        play.dialog = Dialog::NewGame;
    if (button("How to play")) {
        helpFrom_ = Dialog::Menu;
        play.dialog = Dialog::Help;
    }
    // Leaving loses what was not saved: asked twice.
    if (button(confirm_ == Request::MainMenu ? "Leave without saving?" : "To the title screen")) {
        if (confirm_ == Request::MainMenu)
            play.request = Request::MainMenu;
        confirm_ = Request::MainMenu;
    }
    if (button(confirm_ == Request::Quit ? "Quit without saving?" : "Quit")) {
        if (confirm_ == Request::Quit)
            play.request = Request::Quit;
        confirm_ = Request::Quit;
    }
}

void Hud::SaveWindow(ui::Ui& ui, Play& play, bool opened) {
    if (opened) {
        saves_ = ListSaves();
        saveName_ = play.zoo->Name();
        ui.Focus("save-name");
    }
    bool closed = false;
    Box body = ui.Window(Middle(ui, 560.0f, 440.0f), "Save the zoo", &closed);
    if (closed)
        play.dialog = Dialog::Menu;

    Box foot = body.CutBottom(30.0f);
    body.CutBottom(8.0f);
    const std::string safe = SafeTitle(saveName_);
    const bool exists = std::any_of(saves_.begin(), saves_.end(),
                                    [&](const SaveEntry& entry) { return entry.title == safe; });
    bool save = false;
    {
        if (ui.Button(foot.CutRight(120.0f), exists ? "Overwrite" : "Save",
                      {.enabled = !saveName_.empty(), .bold = true}))
            save = true;
        foot.CutRight(8.0f);
        ui.LabelIn(foot.CutLeft(46.0f), "Name", theme::kBody, theme::kMuted);
        if (ui.TextField("save-name", foot, saveName_, 48) && !saveName_.empty())
            save = true;
    }
    if (save) {
        play.request = Request::Save;
        play.requestPath = SavePath(saveName_).string();
        play.dialog = Dialog::None;
    }

    Heading(ui, body, "SAVED ZOOS");
    if (saves_.empty()) {
        Note(ui, body, Format("None yet. They are kept in %s", SaveDirectory().string().c_str()));
        return;
    }
    constexpr float kLine = 30.0f;
    const float offset = ui.ScrollBegin("save-list", body, kLine * static_cast<float>(saves_.size()));
    float y = body.y - offset;
    for (const SaveEntry& entry : saves_) {
        const Box row{body.x, y, body.w - 10.0f, kLine - 3.0f};
        y += kLine;
        if (row.Bottom() < body.y || row.y > body.Bottom())
            continue;
        if (HotRow(ui, row, entry.title == safe))
            saveName_ = entry.title;
        Box inner = row.Inset(8.0f, 0.0f);
        if (entry.readable)
            ui.LabelIn(inner.CutRight(250.0f),
                       Format("day %d · %d animals · %s", static_cast<int>(entry.summary.day) + 1,
                              entry.summary.animals, zoo::FormatMoney(entry.summary.balance).c_str()),
                       theme::kSmall, theme::kMuted, Align::Right);
        ui.LabelIn(inner, entry.title, theme::kBody, theme::kText);
    }
    ui.ScrollEnd();
}

void Hud::LoadWindow(ui::Ui& ui, Play& play, bool opened) {
    if (opened) {
        saves_ = ListSaves();
        savePicked_ = saves_.empty() ? -1 : 0;
    }
    bool closed = false;
    Box body = ui.Window(Middle(ui, 600.0f, 440.0f), "Load a zoo", &closed);
    if (closed)
        play.dialog = play.mainMenu ? Dialog::None : Dialog::Menu;

    Box foot = body.CutBottom(30.0f);
    body.CutBottom(8.0f);
    const bool pickedOk = savePicked_ >= 0 && savePicked_ < static_cast<int>(saves_.size()) &&
                          saves_[static_cast<std::size_t>(savePicked_)].readable;
    bool load = ui.Button(foot.CutRight(120.0f), "Load", {.enabled = pickedOk, .bold = true});
    if (savePicked_ >= 0 && savePicked_ < static_cast<int>(saves_.size())) {
        const SaveEntry& entry = saves_[static_cast<std::size_t>(savePicked_)];
        foot.CutRight(10.0f);
        ui.LabelIn(foot,
                   entry.readable ? Format("\"%s\", written by version %s", entry.summary.name.c_str(),
                                           entry.summary.gameVersion.c_str())
                                  : entry.problem,
                   theme::kSmall, entry.readable ? theme::kMuted : theme::kAlarm);
    }

    if (saves_.empty()) {
        Note(ui, body, Format("No saved zoos yet. They are kept in %s", SaveDirectory().string().c_str()));
        return;
    }
    constexpr float kLine = 30.0f;
    const float offset = ui.ScrollBegin("load-list", body, kLine * static_cast<float>(saves_.size()));
    float y = body.y - offset;
    int index = 0;
    for (const SaveEntry& entry : saves_) {
        const Box row{body.x, y, body.w - 10.0f, kLine - 3.0f};
        y += kLine;
        const int mine = index++;
        if (row.Bottom() < body.y || row.y > body.Bottom())
            continue;
        if (HotRow(ui, row, mine == savePicked_)) {
            // A second click on the chosen one loads it.
            if (savePicked_ == mine && entry.readable)
                load = true;
            savePicked_ = mine;
        }
        Box inner = row.Inset(8.0f, 0.0f);
        ui.LabelIn(inner.CutRight(300.0f),
                   entry.readable
                       ? Format("year %d, day %d · %d animals of %d species · %s", entry.summary.year,
                                static_cast<int>(entry.summary.day) + 1, entry.summary.animals,
                                entry.summary.species, zoo::FormatMoney(entry.summary.balance).c_str())
                       : std::string("cannot be read"),
                   theme::kSmall, entry.readable ? theme::kMuted : theme::kAlarm, Align::Right);
        ui.LabelIn(inner, entry.title, theme::kBody, theme::kText);
    }
    ui.ScrollEnd();
    if (load && savePicked_ >= 0 && savePicked_ < static_cast<int>(saves_.size())) {
        play.request = Request::Load;
        play.requestPath = saves_[static_cast<std::size_t>(savePicked_)].path.string();
    }
}

void Hud::NewGameWindow(ui::Ui& ui, Play& play, bool opened) {
    if (opened) {
        newName_ = "New Zoo";
        newSeed_ = Format("%llu", static_cast<unsigned long long>(FreshSeed()));
    }
    bool closed = false;
    Box body = ui.Window(Middle(ui, 500.0f, 392.0f), "A new zoo", &closed);
    if (closed)
        play.dialog = play.mainMenu ? Dialog::None : Dialog::Menu;

    static constexpr zoo::Money kFunds[] = {zoo::Dollars(25000), zoo::Dollars(50000), zoo::Dollars(100000)};
    const auto field = [&](const char* label) {
        Box row = body.CutTop(28.0f);
        body.CutTop(8.0f);
        ui.LabelIn(row.CutLeft(110.0f), label, theme::kBody, theme::kMuted);
        return row;
    };
    ui.TextField("new-name", field("Name"), newName_, 40);
    {
        const int picked = Choice(ui, field("Begin with"), {"A small zoo", "Bare land"}, newLand_);
        if (picked >= 0)
            newLand_ = picked;
    }
    Note(ui, body,
         newLand_ == 0
             ? "Three enclosures with lions, zebras and wolves, paths and a few stalls: a zoo to learn on "
               "and to grow."
             : "A fenced piece of land with an entrance and nothing else.");
    body.CutTop(8.0f);
    {
        const int picked =
            Choice(ui, field("Money"), {"Tight", "Comfortable", "Plenty", "Sandbox"}, newFunds_);
        if (picked >= 0)
            newFunds_ = picked;
    }
    Note(ui, body,
         newFunds_ == 3
             ? std::string("Nothing costs anything: build freely.")
             : Format("The zoo opens with %s in the bank.", zoo::FormatMoney(kFunds[newFunds_]).c_str()));
    body.CutTop(8.0f);
    {
        Box row = field("Land number");
        // Digits only.
        ui.TextField("new-seed", row.CutLeft(120.0f), newSeed_, 9);
        std::erase_if(newSeed_, [](char c) { return c < '0' || c > '9'; });
        row.CutLeft(10.0f);
        ui.LabelIn(row, "the same number gives the same land", theme::kSmall, theme::kFaint);
    }

    Box foot = body.CutBottom(32.0f);
    if (ui.Button(foot.CutRight(150.0f), "Open the zoo", {.enabled = !newName_.empty(), .bold = true})) {
        zoo::ZooConfig config;
        config.name = newName_;
        const std::uint64_t seed =
            newSeed_.empty() ? FreshSeed() : std::strtoull(newSeed_.c_str(), nullptr, 10);
        config.seed = seed;
        config.world.seed = seed;
        config.sandbox = newFunds_ == 3;
        config.startingMoney = kFunds[std::min(newFunds_, 2)];
        play.newGame = config;
        play.newGameStarter = newLand_ == 0;
        play.request = Request::NewGame;
    }
}

void Hud::TitleScreen(ui::Ui& ui, Play& play) {
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
    ui.LabelIn(body.CutTop(20.0f), "Build a zoo its animals can live in.", theme::kBody, theme::kMuted);
    body.CutTop(34.0f);

    const auto button = [&](std::string_view text, bool bold = false, bool enabled = true) {
        const Box box = body.CutTop(40.0f);
        body.CutTop(8.0f);
        return ui.Button(box, text, {.enabled = enabled, .bold = bold, .size = theme::kBody + 1.0f});
    };
    if (play.started) {
        if (button("Back to the zoo", true))
            play.mainMenu = false;
    }
    if (button("A new zoo", !play.started))
        play.dialog = Dialog::NewGame;
    if (button("Load a zoo"))
        play.dialog = Dialog::Load;
    if (button("How to play")) {
        helpFrom_ = Dialog::None;
        play.dialog = Dialog::Help;
    }
    if (button("Quit"))
        play.request = Request::Quit;

    ui.LabelIn({body.x, screen.h - 40.0f, body.w, 20.0f}, Format("Version %s", zoo::VersionString()),
               theme::kSmall, theme::kFaint);
}

}  // namespace zoogame
