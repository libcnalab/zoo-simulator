// SPDX-License-Identifier: MIT
#include "ZooGame.hpp"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <stdexcept>
#include <utility>

#include "Saves.hpp"
#include "zoo/sim/Scenario.hpp"
#include "zoo/sim/ZooSave.hpp"

#include "CnaEngine/App/AssetDirectory.hpp"
#include "CnaEngine/Enhanced/PbrMaterialBinder.hpp"
#include "CnaEngine/Enhanced/RelativeMouseCapture.hpp"
#include "CnaEngine/Enhanced/ShadowReception.hpp"

#include "Microsoft/Xna/Framework/GamerServices/GamerServicesDispatcher.hpp"

namespace zoogame {

namespace {

using namespace CnaEngine;

App::GameOptions MakeOptions() {
    App::GameOptions options;
    options.title = "Zoo Simulator";
    options.application = "zoo-simulator";
    options.fieldOfViewDegrees = 50.0f;
    options.nearPlane = 0.3f;
    options.farPlane = 3000.0f;
    return options;
}

template <typename T>
bool Parse(std::string_view text, T& out) {
    const auto result = std::from_chars(text.data(), text.data() + text.size(), out);
    return result.ec == std::errc() && result.ptr == text.data() + text.size();
}

}  // namespace

ZooGame::ZooGame() : EngineGame(MakeOptions()) {}

ZooGame::~ZooGame() {
    if (!editor_ && zoo_ && visitors_ && (Automation().Options().Automated() || script_))
        PrintVisitFigures("at the end");
    bench_.Print(standAt_.empty() ? "entrance" : standAt_.c_str());
}

bool ZooGame::ParseArgument(std::span<const std::string_view> arguments, std::size_t& index) {
    const std::string_view name = arguments[index];
    const bool hasValue = index + 1 < arguments.size();
    if (name == "--seed" && hasValue)
        return Parse(arguments[++index], seed_);
    if (name == "--hour" && hasValue) {
        double hour = 0.0;
        if (!Parse(arguments[++index], hour))
            return false;
        startHour_ = hour;
        return true;
    }
    if (name == "--ticks" && hasValue)
        return Parse(arguments[++index], warmUpTicks_);
    if (name == "--speed" && hasValue)
        return Parse(arguments[++index], play_.speed);
    if (name == "--stand" && hasValue) {
        standAt_ = arguments[++index];
        return true;
    }
    if (name == "--bench") {
        bench_.on = true;
        return true;
    }
    if (name == "--without" && hasValue) {
        const std::string_view what = arguments[++index];
        bool* part = what == "animals"      ? &without_.animals
                     : what == "people"     ? &without_.people
                     : what == "scenery"    ? &without_.scenery
                     : what == "terrain"    ? &without_.terrain
                     : what == "shadows"    ? &without_.shadows
                     : what == "simulation" ? &without_.simulation
                     : what == "flat"       ? &without_.flatPeople
                                            : nullptr;
        if (part == nullptr)
            return false;
        *part = true;
        return true;
    }
    if (name == "--people" && hasValue)
        return Parse(arguments[++index], peopleBudget_);
    if (name == "--list-animals") {
        listAnimals_ = true;
        return true;
    }
    if (name == "--editor") {
        editor_ = true;
        return true;
    }
    if (name == "--empty") {
        emptyMap_ = true;
        return true;
    }
    if (name == "--menu") {
        startAtMenu_ = true;
        return true;
    }
    if (name == "--saves" && hasValue) {
        SetSaveDirectory(std::string(arguments[++index]));
        ownSaves_ = true;
        return true;
    }
    if (name == "--script" && hasValue) {
        std::string error;
        script_ = InputScript::Load(std::string(arguments[++index]), error);
        if (!script_)
            std::printf("zoo: script: %s\n", error.c_str());
        return script_.has_value();
    }
    if (name == "--palette" && hasValue) {
        static constexpr std::string_view kNames[] = {"none",  "paths",     "fences",  "ground",
                                                      "water", "buildings", "scenery", "animals"};
        const std::string_view value = arguments[++index];
        for (std::size_t i = 0; i < std::size(kNames); ++i) {
            if (value == kNames[i]) {
                startPalette_ = static_cast<Palette>(i);
                return true;
            }
        }
        return false;
    }
    if (name == "--view" && hasValue) {
        // x,z,yaw degrees,pitch degrees[,distance]: the overhead camera of the editor;
        // on a visit where the visitor stands and looks (no distance then).
        std::string_view rest = arguments[++index];
        float values[5]{0.0f, 0.0f, 180.0f, 0.0f, 60.0f};
        for (std::size_t i = 0; i < std::size(values); ++i) {
            if (i == 4 && rest.empty())
                break;
            const std::size_t comma = rest.find(',');
            if (!Parse(rest.substr(0, comma), values[i]))
                return false;
            rest = comma == std::string_view::npos ? std::string_view() : rest.substr(comma + 1);
        }
        constexpr float kRadians = 3.14159265f / 180.0f;
        startView_ = View{values[0], values[1], values[2] * kRadians, values[3] * kRadians, values[4]};
        return true;
    }
    return false;
}

void ZooGame::LoadGame() {
    auto& device = getGraphicsDeviceProperty();
    Renderer().SetMaterialBinder(
        std::make_shared<Enhanced::PbrMaterialBinder>(device, Renderer().StockBinder()));
    Renderer().Lighting().defaultLighting = false;

    const auto data = App::FindAssetDirectory("data");
    if (!data)
        throw std::runtime_error("zoo: the data directory is missing beside the executable");
    catalog_ = std::make_unique<zoo::Catalog>(zoo::Catalog::LoadDirectory((*data / "construction").string()));
    species_ = std::make_unique<zoo::SpeciesCatalog>(
        zoo::SpeciesCatalog::LoadDirectory((*data / "species").string()));
    for (const std::string& error : catalog_->errors)
        std::printf("zoo: data: %s\n", error.c_str());

    // The canvas first: building and filling the zoo takes seconds, and a window that
    // says so is better than a black one.
    surfaces_ = std::make_unique<Surfaces>(device);
    const auto assets = App::FindAssetDirectory("assets");
    if (!assets)
        throw std::runtime_error("zoo: the assets directory is missing beside the executable");
    canvas_ = std::make_unique<ui::Canvas>(device, getContentProperty(), *assets);
    ui_ = std::make_unique<ui::Ui>(*canvas_, controls_);
    ShowLoading("Building the habitats and planting the park", 0.05f);

    std::unique_ptr<zoo::Zoo> first;
    if (editor_) {
        zoo::ZooConfig config;
        config.world.seed = seed_;
        config.seed = seed_;
        first = std::make_unique<zoo::Zoo>(*catalog_, *species_, config);
        if (!emptyMap_) {
            const zoo::ScenarioReport report = zoo::BuildStarterZoo(*first);
            std::printf("zoo: starter zoo: %d enclosures, %d animals, %d buildings, %d props\n",
                        report.enclosures, report.animals, report.buildings, report.props);
            for (const std::string& problem : report.problems)
                std::printf("zoo: starter zoo: %s\n", problem.c_str());
        }
        for (std::int64_t i = 0; i < warmUpTicks_; ++i)
            first->Step();
    } else {
        first = MakeVisitorZoo();
    }

    ShowLoading("Laying the paths and the lawns", 0.7f);
    sounds_ = std::make_unique<Soundscape>(*assets);
    sky_ = std::make_unique<Graphics::SkyDome>(device);
    sky_->AttachTo(Renderer());
    clock_.latitudeDegrees = 48.0;
    if (!without_.shadows)
        (void)Enhanced::EnableShadows(*this, {.size = 4096, .radius = 90.0f, .ahead = 0.7f});

    // Visitors are CNA avatars, which belong to its gamer services. Avatars
    // draw themselves, so they get a pass of their own after the opaque scene.
    using Microsoft::Xna::Framework::GamerServices::GamerServicesDispatcher;
    if (!GamerServicesDispatcher::getIsInitializedProperty())
        GamerServicesDispatcher::Initialize(getServicesProperty());
    visitors_ = std::make_unique<VisitorRenderer>();
    // From the ground one looks along whole avenues of people: the nearest are drawn.
    // On foot the nearest are avatars and everybody behind them is drawn flat; from above
    // (the editor) there are only avatars, and no more than the budget.
    visitors_->SetFlat(!editor_ && !without_.flatPeople);
    visitors_->SetBudget(peopleBudget_ > 0 ? peopleBudget_ : editor_ ? 400 : without_.flatPeople ? 120 : 40);
    Renderer().AddPass(Graphics::PassPoint::AfterOpaque, "visitors",
                       [this](auto&, const Graphics::Camera& camera, Graphics::RenderStatistics&) {
                           if (without_.people)
                               return;
                           const Bench::Scope timed(bench_, Bench::PeoplePass);
                           visitors_->Draw(getGraphicsDeviceProperty(), camera, Renderer().Lighting());
                       });

    const int speed = play_.speed;
    Adopt(std::move(first));
    play_.speed = speed;
    std::printf("zoo: ground painted in %.2f s\n", terrain_->LastPaintSeconds());
    std::printf("zoo: scenery: %zu triangles in %zu draw calls\n", scenery_->Triangles(),
                scenery_->DrawCalls());
    if (bench_.on)
        std::printf("zoo: scenery: triangles by kind (count x each)\n%s", scenery_->Tally(zoo_->Objects()).c_str());
    if (listAnimals_) {
        for (const zoo::Animal& animal : zoo_->Fauna().All()) {
            std::printf(
                "zoo: animal %u %s %s at %.1f,%.1f heading %.0f %s%s\n", animal.id.value,
                species_->All()[static_cast<std::size_t>(animal.species)].id.c_str(), animal.name.c_str(),
                static_cast<double>(animal.position.x), static_cast<double>(animal.position.z),
                static_cast<double>(animal.heading * 57.29578f),
                std::string(zoo::ToString(animal.activity)).c_str(), animal.shelter ? " (in shelter)" : "");
        }
    }

    const bool toTitle = startAtMenu_ || (!Automation().Options().Automated() && !script_);
    if (!editor_) {
        // A visit: the mouse looks around without a button held. A player begins at
        // the title, the zoo at eye level behind it; an automated run is already walking.
        SetMouseCapture(std::make_unique<Enhanced::RelativeMouseCapture>());
        ReadVisitSettings();
        ApplyVisitSettings(false);
        freshZoo_ = true;
        std::error_code ignored;
        visit_.canContinue = std::filesystem::exists(SavePath(kContinueTitle), ignored);
        Pose pose = startView_ ? Pose{startView_->x, startView_->z, startView_->yaw, startView_->pitch}
                               : EntrancePose();
        if (!standAt_.empty()) {
            if (const auto stand = StandPose(standAt_))
                pose = *stand;
            else
                std::fprintf(stderr, "zoo: --stand %s: there is no such place\n", standAt_.c_str());
            // Who could be stood in front of: an animal indoors is not to be seen.
            for (const zoo::Enclosure& enclosure : zoo_->Habitats().All()) {
                int lives = 0;
                int outdoors = 0;
                for (const zoo::Animal& animal : zoo_->Fauna().All()) {
                    if (!(animal.home == enclosure.id))
                        continue;
                    ++lives;
                    outdoors += animal.Visible() ? 1 : 0;
                }
                std::fprintf(stderr, "zoo:   %s: %d animals, %d outdoors\n", enclosure.name.c_str(), lives,
                             outdoors);
            }
        }
        BeginWalk(pose);
        if (toTitle) {
            visit_.screen = VisitScreen::Title;
            freshZoo_ = true;
        }
        camera_.SetEye(walker_.Eye(), walker_.Yaw(), walker_.Pitch());
        camera_.Apply(MainCamera());
        UpdateShadowArea();
        UpdateSky();
        return;
    }
    if (startView_)
        camera_.SetOverview({startView_->x, startView_->z}, startView_->yaw, startView_->pitch,
                            startView_->distance);
    UpdateShadowArea();
    UpdateSky();
    OpenPalette(play_, startPalette_);
    // A player begins at the title screen, with the starter zoo as its backdrop; an
    // automated run goes straight into the game.
    if (toTitle) {
        play_.mainMenu = true;
        play_.started = false;
    }
}

void ZooGame::ShowLoading(std::string_view what, float done) {
    if (!canvas_)
        return;
    auto& device = getGraphicsDeviceProperty();
    device.Clear(Microsoft::Xna::Framework::Color(18, 28, 24));
    canvas_->Begin();
    const Microsoft::Xna::Framework::Vector2 screen = canvas_->Size();
    const float left = std::round(0.09f * screen.X) + 40.0f;
    const float top = std::round(0.16f * screen.Y);
    canvas_->Text(editor_ ? "Zoo Simulator" : "Greenhill Zoo", {left, top}, 36.0f, ui::theme::kText, true);
    canvas_->Fill({left, top + 48.0f, 64.0f, 3.0f}, ui::theme::kAccent);
    canvas_->Text(what, {left, top + 70.0f}, ui::theme::kBody, ui::theme::kMuted);
    const ui::Box bar{left, top + 104.0f, 300.0f, 6.0f};
    canvas_->Round(bar, 3.0f, ui::Rgb(255, 255, 255, 40));
    canvas_->Round({bar.x, bar.y, std::round(bar.w * std::clamp(done, 0.0f, 1.0f)), bar.h}, 3.0f,
                   ui::theme::kAccent);
    canvas_->Flush();
    device.Present();
}

void ZooGame::Adopt(std::unique_ptr<zoo::Zoo> zoo) {
    auto& device = getGraphicsDeviceProperty();
    // What draws the old zoo goes before the zoo it refers to.
    tools_.reset();
    if (sounds_)
        sounds_->Reset();
    animals_.reset();
    scenery_.reset();
    terrain_.reset();
    zoo_ = std::move(zoo);
    zoo_->DrainEvents(events_);
    events_.clear();
    tickDebt_ = 0.0;
    autosaveMonth_ = zoo_->Time().Day() / zoo::kDaysPerMonth;

    terrain_ = std::make_unique<TerrainRenderer>(device, *surfaces_);
    terrain_->Build(zoo_->Map());
    // Nothing is built during a visit: the whole zoo is one cell there (a quarter of the draw
    // calls of the editor's small cells, which an edit rebuilds quickly).
    scenery_ = std::make_unique<SceneryRenderer>(
        device, *surfaces_, *catalog_,
        editor_ ? SceneryRenderer::kEditCellTiles : std::max(zoo_->Map().Width(), zoo_->Map().Depth()));
    scenery_->Update(zoo_->Map(), zoo_->Objects(), -1);
    animals_ = std::make_unique<AnimalRenderer>(getContentProperty(), *species_);
    animals_->Update(*zoo_, 0.0f, 0.0f);
    visitors_->Clear();
    visitors_->Update(*zoo_, 0.0f, 0.0f);
    tools_ = std::make_unique<WorldTools>(device, *surfaces_, *catalog_);

    camera_.SetMode(CameraMode::Overview, zoo_->Map());
    const zoo::Vec2 entrance = zoo::World::TileCenter(zoo_->Map().EntranceTile());
    camera_.SetOverview({entrance.x, entrance.z - 30.0f}, 3.14159265f, 0.75f, 70.0f);

    play_ = Play{};
    play_.zoo = zoo_.get();
    play_.catalog = catalog_.get();
    play_.species = species_.get();
    play_.camera = &camera_;
    play_.animals = animals_.get();
    hud_ = Hud();
}

void ZooGame::SaveTo(const std::string& path, bool quietly) {
    std::error_code ignored;
    std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ignored);
    std::string error;
    if (!zoo::SaveZoo(*zoo_, path, &error)) {
        play_.Flash("The zoo could not be saved: " + error);
        std::printf("zoo: save failed: %s\n", error.c_str());
        return;
    }
    std::printf("zoo: saved %s\n", path.c_str());
    if (!quietly)
        play_.Tell("Saved");
}

