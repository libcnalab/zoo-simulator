// SPDX-License-Identifier: MIT
#include "Soundscape.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <map>
#include <utility>

#include <nlohmann/json.hpp>

#include "zoo/world/Terrain.hpp"

namespace zoogame {

namespace {

namespace Audio = CnaEngine::Audio;

/// The figures of the mix. tools/audio/mix_model.py holds the same ones, with the reasons and
/// the levels they give, and tools/audio/test_mix_model.py fails when the two differ.
namespace mix {
/// A source abeam is heard this far to one side at most, of 1 = one ear only.
constexpr float kPanLimit = 0.6f;
// How a call carries, by its kind: full level within the reference distance, half the
// pressure with each doubling beyond it, faded out over the last fifth of its reach.
constexpr float kQuietReference = 8.0f;
constexpr float kQuietReach = 60.0f;
constexpr float kMidReference = 10.0f;
constexpr float kMidReach = 110.0f;
constexpr float kLoudReference = 14.0f;
constexpr float kLoudReach = 220.0f;
constexpr float kCallRolloff = 1.0f;
/// No two calls are the same: each way in octaves, and the lowest level of 1.
constexpr float kCallPitch = 0.05f;
constexpr float kCallLevelFloor = 0.92f;
constexpr float kStepPitch = 0.05f;
constexpr float kStepLevelFloor = 0.88f;
/// One's own steps against the air (0.55): at 0.45 they lay under it and were not in the capture.
constexpr float kStepsCategory = 1.0f;
constexpr float kStepWalk = 0.7f;
/// How fast the air and the leaves come and go, per second.
constexpr float kBedFade = 1.2f;
/// The leaves in still air; the wind (0..1) comes on top.
constexpr float kLeavesCalm = 0.25f;
/// The day's birdsong at full daylight. Its recording lost the rumble below 120 Hz it had (half
/// of its energy, where the lion and the elephant call) and was levelled again, which raised the
/// rest by 1 to 2 dB: this keeps the birds within 1 dB of where they were.
constexpr float kBirdsong = 0.85f;
/// People in a square of ten metres at whom the murmur plays as recorded, and the fewest
/// who make one.
constexpr float kCrowdFull = 30.0f;
constexpr float kCrowdFewest = 3.0f;
// The quiet sounds between animals: how often meeting or playing is heard, how long one
// animal then keeps quiet, and the pause between any two such sounds.
constexpr float kSoftChance = 0.5f;
constexpr float kSoftQuiet = 25.0f;
constexpr float kSoftGap = 4.0f;
constexpr float kFountainReference = 4.0f;
constexpr float kFountainReach = 45.0f;
}  // namespace mix

constexpr Audio::Falloff kWaterFalloff{.reference = 2.5f, .radius = 26.0f, .rolloff = 1.0f};
constexpr Audio::Falloff kFountainFalloff{
    .reference = mix::kFountainReference, .radius = mix::kFountainReach, .rolloff = 1.0f};
constexpr Audio::Falloff kCrowdFalloff{.reference = 5.0f, .radius = 55.0f, .rolloff = 0.9f};

/// How a call carries: a roar is heard across the zoo, a chuff only at the fence.
struct Carry {
    float reference = mix::kQuietReference;
    float reach = mix::kQuietReach;
};

Carry CarryOf(std::string_view call) {
    static constexpr std::array<std::string_view, 3> kLoud = {"_roar", "_howl", "_trumpet"};
    static constexpr std::array<std::string_view, 5> kMid = {"_bray", "_whinny", "_bark", "_call", "_rumble"};
    for (const std::string_view ending : kLoud) {
        if (call.ends_with(ending))
            return {mix::kLoudReference, mix::kLoudReach};
    }
    for (const std::string_view ending : kMid) {
        if (call.ends_with(ending))
            return {mix::kMidReference, mix::kMidReach};
    }
    return {};
}

/// Where a source is put for the engine. The engine pans a source abeam wholly to one ear
/// and CNA's pan law then leaves the other ear silent, which no source in the open is: the
/// source is swung towards the front until it is heard kPanLimit to its side at most, at
/// its true distance. Within `near` metres it is around the ears rather than beside them.
Vector3 Apparent(const Audio::Listener& ears, const Vector3& source, float near) {
    const Vector3 offset = source - ears.position;
    const float distance = offset.Length();
    Vector3 right = Vector3::Cross(ears.forward, ears.up);
    if (distance < 0.01f || right.Length() < 1.0e-4f)
        return source;
    right = Vector3::Normalize(right);
    const float side = Vector3::Dot(offset, right) / distance;
    const float wanted = side * mix::kPanLimit * (near > 0.0f ? std::min(1.0f, distance / near) : 1.0f);
    Vector3 rest = offset - right * (side * distance);
    if (rest.Length() < 1.0e-4f)
        rest = ears.forward;
    rest = Vector3::Normalize(rest);
    return ears.position + (right * wanted + rest * std::sqrt(1.0f - wanted * wanted)) * distance;
}

/// A doing that one of the library's recordings is of: the flamingos' calls were recorded from a
/// feeding flock (assets/audio/SOURCES.md), so a flamingo that starts to feed may call. Species
/// whose recordings say nothing of what the animal was doing are heard only when they meet,
/// play or call out.
struct Doing {
    std::string_view species;
    zoo::AnimalActivity activity;
};
constexpr std::array<Doing, 1> kHeardDoing = {{{"flamingo", zoo::AnimalActivity::FilterFeed}}};

bool HeardDoing(const zoo::Zoo& zoo, const zoo::Animal& animal) {
    const auto& species = zoo.SpeciesList().All();
    if (animal.moving || animal.species < 0 || static_cast<std::size_t>(animal.species) >= species.size())
        return false;
    const std::string& id = species[static_cast<std::size_t>(animal.species)].id;
    return std::ranges::any_of(kHeardDoing, [&](const Doing& doing) {
        return doing.activity == animal.activity && doing.species == id;
    });
}

/// The footsteps to be heard on a tile.
std::string_view Surface(const zoo::Tile& tile) {
    switch (tile.path) {
        case zoo::PathKind::Asphalt:
        case zoo::PathKind::Cobblestone:
        case zoo::PathKind::Boardwalk:
            return "footstep_paved";
        case zoo::PathKind::Gravel:
            return "footstep_gravel";
        default:
            break;
    }
    switch (tile.terrain) {
        case zoo::Terrain::Grass:
        case zoo::Terrain::Savanna:
            return "footstep_grass";
        default:
            return "footstep_gravel";
    }
}

float Eased(float from, float to, float rate, float seconds) {
    return from + (to - from) * (1.0f - std::exp(-rate * seconds));
}

Vector3 On(const zoo::Zoo& zoo, zoo::Vec2 at, float above) {
    return Vector3(at.x, zoo.Map().HeightAt(at) + above, at.z);
}

}  // namespace

Soundscape::Soundscape(const std::filesystem::path& assets) {
    began_ = std::chrono::steady_clock::now();
    if (const char* path = std::getenv("ZOO_SOUND_LOG"); path && *path) {
        log_ = std::fopen(path, "w");
        if (log_)
            std::setvbuf(log_, nullptr, _IOLBF, 0);
    }
    LoadLibrary(assets / "audio");
    figures_.device = system_.State() == Audio::AudioState::Ready;
    system_.SetCategoryVolume("ambience", 0.55f);
    system_.SetCategoryVolume("people", 0.6f);
    system_.SetCategoryVolume("animals", 1.0f);
    system_.SetCategoryVolume("footsteps", mix::kStepsCategory);
    system_.SetMasterVolume(volume_);
}

Soundscape::~Soundscape() {
    system_.StopAll();
    if (log_)
        std::fclose(log_);
}

double Soundscape::Clock() const {
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - began_).count();
}

