#include "simulation.h"

#include <algorithm>
#include <cmath>

#include "spatial_grid.h"
#include "threading.h"

namespace phys {

namespace {

// Integrate one ball: gravity, damping, predictive wall bounce, then move.
// A sleeping ball is frozen (v = 0) until a hard impact wakes it (see separatePair).
void integrateBall(ParticleSystem& ps, std::size_t i, const SimulationConfig& cfg, float dt) {
    ps.contact[i] = 0; // re-set by the pair solve if the ball touches something this step
    if (ps.sleepTimer[i] >= cfg.sleepAfter) {
        ps.vx[i] = 0.0f;
        ps.vy[i] = 0.0f;
        return;
    }
    ps.xPrev[i] = ps.x[i];
    ps.yPrev[i] = ps.y[i];

    ps.vy[i] += cfg.gravity * dt;
    ps.vx[i] *= cfg.damping;
    ps.vy[i] *= cfg.damping;

    // Predictive wall: reflect the velocity before moving, so the ball never
    // penetrates and the PBD velocity update below keeps the bounce.
    float r = ps.radius[i];
    float right = cfg.window.width - r;
    float bottom = cfg.window.height - r;
    float nextX = ps.x[i] + ps.vx[i] * dt;
    float nextY = ps.y[i] + ps.vy[i] * dt;
    if (nextX < r) {
        float e = std::abs(ps.vx[i]) > cfg.restitutionThreshold ? cfg.wallRestitution : 0.0f;
        ps.vx[i] = e * std::abs(ps.vx[i]);
        ps.vy[i] *= (1.0f - cfg.wallFriction);
    } else if (nextX > right) {
        float e = std::abs(ps.vx[i]) > cfg.restitutionThreshold ? cfg.wallRestitution : 0.0f;
        ps.vx[i] = -e * std::abs(ps.vx[i]);
        ps.vy[i] *= (1.0f - cfg.wallFriction);
    }
    if (nextY < r) {
        float e = std::abs(ps.vy[i]) > cfg.restitutionThreshold ? cfg.wallRestitution : 0.0f;
        ps.vy[i] = e * std::abs(ps.vy[i]);
        ps.vx[i] *= (1.0f - cfg.wallFriction);
    } else if (nextY > bottom) {
        float e = std::abs(ps.vy[i]) > cfg.restitutionThreshold ? cfg.wallRestitution : 0.0f;
        ps.vy[i] = -e * std::abs(ps.vy[i]);
        ps.vx[i] *= (1.0f - cfg.wallFriction);
    }

    ps.x[i] += ps.vx[i] * dt;
    ps.y[i] += ps.vy[i] * dt;

    float speedSq = ps.vx[i] * ps.vx[i] + ps.vy[i] * ps.vy[i];
    if (speedSq < cfg.sleepSpeed * cfg.sleepSpeed) ps.sleepTimer[i] += dt;
    else ps.sleepTimer[i] = 0.0f;
}

// Safety clamp: positions never leave the window.
void clampWall(ParticleSystem& ps, std::size_t i, const SimulationConfig& cfg) {
    float r = ps.radius[i];
    ps.x[i] = std::clamp(ps.x[i], r, cfg.window.width - r);
    ps.y[i] = std::clamp(ps.y[i], r, cfg.window.height - r);
}

// Positional-only pair separation (PBD). Two sleeping balls don't interact —
// the settled pile is frozen and costs nothing. A large correction means a
// hard impact and wakes both balls.
void separatePair(ParticleSystem& ps, std::size_t i, std::size_t j, const SimulationConfig& cfg) {
    bool iAsleep = ps.sleepTimer[i] >= cfg.sleepAfter;
    bool jAsleep = ps.sleepTimer[j] >= cfg.sleepAfter;
    if (iAsleep && jAsleep) return;

    float dx = ps.x[j] - ps.x[i];
    float dy = ps.y[j] - ps.y[i];
    float radiusSum = ps.radius[i] + ps.radius[j];
    float distSq = dx * dx + dy * dy;
    if (distSq >= radiusSum * radiusSum) return;

    float dist = std::sqrt(distSq);
    float nx, ny;
    if (dist < 1e-6f) {
        nx = 1.0f;
        ny = 0.0f;
        dist = 1e-6f;
    } else {
        nx = dx / dist;
        ny = dy / dist;
    }

    float corr = std::min((radiusSum - dist) * cfg.positionalCorrection * 0.5f, cfg.maxPairCorrection);
    ps.x[i] -= nx * corr;
    ps.y[i] -= ny * corr;
    ps.x[j] += nx * corr;
    ps.y[j] += ny * corr;
    ps.contact[i] = 1;
    ps.contact[j] = 1;

    if (corr > cfg.wakeCorr) {
        ps.sleepTimer[i] = 0.0f;
        ps.sleepTimer[j] = 0.0f;
    }
}

// Neighbor cells that cover every unordered cell pair exactly once
// (all have a higher cell index than the current one).
constexpr int kNeighborDx[4] = {1, 0, 1, -1};
constexpr int kNeighborDy[4] = {0, 1, 1, 1};

// After applyOrder(), balls of one cell are the contiguous range
// [grid.begin(cell), grid.end(cell)) — all pair access stays cache-local.
void separateRow(const SpatialGrid& grid, ParticleSystem& ps, const SimulationConfig& cfg, int cy) {
    for (int cx = 0; cx < grid.cols; ++cx) {
        int cell = grid.cellIndex(cx, cy);
        if (grid.awakeInCell[cell] > 0) {
            for (int a = grid.begin(cell); a < grid.end(cell); ++a) {
                for (int b = a + 1; b < grid.end(cell); ++b) {
                    separatePair(ps, static_cast<std::size_t>(a), static_cast<std::size_t>(b), cfg);
                }
            }
        }
        for (int n = 0; n < 4; ++n) {
            int nx = cx + kNeighborDx[n];
            int ny = cy + kNeighborDy[n];
            if (nx < 0 || nx >= grid.cols || ny >= grid.rows) continue;
            int other = grid.cellIndex(nx, ny);
            if (grid.awakeInCell[cell] == 0 && grid.awakeInCell[other] == 0) continue;
            for (int a = grid.begin(cell); a < grid.end(cell); ++a) {
                for (int b = grid.begin(other); b < grid.end(other); ++b) {
                    separatePair(ps, static_cast<std::size_t>(a), static_cast<std::size_t>(b), cfg);
                }
            }
        }
    }
}

// Rows of equal parity are processed in parallel; each row writes only balls in
// its own row and the row below, so same-parity rows never share a ball.
void separatePairs(const SpatialGrid& grid, ParticleSystem& ps, const SimulationConfig& cfg, unsigned int numThreads) {
    for (int parity = 0; parity < 2; ++parity) {
        parallelFor(static_cast<std::size_t>(grid.rows), numThreads, [&](std::size_t row) {
            if (static_cast<int>(row & 1u) == parity) separateRow(grid, ps, cfg, static_cast<int>(row));
        });
    }
}

// PBD velocity update: velocity is derived from the actual position change,
// so a resting contact (position restored) ends with v ~ 0 instead of boiling.
void updateVelocity(ParticleSystem& ps, std::size_t i, const SimulationConfig& cfg, float dt) {
    if (ps.sleepTimer[i] >= cfg.sleepAfter) {
        ps.vx[i] = 0.0f;
        ps.vy[i] = 0.0f;
        return;
    }
    ps.vx[i] = (ps.x[i] - ps.xPrev[i]) / dt;
    ps.vy[i] = (ps.y[i] - ps.yPrev[i]) / dt;
    float speed = std::sqrt(ps.vx[i] * ps.vx[i] + ps.vy[i] * ps.vy[i]);
    // Balls in contact with something get damped hard, so a resting pile stops
    // churning and can fall asleep. Free-falling balls have no contact flag.
    if (ps.contact[i] && speed < cfg.contactDampingSpeed) {
        ps.vx[i] *= cfg.contactDamping;
        ps.vy[i] *= cfg.contactDamping;
        speed *= cfg.contactDamping;
    }
    if (speed > cfg.maxVelocity) {
        ps.vx[i] *= cfg.maxVelocity / speed;
        ps.vy[i] *= cfg.maxVelocity / speed;
    }
}

} // namespace

void stepSimulation(ParticleSystem& ps, const SimulationConfig& cfg, float dt, unsigned int numThreads) {
    if (ps.count == 0) return;

    parallelFor(ps.count, numThreads, [&](std::size_t i) { integrateBall(ps, i, cfg, dt); });
    parallelFor(ps.count, numThreads, [&](std::size_t i) { clampWall(ps, i, cfg); });

    SpatialGrid grid;
    grid.build(ps, cfg);
    applyOrder(ps, grid.order);

    for (int iteration = 0; iteration < cfg.collisionIterations; ++iteration) {
        separatePairs(grid, ps, cfg, numThreads);
    }
    parallelFor(ps.count, numThreads, [&](std::size_t i) { clampWall(ps, i, cfg); });

    parallelFor(ps.count, numThreads, [&](std::size_t i) { updateVelocity(ps, i, cfg, dt); });
}

} // namespace phys
