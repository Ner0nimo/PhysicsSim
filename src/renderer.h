#pragma once

#include <cstdint>

#include <SDL.h>

#include "config.h"
#include "particles.h"

namespace phys {

// Dishonest by design (top layer): everything here touches pixels.
// SurfaceRenderer rasterizes the balls into an SDL_Surface framebuffer;
// the app uploads it as a texture once per frame.
class SurfaceRenderer {
public:
    explicit SurfaceRenderer(SDL_Surface* target);

    void clear(std::uint32_t rgb);
    void drawAllBalls(const ParticleSystem& ps);

private:
    void drawBall(const ParticleSystem& ps, std::size_t i);

    SDL_Surface* target_;
};

// Nozzle marker drawn with SDL2_gfx directly onto the SDL_Renderer (on top of the ball texture).
void drawNozzle(SDL_Renderer* renderer, Vec2 origin, float directionRadians, float jetLength, float nozzleRadius);

} // namespace phys