void Soundscape::LoadLibrary(const std::filesystem::path& directory) {
    std::ifstream file(directory / "sounds.json");
    if (!file) {
        std::printf("zoo: sound: there is no %s: the zoo is silent\n",
                    (directory / "sounds.json").string().c_str());
        return;
    }
    nlohmann::json manifest;
    try {
        manifest = nlohmann::json::parse(file);
    } catch (const std::exception& error) {
        std::printf("zoo: sound: sounds.json: %s\n", error.what());
        return;
    }
    if (!manifest.contains("sounds") || !manifest["sounds"].is_array())
        return;
    // The mix: recordings are normalised to their peaks, which leaves a camel's grunt 20 LU
    // louder than a bear's growl. tools/audio/audit_audio.py measures each take and writes
    // the gain that sets it right; a take the mix does not know plays as recorded.
    nlohmann::json mix;
    if (std::ifstream mixFile(directory / "mix.json"); mixFile) {
        try {
            mix = nlohmann::json::parse(mixFile);
        } catch (const std::exception& error) {
            std::printf("zoo: sound: mix.json: %s\n", error.what());
        }
    }
    const nlohmann::json* gains = mix.is_object() && mix.contains("gains_db") && mix["gains_db"].is_object()
                                      ? &mix["gains_db"]
                                      : nullptr;
    int mixed = 0;
    for (const auto& entry : manifest["sounds"]) {
        if (!entry.is_object() || !entry.contains("name") || !entry.contains("files"))
            continue;
        const std::string name = entry.value("name", std::string());
        for (const auto& item : entry["files"]) {
            if (!item.is_string())
                continue;
            const std::string relative = item.get<std::string>();
            const std::filesystem::path path = directory / relative;
            std::error_code fileError;
            if (!std::filesystem::is_regular_file(path, fileError)) {
                ++figures_.failed;
                if (figures_.failed == 1)
                    std::printf("zoo: sound: %s: file not found\n", path.string().c_str());
                continue;
            }
            Recording recording;
            recording.path = path;
            recording.relative = relative;
            if (gains && gains->contains(relative) && (*gains)[relative].is_number()) {
                const float decibels = std::clamp((*gains)[relative].get<float>(), -40.0f, 0.0f);
                recording.gain = std::pow(10.0f, decibels / 20.0f);
                ++mixed;
            }
            library_[name].recordings.push_back(std::move(recording));
            ++figures_.files;
        }
    }
    std::printf("zoo: sound: %d recordings under %zu names, %d failed; the mix sets %d of them; decoded on use\n",
                figures_.files, library_.size(), figures_.failed, mixed);
}

