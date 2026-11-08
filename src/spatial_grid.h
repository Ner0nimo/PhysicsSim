#pragma once

#include <vector>

#include "particles.h"

namespace phys {

// Uniform grid over the window. build() produces `order`, a permutation that
// sorts balls by cell; after applyOrder(ps, grid.order) every cell owns one
// contiguous index range [offsets[c], offsets[c+1]), which makes collision
// access cache-local.
struct SpatialGrid {
    float cellSize = 1.0f;
    int cols = 0;
    int rows = 0;
    std::vector<int> offsets; // size cells+1
    std::vector<int> order;   // permutation: new index j holds old ball order[j]

    void build(const ParticleSystem& ps, float worldWidth, float worldHeight, float cellSize);

    int cellIndex(int cx, int cy) const { return cy * cols + cx; }
    int begin(int cell) const { return offsets[cell]; }
    int end(int cell) const { return offsets[cell + 1]; }
};

} // namespace phys
