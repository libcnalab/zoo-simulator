// SPDX-License-Identifier: MIT
//
// The game as it is played: a visit to the zoo on foot.
#include "ZooGame.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>

#include "CnaEngine/App/AssetDirectory.hpp"

#include "Saves.hpp"
#include "zoo/sim/Scenario.hpp"
#include "zoo/sim/VisitorZoo.hpp"
#include "zoo/sim/ZooSave.hpp"

#include "Microsoft/Xna/Framework/MathHelper.hpp"

namespace zoogame {

namespace {

constexpr float kPi = 3.14159265358979f;
/// A visit begins on a day of early summer, in the middle of the morning: the
/// zoo has been open for an hour and a half and is full of people.
constexpr std::int64_t kVisitDay = 14;
constexpr float kVisitHour = 10.5f;
/// The calendar creeps: one game hour takes six real minutes (the management
/// game's takes thirty seconds), while everything alive moves at its own speed.
constexpr int kVisitPace = 12;
/// The visitors' pace for a visit on foot: a minute at a fence, twenty minutes in the zoo,
/// and as many arrivals as keep the crowd of mid-morning. While the zoo is filled before the
/// player walks in, people come kFillArrivals times as fast as that.
constexpr zoo::VisitorPace kVisitPeople{.linger = 6.0f, .stay = 6.0f, .arrivals = 0.25f};
constexpr float kFillArrivals = 1.7f;
/// The day stops before the afternoon thins the crowd out: a visit has no closing time.
constexpr float kLatestHour = 15.5f;
constexpr float kEarliestHour = 9.5f;
/// Steps the zoo runs with its calendar stopped before a visit, so that the
/// visitors are spread over it and not all at the gate (five real minutes' worth).
constexpr std::int64_t kFillTicks = 6000;
/// Radians the eyes turn for a pixel of the mouse at sensitivity 1 (0.12 degrees).
constexpr float kTurnPerPixel = 0.0021f;
/// Real seconds between two keepings of the visit "Continue Visit" goes on with.
constexpr double kKeepEvery = 120.0;

std::string StampedTitle() {
    const std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm local{};
    localtime_r(&now, &local);
    char text[48];
    std::strftime(text, sizeof text, "visit-%Y-%m-%d-%H%M%S", &local);
    return text;
}

}  // namespace

std::unique_ptr<zoo::Zoo> ZooGame::MakeVisitorZoo() {
    const auto began = std::chrono::steady_clock::now();
    auto made = std::make_unique<zoo::Zoo>(*catalog_, *species_, zoo::VisitorZooConfig(seed_));
    if (!emptyMap_) {
        // A habitat whose animals the game cannot show yet is built and named, and waits for them.
        zoo::VisitorZooOptions options;
        const auto assets = CnaEngine::App::FindAssetDirectory("assets");
        for (const zoo::SpeciesDef& def : species_->All()) {
            if (!assets || !std::filesystem::exists(*assets / "animals" / (def.id + ".glb")))
                options.withoutSpecies.push_back(def.id);
        }
        const zoo::ScenarioReport report = zoo::BuildVisitorZoo(*made, options);
        const double took = std::chrono::duration<double>(std::chrono::steady_clock::now() - began).count();
        std::printf("zoo: visitor zoo: %d enclosures, %d animals, %d buildings, %d props (%.2f s)\n",
                    report.enclosures, report.animals, report.buildings, report.props, took);
        for (const std::string& problem : report.problems)
            std::printf("zoo: visitor zoo: %s\n", problem.c_str());
    }
    ShowLoading("The gates opened at nine: the morning's visitors are coming in", 0.3f);
    PrepareVisit(*made, true);
    return made;
}

void ZooGame::PrepareVisit(zoo::Zoo& zoo, bool fill) {
    // People take their time here: a minute at a fence, not the few seconds of a day that
    // passes in a quarter of an hour.
    zoo.Guests().SetPace(kVisitPeople);
    const float hour = startHour_ ? static_cast<float>(*startHour_) : kVisitHour;
    const float now = zoo.Time().Hour();
    if (fill || now < kEarliestHour || now >= kLatestHour) {
        zoo.SetCalendar(fill ? kVisitDay : zoo.Time().Day(), hour);
        fill = true;
    }
    if (fill) {
        const auto began = std::chrono::steady_clock::now();
        const std::int64_t ticks = warmUpTicks_ > 0 ? warmUpTicks_ : kFillTicks;
        zoo.SetDayPace(0);
        // The people who came in first are set down all over the zoo, as if it had been
        // open since nine; the rest of the time they and the later ones settle.
        const std::int64_t before = ticks * 3 / 5;
        zoo::VisitorPace filling = kVisitPeople;
        filling.arrivals = kFillArrivals;
        zoo.Guests().SetPace(filling);
        for (std::int64_t i = 0; i < before; ++i)
            zoo.Step();
        const int spread = zoo.SpreadVisitors();
        zoo.Guests().SetPace(kVisitPeople);
        for (std::int64_t i = before; i < ticks; ++i)
            zoo.Step();
        const double took = std::chrono::duration<double>(std::chrono::steady_clock::now() - began).count();
        std::printf(
            "zoo: visit: %d visitors (%d set down about the zoo) after %lld steps at %.2f h (%.2f s)\n",
            zoo.Guests().Count(), spread, static_cast<long long>(ticks),
            static_cast<double>(zoo.Time().Hour()), took);
        // Where the people are: how many do what, and how many stand at each habitat.
        std::map<std::uint32_t, int> watchers;
        std::array<int, static_cast<std::size_t>(zoo::VisitorActivity::Count)> doing{};
        for (const zoo::Visitor& visitor : zoo.Guests().All()) {
            ++doing[static_cast<std::size_t>(visitor.activity)];
            if (visitor.activity == zoo::VisitorActivity::Watching)
                ++watchers[visitor.targetEnclosure.value];
        }
        std::string line;
        for (std::size_t i = 0; i < doing.size(); ++i)
            if (doing[i] > 0)
                line += Format(" %s %d,",
                               std::string(zoo::ToString(static_cast<zoo::VisitorActivity>(i))).c_str(),
                               doing[i]);
        line += " at the habitats:";
        for (const auto& [id, count] : watchers) {
            const zoo::Enclosure* enclosure = zoo.Habitats().Find(zoo::EnclosureId{id});
            line += Format(" %s %d,", enclosure ? enclosure->name.c_str() : "?", count);
        }
        std::printf("zoo: visit:%s\n", line.c_str());
        // Where the walkers are going (only the leaders know).
        std::map<std::uint32_t, int> bound;
        std::array<int, static_cast<std::size_t>(zoo::VisitorGoal::Count)> wants{};
        for (const zoo::Visitor& visitor : zoo.Guests().All()) {
            if (visitor.activity != zoo::VisitorActivity::Walking || !visitor.leader)
                continue;
            ++wants[static_cast<std::size_t>(visitor.goal)];
            if (visitor.goal == zoo::VisitorGoal::Watch)
                ++bound[visitor.targetEnclosure.value];
        }
        line = " walking groups:";
        for (std::size_t i = 0; i < wants.size(); ++i)
            if (wants[i] > 0)
                line +=
                    Format(" %s %d,", std::string(zoo::ToString(static_cast<zoo::VisitorGoal>(i))).c_str(),
                           wants[i]);
        line += " bound for:";
        for (const auto& [id, count] : bound) {
            const zoo::Enclosure* enclosure = zoo.Habitats().Find(zoo::EnclosureId{id});
            line += Format(" %s %d,", enclosure ? enclosure->name.c_str() : "?", count);
        }
        std::printf("zoo: visit:%s\n", line.c_str());
    }
    zoo.SetDayPace(kVisitPace);
}

ZooGame::Pose ZooGame::EntrancePose() const {
    // Two tiles inside the gate, on the path that leads from it, looking north into the zoo.
    const zoo::Vec2 gate = zoo::World::TileCenter(zoo_->Map().EntranceTile());
    return Pose{gate.x, gate.z - 2.0f * zoo::kTileSize, kPi, 0.0f};
}

void ZooGame::BeginWalk(const Pose& pose) {
    walker_.Enter(*zoo_, {pose.x, pose.z}, pose.yaw, pose.pitch);
    visit_.screen = VisitScreen::Walking;
    visit_.window = VisitWindow::None;
    visit_.hintLeft = 14.0f;
    visit_.running = false;
    freshZoo_ = false;
    sinceKept_ = 0.0;
    tickDebt_ = 0.0;
}

bool ZooGame::SaveVisit(const std::string& path, bool quietly) {
    std::error_code ignored;
    std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ignored);
    std::string error;
    if (!zoo::SaveZoo(*zoo_, path, &error)) {
        visit_.Flash("The visit could not be saved: " + error, 6.0f);
        std::printf("zoo: save failed: %s\n", error.c_str());
        return false;
    }
    {
        std::ofstream pose(path + std::string(kPoseSuffix), std::ios::trunc);
        const zoo::Vec2 at = walker_.Position();
        pose << at.x << ' ' << at.z << ' ' << walker_.Yaw() << ' ' << walker_.Pitch() << '\n';
    }
    std::printf("zoo: saved %s\n", path.c_str());
    if (!quietly)
        visit_.Flash("Visit saved");
    return true;
}

