#pragma once

#include <cstddef>
#include <cstdint>
#include <random>
#include <vector>

#include "config.h"

namespace phys {

// Struct-of-arrays storage for up to `capacity` balls. Append-only: once full,
// no more balls are added (the cap is a hard stop, not a ring buffer).
struct ParticleSystem {
    std::vector<float> x, y, vx, vy, radius;
    std::vector<float> xPrev, yPrev; // position at step start, for the PBD velocity update
    std::vector<std::uint32_t> color;
    std::vector<float> sleepTimer;   // seconds the ball has been slow; >= cfg.sleepAfter means asleep
    std::vector<std::uint8_t> contact; // set during the pair solve when the ball touched something this step

    std::size_t count = 0;
    float hueCursor = 0.0f; // rainbow mode: hue advances per spawned ball

    void reserve(std::size_t capacity);
    void clear();
    std::size_t capacity() const { return x.size(); }
    bool full() const { return count >= capacity(); }
};

// Honest: appends up to `want` balls described by `jet`, drawing all randomness
// from `rng`. Stops at the capacity cap; SpawnResult tells the caller what happened.
SpawnResult spawnJet(ParticleSystem& ps, const SpawnRequest& jet, std::size_t want, std::mt19937& rng);

// Honest: reorders all arrays so that new index j holds old ball order[j].
// Used to make grid cells contiguous in memory.
void applyOrder(ParticleSystem& ps, const std::vector<int>& order);

} // namespace phys
