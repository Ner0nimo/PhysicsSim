#include "particles.h"

#include <algorithm>
#include <cmath>

#include "color.h"

namespace phys {

namespace {
// Full rainbow cycle spans this many balls; each ball gets its own hue.
constexpr float kHueStepPerBall = 1.0f / 720.0f;
constexpr float kNozzleJitter = 3.0f; // px, keeps freshly spawned balls from perfect overlap
} // namespace

void ParticleSystem::reserve(std::size_t cap) {
    x.reserve(cap);
    y.reserve(cap);
    vx.reserve(cap);
    vy.reserve(cap);
    radius.reserve(cap);
    color.reserve(cap);
    sleepTimer.reserve(cap);
    contact.reserve(cap);
    x.resize(cap);
    y.resize(cap);
    vx.resize(cap);
    vy.resize(cap);
    radius.resize(cap);
    xPrev.resize(cap);
    yPrev.resize(cap);
    color.resize(cap);
    sleepTimer.resize(cap);
    contact.resize(cap);
}

void ParticleSystem::clear() {
    count = 0;
    hueCursor = 0.0f;
}

SpawnResult spawnJet(ParticleSystem& ps, const SpawnRequest& jet, std::size_t want, std::mt19937& rng) {
    SpawnResult result;
    result.capReached = ps.full();

    std::size_t n = std::min(want, ps.capacity() - ps.count);
    std::uniform_real_distribution<float> jitterAngle(-jet.spreadRadians, jet.spreadRadians);
    std::uniform_real_distribution<float> jitterPos(-kNozzleJitter, kNozzleJitter);
    std::uniform_real_distribution<float> speed(jet.minSpeed, jet.maxSpeed);
    std::uniform_real_distribution<float> radius(jet.minRadius, jet.maxRadius);

    for (std::size_t k = 0; k < n; ++k) {
        std::size_t i = ps.count++;
        float angle = jet.directionRadians + jitterAngle(rng);
        float s = speed(rng);
        ps.x[i] = jet.origin.x + jitterPos(rng);
        ps.y[i] = jet.origin.y + jitterPos(rng);
        ps.xPrev[i] = ps.x[i];
        ps.yPrev[i] = ps.y[i];
        ps.vx[i] = std::cos(angle) * s;
        ps.vy[i] = std::sin(angle) * s;
        ps.radius[i] = radius(rng);
        ps.color[i] = jet.color != 0 ? jet.color : hsvToRgb(ps.hueCursor, 0.85f, 1.0f);
        ps.sleepTimer[i] = 0.0f;
        ps.contact[i] = 0;
        ps.hueCursor += kHueStepPerBall;
        if (ps.hueCursor >= 1.0f) ps.hueCursor -= 1.0f;
    }

    result.spawned = n;
    return result;
}

void applyOrder(ParticleSystem& ps, const std::vector<int>& order) {
    auto gather = [&]<typename T>(std::vector<T>& v) {
        std::vector<T> tmp(v.size());
        for (std::size_t j = 0; j < ps.count; ++j) tmp[j] = v[static_cast<std::size_t>(order[j])];
        v.swap(tmp);
    };
    gather(ps.x);
    gather(ps.y);
    gather(ps.vx);
    gather(ps.vy);
    gather(ps.radius);
    gather(ps.xPrev);
    gather(ps.yPrev);
    gather(ps.color);
    gather(ps.sleepTimer);
    gather(ps.contact);
}

} // namespace phys
