#include "GuitarProcessor.h"
#include <cmath>

GuitarProcessor::GuitarProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::discreteChannels(16), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    for (int i = 0; i < numAxes; ++i)
        axes[(size_t) i].store(0.0f, std::memory_order_relaxed);
}

GuitarProcessor::~GuitarProcessor()
{
    tape.shutdown();
}

bool GuitarProcessor::startRecording()
{
    tape.record();
    return true;
}

void GuitarProcessor::stopRecording()
{
    tape.stopRecord();
}

void GuitarProcessor::setAxis(int index, float value)
{
    if (juce::isPositiveAndBelow(index, numAxes))
        axes[(size_t) index].store(juce::jlimit(0.0f, 1.0f, value), std::memory_order_relaxed);
}

void GuitarProcessor::setAxes(const std::array<float, 6>& values)
{
    for (int i = 0; i < numAxes; ++i)
        setAxis(i, values[(size_t) i]);
}

float GuitarProcessor::getAxis(int index) const
{
    if (! juce::isPositiveAndBelow(index, numAxes))
        return 0.0f;

    return axes[(size_t) index].load(std::memory_order_relaxed);
}

std::array<float, 6> GuitarProcessor::getAxes() const
{
    std::array<float, 6> values {};
    for (int i = 0; i < numAxes; ++i)
        values[(size_t) i] = getAxis(i);
    return values;
}

void GuitarProcessor::setFx(int index, float value)
{
    fx.setAmount(index, value);
}

void GuitarProcessor::setFxAmounts(const std::array<float, 8>& values)
{
    fx.setAmounts(values);
}

void GuitarProcessor::setBloomShimmer(bool shouldShimmer)
{
    fx.setBloomShimmer(shouldShimmer);
}

float GuitarProcessor::getFx(int index) const
{
    return fx.getAmount(index);
}

std::array<float, 8> GuitarProcessor::getFxAmounts() const
{
    return fx.getAmounts();
}

bool GuitarProcessor::getBloomShimmer() const
{
    return fx.getBloomShimmer();
}

void GuitarProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    monoBuffer.setSize(1, samplesPerBlock);
    stereoBuffer.setSize(2, samplesPerBlock);

    currentSampleRate = sampleRate;
    clickLength = juce::jmax(32, (int) std::round(sampleRate * 0.008));
    metroSample = 0.0;
    clickSample = clickLength;
    beatIndex = 3;

    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, 1 };
    engine.prepare(spec);
    fx.prepare(spec);
    acoustics.prepare(spec);
    tape.prepare(sampleRate, samplesPerBlock);
    looper.prepare(sampleRate, samplesPerBlock);
}

void GuitarProcessor::releaseResources()
{
    monoBuffer.setSize(0, 0);
    stereoBuffer.setSize(0, 0);
    tape.halt();
    engine.reset();
    fx.reset();
    acoustics.reset();
    looper.reset();
}

bool GuitarProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    const auto& in = layouts.getMainInputChannelSet();

    if (in.isDisabled() || in.size() < 1)
        return false;

    return out == juce::AudioChannelSet::mono()
        || out == juce::AudioChannelSet::stereo();
}

DebugLog::Snapshot GuitarProcessor::captureDebugSnapshot() const
{
    DebugLog::Snapshot snap;
    snap.axes = getAxes();
    snap.fx = getFxAmounts();
    snap.inDb = getInputGainDb();
    snap.outDb = getOutputGainDb();
    snap.cabSize = getCabSize();
    snap.cabBack = getCabBack();
    snap.cabMakeup = getCabMakeup();
    snap.hrtfMakeup = getHrtfMakeup();
    snap.starMakeup = getStarMakeup();
    snap.bloomWetScale = getBloomWetScale();
    snap.echoWetScale = getEchoWetScale();
    snap.shimmer = getBloomShimmer();
    snap.binaural = getBinaural();
    snap.muted = isMuted();
    return snap;
}

void GuitarProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numIns = buffer.getNumChannels();
    const int numOuts = getTotalNumOutputChannels();

    if (numSamples == 0 || numIns == 0)
        return;

    if (monoBuffer.getNumSamples() < numSamples)
        monoBuffer.setSize(1, numSamples, false, false, true);
    if (stereoBuffer.getNumSamples() < numSamples)
        stereoBuffer.setSize(2, numSamples, false, false, true);

    const int inCh = juce::jlimit(0, numIns - 1, inputChannel.load(std::memory_order_relaxed));
    const float inGain = juce::Decibels::decibelsToGain(inputGainDb.load(std::memory_order_relaxed));
    const float outGain = juce::Decibels::decibelsToGain(outputGainDb.load(std::memory_order_relaxed));
    const bool silent = muted.load(std::memory_order_relaxed);

    auto* log = debugLog.load(std::memory_order_acquire);
    float peaks[DebugLog::numStages] {};
    float rms[DebugLog::numStages] {};

    monoBuffer.copyFrom(0, 0, buffer, inCh, 0, numSamples);
    monoBuffer.applyGain(inGain);
    if (log != nullptr)
    {
        peaks[0] = monoBuffer.getMagnitude(0, 0, numSamples);
        rms[0] = monoBuffer.getRMSLevel(0, 0, numSamples);
    }

    fx.processPre(monoBuffer);
    if (log != nullptr)
    {
        peaks[1] = monoBuffer.getMagnitude(0, 0, numSamples);
        rms[1] = monoBuffer.getRMSLevel(0, 0, numSamples);
    }

    engine.process(monoBuffer, composeParams(getAxes()));
    if (log != nullptr)
    {
        peaks[2] = monoBuffer.getMagnitude(0, 0, numSamples);
        rms[2] = monoBuffer.getRMSLevel(0, 0, numSamples);
    }

    fx.processPost(monoBuffer);
    if (log != nullptr)
    {
        peaks[3] = monoBuffer.getMagnitude(0, 0, numSamples);
        rms[3] = monoBuffer.getRMSLevel(0, 0, numSamples);
    }

    acoustics.process(monoBuffer, stereoBuffer);

    const float cabPeak = juce::jmax(stereoBuffer.getMagnitude(0, 0, numSamples),
                                     stereoBuffer.getMagnitude(1, 0, numSamples));
    const float previous = peak.load(std::memory_order_relaxed);
    if (cabPeak > previous)
        peak.store(cabPeak, std::memory_order_relaxed);

    if (log != nullptr)
    {
        peaks[4] = cabPeak;
        rms[4] = 0.5f * (stereoBuffer.getRMSLevel(0, 0, numSamples)
                         + stereoBuffer.getRMSLevel(1, 0, numSamples));
    }

    stereoBuffer.applyGain(outGain);

    const double bpm = (double) juce::jlimit(40.0f, 240.0f, metroBpm.load(std::memory_order_relaxed));
    const double samplesPerBeat = currentSampleRate > 0.0 ? currentSampleRate * 60.0 / bpm : 0.0;
    looper.setBpm((float) bpm);
    tape.process(stereoBuffer, numSamples, metroSample, samplesPerBeat);
    looper.process(stereoBuffer, numSamples, metroSample, samplesPerBeat);

    if (numOuts == 1)
    {
        buffer.copyFrom(0, 0, stereoBuffer, 0, 0, numSamples);
        buffer.addFrom(0, 0, stereoBuffer, 1, 0, numSamples);
        buffer.applyGain(0, 0, numSamples, 0.5f);
    }
    else
    {
        for (int ch = 0; ch < numOuts; ++ch)
            buffer.copyFrom(ch, 0, stereoBuffer, juce::jmin(ch, 1), 0, numSamples);
    }

    if (silent)
    {
        for (int ch = 0; ch < numOuts; ++ch)
            buffer.clear(ch, 0, numSamples);
    }

    if (currentSampleRate > 0.0 && samplesPerBeat > 0.0)
    {
        const bool click = metroOn.load(std::memory_order_relaxed);
        const float twoPi = juce::MathConstants<float>::twoPi;
        const int clickN = juce::jmax(32, clickLength);

        for (int i = 0; i < numSamples; ++i)
        {
            if (metroSample >= samplesPerBeat)
            {
                metroSample -= samplesPerBeat;
                clickSample = 0;
                beatIndex = (beatIndex + 1) & 3;
                metroPulse.store(1.0f, std::memory_order_relaxed);
            }

            if (click && clickSample < clickN)
            {
                const float env = 1.0f - (float) clickSample / (float) clickN;
                const float t = (float) ((double) clickSample / currentSampleRate);
                float tick = std::sin(twoPi * 1000.0f * t) * 0.34f * env * env;
                if (beatIndex == 0)
                    tick += std::sin(twoPi * 200.0f * t) * 0.22f * env * env;

                for (int ch = 0; ch < numOuts; ++ch)
                    buffer.addSample(ch, i, tick);
                ++clickSample;
            }
            else if (clickSample < clickN)
            {
                ++clickSample;
            }

            metroSample += 1.0;
        }
    }

    float outPeak = 0.0f;
    float outRms = 0.0f;
    for (int ch = 0; ch < numOuts; ++ch)
    {
        outPeak = juce::jmax(outPeak, buffer.getMagnitude(ch, 0, numSamples));
        outRms += buffer.getRMSLevel(ch, 0, numSamples);
    }
    if (numOuts > 0)
        outRms /= (float) numOuts;
    if (outPeak >= 0.999f)
        clipped.store(true, std::memory_order_relaxed);

    if (log != nullptr)
    {
        peaks[5] = outPeak;
        rms[5] = outRms;
        log->noteBlock(peaks, rms, numSamples, captureDebugSnapshot());
    }

    for (int ch = numOuts; ch < numIns; ++ch)
        buffer.clear(ch, 0, numSamples);
}
