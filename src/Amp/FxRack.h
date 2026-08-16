#pragma once

#include "Amp/Loudness.h"

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <cmath>
#include <vector>

class FxRack
{
public:
    static constexpr int numJobs = 8;
    static constexpr float bypassFloor = 0.02f;

    enum Job
    {
        Squeeze = 0,
        Talk,
        Shift,
        Echo,
        Bloom,
        Thicken,
        Sweep,
        Pulse
    };

    static const char* jobName(int index, bool bloomShimmer = false)
    {
        static constexpr const char* names[] = {
            "Squeeze", "Talk", "Shift", "Echo", "Bloom", "Thicken", "Sweep", "Pulse"
        };
        if (index == Bloom && bloomShimmer)
            return "Shimmer";
        return names[juce::jlimit(0, numJobs - 1, index)];
    }

    static const char* jobTip(int index, bool bloomShimmer = false)
    {
        if (index == Bloom)
            return bloomShimmer ? "octave-up on the reverb tail" : "spring to pad";

        static constexpr const char* tips[] = {
            "even / sticky / sustain",
            "quack / vocal / auto-wah",
            "bigger / octave / organ",
            "slap / bounce / wash",
            "spring / pad",
            "fatten / two guitars",
            "swirl / jet",
            "chop / surf"
        };
        return tips[juce::jlimit(0, numJobs - 1, index)];
    }

    FxRack()
    {
        for (int i = 0; i < numJobs; ++i)
            amounts[(size_t) i].store(0.0f, std::memory_order_relaxed);
    }

    void setAmount(int index, float value)
    {
        if (juce::isPositiveAndBelow(index, numJobs))
            amounts[(size_t) index].store(juce::jlimit(0.0f, 1.0f, value), std::memory_order_relaxed);
    }

    void setAmounts(const std::array<float, 8>& values)
    {
        for (int i = 0; i < numJobs; ++i)
            setAmount(i, values[(size_t) i]);
    }

    float getAmount(int index) const
    {
        if (! juce::isPositiveAndBelow(index, numJobs))
            return 0.0f;
        return amounts[(size_t) index].load(std::memory_order_relaxed);
    }

    std::array<float, 8> getAmounts() const
    {
        std::array<float, 8> values {};
        for (int i = 0; i < numJobs; ++i)
            values[(size_t) i] = getAmount(i);
        return values;
    }

    void setBloomShimmer(bool shouldShimmer)
    {
        bloomShimmer.store(shouldShimmer, std::memory_order_relaxed);
    }

    bool getBloomShimmer() const
    {
        return bloomShimmer.load(std::memory_order_relaxed);
    }

    float getBloomWetScale() const { return bloomWetScale; }
    float getEchoWetScale() const { return echoWetScale; }

    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        maxBlock = juce::jmax(1, (int) spec.maximumBlockSize);
        const int maxDelay = juce::jmax(256, (int) std::ceil(sampleRate * 2.0));

        squeeze.prepare(spec);
        squeeze.reset();

        echoLine.setMaximumDelayInSamples(maxDelay);
        echoLine.prepare(spec);
        echoLine.reset();

        thicken.prepare((int) std::ceil(sampleRate * 0.048));
        pitchUp.prepare((int) std::ceil(sampleRate * 0.08));
        pitchDown.prepare((int) std::ceil(sampleRate * 0.08));

        flangeLine.setMaximumDelayInSamples(juce::jmax(64, (int) std::ceil(sampleRate * 0.012)));
        flangeLine.prepare(spec);
        flangeLine.reset();

        bloom.prepare(spec);
        bloom.reset();

        work.setSize(1, maxBlock);
        octave.prepare((int) std::ceil(sampleRate * 0.08));

        bloomMix.reset(sampleRate, 0.025);
        bloomMix.setCurrentAndTargetValue(0.0f);
        echoMix.reset(sampleRate, 0.025);
        echoMix.setCurrentAndTargetValue(0.0f);

