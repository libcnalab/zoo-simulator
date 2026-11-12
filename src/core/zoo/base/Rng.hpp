// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>

namespace zoo {

/// PCG32 (O'Neill, pcg-random.org, "pcg32" minimal variant): small, fast and
/// identical on every platform, which the deterministic simulation relies on.
/// The whole state is two integers, so it is saved with the game.
class Rng {
public:
    constexpr Rng() { Seed(0x853c49e6748fea9bULL, 0xda3e39cb94b95bdbULL); }
    constexpr explicit Rng(std::uint64_t seed, std::uint64_t stream = 1) { Seed(seed, stream); }

    constexpr void Seed(std::uint64_t seed, std::uint64_t stream = 1) {
        state_ = 0;
        inc_ = (stream << 1u) | 1u;
        NextU32();
        state_ += seed;
        NextU32();
    }

    constexpr std::uint32_t NextU32() {
        const std::uint64_t old = state_;
        state_ = old * 6364136223846793005ULL + inc_;
        const auto xorshifted = static_cast<std::uint32_t>(((old >> 18u) ^ old) >> 27u);
        const auto rot = static_cast<std::uint32_t>(old >> 59u);
        return (xorshifted >> rot) | (xorshifted << ((32u - rot) & 31u));
    }

    /// Uniform in [0, 1).
    constexpr float NextFloat() { return static_cast<float>(NextU32() >> 8u) * (1.0f / 16777216.0f); }

    /// Uniform in [lo, hi).
    constexpr float Range(float lo, float hi) { return lo + (hi - lo) * NextFloat(); }

    /// Uniform integer in [lo, hi] (both inclusive); `lo` when hi <= lo. Unbiased.
    constexpr int RangeInt(int lo, int hi) {
        if (hi <= lo) {
            return lo;
        }
        const auto span = static_cast<std::uint32_t>(hi - lo) + 1u;
        const std::uint32_t threshold = (0u - span) % span;
        for (;;) {
            const std::uint32_t r = NextU32();
            if (r >= threshold) {
                return lo + static_cast<int>(r % span);
            }
        }
    }

    /// True with the given probability.
    constexpr bool Chance(float probability) { return NextFloat() < probability; }

    constexpr std::uint64_t State() const { return state_; }
    constexpr std::uint64_t Stream() const { return inc_; }
    /// Restores a generator saved with State() / Stream().
    constexpr void Restore(std::uint64_t state, std::uint64_t stream) {
        state_ = state;
        inc_ = stream | 1u;
    }

private:
    std::uint64_t state_ = 0;
    std::uint64_t inc_ = 1;
};

/// Stateless 32 bit hash of two integers (for per-tile and per-entity variation
/// that must not consume the simulation's random stream).
constexpr std::uint32_t HashU32(std::uint32_t a, std::uint32_t b = 0x9e3779b9u) {
    std::uint32_t h = a * 0x85ebca6bu ^ (b + 0x9e3779b9u + (a << 6u) + (a >> 2u));
    h ^= h >> 16u;
    h *= 0x7feb352du;
    h ^= h >> 15u;
    h *= 0x846ca68bu;
    h ^= h >> 16u;
    return h;
}

/// HashU32 mapped to [0, 1).
constexpr float HashFloat(std::uint32_t a, std::uint32_t b = 0x9e3779b9u) {
    return static_cast<float>(HashU32(a, b) >> 8u) * (1.0f / 16777216.0f);
}

}  // namespace zoo
