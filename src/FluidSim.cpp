#include "FluidSim.h"
#include <cmath>

namespace
{
constexpr float kEps = 1.0e-6f;

float sdHex(juce::Point<float> p, const std::array<juce::Point<float>, 6>& verts)
{
    float d = verts[0].getDistanceSquaredFrom(p);
    float s = 1.0f;
    int j = 5;
    for (int i = 0; i < 6; ++i)
    {
        const auto e = verts[(size_t) j] - verts[(size_t) i];
        const auto w = p - verts[(size_t) i];
        const float ee = juce::jmax(e.x * e.x + e.y * e.y, 1.0e-6f);
        const float t = juce::jlimit(0.0f, 1.0f, (w.x * e.x + w.y * e.y) / ee);
        const auto b = w - e * t;
        d = juce::jmin(d, b.x * b.x + b.y * b.y);
        const bool c0 = p.y >= verts[(size_t) i].y;
        const bool c1 = p.y < verts[(size_t) j].y;
        const bool c2 = e.x * w.y > e.y * w.x;
        if ((c0 && c1 && c2) || (! c0 && ! c1 && ! c2))
            s = -s;
        j = i;
    }
    return s * std::sqrt(juce::jmax(d, 0.0f));
}

bool vertsMoved(const std::array<juce::Point<float>, 6>& a,
                const std::array<juce::Point<float>, 6>& b)
{
    for (int i = 0; i < 6; ++i)
        if (a[(size_t) i].getDistanceSquaredFrom(b[(size_t) i]) > 0.25f)
            return true;
    return false;
}
}

void FluidSim::setDomain(int nextW, int nextH,
                         juce::Point<float> nextCentre,
                         const std::array<juce::Point<float>, 6>& nextVerts)
{
    nextW = juce::jmax(1, nextW);
    nextH = juce::jmax(1, nextH);
    const bool changed = nextW != logicalW || nextH != logicalH
                         || vertsMoved(verts, nextVerts);
    logicalW = nextW;
    logicalH = nextH;
    centre = nextCentre;
    verts = nextVerts;
    if (changed)
        rebuildMask();
}

juce::Point<float> FluidSim::gridCentre() const
{
    return { centre.x / (float) logicalW * (float) N,
             centre.y / (float) logicalH * (float) N };
}

void FluidSim::rebuildMask()
{
    const float centreSd = sdHex(centre, verts);
    const bool insideIsNegative = centreSd < 0.0f;
    int fluid = 0;

    for (int j = 0; j < N; ++j)
    {
        for (int i = 0; i < N; ++i)
        {
            const float px = ((float) i + 0.5f) / (float) N * (float) logicalW;
            const float py = ((float) j + 0.5f) / (float) N * (float) logicalH;
            const bool border = i == 0 || j == 0 || i == N - 1 || j == N - 1;
            const float sd = sdHex({ px, py }, verts);
            const bool inside = insideIsNegative ? (sd < 0.0f) : (sd > 0.0f);
            const bool solid = border || ! inside;
            const int id = index(i, j);
            mask[(size_t) id] = solid ? 1 : 0;
            if (solid)
            {
                vxSrc[(size_t) id] = 0.0f;
                vySrc[(size_t) id] = 0.0f;
                dyeSrc[(size_t) id] = 0.0f;
            }
            else
            {
                ++fluid;
            }
        }
    }

    if (fluid > 0 && ! seeded)
    {
        seedDye();
        seeded = true;
    }
}

void FluidSim::seedDye()
{
    const auto gc = gridCentre();
    for (int k = 0; k < 6; ++k)
    {
        const float ang = (float) k * juce::MathConstants<float>::twoPi / 6.0f + 0.4f;
        const float rad = (float) N * 0.11f;
        splatDye(gc.x + std::cos(ang) * rad,
                 gc.y + std::sin(ang) * rad,
                 1.1f, 4.2f);
    }
    splatDye(gc.x, gc.y, 0.85f, 3.2f);
}

