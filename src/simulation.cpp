#include "simulation.h"

#include <cmath>

#include "spatial_grid.h"
#include "threading.h"

namespace phys {

namespace {

// One level down from stepSimulation: a single ball under gravity and damping.
void integrateBall(ParticleSystem& ps, std::size_t i, const SimulationConfig& cfg, float dt) {
    ps.vy[i] += cfg.gravity * dt;
    ps.vx[i] *= cfg.damping;
    ps.vy[i] *= cfg.damping;
    ps.x[i] += ps.vx[i] * dt;
    ps.y[i] += ps.vy[i] * dt;
}

// A single ball against the four window walls.
void resolveWall(ParticleSystem& ps, std::size_t i, const SimulationConfig& cfg) {
    float r = ps.radius[i];
    float right = cfg.window.width - r;
    float bottom = cfg.window.height - r;

    if (ps.x[i] < r) {
        ps.x[i] = r;
        ps.vx[i] = -ps.vx[i] * cfg.wallRestitution;
        ps.vy[i] *= (1.0f - cfg.wallFriction);
    } else if (ps.x[i] > right) {
        ps.x[i] = right;
        ps.vx[i] = -ps.vx[i] * cfg.wallRestitution;
        ps.vy[i] *= (1.0f - cfg.wallFriction);
    }
    if (ps.y[i] < r) {
        ps.y[i] = r;
        ps.vy[i] = -ps.vy[i] * cfg.wallRestitution;
        ps.vx[i] *= (1.0f - cfg.wallFriction);
    } else if (ps.y[i] > bottom) {
        ps.y[i] = bottom;
        ps.vy[i] = -ps.vy[i] * cfg.wallRestitution;
        ps.vx[i] *= (1.0f - cfg.wallFriction);
    }
}

// A single overlapping pair: positional correction (also baked into the velocity,
// PBD-style, so a pile supports itself without many iterations) plus an impulse.
void resolvePair(ParticleSystem& ps, std::size_t i, std::size_t j, const SimulationConfig& cfg, float dt) {
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

    float overlap = radiusSum - dist;
    float corr = overlap * cfg.positionalCorrection * 0.5f;
    ps.x[i] -= nx * corr;
    ps.y[i] -= ny * corr;
    ps.x[j] += nx * corr;
    ps.y[j] += ny * corr;

    float velCorr = corr / dt;
    ps.vx[i] -= nx * velCorr;
    ps.vy[i] -= ny * velCorr;
    ps.vx[j] += nx * velCorr;
    ps.vy[j] += ny * velCorr;

    float relVx = ps.vx[j] - ps.vx[i];
    float relVy = ps.vy[j] - ps.vy[i];
    float vn = relVx * nx + relVy * ny;
    if (vn < 0.0f) {
        float impulse = -(1.0f + cfg.ballRestitution) * vn * 0.5f;
        ps.vx[i] -= nx * impulse;
        ps.vy[i] -= ny * impulse;
        ps.vx[j] += nx * impulse;
        ps.vy[j] += ny * impulse;

        float tx = relVx - vn * nx;
        float ty = relVy - vn * ny;
        float f = cfg.ballFriction * 0.5f;
        ps.vx[i] += tx * f;
        ps.vy[i] += ty * f;
        ps.vx[j] -= tx * f;
        ps.vy[j] -= ty * f;
    }
}

// Neighbor cells that cover every unordered cell pair exactly once
// (all have a higher cell index than the current one).
constexpr int kNeighborDx[4] = {1, 0, 1, -1};
constexpr int kNeighborDy[4] = {0, 1, 1, 1};

// After applyOrder(), balls of one cell are the contiguous range
// [grid.begin(cell), grid.end(cell)) — all pair access stays cache-local.
void collideRow(const SpatialGrid& grid, ParticleSystem& ps, const SimulationConfig& cfg, float dt, int cy) {
    for (int cx = 0; cx < grid.cols; ++cx) {
        int cell = grid.cellIndex(cx, cy);
        for (int a = grid.begin(cell); a < grid.end(cell); ++a) {
            for (int b = a + 1; b < grid.end(cell); ++b) {
                resolvePair(ps, static_cast<std::size_t>(a), static_cast<std::size_t>(b), cfg, dt);
            }
        }
        for (int n = 0; n < 4; ++n) {
            int nx = cx + kNeighborDx[n];
            int ny = cy + kNeighborDy[n];
            if (nx < 0 || nx >= grid.cols || ny >= grid.rows) continue;
            int other = grid.cellIndex(nx, ny);
            for (int a = grid.begin(cell); a < grid.end(cell); ++a) {
                for (int b = grid.begin(other); b < grid.end(other); ++b) {
                    resolvePair(ps, static_cast<std::size_t>(a), static_cast<std::size_t>(b), cfg, dt);
                }
            }
        }
    }
}

// Rows of equal parity are processed in parallel; each row writes only balls in
// its own row and the row below, so same-parity rows never share a ball.
void collidePairs(const SpatialGrid& grid, ParticleSystem& ps, const SimulationConfig& cfg, float dt, unsigned int numThreads) {
    for (int parity = 0; parity < 2; ++parity) {
        parallelFor(static_cast<std::size_t>(grid.rows), numThreads, [&](std::size_t row) {
            if (static_cast<int>(row & 1u) == parity) collideRow(grid, ps, cfg, dt, static_cast<int>(row));
        });
    }
}

} // namespace

void stepSimulation(ParticleSystem& ps, const SimulationConfig& cfg, float dt, unsigned int numThreads) {
    if (ps.count == 0) return;

    parallelFor(ps.count, numThreads, [&](std::size_t i) { integrateBall(ps, i, cfg, dt); });
    parallelFor(ps.count, numThreads, [&](std::size_t i) { resolveWall(ps, i, cfg); });

    SpatialGrid grid;
    grid.build(ps, cfg.window.width, cfg.window.height, 2.0f * cfg.maxBallRadius);
    applyOrder(ps, grid.order);

    for (int iteration = 0; iteration < cfg.collisionIterations; ++iteration) {
        collidePairs(grid, ps, cfg, dt, numThreads);
    }

    // Collisions can push balls past a wall; clamp once more so nothing renders outside.
    parallelFor(ps.count, numThreads, [&](std::size_t i) { resolveWall(ps, i, cfg); });
}

} // namespace phys