bool ZooGame::LoadVisit(const std::string& path) {
    std::string error;
    std::unique_ptr<zoo::Zoo> loaded = zoo::LoadZoo(path, *catalog_, *species_, &error);
    if (!loaded) {
        visit_.Flash("The visit could not be loaded: " + error, 6.0f);
        std::printf("zoo: load failed: %s\n", error.c_str());
        return false;
    }
    std::printf("zoo: loaded %s\n", path.c_str());
    // A zoo saved by the editor may be anywhere in its day; a visit's own saves need nothing.
    PrepareVisit(*loaded, false);
    Adopt(std::move(loaded));
    Pose pose = EntrancePose();
    std::ifstream file(path + std::string(kPoseSuffix));
    Pose read;
    if (file >> read.x >> read.z >> read.yaw >> read.pitch) {
        const zoo::World& map = zoo_->Map();
        if (std::isfinite(read.x) && std::isfinite(read.z) && std::isfinite(read.yaw) &&
            std::isfinite(read.pitch) && read.x > 0.0f && read.z > 0.0f && read.x < map.SizeX() &&
            read.z < map.SizeZ())
            pose = read;
    }
    BeginWalk(pose);
    return true;
}

void ZooGame::KeepVisit() {
    // An automated run writes among the player's saves only where it was told to.
    if ((Automation().Options().Automated() || script_) && !ownSaves_)
        return;
    if (SaveVisit(SavePath(kContinueTitle).string(), true))
        visit_.canContinue = true;
}

