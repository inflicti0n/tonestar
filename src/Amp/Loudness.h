#pragma once

#include <juce_core/juce_core.h>
#include <cmath>

namespace Loudness
{
    static constexpr float starPeakTarget = 0.5f;
    static constexpr float cabPeakTarget = 0.5f;
    static constexpr float hrtfPeakTarget = 0.5f;
    static constexpr float rmsBudget = 1.1220184543f;
    static constexpr float peakBudget = 1.2589254118f;
    static constexpr float shimmerPeakBudget = 1.1220184543f;

    inline float peakAwareGain(float dryRms, float wetRms, float wetPeak,
                               float peakTarget, float minGain, float maxGain)
    {
        const float rmsGain = dryRms / juce::jmax(wetRms, 1.0e-6f);
        const float peakGain = peakTarget / juce::jmax(wetPeak, 1.0e-6f);
        return juce::jlimit(minGain, maxGain, juce::jmin(rmsGain, peakGain));
    }

    inline void fillPink(float* dest, int n, juce::int32 seed)
    {
        juce::Random rng { seed };
        float b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0;
        for (int i = 0; i < n; ++i)
        {
            const float white = rng.nextFloat() * 2.0f - 1.0f;
            b0 = 0.99886f * b0 + white * 0.0555179f;
            b1 = 0.99332f * b1 + white * 0.0750759f;
            b2 = 0.96900f * b2 + white * 0.1538520f;
            b3 = 0.86650f * b3 + white * 0.3104856f;
            b4 = 0.55000f * b4 + white * 0.5329522f;
            b5 = -0.7616f * b5 - white * 0.0168980f;
            dest[i] = (b0 + b1 + b2 + b3 + b4 + b5 + b6 + white * 0.5362f) * 0.11f;
            b6 = white * 0.115926f;
        }
    }

    inline float wetSendScale(const float* dry, const float* wet, int n,
                              float rmsLimit = rmsBudget,
                              float peakLimit = peakBudget)
    {
        if (dry == nullptr || wet == nullptr || n <= 0)
            return 0.25f;

        double sumD2 = 0.0, sumW2 = 0.0, sumDW = 0.0;
        float dryPeak = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            const float d = dry[i];
            const float w = wet[i];
            sumD2 += (double) d * d;
            sumW2 += (double) w * w;
            sumDW += (double) d * w;
            dryPeak = juce::jmax(dryPeak, std::abs(d));
        }

        const float limit = juce::jmax(dryPeak, 1.0e-6f) * peakLimit;
        float sPeak = 2.0f;
        for (int i = 0; i < n; ++i)
        {
            const float d = dry[i];
            const float w = wet[i];
            if (std::abs(w) < 1.0e-12f)
                continue;

            const float upper = w > 0.0f ? (limit - d) / w : (-limit - d) / w;
            if (upper >= 0.0f)
                sPeak = juce::jmin(sPeak, upper);
        }

        float sRms = 2.0f;
        const double target = sumD2 * (double) rmsLimit * (double) rmsLimit;
        const double a = sumW2;
        const double b = 2.0 * sumDW;
        if (a > 1.0e-12)
        {
            const double disc = b * b - 4.0 * a * (sumD2 - target);
            if (disc >= 0.0)
            {
                const double hi = (-b + std::sqrt(disc)) / (2.0 * a);
                sRms = hi > 0.0 ? (float) juce::jmin(2.0, hi) : 0.0f;
            }
            else
            {
                sRms = 0.0f;
            }
        }

        const float s = juce::jmin(sPeak, sRms);
        if (! std::isfinite(s) || s < 1.0e-4f)
            return 0.2f;

        return juce::jmin(s, 2.0f);
    }
}
