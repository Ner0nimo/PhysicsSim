#include "spatial_grid.h"

#include <algorithm>
#include <cmath>

namespace phys {

void SpatialGrid::build(const ParticleSystem& ps, const SimulationConfig& cfg) {
    cellSize = 2.0f * cfg.maxBallRadius;
    cols = std::max(1, static_cast<int>(std::ceil(cfg.window.width / cellSize)));
    rows = std::max(1, static_cast<int>(std::ceil(cfg.window.height / cellSize)));

    const int numCells = cols * rows;
    offsets.assign(numCells + 1, 0);
    awakeInCell.assign(numCells, 0);

    auto cellOf = [&](std::size_t i) {
        int cx = static_cast<int>(ps.x[i] / cellSize);
        int cy = static_cast<int>(ps.y[i] / cellSize);
        cx = std::clamp(cx, 0, cols - 1);
        cy = std::clamp(cy, 0, rows - 1);
        return cellIndex(cx, cy);
    };

    std::vector<int> cellIndexOf(ps.count);
    for (std::size_t i = 0; i < ps.count; ++i) {
        cellIndexOf[i] = cellOf(i);
        ++offsets[cellIndexOf[i] + 1];
        if (ps.sleepTimer[i] < cfg.sleepAfter) ++awakeInCell[cellIndexOf[i]];
    }
    for (int c = 0; c < numCells; ++c) offsets[c + 1] += offsets[c];

    order.assign(ps.count, 0);
    std::vector<int> cursor(offsets.begin(), offsets.end() - 1);
    for (std::size_t i = 0; i < ps.count; ++i) order[cursor[cellIndexOf[i]]++] = static_cast<int>(i);
}

} // namespace phys
