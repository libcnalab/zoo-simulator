// SPDX-License-Identifier: MIT
//
// The game as it is played: a visit to the zoo on foot.
#include "ZooGame.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "CnaEngine/App/AssetDirectory.hpp"

#include "Saves.hpp"
#include "zoo/base/Version.hpp"
#include "zoo/sim/Scenario.hpp"
#include "zoo/sim/VisitStart.hpp"
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
/// The visitors' pace for a visit on foot (the one zoo::FillForVisit is tested with): a
/// minute or two at a fence, forty minutes in the zoo, and as many arrivals as keep the crowd
/// of mid-morning. While the zoo is filled before the player walks in, people come
/// kFillArrivals times as fast as that.
constexpr zoo::VisitorPace kVisitPeople = zoo::VisitStartOptions{}.pace;
constexpr float kFillArrivals = 1.7f;

/// The visitors' pace for a crowd setting: Normal is kVisitPeople as it is; another crowd
/// holds that many people at most and lets them in in proportion.
zoo::VisitorPace CrowdPace(int level) {
    const int last = static_cast<int>(kCrowdLevels.size()) - 1;
    const CrowdLevel& crowd = kCrowdLevels[static_cast<std::size_t>(std::clamp(level, 0, last))];
    zoo::VisitorPace pace = kVisitPeople;
    if (level != kNormalCrowd) {
        pace.arrivals *= static_cast<float>(crowd.people) / static_cast<float>(kVisitPeople.most);
        pace.most = crowd.people;
    }
    return pace;
}
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

// Bump when BuildVisitorZoo or the fill sequence changes without changing the data or save
// version. Data files and every option that affects the filled state are hashed separately.
constexpr std::uint64_t kVisitCacheRevision = 12;

void CacheHash(std::uint64_t& hash, std::string_view bytes) {
    for (const unsigned char byte : bytes) {
        hash ^= byte;
        hash *= 1099511628211ULL;
    }
}

void CacheHash(std::uint64_t& hash, std::uint64_t value) {
    for (int byte = 0; byte < 8; ++byte) {
        hash ^= static_cast<unsigned char>(value >> (byte * 8));
        hash *= 1099511628211ULL;
    }
}

bool CacheHashFile(std::uint64_t& hash, const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return false;
    }
    std::array<char, 16384> buffer{};
    while (file) {
        file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        CacheHash(hash, std::string_view(buffer.data(), static_cast<std::size_t>(file.gcount())));
    }
    return file.eof();
}

bool CacheHashDefinitions(std::uint64_t& hash, const std::filesystem::path& data) {
    std::vector<std::filesystem::path> files;
    std::error_code failed;
    for (const std::filesystem::path& directory : {data / "construction", data / "species"}) {
        for (std::filesystem::directory_iterator it(directory, failed), end; !failed && it != end;
             it.increment(failed)) {
            if (it->is_regular_file(failed) && it->path().extension() == ".json") {
                files.push_back(it->path());
            }
        }
        if (failed) {
            return false;
        }
    }
    std::sort(files.begin(), files.end());
    for (const std::filesystem::path& file : files) {
        CacheHash(hash, file.filename().string());
        if (!CacheHashFile(hash, file)) {
            return false;
        }
    }
    return true;
}

std::optional<std::filesystem::path> VisitCachePath(std::uint64_t seed, int crowd, float hour,
                                                    std::int64_t ticks,
                                                    const zoo::VisitorZooOptions& zooOptions) {
    const auto data = CnaEngine::App::FindAssetDirectory("data");
    if (!data) {
        return std::nullopt;
    }
    std::uint64_t hash = 14695981039346656037ULL;
    CacheHash(hash, kVisitCacheRevision);
    CacheHash(hash, static_cast<std::uint64_t>(zoo::kSaveVersion));
    CacheHash(hash, zoo::VersionString());
    CacheHash(hash, seed);
    CacheHash(hash, static_cast<std::uint64_t>(crowd));
    CacheHash(hash, static_cast<std::uint64_t>(ticks));
    CacheHash(hash, static_cast<std::uint64_t>(std::bit_cast<std::uint32_t>(hour)));
    for (const std::string& omitted : zooOptions.withoutSpecies) {
        CacheHash(hash, omitted);
    }
    if (!CacheHashDefinitions(hash, *data)) {
        return std::nullopt;
    }
    std::ostringstream name;
    name << "visit-" << std::hex << std::setfill('0') << std::setw(16) << hash << ".zoo-cache";
    return SaveDirectory() / ".cache" / name.str();
}

