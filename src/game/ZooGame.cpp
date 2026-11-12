// SPDX-License-Identifier: MIT
#include "ZooGame.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <stdexcept>

#include "zoo/sim/Scenario.hpp"

#include "CnaEngine/App/AssetDirectory.hpp"
#include "CnaEngine/Enhanced/PbrMaterialBinder.hpp"
#include "CnaEngine/Enhanced/ShadowReception.hpp"

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

ZooGame::~ZooGame() = default;

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
        return Parse(arguments[++index], speed_);
    if (name == "--list-animals") {
        listAnimals_ = true;
        return true;
    }
    if (name == "--empty") {
        emptyMap_ = true;
        return true;
    }
    if (name == "--view" && hasValue) {
        // x,z,yaw degrees,pitch degrees,distance
        std::string_view rest = arguments[++index];
        float values[5]{};
        for (float& value : values) {
            const std::size_t comma = rest.find(',');
            if (!Parse(rest.substr(0, comma), value))
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

    zoo::ZooConfig config;
    config.world.seed = seed_;
    config.seed = seed_;
    zoo_ = std::make_unique<zoo::Zoo>(*catalog_, *species_, config);
    if (!emptyMap_) {
        const zoo::ScenarioReport report = zoo::BuildStarterZoo(*zoo_);
        std::printf("zoo: starter zoo: %d enclosures, %d animals, %d buildings, %d props\n",
                    report.enclosures, report.animals, report.buildings, report.props);
        for (const std::string& problem : report.problems)
            std::printf("zoo: starter zoo: %s\n", problem.c_str());
    }
    for (std::int64_t i = 0; i < warmUpTicks_; ++i)
        zoo_->Step();
    zoo_->DrainEvents(events_);
    events_.clear();

    surfaces_ = std::make_unique<Surfaces>(device);
    terrain_ = std::make_unique<TerrainRenderer>(device, *surfaces_);
    terrain_->Build(zoo_->Map());
    std::printf("zoo: ground painted in %.2f s\n", terrain_->LastPaintSeconds());
    scenery_ = std::make_unique<SceneryRenderer>(device, *surfaces_, *catalog_);
    scenery_->Update(zoo_->Map(), zoo_->Objects(), -1);
    std::printf("zoo: scenery: %zu triangles in %zu draw calls\n", scenery_->Triangles(),
                scenery_->DrawCalls());

    animals_ = std::make_unique<AnimalRenderer>(getContentProperty(), *species_);
    animals_->Update(*zoo_, 0.0f, 0.0f);
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

    sky_ = std::make_unique<Graphics::SkyDome>(device);
    sky_->AttachTo(Renderer());
    clock_.latitudeDegrees = 48.0;
    (void)Enhanced::EnableShadows(*this, {.size = 4096, .radius = 90.0f, .ahead = 0.7f});
    UpdateShadowArea();

    const zoo::Vec2 entrance = zoo::World::TileCenter(zoo_->Map().EntranceTile());
    camera_.SetOverview({entrance.x, entrance.z - 30.0f}, 3.14159265f, 0.75f, 70.0f);
    if (startView_)
        camera_.SetOverview({startView_->x, startView_->z}, startView_->yaw, startView_->pitch,
                            startView_->distance);
    UpdateSky();
}

void ZooGame::UpdateShadowArea() {
    // The sun's shadow map covers the ground the player looks at: little of it,
    // finely, from close by; much of it, coarsely, from high above.
    if (camera_.Mode() == CameraMode::Walk) {
        constexpr float kReach = 45.0f;
        const Vector3 ahead = Vector3::Normalize(camera_.Target() - camera_.Position());
        SetShadowArea(camera_.Position() + Vector3(ahead.X, 0.0f, ahead.Z) * (kReach * 0.7f), kReach);
        return;
    }
    SetShadowArea(camera_.Target(), std::clamp(camera_.Distance() * 1.15f, 35.0f, 420.0f));
}

void ZooGame::StepSimulation(double seconds) {
    tickDebt_ += seconds * static_cast<double>(zoo::kTicksPerSecond) * static_cast<double>(speed_);
    // A slow frame must not snowball: what cannot be caught up with is dropped.
    const int most = 4 * std::max(speed_, 1);
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
    clock_.hours = startHour_ ? *startHour_
                              : static_cast<double>(now.Hour()) +
                                    tickDebt_ / static_cast<double>(zoo::kTicksPerGameHour);
    // The game's year begins on the first of March.
    clock_.dayOfYear = std::fmod(59.0 + static_cast<double>(now.YearFraction()) * 365.0, 365.0);
    const Vector3 sun = clock_.DirectionToSun();
    const auto colors = palette_.At(static_cast<float>(clock_.Sun().elevationDegrees));
    sky_->SetColors(colors, sun);
    Graphics::ApplySkyLighting(Renderer().Lighting(), colors, sun, 250.0f, 2600.0f);
}

void ZooGame::UpdateFrame(double seconds) {
    controls_.Begin(CurrentInput());
    if (controls_.KeyboardFree()) {
        if (controls_.Pressed(Keys::Space))
            speed_ = speed_ == 0 ? 1 : 0;
        if (controls_.Pressed(Keys::D1))
            speed_ = 1;
        if (controls_.Pressed(Keys::D2))
            speed_ = 2;
        if (controls_.Pressed(Keys::D3))
            speed_ = 4;
    }
    StepSimulation(seconds);
    const auto dt = static_cast<float>(seconds);
    camera_.Update(controls_, dt, zoo_->Map());
    camera_.Apply(MainCamera());
    UpdateShadowArea();
    terrain_->Update(zoo_->Map());
    scenery_->Update(zoo_->Map(), zoo_->Objects());
    animals_->Update(*zoo_, dt * static_cast<float>(speed_), static_cast<float>(tickDebt_));
    UpdateSky();
}

void ZooGame::Submit(App::Frame& frame) {
    terrain_->Submit(frame);
    scenery_->Submit(frame);
    animals_->Submit(frame);
}

}  // namespace zoogame
