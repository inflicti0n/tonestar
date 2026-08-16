#pragma once

#include "BinaryData.h"
#include "Amp/Loudness.h"

#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <cmath>

class Acoustics
{
public:
    static constexpr float defaultSize = 0.15f;
    static constexpr float defaultBack = 0.25f;

    void setCabSize(float value)
    {
        cabSize.store(juce::jlimit(0.0f, 1.0f, value), std::memory_order_relaxed);
    }

    void setCabBack(float value)
    {
        cabBack.store(juce::jlimit(0.0f, 1.0f, value), std::memory_order_relaxed);
    }

    void setBinaural(bool shouldUse)
    {
        binaural.store(shouldUse, std::memory_order_relaxed);
    }

    float getCabSize() const { return cabSize.load(std::memory_order_relaxed); }
    float getCabBack() const { return cabBack.load(std::memory_order_relaxed); }
    bool getBinaural() const { return binaural.load(std::memory_order_relaxed); }
    float getCabMakeup() const { return cabMakeup; }
    float getHrtfMakeup() const { return hrtfMakeup; }

    static const char* sizeLand(float value)
    {
        if (value < 0.33f)
            return "Combo";
        if (value < 0.67f)
            return "Twin";
        return "Stack";
    }

    static const char* backLand(float value)
    {
        return value < 0.5f ? "Open" : "Closed";
    }

    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        const double newRate = spec.sampleRate;
        const bool first = ! irsLoaded;
        const bool rateChanged = first || std::abs(newRate - sampleRate) > 0.5;
        sampleRate = newRate;
        maxBlock = juce::jmax(1, (int) spec.maximumBlockSize);

        work.setSize(1, maxBlock);
        dry.setSize(1, maxBlock);
        hrtfL.setSize(1, maxBlock);
        hrtfR.setSize(1, maxBlock);
        work.clear();
        dry.clear();
        hrtfL.clear();
        hrtfR.clear();

        juce::dsp::ProcessSpec mono { spec.sampleRate, (juce::uint32) maxBlock, 1 };

        if (! irsLoaded)
        {
            cone.loadImpulseResponse(BinaryData::even_v30_sm57_wav,
                                     (size_t) BinaryData::even_v30_sm57_wavSize,
                                     juce::dsp::Convolution::Stereo::no,
                                     juce::dsp::Convolution::Trim::no,
                                     0,
                                     juce::dsp::Convolution::Normalise::no);
            earL.loadImpulseResponse(BinaryData::sadie_d2_front_L_wav,
                                     (size_t) BinaryData::sadie_d2_front_L_wavSize,
                                     juce::dsp::Convolution::Stereo::no,
                                     juce::dsp::Convolution::Trim::no,
                                     0,
                                     juce::dsp::Convolution::Normalise::no);
            earR.loadImpulseResponse(BinaryData::sadie_d2_front_R_wav,
                                     (size_t) BinaryData::sadie_d2_front_R_wavSize,
                                     juce::dsp::Convolution::Stereo::no,
                                     juce::dsp::Convolution::Trim::no,
                                     0,
                                     juce::dsp::Convolution::Normalise::no);
            irsLoaded = true;
        }

        cone.prepare(mono);
        earL.prepare(mono);
        earR.prepare(mono);

        sizeLow.prepare(mono);
        sizeDark.prepare(mono);
        sizeBeam.prepare(mono);
        backLow.prepare(mono);
        backLeak.prepare(mono);
        floorTone.prepare(mono);
        tailTone.prepare(mono);

        delay.prepare(mono);
        delay.setMaximumDelayInSamples(juce::jmax(4096, (int) std::ceil(sampleRate * 0.05)));
        tail.prepare(mono);
        tail.setMaximumDelayInSamples(juce::jmax(8192, (int) std::ceil(sampleRate * 0.35)));

        binauralMix.reset(sampleRate, 0.025);
        binauralMix.setCurrentAndTargetValue(0.0f);
        cabMakeupSmooth.reset(sampleRate, 0.025);

