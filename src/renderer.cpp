#include "renderer.h"

#include <algorithm>
#include <cmath>

#include <SDL2_gfxPrimitives.h>

namespace phys {

SurfaceRenderer::SurfaceRenderer(SDL_Surface* target) : target_(target) {}

void SurfaceRenderer::clear(std::uint32_t rgb) {
    std::uint32_t px = SDL_MapRGB(target_->format, (rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
    std::uint32_t* pixels = static_cast<std::uint32_t*>(target_->pixels);
    std::size_t n = static_cast<std::size_t>(target_->w) * static_cast<std::size_t>(target_->h);
    for (std::size_t i = 0; i < n; ++i) pixels[i] = px;
}

void SurfaceRenderer::drawBall(const ParticleSystem& ps, std::size_t i) {
    float r = ps.radius[i];
    int cx = static_cast<int>(ps.x[i]);
    int cy = static_cast<int>(ps.y[i]);
    int ir = static_cast<int>(std::ceil(r));

    int x0 = std::max(0, cx - ir);
    int x1 = std::min(target_->w - 1, cx + ir);
    int y0 = std::max(0, cy - ir);
    int y1 = std::min(target_->h - 1, cy + ir);
    if (x0 > x1 || y0 > y1) return;

    std::uint32_t c = ps.color[i];
    std::uint32_t px = SDL_MapRGB(target_->format, (c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF);
    std::uint32_t* pixels = static_cast<std::uint32_t*>(target_->pixels);
    float rSq = r * r;

    for (int dy = y0 - cy; dy <= y1 - cy; ++dy) {
        float span = std::sqrt(std::max(0.0f, rSq - static_cast<float>(dy) * static_cast<float>(dy)));
        int xa = std::max(x0, cx - static_cast<int>(std::ceil(span)));
        int xb = std::min(x1, cx + static_cast<int>(span));
        std::uint32_t* row = pixels + static_cast<std::size_t>(cy + dy) * static_cast<std::size_t>(target_->w);
        for (int x = xa; x <= xb; ++x) row[x] = px;
    }
}

void SurfaceRenderer::drawAllBalls(const ParticleSystem& ps) {
    for (std::size_t i = 0; i < ps.count; ++i) drawBall(ps, i);
}

void drawNozzle(SDL_Renderer* renderer, Vec2 origin, float directionRadians, float jetLength, float nozzleRadius) {
    Sint16 x = static_cast<Sint16>(origin.x);
    Sint16 y = static_cast<Sint16>(origin.y);
    Sint16 r = static_cast<Sint16>(nozzleRadius);
    aacircleRGBA(renderer, x, y, r, 220, 220, 220, 255);
    filledCircleRGBA(renderer, x, y, r - 2, 60, 60, 70, 255);
    Sint16 ex = x + static_cast<Sint16>(std::cos(directionRadians) * jetLength);
    Sint16 ey = y + static_cast<Sint16>(std::sin(directionRadians) * jetLength);
    aalineRGBA(renderer, x, y, ex, ey, 200, 200, 210, 255);
}

} // namespace phys
