// SPDX-License-Identifier: MIT
#pragma once

#include <array>
#include <string>
#include <utility>
#include <vector>

#include "Common.hpp"

namespace zoogame {

/// Where a visit stands: what is on the screen and who has the mouse.
enum class VisitScreen {
    Title,    ///< the title, the zoo at eye level behind it
    Walking,  ///< in the zoo on foot: the mouse looks around
    Paused,   ///< the pause menu: the world stands, the mouse is free
};

/// A window: Settings and Load open from the title or from the pause menu; a
/// board one reads and the map of the zoo open on the walk, which goes on under them.
enum class VisitWindow { None, Settings, Load, Board, Map };

/// The animal the eyes rest on: who it is and what it is doing.
struct AnimalCard {
    std::string name;     ///< "Zuri"
    std::string species;  ///< "African lion"
    std::string latin;    ///< "Panthera leo"
    std::string about;    ///< "Female, 6 years old"
    std::string doing;    ///< "Resting"
    /// 0..1: fades in while the eyes stay on the animal, out when they leave it.
    float shown = 0.0f;
};

/// What an education board at an enclosure says.
struct BoardText {
    std::string title;  ///< "African lion"
    std::string latin;
    std::vector<std::pair<std::string, std::string>> rows;  ///< "Native range" - "Sub-Saharan Africa"
    std::vector<std::string> facts;
    std::string residents;  ///< "Living here: Zuri, Tau and Nala"
    std::string others;     ///< "Also here: plains zebra"
};

/// The visitor map: the zoo from above, as a plan on a board would show it.
struct VisitMap {
    enum class Kind { Ground, Water, Habitat, Path, Building, Entrance };
    struct Shape {
        float x = 0, z = 0, w = 0, d = 0;  ///< metres
        Kind kind = Kind::Ground;
    };
    struct Label {
        float x = 0, z = 0;
        std::string text;
        bool habitat = false;  ///< an animal's name (bold); else a building's
    };
    /// A place one can go to straight from the map: a habitat's front, a house's door, the gate.
    struct Place {
        float x = 0, z = 0;  ///< where one stands, metres
        float yaw = 0;       ///< where one looks then (0 = +z/south, pi = north)
        std::string name;
        bool habitat = false;
    };
    float minX = 0, minZ = 0, maxX = 1, maxZ = 1;
    std::vector<Shape> shapes;
    std::vector<Label> labels;
    std::vector<Place> places;
    Vector2 you{0.0f, 0.0f};  ///< x, z
    float yaw = 0.0f;
};

/// What the interface asks the game to do after this frame.
enum class VisitRequest { None, NewVisit, Continue, Save, Load, ToTitle, Quit };

/// How many people are in the zoo: the most it holds at once. Normal is the crowd of a
/// mid-morning in summer the visit was tuned with; people arrive in proportion, so a smaller
/// crowd is not a full zoo with a queue at the gate.
struct CrowdLevel {
    const char* key;   ///< in the preferences file
    const char* name;  ///< in the settings window
    int people;
};
inline constexpr std::array<CrowdLevel, 5> kCrowdLevels{{{"nobody", "Nobody", 0},
                                                         {"few", "Few", 150},
                                                         {"fewer", "Fewer", 500},
                                                         {"normal", "Normal", 800},
                                                         {"more", "More", 1200}}};
inline constexpr int kNormalCrowd = 3;

/// What the player set (kept in the engine's preferences).
struct VisitSettings {
    /// A factor on the mouse's speed: 1 turns 0.12 degrees a pixel.
    float sensitivity = 1.0f;
    bool invertY = false;
    /// Vertical field of view, degrees (58 is about 90 across a 16:9 screen).
    float fieldOfView = 58.0f;
    bool headBob = true;
    /// The sound of the zoo, 0 silent .. 1 full.
    float volume = 0.8f;
    /// The figures of the visitors and their avatars on the screen (F4).
    bool diagnostics = false;
    /// An index into kCrowdLevels.
    int crowd = kNormalCrowd;
};

/// What the sound of the zoo reports about itself (F4, and the figures line of
/// a scripted run: nobody listens there, so it has to say what it plays).
struct SoundFigures {
    bool device = false;   ///< an audio device plays
    int files = 0;         ///< recordings loaded
    int failed = 0;        ///< recordings of the library that could not be loaded
    int voices = 0;        ///< sounds going on (heard or waiting for a voice)
    int audible = 0;       ///< of them heard now
    int calls = 0;         ///< animal calls since the visit began
    int steps = 0;         ///< footsteps since the visit began
    std::string lastCall;  ///< "lion_roar, 34 m away"
};

/// What the diagnostics overlay shows.
struct VisitFigures {
    int visitors = 0;      ///< people the simulation has in the zoo
    int inRange = 0;       ///< near enough to be drawn
    int inView = 0;        ///< of them in front of the eyes
    int drawn = 0;         ///< drawn on the last frame (the nearest of those in view)
    int castSize = 0;      ///< avatars asked of CNA
    int castReady = 0;     ///< of them ready to be drawn
    int castFailed = 0;    ///< of them CNA could not make
    int animals = 0;
    int animalsDrawn = 0;
    int solids = 0;        ///< shapes of the collision world
    float framesPerSecond = 0.0f;
    Vector3 feet = Vector3(0.0f, 0.0f, 0.0f);
    float yawDegrees = 0.0f;
    float speed = 0.0f;
    SoundFigures sound;
};

/// The state the visitor's interface and the game share.
struct Visit {
    VisitScreen screen = VisitScreen::Title;
    VisitWindow window = VisitWindow::None;
    VisitSettings settings;
    /// The interface changed a setting this frame: the game applies and stores it.
    bool settingsChanged = false;

    VisitRequest request = VisitRequest::None;
    std::string requestPath;  ///< Load: the file

    /// There is a visit to go on with.
    bool canContinue = false;
    /// The time of day as the corner of the screen shows it ("10:42").
    std::string clock;
    /// What E would do now ("Read the board"); empty when nothing is in reach.
    std::string prompt;
    /// A passing notice ("Visit saved").
    std::string flash;
    float flashLeft = 0.0f;
    /// Seconds the keys are still shown for after the walk began.
    float hintLeft = 0.0f;
    /// Shift switches between walking and running; it is not held.
    bool running = false;
    /// B switches the binoculars on and off (the right mouse button holds them up while pressed).
    bool binoculars = false;
    /// 0..1: how far they are raised to the eyes (the view narrows and the eyepieces close in).
    float binocularsUp = 0.0f;
    AnimalCard card;
    BoardText board;
    VisitMap map;
    /// The place of the map clicked this frame, to be gone to (-1: none).
    int travelTo = -1;

    VisitFigures figures;

    void Flash(std::string text, float seconds = 3.5f) {
        flash = std::move(text);
        flashLeft = seconds;
    }
};

}  // namespace zoogame