void ZooGame::HandleVisitRequest() {
    const VisitRequest request = std::exchange(visit_.request, VisitRequest::None);
    switch (request) {
        case VisitRequest::None:
            break;
        case VisitRequest::NewVisit:
            if (!freshZoo_)
                Adopt(MakeVisitorZoo());
            BeginWalk(EntrancePose());
            break;
        case VisitRequest::Continue:
            LoadVisit(SavePath(kContinueTitle).string());
            break;
        case VisitRequest::Load:
            LoadVisit(visit_.requestPath);
            break;
        case VisitRequest::Save:
            SaveVisit(SavePath(StampedTitle()).string(), false);
            break;
        case VisitRequest::ToTitle:
            KeepVisit();
            visit_.screen = VisitScreen::Title;
            visit_.window = VisitWindow::None;
            break;
        case VisitRequest::Quit:
            if (visit_.screen != VisitScreen::Title)
                KeepVisit();
            Exit();
            break;
    }
}

void ZooGame::ReadVisitSettings() {
    const auto& file = Preferences();
    VisitSettings& s = visit_.settings;
    s.sensitivity = file.GetFloat("visit.mouse_speed", s.sensitivity, 0.2f, 3.0f);
    s.invertY = file.GetBool("visit.invert_y", s.invertY);
    s.fieldOfView = file.GetFloat("visit.field_of_view", s.fieldOfView, 45.0f, 75.0f);
    s.headBob = file.GetBool("visit.head_bob", s.headBob);
    s.volume = file.GetFloat("visit.volume", s.volume, 0.0f, 1.0f);
    s.diagnostics = file.GetBool("visit.diagnostics", s.diagnostics);
}

