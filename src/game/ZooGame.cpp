// SPDX-License-Identifier: MIT
#include "ZooGame.hpp"

#include <algorithm>
#include <charconv>
#include <cstdio>

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

    zoo::WorldGenParams params;
    params.seed = seed_;
    world_ = std::make_unique<zoo::World>(zoo::World::Generate(params));

    surfaces_ = std::make_unique<Surfaces>(device);
    terrain_ = std::make_unique<TerrainRenderer>(device, *surfaces_);
    terrain_->Build(*world_);
    std::printf("zoo: ground painted in %.2f s\n", terrain_->LastPaintSeconds());

    sky_ = std::make_unique<Graphics::SkyDome>(device);
    sky_->AttachTo(Renderer());
    clock_.latitudeDegrees = 48.0;
    clock_.dayOfYear = 150.0;
    clock_.hours = startHour_.value_or(10.5);
    (void)Enhanced::EnableShadows(*this, {.size = 4096, .radius = 90.0f, .ahead = 0.7f});
    UpdateShadowArea();

    const zoo::Vec2 entrance = zoo::World::TileCenter(world_->EntranceTile());
    camera_.SetOverview({entrance.x, entrance.z - 30.0f}, 3.14159265f, 0.75f, 70.0f);
    if (startView_)
        camera_.SetOverview({startView_->x, startView_->z}, startView_->yaw, startView_->pitch,
                            startView_->distance);
    UpdateSky(0.0);
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

void ZooGame::UpdateSky(double seconds) {
    clock_.Advance(seconds, 120.0);
    const Vector3 sun = clock_.DirectionToSun();
    const auto colors = palette_.At(static_cast<float>(clock_.Sun().elevationDegrees));
    sky_->SetColors(colors, sun);
    Graphics::ApplySkyLighting(Renderer().Lighting(), colors, sun, 250.0f, 2600.0f);
}

void ZooGame::UpdateFrame(double seconds) {
    controls_.Begin(CurrentInput());
    const auto dt = static_cast<float>(seconds);
    camera_.Update(controls_, dt, *world_);
    camera_.Apply(MainCamera());
    UpdateShadowArea();
    terrain_->Update(*world_);
    UpdateSky(startHour_ ? 0.0 : seconds);
}

void ZooGame::Submit(App::Frame& frame) {
    terrain_->Submit(frame);
}

}  // namespace zoogame
