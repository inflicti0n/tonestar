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
        maxBlock = juce::jmax(32, block);
        tune.prepare(sr, block);
        stackLo.prepare(sr, block);
        stackHi.prepare(sr, block);
        const int maxD = (int) (sampleRate * 2.0) + 8;
        echo.assign((size_t) maxD, 0.0f);
        dbl.assign((size_t) juce::jmax(64, (int) (sampleRate * 0.040)), 0.0f);
        const int wallN = juce::jmax(64, (int) (sampleRate * 0.045));
        wallDblL.assign((size_t) wallN, 0.0f);
        wallDblR.assign((size_t) wallN, 0.0f);
        wallLagLo.assign((size_t) wallN, 0.0f);
        wallLagHi.assign((size_t) wallN, 0.0f);
        echoPos = dblPos = 0;
        wallDblPosL = wallDblPosR = wallLagPosLo = wallLagPosHi = 0;
        dblDelayA = 0.012f * (float) sampleRate;
        dblDelayB = 0.017f * (float) sampleRate;
        wallDelayL = 0.014f * (float) sampleRate;
        wallDelayR = 0.023f * (float) sampleRate;
        wallOldL = wallOldR = 0.0f;
        wallXfadeL = wallXfadeR = 0.0f;
        dbg = {};
        dblHp = 0.0f;
        stackHpLo = 0.0f;

        juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) maxBlock, 1 };
        bloom.prepare(spec);
        bloom.reset();
        bloomWork.setSize(1, maxBlock);
        duckEnv = 0.0f;

        for (int c = 0; c < 2; ++c)
        {
            phoneLp[c] = phoneHp[c] = 0.0f;
            phoneMidLow[c] = phoneMidBand[c] = 0.0f;
        }
    }

    void reset()
    {
        tune.reset();
        stackLo.reset();
        stackHi.reset();
        std::fill(echo.begin(), echo.end(), 0.0f);
        std::fill(dbl.begin(), dbl.end(), 0.0f);
        std::fill(wallDblL.begin(), wallDblL.end(), 0.0f);
        std::fill(wallDblR.begin(), wallDblR.end(), 0.0f);
        std::fill(wallLagLo.begin(), wallLagLo.end(), 0.0f);
        std::fill(wallLagHi.begin(), wallLagHi.end(), 0.0f);
        echoPos = dblPos = 0;
        wallDblPosL = wallDblPosR = wallLagPosLo = wallLagPosHi = 0;
        dblDelayA = 0.012f * (float) sampleRate;
        dblDelayB = 0.017f * (float) sampleRate;
        wallDelayL = 0.014f * (float) sampleRate;
        wallDelayR = 0.023f * (float) sampleRate;
        wallOldL = wallOldR = 0.0f;
        wallXfadeL = wallXfadeR = 0.0f;
        dbg = {};
        dblHp = 0.0f;
        stackHpLo = 0.0f;
        bloom.reset();
        duckEnv = 0.0f;
        for (int c = 0; c < 2; ++c)
        {
            phoneLp[c] = phoneHp[c] = 0.0f;
            phoneMidLow[c] = phoneMidBand[c] = 0.0f;
        }
    }

    void process(juce::AudioBuffer<float>& io, const std::array<float, 6>& amt,
                 int root, bool minor, float bpm)
    {
        const int n = io.getNumSamples();
        if (n <= 0)
            return;

        if (io.getNumChannels() < 2)
            io.setSize(2, n, true, false, true);

        auto* L = io.getWritePointer(0);
        auto* R = io.getWritePointer(1);

        work.setSize(1, n, false, false, true);
        work.copyFrom(0, 0, L, n);

        const float tuneAmt = amt[Tune];
        if (tuneAmt > 0.01f)
            tune.processTune(L, n, tuneAmt, root, minor);

        const float dblAmt = amt[Double];
        if (dblAmt > 0.01f && ! dbl.empty())
        {
            const float cents = 5.0f + 7.0f * dblAmt;
            const float rateA = std::pow(2.0f, cents / 1200.0f);
            const float rateB = std::pow(2.0f, -cents / 1200.0f);
            const float mix = 0.22f + 0.28f * dblAmt;
            const float hpC = 1.0f - std::exp(-2.0f * juce::MathConstants<float>::pi
                                              * 180.0f / (float) sampleRate);
            const float minD = 0.008f * (float) sampleRate;
            const float maxD = 0.0195f * (float) sampleRate;
            const float wrap = maxD - minD;
            const int maxTap = (int) dbl.size() - 2;

            for (int i = 0; i < n; ++i)
            {
                dbl[(size_t) dblPos] = L[i];
                dblDelayA += 1.0f - rateA;
                dblDelayB += 1.0f - rateB;
                if (dblDelayA < minD)
                {
                    dblDelayA += wrap;
                    ++dbg.dblWraps;
                }
                else if (dblDelayA > maxD)
                {
                    dblDelayA -= wrap;
                    ++dbg.dblWraps;
                }
                if (dblDelayB < minD)
                {
                    dblDelayB += wrap;
                    ++dbg.dblWraps;
                }
                else if (dblDelayB > maxD)
                {
                    dblDelayB -= wrap;
                    ++dbg.dblWraps;
                }

                const float a = tapLine(dbl, dblPos, juce::jlimit(1.0f, (float) maxTap, dblDelayA));
                const float b = tapLine(dbl, dblPos, juce::jlimit(1.0f, (float) maxTap, dblDelayB));
                dblPos = (dblPos + 1) % (int) dbl.size();

                const float wet = 0.55f * a + 0.45f * b;
                dblHp += hpC * (wet - dblHp);
                L[i] += (wet - dblHp) * mix;
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
                echo[(size_t) echoPos] = L[i] + d * fb;
                echoPos = (echoPos + 1) % (int) echo.size();
                const float duck = 1.0f / (1.0f + std::abs(L[i]) * 6.0f);
                L[i] += d * wet * duck;
            }
        }

        const float bloomAmt = amt[Bloom];
        if (bloomAmt > 0.01f)
        {
            if (bloomWork.getNumSamples() < n)
                bloomWork.setSize(1, n, false, false, true);

            juce::dsp::Reverb::Parameters p;
            p.roomSize = juce::jmap(bloomAmt, 0.22f, 0.85f);
            p.damping = juce::jmap(bloomAmt, 0.35f, 0.55f);
            p.wetLevel = 1.0f;
            p.dryLevel = 0.0f;
            p.width = 0.25f;
            p.freezeMode = 0.0f;
            bloom.setParameters(p);

            bloomWork.copyFrom(0, 0, L, n);
            auto block = juce::dsp::AudioBlock<float>(bloomWork).getSubBlock(0, (size_t) n);
            juce::dsp::ProcessContextReplacing<float> ctx(block);
            bloom.process(ctx);

            const float send = 0.20f + 0.30f * bloomAmt;
            const float atk = 1.0f - std::exp(-1.0f / (0.008f * (float) sampleRate));
            const float rel = 1.0f - std::exp(-1.0f / (0.100f * (float) sampleRate));
            const auto* wet = bloomWork.getReadPointer(0);
            for (int i = 0; i < n; ++i)
            {
                const float absx = std::abs(L[i]);
                duckEnv += ((absx > duckEnv) ? atk : rel) * (absx - duckEnv);
                const float duck = juce::jmax(0.40f, 1.0f / (1.0f + duckEnv * 4.0f));
                L[i] += wet[i] * send * duck;
            }
        }

        juce::FloatVectorOperations::copy(R, L, n);

        const float stackAmt = amt[Stack];
        if (stackAmt > 0.01f)
            processStackWall(L, R, n, stackAmt, root, minor);

        const float phoneAmt = amt[Phone];
        if (phoneAmt > 0.01f)
        {
            processPhone(L, n, 0, phoneAmt);
            processPhone(R, n, 1, phoneAmt);
        }
    }

    VocalDebug takeDebug()
    {
        VocalDebug out = dbg;
        const auto tuneDbg = tune.takeDebug();
        const auto lo = stackLo.takeDebug();
        const auto hi = stackHi.takeDebug();
        out.harmWraps += lo.harmWraps + hi.harmWraps;
        out.harmStarts += lo.harmStarts + hi.harmStarts;
        out.harmBlocked += lo.harmBlocked + hi.harmBlocked;
        out.tuneResets += tuneDbg.tuneResets;
        out.hzJumps += tuneDbg.hzJumps;
        out.lockLost += lo.lockLost + hi.lockLost;
        out.lockGained += lo.lockGained + hi.lockGained;
        out.hz = tuneDbg.hz > 55.0f ? tuneDbg.hz : lo.hz;
        out.locked = tuneDbg.hz > 55.0f ? tuneDbg.locked : lo.locked;
        dbg = {};
        return out;
    }

