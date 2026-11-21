// SPDX-License-Identifier: MIT
#include "Bench.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>

#include "CnaEngine/Graphics/RenderStatistics.hpp"

namespace zoogame {

namespace {

/// A line of /proc/self/status in megabytes ("VmRSS", "VmHWM"); 0 where there is no such file.
double StatusMegabytes(const char* key) {
    std::ifstream status("/proc/self/status");
    std::string line;
    while (std::getline(status, line)) {
        if (line.compare(0, std::strlen(key), key) == 0 && line[std::strlen(key)] == ':')
            return std::strtod(line.c_str() + std::strlen(key) + 1, nullptr) / 1024.0;
    }
    return 0.0;
}

}  // namespace

void Bench::Frame() {
    if (!on)
        return;
    const Clock::time_point now = Clock::now();
    if (seen_++ >= kSkipped) {
        if (measuring_)
            frames_.push_back(std::chrono::duration<double>(now - last_).count());
        measuring_ = true;
    }
    last_ = now;
}

void Bench::Drawn(unsigned long long drawCalls, unsigned long long primitives) {
    if (!on || !measuring_)
        return;
    drawCalls_ += drawCalls;
    primitives_ += primitives;
    ++drawnFrames_;
}

void Bench::Print(const char* what) const {
    if (!on || frames_.empty())
        return;
    std::vector<double> sorted = frames_;
    std::sort(sorted.begin(), sorted.end());
    double sum = 0.0;
    for (const double seconds : sorted)
        sum += seconds;
    const auto at = [&](double fraction) {
        const auto index = static_cast<std::size_t>(fraction * static_cast<double>(sorted.size() - 1) + 0.5);
        return sorted[std::min(index, sorted.size() - 1)] * 1000.0;
    };
    const double frames = static_cast<double>(sorted.size());
    const double mean = sum / frames * 1000.0;
    std::printf("zoo: bench %s: %zu frames, %.1f fps; frame ms mean %.2f, median %.2f, p95 %.2f, p99 %.2f, "
                "max %.2f\n",
                what, sorted.size(), 1000.0 / mean, mean, at(0.5), at(0.95), at(0.99), sorted.back() * 1000.0);
    static constexpr const char* kNames[SectionCount] = {"simulation", "animals", "people",     "sound",
                                                         "rest",       "submit",  "people pass", "animal gaze"};
    std::string line;
    double known = 0.0;
    for (int i = 0; i < SectionCount; ++i) {
        const double ms = sections_[i] / frames * 1000.0;
        known += ms;
        char text[64];
        std::snprintf(text, sizeof text, " %s %.2f,", kNames[i], ms);
        line += text;
    }
    std::printf("zoo: bench %s: game ms a frame:%s engine and driver %.2f\n", what, line.c_str(),
                std::max(0.0, mean - known));
    if (drawnFrames_ > 0)
        std::printf("zoo: bench %s: %.0f draw calls, %.0f triangles a frame (engine passes; avatars draw "
                    "their own)\n",
                    what, static_cast<double>(drawCalls_) / static_cast<double>(drawnFrames_),
                    static_cast<double>(primitives_) / static_cast<double>(drawnFrames_));
    std::printf("zoo: bench %s: memory %.0f MB resident, %.0f MB at most\n", what, StatusMegabytes("VmRSS"),
                StatusMegabytes("VmHWM"));
    const CnaEngine::Graphics::GpuMemoryStatistics gpu = CnaEngine::Graphics::CurrentGpuMemory();
    std::printf("zoo: bench %s: owned GPU meshes %lld, vertex %.1f MiB, index %.1f MiB\n", what,
                static_cast<long long>(gpu.meshes), static_cast<double>(gpu.vertexBytes) / (1024.0 * 1024.0),
                static_cast<double>(gpu.indexBytes) / (1024.0 * 1024.0));
}

}  // namespace zoogame
