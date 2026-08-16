#include "Tape/TapeEngine.h"
#include "Presets/ToneSlug.h"
#include <algorithm>
#include <cmath>

TapeEngine::TapeEngine()
{
    for (int i = 0; i < numLanes; ++i)
    {
        lanes[i].name = "Track " + juce::String(i + 1);
        applyStamp(lanes[i], {});
    }
    for (int i = 0; i < 5; ++i)
        liveAxes[(size_t) i].store(0.0f, std::memory_order_relaxed);
    for (int i = 0; i < 6; ++i)
        liveFx[(size_t) i].store(0.0f, std::memory_order_relaxed);
}

TapeEngine::~TapeEngine()
{
    shutdown();
}

void TapeEngine::halt()
{
    if (transport == Transport::Recording)
        finishRecord();
    transport = Transport::Stopped;
    playhead = 0;
    closeOnBeat = false;
    transportPub.store((int) Transport::Stopped, std::memory_order_relaxed);
    playheadPub.store(0, std::memory_order_relaxed);
}

void TapeEngine::shutdown()
{
    halt();
    saveSession();
}

void TapeEngine::prepare(double sampleRateToUse, int samplesPerBlock)
{
    sampleRate = sampleRateToUse > 0.0 ? sampleRateToUse : 48000.0;
    maxSamples = juce::jmax(1, (int) std::round(sampleRate * maxSeconds));
    minSamples = juce::jmax(1, (int) std::round(sampleRate * 0.05));
    const int block = juce::jmax(samplesPerBlock, 512);
    guitar.setSize(2, block, false, false, true);
    for (int i = 0; i < numLanes; ++i)
        throughDry[i].setSize(1, block, false, false, true);
}

void TapeEngine::reset()
{
    stop();
}

void TapeEngine::post(Cmd cmd)
{
    command.store((int) cmd, std::memory_order_release);
}

void TapeEngine::play()
{
    const int head = getPlayhead();
    bool inside = false;
    for (int i = 0; i < numLanes; ++i)
    {
        if (lanes[i].hasClip.load(std::memory_order_relaxed) == 0
            || lanes[i].mute.load(std::memory_order_relaxed) != 0)
            continue;
        const int start = lanes[i].start.load(std::memory_order_relaxed);
        const int in = lanes[i].in.load(std::memory_order_relaxed);
        const int end = lanes[i].end.load(std::memory_order_relaxed);
        const int idx = head - start;
        if (idx >= 0 && idx < end - in)
            inside = true;
    }
    if (! inside)
        setPlayhead(0);
    post(Cmd::Play);
}
void TapeEngine::pause() { post(Cmd::Pause); }
void TapeEngine::stop() { post(Cmd::Stop); }

void TapeEngine::record()
{
    const int lane = getArmedLane();
    ensureLane(lane);
    if (recordThrough.load(std::memory_order_relaxed) != 0 && valid(lane))
    {
        lanes[lane].throughChain.store(1, std::memory_order_relaxed);
        applyStamp(lanes[lane], readLiveStamp());
        lanes[lane].vocalSlug = ToneSlug::encodeVocal(readLiveStamp());
        metaDirty.store(true, std::memory_order_relaxed);
    }
    post(Cmd::Rec);
}

void TapeEngine::stopRecord() { post(Cmd::RecStop); }

void TapeEngine::setPlayhead(int sample)
{
    seekSample.store(juce::jmax(0, sample), std::memory_order_release);
    hasSeek.store(true, std::memory_order_release);
}

void TapeEngine::setTimelineView(float px, int start)
{
    pixelsPerBeat = juce::jlimit(8.0f, 240.0f, px);
    viewStart = juce::jmax(0, start);
    metaDirty.store(true, std::memory_order_relaxed);
}

int TapeEngine::beatOrDefault() const
{
    const int beat = beatSamples.load(std::memory_order_relaxed);
    if (beat > 0)
        return beat;
    return juce::jmax(1, (int) std::lround(sampleRate * 0.5));
}

void TapeEngine::setLoop(bool shouldLoop)
{
    if (shouldLoop && loopEnd.load(std::memory_order_relaxed) <= loopStart.load(std::memory_order_relaxed))
    {
        const int beat = beatOrDefault();
        const int start = snapSample(viewStart);
        loopStart.store(start, std::memory_order_relaxed);
        loopEnd.store(start + beat * 4, std::memory_order_relaxed);
    }
    loopOn.store(shouldLoop, std::memory_order_relaxed);
    metaDirty.store(true, std::memory_order_relaxed);
}

void TapeEngine::setLoopRange(int startSample, int endSample)
{
    const int beat = beatOrDefault();
    const int start = juce::jmax(0, startSample);
    const int end = juce::jmax(start + beat, endSample);
    loopStart.store(start, std::memory_order_relaxed);
    loopEnd.store(end, std::memory_order_relaxed);
    metaDirty.store(true, std::memory_order_relaxed);
}

void TapeEngine::setArmedLane(int lane)
{
    if (valid(lane))
        armedLane.store(lane, std::memory_order_relaxed);
}

