#pragma once

#include "FieldEnergy.h"
#include "PlasmaLook.h"

#include <array>
#include <cstdint>
#include <vector>

class FluidSim
{
public:
    static constexpr int N = 128;

    void setDomain(int logicalW, int logicalH,
                   juce::Point<float> centre,
                   const std::array<juce::Point<float>, 6>& verts);
    void step(float dt, FieldEnergy energy, const PlasmaLook& look);

    const float* dye() const { return dyeSrc.data(); }
    const float* vx() const { return vxSrc.data(); }
    const float* vy() const { return vySrc.data(); }
    const uint8_t* solid() const { return mask.data(); }

    static int index(int i, int j) { return i + j * N; }

private:
    void rebuildMask();
    void addForces(float dt, FieldEnergy energy, const PlasmaLook& look);
    void advectVel(float dt);
    void diffuseVel(float dt, float viscosity);
    void project();
    void confine(float dt, float confinement);
    void enforceWalls();
    void clampSpeed(float maxSpeed);
    void advectDye(float dt);
    void fadeAndInject(float dt, FieldEnergy energy, const PlasmaLook& look);
    void splatVel(float x, float y, float gamma, float radius);
    void splatDye(float x, float y, float amount, float radius);

    float sampleField(const float* field, float x, float y) const;
    void diffuseField(std::vector<float>& field, std::vector<float>& temp,
                      float a, float cRecip, int iterations);
    void seedDye();
    juce::Point<float> gridCentre() const;

    std::vector<float> vxSrc = std::vector<float>((size_t) N * N, 0.0f);
    std::vector<float> vySrc = std::vector<float>((size_t) N * N, 0.0f);
    std::vector<float> vxDst = std::vector<float>((size_t) N * N, 0.0f);
    std::vector<float> vyDst = std::vector<float>((size_t) N * N, 0.0f);
    std::vector<float> pressure = std::vector<float>((size_t) N * N, 0.0f);
    std::vector<float> divergence = std::vector<float>((size_t) N * N, 0.0f);
    std::vector<float> dyeSrc = std::vector<float>((size_t) N * N, 0.0f);
    std::vector<float> dyeDst = std::vector<float>((size_t) N * N, 0.0f);
    std::vector<float> omega = std::vector<float>((size_t) N * N, 0.0f);
    std::vector<uint8_t> mask = std::vector<uint8_t>((size_t) N * N, 1);

    int logicalW = 1;
    int logicalH = 1;
    juce::Point<float> centre;
    std::array<juce::Point<float>, 6> verts {};
    float time = 0.0f;
    bool seeded = false;
};