bool Soundscape::Decode(Recording& recording) {
    if (recording.attempted)
        return recording.sound != nullptr;
    recording.attempted = true;
    try {
        recording.sound = std::make_unique<Sound>(recording.path.string());
        ++figures_.loaded;
    } catch (const std::exception& error) {
        ++figures_.failed;
        // Without an audio device every file fails the same way: say it once.
        if (figures_.failed == 1)
            std::printf("zoo: sound: %s: %s\n", recording.path.string().c_str(), error.what());
    }
    return recording.sound != nullptr;
}

float Soundscape::Chance() {
    seed_ ^= seed_ << 13;
    seed_ ^= seed_ >> 17;
    seed_ ^= seed_ << 5;
    return static_cast<float>(seed_ >> 8) / 16777216.0f;
}

const Soundscape::Recording* Soundscape::Pick(std::string_view name) {
    const auto found = library_.find(std::string(name));
    if (found == library_.end() || found->second.recordings.empty())
        return nullptr;
    Takes& takes = found->second;
    const int count = static_cast<int>(takes.recordings.size());
    int index = 0;
    if (count > 1) {
        // Any take but the last one played: the same take twice in a row is heard as a machine.
        const int choices = takes.last < 0 ? count : count - 1;
        index = std::min(static_cast<int>(Chance() * static_cast<float>(choices)), choices - 1);
        if (takes.last >= 0 && index >= takes.last)
            ++index;
    }
    for (int offset = 0; offset < count; ++offset) {
        const int candidate = (index + offset) % count;
        if (count > 1 && candidate == takes.last)
            continue;
        Recording& recording = takes.recordings[static_cast<std::size_t>(candidate)];
        if (Decode(recording)) {
            takes.last = candidate;
            return &recording;
        }
    }
    // If every alternative failed to decode, retain a working last take rather than silence.
    if (takes.last >= 0) {
        Recording& recording = takes.recordings[static_cast<std::size_t>(takes.last)];
        if (Decode(recording))
            return &recording;
    }
    return nullptr;
}

