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
#include "Visit.hpp"
#include "VisitorHud.hpp"
#include "VisitorRenderer.hpp"
#include "Walker.hpp"
#include "zoo/sim/Zoo.hpp"

namespace zoogame {

/// The game: owns the simulation, draws it and turns input into commands.
///
/// It is played as a visitor: on foot through a zoo that stands and is full of
/// life (ZooGameVisit.cpp). The management game it grew out of (building,
/// finances, the overhead camera) is the editor behind --editor.
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
    void UpdateEditor(double seconds);

    // The visit (ZooGameVisit.cpp).
    struct Pose {
        float x = 0.0f, z = 0.0f, yaw = 3.14159265f, pitch = 0.0f;
    };
    /// A zoo for a visit: built, at a good hour of a summer's day, its visitors already in.
    std::unique_ptr<zoo::Zoo> MakeVisitorZoo();
    /// Puts a zoo's clock to a visit's pace; with `fill`, lets visitors arrive first.
    void PrepareVisit(zoo::Zoo& zoo, bool fill);
    /// Where a visit begins: just inside the gate, looking into the zoo.
    Pose EntrancePose() const;
    /// --stand habitat:N | board:N: at a viewing spot of the N-th lived-in enclosure, facing
    /// its nearest animal, or in front of the N-th board (for scripted looks at the zoo).
    std::optional<Pose> StandPose(std::string_view what) const;
    void BeginWalk(const Pose& pose);
    void UpdateVisit(double seconds);
    void HandleVisitRequest();
    bool SaveVisit(const std::string& path, bool quietly);
    bool LoadVisit(const std::string& path);
    void KeepVisit();
    void ReadVisitSettings();
    void ApplyVisitSettings(bool store);
    void FillVisitFigures();
    /// What the eyes rest on and what E would do; opens and closes the board and the map.
    void UpdateEncounters(float seconds);
    void LookAtAnimals(float seconds);
    void OpenBoard(const zoo::Prop& board);
    void BuildVisitMap();
    void PrintVisitFigures(const char* when) const;

    // Command line.
    /// --editor: the management game instead of a visit.
    bool editor_ = false;
    /// --saves was given: an automated run may write there.
    bool ownSaves_ = false;
    /// --people N: the most visitors a frame draws (0: the mode's own figure).
    std::size_t peopleBudget_ = 0;
    std::string standAt_;
    /// The animal the eyes rest on, and the board in reach (null: none).
    zoo::AnimalId lookedAt_;
    zoo::PropId boardInReach_;
    zoo::Vec2 boardRead_{};  ///< where the board being read stands
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
    /// Outlives the zoos: its cast of avatars takes a while to assemble.
    std::unique_ptr<VisitorRenderer> visitors_;
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

    // The visit.
    Visit visit_;
    VisitorHud visitorHud_;
    Walker walker_;
    /// Nobody has walked this zoo yet: "Visit Zoo" need not build another.
    bool freshZoo_ = false;
    float titleTime_ = 0.0f;
    double sinceKept_ = 0.0;
};

}  // namespace zoogame
