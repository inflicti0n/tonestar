#pragma once

#include "Vocals/VocalEngine.h"
#include "Vocals/VocalFx.h"

class VocalChain
{
public:
    void prepare(double sr, int block)
    {
        engine.prepare(sr, block);
        fx.prepare(sr, block);
    }

    void reset()
    {
        engine.reset();
        fx.reset();
    }

    void process(juce::AudioBuffer<float>& stereo, const VocalStamp& stamp, float bpm)
    {
        const int n = stereo.getNumSamples();
        if (n <= 0)
            return;

        if (stereo.getNumChannels() < 2)
            stereo.setSize(2, n, true, false, true);

        juce::AudioBuffer<float> mono (stereo.getArrayOfWritePointers(), 1, n);
        engine.process(mono, composeVocal(stamp.axes));
        fx.process(stereo, stamp.fx, stamp.root, stamp.minor, bpm);
    }

    VocalDebug takeDebug() { return fx.takeDebug(); }

private:
    VocalEngine engine;
    VocalFx fx;
};
