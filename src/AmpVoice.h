#pragma once

#include "Loudness.h"
#include "ToneCompose.h"

#include <juce_dsp/juce_dsp.h>
#include <cmath>

class AmpEngine
{
public:
    static constexpr int probeLength = 1536;
    static constexpr int probePreRoll = 256;

    float getMakeup() const { return lastMakeup; }

    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        prepareFilters(hpf, lowMid, mid, presence, fizz, cab, spec);

        juce::dsp::ProcessSpec probeSpec { spec.sampleRate, (juce::uint32) probeLength, 1 };
        prepareFilters(probeHpf, probeLowMid, probeMid, probePresence, probeFizz, probeCab, probeSpec);

        juce::dsp::ProcessSpec weightSpec { spec.sampleRate, (juce::uint32) probeLength, 1 };
        dryWeight.prepare(weightSpec);
        wetWeight.prepare(weightSpec);
        dryWeight.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, 200.0f, 0.707f);
        wetWeight.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, 200.0f, 0.707f);

        buildPinkProbe();
        sag = 0.0f;
        gateEnv = 0.0f;
        lastProbed = {};
        lastProbed.drive1 = -1.0f;
        smoothed = baseParams();
        smoothed.makeup = makeupFromParams(smoothed);
        lastMakeup = smoothed.makeup;
        updateCoeffs(hpf, lowMid, mid, presence, fizz, cab, smoothed);
    }

    void reset()
    {
        resetFilters(hpf, lowMid, mid, presence, fizz, cab);
        resetFilters(probeHpf, probeLowMid, probeMid, probePresence, probeFizz, probeCab);
        dryWeight.reset();
        wetWeight.reset();
        sag = 0.0f;
        gateEnv = 0.0f;
        lastProbed.drive1 = -1.0f;
        smoothed = baseParams();
        smoothed.makeup = makeupFromParams(smoothed);
        lastMakeup = smoothed.makeup;
    }

    void process(juce::AudioBuffer<float>& buffer, const AmpParams& target)
    {
        AmpParams aimed = target;

        if (paramsMoved(aimed, lastProbed))
        {
            lastProbed = aimed;
            AmpParams probed = aimed;
            probed.makeup = 1.0f;
            lastMakeup = measureProbe(probed);
        }

        aimed.makeup = lastMakeup;

        const float blockMs = 1000.0f * (float) buffer.getNumSamples()
                              / juce::jmax(1.0f, (float) sampleRate);
        const float coeff = 1.0f - std::exp(-blockMs / 12.0f);
        smoothed = lerpParams(smoothed, aimed, coeff);
        updateCoeffs(hpf, lowMid, mid, presence, fizz, cab, smoothed);

        juce::dsp::AudioBlock<float> block(buffer);
        juce::dsp::ProcessContextReplacing<float> context(block);
        hpf.process(context);
        lowMid.process(context);
        mid.process(context);

        auto* data = buffer.getWritePointer(0);
        clip(data, buffer.getNumSamples(), smoothed, true);

        presence.process(context);
        fizz.process(context);
        cab.process(context);
        buffer.applyGain(smoothed.makeup);
    }

