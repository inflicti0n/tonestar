#pragma once

#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <vector>

class VocalShift
{
public:
    enum Mode { Transpose = 0, Robot, Quantize };

    void prepare(double sr, int block)
    {
        sampleRate = sr > 0.0 ? sr : 48000.0;
        const int n = nextPow2(juce::jmax(block * 4, 2048, (int) (sampleRate * 0.12)));
        delay.assign((size_t) n, 0.0f);
        mask = n - 1;
        writePos = 0;
        latency = 0.020f * (float) sampleRate;

        winSize = juce::jmax(128, ((int) (sampleRate * 0.050) / 2) * 2);
        hopSize = juce::jmax(64, (int) (sampleRate * 0.012));
        pitchWin.assign((size_t) winSize, 0.0f);
        linear.assign((size_t) winSize, 0.0f);
        const int decimN = winSize / kDecim;
        decim.assign((size_t) decimN, 0.0f);
        const int maxLag = juce::jmax(8, (int) ((sampleRate / (double) kDecim) / 70.0));
        yinDiff.assign((size_t) maxLag + 2, 0.0f);
        yinCmnd.assign((size_t) maxLag + 2, 0.0f);
        reset();
    }

    void reset()
    {
        std::fill(delay.begin(), delay.end(), 0.0f);
        std::fill(pitchWin.begin(), pitchWin.end(), 0.0f);
        writePos = 0;
        pitchWrite = 0;
        pitchFilled = 0;
        detectHop = 0;
        detectedHz = 0.0f;
        confident = false;
        silentHops = 0;
        hopCount = 0.0f;
        grainHop = latency * 0.5f;
        grain[0] = {};
        grain[1] = {};
        wet = 0.0f;
    }

    void process(float* x, int n, float pitchSt, float formantSt, int mode)
    {
        if (n <= 0 || delay.empty())
            return;

        pitchSt = juce::jlimit(-12.0f, 12.0f, pitchSt);
        formantSt = juce::jlimit(-12.0f, 12.0f, formantSt);
        mode = juce::jlimit(0, 2, mode);

        const bool idle = mode == Transpose
                          && std::abs(pitchSt) < 0.02f
                          && std::abs(formantSt) < 0.02f;
        if (idle)
        {
            reset();
            return;
        }

        const float sr = (float) sampleRate;
        const float formantRatio = juce::jlimit(0.50f, 2.00f, std::pow(2.0f, formantSt / 12.0f));
        const float pitchOff = std::pow(2.0f, pitchSt / 12.0f);
        const float maxD = (float) juce::jmin(mask - 4, (int) (sampleRate * 0.08));
        const float wetC = 1.0f - std::exp(-1.0f / (0.006f * sr));
        const float defaultPeriod = sr / 200.0f;

        for (int i = 0; i < n; ++i)
        {
            const float dry = x[i];
            pushSample(dry);
            delay[(size_t) (writePos & mask)] = dry;

            float pitchRatio = pitchOff;
            bool voiced = confident && detectedHz > 55.0f;
            if (voiced)
            {
                if (mode == Quantize)
                {
                    const float target = snapChromatic(detectedHz) * pitchOff;
                    pitchRatio = juce::jlimit(0.50f, 2.00f, target / detectedHz);
                }
                else if (mode == Robot)
                {
                    const float target = kRobotHz * pitchOff;
                    pitchRatio = juce::jlimit(0.50f, 2.00f, target / detectedHz);
                }
                else
                {
                    pitchRatio = juce::jlimit(0.50f, 2.00f, pitchOff);
                }
            }
            else if (mode != Transpose)
            {
                pitchRatio = 1.0f;
            }

            const float period = voiced ? sr / detectedHz : defaultPeriod;
            const float hop = juce::jlimit(32.0f, 0.040f * sr, period / juce::jmax(pitchRatio, 0.50f));
            const float len = hop * 2.0f;
            const bool run = voiced || mode == Transpose;

            if (run)
            {
                if (! grain[0].on && ! grain[1].on)
                {
                    startGrain(0, hop, len, formantRatio, maxD);
                    hopCount = 0.0f;
                }

                hopCount += 1.0f;
                if (hopCount >= grainHop && (! grain[0].on || ! grain[1].on))
                {
                    hopCount = 0.0f;
                    startGrain(pickSlot(), hop, len, formantRatio, maxD);
                }
            }

            float out = 0.0f;
            float wSum = 0.0f;
            for (auto& g : grain)
            {
                if (! g.on)
                    continue;
                const float w = hann(g.k / juce::jmax(g.len, 1.0f));
                const float d = g.delay0 + g.k * (1.0f - g.formant);
                out += w * readBehind(d);
                wSum += w;
                g.k += 1.0f;
                if (g.k >= g.len)
                    g.on = false;
            }
            if (wSum > 1.0e-3f)
                out /= wSum;

            const float aligned = readBehind(latency);
            const float want = run ? 1.0f : 0.0f;
            wet += wetC * (want - wet);
            x[i] = aligned + (out - aligned) * wet;
            ++writePos;
        }
    }

private:
    static constexpr int kDecim = 2;
    static constexpr float kRobotHz = 523.25116f;