void TapeEngine::setRecordThroughChain(bool should)
{
    recordThrough.store(should ? 1 : 0, std::memory_order_relaxed);
}

void TapeEngine::applyStamp(Lane& lane, const VocalStamp& stamp)
{
    for (int i = 0; i < 5; ++i)
        lane.vAxes[(size_t) i].store(juce::jlimit(0.0f, 1.0f, stamp.axes[(size_t) i]),
                                     std::memory_order_relaxed);
    for (int i = 0; i < 6; ++i)
        lane.vFx[(size_t) i].store(juce::jlimit(0.0f, 1.0f, stamp.fx[(size_t) i]),
                                   std::memory_order_relaxed);
    lane.vRoot.store(((stamp.root % 12) + 12) % 12, std::memory_order_relaxed);
    lane.vMinor.store(stamp.minor ? 1 : 0, std::memory_order_relaxed);
}

VocalStamp TapeEngine::readStamp(const Lane& lane) const
{
    VocalStamp stamp;
    for (int i = 0; i < 5; ++i)
        stamp.axes[(size_t) i] = lane.vAxes[(size_t) i].load(std::memory_order_relaxed);
    for (int i = 0; i < 6; ++i)
        stamp.fx[(size_t) i] = lane.vFx[(size_t) i].load(std::memory_order_relaxed);
    stamp.root = lane.vRoot.load(std::memory_order_relaxed);
    stamp.minor = lane.vMinor.load(std::memory_order_relaxed) != 0;
    return stamp;
}

VocalStamp TapeEngine::readLiveStamp() const
{
    VocalStamp stamp;
    for (int i = 0; i < 5; ++i)
        stamp.axes[(size_t) i] = liveAxes[(size_t) i].load(std::memory_order_relaxed);
    for (int i = 0; i < 6; ++i)
        stamp.fx[(size_t) i] = liveFx[(size_t) i].load(std::memory_order_relaxed);
    stamp.root = liveRoot.load(std::memory_order_relaxed);
    stamp.minor = liveMinor.load(std::memory_order_relaxed) != 0;
    return stamp;
}

void TapeEngine::setLiveVocalStamp(const VocalStamp& stamp)
{
    for (int i = 0; i < 5; ++i)
        liveAxes[(size_t) i].store(juce::jlimit(0.0f, 1.0f, stamp.axes[(size_t) i]),
                                   std::memory_order_relaxed);
    for (int i = 0; i < 6; ++i)
        liveFx[(size_t) i].store(juce::jlimit(0.0f, 1.0f, stamp.fx[(size_t) i]),
                                 std::memory_order_relaxed);
    liveRoot.store(((stamp.root % 12) + 12) % 12, std::memory_order_relaxed);
    liveMinor.store(stamp.minor ? 1 : 0, std::memory_order_relaxed);
}

void TapeEngine::setVocalSlug(int lane, const juce::String& slug)
{
    if (! valid(lane))
        return;

    VocalStamp stamp;
    if (! ToneSlug::decodeVocal(slug, stamp))
        return;

    applyStamp(lanes[lane], stamp);
    lanes[lane].vocalSlug = ToneSlug::encodeVocal(stamp);
    lanes[lane].throughChain.store(1, std::memory_order_relaxed);
    metaDirty.store(true, std::memory_order_relaxed);
}

juce::String TapeEngine::getVocalSlug(int lane) const
{
    if (! valid(lane))
        return {};
    if (lanes[lane].vocalSlug.isNotEmpty())
        return lanes[lane].vocalSlug;
    return ToneSlug::encodeVocal(readStamp(lanes[lane]));
}

VocalStamp TapeEngine::getVocalStamp(int lane) const
{
    if (! valid(lane))
        return {};
    return readStamp(lanes[lane]);
}

bool TapeEngine::laneThrough(const Lane& lane) const
{
    return lane.throughChain.load(std::memory_order_relaxed) != 0
        || lane.vocalSlug.isNotEmpty();
}

bool TapeEngine::isThroughChain(int lane) const
{
    return valid(lane) && laneThrough(lanes[lane]);
}

bool TapeEngine::isVocalLane(int lane) const
{
    return isThroughChain(lane);
}

bool TapeEngine::hasThrough(int lane) const
{
    return valid(lane) && throughActive[(size_t) lane] != 0;
}

const float* TapeEngine::getThroughDry(int lane) const
{
    if (! hasThrough(lane) || throughDry[lane].getNumSamples() <= 0)
        return nullptr;
    return throughDry[lane].getReadPointer(0);
}

void TapeEngine::setMute(int lane, bool shouldMute)
{
    if (! valid(lane))
        return;
    lanes[lane].mute.store(shouldMute ? 1 : 0, std::memory_order_relaxed);
    metaDirty.store(true, std::memory_order_relaxed);
}

void TapeEngine::setLevel(int lane, float level)
{
    if (! valid(lane))
        return;
    lanes[lane].level.store(juce::jlimit(0.0f, 1.0f, level), std::memory_order_relaxed);
    metaDirty.store(true, std::memory_order_relaxed);
}

