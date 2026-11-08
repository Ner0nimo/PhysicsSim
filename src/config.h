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
    float wallFriction = 0.05f;         // fraction of tangential velocity removed on wall hit
    float damping = 0.999f;             // velocity retention per fixed step
    float contactDamping = 0.2f;        // velocity retained per step for balls in contact (speed < contactDampingSpeed)
    float contactDampingSpeed = 400.0f; // px/s; falling balls in mid-air have no contact and are unaffected
    float positionalCorrection = 0.8f;  // fraction of overlap resolved per pair per iteration
    float maxPairCorrection = 1.0f;     // px; caps one pair's push so deep overlaps can't explode velocities
    float maxVelocity = 1500.0f;        // px/s; hard speed cap (jet speed is the practical maximum)
    int collisionIterations = 2;

    float sleepSpeed = 60.0f;           // px/s below which a ball counts as "still"
    float sleepAfter = 0.3f;            // seconds of stillness before a ball sleeps
    float wakeCorr = 2.0f;              // px of positional push in one step that counts as a hard impact
    float restitutionThreshold = 50.0f; // px/s; below this, wall contacts are plastic (e=0)

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