void FluidSim::splatVel(float x, float y, float gamma, float radius)
{
    const int r = juce::jmax(1, (int) std::ceil(radius * 2.4f));
    const int ci = (int) std::floor(x);
    const int cj = (int) std::floor(y);
    const float sig = juce::jmax(0.6f, radius);

    for (int oy = -r; oy <= r; ++oy)
    {
        for (int ox = -r; ox <= r; ++ox)
        {
            const int i = ci + ox;
            const int j = cj + oy;
            if (i <= 0 || j <= 0 || i >= N - 1 || j >= N - 1)
                continue;
            const int id = index(i, j);
            if (mask[(size_t) id] != 0)
                continue;
            const float dx = (float) i + 0.5f - x;
            const float dy = (float) j + 0.5f - y;
            const float w = std::exp(-(dx * dx + dy * dy) / (2.0f * sig * sig));
            vxSrc[(size_t) id] += -dy * gamma * w;
            vySrc[(size_t) id] += dx * gamma * w;
        }
    }
}

void FluidSim::splatDye(float x, float y, float amount, float radius)
{
    const int r = juce::jmax(1, (int) std::ceil(radius * 2.4f));
    const int ci = (int) std::floor(x);
    const int cj = (int) std::floor(y);
    const float sig = juce::jmax(0.6f, radius);

    for (int oy = -r; oy <= r; ++oy)
    {
        for (int ox = -r; ox <= r; ++ox)
        {
            const int i = ci + ox;
            const int j = cj + oy;
            if (i <= 0 || j <= 0 || i >= N - 1 || j >= N - 1)
                continue;
            const int id = index(i, j);
            if (mask[(size_t) id] != 0)
                continue;
            const float dx = (float) i + 0.5f - x;
            const float dy = (float) j + 0.5f - y;
            const float w = std::exp(-(dx * dx + dy * dy) / (2.0f * sig * sig));
            dyeSrc[(size_t) id] = juce::jmin(1.35f, dyeSrc[(size_t) id] + amount * w);
        }
    }
}

void FluidSim::step(float dt, FieldEnergy energy, const PlasmaLook& look)
{
    dt = juce::jlimit(0.008f, 0.10f, dt);
    time += dt;
    addForces(dt, energy, look);
    enforceWalls();
    advectVel(dt);
    enforceWalls();
    diffuseVel(dt, look.viscosity);
    enforceWalls();
    project();
    confine(dt, look.confinement);
    enforceWalls();
    project();
    enforceWalls();
    clampSpeed(28.0f + 36.0f * energy.energy * juce::jmax(0.0f, look.stormBoost)
               + 18.0f * look.swirl);
    advectDye(dt);
    fadeAndInject(dt, energy, look);
    enforceWalls();
}

void FluidSim::addForces(float dt, FieldEnergy energy, const PlasmaLook& look)
{
    const auto gc = gridCentre();
    const float storm = 1.0f + energy.energy * look.stormBoost
                        + energy.punch * 1.6f * look.stormBoost;
    const float spin = look.swirl * 7.5f * storm;

    for (int j = 1; j < N - 1; ++j)
    {
        for (int i = 1; i < N - 1; ++i)
        {
            const int id = index(i, j);
            if (mask[(size_t) id] != 0)
                continue;

            const float dx = (float) i + 0.5f - gc.x;
            const float dy = (float) j + 0.5f - gc.y;
            const float tx = -dy * spin;
            const float ty = dx * spin;
            const float blend = juce::jmin(1.0f, dt * 3.2f);
            vxSrc[(size_t) id] += (tx - vxSrc[(size_t) id]) * blend * 0.35f;
            vySrc[(size_t) id] += (ty - vySrc[(size_t) id]) * blend * 0.35f;
            vxSrc[(size_t) id] += tx * dt * 0.55f;
            vySrc[(size_t) id] += ty * dt * 0.55f;
        }
    }

    const int count = 5;
    for (int k = 0; k < count; ++k)
    {
        const float sign = (k % 2 == 0) ? 1.0f : -1.0f;
        const float ang = time * (0.70f + 0.18f * (float) k) * (0.65f + 0.35f * look.swirl)
                          + (float) k * 1.2566f;
        const float rad = (float) N * (0.08f + 0.07f * (float) ((k * 3) % 5) / 4.0f);
        const float x = gc.x + std::cos(ang) * rad;
        const float y = gc.y + std::sin(ang) * rad;
        const float gamma = sign * (2.8f + 6.5f * look.swirl) * storm
                            * (0.75f + 0.25f * energy.punch);
        splatVel(x, y, gamma * dt, 3.4f);
    }

    if (energy.punch * look.inject * look.stormBoost < 0.04f)
        return;

    for (int k = 0; k < 3; ++k)
    {
        const float ang = time * (1.4f + 0.3f * (float) k) + (float) k * 2.1f;
        const float rad = (float) N * (0.06f + 0.09f * energy.punch);
        splatVel(gc.x + std::cos(ang) * rad,
                 gc.y + std::sin(ang) * rad,
                 (k % 2 == 0 ? 18.0f : -18.0f) * energy.punch * look.inject * look.stormBoost * dt,
                 2.6f);
    }
}