void TapeEngine::setPan(int lane, float pan)
{
    if (! valid(lane))
        return;
    lanes[lane].pan.store(juce::jlimit(-1.0f, 1.0f, pan), std::memory_order_relaxed);
    metaDirty.store(true, std::memory_order_relaxed);
}

void TapeEngine::setName(int lane, const juce::String& name)
{
    if (! valid(lane) || name.trim().isEmpty())
        return;
    lanes[lane].name = name.trim();
    metaDirty.store(true, std::memory_order_relaxed);
}

void TapeEngine::setStart(int lane, int startSample)
{
    if (! valid(lane))
        return;
    lanes[lane].start.store(juce::jmax(0, startSample), std::memory_order_relaxed);
    metaDirty.store(true, std::memory_order_relaxed);
}

void TapeEngine::setEnd(int lane, int endSample)
{
    if (! valid(lane))
        return;
    const int frames = lanes[lane].fileFrames.load(std::memory_order_relaxed);
    const int in = lanes[lane].in.load(std::memory_order_relaxed);
    lanes[lane].end.store(juce::jlimit(in + 1, juce::jmax(in + 1, frames), endSample),
                          std::memory_order_relaxed);
    metaDirty.store(true, std::memory_order_relaxed);
}

void TapeEngine::trimLeft(int lane, int newTimelineStart)
{
    if (! valid(lane))
        return;

    auto& l = lanes[lane];
    const int start0 = l.start.load(std::memory_order_relaxed);
    const int in0 = l.in.load(std::memory_order_relaxed);
    const int end0 = l.end.load(std::memory_order_relaxed);
    if (end0 <= 0)
        return;

    int newIn = in0 + (newTimelineStart - start0);
    newIn = juce::jlimit(0, end0 - 1, newIn);
    l.in.store(newIn, std::memory_order_relaxed);
    l.start.store(juce::jmax(0, start0 + (newIn - in0)), std::memory_order_relaxed);
    metaDirty.store(true, std::memory_order_relaxed);
}

void TapeEngine::setQuantize(bool shouldQuantize)
{
    quantize.store(shouldQuantize, std::memory_order_relaxed);
    metaDirty.store(true, std::memory_order_relaxed);
}

void TapeEngine::clearLane(int lane)
{
    if (! valid(lane))
        return;

    auto& l = lanes[lane];
    l.hasClip.store(0, std::memory_order_relaxed);
    l.fileFrames.store(0, std::memory_order_relaxed);
    l.start.store(0, std::memory_order_relaxed);
    l.in.store(0, std::memory_order_relaxed);
    l.end.store(0, std::memory_order_relaxed);
    l.hopCount.store(0, std::memory_order_relaxed);
    l.throughChain.store(0, std::memory_order_relaxed);
    l.vocalSlug.clear();
    applyStamp(l, {});
    const auto file = laneFile(lane);
    if (file.existsAsFile())
        file.deleteFile();
    metaDirty.store(true, std::memory_order_relaxed);
    audioDirty.store(true, std::memory_order_relaxed);
}

bool TapeEngine::isRecording() const
{
    const auto t = getTransport();
    return t == Transport::Armed || t == Transport::Recording;
}

bool TapeEngine::isPlaying() const
{
    const auto t = getTransport();
    return t == Transport::Playing || t == Transport::Recording;
}

int TapeEngine::getViewSamples() const
{
    int view = juce::jmax(1, (int) std::round(sampleRate * 2.0));
    view = juce::jmax(view, playheadPub.load(std::memory_order_relaxed));
    view = juce::jmax(view, recStartPub.load(std::memory_order_relaxed)
                            + recFramesPub.load(std::memory_order_relaxed));
    for (int i = 0; i < numLanes; ++i)
    {
        const auto& l = lanes[i];
        if (l.hasClip.load(std::memory_order_relaxed) != 0)
            view = juce::jmax(view, l.start.load(std::memory_order_relaxed)
                                    + juce::jmax(0, l.end.load(std::memory_order_relaxed)
                                                    - l.in.load(std::memory_order_relaxed)));
    }
    return view;
}