void Soundscape::SetVolume(float volume) {
    volume_ = std::clamp(volume, 0.0f, 1.0f);
}

void Soundscape::Reset() {
    system_.StopAll();
    air_ = {};
    leaves_ = {};
    water_ = {};
    fountain_ = {};
    people_[0] = {};
    people_[1] = {};
    animals_.clear();
    calling_ = {};
    fountainDef_ = -2;
    survey_ = 0.0f;
    sinceSoft_ = 60.0f;
    figures_.calls = 0;
    figures_.steps = 0;
    figures_.lastCall.clear();
}

void Soundscape::Lay(Bed& bed, std::string_view sound, float wanted, int priority, float seconds) {
    // Never at once, however long the frame was: a bed that starts or stops in one step is a click.
    bed.level = Eased(bed.level, wanted, mix::kBedFade, std::min(seconds, 0.05f));
    if (bed.voice == 0) {
        if (wanted <= 0.0f)
            return;
        const Recording* loop = Pick(sound);
        if (!loop)
            return;
        bed.voice = system_.Play(
            *loop->sound, {.category = "ambience", .volume = bed.level, .loop = true, .priority = priority});
        if (log_)
            std::fprintf(log_, "%.3f\tbeds\t%s\n", Clock(), loop->relative.c_str());
        return;
    }
    system_.SetVolume(bed.voice, bed.level);
}

void Soundscape::HearAir(const zoo::Zoo& zoo, float seconds) {
    // The birds sing by daylight and the leaves rustle with the wind the sky shows.
    const zoo::WeatherNow& weather = zoo.Weather();
    Lay(air_, "park_day", mix::kBirdsong * std::clamp(weather.daylight, 0.0f, 1.0f), 3, seconds);
    Lay(leaves_, "wind_trees", std::clamp(mix::kLeavesCalm + weather.wind, 0.0f, 1.0f), 2, seconds);
}

void Soundscape::Follow(Follower& follower, std::string_view sound, std::string_view category,
                        const Vector3* target, float volume, float pitch, const Audio::Falloff& falloff,
                        float near, const Audio::Listener& ears, float seconds) {
    const float wanted = target ? volume : 0.0f;
    if (target) {
        // A new source is taken up where it is; a known one is glided to, so that the
        // sound never jumps from one ear to the other.
        if (!follower.placed || follower.volume < 0.02f) {
            follower.position = *target;
        } else {
            follower.position = Vector3::Lerp(follower.position, *target, 1.0f - std::exp(-1.5f * seconds));
        }
        follower.placed = true;
    }
    follower.volume = Eased(follower.volume, wanted, 1.2f, std::min(seconds, 0.05f));
    const Vector3 heard = Apparent(ears, follower.position, near);
    if (follower.voice == 0) {
        if (wanted <= 0.0f)
            return;
        const Recording* loop = Pick(sound);
        if (!loop)
            return;
        if (log_)
            std::fprintf(log_, "%.3f\tloop\t%s\t%s\n", Clock(), std::string(sound).c_str(),
                         loop->relative.c_str());
        follower.voice = system_.Play(*loop->sound, {.category = std::string(category),
                                                     .volume = follower.volume,
                                                     .pitch = pitch,
                                                     .loop = true,
                                                     .position = heard,
                                                     .falloff = falloff,
                                                     .priority = 1});
        return;
    }
    system_.SetPosition(follower.voice, heard);
    system_.SetVolume(follower.voice, follower.volume);
}

