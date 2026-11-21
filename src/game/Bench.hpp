// SPDX-License-Identifier: MIT
#pragma once

#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

namespace zoogame {

/// --bench: what a run costs, measured on the machine it runs on.
///
/// Frame times are the wall clock between two updates (so they hold the
/// update, the draw and the presentation); the sections are the processor
/// time the game itself spends in its own parts of a frame. The first frames
/// are left out: the avatars' cast is still being put together then.
class Bench {
public:
    enum Section { Simulation, Animals, People, Sound, Rest, Submit, PeoplePass, AnimalLook, SectionCount };

    using Clock = std::chrono::steady_clock;

    bool on = false;

    /// Call once at the start of every update.
    void Frame();
    void Add(Section section, double seconds) {
        if (on && measuring_)
            sections_[section] += seconds;
    }
    /// Seconds in the sections of an update so far (not the passes of the draw).
    double Timed() const {
        return sections_[Simulation] + sections_[Animals] + sections_[People] + sections_[Sound] +
               sections_[AnimalLook];
    }
    /// Counts of the frame the renderer drew last.
    void Drawn(unsigned long long drawCalls, unsigned long long primitives);
    void Print(const char* what) const;

    /// Times a scope into a section.
    class Scope {
    public:
        Scope(Bench& bench, Section section) : bench_(bench), section_(section), began_(Clock::now()) {}
        ~Scope() { bench_.Add(section_, std::chrono::duration<double>(Clock::now() - began_).count()); }
        Scope(const Scope&) = delete;
        Scope& operator=(const Scope&) = delete;

    private:
        Bench& bench_;
        Section section_;
        Clock::time_point began_;
    };

private:
    static constexpr int kSkipped = 40;
    int seen_ = 0;
    bool measuring_ = false;
    Clock::time_point last_{};
    std::vector<double> frames_;
    double sections_[SectionCount]{};
    unsigned long long drawCalls_ = 0;
    unsigned long long primitives_ = 0;
    unsigned long long drawnFrames_ = 0;
};

}  // namespace zoogame
