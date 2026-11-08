#pragma once

#include <cstddef>

#include "config.h"

namespace phys {

struct AppOptions {
    std::size_t benchFrames = 0;        // 0 = run until the window is closed
    float spawnRatePerSecond = 4000.0f; // balls per second from the nozzle
};

// Top level of the dishonesty stack: owns the window, event loop, clock, RNG
// and spawn schedule. Calls into the honest simulation core once per fixed step.
int runApp(const SimulationConfig& cfg, const SpawnRequest& jet, const AppOptions& opts, unsigned int numThreads);

} // namespace phys