void PruneVisitCaches(const std::filesystem::path& directory) {
    struct Entry {
        std::filesystem::file_time_type written;
        std::filesystem::path path;
    };
    std::vector<Entry> entries;
    std::error_code failed;
    for (std::filesystem::directory_iterator it(directory, failed), end; !failed && it != end;
         it.increment(failed)) {
        if (!it->is_regular_file(failed) || it->path().extension() != ".zoo-cache") {
            continue;
        }
        entries.push_back({it->last_write_time(failed), it->path()});
    }
    std::sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) {
        return a.written > b.written;
    });
    constexpr std::size_t kRetainedVisitCaches = 8;
    for (std::size_t index = kRetainedVisitCaches; index < entries.size(); ++index) {
        std::filesystem::remove(entries[index].path, failed);
        std::filesystem::remove(entries[index].path.string() + zoo::kSaveSummarySuffix, failed);
    }
}

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
    zoo::VisitorZooOptions options;
    if (!emptyMap_) {
        // A habitat whose animals the game cannot show yet is built and named, and waits for them.
        // Several species deliberately share an existing body plan; the asset key is `model`, not
        // the species id (for example, cheetah currently uses the lion body).
        const auto assets = CnaEngine::App::FindAssetDirectory("assets");
        for (const zoo::SpeciesDef& def : species_->All()) {
            if (!assets || !std::filesystem::exists(*assets / "animals" / (def.model + ".glb")))
                options.withoutSpecies.push_back(def.id);
        }
    }
    const float hour = startHour_ ? static_cast<float>(*startHour_) : kVisitHour;
    const std::int64_t ticks = warmUpTicks_ > 0 ? warmUpTicks_ : kFillTicks;
    const bool automated = Automation().Options().Automated() || script_.has_value();
    const auto cache = !emptyMap_ && (!automated || ownSaves_)
                           ? VisitCachePath(seed_, visit_.settings.crowd, hour, ticks, options)
                           : std::nullopt;
    if (cache) {
        std::string error;
        if (std::unique_ptr<zoo::Zoo> loaded = zoo::LoadZoo(cache->string(), *catalog_, *species_, &error)) {
            // The cache contains a completed visit at the requested hour. Only visitor pace is
            // transient. Preparing a user save here would repeat the fill for evening starts.
            loaded->Guests().SetPace(CrowdPace(visit_.settings.crowd));
            ShowLoading("The morning's visitors are ready", 0.7f);
            const double took =
                std::chrono::duration<double>(std::chrono::steady_clock::now() - began).count();
            std::printf("zoo: visit cache: loaded %s in %.2f s (state %llu)\n", cache->string().c_str(), took,
                        static_cast<unsigned long long>(loaded->StateHash()));
            return loaded;
        }
        std::error_code presentError;
        if (std::filesystem::exists(*cache, presentError)) {
            std::printf("zoo: visit cache: ignored %s: %s\n", cache->string().c_str(), error.c_str());
        }
    }

    auto made = std::make_unique<zoo::Zoo>(*catalog_, *species_, zoo::VisitorZooConfig(seed_));
    if (!emptyMap_) {
        const zoo::ScenarioReport report = zoo::BuildVisitorZoo(*made, options);
        const double took = std::chrono::duration<double>(std::chrono::steady_clock::now() - began).count();
        std::printf("zoo: visitor zoo: %d enclosures, %d animals, %d buildings, %d props (%.2f s)\n",
                    report.enclosures, report.animals, report.buildings, report.props, took);
        for (const std::string& problem : report.problems)
            std::printf("zoo: visitor zoo: %s\n", problem.c_str());
    }
    ShowLoading("The gates opened at nine: the morning's visitors are coming in", 0.3f);
    PrepareVisit(*made, true);
    if (cache) {
        std::error_code ignored;
        std::filesystem::create_directories(cache->parent_path(), ignored);
        std::string error;
        if (zoo::SaveZoo(*made, cache->string(), &error)) {
            PruneVisitCaches(cache->parent_path());
            std::printf("zoo: visit cache: stored %s (state %llu)\n", cache->string().c_str(),
                        static_cast<unsigned long long>(made->StateHash()));
        } else {
            std::printf("zoo: visit cache: could not store %s: %s\n", cache->string().c_str(), error.c_str());
        }
    }
    return made;
}