float FluidSim::sampleField(const float* field, float x, float y) const
{
    x = juce::jlimit(0.5f, (float) N - 1.5f, x);
    y = juce::jlimit(0.5f, (float) N - 1.5f, y);
    const int i0 = (int) std::floor(x);
    const int j0 = (int) std::floor(y);
    const int i1 = i0 + 1;
    const int j1 = j0 + 1;
    const float tx = x - (float) i0;
    const float ty = y - (float) j0;

    auto at = [&] (int i, int j) -> float
    {
        const int id = index(i, j);
        if (mask[(size_t) id] != 0)
            return 0.0f;
        return field[id];
    };

    const float a = at(i0, j0);
    const float b = at(i1, j0);
    const float c = at(i0, j1);
    const float d = at(i1, j1);
    return juce::jmap(ty, juce::jmap(tx, a, b), juce::jmap(tx, c, d));
}

void FluidSim::advectVel(float dt)
{
    for (int j = 1; j < N - 1; ++j)
    {
        for (int i = 1; i < N - 1; ++i)
        {
            const int id = index(i, j);
            if (mask[(size_t) id] != 0)
            {
                vxDst[(size_t) id] = 0.0f;
                vyDst[(size_t) id] = 0.0f;
                continue;
            }

            const float x = (float) i - vxSrc[(size_t) id] * dt;
            const float y = (float) j - vySrc[(size_t) id] * dt;
            vxDst[(size_t) id] = sampleField(vxSrc.data(), x, y);
            vyDst[(size_t) id] = sampleField(vySrc.data(), x, y);
        }
    }
    vxSrc.swap(vxDst);
    vySrc.swap(vyDst);
}

void FluidSim::diffuseField(std::vector<float>& field, std::vector<float>& temp,
                            float a, float cRecip, int iterations)
{
    for (int n = 0; n < iterations; ++n)
    {
        for (int j = 1; j < N - 1; ++j)
        {
            for (int i = 1; i < N - 1; ++i)
            {
                const int id = index(i, j);
                if (mask[(size_t) id] != 0)
                {
                    temp[(size_t) id] = 0.0f;
                    continue;
                }

                const float left = mask[(size_t) index(i - 1, j)] ? field[(size_t) id] : field[(size_t) index(i - 1, j)];
                const float right = mask[(size_t) index(i + 1, j)] ? field[(size_t) id] : field[(size_t) index(i + 1, j)];
                const float down = mask[(size_t) index(i, j - 1)] ? field[(size_t) id] : field[(size_t) index(i, j - 1)];
                const float up = mask[(size_t) index(i, j + 1)] ? field[(size_t) id] : field[(size_t) index(i, j + 1)];
                temp[(size_t) id] = (field[(size_t) id] + a * (left + right + down + up)) * cRecip;
            }
        }
        field.swap(temp);
    }
}

void FluidSim::diffuseVel(float dt, float viscosity)
{
    if (viscosity <= 0.035f)
    {
        const float damp = juce::jmax(0.0f, 1.0f - viscosity * 1.8f * dt);
        for (int i = 0; i < N * N; ++i)
        {
            vxSrc[(size_t) i] *= damp;
            vySrc[(size_t) i] *= damp;
        }
        return;
    }

    const float a = dt * viscosity * 8.0f;
    const float cRecip = 1.0f / (1.0f + 4.0f * a);
    diffuseField(vxSrc, vxDst, a, cRecip, 6);
    diffuseField(vySrc, vyDst, a, cRecip, 6);
}

