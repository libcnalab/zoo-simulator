// SPDX-License-Identifier: MIT
// The inspector: what the selected animal, enclosure, building or object is and needs.
#include <algorithm>
#include <cmath>
#include <map>
#include <string>
#include <vector>

#include "HudWidgets.hpp"

namespace zoogame {

namespace {

using hud::Fact;
using hud::Heading;
using hud::kGap;
using hud::Meter;
using hud::Note;
using ui::Align;
using ui::Box;
namespace theme = ui::theme;

constexpr float kInspectorWidth = 300.0f;

std::string AgeText(float years) {
    if (years < 1.0f) {
        const int months = std::max(1, static_cast<int>(std::lround(years * 12.0f)));
        return Format("%d month%s", months, months == 1 ? "" : "s");
    }
    if (years < 10.0f)
        return Format("%.1f years", static_cast<double>(years));
    return Format("%d years", static_cast<int>(std::lround(years)));
}

/// Two or three buttons side by side at the foot of a panel.
Box ButtonSlot(Box& row, int left) {
    const float width = std::round((row.w - kGap * static_cast<float>(left - 1)) / static_cast<float>(left));
    const Box slot = row.CutLeft(width);
    row.CutLeft(kGap);
    return slot;
}

}  // namespace

std::string ActivityText(const zoo::Animal& animal) {
    using zoo::AnimalActivity;
    if (animal.shelter)
        return animal.activity == AnimalActivity::Sleep ? "Asleep in the shelter" : "In the shelter";
    if (animal.moving) {
        switch (animal.activity) {
            case AnimalActivity::Eat:
                return "Going to feed";
            case AnimalActivity::Drink:
                return "Going to drink";
            case AnimalActivity::Sleep:
            case AnimalActivity::Rest:
                return "Looking for a place to rest";
            case AnimalActivity::Shelter:
                return "Heading for shelter";
            case AnimalActivity::Play:
                return "Off to play";
            case AnimalActivity::Socialize:
                return "Joining its companions";
            case AnimalActivity::Browse:
            case AnimalActivity::FilterFeed:
                return "Going to feed";
            case AnimalActivity::Bathe:
                return "Going to the water";
            case AnimalActivity::Sentinel:
                return "Taking up its lookout";
            case AnimalActivity::Huddle:
                return "Joining the flock";
            default:
                break;
        }
    }
    switch (animal.activity) {
        case AnimalActivity::Idle:
            return "Looking around";
        case AnimalActivity::Wander:
            return "Strolling";
        case AnimalActivity::Forage:
            return "Foraging";
        case AnimalActivity::Eat:
            return "Eating";
        case AnimalActivity::Drink:
            return "Drinking";
        case AnimalActivity::Rest:
            return "Resting";
        case AnimalActivity::Sleep:
            return "Asleep";
        case AnimalActivity::Play:
            return "Playing";
        case AnimalActivity::Socialize:
            return "With its companions";
        case AnimalActivity::Vocalize:
            return "Calling";
        case AnimalActivity::Pace:
            return "Pacing along the fence";
        case AnimalActivity::Shelter:
            return "Sheltering";
        case AnimalActivity::BreakOut:
            return "Trying to break out";
        case AnimalActivity::Loose:
            return "On the loose";
        case AnimalActivity::Crated:
            return "In a transport crate";
        case AnimalActivity::Browse:
            return "Browsing";
        case AnimalActivity::Ruminate:
            return "Chewing the cud";
        case AnimalActivity::Bathe:
            return "Bathing";
        case AnimalActivity::DustBathe:
            return "Dust bathing";
        case AnimalActivity::Preen:
            return "Preening";
        case AnimalActivity::Sentinel:
            return "On sentry duty";
        case AnimalActivity::Dig:
            return "Digging";
        case AnimalActivity::FilterFeed:
            return "Sieving the shallows";
        case AnimalActivity::Patrol:
            return "Patrolling its territory";
        case AnimalActivity::Huddle:
            return "Standing with the flock";
        case AnimalActivity::Count:
            break;
    }
    return "";
}

void Hud::Inspector(ui::Ui& ui, Play& play) {
    if (play.selection.kind == Picked::None)
        return;
    const float top = hud::kBarHeight + hud::kMargin;
    const float most = ui.Screen().Y - top - hud::kToolbarHeight - 8.0f - hud::kMargin;
    const float x = ui.Screen().X - kInspectorWidth - hud::kMargin;
    zoo::Zoo& zoo = *play.zoo;
    bool closed = false;
    switch (play.selection.kind) {
        case Picked::None:
            break;
        case Picked::Animal: {
            const zoo::Animal* animal = zoo.Fauna().Find(play.selection.animal);
            if (animal == nullptr) {
                // Sold, dead or gone.
                play.selection.Clear();
                play.follow = false;
                return;
            }
            const Box body =
                ui.Window({x, top, kInspectorWidth, std::min(most, 560.0f)}, animal->name, &closed);
            AnimalInspector(ui, play, body);
            break;
        }
        case Picked::Enclosure: {
            const zoo::Enclosure* enclosure = zoo.Habitats().Find(play.selection.enclosure);
            if (enclosure == nullptr) {
                play.selection.Clear();
                return;
            }
            const Box body =
                ui.Window({x, top, kInspectorWidth, std::min(most, 560.0f)}, enclosure->name, &closed);
            EnclosureInspector(ui, play, body);
            break;
        }
        case Picked::Building: {
            const zoo::Building* building = zoo.Objects().FindBuilding(play.selection.building);
            if (building == nullptr) {
                play.selection.Clear();
                return;
            }
            const zoo::BuildingDef& def = play.catalog->buildings[static_cast<std::size_t>(building->def)];
            const Box body = ui.Window({x, top, kInspectorWidth, std::min(most, 330.0f)}, def.name, &closed);
            BuildingInspector(ui, play, body);
            break;
        }
        case Picked::Prop: {
            const zoo::Prop* prop = zoo.Objects().FindProp(play.selection.prop);
            if (prop == nullptr) {
                play.selection.Clear();
                return;
            }
            const zoo::PropDef& def = play.catalog->props[static_cast<std::size_t>(prop->def)];
            const Box body = ui.Window({x, top, kInspectorWidth, std::min(most, 250.0f)}, def.name, &closed);
            PropInspector(ui, play, body);
            break;
        }
    }
    if (closed) {
        play.selection.Clear();
        play.follow = false;
    }
}

void Hud::AnimalInspector(ui::Ui& ui, Play& play, Box body) {
    zoo::Zoo& zoo = *play.zoo;
    const zoo::AnimalId id = play.selection.animal;
    const zoo::Animal& animal = *zoo.Fauna().Find(id);
    const zoo::SpeciesDef& species = play.species->All()[static_cast<std::size_t>(animal.species)];
    const zoo::GameTime now = zoo.Time();
    zoo::Env env = zoo.MakeEnv();
    const zoo::WelfareReport report = zoo.Fauna().Welfare(env, animal);

    ui.LabelIn(
        body.CutTop(19.0f),
        Format("%s · %s · %s", species.commonName.c_str(), animal.sex == zoo::Sex::Female ? "female" : "male",
               AgeText(zoo::AgeYears(animal, now)).c_str()),
        theme::kSmall + 1.0f, theme::kText);
    ui.LabelIn(body.CutTop(17.0f), species.scientificName, theme::kSmall, theme::kFaint);
    body.CutTop(3.0f);

    // What it is doing, and what is the matter.
    std::string doing = ActivityText(animal);
    if (animal.Pregnant())
        doing += " · expecting young";
    const bool alarm = animal.loose || animal.activity == zoo::AnimalActivity::BreakOut;
    ui.LabelIn(body.CutTop(20.0f), doing, theme::kBody, alarm ? theme::kAlarm : theme::kInfo);
    if (animal.sick)
        ui.LabelIn(body.CutTop(18.0f),
                   animal.treated ? "Sick, recovering after treatment" : "Sick: needs a vet",
                   theme::kSmall + 1.0f, animal.treated ? theme::kWarning : theme::kAlarm);
    body.CutTop(4.0f);

    // Welfare, large.
    {
        Box row = body.CutTop(24.0f);
        ui.LabelIn(row.CutLeft(70.0f), "Welfare", theme::kBody, theme::kText, Align::Left, true);
        ui.LabelIn(row.CutRight(48.0f), Percent(report.overall), theme::kBody, WelfareColor(report.overall),
                   Align::Right, true);
        row.CutRight(6.0f);
        ui.Bar({row.x, std::round(row.MidY() - 5.0f), row.w, 10.0f}, report.overall,
               WelfareColor(report.overall));
    }
    body.CutTop(4.0f);

    Heading(ui, body, "ITS STATE");
    const auto state = [&](std::string_view label, float need, std::string_view tip) {
        const float good = 1.0f - std::clamp(need, 0.0f, 1.0f);
        Meter(ui, body, label, good, WelfareColor(good), tip);
    };
    state("Fed", animal.hunger, "How well fed it is");
    state("Watered", animal.thirst, "How well watered it is");
    state("Rested", animal.fatigue, "How rested it is");
    state("Occupied", animal.boredom, "How much it has to do");
    state("Calm", animal.stress, "Crowds, noise and danger wear on it");
    Meter(ui, body, "Health", animal.health, WelfareColor(animal.health), "Its physical condition");
    body.CutTop(4.0f);

    Heading(ui, body, "ITS HOME");
    {
        // Two columns of what the enclosure gives it.
        struct Aspect {
            const char* label;
            float value;
        };
        const Aspect aspects[] = {{"Space", report.space},           {"Ground", report.terrain},
                                  {"Water", report.water},           {"Plants", report.vegetation},
                                  {"Shelter", report.shelter},       {"Company", report.social},
                                  {"Enrichment", report.enrichment}, {"Climate", report.comfort}};
        for (std::size_t i = 0; i < std::size(aspects); i += 2) {
            Box row = body.CutTop(19.0f);
            Box left = row.CutLeft(std::round(0.5f * row.w - 5.0f));
            row.CutLeft(10.0f);
            hud::SmallMeter(ui, left, aspects[i].label, aspects[i].value, WelfareColor(aspects[i].value));
            hud::SmallMeter(ui, row, aspects[i + 1].label, aspects[i + 1].value,
                            WelfareColor(aspects[i + 1].value));
        }
    }
    const zoo::Enclosure* home = zoo.Habitats().Find(zoo.Habitats().At(animal.position));
    {
        Box row = body.CutTop(24.0f);
        ui.LabelIn(row.CutLeft(56.0f), "Lives in", theme::kSmall, theme::kMuted);
        if (home != nullptr) {
            if (ui.Button(row, home->name, {.size = theme::kSmall + 1.0f, .tooltip = "Show the enclosure"}))
                play.Select({.kind = Picked::Enclosure, .enclosure = home->id});
        } else {
            ui.LabelIn(row, "no enclosure", theme::kSmall + 1.0f, theme::kAlarm);
        }
    }

    // The foot: what can be done with it.
    Box foot = body.CutBottom(60.0f);
    {
        Box row = foot.CutTop(28.0f);
        if (ui.Button(ButtonSlot(row, 3), play.follow ? "Stop" : "Follow",
                      {.selected = play.follow, .tooltip = "The camera stays with the animal"}))
            play.follow = !play.follow;
        if (ui.Button(ButtonSlot(row, 2), "Move", {.tooltip = "Carry it to another place"})) {
            play.palette = Palette::None;
            play.tool.moving = id;
            play.Use(ToolKind::MoveAnimal);
        }
        const zoo::Money value = zoo.Fauna().Value(env, animal);
        if (ui.Button(row, Format("Sell %s", zoo::FormatMoney(value).c_str()),
                      {.tooltip = "Sell it to another zoo"})) {
            const std::string name = animal.name;  // the animal is gone after the sale
            const zoo::Money got = zoo.SellAnimal(id);
            if (got >= 0)
                play.Tell(Format("%s was sold for %s", name.c_str(), zoo::FormatMoney(got).c_str()));
            play.selection.Clear();
            play.follow = false;
            return;
        }
    }
    foot.CutTop(4.0f);
    {
        Box row = foot.CutTop(28.0f);
        if (animal.loose) {
            if (ui.Button(row, "Recapture", {.bold = true, .tooltip = "Catch it and bring it home"})) {
                const zoo::ActionError error = zoo.RecaptureAnimal(id);
                if (error != zoo::ActionError::None)
                    play.Flash(std::string(zoo::Describe(error)));
            }
        } else if (animal.sick && !animal.treated) {
            if (ui.Button(row, Format("Call the vet, %s", zoo::FormatMoney(zoo::Zoo::kTreatmentFee).c_str()),
                          {.bold = true})) {
                const zoo::ActionError error = zoo.TreatAnimal(id);
                if (error != zoo::ActionError::None)
                    play.Flash(std::string(zoo::Describe(error)));
            }
        } else {
            zoo::Animal* own = zoo.Fauna().Find(id);
            if (ui.Button(
                    row, own->breeding ? "May breed" : "May not breed",
                    {.selected = own->breeding, .tooltip = "Whether this animal is allowed to have young"}))
                own->breeding = !own->breeding;
        }
    }

    // Between: what is wrong, in words.
    body.CutBottom(4.0f);
    body.CutTop(2.0f);
    if (report.issues == 0) {
        Note(ui, body, "Nothing troubles it.", theme::kGood);
        return;
    }
    float contentHeight = 0.0f;
    std::vector<std::vector<std::string>> texts;
    for (int i = 0; i < static_cast<int>(zoo::WelfareIssue::Count); ++i) {
        const auto issue = static_cast<zoo::WelfareIssue>(i);
        if (!report.Has(issue))
            continue;
        texts.push_back(ui.Paint().Wrap(zoo::Describe(issue), body.w - 26.0f, theme::kSmall));
        contentHeight +=
            static_cast<float>(texts.back().size()) * ui.Paint().LineHeight(theme::kSmall) + 4.0f;
    }
    const float offset = ui.ScrollBegin("animal-issues", body, contentHeight);
    float y = body.y - offset;
    for (const auto& lines : texts) {
        ui.Paint().Round({body.x + 2.0f, y + 5.0f, 5.0f, 5.0f}, 2.5f, theme::kWarning);
        for (const std::string& line : lines) {
            ui.Label(line, body.x + 14.0f, y, theme::kSmall, theme::kText);
            y += ui.Paint().LineHeight(theme::kSmall);
        }
        y += 4.0f;
    }
    ui.ScrollEnd();
}

void Hud::EnclosureInspector(ui::Ui& ui, Play& play, Box body) {
    zoo::Zoo& zoo = *play.zoo;
    const zoo::Enclosure& enclosure = *zoo.Habitats().Find(play.selection.enclosure);
    const zoo::EnclosureStats& stats = enclosure.stats;
    zoo::Env env = zoo.MakeEnv();

    // Its name, to be changed by typing.
    if (!ui.Typing() || nameEditFor_ != enclosure.id.value) {
        nameEdit_ = enclosure.name;
        nameEditFor_ = enclosure.id.value;
    }
    {
        Box row = body.CutTop(26.0f);
        ui.LabelIn(row.CutLeft(46.0f), "Name", theme::kSmall, theme::kMuted);
        ui.TextField("enclosure-name", row, nameEdit_, 40);
        if (!nameEdit_.empty() && nameEdit_ != enclosure.name)
            zoo.RenameEnclosure(enclosure.id, nameEdit_);
    }
    body.CutTop(6.0f);

    // Who lives here.
    std::vector<zoo::AnimalId> ids;
    zoo.Fauna().InEnclosure(env, enclosure.id, ids);
    std::map<int, int> perSpecies;
    float welfare = 0.0f;
    std::uint32_t issues = 0;
    for (const zoo::AnimalId id : ids) {
        const zoo::Animal& animal = *zoo.Fauna().Find(id);
        ++perSpecies[animal.species];
        welfare += animal.welfare;
        issues |= animal.issues;
    }
    Heading(ui, body, "ANIMALS");
    if (ids.empty()) {
        Note(ui, body,
             "Nobody lives here yet. Buy animals from the Animals palette and release them inside.");
    } else {
        welfare /= static_cast<float>(ids.size());
        for (const auto& [speciesIndex, count] : perSpecies) {
            const zoo::SpeciesDef& species = play.species->All()[static_cast<std::size_t>(speciesIndex)];
            Fact(ui, body, species.commonName, Format("%d", count));
        }
        Meter(ui, body, "Welfare", welfare, WelfareColor(welfare), "Mean welfare of the animals living here");
    }
    body.CutTop(4.0f);

    Heading(ui, body, "THE ENCLOSURE");
    Fact(ui, body, "Area", Format("%d m²", static_cast<int>(std::lround(stats.areaM2))));
    Fact(ui, body, "Water",
         Format("%s%s", Percent(stats.waterShare).c_str(), stats.deepWater ? ", deep" : ""));
    Fact(ui, body, "Plant cover", Percent(stats.vegetationDensity));
    Fact(ui, body, "Shelter for",
         Format("%d animal%s%s", stats.shelterCapacity, stats.shelterCapacity == 1 ? "" : "s",
                stats.heatedShelter   ? ", heated"
                : stats.cooledShelter ? ", cooled"
                                      : ""),
         stats.shelterCapacity < static_cast<int>(ids.size()) ? theme::kWarning : theme::kText);
    Fact(ui, body, "Feeders, troughs", Format("%d, %d", stats.feeders, stats.troughs),
         !ids.empty() && stats.feeders == 0 ? theme::kWarning : theme::kText);
    Fact(ui, body, "Enrichment", Format("%.1f", static_cast<double>(stats.enrichment)));
    const float dirt = zoo.Fauna().Dirt(enclosure.id);
    Meter(ui, body, "Cleanliness", 1.0f - dirt, WelfareColor(1.0f - dirt));
    body.CutTop(4.0f);

    Heading(ui, body, "THE BARRIER");
    Fact(ui, body, "Lowest fence",
         Format("%.1f m, strength %d", static_cast<double>(stats.minFenceHeightM), stats.minFenceStrength));
    Fact(ui, body, "Condition", Percent(stats.minFenceCondition),
         stats.minFenceCondition < 0.5f ? theme::kAlarm : theme::kText);
    Fact(ui, body, "Gates", Format("%d", stats.gates), stats.gates == 0 ? theme::kAlarm : theme::kText);
    Fact(ui, body, "Visitors watch from", Format("%d places", stats.viewingTiles),
         stats.viewingTiles == 0 ? theme::kWarning : theme::kText);
    for (const auto& [speciesIndex, count] : perSpecies) {
        const zoo::SpeciesDef& species = play.species->All()[static_cast<std::size_t>(speciesIndex)];
        const float holds = zoo::Containment(species, stats);
        if (holds < 0.999f)
            Note(ui, body,
                 Format("This barrier will not hold a %s for long: it needs %.1f m and strength %d.",
                        species.commonName.c_str(), static_cast<double>(species.fenceHeightM),
                        species.fenceStrength),
                 theme::kAlarm);
    }
    if (stats.gates == 0)
        Note(ui, body, "Without a gate the keepers cannot get in to feed and clean.", theme::kAlarm);

    // The foot.
    Box foot = body.CutBottom(28.0f);
    if (ui.Button(ButtonSlot(foot, 2), "Fill feeders",
                  {.enabled = stats.feeders + stats.troughs > 0,
                   .tooltip = "Fill every feeder and trough here now (the food is paid for)"})) {
        zoo::Money paid = 0;
        for (const zoo::PropId feeder : enclosure.feeders)
            paid += zoo.RefillFeeder(feeder);
        for (const zoo::PropId trough : enclosure.troughs)
            zoo.RefillTrough(trough);
        play.Tell(paid > 0 ? Format("Feeders filled for %s", zoo::FormatMoney(paid).c_str())
                           : std::string("Feeders and troughs are full"));
    }
    if (ui.Button(foot, "Go there", {.tooltip = "Move the camera to the enclosure"})) {
        const zoo::Vec2 a = zoo::World::TileCenter(enclosure.min);
        const zoo::Vec2 b = zoo::World::TileCenter(enclosure.max);
        play.camera->GoTo({0.5f * (a.x + b.x), 0.5f * (a.z + b.z)});
    }

    // Between: what the animals living here miss.
    body.CutBottom(4.0f);
    for (int i = 0; i < static_cast<int>(zoo::WelfareIssue::Count) && body.h > 30.0f; ++i) {
        const auto issue = static_cast<zoo::WelfareIssue>(i);
        // The animals' own passing states say nothing about the enclosure.
        if ((issues & zoo::Bit(issue)) == 0 || issue == zoo::WelfareIssue::Hungry ||
            issue == zoo::WelfareIssue::Thirsty || issue == zoo::WelfareIssue::Sick ||
            issue == zoo::WelfareIssue::Escaped)
            continue;
        Note(ui, body, zoo::Describe(issue), theme::kWarning);
    }
}

void Hud::BuildingInspector(ui::Ui& ui, Play& play, Box body) {
    zoo::Zoo& zoo = *play.zoo;
    zoo::Building& building = *zoo.Objects().FindBuilding(play.selection.building);
    const zoo::BuildingDef& def = play.catalog->buildings[static_cast<std::size_t>(building.def)];
    Note(ui, body, def.description);
    body.CutTop(4.0f);
    Fact(ui, body, "Condition", Percent(building.condition),
         building.condition < 0.5f ? theme::kWarning : theme::kText);
    Fact(ui, body, "Upkeep a day", zoo::FormatMoney(def.upkeepPerDay));
    if (def.role == zoo::BuildingRole::AnimalShelter) {
        Fact(ui, body, "Shelters",
             Format("%d animal%s%s", def.capacity, def.capacity == 1 ? "" : "s",
                    def.heated   ? ", heated"
                    : def.cooled ? ", cooled"
                                 : ""));
        const zoo::Enclosure* home =
            zoo.Habitats().Find(zoo.Habitats().At(zoo::FrontTile(def, building.origin, building.facing)));
        Fact(ui, body, "Opens into", home != nullptr ? home->name : std::string("no enclosure"),
             home != nullptr ? theme::kText : theme::kWarning);
    } else {
        Fact(ui, body, "Served today", Format("%d", building.servedToday));
        Fact(ui, body, "Served in all", Format("%d", building.servedTotal));
    }
    if (!def.item.empty()) {
        body.CutTop(6.0f);
        Box row = body.CutTop(26.0f);
        ui.LabelIn(row.CutLeft(110.0f), Format("%s costs", def.item.c_str()), theme::kSmall + 1.0f,
                   theme::kMuted);
        const zoo::Money step = def.itemPrice >= zoo::Dollars(5) ? zoo::Dollars(1) : zoo::Money{50};
        if (ui.Button(row.CutRight(28.0f), "+", {.bold = true, .tooltip = "Dearer"}))
            building.itemPrice = std::min(building.itemPrice + step, def.itemPrice * 4);
        row.CutRight(kGap);
        if (ui.Button(row.CutRight(28.0f), "-", {.bold = true, .tooltip = "Cheaper"}))
            building.itemPrice = std::max<zoo::Money>(building.itemPrice - step, 0);
        row.CutRight(8.0f);
        ui.LabelIn(row, zoo::FormatMoney(building.itemPrice), theme::kBody, theme::kMoney, Align::Right,
                   true);
    }

    Box foot = body.CutBottom(28.0f);
    if (def.role != zoo::BuildingRole::AnimalShelter) {
        if (ui.Button(ButtonSlot(foot, 2), building.open ? "Open" : "Closed",
                      {.selected = building.open, .tooltip = "A closed building serves nobody"}))
            building.open = !building.open;
    }
    const zoo::BuildPlan plan = zoo.Build().PlanDemolish(building.id);
    if (ui.Button(foot,
                  plan.cost < 0 ? Format("Demolish, %s back", zoo::FormatMoney(-plan.cost).c_str())
                                : std::string("Demolish"),
                  {.enabled = plan.Ok()})) {
        const zoo::BuildPlan done = zoo.Build().Demolish(building.id);
        if (!done.Ok())
            play.Flash(std::string(zoo::Describe(done.error)));
        play.selection.Clear();
    }
}

void Hud::PropInspector(ui::Ui& ui, Play& play, Box body) {
    zoo::Zoo& zoo = *play.zoo;
    const zoo::Prop& prop = *zoo.Objects().FindProp(play.selection.prop);
    const zoo::PropDef& def = play.catalog->props[static_cast<std::size_t>(prop.def)];
    zoo::Env env = zoo.MakeEnv();
    const zoo::Enclosure* home = zoo.Habitats().Find(zoo.Habitats().At(prop.position));
    Fact(ui, body, "Stands in", home != nullptr ? home->name : std::string("the public area"));

    Box foot = body.CutBottom(28.0f);
    if (def.role == zoo::PropRole::Feeder) {
        const bool holds = prop.food >= 0 && prop.amount > 0.0f;
        Fact(ui, body, "Holds",
             holds ? Format("%.0f of %.0f kg of %s", static_cast<double>(prop.amount),
                            static_cast<double>(def.capacity),
                            play.catalog->foods[static_cast<std::size_t>(prop.food)].name.c_str())
                   : std::string("nothing"),
             holds ? theme::kText : theme::kWarning);
        if (holds && zoo::Animals::EdibleKg(env, prop) <= 0.0f)
            Note(ui, body, "The food in it has spoiled.", theme::kAlarm);
        const int wanted = zoo.Fauna().FoodFor(env, prop);
        if (wanted < 0)
            Note(ui, body, "No animal living around it eats what this feeder takes.", theme::kWarning);
        if (ui.Button(ButtonSlot(foot, 2), "Fill it", {.enabled = wanted >= 0})) {
            const zoo::Money paid = zoo.RefillFeeder(prop.id);
            play.Tell(paid > 0 ? Format("Filled for %s", zoo::FormatMoney(paid).c_str())
                               : std::string("It is full"));
        }
    } else if (def.role == zoo::PropRole::WaterTrough) {
        Fact(ui, body, "Holds",
             Format("%.0f of %.0f litres", static_cast<double>(prop.amount),
                    static_cast<double>(def.capacity)),
             prop.amount > 0.0f ? theme::kText : theme::kWarning);
        if (ui.Button(ButtonSlot(foot, 2), "Fill it"))
            zoo.RefillTrough(prop.id);
    } else {
        if (def.shadeM2 > 0.0f)
            Fact(ui, body, "Shade", Format("%d m²", static_cast<int>(std::lround(def.shadeM2))));
        if (def.enrichment > 0.0f)
            Fact(ui, body, "Enrichment", Format("%.1f", static_cast<double>(def.enrichment)));
        if (def.beauty > 0.0f)
            Fact(ui, body, "Beauty", Format("%.1f", static_cast<double>(def.beauty)));
    }
    const zoo::BuildPlan plan = zoo.Build().PlanRemoveProp(prop.id);
    if (ui.Button(foot,
                  plan.cost < 0 ? Format("Remove, %s back", zoo::FormatMoney(-plan.cost).c_str())
                                : std::string("Remove"),
                  {.enabled = plan.Ok()})) {
        zoo.Build().RemoveProp(prop.id);
        play.selection.Clear();
    }
}

}  // namespace zoogame