void ZooGame::PrepareVisit(zoo::Zoo& zoo, bool fill) {
    // People take their time here: a minute or two at a fence, not the few seconds of a day
    // that passes in a quarter of an hour.
    const zoo::VisitorPace pace = CrowdPace(visit_.settings.crowd);
    zoo.Guests().SetPace(pace);
    if (zoo.Guests().Count() > pace.most)
        zoo.Guests().SendHome(pace.most);
    const float hour = startHour_ ? static_cast<float>(*startHour_) : kVisitHour;
    const float now = zoo.Time().Hour();
    const std::int64_t day = fill ? kVisitDay : zoo.Time().Day();
    if (fill || now < kEarliestHour || now >= kLatestHour) {
        zoo.SetCalendar(day, hour);
        fill = true;
    }
    if (fill) {
        const auto began = std::chrono::steady_clock::now();
        // The people who came in first are set down all over the zoo, as if it had been
        // open since nine; the rest of the time they and the later ones settle
        // (zoo::FillForVisit, docs/VISIT_START.md).
        const zoo::VisitStartOptions options{.day = day,
                                             .hour = hour,
                                             .fillTicks = warmUpTicks_ > 0 ? warmUpTicks_ : kFillTicks,
                                             .pace = pace,
                                             .fillArrivals = kFillArrivals * pace.arrivals /
                                                             kVisitPeople.arrivals};
        const zoo::VisitStartReport filled = zoo::FillForVisit(zoo, options, [this](float share) {
            ShowLoading("The gates opened at nine: the morning's visitors are coming in",
                        0.3f + 0.4f * share);
        });
        const double took = std::chrono::duration<double>(std::chrono::steady_clock::now() - began).count();
        std::printf(
            "zoo: visit: %d visitors (%d set down about the zoo) after %lld steps at %.2f h (%.2f s)\n",
            filled.visitors, filled.spread, static_cast<long long>(filled.ticks),
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
    if (visit_.map.shapes.empty())
        BuildVisitMap();
    visit_.screen = VisitScreen::Walking;
    visit_.window = VisitWindow::None;
    visit_.hintLeft = 14.0f;
    visit_.running = false;
    visit_.binoculars = false;
    visit_.binocularsUp = 0.0f;
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
            read.z < map.SizeZ()) {
            // The sidecar may belong to another zoo. Use the entrance if it puts the
            // visitor behind a barrier, inside a building or in deep water.
            const zoo::Vec2 at{read.x, read.z};
            const zoo::TileCoord tile = zoo::World::TileAt(at);
            if (zoo_->Habitats().RegionAt(tile) == zoo::Region::Public && !map.At(tile).building &&
                map.WaterDepthAt(at) < 0.35f)
                pose = read;
        }
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
    const std::string crowd = file.GetString("visit.crowd", "");
    for (std::size_t i = 0; i < kCrowdLevels.size(); ++i)
        if (crowd == kCrowdLevels[i].key)
            s.crowd = static_cast<int>(i);
}

float ZooGame::Magnification() const {
    // Raised smoothly, the way the eyepieces come up to the eyes.
    const float up = visit_.binocularsUp;
    const float eased = up * up * (3.0f - 2.0f * up);
    return 1.0f + (kBinocularPower - 1.0f) * eased;
}

void ZooGame::ApplyVisitSettings(bool store) {
    using Microsoft::Xna::Framework::MathHelper;
    const VisitSettings& s = visit_.settings;
    auto& camera = MainCamera();
    camera.SetPerspective(MathHelper::ToRadians(s.fieldOfView / Magnification()), camera.AspectRatio(),
                          camera.NearPlane(), camera.FarPlane());
    walker_.SetHeadBob(s.headBob ? 0.012f : 0.0f);
    sounds_->SetVolume(s.volume);
    if (zoo_) {
        // A smaller crowd chosen during a visit: the groups the player does not see go home
        // at once; a larger one fills up as people come in.
        const zoo::VisitorPace pace = CrowdPace(s.crowd);
        zoo_->Guests().SetPace(pace);
        if (const int gone = zoo_->Guests().SendHome(pace.most); gone > 0)
            std::printf("zoo: settings: %d visitors went home (%s)\n", gone,
                        kCrowdLevels[static_cast<std::size_t>(s.crowd)].name);
    }
    if (!store)
        return;
    auto& file = Preferences();
    file.SetFloat("visit.mouse_speed", s.sensitivity, 2);
    file.SetBool("visit.invert_y", s.invertY);
    file.SetFloat("visit.field_of_view", s.fieldOfView, 1);
    file.SetBool("visit.head_bob", s.headBob);
    file.SetFloat("visit.volume", s.volume, 2);
    file.SetBool("visit.diagnostics", s.diagnostics);
    file.Set("visit.crowd", kCrowdLevels[static_cast<std::size_t>(s.crowd)].key);
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
    f.animalsProxied = static_cast<int>(animals_->Proxied());
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
        "avatars %d ready of %zu, %d failed; animals %zu, %zu drawn (%zu distant LOD)\n",
        when, visit_.clock.c_str(), static_cast<double>(at.x), static_cast<double>(at.z),
        static_cast<double>(walker_.Yaw() * 180.0f / kPi), zoo_->Guests().Count(), visitors_->InRange(),
        visitors_->InView(), visitors_->Drawn(), visitors_->Ready(), visitors_->CastSize(),
        visitors_->Failed(), zoo_->Fauna().All().size(), animals_->Drawn(), animals_->Proxied());
    const SoundFigures& sound = sounds_->Figures();
    std::printf(
        "zoo: visit %s: sound %s, %d recordings (%d decoded, %d failed), %d voices (%d heard), %d steps, "
        "%d calls, last \"%s\"\n",
        when, sound.device ? "plays" : "has no device", sound.files, sound.loaded, sound.failed,
        sound.voices, sound.audible, sound.steps, sound.calls, sound.lastCall.c_str());
    if (visit_.card.shown > 0.0f || !visit_.prompt.empty() || visit_.window != VisitWindow::None)
        std::printf("zoo: visit %s: looking at \"%s, %s\" (%s); E: \"%s\"; window %d\n", when,
                    visit_.card.shown > 0.0f ? visit_.card.name.c_str() : "",
                    visit_.card.shown > 0.0f ? visit_.card.species.c_str() : "",
                    visit_.card.shown > 0.0f ? visit_.card.doing.c_str() : "", visit_.prompt.c_str(),
                    static_cast<int>(visit_.window));
}