void Soundscape::HearWater(const zoo::Zoo& zoo, const Audio::Listener& ears, float seconds) {
    const Vector3& at = ears.position;
    if (survey_ <= 0.0f) {
        const zoo::World& map = zoo.Map();
        const zoo::TileCoord here = zoo::World::TileAt({at.X, at.Z});
        constexpr int kTiles = 13;
        float best = 1.0e9f;
        waterNear_ = false;
        for (int dz = -kTiles; dz <= kTiles; ++dz) {
            for (int dx = -kTiles; dx <= kTiles; ++dx) {
                const zoo::TileCoord tile{here.x + dx, here.z + dz};
                if (!map.HasWater(tile))
                    continue;
                const float x = (static_cast<float>(tile.x) + 0.5f) * zoo::kTileSize;
                const float z = (static_cast<float>(tile.z) + 0.5f) * zoo::kTileSize;
                const float distance = (x - at.X) * (x - at.X) + (z - at.Z) * (z - at.Z);
                if (distance < best) {
                    best = distance;
                    waterAt_ = Vector3(x, map.At(tile).waterLevel, z);
                    waterNear_ = true;
                }
            }
        }
        // The nearest fountain: its jet and what falls from its bowl.
        if (fountainDef_ == -2) {
            fountainDef_ = -1;
            const auto& props = zoo.Definitions().props;
            for (std::size_t i = 0; i < props.size(); ++i) {
                if (props[i].id == "fountain")
                    fountainDef_ = static_cast<int>(i);
            }
        }
        fountainNear_ = false;
        best = mix::kFountainReach * mix::kFountainReach;
        for (const zoo::Prop& prop : zoo.Objects().Props()) {
            if (fountainDef_ < 0 || prop.def != fountainDef_)
                continue;
            const float dx = prop.position.x - at.X;
            const float dz = prop.position.z - at.Z;
            if (dx * dx + dz * dz < best) {
                best = dx * dx + dz * dz;
                fountainAt_ = On(zoo, prop.position, 1.5f);
                fountainNear_ = true;
            }
        }
    }
    Follow(water_, "water", "ambience", waterNear_ ? &waterAt_ : nullptr, 0.8f, 0.0f, kWaterFalloff, 6.0f,
           ears, seconds);
    Follow(fountain_, "fountain", "ambience", fountainNear_ ? &fountainAt_ : nullptr, 0.8f, 0.0f,
           kFountainFalloff, 6.0f, ears, seconds);
}

void Soundscape::HearPeople(const zoo::Zoo& zoo, const Audio::Listener& ears, float seconds) {
    if (survey_ <= 0.0f) {
        // People are gathered into squares of ten metres; the murmur comes from the two
        // squares heard loudest, which are the full ones and the near ones.
        constexpr float kSquare = 10.0f;
        constexpr float kNear = 48.0f;
        struct Group {
            float x = 0.0f;
            float z = 0.0f;
            int people = 0;
            float heard = 0.0f;
        };
        std::map<std::pair<int, int>, Group> squares;
        for (const zoo::Visitor& person : zoo.Guests().All()) {
            if (person.activity == zoo::VisitorActivity::Inside)
                continue;
            const float dx = person.position.x - ears.position.X;
            const float dz = person.position.z - ears.position.Z;
            if (dx * dx + dz * dz > kNear * kNear)
                continue;
            Group& group = squares[{static_cast<int>(std::floor(person.position.x / kSquare)),
                                    static_cast<int>(std::floor(person.position.z / kSquare))}];
            group.x += person.position.x;
            group.z += person.position.z;
            ++group.people;
        }
        std::array<Group, 2> loudest{};
        for (auto& [where, group] : squares) {
            const auto count = static_cast<float>(group.people);
            if (count < mix::kCrowdFewest)
                continue;
            group.x /= count;
            group.z /= count;
            // Twice the people are twice the power, not twice the pressure.
            const float level = std::min(1.0f, std::sqrt(count / mix::kCrowdFull));
            const Vector3 centre(group.x, ears.position.Y, group.z);
            group.heard = level * Audio::Spatialize(ears, centre, kCrowdFalloff).volume;
            if (group.heard > loudest[0].heard) {
                loudest[1] = loudest[0];
                loudest[0] = group;
            } else if (group.heard > loudest[1].heard) {
                loudest[1] = group;
            }
        }
        for (std::size_t i = 0; i < loudest.size(); ++i) {
            const Group& group = loudest[i];
            peopleCount_[i] = group.people;
            if (group.heard <= 0.0f) {
                peopleLevel_[i] = 0.0f;
                continue;
            }
            peopleAt_[i] = On(zoo, {group.x, group.z}, 1.5f);
            peopleLevel_[i] = std::min(1.0f, std::sqrt(static_cast<float>(group.people) / mix::kCrowdFull));
        }
    }
    for (std::size_t i = 0; i < 2; ++i) {
        // The same recording twice would beat against itself: the second is a shade lower.
        Follow(people_[i], "crowd", "people", peopleLevel_[i] > 0.0f ? &peopleAt_[i] : nullptr,
               peopleLevel_[i], i == 0 ? 0.0f : -0.07f, kCrowdFalloff, 10.0f, ears, seconds);
    }
}