void FluidSim::project()
{
    for (int j = 1; j < N - 1; ++j)
    {
        for (int i = 1; i < N - 1; ++i)
        {
            const int id = index(i, j);
            if (mask[(size_t) id] != 0)
            {
                divergence[(size_t) id] = 0.0f;
                pressure[(size_t) id] = 0.0f;
                continue;
            }

            const float vxR = mask[(size_t) index(i + 1, j)] ? 0.0f : vxSrc[(size_t) index(i + 1, j)];
            const float vxL = mask[(size_t) index(i - 1, j)] ? 0.0f : vxSrc[(size_t) index(i - 1, j)];
            const float vyU = mask[(size_t) index(i, j + 1)] ? 0.0f : vySrc[(size_t) index(i, j + 1)];
            const float vyD = mask[(size_t) index(i, j - 1)] ? 0.0f : vySrc[(size_t) index(i, j - 1)];
            divergence[(size_t) id] = -0.5f * (vxR - vxL + vyU - vyD);
            pressure[(size_t) id] = 0.0f;
        }
    }

    for (int n = 0; n < 20; ++n)
    {
        for (int j = 1; j < N - 1; ++j)
        {
            for (int i = 1; i < N - 1; ++i)
            {
                const int id = index(i, j);
                if (mask[(size_t) id] != 0)
                    continue;

                const float pL = mask[(size_t) index(i - 1, j)] ? pressure[(size_t) id] : pressure[(size_t) index(i - 1, j)];
                const float pR = mask[(size_t) index(i + 1, j)] ? pressure[(size_t) id] : pressure[(size_t) index(i + 1, j)];
                const float pD = mask[(size_t) index(i, j - 1)] ? pressure[(size_t) id] : pressure[(size_t) index(i, j - 1)];
                const float pU = mask[(size_t) index(i, j + 1)] ? pressure[(size_t) id] : pressure[(size_t) index(i, j + 1)];
                vxDst[(size_t) id] = (divergence[(size_t) id] + pL + pR + pD + pU) * 0.25f;
            }
        }

        for (int j = 1; j < N - 1; ++j)
        {
            for (int i = 1; i < N - 1; ++i)
            {
                const int id = index(i, j);
                if (mask[(size_t) id] != 0)
                    continue;
                pressure[(size_t) id] = vxDst[(size_t) id];
            }
        }
    }

    for (int j = 1; j < N - 1; ++j)
    {
        for (int i = 1; i < N - 1; ++i)
        {
            const int id = index(i, j);
            if (mask[(size_t) id] != 0)
            {
                vxSrc[(size_t) id] = 0.0f;
                vySrc[(size_t) id] = 0.0f;
                continue;
            }

            const float pL = mask[(size_t) index(i - 1, j)] ? pressure[(size_t) id] : pressure[(size_t) index(i - 1, j)];
            const float pR = mask[(size_t) index(i + 1, j)] ? pressure[(size_t) id] : pressure[(size_t) index(i + 1, j)];
            const float pD = mask[(size_t) index(i, j - 1)] ? pressure[(size_t) id] : pressure[(size_t) index(i, j - 1)];
            const float pU = mask[(size_t) index(i, j + 1)] ? pressure[(size_t) id] : pressure[(size_t) index(i, j + 1)];
            vxSrc[(size_t) id] -= 0.5f * (pR - pL);
            vySrc[(size_t) id] -= 0.5f * (pU - pD);
        }
    }
}

