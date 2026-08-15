#include "StarPlasma.h"
#include <cmath>

namespace
{
float smoothstep(float edge0, float edge1, float x)
{
    const float t = juce::jlimit(0.0f, 1.0f, (x - edge0) / juce::jmax(edge1 - edge0, 1.0e-6f));
    return t * t * (3.0f - 2.0f * t);
}
}

void StarPlasma::setFieldEnergy(FieldEnergy next)
{
    const juce::SpinLock::ScopedLockType sl(lock);
    snapshot.energy = next;
}

void StarPlasma::setLook(const PlasmaLook& next)
{
    const juce::SpinLock::ScopedLockType sl(lock);
    snapshot.look = next;
}

void StarPlasma::setShape(juce::Point<float> centre, float radius,
                          const std::array<juce::Point<float>, 6>& verts,
                          int logicalW, int logicalH)
{
    const juce::SpinLock::ScopedLockType sl(lock);
    snapshot.centre = centre;
    snapshot.radius = radius;
    snapshot.verts = verts;
    snapshot.logicalW = juce::jmax(1, logicalW);
    snapshot.logicalH = juce::jmax(1, logicalH);
}

void StarPlasma::requestFrame()
{
    const auto now = (int64_t) juce::Time::getMillisecondCounterHiRes();
    if (! dueForFrame(now))
        return;
    lastFrameMs.store(now, std::memory_order_relaxed);

    const auto snap = copySnapshot();
    const float dt = 0.033f * juce::jlimit(0.05f, 4.0f, snap.look.timeScale);
    fluid.setDomain(snap.logicalW, snap.logicalH, snap.centre, snap.verts);
    fluid.step(dt, snap.energy, snap.look);
    rasterize(snap);
    serial.fetch_add(1, std::memory_order_release);
}

bool StarPlasma::dueForFrame(int64_t nowMs) const
{
    return nowMs - lastFrameMs.load(std::memory_order_relaxed) >= minFrameMs;
}

juce::Image StarPlasma::copyFrame() const
{
    const juce::SpinLock::ScopedLockType sl(lock);
    return frame;
}

StarPlasma::Snapshot StarPlasma::copySnapshot() const
{
    const juce::SpinLock::ScopedLockType sl(lock);
    return snapshot;
}

void StarPlasma::rasterize(const Snapshot& snap)
{
    constexpr int n = FluidSim::N;
    juce::Image next(juce::Image::ARGB, n, n, true);
    juce::Image::BitmapData bits(next, juce::Image::BitmapData::writeOnly);

    const float* dye = fluid.dye();
    const float* vx = fluid.vx();
    const float* vy = fluid.vy();
    const uint8_t* solid = fluid.solid();
    const auto& look = snap.look;
    const float punch = snap.energy.punch;
    const float storm = juce::jmax(0.0f, look.stormBoost);

    for (int y = 0; y < n; ++y)
    {
        for (int x = 0; x < n; ++x)
        {
            const int id = FluidSim::index(x, y);
            if (solid[id] != 0)
            {
                bits.setPixelColour(x, y, juce::Colours::transparentBlack);
                continue;
            }

            const float d = juce::jlimit(0.0f, 1.0f, dye[id]);
            if (d < 0.04f)
            {
                bits.setPixelColour(x, y, juce::Colours::transparentBlack);
                continue;
            }

            const float spd = std::sqrt(vx[id] * vx[id] + vy[id] * vy[id]);
            const float alpha = look.idleAlpha * smoothstep(0.04f, 0.22f, d);
            juce::Colour out = look.deep.withMultipliedAlpha(alpha);
            out = out.overlaidWith(look.body.withMultipliedAlpha(alpha * smoothstep(0.16f, 0.48f, d)));
            out = out.overlaidWith(look.core.withMultipliedAlpha(alpha * smoothstep(0.42f, 0.86f, d)));
            const float heat = juce::jlimit(0.0f, 1.0f, punch * storm * d + smoothstep(6.0f, 22.0f, spd) * 0.65f);
            out = out.overlaidWith(look.hot.withMultipliedAlpha(alpha * heat));
            out = out.overlaidWith(look.accent.withMultipliedAlpha(
                alpha * juce::jlimit(0.0f, 1.0f, punch * storm * d * 0.8f + smoothstep(0.55f, 0.95f, d) * 0.35f)));
            bits.setPixelColour(x, y, out);
        }
    }

    const juce::SpinLock::ScopedLockType sl(lock);
    frame = std::move(next);
}