void ZooGame::HandleRequest() {
    const Request request = std::exchange(play_.request, Request::None);
    switch (request) {
        case Request::None:
            break;
        case Request::Save:
            SaveTo(play_.requestPath, false);
            break;
        case Request::Load: {
            std::string error;
            std::unique_ptr<zoo::Zoo> loaded = zoo::LoadZoo(play_.requestPath, *catalog_, *species_, &error);
            if (!loaded) {
                play_.Flash("The zoo could not be loaded: " + error);
                std::printf("zoo: load failed: %s\n", error.c_str());
                break;
            }
            std::printf("zoo: loaded %s\n", play_.requestPath.c_str());
            Adopt(std::move(loaded));
            play_.Tell(Format("Welcome back to %s", zoo_->Name().c_str()));
            break;
        }
        case Request::NewGame: {
            // By value: Adopt() starts the shared state afresh.
            const zoo::ZooConfig config = play_.newGame;
            const bool starter = play_.newGameStarter;
            auto fresh = std::make_unique<zoo::Zoo>(*catalog_, *species_, config);
            if (starter)
                (void)zoo::BuildStarterZoo(*fresh);
            std::printf("zoo: new zoo \"%s\" seed %llu\n", config.name.c_str(),
                        static_cast<unsigned long long>(config.seed));
            Adopt(std::move(fresh));
            break;
        }
        case Request::Quit:
            Exit();
            break;
        case Request::MainMenu:
            play_.mainMenu = true;
            play_.dialog = Dialog::None;
            play_.palette = Palette::None;
            play_.Use(ToolKind::Inspect);
            play_.walk = false;
            play_.follow = false;
            break;
    }
}

