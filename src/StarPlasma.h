#pragma once

#include "FieldEnergy.h"
#include "FluidSim.h"
#include "PlasmaLook.h"

#include <array>
#include <atomic>
#include <cstdint>

class StarPlasma
{
public:
    void setFieldEnergy(FieldEnergy next);
    void setLook(const PlasmaLook& next);
    void setShape(juce::Point<float> centre, float radius,
                  const std::array<juce::Point<float>, 6>& verts,
                  int logicalW, int logicalH);
    void requestFrame();
    bool isReady() const { return true; }
    uint32_t frameSerial() const { return serial.load(std::memory_order_acquire); }
    juce::Image copyFrame() const;

private:
    struct Snapshot
    {
        FieldEnergy energy;
        PlasmaLook look;
        juce::Point<float> centre;
        float radius = 90.0f;
        std::array<juce::Point<float>, 6> verts {};
        int logicalW = 1;
        int logicalH = 1;
    };

    Snapshot copySnapshot() const;
    bool dueForFrame(int64_t nowMs) const;
    void rasterize(const Snapshot& snap);

    mutable juce::SpinLock lock;
    Snapshot snapshot;
    juce::Image frame;
    FluidSim fluid;
    std::atomic<uint32_t> serial { 0 };
    std::atomic<int64_t> lastFrameMs { 0 };

    static constexpr int64_t minFrameMs = 33;
};
