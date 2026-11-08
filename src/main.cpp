#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>

#include "app.h"

namespace {

void printUsage(const char* argv0) {
    std::printf("usage: %s [--frames N] [--rate BALLS_PER_SECOND]\n", argv0);
    std::printf("  --frames N   run N frames, print a benchmark summary, then exit\n");
    std::printf("  --rate R     spawn rate of the jet (default 4000 balls/s)\n");
    std::printf("keys: ESC quit, SPACE clear all balls\n");
}

} // namespace

int main(int argc, char** argv) {
    phys::AppOptions opts;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--frames") == 0 && i + 1 < argc) {
            opts.benchFrames = std::strtoull(argv[++i], nullptr, 10);
        } else if (std::strcmp(argv[i], "--rate") == 0 && i + 1 < argc) {
            opts.spawnRatePerSecond = static_cast<float>(std::strtod(argv[++i], nullptr));
        } else if (std::strcmp(argv[i], "--help") == 0) {
            printUsage(argv[0]);
            return 0;
        } else {
            std::fprintf(stderr, "unknown argument: %s\n", argv[i]);
            printUsage(argv[0]);
            return 2;
        }
    }

    phys::SimulationConfig cfg;
    phys::SpawnRequest jet;
    unsigned int threads = std::thread::hardware_concurrency();
    if (threads == 0) threads = 1;

    return phys::runApp(cfg, jet, opts, threads);
}