void FluidSim::confine(float dt, float confinement)
{
    if (confinement <= 0.001f)
        return;

    for (int j = 1; j < N - 1; ++j)
    {
        for (int i = 1; i < N - 1; ++i)
        {
            const int id = index(i, j);
            if (mask[(size_t) id] != 0)
            {
                omega[(size_t) id] = 0.0f;
                continue;
            }

            const float vyR = mask[(size_t) index(i + 1, j)] ? 0.0f : vySrc[(size_t) index(i + 1, j)];
            const float vyL = mask[(size_t) index(i - 1, j)] ? 0.0f : vySrc[(size_t) index(i - 1, j)];
            const float vxU = mask[(size_t) index(i, j + 1)] ? 0.0f : vxSrc[(size_t) index(i, j + 1)];
            const float vxD = mask[(size_t) index(i, j - 1)] ? 0.0f : vxSrc[(size_t) index(i, j - 1)];
            omega[(size_t) id] = 0.5f * (vyR - vyL - (vxU - vxD));
        }
    }

    const float eps = confinement * 12.0f;
    for (int j = 1; j < N - 1; ++j)
    {
        for (int i = 1; i < N - 1; ++i)
        {
            const int id = index(i, j);
            if (mask[(size_t) id] != 0)
                continue;

            const float wL = std::abs(omega[(size_t) index(i - 1, j)]);
            const float wR = std::abs(omega[(size_t) index(i + 1, j)]);
            const float wD = std::abs(omega[(size_t) index(i, j - 1)]);
            const float wU = std::abs(omega[(size_t) index(i, j + 1)]);
            float nx = 0.5f * (wR - wL);
            float ny = 0.5f * (wU - wD);
            const float len = std::sqrt(nx * nx + ny * ny) + kEps;
            nx /= len;
            ny /= len;
            const float w = omega[(size_t) id];
            vxSrc[(size_t) id] += eps * (-ny * w) * dt;
            vySrc[(size_t) id] += eps * (nx * w) * dt;
        }
    }
}

void FluidSim::enforceWalls()
{
    for (int i = 0; i < N * N; ++i)
    {
        if (mask[(size_t) i] == 0)
            continue;
        vxSrc[(size_t) i] = 0.0f;
        vySrc[(size_t) i] = 0.0f;
        dyeSrc[(size_t) i] = 0.0f;
    }
}

void FluidSim::clampSpeed(float maxSpeed)
{
    const float max2 = maxSpeed * maxSpeed;
    for (int i = 0; i < N * N; ++i)
    {
        if (mask[(size_t) i] != 0)
            continue;
        const float v2 = vxSrc[(size_t) i] * vxSrc[(size_t) i] + vySrc[(size_t) i] * vySrc[(size_t) i];
        if (v2 <= max2)
            continue;
        const float s = maxSpeed / std::sqrt(v2);
        vxSrc[(size_t) i] *= s;
        vySrc[(size_t) i] *= s;
    }
}

void FluidSim::advectDye(float dt)
{
    for (int j = 1; j < N - 1; ++j)
    {
        for (int i = 1; i < N - 1; ++i)
        {
            const int id = index(i, j);
            if (mask[(size_t) id] != 0)
            {
                dyeDst[(size_t) id] = 0.0f;
                continue;
            }

            const float x = (float) i - vxSrc[(size_t) id] * dt;
            const float y = (float) j - vySrc[(size_t) id] * dt;
            dyeDst[(size_t) id] = sampleField(dyeSrc.data(), x, y);
        }
    }
    dyeSrc.swap(dyeDst);
}

void FluidSim::fadeAndInject(float dt, FieldEnergy energy, const PlasmaLook& look)
{
    const float fade = 0.22f + juce::jlimit(0.0f, 0.95f, look.dyeFade) * 2.4f;
    const float keep = std::exp(-fade * dt);
    for (int i = 0; i < N * N; ++i)
    {
        if (mask[(size_t) i] != 0)
        {
            dyeSrc[(size_t) i] = 0.0f;
            continue;
        }
        dyeSrc[(size_t) i] *= keep;
    }

    const auto gc = gridCentre();
    const float storm = 1.0f + energy.energy * look.stormBoost;
    const float amp = (0.55f + 0.85f * look.inject) * (0.55f + 0.45f * storm)
                      * (0.70f + 0.80f * energy.punch * look.stormBoost);

    for (int k = 0; k < 6; ++k)
    {
        const float ang = time * (0.85f + 0.16f * (float) k) + (float) k * 1.047f;
        const float rad = (float) N * (0.07f + 0.06f * (float) ((k * 2) % 3) / 2.0f);
        splatDye(gc.x + std::cos(ang) * rad,
                 gc.y + std::sin(ang) * rad,
                 amp * (0.55f + 0.45f * (float) (k % 2)),
                 2.8f + 1.4f * energy.punch);
    }
}
