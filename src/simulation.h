#pragma once

#include "config.h"
#include "particles.h"

namespace phys {

// One fixed step of the whole system. Honest: no I/O, no globals, no rendering —
// state in, state out. The app layer owns the clock and decides when this runs.
void stepSimulation(ParticleSystem& ps, const SimulationConfig& cfg, float dt, unsigned int numThreads);

} // namespace phys
