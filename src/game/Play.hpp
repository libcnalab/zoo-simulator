// SPDX-License-Identifier: MIT
#pragma once

#include <string>

#include "AnimalRenderer.hpp"
#include "CameraRig.hpp"
#include "zoo/sim/Zoo.hpp"

namespace zoogame {

/// What a click on the world does.
enum class ToolKind {
    Inspect,     ///< select what is clicked
    Path,        ///< lay paths (drag a line)
    Fence,       ///< build fences (drag a line; shift: around a rectangle) or a gate
    Terrain,     ///< paint ground cover with a brush
    Water,       ///< flood a rectangle
    Terraform,   ///< raise, lower, flatten, smooth
    Building,    ///< place a building
    Prop,        ///< place scenery or a habitat item
    Animal,      ///< release a bought animal
    MoveAnimal,  ///< carry an animal the zoo owns to another place
    Bulldoze,    ///< remove what is clicked
};

struct Tool {
    ToolKind kind = ToolKind::Inspect;
    zoo::PathKind path = zoo::PathKind::Asphalt;
    int fence = 1;  ///< FenceDef::kind
    bool gate = false;
    zoo::Terrain terrain = zoo::Terrain::Grass;
    int brushTiles = 1;  ///< half-width of the terrain brush, tiles (0 = one tile)
    float waterDepth = 1.0f;
    bool drain = false;  ///< the water tool removes water
    zoo::Builder::Brush brush = zoo::Builder::Brush::Raise;
    bool lower = false;   ///< Raise digs instead
    float radius = 4.0f;  ///< of the terraforming brush, metres
    int building = 0;     ///< index into Catalog::buildings
    zoo::Dir facing = zoo::Dir::South;
    int prop = 0;  ///< index into Catalog::props
    float heading = 0.0f;
    int species = 0;
    zoo::Sex sex = zoo::Sex::Female;
    zoo::AnimalId moving;
};

enum class Picked { None, Animal, Enclosure, Building, Prop };

/// What the inspector shows.
struct Selection {
    Picked kind = Picked::None;
    zoo::AnimalId animal;
    zoo::EnclosureId enclosure;
    zoo::BuildingId building;
    zoo::PropId prop;

    void Clear() { *this = {}; }
};

/// The palette open on the left (what the build buttons of the toolbar switch).
enum class Palette { None, Paths, Fences, Terrain, Water, Buildings, Scenery, Animals };

/// The window open in the middle of the screen.
enum class Dialog { None, Zoo, Finances, Animals, Messages, Menu, Save, Load, NewGame, Help };

/// The windows that stop the game and take every click until they are closed.
constexpr bool IsModal(Dialog dialog) {
    return dialog == Dialog::Menu || dialog == Dialog::Save || dialog == Dialog::Load ||
           dialog == Dialog::NewGame || dialog == Dialog::Help;
}

/// What the interface asks of the game.
enum class Request { None, NewGame, Save, Load, Quit, MainMenu };

/// What the interface and the world tools share, between them and with the game.
struct Play {
    zoo::Zoo* zoo = nullptr;
    const zoo::Catalog* catalog = nullptr;
    const zoo::SpeciesCatalog* species = nullptr;
    CameraRig* camera = nullptr;
    const AnimalRenderer* animals = nullptr;

    /// Game speed: ticks per tick of real time (0 = paused).
    int speed = 1;
    Tool tool;
    Selection selection;
    Palette palette = Palette::None;
    Dialog dialog = Dialog::None;
    /// The camera keeps on the selected animal.
    bool follow = false;
    /// The player walks the zoo at eye height instead of looking from above.
    bool walk = false;
    /// The title screen is up: the zoo behind it is a backdrop.
    bool mainMenu = false;
    /// A game is in progress (the title screen offers to go back to it).
    bool started = true;

    /// What the tool in hand would do or why it cannot, shown above the toolbar.
    std::string hint;
    bool hintBad = false;

    /// What just went wrong ("Not enough money"), shown for a few seconds.
    std::string flash;
    float flashLeft = 0.0f;
    bool flashBad = true;

    Request request = Request::None;
    std::string requestPath;     ///< the save file to write or read
    zoo::ZooConfig newGame;      ///< for Request::NewGame
    bool newGameStarter = true;  ///< Request::NewGame begins with the starter zoo, not bare land

    void Use(ToolKind kind) {
        tool.kind = kind;
        hint.clear();
        hintBad = false;
    }
    void Flash(std::string text) {
        flash = std::move(text);
        flashLeft = 3.5f;
        flashBad = true;
    }
    /// News rather than a refusal ("Saved").
    void Tell(std::string text) {
        Flash(std::move(text));
        flashBad = false;
    }
    void Select(Selection s) {
        selection = s;
        follow = false;
    }
};

}  // namespace zoogame
