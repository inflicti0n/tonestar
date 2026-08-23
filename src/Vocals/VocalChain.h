#pragma once

#include "VocalEngine.h"
#include "VocalFx.h"
#include "VocalShift.h"

class VocalChain
{
public:
    void prepare(double sr, int block)
    {
        engine.prepare(sr, block);
        shift.prepare(sr, block);
        fx.prepare(sr, block);
    }

    void reset()
    {
        engine.reset();
        shift.reset();
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
        shift.process(mono.getWritePointer(0), n, stamp.pitch, stamp.formant, stamp.shiftMode);
        fx.process(stereo, stamp.fx, stamp.root, stamp.minor, bpm);
    }

    VocalDebug takeDebug() { return fx.takeDebug(); }

private:
    VocalEngine engine;
    VocalShift shift;
    VocalFx fx;
};