    static int nextPow2(int n)
    {
        int p = 1;
        while (p < n)
            p <<= 1;
        return juce::jmax(2, p);
    }

    static float hann(float phase)
    {
        phase = juce::jlimit(0.0f, 1.0f, phase);
        return 0.5f * (1.0f - std::cos(juce::MathConstants<float>::twoPi * phase));
    }

    static float snapChromatic(float hz)
    {
        if (hz < 55.0f || hz > 1200.0f)
            return hz;
        const float midi = 69.0f + 12.0f * std::log2(hz / 440.0f);
        const float snapped = std::round(midi);
        return 440.0f * std::pow(2.0f, (snapped - 69.0f) / 12.0f);
    }

    float readBehind(float d) const
    {
        d = juce::jlimit(1.0f, (float) mask - 2.0f, d);
        const int whole = (int) d;
        const float frac = d - (float) whole;
        const int i0 = (writePos - whole) & mask;
        const int i1 = (writePos - whole - 1) & mask;
        return delay[(size_t) i0] + (delay[(size_t) i1] - delay[(size_t) i0]) * frac;
    }

    void startGrain(int slot, float hop, float len, float formant, float maxD)
    {
        grainHop = hop;
        auto& g = grain[slot == 0 ? 0 : 1];
        g.on = true;
        g.k = 0.0f;
        g.len = len;
        g.delay0 = juce::jlimit(1.0f, maxD, latency);
        g.formant = formant;
    }

    int pickSlot() const
    {
        if (! grain[0].on)
            return 0;
        if (! grain[1].on)
            return 1;
        return grain[0].k >= grain[1].k ? 0 : 1;
    }

    void pushSample(float s)
    {
        if (pitchWin.empty())
            return;

        pitchWin[(size_t) pitchWrite] = s;
        pitchWrite = (pitchWrite + 1) % winSize;
        if (pitchFilled < winSize)
            ++pitchFilled;
        ++detectHop;
        if (detectHop >= hopSize && pitchFilled >= winSize)
        {
            detectHop = 0;
            detectFromWindow();
        }
    }

