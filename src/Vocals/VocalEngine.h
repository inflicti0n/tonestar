#pragma once

#include "Vocals/VocalCompose.h"

#include <juce_dsp/juce_dsp.h>
#include <cmath>

class VocalEngine
{
public:
    void prepare(double sr, int block)
    {
        spec.sampleRate = sr > 0.0 ? sr : 48000.0;
        spec.maximumBlockSize = (juce::uint32) juce::jmax(32, block);
        spec.numChannels = 1;
        hpf.prepare(spec);
        lowMid.prepare(spec);
        presence.prepare(spec);
        air.prepare(spec);
        hpf.reset();
        lowMid.reset();
        presence.reset();
        air.reset();
        env = 0.0f;
        ess = 0.0f;
        gateOpen = 0.0f;
    }

    void reset()
    {
        hpf.reset();
        lowMid.reset();
        presence.reset();
        air.reset();
        env = 0.0f;
        ess = 0.0f;
        gateOpen = 0.0f;
    }

    void process(juce::AudioBuffer<float>& mono, const VocalParams& p)
    {
        if (mono.getNumSamples() <= 0)
            return;

        auto* x = mono.getWritePointer(0);
        const int n = mono.getNumSamples();
        const float sr = (float) spec.sampleRate;

        hpf.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass(spec.sampleRate,
                                                                            juce::jlimit(40.0f, 240.0f, p.hpfHz), 0.7f);
        lowMid.coefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter(spec.sampleRate, 220.0f, 0.85f,
                                                                                 juce::Decibels::decibelsToGain(p.lowMidDb));
        presence.coefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter(spec.sampleRate, 3200.0f, 0.8f,
                                                                                   juce::Decibels::decibelsToGain(p.presenceDb + p.cutDb * 0.35f));
        air.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighShelf(spec.sampleRate, 8000.0f, 0.7f,
                                                                             juce::Decibels::decibelsToGain(p.airDb));

        juce::dsp::AudioBlock<float> block(mono);
        juce::dsp::ProcessContextReplacing<float> ctx(block);
        hpf.process(ctx);
        lowMid.process(ctx);
        presence.process(ctx);
        air.process(ctx);

        const float atk = 1.0f - std::exp(-1.0f / (0.012f * sr));
        const float rel = 1.0f - std::exp(-1.0f / (0.080f * sr));
        const float gateAtk = 1.0f - std::exp(-1.0f / (0.008f * sr));
        const float gateRel = 1.0f - std::exp(-1.0f / (0.060f * sr));
        const float thresh = 0.02f + (1.0f - p.leveler) * 0.08f;
        const float ratio = 1.6f + p.crush * 6.0f;
        const float sat = p.sat;
        const float deessAmt = p.deess;
        const float gateThr = 0.004f + p.gate * 0.03f;

        for (int i = 0; i < n; ++i)
        {
            float s = x[i];
            const float abs = std::abs(s);
            env += ((abs > env) ? atk : rel) * (abs - env);

            const float want = abs > gateThr ? 1.0f : 0.0f;
            gateOpen += ((want > gateOpen) ? gateAtk : gateRel) * (want - gateOpen);
            s *= 0.12f + 0.88f * gateOpen;

            if (env > thresh)
            {
                const float over = env / juce::jmax(thresh, 1.0e-5f);
                const float gr = std::pow(over, 1.0f / ratio - 1.0f);
                s *= juce::jlimit(0.15f, 1.0f, gr);
            }

            const float hi = s - last;
            last = s;
            ess += 0.15f * (std::abs(hi) - ess);
            if (ess > 0.04f)
                s -= hi * juce::jlimit(0.0f, 0.85f, (ess - 0.04f) * 8.0f * deessAmt);

            if (sat > 0.001f)
                s = std::tanh(s * (1.0f + sat * 3.4f)) / (1.0f + sat);

            x[i] = s * (1.0f + p.leveler * 0.35f);
        }
    }

private:
    juce::dsp::ProcessSpec spec { 48000.0, 128, 1 };
    juce::dsp::IIR::Filter<float> hpf, lowMid, presence, air;
    float env = 0.0f;
    float ess = 0.0f;
    float last = 0.0f;
    float gateOpen = 0.0f;
};
