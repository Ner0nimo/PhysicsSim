#pragma once

#include <cstddef>
#include <cstdint>

#include "vec2.h"

namespace phys {

struct Bounds {
    float width;
    float height;
};

// All tunables in one place; the app layer owns this, the physics core only reads it.
struct SimulationConfig {
    Bounds window{800.0f, 450.0f};

    float gravity = 1500.0f;            // px/s^2, +y is down (SDL coordinates)
    float wallRestitution = 0.55f;
    float ballRestitution = 0.35f;
    float wallFriction = 0.05f;         // fraction of tangential velocity removed on wall hit
    float ballFriction = 0.02f;         // same for ball-ball hits
    float damping = 0.999f;             // velocity retention per fixed step
    float positionalCorrection = 0.8f;  // fraction of overlap resolved per iteration
    int collisionIterations = 2;

    std::size_t maxBalls = 100000;
    float maxBallRadius = 2.5f;         // grid cell size = 2 * maxBallRadius

    float fixedDt = 1.0f / 60.0f;
    int maxStepsPerFrame = 5;

    unsigned int seed = 1337u;
};

// Describes the water jet. color == 0 means "random hue per ball".
struct SpawnRequest {
    Vec2 origin;                        // nozzle position, set by the app from the window size
    float directionRadians = -0.17f;    // 0 = +x, negative = slightly upward
    float spreadRadians = 0.04f;        // half-angle of the cone — tight beam, little scatter
    float minSpeed = 1500.0f;           // px/s — the "pressure"
    float maxSpeed = 1750.0f;
    float minRadius = 1.5f;
    float maxRadius = 2.5f;
    std::uint32_t color = 0;
};

struct SpawnResult {
    std::size_t spawned = 0;
    bool capReached = false;
};

} // namespace phys
