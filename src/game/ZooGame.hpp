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
#include "Hud.hpp"
#include "InputScript.hpp"
#include "Play.hpp"
#include "SceneryRenderer.hpp"
#include "Surfaces.hpp"
#include "TerrainRenderer.hpp"
#include "Tools.hpp"
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
    void DrawOverlay(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device) override;
    /// Escape belongs to the game (it puts the tool down, opens the menu), not to the engine.
    void OnQuit() override {}

private:
    /// Makes `zoo` the game being played: the world is drawn anew, the interface starts afresh.
    void Adopt(std::unique_ptr<zoo::Zoo> zoo);
    /// Carries out what the interface asked for this frame (saving, loading, a new zoo, quitting).
    void HandleRequest();
    void SaveTo(const std::string& path, bool quietly);
    void UpdateCameraMode();
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
    /// Scripted input of an automated run (--script).
    std::optional<InputScript> script_;
    std::vector<std::string> shots_;
    /// The palette open at the start (--palette, for screenshots).
    Palette startPalette_ = Palette::None;
    /// --menu: the title screen even in an automated run.
    bool startAtMenu_ = false;
    /// The month an autosave was last written in.
    std::int64_t autosaveMonth_ = -1;

    // The simulation. The catalogs outlive the zoo, which refers to them.
    std::unique_ptr<zoo::Catalog> catalog_;
    std::unique_ptr<zoo::SpeciesCatalog> species_;
    std::unique_ptr<zoo::Zoo> zoo_;
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

    std::unique_ptr<ui::Canvas> canvas_;
    std::unique_ptr<ui::Ui> ui_;
    Hud hud_;
    Play play_;
    std::unique_ptr<WorldTools> tools_;
};

}  // namespace zoogame