private:
    static float tapLine(const std::vector<float>& buf, int write, float delaySamp)
    {
        const int n = (int) buf.size();
        if (n <= 1)
            return 0.0f;

        float pos = (float) write - delaySamp;
        while (pos < 0.0f)
            pos += (float) n;
        const int i0 = ((int) pos % n + n) % n;
        const int i1 = (i0 + 1) % n;
        const float f = pos - std::floor(pos);
        return buf[(size_t) i0] + (buf[(size_t) i1] - buf[(size_t) i0]) * f;
    }

    static void addPanned(float* left, float* right, const float* v, int n,
                          float gain, float pan)
    {
        const float angle = (juce::jlimit(-1.0f, 1.0f, pan) + 1.0f)
                            * (juce::MathConstants<float>::halfPi * 0.5f);
        const float gL = gain * std::cos(angle);
        const float gR = gain * std::sin(angle);
        for (int i = 0; i < n; ++i)
        {
            left[i] += v[i] * gL;
            right[i] += v[i] * gR;
        }
    }

    void runCentsVoice(std::vector<float>& buf, int& write, float& delayState,
                       float& oldDelay, float& xfade,
                       const float* src, float* dest, int n,
                       float rate, float targetDelay)
    {
        if (buf.empty())
            return;

        const float minD = juce::jmax(1.0f, targetDelay - 0.004f * (float) sampleRate);
        const float maxD = juce::jmin((float) buf.size() - 2.0f,
                                      targetDelay + 0.004f * (float) sampleRate);
        const float wrap = juce::jmax(1.0f, maxD - minD);
        const int maxTap = (int) buf.size() - 2;
        const float fadeStep = 1.0f / juce::jmax(32.0f, 0.008f * (float) sampleRate);

        for (int i = 0; i < n; ++i)
        {
            buf[(size_t) write] = src[i];
            delayState += 1.0f - rate;
            if (delayState < minD)
            {
                oldDelay = delayState;
                delayState += wrap;
                xfade = 1.0f;
                ++dbg.stackWraps;
            }
            else if (delayState > maxD)
            {
                oldDelay = delayState;
                delayState -= wrap;
                xfade = 1.0f;
                ++dbg.stackWraps;
            }

            float y = tapLine(buf, write, juce::jlimit(1.0f, (float) maxTap, delayState));
            if (xfade > 0.0f)
            {
                oldDelay += 1.0f - rate;
                const float z = tapLine(buf, write, juce::jlimit(1.0f, (float) maxTap, oldDelay));
                y += (z - y) * xfade;
                xfade = juce::jmax(0.0f, xfade - fadeStep);
            }
            dest[i] = y;
            write = (write + 1) % (int) buf.size();
        }
    }

    void lagVoice(std::vector<float>& buf, int& write, const float* src, float* dest,
                  int n, float delaySec)
    {
        const int m = (int) buf.size();
        if (m <= 1)
        {
            juce::FloatVectorOperations::copy(dest, src, n);
            return;
        }

        const int lag = juce::jlimit(1, m - 1, (int) (delaySec * sampleRate));
        for (int i = 0; i < n; ++i)
        {
            buf[(size_t) write] = src[i];
            dest[i] = buf[(size_t) ((write - lag + m) % m)];
            write = (write + 1) % m;
        }
    }

    void processStackWall(float* left, float* right, int n, float amount, int root, bool minor)
    {
        const auto* dry = work.getReadPointer(0);
        stackBuf.setSize(1, n, false, false, true);
        voiceBuf.setSize(1, n, false, false, true);
        auto* shifted = stackBuf.getWritePointer(0);
        auto* voice = voiceBuf.getWritePointer(0);

        const float dblGain = juce::Decibels::decibelsToGain(juce::jmap(amount, -18.0f, -8.0f));
        const float harmGain = juce::Decibels::decibelsToGain(juce::jmap(amount, -22.0f, -12.0f));
        const float dblPan = juce::jmap(amount, 0.28f, 0.40f);
        const float harmPan = juce::jmap(amount, 0.70f, 1.00f);

        runCentsVoice(wallDblL, wallDblPosL, wallDelayL, wallOldL, wallXfadeL, dry, voice, n,
                      std::pow(2.0f, -9.0f / 1200.0f), 0.014f * (float) sampleRate);
        addPanned(left, right, voice, n, dblGain, -dblPan);

        runCentsVoice(wallDblR, wallDblPosR, wallDelayR, wallOldR, wallXfadeR, dry, voice, n,
                      std::pow(2.0f, 11.0f / 1200.0f), 0.023f * (float) sampleRate);
        addPanned(left, right, voice, n, dblGain, dblPan);

        juce::FloatVectorOperations::copy(shifted, dry, n);
        stackLo.processHarmony(shifted, n, root, minor, -2);
        const float hpC = 1.0f - std::exp(-2.0f * juce::MathConstants<float>::pi
                                          * 300.0f / (float) sampleRate);
        for (int i = 0; i < n; ++i)
        {
            stackHpLo += hpC * (shifted[i] - stackHpLo);
            shifted[i] -= stackHpLo;
        }
        lagVoice(wallLagLo, wallLagPosLo, shifted, voice, n, 0.018f);
        addPanned(left, right, voice, n, harmGain, -harmPan);

        if (amount > 0.45f)
        {
            const float fifthAmt = juce::jlimit(0.0f, 1.0f, (amount - 0.45f) / 0.55f);
            juce::FloatVectorOperations::copy(shifted, dry, n);
            stackHi.processHarmony(shifted, n, root, minor, 4);
            lagVoice(wallLagHi, wallLagPosHi, shifted, voice, n, 0.026f);
            addPanned(left, right, voice, n, harmGain * fifthAmt, harmPan);
        }
    }

    void processPhone(float* x, int n, int ch, float phoneAmt)
    {
        const float lo = phoneAmt < 0.50f
                             ? juce::jmap(phoneAmt, 0.0f, 0.50f, 150.0f, 400.0f)
                             : juce::jmap(phoneAmt, 0.50f, 1.0f, 400.0f, 500.0f);
        const float hi = phoneAmt < 0.50f
                             ? juce::jmap(phoneAmt, 0.0f, 0.50f, 8000.0f, 4500.0f)
                             : juce::jmap(phoneAmt, 0.50f, 1.0f, 4500.0f, 2800.0f);
        const float midBoost = phoneAmt < 0.50f
                                   ? juce::jmap(phoneAmt, 0.0f, 0.50f, 0.0f, 1.15f)
                                   : juce::jmap(phoneAmt, 0.50f, 1.0f, 1.15f, 1.85f);
        const float sat = phoneAmt > 0.55f ? (phoneAmt - 0.55f) / 0.45f * 1.15f : 0.0f;
        const float makeup = 1.0f + phoneAmt * 0.45f;
        const float lpC = 1.0f - std::exp(-2.0f * juce::MathConstants<float>::pi * hi / (float) sampleRate);
        const float hpC = 1.0f - std::exp(-2.0f * juce::MathConstants<float>::pi * lo / (float) sampleRate);
        const float midF = 2.0f * std::sin(juce::MathConstants<float>::pi
                                           * juce::jlimit(200.0f, 4000.0f, 1800.0f) / (float) sampleRate);
        const float midQ = 0.28f;

        for (int i = 0; i < n; ++i)
        {
            phoneLp[ch] += lpC * (x[i] - phoneLp[ch]);
            phoneHp[ch] += hpC * (phoneLp[ch] - phoneHp[ch]);
            float s = phoneLp[ch] - phoneHp[ch];

            phoneMidLow[ch] += midF * phoneMidBand[ch];
            const float midHigh = s - phoneMidLow[ch] - midQ * phoneMidBand[ch];
            phoneMidBand[ch] += midF * midHigh;
            s += phoneMidBand[ch] * midBoost;

            if (sat > 0.001f)
                s = std::tanh(s * (1.0f + sat));

            s *= makeup;
            x[i] += (s - x[i]) * phoneAmt;
        }
    }

    double sampleRate = 48000.0;
    int maxBlock = 512;
    VocalPitch tune, stackLo, stackHi;
    juce::AudioBuffer<float> work, stackBuf, voiceBuf, bloomWork;
    juce::dsp::Reverb bloom;
    std::vector<float> echo, dbl;
    std::vector<float> wallDblL, wallDblR, wallLagLo, wallLagHi;
    int echoPos = 0, dblPos = 0;
    int wallDblPosL = 0, wallDblPosR = 0, wallLagPosLo = 0, wallLagPosHi = 0;
    float dblDelayA = 0.0f;
    float dblDelayB = 0.0f;
    float wallDelayL = 0.0f;
    float wallDelayR = 0.0f;
    float wallOldL = 0.0f;
    float wallOldR = 0.0f;
    float wallXfadeL = 0.0f;
    float wallXfadeR = 0.0f;
    VocalDebug dbg {};
    float dblHp = 0.0f;
    float stackHpLo = 0.0f;
    float duckEnv = 0.0f;
    float phoneLp[2] {};
    float phoneHp[2] {};
    float phoneMidLow[2] {};
    float phoneMidBand[2] {};
};
