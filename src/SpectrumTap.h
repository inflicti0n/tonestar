#pragma once

#include "FieldSpectrum.h"

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <memory>
#include <vector>

class SpectrumTap
{
public:
    void prepare(double sampleRate);
    void reset();
    void pushStereo(const float* left, const float* right, int numSamples);
    FieldSpectrum snapshot() const;

private:
    void analyse();

    static constexpr int fftOrder = 9;
    static constexpr int fftSize = 1 << fftOrder;
    static constexpr int hop = 64;

    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> fifo;
    std::vector<float> window;
    std::vector<float> work;
    std::array<float, FieldSpectrum::bins> smooth {};
    std::array<float, FieldSpectrum::bins> published {};
    mutable juce::SpinLock lock;
    int fifoPos = 0;
    double sampleRate = 48000.0;
    bool ready = false;
};