void ZooGame::UpdateCinema(float seconds) {
    constexpr float kSecondsPerAnimal = 3.0f;
    const auto& all = zoo_->Fauna().All();
    visit_.cinemaTotal = static_cast<int>(cinemaSpecies_.size());
    if (controls_.KeyboardFree() && !cinemaSpecies_.empty()) {
        if (controls_.Pressed(Keys::Right)) {
            // cinemaNext_ already names the species after the current representative.
            cinemaLeft_ = 0.0f;
        } else if (controls_.Pressed(Keys::Left)) {
            const std::size_t current =
                visit_.cinemaIndex > 0 ? static_cast<std::size_t>(visit_.cinemaIndex - 1) : 0;
            cinemaNext_ = (current + cinemaSpecies_.size() - 1) % cinemaSpecies_.size();
            cinemaLeft_ = 0.0f;
        }
    }
    cinemaLeft_ -= seconds;

    const zoo::Animal* featured = cinemaAnimal_ ? zoo_->Fauna().Find(cinemaAnimal_) : nullptr;
    std::optional<Vector3> position = featured ? animals_->PositionOf(featured->id) : std::nullopt;
    bool changed = false;
    if (cinemaLeft_ <= 0.0f || featured == nullptr || !position) {
        featured = nullptr;
        position.reset();
        for (std::size_t tried = 0; tried < cinemaSpecies_.size(); ++tried) {
            const std::size_t index = cinemaNext_ % cinemaSpecies_.size();
            cinemaNext_ = (index + 1) % cinemaSpecies_.size();
            const int wantedSpecies = cinemaSpecies_[index];
            for (const zoo::Animal& candidate : all) {
                if (candidate.species != wantedSpecies || !candidate.Visible())
                    continue;
                const auto at = animals_->PositionOf(candidate.id);
                if (!at)
                    continue;
                featured = &candidate;
                position = at;
                cinemaAnimal_ = candidate.id;
                visit_.cinemaIndex = static_cast<int>(index + 1);
                cinemaLeft_ = kSecondsPerAnimal;
                changed = true;
                break;
            }
            if (featured != nullptr)
                break;
        }
    }

    if (featured == nullptr || !position || featured->species < 0 ||
        featured->species >= static_cast<int>(species_->All().size())) {
        visit_.cinema = false;
        cinemaAnimal_ = {};
        visit_.card.shown = 0.0f;
        visit_.Flash("No animal is outdoors for Cinema mode", 3.0f);
        camera_.SetEye(walker_.Eye(), walker_.Yaw(), walker_.Pitch());
        return;
    }

    const zoo::SpeciesDef& def = species_->All()[static_cast<std::size_t>(featured->species)];
    const float size = std::max(def.bodyLengthM, def.shoulderHeightM) *
                       zoo::BodyScale(*featured, def, zoo_->Time());
    if (changed)
        camera_.FrameFollowTarget(*position, size);
    else
        camera_.SetFollowTarget(*position, size);
    camera_.Update(controls_, seconds, zoo_->Map());
    lookedAt_ = featured->id;
    SetAnimalCard(*featured, 1.0f);
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
        if (controls_.Pressed(Keys::C) && visit_.screen == VisitScreen::Walking &&
            visit_.window == VisitWindow::None) {
            visit_.cinema = !visit_.cinema;
            cinemaAnimal_ = {};
            cinemaSpecies_.clear();
            cinemaNext_ = 0;
            cinemaLeft_ = 0.0f;
            visit_.cinemaIndex = 0;
            visit_.binoculars = false;
            if (visit_.cinema) {
                std::vector<bool> resident(species_->All().size(), false);
                for (const zoo::Animal& animal : zoo_->Fauna().All())
                    if (animal.InWorld() && animal.species >= 0 &&
                        animal.species < static_cast<int>(resident.size()))
                        resident[static_cast<std::size_t>(animal.species)] = true;
                for (std::size_t species = 0; species < resident.size(); ++species)
                    if (resident[species])
                        cinemaSpecies_.push_back(static_cast<int>(species));
                visit_.cinemaTotal = static_cast<int>(cinemaSpecies_.size());
                visit_.Flash("Cinema mode", 1.5f);
            } else {
                visit_.cinemaTotal = 0;
                visit_.card.shown = 0.0f;
                camera_.SetEye(walker_.Eye(), walker_.Yaw(), walker_.Pitch());
                visit_.Flash("Back to the visit", 1.5f);
            }
        }
    }
    HandleVisitRequest();
    if (const int to = std::exchange(visit_.travelTo, -1);
        to >= 0 && to < static_cast<int>(visit_.map.places.size()) && visit_.screen == VisitScreen::Walking) {
        const VisitMap::Place& place = visit_.map.places[static_cast<std::size_t>(to)];
        walker_.Enter(*zoo_, {place.x, place.z}, place.yaw);
        visit_.window = VisitWindow::None;
        visit_.Flash(place.name, 2.5f);
        std::printf("zoo: went to %s at %.1f, %.1f\n", place.name.c_str(), place.x, place.z);
    }

    // The window lost the keyboard (Alt+Tab): the walk waits, the mouse is let go.
    if (visit_.screen == VisitScreen::Walking && !automated && !getIsActiveProperty())
        visit_.screen = VisitScreen::Paused;
    const bool walking = visit_.screen == VisitScreen::Walking;
    // The mouse is let go while the map is open, to point at a place on it.
    const bool capture = walking && !automated && visit_.window != VisitWindow::Map && !visit_.cinema;
    if (capture != MouseCaptured())
        CaptureMouse(capture);

    // The world lives while one walks and behind the title; the pause menu stops it.
    play_.speed = visit_.screen == VisitScreen::Paused ? 0 : 1;
    if (walking) {
        Walker::Input input;
        if (controls_.KeyboardFree() && !visit_.cinema) {
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
        input.run = visit_.running && !visit_.cinema;
        // Over the map the mouse points; it does not turn the head.
        if (visit_.window == VisitWindow::Map)
            look = Vector2(0.0f, 0.0f);

        // Binoculars: B raises and lowers them, the right mouse button holds them up. They are
        // used standing still, and the head turns as much less as the view is narrower.
        const bool free = visit_.window == VisitWindow::None && !visit_.cinema;
        if (controls_.KeyboardFree() && controls_.Pressed(Keys::B) && free)
            visit_.binoculars = !visit_.binoculars;
        if (!free)
            visit_.binoculars = false;
        const bool raised = visit_.binoculars || (free && controls_.Down(MouseKey::Right));
        visit_.binocularsUp = std::clamp(visit_.binocularsUp + (raised ? 4.0f : -5.0f) * dt, 0.0f, 1.0f);
        const float magnified = Magnification();
        if (visit_.binocularsUp > 0.0f || magnified != appliedMagnification_) {
            auto& camera = MainCamera();
            camera.SetPerspective(
                Microsoft::Xna::Framework::MathHelper::ToRadians(visit_.settings.fieldOfView / magnified),
                camera.AspectRatio(), camera.NearPlane(), camera.FarPlane());
            appliedMagnification_ = magnified;
        }
        if (visit_.binocularsUp > 0.5f) {
            input.ahead = 0.0f;
            input.right = 0.0f;
        }
        look = look * (1.0f / magnified);
        const float turn = kTurnPerPixel * visit_.settings.sensitivity;
        input.turn = look.X * turn;
        input.tilt = look.Y * turn * (visit_.settings.invertY ? -1.0f : 1.0f);
        if (!visit_.cinema) {
            walker_.Update(*zoo_, input, dt);
            UpdateBoardsAndMap();
        } else {
            visit_.prompt.clear();
        }
        sinceKept_ += seconds;
        if (sinceKept_ >= kKeepEvery) {
            sinceKept_ = 0.0;
            if (!automated)
                KeepVisit();
        }
    } else if (visit_.screen == VisitScreen::Title) {
        titleTime_ += dt;
    }
    if (play_.speed > 0 && !without_.simulation) {
        const Bench::Scope timed(bench_, Bench::Simulation);
        // The people walk around whoever walks among them.
        if (visit_.screen == VisitScreen::Walking)
            zoo_->Guests().SetBystander(walker_.Position());
        else
            zoo_->Guests().ClearBystander();
        StepSimulation(seconds);
    }
    if (zoo_->Config().dayPace != 0 && zoo_->Time().Hour() >= kLatestHour)
        zoo_->SetDayPace(0);

    if (visit_.screen == VisitScreen::Title) {
        // Behind the title: the entrance as someone standing there sees it, looking slowly about.
        const float yaw = walker_.Yaw() + 0.32f * std::sin(titleTime_ * 0.09f);
        camera_.SetEye(walker_.Eye(), yaw, 0.02f);
    } else if (visit_.cinema) {
        UpdateCinema(dt);
    } else {
        camera_.SetEye(walker_.Eye(), walker_.Yaw(), walker_.Pitch());
    }
    camera_.Apply(MainCamera());
    UpdateShadowArea();
    terrain_->Update(zoo_->Map());
    scenery_->Update(zoo_->Map(), zoo_->Objects(), 2, MainCamera().Position());
    const float lived = dt * static_cast<float>(play_.speed);
    {
        const Bench::Scope timed(bench_, Bench::Animals);
        animals_->Update(*zoo_, lived, static_cast<float>(tickDebt_));
    }
    if (visit_.screen == VisitScreen::Walking && !visit_.cinema) {
        const Bench::Scope timed(bench_, Bench::AnimalLook);
        LookAtAnimals(dt);
    }
    {
        const Bench::Scope timed(bench_, Bench::People);
        visitors_->Update(*zoo_, lived, static_cast<float>(tickDebt_));
        visitors_->Prepare(getGraphicsDeviceProperty());
        keepers_->Update(*zoo_, lived, static_cast<float>(tickDebt_));
    }
    UpdateSky();

    const bool afoot = visit_.screen == VisitScreen::Walking && !visit_.cinema;
    const Bench::Scope timed(bench_, Bench::Sound);
    sounds_->Update(*zoo_, CnaEngine::Audio::ListenerFrom(MainCamera()),
                    {.feet = walker_.Position(),
                     .stepped = afoot && walker_.Stepped(),
                     .running = visit_.running,
                     .paused = visit_.screen == VisitScreen::Paused},
                    dt);
}

}  // namespace zoogame