        resetDynamics();
        calibrateSends();
    }

    void reset()
    {
        squeeze.reset();
        echoLine.reset();
        thicken.reset();
        pitchUp.reset();
        pitchDown.reset();
        flangeLine.reset();
        bloom.reset();
        octave.reset();
        bloomMix.setCurrentAndTargetValue(0.0f);
        echoMix.setCurrentAndTargetValue(0.0f);
        resetDynamics();
    }

    void processPre(juce::AudioBuffer<float>& mono)
    {
        const float squeezeAmt = getAmount(Squeeze);
        const float talkAmt = getAmount(Talk);
        const float shiftAmt = getAmount(Shift);

        if (squeezeAmt >= bypassFloor)
            processSqueeze(mono, squeezeAmt);
        if (talkAmt >= bypassFloor)
            processTalk(mono, talkAmt);
        if (shiftAmt >= bypassFloor)
            processShift(mono, shiftAmt);
    }

    void processPost(juce::AudioBuffer<float>& mono)
    {
        const float thickenAmt = getAmount(Thicken);
        const float sweepAmt = getAmount(Sweep);
        const float pulseAmt = getAmount(Pulse);
        const float echoAmt = getAmount(Echo);
        const float bloomAmt = getAmount(Bloom);

        if (thickenAmt >= bypassFloor)
            processThicken(mono, thickenAmt);
        if (sweepAmt >= bypassFloor)
            processSweep(mono, sweepAmt);
        if (pulseAmt >= bypassFloor)
            processPulse(mono, pulseAmt);

        processEcho(mono, echoAmt);
        processBloom(mono, bloomAmt);
    }

