#pragma once

#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <vector>

struct VocalDebug
{
    int dblWraps = 0;
    int stackWraps = 0;
    int harmWraps = 0;
    int harmStarts = 0;
    int harmBlocked = 0;
    int tuneResets = 0;
    int hzJumps = 0;
    int lockLost = 0;
    int lockGained = 0;
    float hz = 0.0f;
    bool locked = false;
};

class VocalPitch
{
public:
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
        pitchWrite = 0;
        pitchFilled = 0;
        hop = 0;
        detectedHz = 0.0f;
        confident = false;
        ratioSmoothed = 1.0f;
        phaseA = 0.0f;
        phaseB = 0.5f;
        grainA = latency;
        grainB = latency;
        harmD = latency;
        harm[0] = {};
        harm[1] = {};
        harmHop = latency * 0.5f;
        harmHopCount = 0.0f;
        harmGain = 0.0f;
        harmFail = 0;
        dbg = {};
        wasLocked = false;
        tuneEnv = 0.0f;
        voiceWet = 0.0f;
        silentHops = 0;
        pitchJumpHold = 0;
    }

    void reset()
    {
        std::fill(delay.begin(), delay.end(), 0.0f);
        std::fill(pitchWin.begin(), pitchWin.end(), 0.0f);
        writePos = 0;
        pitchWrite = 0;
        pitchFilled = 0;
        hop = 0;
        detectedHz = 0.0f;
        confident = false;
        ratioSmoothed = 1.0f;
        phaseA = 0.0f;
        phaseB = 0.5f;
        grainA = latency;
        grainB = latency;
        harmD = latency;
        harm[0] = {};
        harm[1] = {};
        harmHopCount = 0.0f;
        harmGain = 0.0f;
        harmFail = 0;
        dbg = {};
        wasLocked = false;
        tuneEnv = 0.0f;
        voiceWet = 0.0f;
        silentHops = 0;
        pitchJumpHold = 0;
    }

    VocalDebug takeDebug()
    {
        VocalDebug out = dbg;
        out.hz = detectedHz;
        out.locked = confident && detectedHz > 55.0f;
        dbg = {};
        return out;
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

    static float degreeHz(float hz, int root, bool minor, int degrees)
    {
        if (hz < 55.0f || hz > 1200.0f || degrees == 0)
            return snapHz(hz, root, minor);

        const float midi = 69.0f + 12.0f * std::log2(hz / 440.0f);
        const int nearest = (int) std::lround(midi);
        const int* scale = minor ? kMinor : kMajor;
        int snapped = nearest;
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
                    snapped = cand;
                }
            }
        }

        const int pc = ((snapped - root) % 12 + 12) % 12;
        int deg = 0;
        int degDist = 99;
        for (int i = 0; i < 7; ++i)
        {
            const int d = std::abs(scale[i] - pc);
            const int wrap = 12 - d;
            const int dist = juce::jmin(d, wrap);
            if (dist < degDist)
            {
                degDist = dist;
                deg = i;
            }
        }

        int moved = deg + degrees;
        int oct = 0;
        while (moved < 0)
        {
            moved += 7;
            --oct;
        }
        while (moved >= 7)
        {
            moved -= 7;
            ++oct;
        }

        const int newMidi = snapped - scale[deg] + scale[moved] + oct * 12;
        return 440.0f * std::pow(2.0f, ((float) newMidi - 69.0f) / 12.0f);
    }

    void processTune(float* x, int n, float amount, int root, bool minor)
    {
        if (n <= 0 || delay.empty())
            return;

        amount = juce::jlimit(0.0f, 1.0f, amount);
        const float strength = std::pow(amount, 1.15f);
        const float maxCents = juce::jmap(amount, 30.0f, 220.0f);
        const float tauSec = juce::jmap(amount, 0.070f, 0.005f);
        const float coeff = 1.0f - std::exp(-1.0f / (tauSec * (float) sampleRate));
        const float maxD = (float) juce::jmin(mask - 4, (int) (sampleRate * 0.07));
        const float atk = 1.0f - std::exp(-1.0f / (0.004f * (float) sampleRate));
        const float rel = 1.0f - std::exp(-1.0f / (0.025f * (float) sampleRate));
        const float wetC = 1.0f - std::exp(-1.0f / (0.008f * (float) sampleRate));
        const float gate = 0.0035f;

        for (int i = 0; i < n; ++i)
        {
            const float dry = x[i];
            pushSample(dry);
            delay[(size_t) (writePos & mask)] = dry;

            const float absx = std::abs(dry);
            tuneEnv += ((absx > tuneEnv) ? atk : rel) * (absx - tuneEnv);

            float desired = 1.0f;
            const bool voiced = confident && detectedHz > 55.0f && tuneEnv > gate;
            if (voiced)
            {
                const float target = snapHz(detectedHz, root, minor);
                if (target > 55.0f)
                {
                    const float cents = 1200.0f * std::log2(juce::jmax(detectedHz, 1.0f) / target);
                    if (std::abs(cents) <= maxCents)
                        desired = 1.0f + (target / detectedHz - 1.0f) * strength;
                }
            }

            ratioSmoothed += coeff * (desired - ratioSmoothed);
            ratioSmoothed = juce::jlimit(0.72f, 1.40f, ratioSmoothed);

            const float r = ratioSmoothed;
            const float dA = juce::jlimit(1.0f, maxD,
                                          latency - (r - 1.0f) * phaseA * grainA);
            const float dB = juce::jlimit(1.0f, maxD,
                                          latency - (r - 1.0f) * phaseB * grainB);
            const float wA = hann(phaseA);
            const float wB = hann(phaseB);
            const float wSum = juce::jmax(1.0e-3f, wA + wB);
            const float shifted = (readBehind(dA) * wA + readBehind(dB) * wB) / wSum;
            const float aligned = readBehind(latency);
            const float need = juce::jlimit(0.0f, 1.0f, std::abs(r - 1.0f) * 28.0f);
            const float targetWet = voiced ? need : 0.0f;
            voiceWet += wetC * (targetWet - voiceWet);
            x[i] = aligned + (shifted - aligned) * voiceWet;

            advanceGrain(phaseA, grainA);
            advanceGrain(phaseB, grainB);
            ++writePos;
        }
    }

    void processHarmony(float* x, int n, int root, bool minor, int scaleDegrees)
    {
        if (n <= 0 || delay.empty())
            return;

        const float sr = (float) sampleRate;
        const float muteC = 1.0f - std::exp(-1.0f / (0.006f * sr));
        const float maxD = (float) juce::jmin(mask - 4, (int) (sampleRate * 0.08));

        for (int i = 0; i < n; ++i)
        {
            const float dry = x[i];
            pushSample(dry);
            delay[(size_t) (writePos & mask)] = dry;

            const bool detected = confident && detectedHz > 55.0f;
            if (detected)
                harmFail = 0;
            else if (harmFail < 8)
                ++harmFail;
            const bool locked = harmFail < 3 && detectedHz > 55.0f;
            if (locked && ! wasLocked)
                ++dbg.lockGained;
            else if (! locked && wasLocked)
                ++dbg.lockLost;
            wasLocked = locked;
            harmGain += muteC * ((locked ? 1.0f : 0.0f) - harmGain);

            if (locked)
            {
                const float period = sr / detectedHz;
                const float target = degreeHz(detectedHz, root, minor, scaleDegrees);
                const float ratio = target > 55.0f
                                        ? juce::jlimit(0.50f, 2.0f, target / detectedHz)
                                        : 1.0f;
                const float hop = juce::jlimit(32.0f, 0.040f * sr, period / ratio);
                const float grain = hop * 2.0f;

                if (! harm[0].on && ! harm[1].on)
                {
                    startHarmGrain(0, hop, grain, period, maxD);
                    harmHopCount = 0.0f;
                }

                harmHopCount += 1.0f;
                if (harmHopCount >= harmHop)
                {
                    harmHopCount = 0.0f;
                    if (! harm[0].on || ! harm[1].on)
                        startHarmGrain(pickHarmSlot(), hop, grain, period, maxD);
                    else
                        ++dbg.harmBlocked;
                }
            }

            float out = 0.0f;
            for (auto& g : harm)
            {
                if (! g.on)
                    continue;
                out += hann(g.k / juce::jmax(g.len, 1.0f)) * readBehind(g.delay);
                g.k += 1.0f;
                if (g.k >= g.len)
                    g.on = false;
            }

            x[i] = out * harmGain;
            ++writePos;
        }
    }

