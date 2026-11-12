// SPDX-License-Identifier: MIT
#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "CnaEngine/App/EngineGame.hpp"
#include "CnaEngine/Graphics/Sky.hpp"

#include "AnimalRenderer.hpp"
#include "CameraRig.hpp"
#include "Controls.hpp"
#include "SceneryRenderer.hpp"
#include "Surfaces.hpp"
#include "TerrainRenderer.hpp"
#include "zoo/sim/Zoo.hpp"

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
    void StepSimulation(double seconds);
    void UpdateSky();
    void UpdateShadowArea();

    // Command line.
    std::uint64_t seed_ = 1;
    std::optional<double> startHour_;
    struct View {
        float x, z, yaw, pitch, distance;
    };
    std::optional<View> startView_;
    /// Ticks simulated before the first frame (screenshots of a zoo in full swing).
    std::int64_t warmUpTicks_ = 0;
    bool emptyMap_ = false;
    bool listAnimals_ = false;

    // The simulation. The catalogs outlive the zoo, which refers to them.
    std::unique_ptr<zoo::Catalog> catalog_;
    std::unique_ptr<zoo::SpeciesCatalog> species_;
    std::unique_ptr<zoo::Zoo> zoo_;
    /// Game speed: ticks per tick of real time (0 = paused).
    int speed_ = 1;
    double tickDebt_ = 0.0;
    std::vector<zoo::Event> events_;

    std::unique_ptr<Surfaces> surfaces_;
    std::unique_ptr<TerrainRenderer> terrain_;
    std::unique_ptr<SceneryRenderer> scenery_;
    std::unique_ptr<AnimalRenderer> animals_;
    std::unique_ptr<CnaEngine::Graphics::SkyDome> sky_;
    CnaEngine::Graphics::DayNightPalette palette_ = CnaEngine::Graphics::DayNightPalette::Default();
    CnaEngine::Graphics::TimeOfDay clock_;

    Controls controls_;
    CameraRig camera_;
};

}  // namespace zoogame
