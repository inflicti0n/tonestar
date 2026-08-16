#include "App/GuitarProcessor.h"
#include "App/AppLog.h"
#include <cmath>

GuitarProcessor::GuitarProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::discreteChannels(16), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    for (int i = 0; i < numAxes; ++i)
        axes[(size_t) i].store(0.0f, std::memory_order_relaxed);
    for (int i = 0; i < 5; ++i)
        vocalAxes[(size_t) i].store(0.0f, std::memory_order_relaxed);
    for (int i = 0; i < 6; ++i)
        vocalFx[(size_t) i].store(0.0f, std::memory_order_relaxed);
    tape.setThroughRender([this] (const VocalStamp& stamp, juce::AudioBuffer<float>& mono, float bpm)
    {
        renderVocal(mono, stamp, bpm);
    });
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

void GuitarProcessor::setRigMode(RigMode next)
{
    rigMode.store((int) next, std::memory_order_relaxed);
    tape.setRecordThroughChain(next == RigMode::Vocals);
}

void GuitarProcessor::setVocalStamp(const VocalStamp& stamp)
{
    for (int i = 0; i < 5; ++i)
        vocalAxes[(size_t) i].store(juce::jlimit(0.0f, 1.0f, stamp.axes[(size_t) i]),
                                    std::memory_order_relaxed);
    for (int i = 0; i < 6; ++i)
        vocalFx[(size_t) i].store(juce::jlimit(0.0f, 1.0f, stamp.fx[(size_t) i]),
                                  std::memory_order_relaxed);
    vocalRoot.store(((stamp.root % 12) + 12) % 12, std::memory_order_relaxed);
    vocalMinor.store(stamp.minor ? 1 : 0, std::memory_order_relaxed);
    tape.setLiveVocalStamp(stamp);
}

void GuitarProcessor::setListenLane(int lane)
{
    listenLane.store(lane, std::memory_order_relaxed);
}

VocalStamp GuitarProcessor::getVocalStamp() const
{
    VocalStamp stamp;
    for (int i = 0; i < 5; ++i)
        stamp.axes[(size_t) i] = vocalAxes[(size_t) i].load(std::memory_order_relaxed);
    for (int i = 0; i < 6; ++i)
        stamp.fx[(size_t) i] = vocalFx[(size_t) i].load(std::memory_order_relaxed);
    stamp.root = vocalRoot.load(std::memory_order_relaxed);
    stamp.minor = vocalMinor.load(std::memory_order_relaxed) != 0;
    return stamp;
}

void GuitarProcessor::renderVocal(juce::AudioBuffer<float>& mono, const VocalStamp& stamp, float bpm)
{
    exportVocal.process(mono, stamp, bpm);
}

FieldEnergy GuitarProcessor::getFieldEnergy() const
{
    return {
        fieldEnergyOut.load(std::memory_order_relaxed),
        fieldPunchOut.load(std::memory_order_relaxed),
        fieldBreathOut.load(std::memory_order_relaxed)
    };
}

FieldSpectrum GuitarProcessor::getFieldSpectrum() const
{
    return spectrum.snapshot();
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
    liveVocal.prepare(sampleRate, samplesPerBlock);
    exportVocal.prepare(sampleRate, samplesPerBlock);
    for (auto& voice : laneVocals)
        voice.prepare(sampleRate, samplesPerBlock);
    tape.prepare(sampleRate, samplesPerBlock);
    tape.setThroughRender([this] (const VocalStamp& stamp, juce::AudioBuffer<float>& mono, float bpm)
    {
        renderVocal(mono, stamp, bpm);
    });
    looper.prepare(sampleRate, samplesPerBlock);
    tuner.prepare(sampleRate);
    AppLog::note("processor prepare " + juce::String(sampleRate, 0)
                 + " / " + juce::String(samplesPerBlock));
    spectrum.prepare(sampleRate);

    fieldEnergyEnv = 0.0f;
    fieldPunchEnv = 0.0f;
    fieldBreathEnv = 0.08f;
}