bool TapeEngine::exportMix(const juce::File& dest, double startSeconds, double lengthSeconds,
                           juce::String& error)
{
    if (isRecording())
    {
        error = "Stop recording first.";
        return false;
    }

    const double sr = sampleRate > 0.0 ? sampleRate : 48000.0;
    const int cap = maxSamples > 0 ? maxSamples : (int) std::lround(sr * maxSeconds);
    const int start = juce::jlimit(0, cap, (int) std::lround(juce::jmax(0.0, startSeconds) * sr));
    const int frames = juce::jlimit(1, cap, (int) std::lround(juce::jmax(0.05, lengthSeconds) * sr));

    bool any = false;
    for (int i = 0; i < numLanes; ++i)
    {
        if (lanes[i].hasClip.load(std::memory_order_relaxed) != 0
            && lanes[i].mute.load(std::memory_order_relaxed) == 0)
            any = true;
    }
    if (! any)
    {
        error = "No unmuted clips to export.";
        return false;
    }

    juce::AudioBuffer<float> mix(2, frames);
    mix.clear();
    const int block = 128;
    juce::AudioBuffer<float> dry(1, block);
    const float bpm = lastBpm.load(std::memory_order_relaxed);
    for (int done = 0; done < frames; )
    {
        const int n = juce::jmin(block, frames - done);
        auto* left = mix.getWritePointer(0, done);
        auto* right = mix.getWritePointer(1, done);
        for (int s = 0; s < n; ++s)
        {
            float mixL = 0.0f;
            float mixR = 0.0f;
            const int head = start + done + s;
            for (int i = 0; i < numLanes; ++i)
            {
                if (laneThrough(lanes[i]))
                    continue;
                mixLane(lanes[i], head, mixL, mixR);
            }
            left[s] += mixL;
            right[s] += mixR;
        }

        if (throughRender != nullptr)
        {
            for (int i = 0; i < numLanes; ++i)
            {
                auto& lane = lanes[i];
                if (! laneThrough(lane)
                    || lane.hasClip.load(std::memory_order_relaxed) == 0
                    || lane.mute.load(std::memory_order_relaxed) != 0)
                    continue;

                dry.setSize(1, n, false, false, true);
                dry.clear();
                auto* d = dry.getWritePointer(0);
                bool any = false;
                for (int s = 0; s < n; ++s)
                {
                    float L = 0.0f, R = 0.0f;
                    if (readLaneAt(lane, start + done + s, L, R))
                    {
                        d[s] = 0.5f * (L + R);
                        any = true;
                    }
                }
                if (! any)
                    continue;

                throughRender(readStamp(lane), dry, bpm);
                const float gain = lane.level.load(std::memory_order_relaxed);
                const float angle = (lane.pan.load(std::memory_order_relaxed) + 1.0f)
                                    * (juce::MathConstants<float>::halfPi * 0.5f);
                const float gL = gain * std::cos(angle);
                const float gR = gain * std::sin(angle);
                const auto* wet = dry.getReadPointer(0);
                for (int s = 0; s < n; ++s)
                {
                    left[s] += wet[s] * gL;
                    right[s] += wet[s] * gR;
                }
            }
        }
        done += n;
    }

    dest.getParentDirectory().createDirectory();
    dest.deleteFile();
    std::unique_ptr<juce::OutputStream> stream(dest.createOutputStream());
    if (stream == nullptr)
    {
        error = "Could not write that file.";
        return false;
    }

    juce::WavAudioFormat wav;
    const auto options = juce::AudioFormatWriterOptions{}
                             .withSampleRate(sr)
                             .withChannelLayout(juce::AudioChannelSet::stereo())
                             .withBitsPerSample(24)
                             .withSampleFormat(juce::AudioFormatWriterOptions::SampleFormat::integral);
    auto writer = wav.createWriterFor(stream, options);
    if (writer == nullptr)
    {
        error = "Could not create a WAV writer.";
        return false;
    }
    if (! writer->writeFromAudioSampleBuffer(mix, 0, frames))
    {
        error = "Write failed.";
        return false;
    }
    writer.reset();
    return true;
}

int TapeEngine::snapSample(int sample) const
{
    const int beat = beatSamples.load(std::memory_order_relaxed);
    if (! quantize.load(std::memory_order_relaxed) || beat <= 0)
        return juce::jmax(0, sample);
    return juce::jmax(0, (int) std::lround((double) sample / (double) beat) * beat);
}

juce::String TapeEngine::getName(int lane) const
{
    if (! valid(lane))
        return {};
    return lanes[lane].name;
}

bool TapeEngine::isMuted(int lane) const
{
    return valid(lane) && lanes[lane].mute.load(std::memory_order_relaxed) != 0;
}

float TapeEngine::getLevel(int lane) const
{
    if (! valid(lane))
        return 1.0f;
    return lanes[lane].level.load(std::memory_order_relaxed);
}

float TapeEngine::getPan(int lane) const
{
    if (! valid(lane))
        return 0.0f;
    return lanes[lane].pan.load(std::memory_order_relaxed);
}

bool TapeEngine::hasClip(int lane) const
{
    return valid(lane) && lanes[lane].hasClip.load(std::memory_order_relaxed) != 0;
}

TapeEngine::LaneView TapeEngine::getLane(int lane) const
{
    LaneView view;
    if (! valid(lane))
        return view;

    const auto& l = lanes[lane];
    view.hasClip = l.hasClip.load(std::memory_order_relaxed) != 0;
    view.mute = l.mute.load(std::memory_order_relaxed) != 0;
    view.start = l.start.load(std::memory_order_relaxed);
    view.in = l.in.load(std::memory_order_relaxed);
    view.end = l.end.load(std::memory_order_relaxed);
    view.fileFrames = l.fileFrames.load(std::memory_order_relaxed);
    view.hopCount = juce::jlimit(0, maxHops, l.hopCount.load(std::memory_order_relaxed));
    view.hops = l.hops;
    return view;
}

bool TapeEngine::takeDirty()
{
    return metaDirty.exchange(false, std::memory_order_acq_rel)
        || audioDirty.load(std::memory_order_relaxed);
}

bool TapeEngine::valid(int lane) const
{
    return juce::isPositiveAndBelow(lane, numLanes);
}