        if (rateChanged)
        {
            buildPinkProbe();
            lastSize = -1.0f;
            lastBack = -1.0f;
            buildCabLut();
            measureHrtfMakeup();
        }

        updateRecipe(true);
        const float startMakeup = lookupCabMakeup(cabSize.load(std::memory_order_relaxed),
                                                  cabBack.load(std::memory_order_relaxed));
        cabMakeupSmooth.setCurrentAndTargetValue(startMakeup);
        cabMakeup = startMakeup;
        reset();
    }

    void reset()
    {
        cone.reset();
        earL.reset();
        earR.reset();
        sizeLow.reset();
        sizeDark.reset();
        sizeBeam.reset();
        backLow.reset();
        backLeak.reset();
        floorTone.reset();
        tailTone.reset();
        delay.reset();
        tail.reset();
        work.clear();
        dry.clear();
        hrtfL.clear();
        hrtfR.clear();
        binauralMix.setCurrentAndTargetValue(binaural.load(std::memory_order_relaxed) ? 1.0f : 0.0f);
    }

    void process(const juce::AudioBuffer<float>& monoIn, juce::AudioBuffer<float>& stereoOut)
    {
        const int total = juce::jmin(monoIn.getNumSamples(), stereoOut.getNumSamples());
        if (total <= 0 || stereoOut.getNumChannels() < 1 || maxBlock <= 0)
            return;

        updateRecipe(false);
        binauralMix.setTargetValue(binaural.load(std::memory_order_relaxed) ? 1.0f : 0.0f);

        int done = 0;
        while (done < total)
        {
            const int n = juce::jmin(maxBlock, total - done);
            processSlice(monoIn, stereoOut, done, n);
            done += n;
        }
    }

