#pragma once

#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <vector>

class VocalPitch
{
public:
    void prepare(double sr, int block)
    {
        sampleRate = sr > 0.0 ? sr : 48000.0;
        const int n = juce::jmax(block * 4, 2048);
        delay.assign((size_t) n, 0.0f);
        mask = n - 1;
        writePos = 0;
        readA = 0.0f;
        readB = (float) n * 0.5f;
        detectedHz = 0.0f;
        hop = 0;
    }

    void reset()
    {
        std::fill(delay.begin(), delay.end(), 0.0f);
        detectedHz = 0.0f;
        airLp = 0.0f;
    }

    static float snapHz(float hz, int root, bool minor)
    {
        if (hz < 55.0f || hz > 1200.0f)
            return hz;

        const float midi = 69.0f + 12.0f * std::log2(hz / 440.0f);
        const int nearest = (int) std::lround(midi);
        const int* scale = minor ? kMinor : kMajor;
        int best = nearest;
        int bestDist = 99;
        for (int oct = -1; oct <= 1; ++oct)
        {
            for (int i = 0; i < 7; ++i)
            {
                const int cand = root + scale[i] + (nearest / 12 + oct) * 12;
                const int dist = std::abs(cand - nearest);
                if (dist < bestDist)
                {
                    bestDist = dist;
                    best = cand;
                }
            }
        }
        return 440.0f * std::pow(2.0f, ((float) best - 69.0f) / 12.0f);
    }

    float detect(const float* x, int n)
    {
        if (n < 64 || sampleRate <= 0.0)
            return detectedHz;

        const int minLag = (int) (sampleRate / 800.0);
        const int maxLag = juce::jmin(n / 2, (int) (sampleRate / 70.0));
        if (maxLag <= minLag)
            return detectedHz;

        float best = 0.0f;
        int bestLag = 0;
        for (int lag = minLag; lag < maxLag; ++lag)
        {
            float acc = 0.0f;
            for (int i = 0; i < n - lag; ++i)
                acc += x[i] * x[i + lag];
            if (acc > best)
            {
                best = acc;
                bestLag = lag;
            }
        }

        float energy = 0.0f;
        for (int i = 0; i < n; ++i)
            energy += x[i] * x[i];
        if (bestLag <= 0 || best < energy * 0.15f)
            return detectedHz;

        detectedHz = (float) sampleRate / (float) bestLag;
        return detectedHz;
    }

    void process(float* x, int n, float amount, int root, bool minor, float extraSemis)
    {
        if (n <= 0)
            return;

        hop += n;
        if (hop >= 256)
        {
            detect(x, n);
            hop = 0;
        }

        const float target = snapHz(detectedHz, root, minor);
        float ratio = 1.0f;
        if (detectedHz > 55.0f && target > 55.0f)
            ratio = target / detectedHz;
        if (extraSemis != 0.0f)
            ratio *= std::pow(2.0f, extraSemis / 12.0f);

        const float mix = juce::jlimit(0.0f, 1.0f, amount);
        if (mix <= 0.001f && extraSemis == 0.0f)
            return;

        if (delay.empty())
            return;

        const float grainSec = extraSemis == 0.0f
                                   ? (0.012f + mix * 0.026f)
                                   : 0.022f;
        const float grain = (float) juce::jmax(64, (int) (sampleRate * (double) grainSec));
        for (int i = 0; i < n; ++i)
        {
            const float dry = x[i];
            delay[(size_t) (writePos & mask)] = dry;
            const float a = readAt(readA);
            const float b = readAt(readB);
            const float wa = 0.5f + 0.5f * std::cos(juce::MathConstants<float>::twoPi
                                                    * (readA - (float) writePos) / grain);
            const float wb = 1.0f - wa;
            const float shifted = a * wa + b * wb;
            airLp += 0.14f * (dry - airLp);
            const float air = dry - airLp;
            x[i] = dry + (shifted - dry) * mix + air * 0.20f * mix * (1.0f - mix);
            readA += ratio;
            readB += ratio;
            ++writePos;
        }
    }

private:
    float readAt(float pos) const
    {
        const int i0 = (int) pos;
        const float f = pos - (float) i0;
        const float a = delay[(size_t) (i0 & mask)];
        const float b = delay[(size_t) ((i0 + 1) & mask)];
        return a + (b - a) * f;
    }

    static constexpr int kMajor[7] = { 0, 2, 4, 5, 7, 9, 11 };
    static constexpr int kMinor[7] = { 0, 2, 3, 5, 7, 8, 10 };

    double sampleRate = 48000.0;
    std::vector<float> delay;
    int mask = 0;
    int writePos = 0;
    float readA = 0.0f;
    float readB = 0.0f;
    float detectedHz = 0.0f;
    float airLp = 0.0f;
    int hop = 0;
};