void TapeEngine::ensureLane(int lane)
{
    if (! valid(lane) || maxSamples <= 0)
        return;
    auto& l = lanes[lane];
    if (l.audio.getNumSamples() < maxSamples)
        l.audio.setSize(2, maxSamples, false, true, false);
}

int TapeEngine::beatLength() const
{
    if (currentSpb <= 0.0)
        return minSamples;
    return juce::jmax(1, (int) std::lround(currentSpb));
}

void TapeEngine::process(juce::AudioBuffer<float>& stereo, int numSamples,
                         double metroPhase, double samplesPerBeat,
                         const float* dryMono)
{
    if (numSamples <= 0 || stereo.getNumChannels() < 2)
        return;

    currentSpb = samplesPerBeat;
    beatSamples.store(samplesPerBeat > 0.0 ? juce::jmax(1, (int) std::lround(samplesPerBeat)) : 0,
                      std::memory_order_relaxed);
    if (samplesPerBeat > 0.0 && sampleRate > 0.0)
        lastBpm.store((float) juce::jlimit(40.0, 240.0, sampleRate * 60.0 / samplesPerBeat),
                      std::memory_order_relaxed);
    if (guitar.getNumSamples() < numSamples)
        guitar.setSize(2, numSamples, false, false, true);
    for (int i = 0; i < numLanes; ++i)
    {
        if (throughDry[i].getNumSamples() < numSamples)
            throughDry[i].setSize(1, numSamples, false, false, true);
        throughDry[i].clear();
        throughActive[(size_t) i] = 0;
    }

    guitar.copyFrom(0, 0, stereo, 0, 0, numSamples);
    guitar.copyFrom(1, 0, stereo, 1, 0, numSamples);

    const auto cmd = (Cmd) command.exchange((int) Cmd::None, std::memory_order_acq_rel);
    if (cmd != Cmd::None)
        apply(cmd);

    if (hasSeek.exchange(false, std::memory_order_acq_rel) && transport != Transport::Recording)
        playhead = juce::jmax(0, seekSample.load(std::memory_order_relaxed));

    auto* left = stereo.getWritePointer(0);
    auto* right = stereo.getWritePointer(1);
    const auto* gL = guitar.getReadPointer(0);
    const auto* gR = guitar.getReadPointer(1);
    double phase = metroPhase;

    for (int s = 0; s < numSamples; ++s)
    {
        bool onBeat = false;
        if (samplesPerBeat > 0.0 && phase >= samplesPerBeat)
        {
            phase -= samplesPerBeat;
            onBeat = true;
        }

        if (transport == Transport::Armed && onBeat)
            beginRecord();

        if (transport == Transport::Recording)
        {
            const bool beatClose = closeOnBeat && onBeat
                                   && recFrames >= minSamples
                                   && recFrames >= beatLength();
            if (beatClose)
            {
                finishRecord();
                transport = Transport::Paused;
            }
            else if (valid(recLane) && recFrames < maxSamples
                     && lanes[recLane].audio.getNumSamples() > recFrames)
            {
                auto& lane = lanes[recLane];
                const bool dryRec = lane.throughChain.load(std::memory_order_relaxed) != 0
                                    && dryMono != nullptr;
                const float liveL = dryRec ? dryMono[s] : gL[s];
                const float liveR = dryRec ? dryMono[s] : gR[s];
                lane.audio.setSample(0, recFrames, liveL);
                lane.audio.setSample(1, recFrames, liveR);
                noteHop(lane, recFrames, juce::jmax(std::abs(liveL), std::abs(liveR)));
                ++recFrames;
                lane.start.store(recStart, std::memory_order_relaxed);
                lane.end.store(recFrames, std::memory_order_relaxed);
                lane.fileFrames.store(recFrames, std::memory_order_relaxed);
                if (recFrames >= maxSamples)
                {
                    finishRecord();
                    transport = Transport::Paused;
                }
            }
        }

        if (transport == Transport::Playing || transport == Transport::Recording)
        {
            float mixL = 0.0f;
            float mixR = 0.0f;
            for (int i = 0; i < numLanes; ++i)
            {
                if (transport == Transport::Recording && i == recLane)
                    continue;
                auto& lane = lanes[i];
                if (laneThrough(lane))
                {
                    float L = 0.0f, R = 0.0f;
                    if (readLaneAt(lane, playhead, L, R)
                        && lane.mute.load(std::memory_order_relaxed) == 0)
                    {
                        throughDry[i].setSample(0, s, 0.5f * (L + R));
                        throughActive[(size_t) i] = 1;
                    }
                    continue;
                }
                mixLane(lane, playhead, mixL, mixR);
            }
            left[s] += mixL;
            right[s] += mixR;
            ++playhead;
            if (loopOn.load(std::memory_order_relaxed) && transport == Transport::Playing)
            {
                const int a = loopStart.load(std::memory_order_relaxed);
                const int b = loopEnd.load(std::memory_order_relaxed);
                if (b > a && playhead >= b)
                    playhead = a;
            }
        }

        phase += 1.0;
    }

    transportPub.store((int) transport, std::memory_order_relaxed);
    playheadPub.store(playhead, std::memory_order_relaxed);
    recLanePub.store(recLane, std::memory_order_relaxed);
    recFramesPub.store(recFrames, std::memory_order_relaxed);
    recStartPub.store(recStart, std::memory_order_relaxed);
}