void ZooGame::UpdateCameraMode() {
    CameraMode mode = CameraMode::Overview;
    if (!play_.mainMenu) {
        if (play_.follow && play_.selection.kind == Picked::Animal) {
            const zoo::Animal* animal = zoo_->Fauna().Find(play_.selection.animal);
            const auto position = animals_->PositionOf(play_.selection.animal);
            if (animal != nullptr && position) {
                const zoo::SpeciesDef& species = species_->All()[static_cast<std::size_t>(animal->species)];
                camera_.SetFollowTarget(*position, std::max(species.bodyLengthM, species.shoulderHeightM));
                mode = CameraMode::Follow;
            } else {
                play_.follow = false;
            }
        } else {
            play_.follow = false;
        }
        if (mode != CameraMode::Follow && play_.walk)
            mode = CameraMode::Walk;
    }
    if (mode != camera_.Mode())
        camera_.SetMode(mode, zoo_->Map());
}

void ZooGame::UpdateShadowArea() {
    // The sun's shadow map covers the ground the player looks at: little of it,
    // finely, from close by; much of it, coarsely, from high above.
    if (camera_.Mode() == CameraMode::Walk || camera_.Mode() == CameraMode::Eye) {
        constexpr float kReach = 45.0f;
        const Vector3 ahead = Vector3::Normalize(camera_.Target() - camera_.Position());
        SetShadowArea(camera_.Position() + Vector3(ahead.X, 0.0f, ahead.Z) * (kReach * 0.7f), kReach);
        return;
    }
    SetShadowArea(camera_.Target(), std::clamp(camera_.Distance() * 1.15f, 35.0f, 420.0f));
}

