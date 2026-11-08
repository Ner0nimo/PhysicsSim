#include "app.h"

#include <algorithm>
#include <cstdio>
#include <random>

#include <SDL.h>

#include "particles.h"
#include "renderer.h"
#include "simulation.h"

namespace phys {

namespace {

constexpr const char* kWindowTitle = "PhysicsSim - Wasserstrahl";

// Fixed nozzle position: left half, upper third of the window.
Vec2 nozzlePosition(const Bounds& window) {
    return {window.width * 0.25f, window.height / 3.0f};
}

} // namespace

int runApp(const SimulationConfig& cfg, const SpawnRequest& jet, const AppOptions& opts, unsigned int numThreads) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(kWindowTitle, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                          static_cast<int>(cfg.window.width), static_cast<int>(cfg.window.height),
                                          SDL_WINDOW_SHOWN);
    if (!window) {
        std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    // Software renderer; the ball framebuffer goes up as a streaming texture once per frame.
    // With the dummy video driver this still works headless, so --frames benchmarks run anywhere.
    SDL_Renderer* sdlRenderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    SDL_Texture* ballTexture = nullptr;
    if (sdlRenderer) {
        ballTexture = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING,
                                        static_cast<int>(cfg.window.width), static_cast<int>(cfg.window.height));
    }
    SDL_Surface* frame = SDL_CreateRGBSurfaceWithFormat(0, static_cast<int>(cfg.window.width),
                                                        static_cast<int>(cfg.window.height), 32, SDL_PIXELFORMAT_RGBA32);
    if (!frame) {
        std::fprintf(stderr, "SDL_CreateRGBSurfaceWithFormat failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SurfaceRenderer renderer(frame);
    ParticleSystem ps;
    ps.reserve(cfg.maxBalls);
    std::mt19937 rng(cfg.seed);

    SpawnRequest request = jet;
    request.origin = nozzlePosition(cfg.window);

    const double freq = static_cast<double>(SDL_GetPerformanceFrequency());
    std::uint64_t prevCounter = SDL_GetPerformanceCounter();

    double accumulator = 0.0;
    double spawnAccumulator = 0.0;
    bool capAnnounced = false;

    std::size_t frameNo = 0;
    double statsWindowStart = 0.0;
    std::size_t statsFrames = 0;
    double physicsMs = 0.0;
    double renderMs = 0.0;
    double totalSeconds = 0.0;
    double emaPhys = 0.0, emaRender = 0.0, emaFrame = 0.0; // smoothed values for the HUD
    bool emaInit = false;

    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = false;
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) running = false;
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_SPACE) ps.clear();
        }

        std::uint64_t now = SDL_GetPerformanceCounter();
        double frameDt = static_cast<double>(now - prevCounter) / freq;
        prevCounter = now;
        frameDt = std::min(frameDt, 0.25);
        totalSeconds += frameDt;

        accumulator += frameDt;
        if (!ps.full()) spawnAccumulator += frameDt * opts.spawnRatePerSecond;

        int steps = 0;
        std::uint64_t stepStart = SDL_GetPerformanceCounter();
        while (accumulator >= cfg.fixedDt && steps < cfg.maxStepsPerFrame) {
            std::size_t want = static_cast<std::size_t>(spawnAccumulator);
            spawnAccumulator -= want;
            if (want > 0) {
                SpawnResult res = spawnJet(ps, request, want, rng);
                if (res.capReached && !capAnnounced) {
                    std::printf("Limit erreicht: %zu Bälle, Spawnen stoppt.\n", ps.count);
                    capAnnounced = true;
                }
                if (res.capReached) spawnAccumulator = 0.0;
            }
            stepSimulation(ps, cfg, cfg.fixedDt, numThreads);
            accumulator -= cfg.fixedDt;
            ++steps;
        }
        if (steps == cfg.maxStepsPerFrame) accumulator = 0.0; // drop backlog, avoid spiral of death
        double stepMs = 1000.0 * static_cast<double>(SDL_GetPerformanceCounter() - stepStart) / freq;
        physicsMs += stepMs;

        std::uint64_t renderStart = SDL_GetPerformanceCounter();
        renderer.clear(0x101018);
        renderer.drawAllBalls(ps);
        if (sdlRenderer && ballTexture) {
            SDL_UpdateTexture(ballTexture, nullptr, frame->pixels, frame->pitch);
            SDL_RenderClear(sdlRenderer);
            SDL_RenderCopy(sdlRenderer, ballTexture, nullptr, nullptr);
            drawNozzle(sdlRenderer, request.origin, request.directionRadians, 60.0f, 8.0f);
            drawHud(sdlRenderer, ps.count, emaPhys, emaRender, emaFrame);
            SDL_RenderPresent(sdlRenderer);
        }
        double rendMs = 1000.0 * static_cast<double>(SDL_GetPerformanceCounter() - renderStart) / freq;
        renderMs += rendMs;

        double frameMs = frameDt * 1000.0;
        if (!emaInit) {
            emaPhys = stepMs;
            emaRender = rendMs;
            emaFrame = frameMs;
            emaInit = true;
        } else {
            emaPhys = 0.9 * emaPhys + 0.1 * stepMs;
            emaRender = 0.9 * emaRender + 0.1 * rendMs;
            emaFrame = 0.9 * emaFrame + 0.1 * frameMs;
        }

        ++frameNo;
        ++statsFrames;
        if (totalSeconds - statsWindowStart >= 1.0) {
            double fps = statsFrames / (totalSeconds - statsWindowStart);
            std::printf("t=%.1fs  balls=%zu  fps=%.1f  phys=%.2fms  render=%.2fms\n", totalSeconds, ps.count, fps,
                        physicsMs / statsFrames, renderMs / statsFrames);
            statsWindowStart = totalSeconds;
            statsFrames = 0;
            physicsMs = 0.0;
            renderMs = 0.0;
        }

        if (opts.benchFrames != 0 && frameNo >= opts.benchFrames) running = false;
    }

    if (opts.benchFrames != 0) {
        double avgFps = frameNo / std::max(totalSeconds, 1e-9);
        std::printf("BENCH frames=%zu  time=%.2fs  avgFps=%.1f  avgPhys=%.2fms  avgRender=%.2fms  balls=%zu  cap=%s\n",
                    frameNo, totalSeconds, avgFps, physicsMs / frameNo, renderMs / frameNo, ps.count,
                    ps.full() ? "reached" : "no");
    }

    SDL_FreeSurface(frame);
    if (ballTexture) SDL_DestroyTexture(ballTexture);
    if (sdlRenderer) SDL_DestroyRenderer(sdlRenderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

} // namespace phys