void GuitarProcessor::releaseResources()
{
    monoBuffer.setSize(0, 0);
    stereoBuffer.setSize(0, 0);
    tape.halt();
    engine.reset();
    fx.reset();
    acoustics.reset();
    liveVocal.reset();
    exportVocal.reset();
    for (auto& voice : laneVocals)
        voice.reset();
    looper.reset();
    spectrum.reset();
    tuner.reset();
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
    if (dryTap.getNumSamples() < numSamples)
        dryTap.setSize(1, numSamples, false, false, true);
    if (vocalScratch.getNumChannels() != 1 || vocalScratch.getNumSamples() != numSamples)
        vocalScratch.setSize(1, numSamples, false, false, true);

    const int inCh = juce::jlimit(0, numIns - 1, inputChannel.load(std::memory_order_relaxed));
    const float inGain = juce::Decibels::decibelsToGain(inputGainDb.load(std::memory_order_relaxed));
    const float outGain = juce::Decibels::decibelsToGain(outputGainDb.load(std::memory_order_relaxed));
    const bool silent = muted.load(std::memory_order_relaxed);
    const bool vocals = (RigMode) rigMode.load(std::memory_order_relaxed) == RigMode::Vocals;

    auto* log = debugLog.load(std::memory_order_acquire);
    float peaks[DebugLog::numStages] {};
    float rms[DebugLog::numStages] {};

    dryTap.copyFrom(0, 0, buffer, inCh, 0, numSamples);
    tuner.push(dryTap.getReadPointer(0), numSamples);
    monoBuffer.makeCopyOf(dryTap);
    monoBuffer.applyGain(inGain);
    if (log != nullptr)
    {
        peaks[0] = monoBuffer.getMagnitude(0, 0, numSamples);
        rms[0] = monoBuffer.getRMSLevel(0, 0, numSamples);
    }

    const double bpm = (double) juce::jlimit(40.0f, 240.0f, metroBpm.load(std::memory_order_relaxed));

    if (vocals)
    {
        liveVocal.process(monoBuffer, getVocalStamp(), (float) bpm);
        if (log != nullptr)
        {
            peaks[1] = monoBuffer.getMagnitude(0, 0, numSamples);
            rms[1] = monoBuffer.getRMSLevel(0, 0, numSamples);
            peaks[2] = peaks[1];
            rms[2] = rms[1];
            peaks[3] = peaks[1];
            rms[3] = rms[1];
        }

        stereoBuffer.copyFrom(0, 0, monoBuffer, 0, 0, numSamples);
        stereoBuffer.copyFrom(1, 0, monoBuffer, 0, 0, numSamples);
    }
    else
    {
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
    }

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

    const double samplesPerBeat = currentSampleRate > 0.0 ? currentSampleRate * 60.0 / bpm : 0.0;
    looper.setBpm((float) bpm);
    tape.setRecordThroughChain(vocals);
    tape.setLiveVocalStamp(getVocalStamp());
    tape.process(stereoBuffer, numSamples, metroSample, samplesPerBeat, dryTap.getReadPointer(0));

    const int listen = listenLane.load(std::memory_order_relaxed);
    const VocalStamp live = getVocalStamp();
    for (int i = 0; i < TapeEngine::numLanes; ++i)
    {
        const float* dry = tape.getThroughDry(i);
        if (dry == nullptr)
            continue;

        const bool followLive = vocals && (listen < 0 || listen == i);
        const VocalStamp stamp = followLive ? live : tape.getVocalStamp(i);
        vocalScratch.copyFrom(0, 0, dry, numSamples);
        vocalScratch.applyGain(inGain);
        laneVocals[(size_t) i].process(vocalScratch, stamp, (float) bpm);
        vocalScratch.applyGain(outGain);

        const float gain = tape.getLevel(i);
        const float angle = (tape.getPan(i) + 1.0f) * (juce::MathConstants<float>::halfPi * 0.5f);
        const float gL = gain * std::cos(angle);
        const float gR = gain * std::sin(angle);
        stereoBuffer.addFrom(0, 0, vocalScratch, 0, 0, numSamples, gL);
        stereoBuffer.addFrom(1, 0, vocalScratch, 0, 0, numSamples, gR);
    }

    looper.process(stereoBuffer, numSamples, metroSample, samplesPerBeat);

    {
        const float mixPeak = juce::jmax(stereoBuffer.getMagnitude(0, 0, numSamples),
                                         stereoBuffer.getMagnitude(1, 0, numSamples));
        const float mixRms = 0.5f * (stereoBuffer.getRMSLevel(0, 0, numSamples)
                                     + stereoBuffer.getRMSLevel(1, 0, numSamples));
        const float dt = currentSampleRate > 0.0
                             ? (float) numSamples / (float) currentSampleRate
                             : 0.005f;
        const float energyCoeff = 1.0f - std::exp(-dt / 0.080f);
        const float breathCoeff = 1.0f - std::exp(-dt / 1.40f);
        const float punchFall = 1.0f - std::exp(-dt / 0.165f);

        fieldEnergyEnv += (mixRms - fieldEnergyEnv) * energyCoeff;
        fieldBreathEnv += (mixRms - fieldBreathEnv) * breathCoeff;
        const float onset = juce::jmax(0.0f, mixPeak - fieldEnergyEnv * 2.2f);
        fieldPunchEnv = juce::jmax(onset, fieldPunchEnv * (1.0f - punchFall));

        const float energyNorm = juce::jlimit(0.0f, 1.0f, fieldEnergyEnv * 8.0f);
        const float breathNorm = juce::jlimit(0.0f, 1.0f, fieldBreathEnv * 8.0f);
        fieldEnergyOut.store(juce::jlimit(0.0f, 1.0f, std::pow(energyNorm, 0.55f)),
                             std::memory_order_relaxed);
        fieldPunchOut.store(juce::jlimit(0.0f, 1.0f, fieldPunchEnv * 2.4f),
                            std::memory_order_relaxed);
        fieldBreathOut.store(juce::jlimit(0.0f, 1.0f, std::pow(breathNorm, 0.65f)),
                             std::memory_order_relaxed);

        spectrum.pushStereo(stereoBuffer.getReadPointer(0),
                            stereoBuffer.getNumChannels() > 1 ? stereoBuffer.getReadPointer(1) : nullptr,
                            numSamples);
    }

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