void ZooGame::StepSimulation(double seconds) {
    tickDebt_ += seconds * static_cast<double>(zoo::kTicksPerSecond) * static_cast<double>(play_.speed);
    // A slow frame must not snowball: what cannot be caught up with is dropped.
    const int most = 4 * std::max(play_.speed, 1);
    int done = 0;
    while (tickDebt_ >= 1.0 && done < most) {
        zoo_->Step();
        tickDebt_ -= 1.0;
        ++done;
    }
    if (tickDebt_ >= 1.0)
        tickDebt_ = 0.0;
    events_.clear();
    zoo_->DrainEvents(events_);
}

void ZooGame::UpdateSky() {
    const zoo::GameTime now = zoo_->Time();
    // On a visit the calendar creeps (ZooConfig::dayPace): no fraction of a tick to add.
    clock_.hours = startHour_ ? *startHour_
                              : static_cast<double>(now.Hour()) +
                                    (editor_ ? tickDebt_ / static_cast<double>(zoo::kTicksPerGameHour) : 0.0);
    // The game's year begins on the first of March.
    clock_.dayOfYear = std::fmod(59.0 + static_cast<double>(now.YearFraction()) * 365.0, 365.0);
    const Vector3 sun = clock_.DirectionToSun();
    const auto colors = palette_.At(static_cast<float>(clock_.Sun().elevationDegrees));
    sky_->SetColors(colors, sun);
    Graphics::ApplySkyLighting(Renderer().Lighting(), colors, sun, 250.0f, 2600.0f);
}