private:
    using Filter = juce::dsp::IIR::Filter<float>;
    using Coeffs = juce::dsp::IIR::Coefficients<float>;
    using Delay = juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear>;

    static juce::dsp::AudioBlock<float> view(juce::AudioBuffer<float>& buffer, int n)
    {
        const int count = juce::jlimit(0, buffer.getNumSamples(), n);
        return juce::dsp::AudioBlock<float>(buffer).getSubBlock(0, (size_t) count);
    }

    static void processReplacing(juce::dsp::Convolution& conv, juce::AudioBuffer<float>& buffer, int n)
    {
        auto block = view(buffer, n);
        juce::dsp::ProcessContextReplacing<float> ctx(block);
        conv.process(ctx);
    }

    static void processReplacing(Filter& filter, juce::AudioBuffer<float>& buffer, int n)
    {
        auto block = view(buffer, n);
        juce::dsp::ProcessContextReplacing<float> ctx(block);
        filter.process(ctx);
    }

    static float guard(float x)
    {
        return juce::jlimit(-8.0f, 8.0f, x);
    }

    float lookupCabMakeup(float size, float back) const
    {
        size = juce::jlimit(0.0f, 1.0f, size);
        back = juce::jlimit(0.0f, 1.0f, back);
        const float sx = size * (float) (lutN - 1);
        const float sy = back * (float) (lutN - 1);
        const int i0 = (int) sx;
        const int j0 = (int) sy;
        const int i1 = juce::jmin(i0 + 1, lutN - 1);
        const int j1 = juce::jmin(j0 + 1, lutN - 1);
        const float fx = sx - (float) i0;
        const float fy = sy - (float) j0;
        const float a = cabLut[i0][j0] * (1.0f - fy) + cabLut[i0][j1] * fy;
        const float b = cabLut[i1][j0] * (1.0f - fy) + cabLut[i1][j1] * fy;
        return a * (1.0f - fx) + b * fx;
    }

    static void applyGain(juce::AudioBuffer<float>& buffer, int n, float gain)
    {
        buffer.applyGain(0, 0, juce::jmin(n, buffer.getNumSamples()), gain);
    }

    void processSlice(const juce::AudioBuffer<float>& monoIn, juce::AudioBuffer<float>& stereoOut,
                      int offset, int n)
    {
        work.copyFrom(0, 0, monoIn, 0, offset, n);
        processCab(work, n);
        cabMakeupSmooth.setTargetValue(lookupCabMakeup(lastSize, lastBack));
        if (n > 1)
            cabMakeupSmooth.skip(n - 1);
        cabMakeup = cabMakeupSmooth.getNextValue();
        applyGain(work, n, cabMakeup);

        const bool needHrtf = binauralMix.getCurrentValue() > 0.001f
                           || binauralMix.getTargetValue() > 0.001f;

        if (! needHrtf)
        {
            copyMonoToStereo(work, n, stereoOut, offset);
            binauralMix.skip(n);
            return;
        }

        hrtfL.copyFrom(0, 0, work, 0, 0, n);
        hrtfR.copyFrom(0, 0, work, 0, 0, n);
        processReplacing(earL, hrtfL, n);
        processReplacing(earR, hrtfR, n);

        auto* mono = work.getReadPointer(0);
        auto* left = hrtfL.getReadPointer(0);
        auto* right = hrtfR.getReadPointer(0);
        auto* outL = stereoOut.getWritePointer(0);
        auto* outR = stereoOut.getNumChannels() > 1 ? stereoOut.getWritePointer(1) : nullptr;

        for (int i = 0; i < n; ++i)
        {
            const float mix = binauralMix.getNextValue();
            const float dryAmt = 1.0f - mix;
            const float l = guard(mono[i] * dryAmt + left[i] * mix * hrtfMakeup);
            const float r = guard(mono[i] * dryAmt + right[i] * mix * hrtfMakeup);
            outL[offset + i] = l;
            if (outR != nullptr)
                outR[offset + i] = r;
        }
    }

    void copyMonoToStereo(const juce::AudioBuffer<float>& mono, int n,
                          juce::AudioBuffer<float>& stereo, int destOffset)
    {
        const int count = juce::jmin(n, mono.getNumSamples(),
                                     stereo.getNumSamples() - destOffset);
        if (count <= 0)
            return;
        auto* src = mono.getReadPointer(0);
        auto* outL = stereo.getWritePointer(0);
        auto* outR = stereo.getNumChannels() > 1 ? stereo.getWritePointer(1) : nullptr;
        for (int i = 0; i < count; ++i)
        {
            const float s = guard(src[i]);
            outL[destOffset + i] = s;
            if (outR != nullptr)
                outR[destOffset + i] = s;
        }
    }

    void processCab(juce::AudioBuffer<float>& buffer, int n)
    {
        n = juce::jmin(n, buffer.getNumSamples(), dry.getNumSamples());
        dry.copyFrom(0, 0, buffer, 0, 0, n);

        processReplacing(cone, buffer, n);

        const float irMix = juce::jlimit(0.0f, 1.0f, 0.72f + 0.08f * lastSize);
        auto* wet = buffer.getWritePointer(0);
        auto* dryPtr = dry.getReadPointer(0);
        for (int i = 0; i < n; ++i)
            wet[i] = dryPtr[i] * (1.0f - irMix) + wet[i] * irMix;

        processReplacing(sizeLow, buffer, n);
        processReplacing(sizeDark, buffer, n);
        processReplacing(sizeBeam, buffer, n);
        processReplacing(backLow, buffer, n);
        processReplacing(backLeak, buffer, n);
        processRoom(buffer, n);
    }

    void processRoom(juce::AudioBuffer<float>& buffer, int n)
    {
        const float size = lastSize;
        const float back = lastBack;
        const float roomAmt = juce::jlimit(0.0f, 1.0f, (0.85f + 0.25f * size) * (1.0f - 0.42f * back));

        const float floorDelay = (float) (sampleRate * (0.0014 + 0.0014 * (double) size));
        const float tap1Delay = (float) (sampleRate * 0.0052);
        const float tap2Delay = (float) (sampleRate * 0.0091);
        const float tap3Delay = (float) (sampleRate * 0.0144);
        const float tailDelay = (float) (sampleRate * (0.14 + 0.08 * (double) size));

        const float floorGain = (0.13f + 0.07f * size) * roomAmt;
        const float tap1Gain = 0.065f * roomAmt;
        const float tap2Gain = 0.042f * roomAmt;
        const float tap3Gain = 0.028f * roomAmt;
        const float tailMix = (0.055f + 0.035f * size) * roomAmt;
        const float tailFb = 0.26f + 0.08f * size - 0.05f * back;
        tail.setDelay(tailDelay);

        auto* data = buffer.getWritePointer(0);
        n = juce::jmin(n, buffer.getNumSamples());

        for (int i = 0; i < n; ++i)
        {
            const float x = data[i];
            delay.pushSample(0, x);

            float early = delay.popSample(0, floorDelay, false) * floorGain;
            early = floorTone.processSample(early);
            early += delay.popSample(0, tap1Delay, false) * tap1Gain;
            early += delay.popSample(0, tap2Delay, false) * tap2Gain;
            early += delay.popSample(0, tap3Delay, true) * tap3Gain;

            const float tailIn = x + early * 0.35f;
            const float delayed = tail.popSample(0);
            const float tailSample = tailTone.processSample(delayed);
            tail.pushSample(0, tailIn + tailSample * tailFb);

            data[i] = x + early + tailSample * tailMix;
        }
    }

    void updateRecipe(bool force)
    {
        const float size = cabSize.load(std::memory_order_relaxed);
        const float back = cabBack.load(std::memory_order_relaxed);
        const bool moved = force
            || std::abs(size - lastSize) > 0.0015f
            || std::abs(back - lastBack) > 0.0015f;
        if (! moved)
            return;

        applyRecipe(size, back);
    }

    void applyRecipe(float size, float back)
    {
        lastSize = size;
        lastBack = back;

        const float couplingDb = juce::jmap(size, 0.0f, 6.5f);
        const float darkHz = juce::jmap(size, 11000.0f, 4800.0f);
        const float beamDb = juce::jmap(size, 1.2f, -5.5f);
        const float closedLowDb = juce::jmap(back, -2.2f, 5.5f);
        const float leakDb = juce::jmap(back, 4.0f, -3.8f);
        const float floorHz = juce::jmap(size, 2600.0f, 1500.0f);
        const float tailHz = juce::jmap(back, 1900.0f, 1050.0f);

        sizeLow.coefficients = Coeffs::makePeakFilter(sampleRate, 145.0f, 0.75f,
                                                      juce::Decibels::decibelsToGain(couplingDb));
        sizeDark.coefficients = Coeffs::makeLowPass(sampleRate, juce::jmax(2800.0f, darkHz), 0.7f);
        sizeBeam.coefficients = Coeffs::makeHighShelf(sampleRate, 4300.0f, 0.7f,
                                                      juce::Decibels::decibelsToGain(beamDb));
        backLow.coefficients = Coeffs::makePeakFilter(sampleRate, 88.0f, 0.8f,
                                                      juce::Decibels::decibelsToGain(closedLowDb));
        backLeak.coefficients = Coeffs::makeHighShelf(sampleRate, 3100.0f, 0.7f,
                                                      juce::Decibels::decibelsToGain(leakDb));
        floorTone.coefficients = Coeffs::makeLowPass(sampleRate, floorHz, 0.7f);
        tailTone.coefficients = Coeffs::makeLowPass(sampleRate, tailHz, 0.7f);
    }

    void buildPinkProbe()
    {
        probeDry.setSize(1, probeLength);
        probeWet.setSize(1, probeLength);
        auto* dest = probeDry.getWritePointer(0);
        juce::Random rng { 0xAC0E57 };
        float b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0;
        for (int i = 0; i < probeLength; ++i)
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
        dryProbeRms = juce::jmax(probeDry.getRMSLevel(0, 0, probeLength), 1.0e-6f);
    }

    void processCabChunked(juce::AudioBuffer<float>& buffer)
    {
        const int total = buffer.getNumSamples();
        int done = 0;
        while (done < total)
        {
            const int chunk = juce::jmin(maxBlock, total - done);
            work.copyFrom(0, 0, buffer, 0, done, chunk);
            processCab(work, chunk);
            buffer.copyFrom(0, done, work, 0, 0, chunk);
            done += chunk;
        }
    }

    void processConvChunked(juce::dsp::Convolution& conv, juce::AudioBuffer<float>& buffer)
    {
        const int total = buffer.getNumSamples();
        int done = 0;
        while (done < total)
        {
            const int chunk = juce::jmin(maxBlock, total - done);
            work.copyFrom(0, 0, buffer, 0, done, chunk);
            processReplacing(conv, work, chunk);
            buffer.copyFrom(0, done, work, 0, 0, chunk);
            done += chunk;
        }
    }

    void resetCabState()
    {
        cone.reset();
        sizeLow.reset();
        sizeDark.reset();
        sizeBeam.reset();
        backLow.reset();
        backLeak.reset();
        floorTone.reset();
        tailTone.reset();
        delay.reset();
        tail.reset();
        work.clear();
        dry.clear();
    }

    float measureCabCell()
    {
        resetCabState();
        probeWet.makeCopyOf(probeDry);
        processCabChunked(probeWet);
        const float gain = Loudness::peakAwareGain(dryProbeRms,
                                                   probeWet.getRMSLevel(0, 0, probeLength),
                                                   probeWet.getMagnitude(0, 0, probeLength),
                                                   Loudness::cabPeakTarget, 0.15f, 1.2f);
        resetCabState();
        return gain;
    }

    void buildCabLut()
    {
        if (maxBlock <= 0)
        {
            for (int i = 0; i < lutN; ++i)
                for (int j = 0; j < lutN; ++j)
                    cabLut[i][j] = 1.0f;
            return;
        }

        probing = true;
        for (int i = 0; i < lutN; ++i)
        {
            for (int j = 0; j < lutN; ++j)
            {
                applyRecipe((float) i / (float) (lutN - 1), (float) j / (float) (lutN - 1));
                cabLut[i][j] = measureCabCell();
            }
        }
        applyRecipe(cabSize.load(std::memory_order_relaxed),
                    cabBack.load(std::memory_order_relaxed));
        probing = false;
    }

    void measureHrtfMakeup()
    {
        if (maxBlock <= 0)
        {
            hrtfMakeup = 1.0f;
            return;
        }

        earL.reset();
        earR.reset();
        probeWet.makeCopyOf(probeDry);
        processConvChunked(earL, probeWet);
        const float wetL = probeWet.getRMSLevel(0, 0, probeLength);
        const float peakL = probeWet.getMagnitude(0, 0, probeLength);
        probeWet.makeCopyOf(probeDry);
        processConvChunked(earR, probeWet);
        const float wetR = probeWet.getRMSLevel(0, 0, probeLength);
        const float peakR = probeWet.getMagnitude(0, 0, probeLength);
        hrtfMakeup = Loudness::peakAwareGain(dryProbeRms,
                                             0.5f * (wetL + wetR),
                                             juce::jmax(peakL, peakR),
                                             Loudness::hrtfPeakTarget, 0.4f, 1.2f);
        earL.reset();
        earR.reset();
        work.clear();
        hrtfL.clear();
        hrtfR.clear();
    }

    std::atomic<float> cabSize { defaultSize };
    std::atomic<float> cabBack { defaultBack };
    std::atomic<bool> binaural { false };

    double sampleRate = 48000.0;
    int maxBlock = 512;
    bool irsLoaded = false;
    float lastSize = defaultSize;
    float lastBack = defaultBack;
    float cabMakeup = 1.0f;
    float hrtfMakeup = 1.0f;
    float dryProbeRms = 1.0f;
    bool probing = false;
    static constexpr int probeLength = 4096;
    static constexpr int lutN = 5;
    float cabLut[lutN][lutN] {};

    juce::dsp::Convolution cone, earL, earR;
    Filter sizeLow, sizeDark, sizeBeam, backLow, backLeak, floorTone, tailTone;
    Delay delay { 8192 };
    Delay tail { 32768 };
    juce::SmoothedValue<float> binauralMix;
    juce::SmoothedValue<float> cabMakeupSmooth;

    juce::AudioBuffer<float> work, dry, hrtfL, hrtfR, probeDry, probeWet;
};