bool TapeEngine::readLaneAt(const Lane& lane, int head, float& outL, float& outR) const
{
    if (lane.hasClip.load(std::memory_order_relaxed) == 0)
        return false;

    const int start = lane.start.load(std::memory_order_relaxed);
    const int in = lane.in.load(std::memory_order_relaxed);
    const int end = lane.end.load(std::memory_order_relaxed);
    const int audible = end - in;
    const int idx = head - start;
    if (idx < 0 || idx >= audible)
        return false;
    const int fileIdx = in + idx;
    if (fileIdx < 0 || fileIdx >= lane.audio.getNumSamples())
        return false;

    outL = lane.audio.getSample(0, fileIdx);
    outR = lane.audio.getNumChannels() > 1 ? lane.audio.getSample(1, fileIdx) : outL;
    return true;
}

void TapeEngine::mixLane(const Lane& lane, int head, float& outL, float& outR) const
{
    if (lane.mute.load(std::memory_order_relaxed) != 0)
        return;

    float L = 0.0f, R = 0.0f;
    if (! readLaneAt(lane, head, L, R))
        return;

    const float gain = lane.level.load(std::memory_order_relaxed);
    const float angle = (lane.pan.load(std::memory_order_relaxed) + 1.0f)
                        * (juce::MathConstants<float>::halfPi * 0.5f);
    outL += L * gain * std::cos(angle);
    outR += R * gain * std::sin(angle);
}

void TapeEngine::noteHop(Lane& lane, int pos, float peak)
{
    const int h = pos / hop;
    if (! juce::isPositiveAndBelow(h, maxHops))
        return;
    lane.hops[h] = (pos % hop == 0) ? peak : juce::jmax(lane.hops[h], peak);
    const int count = juce::jmax(lane.hopCount.load(std::memory_order_relaxed), h + 1);
    lane.hopCount.store(count, std::memory_order_relaxed);
}

void TapeEngine::apply(Cmd cmd)
{
    switch (cmd)
    {
        case Cmd::Play:
            if (transport == Transport::Recording || transport == Transport::Armed)
                break;
            transport = Transport::Playing;
            break;

        case Cmd::Pause:
            if (transport == Transport::Recording)
            {
                finishRecord();
                transport = Transport::Paused;
                break;
            }
            if (transport == Transport::Armed)
            {
                transport = Transport::Paused;
                break;
            }
            if (transport == Transport::Playing)
                transport = Transport::Paused;
            break;

        case Cmd::Stop:
            if (transport == Transport::Recording)
                finishRecord();
            transport = Transport::Stopped;
            playhead = 0;
            closeOnBeat = false;
            break;

        case Cmd::Rec:
            if (transport == Transport::Recording || transport == Transport::Armed)
                break;
            recLane = armedLane.load(std::memory_order_relaxed);
            if (! valid(recLane))
                recLane = 0;
            if (lanes[recLane].hasClip.load(std::memory_order_relaxed) != 0)
                break;
            closeOnBeat = false;
            wasPlaying = (transport == Transport::Playing);
            if (quantize.load(std::memory_order_relaxed) && currentSpb > 0.0)
                transport = Transport::Armed;
            else
                beginRecord();
            break;

        case Cmd::RecStop:
            if (transport == Transport::Armed)
            {
                transport = playhead > 0 ? Transport::Paused : Transport::Stopped;
                break;
            }
            if (transport == Transport::Recording)
                requestClose();
            break;

        case Cmd::None:
            break;
    }
}

void TapeEngine::beginRecord()
{
    if (! valid(recLane))
        recLane = 0;
    if (lanes[recLane].hasClip.load(std::memory_order_relaxed) != 0)
    {
        transport = playhead > 0 ? Transport::Paused : Transport::Stopped;
        return;
    }

    auto& lane = lanes[recLane];
    recFrames = 0;
    recStart = playhead;
    closeOnBeat = false;
    const bool through = recordThrough.load(std::memory_order_relaxed) != 0
                         || lane.throughChain.load(std::memory_order_relaxed) != 0;
    lane.throughChain.store(through ? 1 : 0, std::memory_order_relaxed);
    if (through)
        applyStamp(lane, readLiveStamp());
    else
    {
        applyStamp(lane, {});
        lane.vocalSlug.clear();
    }
    std::fill(std::begin(lane.hops), std::end(lane.hops), 0.0f);
    lane.hopCount.store(0, std::memory_order_relaxed);
    lane.hasClip.store(0, std::memory_order_relaxed);
    lane.fileFrames.store(0, std::memory_order_relaxed);
    lane.start.store(recStart, std::memory_order_relaxed);
    lane.in.store(0, std::memory_order_relaxed);
    lane.end.store(0, std::memory_order_relaxed);
    transport = Transport::Recording;
}