void ZooGame::UpdateFrame(double seconds) {
    bench_.Frame();
    bench_.Drawn(RenderedLastFrame().drawCalls, RenderedLastFrame().primitives);
    const double before = bench_.Timed();
    const auto began = Bench::Clock::now();
    if (editor_)
        UpdateEditor(seconds);
    else
        UpdateVisit(seconds);
    // What the update took beyond its timed parts.
    const double took = std::chrono::duration<double>(Bench::Clock::now() - began).count();
    bench_.Add(Bench::Rest, took - (bench_.Timed() - before));
}

void ZooGame::UpdateEditor(double seconds) {
    const auto dt = static_cast<float>(seconds);
    if (script_) {
        shots_.clear();
        controls_.Begin(script_->Next(shots_));
        for (const std::string& path : shots_)
            Screenshots().Request({.path = path, .opaque = true});
    } else {
        controls_.Begin(CurrentInput());
    }
    ui_->Begin(dt);
    hud_.Build(*ui_, play_);
    const bool playing = !play_.mainMenu;
    if (!playing) {
        play_.hint.clear();
        play_.flashLeft = std::max(0.0f, play_.flashLeft - dt);
        if (controls_.KeyboardFree() && controls_.Pressed(Keys::Escape))
            play_.dialog = Dialog::None;
    }
    if (playing && controls_.KeyboardFree() && !IsModal(play_.dialog)) {
        if (controls_.Pressed(Keys::Tab)) {
            play_.walk = !play_.walk;
            play_.follow = false;
        }
        if (controls_.Pressed(Keys::F5))
            SaveTo(SavePath("quicksave").string(), false);
    }
    if (playing && controls_.KeyboardFree() && !IsModal(play_.dialog)) {
        if (controls_.Pressed(Keys::Space))
            play_.speed = play_.speed == 0 ? 1 : 0;
        if (controls_.Pressed(Keys::D1))
            play_.speed = 0;
        if (controls_.Pressed(Keys::D2))
            play_.speed = 1;
        if (controls_.Pressed(Keys::D3))
            play_.speed = 3;
        if (controls_.Pressed(Keys::D4))
            play_.speed = 8;
    }
    const auto& viewport = getGraphicsDeviceProperty().getViewportProperty();
    if (playing)
        tools_->Update(play_, controls_, MainCamera(),
                       Vector2(static_cast<float>(viewport.getWidthProperty()),
                               static_cast<float>(viewport.getHeightProperty())),
                       dt);
    ui_->End();
    HandleRequest();
    // The title screen shows the zoo alive behind it; a window that takes over the screen stops it.
    if (play_.mainMenu ? play_.dialog == Dialog::None : !IsModal(play_.dialog))
        StepSimulation(seconds);
    // An autosave at the turn of every month.
    const std::int64_t month = zoo_->Time().Day() / zoo::kDaysPerMonth;
    if (month != autosaveMonth_) {
        autosaveMonth_ = month;
        if (!Automation().Options().Automated() && !script_)
            SaveTo(SavePath(kAutosaveTitle).string(), true);
    }
    UpdateCameraMode();
    camera_.Update(controls_, dt, zoo_->Map());
    camera_.Apply(MainCamera());
    UpdateShadowArea();
    terrain_->Update(zoo_->Map());
    scenery_->Update(zoo_->Map(), zoo_->Objects());
    animals_->Update(*zoo_, dt * static_cast<float>(play_.speed), static_cast<float>(tickDebt_));
    visitors_->Update(*zoo_, dt * static_cast<float>(play_.speed), static_cast<float>(tickDebt_));
    visitors_->Prepare(getGraphicsDeviceProperty());
    UpdateSky();
}

void ZooGame::Submit(App::Frame& frame) {
    const Bench::Scope timed(bench_, Bench::Submit);
    if (!without_.terrain)
        terrain_->Submit(frame);
    if (!without_.scenery)
        scenery_->Submit(frame);
    if (!without_.animals)
        animals_->Submit(frame);
    if (editor_ && !play_.mainMenu)
        tools_->Submit(frame);
}

void ZooGame::DrawOverlay(Microsoft::Xna::Framework::Graphics::GraphicsDevice&) {
    canvas_->Flush();
}

}  // namespace zoogame