bool Soundscape::Call(const zoo::Zoo& zoo, const zoo::Animal& animal, const Audio::Listener& ears, bool loud,
                      const char* why) {
    const auto& species = zoo.SpeciesList().All();
    if (animal.species < 0 || static_cast<std::size_t>(animal.species) >= species.size())
        return false;
    const zoo::SpeciesDef& def = species[static_cast<std::size_t>(animal.species)];
    // Of the sounds the species has, those the library holds. An animal that calls out uses
    // the one of them that carries farthest; the quiet sounds between animals are never the
    // roar, the howl or the trumpet. A species the library has nothing for stays silent.
    const std::string* chosen = nullptr;
    Carry carry;
    int known = 0;
    for (const std::string& name : def.sounds) {
        if (!library_.contains(name))
            continue;
        const Carry its = CarryOf(name);
        if (loud ? chosen && its.reach < carry.reach : its.reach >= mix::kLoudReach)
            continue;
        if (loud && chosen && its.reach > carry.reach)
            known = 0;
        ++known;
        if (Chance() < 1.0f / static_cast<float>(known)) {
            chosen = &name;
            carry = its;
        }
    }
    if (!chosen) {
        if (log_)
            std::fprintf(log_, "%.3f\tnocall\t%s\t%s\tanimal=%u\tnone-in-library\n", Clock(), def.id.c_str(),
                         why, animal.id.value);
        return false;
    }
    const Vector3 mouth = On(zoo, animal.position, std::max(0.3f, def.shoulderHeightM));
    const float away = Vector3::Distance(mouth, ears.position);
    if (away > carry.reach) {
        if (log_)
            std::fprintf(log_, "%.3f\tnocall\t%s\t%s\tanimal=%u\tout-of-reach\taway=%.1f\treach=%.0f\n",
                         Clock(), chosen->c_str(), why, animal.id.value, static_cast<double>(away),
                         static_cast<double>(carry.reach));
        return false;
    }
    const Recording* sound = Pick(*chosen);
    if (!sound)
        return false;
    const float level = sound->gain * (mix::kCallLevelFloor + Chance() * (1.0f - mix::kCallLevelFloor));
    const float pitch = (Chance() - 0.5f) * 2.0f * mix::kCallPitch;
    const Audio::Falloff falloff{
        .reference = carry.reference, .radius = carry.reach, .rolloff = mix::kCallRolloff};
    const Vector3 heard = Apparent(ears, mouth, 0.0f);
    const Audio::VoiceId voice = system_.Play(*sound->sound, {.category = "animals",
                                                              .volume = level,
                                                              .pitch = pitch,
                                                              .position = heard,
                                                              .falloff = falloff,
                                                              .priority = 2});
    if (voice == 0)
        return false;
    for (Calling& call : calling_) {
        if (call.voice == 0) {
            call = {voice, mouth};
            break;
        }
    }
    if (log_) {
        const Audio::Spatial spatial = Audio::Spatialize(ears, heard, falloff);
        std::fprintf(log_,
                     "%.3f\tcall\t%s\t%s\t%s\tanimal=%u\tgain=%.3f\tpitch=%.4f\taway=%.1f\treach=%.0f\t"
                     "spatial=%.4f\tpan=%.3f\tvoice=%u\treference=%.1f\n",
                     Clock(), chosen->c_str(), sound->relative.c_str(), why, animal.id.value,
                     static_cast<double>(level), static_cast<double>(pitch), static_cast<double>(away),
                     static_cast<double>(carry.reach), static_cast<double>(spatial.volume),
                     static_cast<double>(spatial.pan), voice, static_cast<double>(carry.reference));
    }
    ++figures_.calls;
    char text[96];
    std::snprintf(text, sizeof text, "%s, %.0f m away", chosen->c_str(), static_cast<double>(away));
    figures_.lastCall = text;
    return true;
}

