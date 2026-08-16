#pragma once

#include "Vocals/VocalPitch.h"

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <cmath>
#include <vector>

class VocalFx
{
public:
    static constexpr int numJobs = 6;
    enum Job { Tune = 0, Double, Echo, Bloom, Stack, Phone };

    void prepare(double sr, int block)
    {
        sampleRate = sr > 0.0 ? sr : 48000.0;
        tune.prepare(sr, block);
        stackLo.prepare(sr, block);
        stackHi.prepare(sr, block);
        const int maxD = (int) (sampleRate * 2.0) + 8;
        echo.assign((size_t) maxD, 0.0f);
        bloom.assign((size_t) maxD, 0.0f);
        dbl.assign((size_t) juce::jmax(64, (int) (sampleRate * 0.04)), 0.0f);
        echoPos = bloomPos = dblPos = 0;
        bloomState = 0.0f;
    }

    void reset()
    {
        tune.reset();
        stackLo.reset();
        stackHi.reset();
        std::fill(echo.begin(), echo.end(), 0.0f);
        std::fill(bloom.begin(), bloom.end(), 0.0f);
        std::fill(dbl.begin(), dbl.end(), 0.0f);
        bloomState = 0.0f;
    }

    void process(juce::AudioBuffer<float>& mono, const std::array<float, 6>& amt,
                 int root, bool minor, float bpm)
    {
        if (mono.getNumSamples() <= 0)
            return;

        auto* x = mono.getWritePointer(0);
        const int n = mono.getNumSamples();
        work.makeCopyOf(mono);

        const float tuneAmt = amt[Tune];
        if (tuneAmt > 0.01f)
            tune.process(x, n, tuneAmt, root, minor, 0.0f);

        const float dblAmt = amt[Double];
        if (dblAmt > 0.01f && ! dbl.empty())
        {
            const int delay = juce::jlimit(1, (int) dbl.size() - 1,
                                           (int) (sampleRate * (0.012 + 0.018 * dblAmt)));
            for (int i = 0; i < n; ++i)
            {
                const int r = (dblPos - delay + (int) dbl.size()) % (int) dbl.size();
                const float d = dbl[(size_t) r];
                dbl[(size_t) dblPos] = x[i];
                dblPos = (dblPos + 1) % (int) dbl.size();
                x[i] += d * (0.22f + 0.45f * dblAmt);
            }
        }

        const float echoAmt = amt[Echo];
        if (echoAmt > 0.01f && ! echo.empty())
        {
            const float beats = echoAmt < 0.34f ? 0.25f : (echoAmt < 0.67f ? 0.5f : 0.75f);
            const float spb = (float) (sampleRate * 60.0 / (double) juce::jmax(40.0f, bpm));
            const int delay = juce::jlimit(1, (int) echo.size() - 1, (int) (spb * beats));
            const float fb = 0.18f + echoAmt * 0.35f;
            const float wet = 0.16f + echoAmt * 0.42f;
            for (int i = 0; i < n; ++i)
            {
                const int r = (echoPos - delay + (int) echo.size()) % (int) echo.size();
                const float d = echo[(size_t) r];
                echo[(size_t) echoPos] = x[i] + d * fb;
                echoPos = (echoPos + 1) % (int) echo.size();
                const float duck = 1.0f / (1.0f + std::abs(x[i]) * 6.0f);
                x[i] += d * wet * duck;
            }
        }

        const float bloomAmt = amt[Bloom];
        if (bloomAmt > 0.01f)
        {
            const float decay = 0.82f + bloomAmt * 0.14f;
            const float wet = 0.12f + bloomAmt * 0.38f;
            for (int i = 0; i < n; ++i)
            {
                bloomState = bloomState * decay + x[i] * (1.0f - decay);
                const float duck = 1.0f / (1.0f + std::abs(x[i]) * 5.0f);
                x[i] += bloomState * wet * duck;
            }
        }

        const float stackAmt = amt[Stack];
        if (stackAmt > 0.01f)
        {
            stackBuf.makeCopyOf(work);
            auto* a = stackBuf.getWritePointer(0);
            const float third = juce::jmin(1.0f, stackAmt * 1.4f);
            stackLo.process(a, n, third, root, minor, minor ? -3.0f : -4.0f);
            for (int i = 0; i < n; ++i)
                x[i] += a[i] * (0.16f + 0.22f * stackAmt);

            if (stackAmt > 0.45f)
            {
                stackBuf.makeCopyOf(work);
                a = stackBuf.getWritePointer(0);
                stackHi.process(a, n, juce::jmin(1.0f, (stackAmt - 0.45f) * 2.0f), root, minor, 7.0f);
                for (int i = 0; i < n; ++i)
                    x[i] += a[i] * (0.10f + 0.18f * stackAmt);
            }
        }

        const float phoneAmt = amt[Phone];
        if (phoneAmt > 0.01f)
        {
            const float lo = 280.0f + phoneAmt * 220.0f;
            const float hi = 3400.0f - phoneAmt * 1800.0f;
            const float crush = 1.0f + phoneAmt * 7.0f;
            float lp = 0.0f, hp = 0.0f;
            const float lpC = 1.0f - std::exp(-2.0f * juce::MathConstants<float>::pi * hi / (float) sampleRate);
            const float hpC = 1.0f - std::exp(-2.0f * juce::MathConstants<float>::pi * lo / (float) sampleRate);
            for (int i = 0; i < n; ++i)
            {
                lp += lpC * (x[i] - lp);
                hp += hpC * (lp - hp);
                float s = lp - hp;
                s = std::tanh(s * crush) / crush;
                x[i] += (s - x[i]) * phoneAmt;
            }
        }
    }

private:
    double sampleRate = 48000.0;
    VocalPitch tune, stackLo, stackHi;
    juce::AudioBuffer<float> work, stackBuf;
    std::vector<float> echo, bloom, dbl;
    int echoPos = 0, bloomPos = 0, dblPos = 0;
    float bloomState = 0.0f;
};