private:
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

    float nextGrainLen() const
    {
        if (detectedHz < 55.0f)
            return juce::jmax(latency, 0.020f * (float) sampleRate);

        const float period = (float) sampleRate / detectedHz;
        return juce::jlimit(0.016f * (float) sampleRate,
                            0.040f * (float) sampleRate,
                            2.0f * period);
    }

    void advanceGrain(float& phase, float& grain)
    {
        phase += 1.0f / juce::jmax(grain, 64.0f);
        if (phase >= 1.0f)
        {
            phase -= 1.0f;
            grain = nextGrainLen();
            ++dbg.tuneResets;
        }
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

    void startHarmGrain(int slot, float hop, float grain, float period, float maxD)
    {
        if (harm[0].on || harm[1].on)
            harmD += hop;

        const float halfP = period * 0.5f;
        while (harmD > latency + halfP)
        {
            harmD -= period;
            ++dbg.harmWraps;
        }
        while (harmD < juce::jmax(1.0f, latency - halfP))
        {
            harmD += period;
            ++dbg.harmWraps;
        }

        harmHop = hop;
        auto& g = harm[slot == 0 ? 0 : 1];
        g.on = true;
        ++dbg.harmStarts;
        g.k = 0.0f;
        g.len = grain;
        g.delay = juce::jlimit(1.0f, maxD, harmD);
    }

    int pickHarmSlot() const
    {
        if (! harm[0].on)
            return 0;
        if (! harm[1].on)
            return 1;
        return harm[0].k >= harm[1].k ? 0 : 1;
    }

    void pushSample(float s)
    {
        if (pitchWin.empty())
            return;

        pitchWin[(size_t) pitchWrite] = s;
        pitchWrite = (pitchWrite + 1) % winSize;
        if (pitchFilled < winSize)
            ++pitchFilled;
        ++hop;
        if (hop >= hopSize && pitchFilled >= winSize)
        {
            hop = 0;
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
            {
                hz = detectedHz;
                pitchJumpHold = 0;
            }
            else if (std::abs(std::log2(hz / juce::jmax(detectedHz, 1.0f))) > 0.20f)
            {
                ++dbg.hzJumps;
                if (++pitchJumpHold < 3)
                    return;
                pitchJumpHold = 0;
            }
            else
            {
                pitchJumpHold = 0;
            }
        }

        if (detectedHz < 55.0f)
            detectedHz = hz;
        else
            detectedHz += 0.28f * (hz - detectedHz);

        confident = true;
    }

    static constexpr int kDecim = 2;
    static constexpr int kMajor[7] = { 0, 2, 4, 5, 7, 9, 11 };
    static constexpr int kMinor[7] = { 0, 2, 3, 5, 7, 8, 10 };

    double sampleRate = 48000.0;
    std::vector<float> delay;
    int mask = 0;
    int writePos = 0;
    float latency = 960.0f;
    float detectedHz = 0.0f;
    float ratioSmoothed = 1.0f;
    bool confident = false;
    float phaseA = 0.0f;
    float phaseB = 0.5f;
    float grainA = 960.0f;
    float grainB = 960.0f;

    struct HarmGrain
    {
        bool on = false;
        float k = 0.0f;
        float len = 960.0f;
        float delay = 960.0f;
    };

    HarmGrain harm[2] {};
    float harmD = 960.0f;
    float harmHop = 480.0f;
    float harmHopCount = 0.0f;
    float harmGain = 0.0f;
    int harmFail = 0;
    bool wasLocked = false;
    float tuneEnv = 0.0f;
    float voiceWet = 0.0f;
    int silentHops = 0;
    int pitchJumpHold = 0;
    VocalDebug dbg {};

    std::vector<float> pitchWin, linear, decim, yinDiff, yinCmnd;
    int winSize = 0;
    int hopSize = 256;
    int pitchWrite = 0;
    int pitchFilled = 0;
    int hop = 0;
};
