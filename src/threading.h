#pragma once

#include <algorithm>
#include <cstddef>
#include <thread>
#include <vector>

namespace phys {

// Runs fn(i) for i in [0, n) on up to numThreads threads with strided assignment
// (thread k handles i = k, k+t, k+2t, ...), which balances uneven per-index cost.
// Callers must guarantee that concurrent indices never touch shared state.
template <typename F>
void parallelFor(std::size_t n, unsigned int numThreads, F&& fn) {
    if (n == 0) return;
    if (numThreads <= 1 || n < 64) {
        for (std::size_t i = 0; i < n; ++i) fn(i);
        return;
    }
    unsigned int t = std::min(numThreads, static_cast<unsigned int>(n));
    std::vector<std::thread> pool;
    pool.reserve(t);
    for (unsigned int k = 0; k < t; ++k) {
        pool.emplace_back([k, t, n, &fn] {
            for (std::size_t i = k; i < n; i += t) fn(i);
        });
    }
    for (auto& th : pool) th.join();
}

} // namespace phys
