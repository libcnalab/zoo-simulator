// SPDX-License-Identifier: MIT
#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "CnaEngine/Audio/SoundSystem.hpp"

#include "Common.hpp"
#include "Visit.hpp"
#include "zoo/sim/Zoo.hpp"

namespace zoogame {

/// What a visitor hears: the park's air, water where there is water, the murmur
/// where people gather, the animals' calls from where the animals are, and the
/// visitor's own steps on what they walk on.
///
/// The recordings come from assets/audio/sounds.json (their origins and licences:
/// assets/audio/SOURCES.md). A sound the library lacks is simply not played, and
/// without a library or an audio device the zoo is silent; nothing fails.
/// The manifest is read at start, but each Ogg is decoded only when first played.
class Soundscape {
public:
    /// What the soundscape needs to know of the visitor each frame.
    struct Visitor {
        zoo::Vec2 feet;
        bool stepped = false;  ///< a foot came down in this frame
        bool running = false;
        bool paused = false;  ///< the pause menu is up: the world is held back
    };

    explicit Soundscape(const std::filesystem::path& assets);
    ~Soundscape();

    /// 0 silent .. 1 full.
    void SetVolume(float volume);
    /// Another zoo was loaded: what was heard of the old one stops.
    void Reset();
    void Update(const zoo::Zoo& zoo, const CnaEngine::Audio::Listener& ears, const Visitor& visitor,
                float seconds);
    const SoundFigures& Figures() const { return figures_; }

private:
    using Sound = Microsoft::Xna::Framework::Audio::SoundEffect;
    struct Recording {
        std::filesystem::path path;
        std::string relative;
        std::unique_ptr<Sound> sound;
        float gain = 1.0f;
        bool attempted = false;
    };
    /// The takes of one sound, and which of them was played last: no take twice in a row.
    struct Takes {
        std::vector<Recording> recordings;
        int last = -1;
    };
    /// A loop that is everywhere (the air of the park, the leaves in the wind).
    struct Bed {
        CnaEngine::Audio::VoiceId voice = 0;
        float level = 0.0f;
    };
    /// A loop that follows something in the world (the nearest water, a group of people).
    struct Follower {
        CnaEngine::Audio::VoiceId voice = 0;
        Vector3 position = Vector3(0.0f, 0.0f, 0.0f);
        float volume = 0.0f;
        bool placed = false;
    };
    /// A call going on: where it really comes from, so that it stays there while the head turns.
    struct Calling {
        CnaEngine::Audio::VoiceId voice = 0;
        Vector3 from = Vector3(0.0f, 0.0f, 0.0f);
    };
    /// What an animal does that can be heard.
    enum class Cue : std::uint8_t { None, Calling, Lively };

    void LoadLibrary(const std::filesystem::path& directory);
    bool Decode(Recording& recording);
    const Recording* Pick(std::string_view name);
    float Chance();
    void Lay(Bed& bed, std::string_view sound, float wanted, int priority, float seconds);
    void HearAir(const zoo::Zoo& zoo, float seconds);
    void Follow(Follower& follower, std::string_view sound, std::string_view category, const Vector3* target,
                float volume, float pitch, const CnaEngine::Audio::Falloff& falloff, float near,
                const CnaEngine::Audio::Listener& ears, float seconds);
    void HearWater(const zoo::Zoo& zoo, const CnaEngine::Audio::Listener& ears, float seconds);
    void HearPeople(const zoo::Zoo& zoo, const CnaEngine::Audio::Listener& ears, float seconds);
    void HearAnimals(const zoo::Zoo& zoo, const CnaEngine::Audio::Listener& ears, float seconds);
    bool Call(const zoo::Zoo& zoo, const zoo::Animal& animal, const CnaEngine::Audio::Listener& ears,
              bool loud, const char* why);
    double Clock() const;
    void Step(const zoo::Zoo& zoo, const Visitor& visitor);

    // The recordings outlive the voices that play them: they are declared first.
    std::unordered_map<std::string, Takes> library_;
    CnaEngine::Audio::SoundSystem system_{20};
    SoundFigures figures_;
    std::uint32_t seed_ = 0x9E3779B9u;
    float volume_ = 0.8f;
    float level_ = 1.0f;  ///< eased: 1 while walking, held down behind the pause menu

    Bed air_;
    Bed leaves_;
    Follower water_;
    Follower fountain_;
    Follower people_[2];
    float survey_ = 0.0f;  ///< until the water and the people are looked for again
    Vector3 waterAt_ = Vector3(0.0f, 0.0f, 0.0f);
    bool waterNear_ = false;
    Vector3 fountainAt_ = Vector3(0.0f, 0.0f, 0.0f);
    bool fountainNear_ = false;
    int fountainDef_ = -2;  ///< the fountain's index in the catalog; -2 not looked up yet, -1 there is none
    Vector3 peopleAt_[2] = {Vector3(0.0f, 0.0f, 0.0f), Vector3(0.0f, 0.0f, 0.0f)};
    float peopleLevel_[2] = {0.0f, 0.0f};
    int peopleCount_[2] = {0, 0};

    /// What each animal did when it was last heard of, and how long it has been quiet.
    struct Heard {
        Cue cue = Cue::None;
        float quiet = 0.0f;
        bool seen = false;
    };
    std::unordered_map<std::uint32_t, Heard> animals_;
    std::array<Calling, 8> calling_{};
    float sinceSoft_ = 60.0f;  ///< since an animal last made one of its quiet sounds

    std::chrono::steady_clock::time_point began_;
    std::FILE* log_ = nullptr;
    float untilState_ = 0.0f;
};

}  // namespace zoogame
