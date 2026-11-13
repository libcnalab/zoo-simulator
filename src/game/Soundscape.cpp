// SPDX-License-Identifier: MIT
#include "Soundscape.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <exception>
#include <fstream>
#include <map>
#include <utility>

#include <nlohmann/json.hpp>

#include "zoo/world/Terrain.hpp"

namespace zoogame {

namespace {

namespace Audio = CnaEngine::Audio;

/// How far a call carries, metres, by what kind of call it is: a roar is heard
/// across the zoo, a chuff only at the fence.
float Reach(std::string_view call) {
    static constexpr std::array<std::pair<std::string_view, float>, 8> kLoud = {{
        {"_roar", 170.0f},
        {"_howl", 190.0f},
        {"_trumpet", 170.0f},
        {"_bray", 90.0f},
        {"_whinny", 90.0f},
        {"_bark", 80.0f},
        {"_call", 70.0f},
        {"_rumble", 70.0f},
    }};
    for (const auto& [ending, metres] : kLoud) {
        if (call.ends_with(ending))
            return metres;
    }
    return 45.0f;
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
    Load(assets / "audio");
    figures_.device = system_.State() == Audio::AudioState::Ready;
    system_.SetCategoryVolume("ambience", 0.55f);
    system_.SetCategoryVolume("people", 0.6f);
    system_.SetCategoryVolume("animals", 1.0f);
    system_.SetCategoryVolume("footsteps", 0.45f);
    system_.SetMasterVolume(volume_);
}

Soundscape::~Soundscape() {
    system_.StopAll();
}

void Soundscape::Load(const std::filesystem::path& directory) {
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
            try {
                library_[name].push_back(std::make_unique<Sound>(path.string()));
                ++figures_.files;
                if (gains && gains->contains(relative) && (*gains)[relative].is_number()) {
                    const float decibels = std::clamp((*gains)[relative].get<float>(), -40.0f, 0.0f);
                    gains_[library_[name].back().get()] = std::pow(10.0f, decibels / 20.0f);
                    ++mixed;
                }
            } catch (const std::exception& error) {
                ++figures_.failed;
                // Without an audio device every file fails the same way: say it once.
                if (figures_.failed == 1)
                    std::printf("zoo: sound: %s: %s\n", path.string().c_str(), error.what());
            }
        }
    }
    std::printf("zoo: sound: %d recordings under %zu names, %d failed; the mix sets %d of them\n", figures_.files,
                library_.size(), figures_.failed, mixed);
}

float Soundscape::Chance() {
    seed_ ^= seed_ << 13;
    seed_ ^= seed_ >> 17;
    seed_ ^= seed_ << 5;
    return static_cast<float>(seed_ >> 8) / 16777216.0f;
}

const Soundscape::Sound* Soundscape::Pick(std::string_view name) {
    const auto found = library_.find(std::string(name));
    if (found == library_.end() || found->second.empty())
        return nullptr;
    const auto count = static_cast<float>(found->second.size());
    const auto index = std::min(static_cast<std::size_t>(Chance() * count), found->second.size() - 1);
    return found->second[index].get();
}

void Soundscape::SetVolume(float volume) {
    volume_ = std::clamp(volume, 0.0f, 1.0f);
}

void Soundscape::Reset() {
    system_.StopAll();
    water_ = {};
    people_[0] = {};
    people_[1] = {};
    animals_.clear();
    beds_ = false;
    survey_ = 0.0f;
    untilCall_ = 4.0f;
    figures_.calls = 0;
    figures_.steps = 0;
    figures_.lastCall.clear();
}

void Soundscape::StartBeds() {
    beds_ = true;
    if (const Sound* air = Pick("park_day"))
        system_.Play(*air, {.category = "ambience", .volume = 1.0f, .loop = true, .priority = 3});
    if (const Sound* leaves = Pick("wind_trees"))
        system_.Play(*leaves, {.category = "ambience", .volume = 0.45f, .loop = true, .priority = 2});
}