void TapeEngine::requestClose()
{
    if (quantize.load(std::memory_order_relaxed) && currentSpb > 0.0)
    {
        const int n = (int) std::floor((double) recFrames / currentSpb);
        const double frac = (double) recFrames / currentSpb - (double) n;
        if (n >= 1 && recFrames >= minSamples && frac <= 0.10)
        {
            recFrames = juce::jlimit(minSamples, recFrames,
                                     (int) std::lround((double) n * currentSpb));
            finishRecord();
            transport = Transport::Paused;
            return;
        }

        closeOnBeat = true;
        return;
    }

    finishRecord();
    transport = Transport::Paused;
}

void TapeEngine::finishRecord()
{
    if (transport != Transport::Recording || ! valid(recLane))
        return;

    auto& lane = lanes[recLane];
    if (recFrames <= 0)
    {
        lane.hasClip.store(0, std::memory_order_relaxed);
        return;
    }

    int frames = recFrames;
    if (quantize.load(std::memory_order_relaxed) && currentSpb > 0.0)
    {
        const int n = juce::jmax(1, (int) std::lround((double) frames / currentSpb));
        frames = juce::jmin(frames, (int) std::lround((double) n * currentSpb));
    }
    frames = juce::jlimit(1, lane.audio.getNumSamples(), frames);

    lane.fileFrames.store(frames, std::memory_order_relaxed);
    lane.in.store(0, std::memory_order_relaxed);
    lane.end.store(frames, std::memory_order_relaxed);
    lane.start.store(recStart, std::memory_order_relaxed);
    lane.hasClip.store(1, std::memory_order_relaxed);
    lane.hopCount.store(juce::jlimit(0, maxHops, (frames + hop - 1) / hop),
                        std::memory_order_relaxed);
    metaDirty.store(true, std::memory_order_relaxed);
    audioDirty.store(true, std::memory_order_relaxed);
}

juce::File TapeEngine::tapeDirectory()
{
    auto dir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                   .getChildFile("ToneStar")
                   .getChildFile("tape");
    dir.createDirectory();
    return dir;
}

juce::File TapeEngine::laneFile(int lane)
{
    return tapeDirectory().getChildFile("track" + juce::String(lane) + ".wav");
}

void TapeEngine::loadSession()
{
    if (maxSamples <= 0)
        maxSamples = (int) std::round((sampleRate > 0.0 ? sampleRate : 48000.0) * maxSeconds);

    const auto dir = tapeDirectory();
    std::unique_ptr<juce::XmlElement> xml(juce::XmlDocument::parse(dir.getChildFile("tape.xml")));

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();

    for (int i = 0; i < numLanes; ++i)
    {
        auto& lane = lanes[i];
        lane.name = "Track " + juce::String(i + 1);
        lane.hasClip.store(0, std::memory_order_relaxed);
        lane.fileFrames.store(0, std::memory_order_relaxed);
        lane.start.store(0, std::memory_order_relaxed);
        lane.in.store(0, std::memory_order_relaxed);
        lane.end.store(0, std::memory_order_relaxed);
        lane.mute.store(0, std::memory_order_relaxed);
        lane.level.store(1.0f, std::memory_order_relaxed);
        lane.pan.store(0.0f, std::memory_order_relaxed);
        lane.hopCount.store(0, std::memory_order_relaxed);
        lane.throughChain.store(0, std::memory_order_relaxed);
        lane.vocalSlug.clear();
        applyStamp(lane, {});
        std::fill(std::begin(lane.hops), std::end(lane.hops), 0.0f);

        if (xml != nullptr)
        {
            for (auto* node = xml->getFirstChildElement(); node != nullptr; node = node->getNextElement())
            {
                if (! node->hasTagName("TRACK") || node->getIntAttribute("index") != i)
                    continue;
                const auto name = node->getStringAttribute("name").trim();
                if (name.isNotEmpty())
                    lane.name = name;
                lane.start.store(juce::jmax(0, node->getIntAttribute("start")), std::memory_order_relaxed);
                lane.in.store(juce::jmax(0, node->getIntAttribute("in")), std::memory_order_relaxed);
                lane.end.store(juce::jmax(0, node->getIntAttribute("end")), std::memory_order_relaxed);
                lane.mute.store(node->getBoolAttribute("mute") ? 1 : 0, std::memory_order_relaxed);
                lane.level.store(juce::jlimit(0.0f, 1.0f, (float) node->getDoubleAttribute("level", 1.0)),
                                 std::memory_order_relaxed);
                lane.pan.store(juce::jlimit(-1.0f, 1.0f, (float) node->getDoubleAttribute("pan", 0.0)),
                                 std::memory_order_relaxed);
                lane.throughChain.store(node->getBoolAttribute("throughChain") ? 1 : 0,
                                        std::memory_order_relaxed);
                const auto slug = node->getStringAttribute("slug").trim();
                VocalStamp stamp;
                if (ToneSlug::decodeVocal(slug, stamp))
                {
                    applyStamp(lane, stamp);
                    lane.vocalSlug = ToneSlug::encodeVocal(stamp);
                }
            }
        }

        const auto file = laneFile(i);
        if (! file.existsAsFile())
            continue;

        std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
        if (reader == nullptr || reader->lengthInSamples <= 0)
            continue;

        const int frames = (int) juce::jmin((juce::int64) maxSamples > 0 ? (juce::int64) maxSamples
                                                                        : (juce::int64) (48000 * maxSeconds),
                                            reader->lengthInSamples);
        lane.audio.setSize(2, juce::jmax(frames, maxSamples), false, true, false);
        reader->read(&lane.audio, 0, frames, 0, true, true);
        lane.fileFrames.store(frames, std::memory_order_relaxed);
        if (lane.end.load(std::memory_order_relaxed) <= 0
            || lane.end.load(std::memory_order_relaxed) > frames)
            lane.end.store(frames, std::memory_order_relaxed);
        if (lane.in.load(std::memory_order_relaxed) >= lane.end.load(std::memory_order_relaxed))
            lane.in.store(0, std::memory_order_relaxed);
        lane.hasClip.store(1, std::memory_order_relaxed);

        const int hopsN = juce::jlimit(0, maxHops, (frames + hop - 1) / hop);
        for (int h = 0; h < hopsN; ++h)
        {
            const int i0 = h * hop;
            const int i1 = juce::jmin(frames, i0 + hop);
            float peak = 0.0f;
            for (int s = i0; s < i1; ++s)
                peak = juce::jmax(peak, std::abs(lane.audio.getSample(0, s)),
                                  std::abs(lane.audio.getSample(1, s)));
            lane.hops[h] = peak;
        }
        lane.hopCount.store(hopsN, std::memory_order_relaxed);
    }

    if (xml != nullptr)
    {
        quantize.store(xml->getBoolAttribute("quantize"), std::memory_order_relaxed);
        pixelsPerBeat = (float) xml->getDoubleAttribute("pixelsPerBeat", 48.0);
        pixelsPerBeat = juce::jlimit(8.0f, 240.0f, pixelsPerBeat);
        viewStart = juce::jmax(0, xml->getIntAttribute("viewStart"));
        loopOn.store(xml->getBoolAttribute("loop"), std::memory_order_relaxed);
        loopStart.store(juce::jmax(0, xml->getIntAttribute("loopStart")), std::memory_order_relaxed);
        loopEnd.store(juce::jmax(0, xml->getIntAttribute("loopEnd")), std::memory_order_relaxed);
    }
}

