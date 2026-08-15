#include "SpectrumTap.h"
#include <cmath>
#include <cstring>

void SpectrumTap::prepare(double nextSampleRate)
{
    sampleRate = nextSampleRate > 0.0 ? nextSampleRate : 48000.0;
    fifo.assign((size_t) fftSize, 0.0f);
    window.assign((size_t) fftSize, 0.0f);
    work.assign((size_t) fftSize * 2, 0.0f);
    fifoPos = 0;
    for (int i = 0; i < fftSize; ++i)
        window[(size_t) i] = 0.5f - 0.5f * std::cos(juce::MathConstants<float>::twoPi
                                                    * (float) i / (float) (fftSize - 1));
    fft = std::make_unique<juce::dsp::FFT>(fftOrder);
    ready = fft != nullptr && work.size() >= (size_t) fftSize * 2;
    reset();
}

void SpectrumTap::reset()
{
    if (! fifo.empty())
        std::fill(fifo.begin(), fifo.end(), 0.0f);
    std::fill(smooth.begin(), smooth.end(), 0.0f);
    const juce::SpinLock::ScopedLockType sl(lock);
    published = {};
    fifoPos = 0;
}

void SpectrumTap::pushStereo(const float* left, const float* right, int numSamples)
{
    if (! ready || fft == nullptr || left == nullptr || numSamples <= 0)
        return;
    if (fifo.size() != (size_t) fftSize || work.size() < (size_t) fftSize * 2)
        return;

    for (int i = 0; i < numSamples; ++i)
    {
        const float r = right != nullptr ? right[i] : left[i];
        fifo[(size_t) fifoPos++] = 0.5f * (left[i] + r);
        if (fifoPos >= fftSize)
        {
            analyse();
            std::memmove(fifo.data(), fifo.data() + hop, (size_t) (fftSize - hop) * sizeof(float));
            fifoPos = fftSize - hop;
        }
    }
}

void SpectrumTap::analyse()
{
    if (fft == nullptr || work.size() < (size_t) fftSize * 2)
        return;

    std::fill(work.begin(), work.end(), 0.0f);
    for (int i = 0; i < fftSize; ++i)
        work[(size_t) i] = fifo[(size_t) i] * window[(size_t) i];

    fft->performFrequencyOnlyForwardTransform(work.data());

    const float nyquist = (float) sampleRate * 0.5f;
    const float fMin = 60.0f;
    const float fMax = juce::jmin(12000.0f, nyquist * 0.92f);
    const float hzPerBin = (float) sampleRate / (float) fftSize;

    for (int b = 0; b < FieldSpectrum::bins; ++b)
    {
        const float t0 = (float) b / (float) FieldSpectrum::bins;
        const float t1 = (float) (b + 1) / (float) FieldSpectrum::bins;
        const float f0 = fMin * std::pow(fMax / fMin, t0);
        const float f1 = fMin * std::pow(fMax / fMin, t1);
        const int i0 = juce::jlimit(1, fftSize / 2 - 1, (int) std::floor(f0 / hzPerBin));
        const int i1 = juce::jlimit(i0 + 1, fftSize / 2, (int) std::ceil(f1 / hzPerBin));

        float peak = 0.0f;
        for (int k = i0; k < i1; ++k)
            peak = juce::jmax(peak, work[(size_t) k]);
        const float db = 20.0f * std::log10(peak + 1.0e-6f);
        const float bright = 0.62f + 1.15f * t0 * t0;
        const float target = juce::jlimit(0.0f, 1.0f,
                                          juce::jmap(db, -48.0f, -4.0f, 0.0f, 1.0f) * bright);
        const float coeff = target > smooth[(size_t) b] ? 0.82f : 0.28f;
        smooth[(size_t) b] += (target - smooth[(size_t) b]) * coeff;
    }

    const juce::SpinLock::ScopedLockType sl(lock);
    published = smooth;
}

FieldSpectrum SpectrumTap::snapshot() const
{
    FieldSpectrum out;
    const juce::SpinLock::ScopedLockType sl(lock);
    out.mag = published;
    return out;
}