void Soundscape::HearAnimals(const zoo::Zoo& zoo, const Audio::Listener& ears, float seconds) {
    for (auto& [id, heard] : animals_)
        heard.seen = false;
    sinceSoft_ += seconds;
    for (const zoo::Animal& animal : zoo.Fauna().All()) {
        if (!animal.Visible())
            continue;
        const auto [entry, fresh] = animals_.try_emplace(animal.id.value);
        Heard& heard = entry->second;
        // Nobody knows how long an animal one comes upon has been quiet.
        if (fresh)
            heard.quiet = 10.0f + Chance() * 35.0f;
        heard.seen = true;
        heard.quiet += seconds;
        // What is heard is what the animal does, when it begins to do it: it calls out where
        // it stopped to call (Vocalize), and it may make one of its quiet sounds when it has
        // reached a group mate, starts to play or starts what its recording was made of
        // (HeardDoing). Nothing sounds by the clock.
        Cue cue = Cue::None;
        if (animal.activity == zoo::AnimalActivity::Vocalize && !animal.moving) {
            cue = Cue::Calling;
        } else if ((animal.activity == zoo::AnimalActivity::Socialize && !animal.moving) ||
                   animal.activity == zoo::AnimalActivity::Play || HeardDoing(zoo, animal)) {
            cue = Cue::Lively;
        }
        const Cue before = heard.cue;
        heard.cue = cue;
        if (cue == before || cue == Cue::None)
            continue;
        if (log_)
            std::fprintf(log_, "%.3f\t%s\tanimal=%u\tspecies=%d\tquiet=%.1f\n", Clock(),
                         cue == Cue::Calling ? "vocalize" : "lively", animal.id.value, animal.species,
                         static_cast<double>(heard.quiet));
        if (cue == Cue::Calling) {
            if (heard.quiet > 3.0f && Call(zoo, animal, ears, true, "vocalize"))
                heard.quiet = 0.0f;
        } else if (heard.quiet > mix::kSoftQuiet && sinceSoft_ > mix::kSoftGap &&
                   Chance() < mix::kSoftChance &&
                   Call(zoo, animal, ears, false,
                        animal.activity == zoo::AnimalActivity::Play        ? "play"
                        : animal.activity == zoo::AnimalActivity::Socialize ? "company"
                                                                            : "doing")) {
            heard.quiet = 0.0f;
            sinceSoft_ = 0.0f;
        }
    }
    std::erase_if(animals_, [](const auto& entry) { return !entry.second.seen; });
}