void ZooGame::ApplyVisitSettings(bool store) {
    using Microsoft::Xna::Framework::MathHelper;
    const VisitSettings& s = visit_.settings;
    auto& camera = MainCamera();
    camera.SetPerspective(MathHelper::ToRadians(s.fieldOfView), camera.AspectRatio(), camera.NearPlane(),
                          camera.FarPlane());
    walker_.SetHeadBob(s.headBob ? 0.012f : 0.0f);
    sounds_->SetVolume(s.volume);
    if (!store)
        return;
    auto& file = Preferences();
    file.SetFloat("visit.mouse_speed", s.sensitivity, 2);
    file.SetBool("visit.invert_y", s.invertY);
    file.SetFloat("visit.field_of_view", s.fieldOfView, 1);
    file.SetBool("visit.head_bob", s.headBob);
    file.SetFloat("visit.volume", s.volume, 2);
    file.SetBool("visit.diagnostics", s.diagnostics);
    std::string error;
    if (!SavePreferences(&error))
        std::printf("zoo: settings: %s\n", error.c_str());
}

void ZooGame::FillVisitFigures() {
    VisitFigures& f = visit_.figures;
    f.visitors = zoo_->Guests().Count();
    f.inRange = static_cast<int>(visitors_->InRange());
    f.inView = static_cast<int>(visitors_->InView());
    f.drawn = static_cast<int>(visitors_->Drawn());
    f.castSize = static_cast<int>(visitors_->CastSize());
    f.castReady = visitors_->Ready();
    f.castFailed = visitors_->Failed();
    f.animals = static_cast<int>(zoo_->Fauna().All().size());
    f.animalsDrawn = static_cast<int>(animals_->Drawn());
    f.solids = static_cast<int>(walker_.Shapes());
    f.framesPerSecond = static_cast<float>(FrameTimes().FramesPerSecond());
    f.feet = walker_.Feet();
    f.yawDegrees = walker_.Yaw() * 180.0f / kPi;
    f.speed = walker_.Speed();
    f.sound = sounds_->Figures();

    const float hour = zoo_->Time().Hour();
    const int minutes = static_cast<int>(hour * 60.0f);
    visit_.clock = Format("%d:%02d", minutes / 60, minutes % 60);
}

void ZooGame::PrintVisitFigures(const char* when) const {
    const zoo::Vec2 at = walker_.Position();
    std::printf(
        "zoo: visit %s: %s, at %.1f,%.1f heading %.0f; visitors %d simulated, %zu in range, %zu in view, %zu "
        "drawn; "
        "avatars %d ready of %zu, %d failed; animals %zu, %zu drawn\n",
        when, visit_.clock.c_str(), static_cast<double>(at.x), static_cast<double>(at.z),
        static_cast<double>(walker_.Yaw() * 180.0f / kPi), zoo_->Guests().Count(), visitors_->InRange(),
        visitors_->InView(), visitors_->Drawn(), visitors_->Ready(), visitors_->CastSize(),
        visitors_->Failed(), zoo_->Fauna().All().size(), animals_->Drawn());
    const SoundFigures& sound = sounds_->Figures();
    std::printf(
        "zoo: visit %s: sound %s, %d recordings (%d failed), %d voices (%d heard), %d steps, %d calls, last "
        "\"%s\"\n",
        when, sound.device ? "plays" : "has no device", sound.files, sound.failed, sound.voices,
        sound.audible, sound.steps, sound.calls, sound.lastCall.c_str());
    if (visit_.card.shown > 0.0f || !visit_.prompt.empty() || visit_.window != VisitWindow::None)
        std::printf("zoo: visit %s: looking at \"%s, %s\" (%s); E: \"%s\"; window %d\n", when,
                    visit_.card.shown > 0.0f ? visit_.card.name.c_str() : "",
                    visit_.card.shown > 0.0f ? visit_.card.species.c_str() : "",
                    visit_.card.shown > 0.0f ? visit_.card.doing.c_str() : "", visit_.prompt.c_str(),
                    static_cast<int>(visit_.window));
}