private:
    struct CheapOctave
    {
        std::vector<float> buf;
        size_t write = 0;
        float read = 0.0f;

        void prepare(int n)
        {
            buf.assign((size_t) juce::jmax(32, n), 0.0f);
            reset();
        }

        void reset()
        {
            std::fill(buf.begin(), buf.end(), 0.0f);
            write = 0;
            read = 0.0f;
        }

        float process(float x)
        {
            if (buf.empty())
                return x;

            buf[write] = x;
            const float n = (float) buf.size();
            const float a = interpolate(read);
            const float b = interpolate(std::fmod(read + n * 0.5f, n));
            const float fade = std::sin(juce::MathConstants<float>::pi * (read / n));
            write = (write + 1) % buf.size();
            read += 2.0f;
            if (read >= n)
                read -= n;
            return a * fade + b * (1.0f - fade);
        }

    private:
        float interpolate(float pos) const
        {
            const int n = (int) buf.size();
            const int i0 = ((int) pos % n + n) % n;
            const int i1 = (i0 + 1) % n;
            const float frac = pos - std::floor(pos);
            return buf[(size_t) i0] + (buf[(size_t) i1] - buf[(size_t) i0]) * frac;
        }
    };

    struct ThickenDelay
    {
        std::vector<float> buf;
        size_t write = 0;
        float wow = 0.0f;
        float wowTarget = 0.0f;
        juce::Random rng { 0x71C4E11 };

        void prepare(int n)
        {
            buf.assign((size_t) juce::jmax(64, n), 0.0f);
            reset();
        }

        void reset()
        {
            std::fill(buf.begin(), buf.end(), 0.0f);
            write = 0;
            wow = 0.0f;
            wowTarget = 0.0f;
        }

        void push(float x)
        {
            if (buf.empty())
                return;
            buf[write] = x;
            write = (write + 1) % buf.size();
        }

        float tap(float delaySamp) const
        {
            if (buf.empty())
                return 0.0f;
            const float n = (float) buf.size();
            float pos = (float) write - juce::jlimit(1.0f, n - 2.0f, delaySamp);
            if (pos < 0.0f)
                pos += n;
            return interpolate(pos);
        }

        void walkWow(float amount)
        {
            if (amount < 0.001f)
            {
                wow += 0.002f * (0.0f - wow);
                return;
            }

            wow += 0.0004f * (wowTarget - wow);
            if (std::abs(wowTarget - wow) < 0.03f)
                wowTarget = rng.nextFloat() * 2.0f - 1.0f;
        }

        float wowSamples(float sr) const
        {
            return wow * 0.0005f * sr;
        }

    private:
        float interpolate(float pos) const
        {
            const int n = (int) buf.size();
            const int i0 = ((int) pos % n + n) % n;
            const int i1 = (i0 + 1) % n;
            const float frac = pos - std::floor(pos);
            return buf[(size_t) i0] + (buf[(size_t) i1] - buf[(size_t) i0]) * frac;
        }
    };

    struct MicroPitch
    {
        std::vector<float> buf;
        size_t write = 0;
        float read = 0.0f;

        void prepare(int n)
        {
            buf.assign((size_t) juce::jmax(64, n), 0.0f);
            reset();
        }

        void reset()
        {
            std::fill(buf.begin(), buf.end(), 0.0f);
            write = 0;
            read = 0.0f;
        }

        float process(float x, float rate)
        {
            if (buf.empty())
                return x;

            buf[write] = x;
            const float n = (float) buf.size();
            if (read <= 0.0f && write == 0)
                read = n - 1.0f;
            const float y = interpolate(read);
            write = (write + 1) % buf.size();
            read += rate;
            if (read >= n)
                read -= n;
            if (read < 0.0f)
                read += n;
            return y;
        }

    private:
        float interpolate(float pos) const
        {
            const int n = (int) buf.size();
            const int i0 = ((int) pos % n + n) % n;
            const int i1 = (i0 + 1) % n;
            const float frac = pos - std::floor(pos);
            return buf[(size_t) i0] + (buf[(size_t) i1] - buf[(size_t) i0]) * frac;
        }
    };

    void resetDynamics()
    {
        talkEnv = 0.0f;
        talkLow = 0.0f;
        talkBand = 0.0f;
        octLow = 0.0f;
        octHigh = 0.0f;
        echoFilter = 0.0f;
        flangeLp = 0.0f;
        thicken.reset();
        pitchUp.reset();
        pitchDown.reset();
        sweepLfo = 0.0f;
        pulseLfo = 0.0f;
        for (auto& z : sweepZ)
            z = 0.0f;
        flangeLine.reset();
    }

    static float onePole(float x, float& z, float coeff)
    {
        z += coeff * (x - z);
        return z;
    }

    static float allpass(float x, float coeff, float& z)
    {
        const float y = coeff * x + z;
        z = x - coeff * y;
        return y;
    }

    static float smoothstep01(float t)
    {
        t = juce::jlimit(0.0f, 1.0f, t);
        return t * t * (3.0f - 2.0f * t);
    }

    struct SweepRecipe
    {
        float mix = 0.5f;
        float phaserFb = 0.2f;
        float flangeFb = 0.35f;
        float rate = 0.25f;
        float minHz = 200.0f;
        float maxHz = 2500.0f;
        float minDelayS = 0.002f;
        float maxDelayS = 0.005f;
        float thruDelayS = 0.0f;
        float flangeT = 0.0f;
        float toneHz = 4500.0f;
    };

    static SweepRecipe sweepRecipe(float amount)
    {
        SweepRecipe r;
        r.flangeT = smoothstep01((amount - 0.65f) / 0.35f);
        r.mix = 0.5f;
        r.phaserFb = juce::jmap(amount, 0.2f, 0.28f);
        r.flangeFb = juce::jmap(amount, 0.18f, 0.30f);
        r.rate = juce::jmap(amount, 0.25f, 0.40f);
        r.thruDelayS = 0.0f;
        r.toneHz = 4500.0f;
        return r;
    }

    static float sweepAllpassCoeff(float hz, double sr)
    {
        const float w = juce::MathConstants<float>::pi
                        * juce::jlimit(40.0f, 8000.0f, hz) / (float) sr;
        const float t = std::tan(juce::jlimit(0.01f, 1.2f, w));
        return (1.0f - t) / (1.0f + t);
    }

    static float sweepAlignLfo(const SweepRecipe&)
    {
        return 0.5f;
    }

    void sweepVoices(float x, const SweepRecipe& recipe, float lfo, float& dryOut, float& wet)
    {
        const float fc = recipe.minHz + (recipe.maxHz - recipe.minHz) * lfo;
        float phaser = x + sweepZ[4] * recipe.phaserFb;
        const float coeff = sweepAllpassCoeff(fc, sampleRate);
        for (int s = 0; s < 4; ++s)
            phaser = allpass(phaser, coeff, sweepZ[s]);
        sweepZ[4] = phaser;

        const float minDelay = recipe.minDelayS * (float) sampleRate;
        const float maxDelay = recipe.maxDelayS * (float) sampleRate;
        const float maxSamp = (float) flangeLine.getMaximumDelayInSamples() - 4.0f;
        const float wetDelay = juce::jlimit(2.0f, maxSamp, minDelay + lfo * (maxDelay - minDelay));
        float delayed = flangeLine.popSample(0, wetDelay);
        const float toneCoeff = 1.0f - std::exp(-2.0f * juce::MathConstants<float>::pi
                                                * recipe.toneHz / (float) sampleRate);
        delayed = onePole(delayed, flangeLp, toneCoeff);
        flangeLine.pushSample(0, x + delayed * recipe.flangeFb * recipe.flangeT);

        dryOut = x;
        wet = phaser + (delayed - phaser) * recipe.flangeT;
    }

    void processSqueeze(juce::AudioBuffer<float>& mono, float amount)
    {
        const float threshold = juce::jmap(amount, -16.0f, -28.0f);
        const float ratio = juce::jmap(amount, 2.2f, 8.0f);
        squeeze.setThreshold(threshold);
        squeeze.setRatio(ratio);
        squeeze.setAttack(juce::jmap(amount, 8.0f, 18.0f));
        squeeze.setRelease(juce::jmap(amount, 80.0f, 220.0f));

        juce::dsp::AudioBlock<float> block(mono);
        juce::dsp::ProcessContextReplacing<float> context(block);
        squeeze.process(context);

        const float makeupDb = -threshold * (1.0f - 1.0f / ratio) * 0.30f;
        mono.applyGain(juce::Decibels::decibelsToGain(makeupDb));
    }

    void processTalk(juce::AudioBuffer<float>& mono, float amount)
    {
        auto* data = mono.getWritePointer(0);
        const int n = mono.getNumSamples();
        const float attack = 1.0f - std::exp(-1.0f / (0.004f * (float) sampleRate));
        const float release = 1.0f - std::exp(-1.0f / (0.080f * (float) sampleRate));
        const float q = juce::jmap(amount, 0.35f, 0.18f);

        for (int i = 0; i < n; ++i)
        {
            const float x = data[i];
            const float absx = std::abs(x);
            talkEnv += (absx > talkEnv ? attack : release) * (absx - talkEnv);

            const float fc = 320.0f + talkEnv * juce::jmap(amount, 1400.0f, 2600.0f);
            const float f = 2.0f * std::sin(juce::MathConstants<float>::pi
                                            * juce::jlimit(40.0f, 6000.0f, fc) / (float) sampleRate);
            talkLow += f * talkBand;
            const float high = x - talkLow - q * talkBand;
            talkBand += f * high;
            const float band = talkBand;
            data[i] = x * (1.0f - 0.72f * amount) + band * (0.95f * amount);
        }
    }

    void processShift(juce::AudioBuffer<float>& mono, float amount)
    {
        auto* data = mono.getWritePointer(0);
        const int n = mono.getNumSamples();
        const float mix = juce::jmap(amount, 0.18f, 0.62f);
        const float downCoeff = 1.0f - std::exp(-2.0f * juce::MathConstants<float>::pi * 420.0f / (float) sampleRate);
        const float upCoeff = 1.0f - std::exp(-2.0f * juce::MathConstants<float>::pi * 900.0f / (float) sampleRate);

        for (int i = 0; i < n; ++i)
        {
            const float x = data[i];
            const float rect = std::abs(x);
            const float down = onePole(rect, octLow, downCoeff);
            const float up = x - onePole(x, octHigh, upCoeff);
            const float wet = down * 1.15f + up * (0.22f * amount);
            data[i] = x * (1.0f - mix) + wet * mix;
        }
    }

    struct ThickenRecipe
    {
        float mix = 0.22f;
        float delayA = 0.012f;
        float delayB = 0.018f;
        float cents = 0.5f;
        float voiceB = 0.0f;
        float wow = 0.0f;
    };

    static ThickenRecipe thickenRecipe(float amount)
    {
        ThickenRecipe r;
        r.mix = juce::jmap(amount, 0.22f, 0.50f);
        r.delayA = juce::jmap(amount, 0.012f, 0.022f);
        r.delayB = juce::jmap(amount, 0.018f, 0.032f);
        r.cents = juce::jmap(amount, 0.5f, 12.0f);
        r.voiceB = smoothstep01((amount - 0.25f) / 0.55f);
        r.wow = smoothstep01((amount - 0.55f) / 0.45f);
        return r;
    }

    float thickenWet(float x, const ThickenRecipe& recipe)
    {
        thicken.push(x);
        thicken.walkWow(recipe.wow);
        const float delayA = recipe.delayA * (float) sampleRate;
        const float delayB = recipe.delayB * (float) sampleRate
                             + thicken.wowSamples((float) sampleRate) * recipe.wow;
        float a = thicken.tap(delayA);
        float b = thicken.tap(delayB);
        if (recipe.cents > 2.0f)
        {
            const float rateUp = std::pow(2.0f, recipe.cents / 1200.0f);
            const float rateDown = std::pow(2.0f, -recipe.cents / 1200.0f);
            a = pitchUp.process(a, rateUp);
            b = pitchDown.process(b, rateDown);
        }
        return a + b * recipe.voiceB;
    }

    void processThicken(juce::AudioBuffer<float>& mono, float amount)
    {
        auto* data = mono.getWritePointer(0);
        const int n = mono.getNumSamples();
        const auto recipe = thickenRecipe(amount);
        const float send = recipe.mix * thickenScale;

        for (int i = 0; i < n; ++i)
        {
            const float x = data[i];
            data[i] = x + thickenWet(x, recipe) * send;
        }
    }

    void processSweep(juce::AudioBuffer<float>& mono, float amount)
    {
        auto* data = mono.getWritePointer(0);
        const int n = mono.getNumSamples();
        const auto recipe = sweepRecipe(amount);
        const float scale = sweepPhaserScale
                            + (sweepFlangeScale - sweepPhaserScale) * recipe.flangeT;
        const float inc = juce::MathConstants<float>::twoPi * recipe.rate / (float) sampleRate;

        for (int i = 0; i < n; ++i)
        {
            const float x = data[i];
            const float lfo = 0.5f + 0.5f * std::sin(sweepLfo);
            sweepLfo += inc;
            if (sweepLfo > juce::MathConstants<float>::twoPi)
                sweepLfo -= juce::MathConstants<float>::twoPi;

            float dryOut = x;
            float wet = x;
            sweepVoices(x, recipe, lfo, dryOut, wet);
            data[i] = x + ((dryOut - x) + wet * recipe.mix) * scale;
        }
    }

    void processPulse(juce::AudioBuffer<float>& mono, float amount)
    {
        auto* data = mono.getWritePointer(0);
        const int n = mono.getNumSamples();
        const float depth = juce::jmap(amount, 0.25f, 0.92f);
        const float rate = 5.2f;
        const float inc = juce::MathConstants<float>::twoPi * rate / (float) sampleRate;
        const float squareAmt = juce::jlimit(0.0f, 1.0f, (amount - 0.45f) / 0.55f);

        for (int i = 0; i < n; ++i)
        {
            const float sine = std::sin(pulseLfo);
            pulseLfo += inc;
            if (pulseLfo > juce::MathConstants<float>::twoPi)
                pulseLfo -= juce::MathConstants<float>::twoPi;
            const float square = sine >= 0.0f ? 1.0f : -1.0f;
            const float lfo = sine + (square - sine) * squareAmt;
            const float gain = 1.0f - depth * (0.5f - 0.5f * lfo);
            data[i] *= gain;
        }
    }

    struct EchoRecipe
    {
        float mix = 0.0f;
        float timeMs = 140.0f;
        float feedback = 0.0f;
        float toneHz = 6500.0f;
    };

    static EchoRecipe echoRecipe(float amount)
    {
        EchoRecipe r;
        r.mix = amount < bypassFloor ? 0.0f : juce::jmap(amount, 0.20f, 1.0f);
        r.timeMs = amount < 0.38f ? juce::jmap(amount / 0.38f, 90.0f, 140.0f)
                 : amount < 0.72f ? juce::jmap((amount - 0.38f) / 0.34f, 320.0f, 400.0f)
                                  : juce::jmap((amount - 0.72f) / 0.28f, 420.0f, 520.0f);
        r.feedback = amount < bypassFloor ? 0.0f
                   : amount < 0.38f ? juce::jmap(amount / 0.38f, 0.08f, 0.18f)
                   : amount < 0.72f ? juce::jmap((amount - 0.38f) / 0.34f, 0.28f, 0.42f)
                                    : juce::jmap((amount - 0.72f) / 0.28f, 0.48f, 0.62f);
        r.toneHz = amount < 0.72f ? 6500.0f : juce::jmap((amount - 0.72f) / 0.28f, 4200.0f, 2200.0f);
        return r;
    }

    void applyBloomTank(float recipe)
    {
        juce::dsp::Reverb::Parameters p;
        p.roomSize = juce::jmap(recipe, 0.18f, 0.88f);
        p.damping = recipe < 0.55f ? juce::jmap(recipe / 0.55f, 0.25f, 0.45f)
                                   : juce::jmap((recipe - 0.55f) / 0.45f, 0.45f, 0.72f);
        p.wetLevel = 1.0f;
        p.dryLevel = 0.0f;
        p.width = 0.25f;
        p.freezeMode = 0.0f;
        bloom.setParameters(p);
    }

    void processEcho(juce::AudioBuffer<float>& mono, float amount)
    {
        auto* data = mono.getWritePointer(0);
        const int n = mono.getNumSamples();
        const bool audible = amount >= bypassFloor;
        const auto recipe = echoRecipe(amount);

        if (audible)
        {
            lastEchoTimeMs = recipe.timeMs;
            lastEchoFeedback = recipe.feedback;
            lastEchoToneHz = recipe.toneHz;
        }

        echoMix.setTargetValue(audible ? recipe.mix * echoWetScale : 0.0f);
        const bool trailing = echoMix.isSmoothing() || echoMix.getCurrentValue() > 1.0e-4f;
        const float feedback = (audible || trailing) ? lastEchoFeedback : 0.0f;
        const float toneHz = lastEchoToneHz;
        const float toneCoeff = 1.0f - std::exp(-2.0f * juce::MathConstants<float>::pi * toneHz / (float) sampleRate);
        const float delaySamp = juce::jlimit(1.0f, (float) echoLine.getMaximumDelayInSamples() - 2.0f,
                                             lastEchoTimeMs * 0.001f * (float) sampleRate);

        for (int i = 0; i < n; ++i)
        {
            const float x = data[i];
            const float delayed = echoLine.popSample(0, delaySamp);
            const float dark = onePole(delayed, echoFilter, toneCoeff);
            echoLine.pushSample(0, x + dark * feedback);
            data[i] = x + dark * echoMix.getNextValue();
        }
    }

    void processBloom(juce::AudioBuffer<float>& mono, float amount)
    {
        const int n = mono.getNumSamples();
        if (work.getNumSamples() < n)
            work.setSize(1, n, false, false, true);

        const bool audible = amount >= bypassFloor;
        if (audible)
            lastBloomRecipe = amount;

        applyBloomTank(audible ? amount : lastBloomRecipe);
        bloomMix.setTargetValue(audible ? juce::jmap(amount, 0.18f, 1.0f) * bloomWetScale : 0.0f);

        const bool trailing = bloomMix.isSmoothing() || bloomMix.getCurrentValue() > 1.0e-5f;
        work.makeCopyOf(mono, true);

        juce::dsp::AudioBlock<float> block(work);
        juce::dsp::ProcessContextReplacing<float> context(block);
        bloom.process(context);

        if (bloomShimmer.load(std::memory_order_relaxed) && (audible || trailing))
        {
            auto* wetPtr = work.getWritePointer(0);
            const float octAmt = juce::jmap(lastBloomRecipe, 0.18f, 0.42f) * shimmerOctaveScale;
            for (int i = 0; i < n; ++i)
                wetPtr[i] += octave.process(wetPtr[i]) * octAmt;
        }

        if (! audible && ! trailing)
        {
            bloomMix.skip(n);
            return;
        }

        auto* dry = mono.getWritePointer(0);
        const auto* wet = work.getReadPointer(0);
        for (int i = 0; i < n; ++i)
            dry[i] += wet[i] * bloomMix.getNextValue();
    }

    void processReplacingChunked(juce::dsp::Reverb& reverb, juce::AudioBuffer<float>& buffer)
    {
        const int total = buffer.getNumSamples();
        int done = 0;
        while (done < total)
        {
            const int chunk = juce::jmin(maxBlock, total - done);
            work.copyFrom(0, 0, buffer, 0, done, chunk);
            auto block = juce::dsp::AudioBlock<float>(work).getSubBlock(0, (size_t) chunk);
            juce::dsp::ProcessContextReplacing<float> ctx(block);
            reverb.process(ctx);
            buffer.copyFrom(0, done, work, 0, 0, chunk);
            done += chunk;
        }
    }

    void runThickenProbe(juce::AudioBuffer<float>& dryBuf, juce::AudioBuffer<float>& wetBuf)
    {
        thicken.reset();
        pitchUp.reset();
        pitchDown.reset();
        const auto recipe = thickenRecipe(1.0f);
        const auto* dry = dryBuf.getReadPointer(0);
        auto* wet = wetBuf.getWritePointer(0);
        const int n = dryBuf.getNumSamples();
        for (int i = 0; i < n; ++i)
            wet[i] = thickenWet(dry[i], recipe);
    }

    void runSweepPhaserProbe(juce::AudioBuffer<float>& dryBuf, juce::AudioBuffer<float>& wetBuf)
    {
        for (auto& z : sweepZ)
            z = 0.0f;
        flangeLine.reset();

        const auto recipe = sweepRecipe(0.0f);
        const float lfo = 0.5f;
        const auto* dry = dryBuf.getReadPointer(0);
        auto* wet = wetBuf.getWritePointer(0);
        const int n = dryBuf.getNumSamples();

        for (int i = 0; i < n; ++i)
        {
            const float x = dry[i];
            float dryOut = x;
            float voice = x;
            sweepVoices(x, recipe, lfo, dryOut, voice);
            wet[i] = (dryOut - x) + voice * recipe.mix;
        }
    }

    void runSweepFlangeProbe(juce::AudioBuffer<float>& dryBuf, juce::AudioBuffer<float>& wetBuf)
    {
        for (auto& z : sweepZ)
            z = 0.0f;
        flangeLine.reset();

        const auto recipe = sweepRecipe(1.0f);
        const float lfo = sweepAlignLfo(recipe);
        const auto* dry = dryBuf.getReadPointer(0);
        auto* wet = wetBuf.getWritePointer(0);
        const int n = dryBuf.getNumSamples();

        for (int i = 0; i < n; ++i)
        {
            const float x = dry[i];
            float dryOut = x;
            float voice = x;
            sweepVoices(x, recipe, lfo, dryOut, voice);
            wet[i] = (dryOut - x) + voice * recipe.mix;
        }
    }

    void runEchoProbe(juce::AudioBuffer<float>& dryBuf, juce::AudioBuffer<float>& wetBuf, const EchoRecipe& recipe)
    {
        echoLine.reset();
        echoFilter = 0.0f;
        const int n = dryBuf.getNumSamples();
        const float toneCoeff = 1.0f - std::exp(-2.0f * juce::MathConstants<float>::pi
                                                * recipe.toneHz / (float) sampleRate);
        const float delaySamp = juce::jlimit(1.0f, (float) echoLine.getMaximumDelayInSamples() - 2.0f,
                                             recipe.timeMs * 0.001f * (float) sampleRate);
        const auto* dry = dryBuf.getReadPointer(0);
        auto* wet = wetBuf.getWritePointer(0);
        for (int i = 0; i < n; ++i)
        {
            const float x = dry[i];
            const float delayed = echoLine.popSample(0, delaySamp);
            const float dark = onePole(delayed, echoFilter, toneCoeff);
            echoLine.pushSample(0, x + dark * recipe.feedback);
            wet[i] = dark;
        }
    }

    void calibrateSends()
    {
        const int probeN = juce::jmax(8192, (int) std::ceil(sampleRate * 0.85));
        juce::AudioBuffer<float> probeDry (1, probeN);
        juce::AudioBuffer<float> probeWet (1, probeN);
        Loudness::fillPink(probeDry.getWritePointer(0), probeN, 0xB1004);

        applyBloomTank(1.0f);
        bloom.reset();
        probeWet.makeCopyOf(probeDry);
        processReplacingChunked(bloom, probeWet);
        const int bloomSkip = juce::jmin(512, probeN / 4);
        bloomWetScale = Loudness::wetSendScale(probeDry.getReadPointer(0) + bloomSkip,
                                               probeWet.getReadPointer(0) + bloomSkip,
                                               probeN - bloomSkip);

        const float peakWet = juce::jmax(probeWet.getMagnitude(0, bloomSkip, probeN - bloomSkip), 1.0e-6f);
        octave.reset();
        auto* wetPtr = probeWet.getWritePointer(0);
        for (int i = 0; i < probeN; ++i)
            wetPtr[i] += octave.process(wetPtr[i]) * 0.42f;
        const float peakShim = juce::jmax(probeWet.getMagnitude(0, bloomSkip, probeN - bloomSkip), 1.0e-6f);
        shimmerOctaveScale = peakShim > peakWet * Loudness::shimmerPeakBudget
                                 ? (peakWet * Loudness::shimmerPeakBudget) / peakShim
                                 : 1.0f;
        octave.reset();
        bloom.reset();
        applyBloomTank(lastBloomRecipe);

        const auto maxEcho = echoRecipe(1.0f);
        runEchoProbe(probeDry, probeWet, maxEcho);
        const int echoSkip = juce::jlimit(1, probeN / 2,
                                          (int) std::ceil(maxEcho.timeMs * 0.001 * sampleRate));
        echoWetScale = Loudness::wetSendScale(probeDry.getReadPointer(0) + echoSkip,
                                              probeWet.getReadPointer(0) + echoSkip,
                                              probeN - echoSkip);
        echoLine.reset();
        echoFilter = 0.0f;

        const int sweepSkip = juce::jmin(512, probeN / 4);
        runSweepPhaserProbe(probeDry, probeWet);
        sweepPhaserScale = Loudness::wetSendScale(probeDry.getReadPointer(0) + sweepSkip,
                                                  probeWet.getReadPointer(0) + sweepSkip,
                                                  probeN - sweepSkip);
        runSweepFlangeProbe(probeDry, probeWet);
        sweepFlangeScale = Loudness::wetSendScale(probeDry.getReadPointer(0) + sweepSkip,
                                                  probeWet.getReadPointer(0) + sweepSkip,
                                                  probeN - sweepSkip);
        for (auto& z : sweepZ)
            z = 0.0f;
        flangeLine.reset();
        flangeLp = 0.0f;
        sweepLfo = 0.0f;

        const int thickenSkip = juce::jmin(1024, probeN / 4);
        runThickenProbe(probeDry, probeWet);
        thickenScale = Loudness::wetSendScale(probeDry.getReadPointer(0) + thickenSkip,
                                              probeWet.getReadPointer(0) + thickenSkip,
                                              probeN - thickenSkip);
        thicken.reset();
        pitchUp.reset();
        pitchDown.reset();
    }

    std::array<std::atomic<float>, 8> amounts {};
    std::atomic<bool> bloomShimmer { false };

    double sampleRate = 48000.0;
    int maxBlock = 512;
    float bloomWetScale = 1.0f;
    float echoWetScale = 1.0f;
    float sweepPhaserScale = 1.0f;
    float sweepFlangeScale = 1.0f;
    float thickenScale = 1.0f;
    float shimmerOctaveScale = 1.0f;
    float lastBloomRecipe = 1.0f;
    float lastEchoTimeMs = 140.0f;
    float lastEchoFeedback = 0.0f;
    float lastEchoToneHz = 6500.0f;
    juce::SmoothedValue<float> bloomMix;
    juce::SmoothedValue<float> echoMix;
    juce::dsp::Compressor<float> squeeze;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> echoLine;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd> flangeLine;
    juce::dsp::Reverb bloom;
    CheapOctave octave;
    ThickenDelay thicken;
    MicroPitch pitchUp;
    MicroPitch pitchDown;
    juce::AudioBuffer<float> work;

    float talkEnv = 0.0f;
    float talkLow = 0.0f;
    float talkBand = 0.0f;
    float octLow = 0.0f;
    float octHigh = 0.0f;
    float echoFilter = 0.0f;
    float flangeLp = 0.0f;
    float sweepLfo = 0.0f;
    float pulseLfo = 0.0f;
    float sweepZ[5] {};
};