    void detectFromWindow()
    {
        if (pitchWin.empty() || linear.size() < (size_t) winSize)
            return;

        for (int i = 0; i < winSize; ++i)
            linear[(size_t) i] = pitchWin[(size_t) ((pitchWrite + i) % winSize)];

        float energy = 0.0f;
        for (int i = 0; i < winSize; ++i)
            energy += linear[(size_t) i] * linear[(size_t) i];
        if (energy < 1.0e-5f * (float) winSize)
        {
            confident = false;
            if (++silentHops > 8)
                detectedHz = 0.0f;
            return;
        }
        silentHops = 0;

        const int decimN = winSize / kDecim;
        if (decimN < 64 || decim.size() < (size_t) decimN)
            return;

        for (int i = 0; i < decimN; ++i)
            decim[(size_t) i] = 0.5f * (linear[(size_t) (i * kDecim)]
                                        + linear[(size_t) (i * kDecim + 1)]);

        const double decimSr = sampleRate / (double) kDecim;
        const int minLag = juce::jmax(2, (int) (decimSr / 800.0));
        const int maxLag = juce::jmin(decimN / 2 - 2, (int) (decimSr / 70.0));
        if (maxLag <= minLag || yinDiff.size() < (size_t) maxLag + 2)
            return;

        for (int tau = 1; tau <= maxLag; ++tau)
        {
            float sum = 0.0f;
            const int count = decimN - tau;
            for (int j = 0; j < count; ++j)
            {
                const float d = decim[(size_t) j] - decim[(size_t) (j + tau)];
                sum += d * d;
            }
            yinDiff[(size_t) tau] = sum;
        }

        float running = 0.0f;
        yinCmnd[0] = 1.0f;
        int chosen = 0;
        float bestVal = 1.0f;
        int bestTau = 0;
        constexpr float thresh = 0.15f;

        for (int tau = 1; tau <= maxLag; ++tau)
        {
            running += yinDiff[(size_t) tau];
            const float cmnd = yinDiff[(size_t) tau] * (float) tau
                               / juce::jmax(running, 1.0e-12f);
            yinCmnd[(size_t) tau] = cmnd;
            if (tau >= minLag && cmnd < bestVal)
            {
                bestVal = cmnd;
                bestTau = tau;
            }
            if (chosen == 0 && tau >= minLag && cmnd < thresh)
                chosen = tau;
        }

        int tau = chosen > 0 ? chosen : bestTau;
        const float cmnd = (tau > 0 && tau <= maxLag) ? yinCmnd[(size_t) tau] : 1.0f;
        if (tau < minLag || cmnd > 0.28f)
        {
            confident = false;
            return;
        }

        if (tau * 2 <= maxLag && yinCmnd[(size_t) (tau * 2)] < cmnd + 0.06f)
            tau *= 2;

        float period = (float) tau;
        if (tau > 1 && tau < maxLag)
        {
            const float s0 = yinCmnd[(size_t) (tau - 1)];
            const float s1 = yinCmnd[(size_t) tau];
            const float s2 = yinCmnd[(size_t) (tau + 1)];
            const float denom = 2.0f * (s0 - 2.0f * s1 + s2);
            if (std::abs(denom) > 1.0e-8f)
                period += juce::jlimit(-0.5f, 0.5f, (s0 - s2) / denom);
        }

        float hz = (float) (decimSr / (double) juce::jmax(period, 1.0f));
        hz = juce::jlimit(70.0f, 800.0f, hz);

        if (detectedHz > 55.0f)
        {
            const float oct = hz / detectedHz;
            if (((oct > 1.85f && oct < 2.20f) || (oct > 0.45f && oct < 0.55f)) && cmnd > 0.08f)
                hz = detectedHz;
        }

        if (detectedHz < 55.0f)
            detectedHz = hz;
        else
            detectedHz += 0.28f * (hz - detectedHz);

        confident = true;
    }

    struct Grain
    {
        bool on = false;
        float k = 0.0f;
        float len = 960.0f;
        float delay0 = 960.0f;
        float formant = 1.0f;
    };

    double sampleRate = 48000.0;
    std::vector<float> delay;
    int mask = 0;
    int writePos = 0;
    float latency = 960.0f;
    float detectedHz = 0.0f;
    bool confident = false;
    Grain grain[2] {};
    float grainHop = 480.0f;
    float hopCount = 0.0f;
    float wet = 0.0f;

    std::vector<float> pitchWin, linear, decim, yinDiff, yinCmnd;
    int winSize = 0;
    int hopSize = 256;
    int pitchWrite = 0;
    int pitchFilled = 0;
    int detectHop = 0;
    int silentHops = 0;
};