void Soundscape::Follow(Follower& follower, std::string_view sound, std::string_view category,
                        const Vector3* target, float volume, float pitch, const Audio::Falloff& falloff,
                        float seconds) {
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
    follower.volume = Eased(follower.volume, wanted, 1.2f, seconds);
    if (follower.voice == 0) {
        if (wanted <= 0.0f)
            return;
        const Sound* loop = Pick(sound);
        if (!loop)
            return;
        follower.voice = system_.Play(*loop, {.category = std::string(category),
                                              .volume = follower.volume,
                                              .pitch = pitch,
                                              .loop = true,
                                              .position = follower.position,
                                              .falloff = falloff,
                                              .priority = 1});
        return;
    }
    system_.SetPosition(follower.voice, follower.position);
    system_.SetVolume(follower.voice, follower.volume);
}

void Soundscape::HearWater(const zoo::Zoo& zoo, const Vector3& ears, float seconds) {
    if (survey_ <= 0.0f) {
        const zoo::World& map = zoo.Map();
        const zoo::TileCoord here = zoo::World::TileAt({ears.X, ears.Z});
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
                const float distance = (x - ears.X) * (x - ears.X) + (z - ears.Z) * (z - ears.Z);
                if (distance < best) {
                    best = distance;
                    waterAt_ = Vector3(x, map.At(tile).waterLevel, z);
                    waterNear_ = true;
                }
            }
        }
    }
    Follow(water_, "water", "ambience", waterNear_ ? &waterAt_ : nullptr, 0.8f, 0.0f,
           {.reference = 2.5f, .radius = 26.0f, .rolloff = 1.0f}, seconds);
}

void Soundscape::HearPeople(const zoo::Zoo& zoo, const Vector3& ears, float seconds) {
    if (survey_ <= 0.0f) {
        // People are gathered into squares of ten metres; the two fullest squares
        // near the ears are where the murmur comes from.
        constexpr float kSquare = 10.0f;
        constexpr float kNear = 48.0f;
        struct Group {
            float x = 0.0f;
            float z = 0.0f;
            int people = 0;
        };
        std::map<std::pair<int, int>, Group> squares;
        for (const zoo::Visitor& person : zoo.Guests().All()) {
            if (person.activity == zoo::VisitorActivity::Inside)
                continue;
            const float dx = person.position.x - ears.X;
            const float dz = person.position.z - ears.Z;
            if (dx * dx + dz * dz > kNear * kNear)
                continue;
            Group& group = squares[{static_cast<int>(std::floor(person.position.x / kSquare)),
                                    static_cast<int>(std::floor(person.position.z / kSquare))}];
            group.x += person.position.x;
            group.z += person.position.z;
            ++group.people;
        }
        std::array<Group, 2> fullest{};
        for (const auto& [where, group] : squares) {
            if (group.people > fullest[0].people) {
                fullest[1] = fullest[0];
                fullest[0] = group;
            } else if (group.people > fullest[1].people) {
                fullest[1] = group;
            }
        }
        for (std::size_t i = 0; i < fullest.size(); ++i) {
            const Group& group = fullest[i];
            if (group.people < 4) {
                peopleLevel_[i] = 0.0f;
                continue;
            }
            const auto count = static_cast<float>(group.people);
            peopleAt_[i] = On(zoo, {group.x / count, group.z / count}, 1.5f);
            peopleLevel_[i] = std::clamp(0.3f + count / 20.0f, 0.3f, 1.0f);
        }
    }
    for (std::size_t i = 0; i < 2; ++i) {
        // The same recording twice would beat against itself: the second is a shade lower.
        Follow(people_[i], "crowd", "people", peopleLevel_[i] > 0.0f ? &peopleAt_[i] : nullptr,
               peopleLevel_[i], i == 0 ? 0.0f : -0.07f, {.reference = 5.0f, .radius = 55.0f, .rolloff = 0.9f},
               seconds);
    }
}

