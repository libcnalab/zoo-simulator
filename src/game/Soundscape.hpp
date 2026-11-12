// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
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
    /// A loop that follows something in the world (the nearest water, a group of people).
    struct Follower {
        CnaEngine::Audio::VoiceId voice = 0;
        Vector3 position = Vector3(0.0f, 0.0f, 0.0f);
        float volume = 0.0f;
        bool placed = false;
    };

    void Load(const std::filesystem::path& directory);
    const Sound* Pick(std::string_view name);
    float Chance();
    void StartBeds();
    void Follow(Follower& follower, std::string_view sound, std::string_view category, const Vector3* target,
                float volume, float pitch, const CnaEngine::Audio::Falloff& falloff, float seconds);
    void HearWater(const zoo::Zoo& zoo, const Vector3& ears, float seconds);
    void HearPeople(const zoo::Zoo& zoo, const Vector3& ears, float seconds);
    void HearAnimals(const zoo::Zoo& zoo, const Vector3& ears, float seconds);
    void Call(const zoo::Zoo& zoo, const zoo::Animal& animal, const Vector3& ears, bool loud);
    void Step(const zoo::Zoo& zoo, const Visitor& visitor);

    // The recordings outlive the voices that play them: they are declared first.
    std::unordered_map<std::string, std::vector<std::unique_ptr<Sound>>> library_;
    CnaEngine::Audio::SoundSystem system_{20};
    SoundFigures figures_;
    std::uint32_t seed_ = 0x9E3779B9u;
    float volume_ = 0.8f;
    float level_ = 1.0f;  ///< eased: 1 while walking, held down behind the pause menu
    bool beds_ = false;

    Follower water_;
    Follower people_[2];
    float survey_ = 0.0f;  ///< until the water and the people are looked for again
    Vector3 waterAt_ = Vector3(0.0f, 0.0f, 0.0f);
    bool waterNear_ = false;
    Vector3 peopleAt_[2] = {Vector3(0.0f, 0.0f, 0.0f), Vector3(0.0f, 0.0f, 0.0f)};
    float peopleLevel_[2] = {0.0f, 0.0f};

    /// What each animal did when it was last heard of, and how long it has been quiet.
    struct Heard {
        zoo::AnimalActivity activity = zoo::AnimalActivity::Idle;
        float quiet = 0.0f;
        bool seen = false;
    };
    std::unordered_map<std::uint32_t, Heard> animals_;
    float untilCall_ = 4.0f;
};

}  // namespace zoogame