void Soundscape::Step(const zoo::Zoo& zoo, const Visitor& visitor) {
    const zoo::TileCoord tile = zoo::World::TileAt(visitor.feet);
    if (!zoo.Map().InBounds(tile))
        return;
    // One footfall each stride of the body (Walker.cpp: 1.5 m, with a sway of the eyes): about
    // three a second at the walk, six at the run, in time with what is seen.
    const Recording* step = Pick(Surface(zoo.Map().At(tile)));
    if (!step)
        return;
    const float pitch = (Chance() - 0.5f) * 2.0f * mix::kStepPitch;
    const float level = step->gain * (visitor.running ? 1.0f : mix::kStepWalk) *
                        (mix::kStepLevelFloor + Chance() * (1.0f - mix::kStepLevelFloor));
    system_.Play(*step->sound, {.category = "footsteps",
                                .volume = level,
                                .pitch = pitch,
                                .pan = (figures_.steps % 2 == 0 ? -0.08f : 0.08f),
                                .priority = 2});
    if (log_)
        std::fprintf(log_, "%.3f\tstep\t%s\tpitch=%.4f\tlevel=%.3f\trun=%d\tx=%.2f\tz=%.2f\n", Clock(),
                     step->relative.c_str(), static_cast<double>(pitch), static_cast<double>(level),
                     visitor.running ? 1 : 0, static_cast<double>(visitor.feet.x),
                     static_cast<double>(visitor.feet.z));
    ++figures_.steps;
}

void Soundscape::Update(const zoo::Zoo& zoo, const Audio::Listener& ears, const Visitor& visitor,
                        float seconds) {
    if (library_.empty())
        return;
    level_ = Eased(level_, visitor.paused ? 0.3f : 1.0f, 4.0f, seconds);
    system_.SetMasterVolume(volume_ * level_);
    system_.SetListener(ears);
    // A call stays where its animal stood while the head turns and the visitor walks on.
    for (Calling& call : calling_) {
        if (call.voice == 0)
            continue;
        if (system_.IsPlaying(call.voice)) {
            system_.SetPosition(call.voice, Apparent(ears, call.from, 0.0f));
        } else {
            call.voice = 0;
        }
    }

    HearAir(zoo, seconds);
    survey_ -= seconds;
    HearWater(zoo, ears, seconds);
    HearPeople(zoo, ears, seconds);
    if (survey_ <= 0.0f)
        survey_ = 0.5f;
    if (!visitor.paused) {
        HearAnimals(zoo, ears, seconds);
        if (visitor.stepped)
            Step(zoo, visitor);
    }

    system_.Update(seconds);
    figures_.device = system_.State() == Audio::AudioState::Ready;
    figures_.voices = static_cast<int>(system_.VoiceCount());
    figures_.audible = static_cast<int>(system_.AudibleCount());
    if (log_) {
        untilState_ -= seconds;
        if (untilState_ <= 0.0f) {
            untilState_ = 0.25f;
            const auto follower = [&](const char* name, const Follower& f) {
                std::fprintf(log_, "\t%s=%.4f,%.3f,%.1f", name,
                             static_cast<double>(system_.EffectiveVolume(f.voice)),
                             static_cast<double>(system_.EffectivePan(f.voice)),
                             static_cast<double>(Vector3::Distance(f.position, ears.position)));
            };
            std::fprintf(log_,
                         "%.3f\tstate\thour=%.2f\tx=%.2f\tz=%.2f\tfx=%.3f\tfz=%.3f\tvoices=%d\taudible=%d\t"
                         "master=%.3f\tdaylight=%.3f\twind=%.3f\tair=%.4f\tleaves=%.4f",
                         Clock(), static_cast<double>(zoo.Time().Hour()),
                         static_cast<double>(ears.position.X), static_cast<double>(ears.position.Z),
                         static_cast<double>(ears.forward.X), static_cast<double>(ears.forward.Z),
                         figures_.voices, figures_.audible, static_cast<double>(volume_ * level_),
                         static_cast<double>(zoo.Weather().daylight), static_cast<double>(zoo.Weather().wind),
                         static_cast<double>(system_.EffectiveVolume(air_.voice)),
                         static_cast<double>(system_.EffectiveVolume(leaves_.voice)));
            follower("water", water_);
            follower("fountain", fountain_);
            follower("people0", people_[0]);
            follower("people1", people_[1]);
            std::fprintf(log_, "\tgroups=%d,%d\n", peopleCount_[0], peopleCount_[1]);
        }
    }
}

}  // namespace zoogame
