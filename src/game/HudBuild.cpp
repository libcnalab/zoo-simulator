// SPDX-License-Identifier: MIT
// The toolbar along the bottom and the palettes of things to build.
#include <algorithm>
#include <cmath>
#include <string>

#include "Hud.hpp"

namespace zoogame {

namespace {

using ui::Align;
using ui::Box;
namespace theme = ui::theme;

constexpr float kPaletteWidth = 272.0f;
constexpr float kRow = 28.0f;  ///< a row of a list, with its gap
constexpr float kGap = 3.0f;

/// A row of a list: a name on the left, a figure on the right.
bool ItemRow(ui::Ui& ui, const Box& box, std::string_view name, std::string_view figure, bool selected,
             std::string_view tooltip = {}) {
    const bool clicked = ui.Button(box, "", {.selected = selected, .tooltip = tooltip});
    Box inner = box.Inset(9.0f, 0.0f);
    if (!figure.empty()) {
        const float width = ui.Paint().TextWidth(figure, theme::kSmall);
        ui.LabelIn(inner.CutRight(width + 2.0f), figure, theme::kSmall,
                   selected ? theme::kText : theme::kMoney, Align::Right);
        inner.CutRight(6.0f);
    }
    ui.LabelIn(inner, name, theme::kBody, theme::kText);
    return clicked;
}

/// A row of equal buttons, one of which is chosen; returns the one clicked or -1.
int Choice(ui::Ui& ui, Box row, std::initializer_list<std::string_view> labels, int chosen) {
    const float count = static_cast<float>(labels.size());
    const float width = (row.w - kGap * (count - 1.0f)) / count;
    int clicked = -1;
    int index = 0;
    for (const std::string_view label : labels) {
        const Box box{std::round(row.x + (width + kGap) * static_cast<float>(index)), row.y,
                      std::round(width), row.h};
        if (ui.Button(box, label, {.selected = index == chosen, .size = theme::kSmall + 1.0f}))
            clicked = index;
        ++index;
    }
    return clicked;
}

void Heading(ui::Ui& ui, Box& body, std::string_view text) {
    ui.LabelIn(body.CutTop(20.0f), text, theme::kSmall, theme::kMuted, Align::Left, true);
    body.CutTop(2.0f);
}

void Note(ui::Ui& ui, Box& body, std::string_view text) {
    body.CutTop(4.0f);
    const float used = ui.Paragraph(body, text, theme::kSmall, theme::kMuted);
    body.CutTop(used + 2.0f);
}

void Fact(ui::Ui& ui, Box& body, std::string_view label, std::string_view value) {
    const Box row = body.CutTop(19.0f);
    ui.LabelIn(row, label, theme::kSmall, theme::kMuted);
    Box right = row;
    right.CutLeft(std::min(row.w * 0.42f, ui.Paint().TextWidth(label, theme::kSmall) + 10.0f));
    ui.LabelIn(right, value, theme::kSmall, theme::kText, Align::Right);
}

std::string TerrainName(const zoo::Catalog& catalog, zoo::Terrain terrain) {
    return catalog.TerrainInfo(terrain).name;
}

std::string_view Where(zoo::Placement placement) {
    switch (placement) {
        case zoo::Placement::Anywhere:
            return "anywhere";
        case zoo::Placement::Enclosure:
            return "inside an enclosure";
        case zoo::Placement::Public:
            return "on public ground";
    }
    return "";
}

/// The tab of the scenery palette a prop belongs under.
int SceneryTab(zoo::PropRole role) {
    switch (role) {
        case zoo::PropRole::Tree:
        case zoo::PropRole::Bush:
            return 0;
        case zoo::PropRole::Rock:
            return 1;
        case zoo::PropRole::Feeder:
        case zoo::PropRole::WaterTrough:
        case zoo::PropRole::Enrichment:
            return 2;
        case zoo::PropRole::Bench:
        case zoo::PropRole::Bin:
        case zoo::PropRole::Lamp:
        case zoo::PropRole::InfoBoard:
        case zoo::PropRole::Decoration:
        case zoo::PropRole::Count:
            break;
    }
    return 3;
}

std::string Metres(float value) {
    return std::abs(value - std::round(value)) < 0.05f ? Format("%d m", static_cast<int>(std::lround(value)))
                                                       : Format("%.1f m", static_cast<double>(value));
}

}  // namespace

void OpenPalette(Play& play, Palette palette) {
    play.palette = palette;
    switch (palette) {
        case Palette::None:
            play.Use(ToolKind::Inspect);
            break;
        case Palette::Paths:
            play.Use(ToolKind::Path);
            break;
        case Palette::Fences:
            play.Use(ToolKind::Fence);
            break;
        case Palette::Terrain:
            play.Use(ToolKind::Terrain);
            break;
        case Palette::Water:
            play.Use(ToolKind::Water);
            break;
        case Palette::Buildings:
            play.Use(ToolKind::Building);
            break;
        case Palette::Scenery:
            play.Use(ToolKind::Prop);
            break;
        case Palette::Animals:
            play.Use(ToolKind::Animal);
            break;
    }
}

void Hud::Toolbar(ui::Ui& ui, Play& play) {
    struct Entry {
        const char* label;
        const char* tip;
        Palette palette = Palette::None;
        Dialog dialog = Dialog::None;
        bool bulldoze = false;
        bool gapBefore = false;
    };
    static const Entry kEntries[] = {
        {"Paths", "Lay the paths visitors walk on", Palette::Paths},
        {"Fences", "Fence in enclosures; add gates", Palette::Fences},
        {"Ground", "Paint the ground cover; shape the land", Palette::Terrain},
        {"Water", "Dig pools and moats", Palette::Water},
        {"Buildings", "Shelters, stalls and services", Palette::Buildings},
        {"Scenery", "Plants, rocks, feeders, benches", Palette::Scenery},
        {"Animals", "Buy animals and release them", Palette::Animals},
        {"Bulldoze", "Remove what you click (and get some of the money back)", Palette::None, Dialog::None,
         true, true},
        {"Zoo", "The zoo at a glance", Palette::None, Dialog::Zoo, false, true},
        {"Animal list", "Every animal of the zoo", Palette::None, Dialog::Animals},
        {"Finances", "Income, costs and the loan", Palette::None, Dialog::Finances},
        {"Messages", "What happened lately", Palette::None, Dialog::Messages},
        {"Menu", "Save, load, quit (Esc)", Palette::None, Dialog::Menu, false, true},
    };
    constexpr float kPad = 13.0f;
    constexpr float kGroupGap = 12.0f;
    float width = 0.0f;
    for (const Entry& e : kEntries)
        width += ui.Paint().TextWidth(e.label, theme::kBody) + 2.0f * kPad + kGap +
                 (e.gapBefore ? kGroupGap : 0.0f);
    width += 2.0f * 7.0f - kGap;
    const Box bar{std::round(0.5f * (ui.Screen().X - width)), ui.Screen().Y - hud::kToolbarHeight - 8.0f,
                  width, hud::kToolbarHeight};
    ui.Panel(bar);
    Box row = bar.Inset(7.0f);
    for (const Entry& e : kEntries) {
        if (e.gapBefore) {
            const Box gap = row.CutLeft(kGroupGap);
            ui.Paint().Fill({std::round(gap.MidX() - 1.5f), gap.y + 4.0f, 1.0f, gap.h - 8.0f},
                            theme::kPanelEdge);
        }
        const Box button = row.CutLeft(std::round(ui.Paint().TextWidth(e.label, theme::kBody) + 2.0f * kPad));
        row.CutLeft(kGap);
        bool selected = false;
        if (e.bulldoze)
            selected = play.tool.kind == ToolKind::Bulldoze;
        else if (e.palette != Palette::None)
            selected = play.palette == e.palette;
        else
            selected = play.dialog == e.dialog;
        if (!ui.Button(button, e.label, {.selected = selected, .tooltip = e.tip}))
            continue;
        if (e.bulldoze) {
            play.palette = Palette::None;
            play.Use(selected ? ToolKind::Inspect : ToolKind::Bulldoze);
        } else if (e.palette != Palette::None) {
            OpenPalette(play, selected ? Palette::None : e.palette);
        } else {
            play.dialog = selected ? Dialog::None : e.dialog;
        }
    }
}

void Hud::HintLine(ui::Ui& ui, Play& play) {
    const bool flashing = play.flashLeft > 0.0f && !play.flash.empty();
    const std::string& text = flashing ? play.flash : play.hint;
    if (text.empty())
        return;
    const bool bad = flashing || play.hintBad;
    const float size = theme::kBody;
    const float width =
        std::min(ui.Paint().TextWidth(text, size, flashing) + 28.0f, ui.Screen().X - 2.0f * kPaletteWidth);
    const Box box{std::round(0.5f * (ui.Screen().X - width)),
                  ui.Screen().Y - hud::kToolbarHeight - 8.0f - 36.0f, width, 27.0f};
    const float fade = flashing ? std::clamp(play.flashLeft / 0.4f, 0.0f, 1.0f) : 1.0f;
    ui.Paint().Shadow(box, 8.0f, 0.3f * fade);
    ui.Paint().Frame(box, 13.0f, ui::Faded(bad ? ui::Rgb(64, 24, 22, 240) : theme::kPanel, fade),
                     ui::Faded(bad ? theme::kAlarm : theme::kPanelEdge, fade));
    ui.LabelIn(box.Inset(12.0f, 0.0f), text, size,
               ui::Faded(bad ? ui::Rgb(255, 214, 208, 255) : theme::kText, fade), Align::Center, flashing);
}

void Hud::PalettePanel(ui::Ui& ui, Play& play) {
    if (play.palette == Palette::None)
        return;
    const zoo::Catalog& catalog = *play.catalog;
    const float top = hud::kBarHeight + hud::kMargin;
    const float most = ui.Screen().Y - top - hud::kToolbarHeight - 8.0f - hud::kMargin;
    // How tall the palette wants to be (the long ones take all there is).
    float wanted = most;
    const char* title = "";
    switch (play.palette) {
        case Palette::None:
            return;
        case Palette::Paths:
            title = "Paths";
            wanted = 62.0f + 5.0f * kRow + 62.0f;
            break;
        case Palette::Fences:
            title = "Fences";
            wanted = 62.0f + static_cast<float>(catalog.fences.size()) * kRow + 152.0f;
            break;
        case Palette::Terrain:
            title = "Ground";
            wanted = 62.0f + static_cast<float>(zoo::kTerrainCount) * kRow + 166.0f;
            break;
        case Palette::Water:
            title = "Water";
            wanted = 196.0f;
            break;
        case Palette::Buildings:
            title = "Buildings";
            break;
        case Palette::Scenery:
            title = "Scenery and habitat";
            break;
        case Palette::Animals:
            title = "Animals";
            break;
    }
    const Box box{hud::kMargin, top, kPaletteWidth, std::min(wanted, most)};
    bool closed = false;
    const Box body = ui.Window(box, title, &closed);
    switch (play.palette) {
        case Palette::None:
            break;
        case Palette::Paths:
            PathPalette(ui, play, body);
            break;
        case Palette::Fences:
            FencePalette(ui, play, body);
            break;
        case Palette::Terrain:
            GroundPalette(ui, play, body);
            break;
        case Palette::Water:
            WaterPalette(ui, play, body);
            break;
        case Palette::Buildings:
            BuildingPalette(ui, play, body);
            break;
        case Palette::Scenery:
            SceneryPalette(ui, play, body);
            break;
        case Palette::Animals:
            AnimalPalette(ui, play, body);
            break;
    }
    if (closed)
        OpenPalette(play, Palette::None);
}

void Hud::PathPalette(ui::Ui& ui, Play& play, Box body) {
    const zoo::Catalog& catalog = *play.catalog;
    Tool& tool = play.tool;
    for (int k = 1; k < zoo::kPathKindCount; ++k) {
        const auto kind = static_cast<zoo::PathKind>(k);
        const zoo::PathDef& def = catalog.Path(kind);
        const Box row = body.CutTop(kRow - kGap);
        body.CutTop(kGap);
        if (ItemRow(ui, row, def.name, zoo::FormatMoney(def.cost),
                    tool.kind == ToolKind::Path && tool.path == kind)) {
            tool.path = kind;
            play.Use(ToolKind::Path);
        }
    }
    const Box row = body.CutTop(kRow - kGap);
    if (ItemRow(ui, row, "Remove paths", "",
                tool.kind == ToolKind::Path && tool.path == zoo::PathKind::None)) {
        tool.path = zoo::PathKind::None;
        play.Use(ToolKind::Path);
    }
    Note(ui, body,
         "Drag a line of path. Visitors walk only on paths, and see animals from a path beside a fence.");
}

void Hud::FencePalette(ui::Ui& ui, Play& play, Box body) {
    const zoo::Catalog& catalog = *play.catalog;
    Tool& tool = play.tool;
    for (const zoo::FenceDef& def : catalog.fences) {
        const Box row = body.CutTop(kRow - kGap);
        body.CutTop(kGap);
        if (ItemRow(ui, row, def.name, zoo::FormatMoney(def.cost),
                    tool.kind == ToolKind::Fence && tool.fence == def.kind)) {
            tool.fence = def.kind;
            play.Use(ToolKind::Fence);
        }
    }
    body.CutTop(6.0f);
    const int chosen = Choice(ui, body.CutTop(26.0f), {"Fence", "Gate"}, tool.gate ? 1 : 0);
    if (chosen >= 0) {
        tool.gate = chosen == 1;
        play.Use(ToolKind::Fence);
    }
    body.CutTop(8.0f);
    if (const zoo::FenceDef* def = catalog.FenceByKind(tool.fence)) {
        Fact(ui, body, "Height", Metres(def->heightM));
        Fact(ui, body, "Strength", Format("%d of 5", def->strength));
        Fact(ui, body, "Visitors see through", Percent(def->visibility));
        Fact(ui, body, "A gate costs", zoo::FormatMoney(def->cost + def->gateCost));
        Fact(ui, body, "Upkeep a day", zoo::FormatMoney(def->upkeepPerDay));
    }
    Note(ui, body,
         tool.gate ? "Click a fence panel to put a gate in it: keepers need one to get in."
                   : "Drag along the grid to build; hold Shift to fence a whole rectangle.");
}

void Hud::GroundPalette(ui::Ui& ui, Play& play, Box body) {
    const zoo::Catalog& catalog = *play.catalog;
    Tool& tool = play.tool;
    Heading(ui, body, "GROUND COVER");
    for (int t = 0; t < zoo::kTerrainCount; ++t) {
        const auto terrain = static_cast<zoo::Terrain>(t);
        const Box row = body.CutTop(kRow - kGap);
        body.CutTop(kGap);
        if (ItemRow(ui, row, TerrainName(catalog, terrain),
                    zoo::FormatMoney(catalog.TerrainInfo(terrain).cost),
                    tool.kind == ToolKind::Terrain && tool.terrain == terrain)) {
            tool.terrain = terrain;
            play.Use(ToolKind::Terrain);
        }
    }
    body.CutTop(4.0f);
    {
        Box row = body.CutTop(24.0f);
        ui.LabelIn(row.CutLeft(52.0f), "Brush", theme::kSmall, theme::kMuted);
        const int chosen =
            Choice(ui, row, {"1", "3", "5", "7", "9"}, tool.kind == ToolKind::Terrain ? tool.brushTiles : -1);
        if (chosen >= 0) {
            tool.brushTiles = chosen;
            play.Use(ToolKind::Terrain);
        }
    }
    body.CutTop(10.0f);
    Heading(ui, body, "SHAPE THE LAND");
    {
        int now = -1;
        if (tool.kind == ToolKind::Terraform) {
            if (tool.brush == zoo::Builder::Brush::Raise)
                now = tool.lower ? 1 : 0;
            else
                now = tool.brush == zoo::Builder::Brush::Flatten ? 2 : 3;
        }
        const int chosen = Choice(ui, body.CutTop(26.0f), {"Raise", "Lower", "Flatten", "Smooth"}, now);
        if (chosen >= 0) {
            tool.brush = chosen < 2    ? zoo::Builder::Brush::Raise
                         : chosen == 2 ? zoo::Builder::Brush::Flatten
                                       : zoo::Builder::Brush::Smooth;
            tool.lower = chosen == 1;
            play.Use(ToolKind::Terraform);
        }
    }
    body.CutTop(5.0f);
    {
        static constexpr float kRadii[] = {2.0f, 4.0f, 6.0f, 10.0f};
        Box row = body.CutTop(24.0f);
        ui.LabelIn(row.CutLeft(52.0f), "Radius", theme::kSmall, theme::kMuted);
        int now = -1;
        for (int i = 0; i < 4; ++i) {
            if (tool.kind == ToolKind::Terraform && std::abs(tool.radius - kRadii[i]) < 0.1f)
                now = i;
        }
        const int chosen = Choice(ui, row, {"2 m", "4 m", "6 m", "10 m"}, now);
        if (chosen >= 0) {
            tool.radius = kRadii[chosen];
            play.Use(ToolKind::Terraform);
        }
    }
    Note(ui, body, "Hold the mouse button to paint or to shape. Every species has ground it prefers.");
}

void Hud::WaterPalette(ui::Ui& ui, Play& play, Box body) {
    Tool& tool = play.tool;
    const int mode = Choice(ui, body.CutTop(28.0f), {"Fill", "Drain"}, tool.drain ? 1 : 0);
    if (mode >= 0) {
        tool.drain = mode == 1;
        play.Use(ToolKind::Water);
    }
    body.CutTop(10.0f);
    Heading(ui, body, "DEPTH");
    static constexpr float kDepths[] = {0.4f, 1.0f, 2.5f};
    int now = -1;
    for (int i = 0; i < 3; ++i) {
        if (!tool.drain && std::abs(tool.waterDepth - kDepths[i]) < 0.05f)
            now = i;
    }
    const int chosen = Choice(ui, body.CutTop(26.0f), {"Shallow", "Pond", "Deep"}, now);
    if (chosen >= 0) {
        tool.waterDepth = kDepths[chosen];
        tool.drain = false;
        play.Use(ToolKind::Water);
    }
    Note(ui, body,
         "Drag a rectangle to flood it. Animals drink from open water; swimmers and wallowers need a deep "
         "pool. "
         "Deep water also holds animals that cannot swim.");
}

void Hud::BuildingPalette(ui::Ui& ui, Play& play, Box body) {
    const zoo::Catalog& catalog = *play.catalog;
    Tool& tool = play.tool;
    Box details = body.CutBottom(176.0f);
    body.CutBottom(8.0f);
    const float content = static_cast<float>(catalog.buildings.size()) * kRow;
    const float offset = ui.ScrollBegin("buildings", body, content);
    const float width = body.w - (content > body.h ? 10.0f : 0.0f);
    for (std::size_t i = 0; i < catalog.buildings.size(); ++i) {
        const zoo::BuildingDef& def = catalog.buildings[i];
        const Box row{body.x, body.y + static_cast<float>(i) * kRow - offset, width, kRow - kGap};
        if (row.Bottom() < body.y || row.y > body.Bottom())
            continue;
        if (ItemRow(ui, row, def.name, zoo::FormatMoney(def.cost),
                    tool.kind == ToolKind::Building && tool.building == static_cast<int>(i))) {
            tool.building = static_cast<int>(i);
            play.Use(ToolKind::Building);
        }
    }
    ui.ScrollEnd();
    ui.Separator({details.x, details.y - 5.0f, details.w, 2.0f});
    if (tool.building < 0 || tool.building >= static_cast<int>(catalog.buildings.size()))
        return;
    const zoo::BuildingDef& def = catalog.buildings[static_cast<std::size_t>(tool.building)];
    ui.LabelIn(details.CutTop(22.0f), def.name, theme::kBody, theme::kText, Align::Left, true);
    Fact(ui, details, "Footprint", Format("%d × %d tiles", def.width, def.depth));
    Fact(ui, details, "Stands", Where(def.placement));
    Fact(ui, details, "Upkeep a day", zoo::FormatMoney(def.upkeepPerDay));
    if (!def.item.empty())
        Fact(ui, details, "Sells",
             Format("%s, %s", def.item.c_str(), zoo::FormatMoney(def.itemPrice).c_str()));
    else if (def.role == zoo::BuildingRole::AnimalShelter)
        Fact(ui, details, "Shelters",
             Format("%d animals%s", def.capacity,
                    def.heated   ? ", heated"
                    : def.cooled ? ", cooled"
                                 : ""));
    else
        Fact(ui, details, "Capacity", Format("%d", def.capacity));
    Note(ui, details, def.description);
}

void Hud::SceneryPalette(ui::Ui& ui, Play& play, Box body) {
    const zoo::Catalog& catalog = *play.catalog;
    Tool& tool = play.tool;
    const int tab = Choice(ui, body.CutTop(26.0f), {"Plants", "Rocks", "Habitat", "Visitors"}, sceneryTab_);
    if (tab >= 0)
        sceneryTab_ = tab;
    body.CutTop(8.0f);
    Box details = body.CutBottom(150.0f);
    body.CutBottom(8.0f);
    int shown = 0;
    for (const zoo::PropDef& def : catalog.props)
        shown += SceneryTab(def.role) == sceneryTab_ ? 1 : 0;
    const float content = static_cast<float>(shown) * kRow;
    const std::string scrollId = Format("scenery%d", sceneryTab_);
    const float offset = ui.ScrollBegin(scrollId, body, content);
    const float width = body.w - (content > body.h ? 10.0f : 0.0f);
    int line = 0;
    for (std::size_t i = 0; i < catalog.props.size(); ++i) {
        const zoo::PropDef& def = catalog.props[i];
        if (SceneryTab(def.role) != sceneryTab_)
            continue;
        const Box row{body.x, body.y + static_cast<float>(line++) * kRow - offset, width, kRow - kGap};
        if (row.Bottom() < body.y || row.y > body.Bottom())
            continue;
        if (ItemRow(ui, row, def.name, zoo::FormatMoney(def.cost),
                    tool.kind == ToolKind::Prop && tool.prop == static_cast<int>(i))) {
            tool.prop = static_cast<int>(i);
            play.Use(ToolKind::Prop);
        }
    }
    ui.ScrollEnd();
    ui.Separator({details.x, details.y - 5.0f, details.w, 2.0f});
    if (tool.prop < 0 || tool.prop >= static_cast<int>(catalog.props.size()))
        return;
    const zoo::PropDef& def = catalog.props[static_cast<std::size_t>(tool.prop)];
    ui.LabelIn(details.CutTop(22.0f), def.name, theme::kBody, theme::kText, Align::Left, true);
    Fact(ui, details, "Stands", Where(def.placement));
    if (!def.biome.empty())
        Fact(ui, details, "Biome", def.biome);
    if (def.shadeM2 > 0.0f)
        Fact(ui, details, "Shade", Format("%d m²", static_cast<int>(std::lround(def.shadeM2))));
    if (def.enrichment > 0.0f)
        Fact(ui, details, "Enrichment", Percent(def.enrichment));
    if (def.role == zoo::PropRole::Feeder) {
        std::string foods;
        for (const std::string& id : def.foods) {
            const int index = catalog.FoodIndex(id);
            if (index < 0)
                continue;
            if (!foods.empty())
                foods += ", ";
            foods += catalog.foods[static_cast<std::size_t>(index)].name;
        }
        Fact(ui, details, "Holds", Format("%d kg", static_cast<int>(std::lround(def.capacity))));
        Note(ui, details, foods);
    } else if (def.role == zoo::PropRole::WaterTrough) {
        Fact(ui, details, "Holds", Format("%d litres", static_cast<int>(std::lround(def.capacity))));
    } else if (def.role == zoo::PropRole::Bench) {
        Fact(ui, details, "Seats", Format("%d", static_cast<int>(std::lround(def.capacity))));
    } else if (def.beauty > 0.0f) {
        Fact(ui, details, "Beauty", Percent(def.beauty));
    }
}

void Hud::AnimalPalette(ui::Ui& ui, Play& play, Box body) {
    const zoo::Zoo& zoo = *play.zoo;
    const auto& species = play.species->All();
    Tool& tool = play.tool;
    Box details = body.CutBottom(258.0f);
    body.CutBottom(8.0f);
    const float content = static_cast<float>(species.size()) * kRow;
    const float offset = ui.ScrollBegin("species", body, content);
    const float width = body.w - (content > body.h ? 10.0f : 0.0f);
    const zoo::Money balance = zoo.Accounts().Balance();
    for (std::size_t i = 0; i < species.size(); ++i) {
        const Box row{body.x, body.y + static_cast<float>(i) * kRow - offset, width, kRow - kGap};
        if (row.Bottom() < body.y || row.y > body.Bottom())
            continue;
        const zoo::Money price = zoo.PriceOf(static_cast<int>(i));
        if (ItemRow(ui, row, species[i].commonName, zoo::FormatMoney(price),
                    tool.kind == ToolKind::Animal && tool.species == static_cast<int>(i),
                    price > balance && !zoo.Sandbox() ? "The zoo cannot afford one now" : "")) {
            tool.species = static_cast<int>(i);
            play.Use(ToolKind::Animal);
        }
    }
    ui.ScrollEnd();
    ui.Separator({details.x, details.y - 5.0f, details.w, 2.0f});
    if (tool.species < 0 || tool.species >= static_cast<int>(species.size()))
        return;
    const zoo::SpeciesDef& def = species[static_cast<std::size_t>(tool.species)];
    ui.LabelIn(details.CutTop(20.0f), def.commonName, theme::kBody, theme::kText, Align::Left, true);
    ui.LabelIn(details.CutTop(17.0f), def.scientificName, theme::kSmall, theme::kMuted);
    details.CutTop(5.0f);
    const int sex =
        Choice(ui, details.CutTop(25.0f), {"♀ Female", "♂ Male"}, tool.sex == zoo::Sex::Male ? 1 : 0);
    if (sex >= 0) {
        tool.sex = sex == 1 ? zoo::Sex::Male : zoo::Sex::Female;
        play.Use(ToolKind::Animal);
    }
    details.CutTop(6.0f);
    Fact(ui, details, "Status", zoo::DisplayName(def.status));
    Fact(ui, details, "In the zoo", Format("%d", zoo.AnimalCount(tool.species)));
    Fact(ui, details, "Lives in groups of",
         def.groupMin == def.groupMax ? Format("%d", def.groupMin)
                                      : Format("%d–%d", def.groupMin, def.groupMax));
    Fact(ui, details, "Enclosure",
         Format("%d m², +%d each", static_cast<int>(std::lround(def.minEnclosureM2)),
                static_cast<int>(std::lround(def.spacePerExtraAnimalM2))));
    Fact(ui, details, "Fence",
         Format("%s, strength %d", Metres(def.fenceHeightM).c_str(), def.fenceStrength));
    Fact(ui, details, "Climate",
         Format("%d to %d °C", static_cast<int>(std::lround(def.tempMinC)),
                static_cast<int>(std::lround(def.tempMaxC))));
    std::string foods;
    for (const std::string& id : def.foods) {
        const int index = play.catalog->FoodIndex(id);
        if (index < 0)
            continue;
        if (!foods.empty())
            foods += ", ";
        foods += play.catalog->foods[static_cast<std::size_t>(index)].name;
    }
    Fact(ui, details, "Eats", foods);
    Fact(ui, details, "From", def.nativeRange);
}

}  // namespace zoogame