void Soundscape::Call(const zoo::Zoo& zoo, const zoo::Animal& animal, const Vector3& ears, bool loud) {
    const auto& species = zoo.SpeciesList().All();
    if (animal.species < 0 || static_cast<std::size_t>(animal.species) >= species.size())
        return;
    const zoo::SpeciesDef& def = species[static_cast<std::size_t>(animal.species)];
    // Of the calls the species has, those the library holds; the loud one when the
    // animal really calls out, any of them when it merely makes itself heard.
    const std::string* chosen = nullptr;
    float chosenReach = 0.0f;
    int known = 0;
    for (const std::string& name : def.sounds) {
        if (!library_.contains(name))
            continue;
        ++known;
        const float reach = Reach(name);
        const bool better = loud ? reach > chosenReach : Chance() < 1.0f / static_cast<float>(known);
        if (!chosen || better) {
            chosen = &name;
            chosenReach = reach;
        }
    }
    if (!chosen)
        return;
    const Sound* sound = Pick(*chosen);
    if (!sound)
        return;
    const Vector3 mouth = On(zoo, animal.position, std::max(0.3f, def.shoulderHeightM));
    const float away = Vector3::Distance(mouth, ears);
    if (away > chosenReach)
        return;
    const auto gain = gains_.find(sound);
    system_.Play(*sound,
                 {.category = "animals",
                  .volume = gain != gains_.end() ? gain->second : 1.0f,
                  .pitch = (Chance() - 0.5f) * 0.1f,
                  .position = mouth,
                  .falloff = {.reference = chosenReach * 0.07f, .radius = chosenReach, .rolloff = 0.85f},
                  .priority = 2});
    ++figures_.calls;
    char text[96];
    std::snprintf(text, sizeof text, "%s, %.0f m away", chosen->c_str(), static_cast<double>(away));
    figures_.lastCall = text;
}

void Soundscape::HearAnimals(const zoo::Zoo& zoo, const Vector3& ears, float seconds) {
    for (auto& [id, heard] : animals_)
        heard.seen = false;
    untilCall_ -= seconds;
    const zoo::Animal* speaker = nullptr;
    int candidates = 0;
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
        const bool calling = animal.activity == zoo::AnimalActivity::Vocalize && !animal.moving;
        const bool called = heard.activity == zoo::AnimalActivity::Vocalize;
        heard.activity = calling ? zoo::AnimalActivity::Vocalize : zoo::AnimalActivity::Idle;
        if (calling && !called && heard.quiet > 3.0f) {
            Call(zoo, animal, ears, true);
            heard.quiet = 0.0f;
            continue;
        }
        // Between its calls an awake animal now and then makes itself heard.
        const bool awake = animal.activity != zoo::AnimalActivity::Sleep && !animal.shelter;
        const float dx = animal.position.x - ears.X;
        const float dz = animal.position.z - ears.Z;
        if (untilCall_ <= 0.0f && awake && heard.quiet > 25.0f && dx * dx + dz * dz < 90.0f * 90.0f) {
            ++candidates;
            if (Chance() < 1.0f / static_cast<float>(candidates))
                speaker = &animal;
        }
    }
    if (untilCall_ <= 0.0f) {
        untilCall_ = 7.0f + Chance() * 11.0f;
        if (speaker) {
            Call(zoo, *speaker, ears, Chance() < 0.3f);
            animals_[speaker->id.value].quiet = 0.0f;
        }
    }
    std::erase_if(animals_, [](const auto& entry) { return !entry.second.seen; });
}

void Soundscape::Step(const zoo::Zoo& zoo, const Visitor& visitor) {
    const zoo::TileCoord tile = zoo::World::TileAt(visitor.feet);
    if (!zoo.Map().InBounds(tile))
        return;
    const Sound* step = Pick(Surface(zoo.Map().At(tile)));
    if (!step)
        return;
    system_.Play(*step, {.category = "footsteps",
                         .volume = visitor.running ? 1.0f : 0.7f,
                         .pitch = (Chance() - 0.5f) * 0.16f,
                         .pan = (figures_.steps % 2 == 0 ? -0.08f : 0.08f),
                         .priority = 2});
    ++figures_.steps;
}

void Soundscape::Update(const zoo::Zoo& zoo, const Audio::Listener& ears, const Visitor& visitor,
                        float seconds) {
    if (library_.empty())
        return;
    if (!beds_)
        StartBeds();
    level_ = Eased(level_, visitor.paused ? 0.3f : 1.0f, 4.0f, seconds);
    system_.SetMasterVolume(volume_ * level_);
    system_.SetListener(ears);

    survey_ -= seconds;
    HearWater(zoo, ears.position, seconds);
    HearPeople(zoo, ears.position, seconds);
    if (survey_ <= 0.0f)
        survey_ = 0.5f;
    if (!visitor.paused) {
        HearAnimals(zoo, ears.position, seconds);
        if (visitor.stepped)
            Step(zoo, visitor);
    }

    system_.Update(seconds);
    figures_.device = system_.State() == Audio::AudioState::Ready;
    figures_.voices = static_cast<int>(system_.VoiceCount());
    figures_.audible = static_cast<int>(system_.AudibleCount());
}

}  // namespace zoogame