private:
    using Filter = juce::dsp::IIR::Filter<float>;

    static void prepareFilters(Filter& a, Filter& b, Filter& c, Filter& d, Filter& e, Filter& f,
                               const juce::dsp::ProcessSpec& spec)
    {
        a.prepare(spec);
        b.prepare(spec);
        c.prepare(spec);
        d.prepare(spec);
        e.prepare(spec);
        f.prepare(spec);
    }

    static void resetFilters(Filter& a, Filter& b, Filter& c, Filter& d, Filter& e, Filter& f)
    {
        a.reset();
        b.reset();
        c.reset();
        d.reset();
        e.reset();
        f.reset();
    }

    void updateCoeffs(Filter& hp, Filter& lm, Filter& md, Filter& pr, Filter& fz, Filter& cb,
                      const AmpParams& p, double sr)
    {
        using Coeffs = juce::dsp::IIR::Coefficients<float>;
        hp.coefficients = Coeffs::makeHighPass(sr, p.hpfHz, 0.707f);
        lm.coefficients = Coeffs::makePeakFilter(sr, p.lowMidHz, p.lowMidQ,
                                                 juce::Decibels::decibelsToGain(p.lowMidDb));
        md.coefficients = Coeffs::makePeakFilter(sr, p.midHz, p.midQ,
                                                 juce::Decibels::decibelsToGain(p.midDb));
        pr.coefficients = Coeffs::makePeakFilter(sr, p.presHz, p.presQ,
                                                 juce::Decibels::decibelsToGain(p.presDb));
        fz.coefficients = Coeffs::makeHighShelf(sr, p.fizzHz, 0.7f,
                                                juce::Decibels::decibelsToGain(p.fizzDb));
        cb.coefficients = Coeffs::makeLowPass(sr, juce::jmax(2500.0f, p.cabHz), 0.707f);
    }

    void updateCoeffs(Filter& hp, Filter& lm, Filter& md, Filter& pr, Filter& fz, Filter& cb,
                      const AmpParams& p)
    {
        updateCoeffs(hp, lm, md, pr, fz, cb, p, sampleRate);
    }

    static bool paramsMoved(const AmpParams& a, const AmpParams& b)
    {
        return std::abs(a.drive1 - b.drive1) > 0.002f
            || std::abs(a.drive2 - b.drive2) > 0.002f
            || std::abs(a.stageMix - b.stageMix) > 0.002f
            || std::abs(a.hard - b.hard) > 0.002f
            || std::abs(a.sag - b.sag) > 0.002f
            || std::abs(a.hpfHz - b.hpfHz) > 0.2f
            || std::abs(a.lowMidDb - b.lowMidDb) > 0.05f
            || std::abs(a.midDb - b.midDb) > 0.05f
            || std::abs(a.presDb - b.presDb) > 0.05f
            || std::abs(a.fizzDb - b.fizzDb) > 0.05f
            || std::abs(a.cabHz - b.cabHz) > 2.0f;
    }

    void buildPinkProbe()
    {
        probeDry.setSize(1, probeLength);
        probeWet.setSize(1, probeLength);
        auto* dest = probeDry.getWritePointer(0);
        juce::Random rng { 0xC057E11 };
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

        probeWet.makeCopyOf(probeDry);
        dryWeight.reset();
        juce::dsp::AudioBlock<float> block(probeWet);
        juce::dsp::ProcessContextReplacing<float> context(block);
        dryWeight.process(context);
        dryProbeRms = juce::jmax(probeWet.getRMSLevel(0, probePreRoll, probeLength - probePreRoll), 1.0e-6f);
        probeWet.makeCopyOf(probeDry);
        dryWeight.reset();
    }

    float measureProbe(const AmpParams& p)
    {
        probeWet.makeCopyOf(probeDry);
        resetFilters(probeHpf, probeLowMid, probeMid, probePresence, probeFizz, probeCab);
        updateCoeffs(probeHpf, probeLowMid, probeMid, probePresence, probeFizz, probeCab, p);

        juce::dsp::AudioBlock<float> block(probeWet);
        juce::dsp::ProcessContextReplacing<float> context(block);
        probeHpf.process(context);
        probeLowMid.process(context);
        probeMid.process(context);
        clip(probeWet.getWritePointer(0), probeLength, p, false);
        probePresence.process(context);
        probeFizz.process(context);
        probeCab.process(context);

        const int count = probeLength - probePreRoll;
        const float wetPeak = juce::jmax(probeWet.getMagnitude(0, probePreRoll, count), 1.0e-6f);

        wetWeight.reset();
        wetWeight.process(context);
        const float wetRms = juce::jmax(probeWet.getRMSLevel(0, probePreRoll, count), 1.0e-6f);
        return Loudness::peakAwareGain(dryProbeRms, wetRms, wetPeak,
                                       Loudness::starPeakTarget, 0.12f, 2.4f);
    }

    void clip(float* data, int n, const AmpParams& p, bool live)
    {
        const float stage2 = juce::jlimit(0.0f, 1.0f, p.stageMix - 1.0f);
        const float stage3 = juce::jlimit(0.0f, 1.0f, p.stageMix - 2.0f);
        const float gateThresh = live ? p.gate * 0.018f : 0.0f;

        for (int i = 0; i < n; ++i)
        {
            float x = data[i];

            if (p.sag > 0.0f)
            {
                if (live)
                {
                    sag += (std::abs(x) - sag) * 0.0045f;
                    x /= 1.0f + sag * p.sag;
                }
                else
                {
                    x /= 1.0f + 0.35f * p.sag;
                }
            }

            x = std::tanh(x * p.drive1);

            if (stage2 > 0.0f)
            {
                const float s2 = std::tanh(x * p.drive2);
                x += (s2 - x) * stage2;
            }

            if (stage3 > 0.0f)
            {
                const float driven = x * p.drive3;
                const float soft = std::tanh(driven);
                const float clipped = juce::jlimit(-0.92f, 0.92f, driven);
                const float s3 = soft + (clipped - soft) * p.hard;
                x += (s3 - x) * stage3;
            }

            if (gateThresh > 1.0e-5f)
            {
                gateEnv += (std::abs(x) - gateEnv) * (std::abs(x) > gateEnv ? 0.35f : 0.04f);
                if (gateEnv < gateThresh)
                    x *= gateEnv / gateThresh;
            }

            data[i] = x;
        }
    }

    double sampleRate = 44100.0;
    Filter hpf, lowMid, mid, presence, fizz, cab;
    Filter probeHpf, probeLowMid, probeMid, probePresence, probeFizz, probeCab;
    Filter dryWeight, wetWeight;
    juce::AudioBuffer<float> probeDry, probeWet;
    float sag = 0.0f;
    float gateEnv = 0.0f;
    float lastMakeup = 1.0f;
    float dryProbeRms = 1.0f;
    AmpParams smoothed, lastProbed;
};
