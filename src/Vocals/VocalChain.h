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

    void process(juce::AudioBuffer<float>& mono, const VocalStamp& stamp, float bpm)
    {
        engine.process(mono, composeVocal(stamp.axes));
        fx.process(mono, stamp.fx, stamp.root, stamp.minor, bpm);
    }

private:
    VocalEngine engine;
    VocalFx fx;
};