void TapeEngine::saveSession()
{
    const auto dir = tapeDirectory();
    auto xml = std::make_unique<juce::XmlElement>("TAPE");
    xml->setAttribute("quantize", quantize.load(std::memory_order_relaxed) ? 1 : 0);
    xml->setAttribute("pixelsPerBeat", (double) pixelsPerBeat);
    xml->setAttribute("viewStart", viewStart);
    xml->setAttribute("loop", loopOn.load(std::memory_order_relaxed) ? 1 : 0);
    xml->setAttribute("loopStart", loopStart.load(std::memory_order_relaxed));
    xml->setAttribute("loopEnd", loopEnd.load(std::memory_order_relaxed));

    const bool writeAudio = audioDirty.exchange(false, std::memory_order_acq_rel);
    juce::WavAudioFormat wav;
    for (int i = 0; i < numLanes; ++i)
    {
        auto& lane = lanes[i];
        auto* node = xml->createNewChildElement("TRACK");
        node->setAttribute("index", i);
        node->setAttribute("name", lane.name);
        node->setAttribute("start", lane.start.load(std::memory_order_relaxed));
        node->setAttribute("in", lane.in.load(std::memory_order_relaxed));
        node->setAttribute("end", lane.end.load(std::memory_order_relaxed));
        node->setAttribute("mute", lane.mute.load(std::memory_order_relaxed) != 0 ? 1 : 0);
        node->setAttribute("level", (double) lane.level.load(std::memory_order_relaxed));
        node->setAttribute("pan", (double) lane.pan.load(std::memory_order_relaxed));
        node->setAttribute("throughChain", lane.throughChain.load(std::memory_order_relaxed) != 0 ? 1 : 0);
        if (lane.throughChain.load(std::memory_order_relaxed) != 0)
        {
            if (lane.vocalSlug.isEmpty())
                lane.vocalSlug = ToneSlug::encodeVocal(readStamp(lane));
            node->setAttribute("slug", lane.vocalSlug);
        }

        if (! writeAudio)
            continue;

        const int frames = lane.fileFrames.load(std::memory_order_relaxed);
        const auto file = laneFile(i);
        if (lane.hasClip.load(std::memory_order_relaxed) == 0 || frames <= 0
            || lane.audio.getNumSamples() < frames)
        {
            if (file.existsAsFile())
                file.deleteFile();
            continue;
        }

        file.deleteFile();
        std::unique_ptr<juce::OutputStream> stream(file.createOutputStream());
        if (stream == nullptr)
            continue;

        const auto options = juce::AudioFormatWriterOptions{}
                                 .withSampleRate(sampleRate > 0.0 ? sampleRate : 48000.0)
                                 .withNumChannels(2)
                                 .withBitsPerSample(32);
        if (auto writer = wav.createWriterFor(stream, options))
            writer->writeFromAudioSampleBuffer(lane.audio, 0, frames);
    }

    xml->writeTo(dir.getChildFile("tape.xml"));
}
