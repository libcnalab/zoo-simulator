// SPDX-License-Identifier: MIT
#pragma once

#include <memory>
#include <optional>
#include <string>

#include "CnaEngine/App/EngineGame.hpp"
#include "CnaEngine/Graphics/Sky.hpp"

#include "CameraRig.hpp"
#include "Controls.hpp"
#include "Surfaces.hpp"
#include "TerrainRenderer.hpp"
#include "zoo/world/World.hpp"

namespace zoogame {

/// The game: owns the simulation, draws it and turns input into commands.
class ZooGame final : public CnaEngine::App::EngineGame {
public:
    ZooGame();
    ~ZooGame() override;

protected:
    bool ParseArgument(std::span<const std::string_view> arguments, std::size_t& index) override;
    void LoadGame() override;
    void UpdateFrame(double seconds) override;
    void Submit(CnaEngine::App::Frame& frame) override;

private:
    void UpdateSky(double seconds);
    void UpdateShadowArea();

    // Command line.
    std::uint64_t seed_ = 1;
    std::optional<double> startHour_;
    struct View {
        float x, z, yaw, pitch, distance;
    };
    std::optional<View> startView_;

    std::unique_ptr<zoo::World> world_;

    std::unique_ptr<Surfaces> surfaces_;
    std::unique_ptr<TerrainRenderer> terrain_;
    std::unique_ptr<CnaEngine::Graphics::SkyDome> sky_;
    CnaEngine::Graphics::DayNightPalette palette_ = CnaEngine::Graphics::DayNightPalette::Default();
    CnaEngine::Graphics::TimeOfDay clock_;

    Controls controls_;
    CameraRig camera_;
};

}  // namespace zoogame