void ZooGame::UpdateVisit(double seconds) {
    const auto dt = static_cast<float>(seconds);
    const bool automated = Automation().Options().Automated() || script_.has_value();
    Vector2 look(0.0f, 0.0f);
    if (script_) {
        shots_.clear();
        controls_.Begin(script_->Next(shots_));
        look = script_->Look();
        for (const std::string& path : shots_) {
            Screenshots().Request({.path = path, .opaque = true});
            PrintVisitFigures(("for " + path).c_str());
        }
    } else {
        controls_.Begin(CurrentInput());
        look = MouseDelta();
    }

    FillVisitFigures();
    ui_->Begin(dt);
    visitorHud_.Build(*ui_, visit_);
    ui_->End();
    if (std::exchange(visit_.settingsChanged, false))
        ApplyVisitSettings(true);

    if (controls_.KeyboardFree()) {
        if (controls_.Pressed(Keys::Escape)) {
            if (visit_.window != VisitWindow::None)
                visit_.window = VisitWindow::None;
            else if (visit_.screen == VisitScreen::Walking)
                visit_.screen = VisitScreen::Paused;
            else if (visit_.screen == VisitScreen::Paused)
                visit_.screen = VisitScreen::Walking;
        }
        if (controls_.Pressed(Keys::F4)) {
            visit_.settings.diagnostics = !visit_.settings.diagnostics;
            ApplyVisitSettings(true);
        }
        if (controls_.Pressed(Keys::F5) && visit_.screen == VisitScreen::Walking)
            visit_.request = VisitRequest::Save;
    }
    HandleVisitRequest();

    // The window lost the keyboard (Alt+Tab): the walk waits, the mouse is let go.
    if (visit_.screen == VisitScreen::Walking && !automated && !getIsActiveProperty())
        visit_.screen = VisitScreen::Paused;
    const bool walking = visit_.screen == VisitScreen::Walking;
    const bool capture = walking && !automated;
    if (capture != MouseCaptured())
        CaptureMouse(capture);

    // The world lives while one walks and behind the title; the pause menu stops it.
    play_.speed = visit_.screen == VisitScreen::Paused ? 0 : 1;
    if (walking) {
        Walker::Input input;
        if (controls_.KeyboardFree()) {
            const auto axis = [&](Keys plus, Keys plusToo, Keys minus, Keys minusToo) {
                return (controls_.Down(plus) || controls_.Down(plusToo) ? 1.0f : 0.0f) -
                       (controls_.Down(minus) || controls_.Down(minusToo) ? 1.0f : 0.0f);
            };
            input.ahead = axis(Keys::W, Keys::Up, Keys::S, Keys::Down);
            input.right = axis(Keys::D, Keys::Right, Keys::A, Keys::Left);
            // Shift switches the pace and it stays: nobody holds a key down across a whole zoo.
            if (controls_.Pressed(Keys::LeftShift) || controls_.Pressed(Keys::RightShift)) {
                visit_.running = !visit_.running;
                visit_.Flash(visit_.running ? "Running" : "Walking", 1.4f);
            }
        }
        input.run = visit_.running;
        const float turn = kTurnPerPixel * visit_.settings.sensitivity;
        input.turn = look.X * turn;
        input.tilt = look.Y * turn * (visit_.settings.invertY ? -1.0f : 1.0f);
        walker_.Update(*zoo_, input, dt);
        UpdateEncounters(dt);
        sinceKept_ += seconds;
        if (sinceKept_ >= kKeepEvery) {
            sinceKept_ = 0.0;
            if (!automated)
                KeepVisit();
        }
    } else if (visit_.screen == VisitScreen::Title) {
        titleTime_ += dt;
    }
    if (play_.speed > 0)
        StepSimulation(seconds);
    if (zoo_->Config().dayPace != 0 && zoo_->Time().Hour() >= kLatestHour)
        zoo_->SetDayPace(0);

    if (visit_.screen == VisitScreen::Title) {
        // Behind the title: the entrance as someone standing there sees it, looking slowly about.
        const float yaw = walker_.Yaw() + 0.32f * std::sin(titleTime_ * 0.09f);
        camera_.SetEye(walker_.Eye(), yaw, 0.02f);
    } else {
        camera_.SetEye(walker_.Eye(), walker_.Yaw(), walker_.Pitch());
    }
    camera_.Apply(MainCamera());
    UpdateShadowArea();
    terrain_->Update(zoo_->Map());
    scenery_->Update(zoo_->Map(), zoo_->Objects());
    const float lived = dt * static_cast<float>(play_.speed);
    animals_->Update(*zoo_, lived, static_cast<float>(tickDebt_));
    visitors_->Update(*zoo_, lived, static_cast<float>(tickDebt_));
    UpdateSky();

    const bool afoot = visit_.screen == VisitScreen::Walking;
    sounds_->Update(*zoo_, CnaEngine::Audio::ListenerFrom(MainCamera()),
                    {.feet = walker_.Position(),
                     .stepped = afoot && walker_.Stepped(),
                     .running = visit_.running,
                     .paused = visit_.screen == VisitScreen::Paused},
                    dt);
}

}  // namespace zoogame
