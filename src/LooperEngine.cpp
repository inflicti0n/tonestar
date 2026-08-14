#include "LooperEngine.h"
#include <cmath>

LooperEngine::LooperEngine()
{
    for (int i = 0; i < numPhrases; ++i)
    {
        commands[(size_t) i].store((int) Cmd::None, std::memory_order_relaxed);
        levels[(size_t) i].store(1.0f, std::memory_order_relaxed);
        states[(size_t) i].store((int) State::Empty, std::memory_order_relaxed);
        heads[(size_t) i].store(0.0f, std::memory_order_relaxed);
        undoFlags[(size_t) i].store(0, std::memory_order_relaxed);
        contentFlags[(size_t) i].store(0, std::memory_order_relaxed);
    }
}

void LooperEngine::prepare(double sampleRateToUse, int)
{
    sampleRate = sampleRateToUse > 0.0 ? sampleRateToUse : 48000.0;
    maxSamples = juce::jmax(1, (int) std::round(sampleRate * maxSeconds));
    minSamples = juce::jmax(1, (int) std::round(sampleRate * minSeconds));
    fadeN = juce::jmax(1, (int) std::round(sampleRate * fadeSeconds));

    for (int i = 0; i < numPhrases; ++i)
    {
        auto& p = phrases[i];
        p.committed.setSize(2, maxSamples, false, true, false);
        p.pending.setSize(2, maxSamples, false, true, false);
        p.undoLayer.setSize(2, maxSamples, false, true, false);
        p.length = 0;
        p.playhead = 0;
        p.recPos = 0;
        p.recorded = 0;
        p.state = State::Empty;
        p.closeWhenMin = false;
        p.closeOnBeat = false;
        p.closeAfterStart = false;
        p.alignedRecord = false;
        p.protectSamples = 0;
        p.hasUndoLayer = false;
        p.undone = false;
        p.playOnMasterZero = false;
        p.level = levels[(size_t) i].load(std::memory_order_relaxed);
        commands[(size_t) i].store((int) Cmd::None, std::memory_order_relaxed);
    }

    master = -1;
    wasSimple = simpleMode.load(std::memory_order_relaxed);
    publish();
}

void LooperEngine::reset()
{
    for (int i = 0; i < numPhrases; ++i)
        clearPhrase(i);
    master = -1;
    publish();
}

void LooperEngine::tap(int phrase) { post(phrase, Cmd::Tap); }
void LooperEngine::doubleTap(int phrase) { post(phrase, Cmd::DoubleTap); }
void LooperEngine::hold(int phrase) { post(phrase, Cmd::Hold); }
void LooperEngine::stop(int phrase) { post(phrase, Cmd::Stop); }

void LooperEngine::setLevel(int phrase, float value)
{
    if (! validIndex(phrase))
        return;
    levels[(size_t) phrase].store(juce::jlimit(0.0f, 1.0f, value), std::memory_order_relaxed);
}

void LooperEngine::setSimpleMode(bool shouldBeSimple)
{
    simpleMode.store(shouldBeSimple, std::memory_order_relaxed);
    if (shouldBeSimple)
        armed.store(0, std::memory_order_relaxed);
}

void LooperEngine::setArmed(int phrase)
{
    if (! validIndex(phrase))
        return;
    if (simpleMode.load(std::memory_order_relaxed))
        phrase = 0;
    armed.store(phrase, std::memory_order_relaxed);
}

void LooperEngine::setQuantize(bool shouldQuantize)
{
    quantize.store(shouldQuantize, std::memory_order_relaxed);
}

void LooperEngine::setBpm(float value)
{
    bpm.store(juce::jlimit(40.0f, 240.0f, value), std::memory_order_relaxed);
}

int LooperEngine::beatLength() const
{
    if (currentSpb <= 0.0)
        return minSamples;
    return juce::jmax(1, (int) std::lround(currentSpb));
}

LooperEngine::State LooperEngine::getState(int phrase) const
{
    if (! validIndex(phrase))
        return State::Empty;
    return (State) states[(size_t) phrase].load(std::memory_order_relaxed);
}

float LooperEngine::getPlayhead01(int phrase) const
{
    if (! validIndex(phrase))
        return 0.0f;
    return heads[(size_t) phrase].load(std::memory_order_relaxed);
}

bool LooperEngine::hasUndo(int phrase) const
{
    if (! validIndex(phrase))
        return false;
    return undoFlags[(size_t) phrase].load(std::memory_order_relaxed) != 0;
}

float LooperEngine::getLevel(int phrase) const
{
    if (! validIndex(phrase))
        return 1.0f;
    return levels[(size_t) phrase].load(std::memory_order_relaxed);
}

bool LooperEngine::hasContent(int phrase) const
{
    if (! validIndex(phrase))
        return false;
    return contentFlags[(size_t) phrase].load(std::memory_order_relaxed) != 0;
}

void LooperEngine::post(int phrase, Cmd cmd)
{
    if (! validIndex(phrase))
        return;
    if (simpleMode.load(std::memory_order_relaxed))
        phrase = 0;
    commands[(size_t) phrase].store((int) cmd, std::memory_order_release);
}

void LooperEngine::process(juce::AudioBuffer<float>& stereo, int numSamples,
                           double metroPhase, double samplesPerBeat)
{
    if (numSamples <= 0 || stereo.getNumChannels() < 2 || maxSamples <= 0)
        return;

    currentSpb = samplesPerBeat;
    const bool simple = simpleMode.load(std::memory_order_relaxed);
    if (simple && ! wasSimple)
        freezeSecondary();
    wasSimple = simple;

    for (int i = 0; i < numPhrases; ++i)
    {
        phrases[i].level = levels[(size_t) i].load(std::memory_order_relaxed);
        const auto cmd = (Cmd) commands[(size_t) i].exchange((int) Cmd::None, std::memory_order_acq_rel);
        if (cmd != Cmd::None && ! (simple && i == 1))
            applyCommand(i, cmd);
    }

    auto* left = stereo.getWritePointer(0);
    auto* right = stereo.getWritePointer(1);
    double phase = metroPhase;

    for (int s = 0; s < numSamples; ++s)
    {
        bool onBeat = false;
        if (samplesPerBeat > 0.0 && phase >= samplesPerBeat)
        {
            phase -= samplesPerBeat;
            onBeat = true;
        }

        const float liveL = left[s];
        const float liveR = right[s];
        float mixL = 0.0f;
        float mixR = 0.0f;

        if (auto* m = masterPhrase())
        {
            if (m->playhead == 0 && isActive(*m))
            {
                for (int i = 0; i < numPhrases; ++i)
                {
                    auto& p = phrases[i];
                    if (p.playOnMasterZero && p.length > 0)
                    {
                        p.playOnMasterZero = false;
                        p.playhead = 0;
                        p.state = State::Playing;
                    }
                }
            }
        }

        const int last = simple ? 1 : numPhrases;
        for (int i = 0; i < last; ++i)
        {
            auto& p = phrases[i];
            if (p.committed.getNumSamples() < maxSamples)
                continue;

            if (p.state == State::Armed && onBeat)
                startRecord(i);

            if (p.protectSamples > 0)
                --p.protectSamples;

            switch (p.state)
            {
                case State::Recording:
                {
                    if (p.alignedRecord && p.length > 0)
                    {
                        p.committed.setSample(0, p.playhead, liveL);
                        p.committed.setSample(1, p.playhead, liveR);
                        p.playhead = (p.playhead + 1) % p.length;
                        ++p.recorded;
                        if (p.recorded >= p.length)
                            closeRecord(i);
                    }
                    else
                    {
                        const bool beatClose = p.closeOnBeat && onBeat
                                               && p.recPos >= minSamples
                                               && p.recPos >= beatLength();
                        if (beatClose)
                        {
                            closeRecord(i);
                            break;
                        }

                        if (p.recPos < maxSamples)
                        {
                            p.committed.setSample(0, p.recPos, liveL);
                            p.committed.setSample(1, p.recPos, liveR);
                            ++p.recPos;
                        }

                        const bool hitMax = p.recPos >= maxSamples;
                        const bool hitMinClose = p.closeWhenMin && p.recPos >= minSamples;
                        bool hitMaster = false;
                        if (auto* m = masterPhrase())
                            hitMaster = m->length > 0 && p.recPos >= m->length;

                        if (hitMax || hitMinClose || hitMaster)
                            closeRecord(i);
                    }
                    break;
                }

                case State::Playing:
                case State::Overdubbing:
                {
                    if (p.length <= 0)
                        break;

                    const float g = p.level;
                    mixWrapped(p.committed, p.length, p.playhead, g, mixL, mixR);

                    if (p.state == State::Overdubbing)
                    {
                        mixWrapped(p.pending, p.length, p.playhead, g, mixL, mixR);
                        p.pending.addSample(0, p.playhead, liveL);
                        p.pending.addSample(1, p.playhead, liveR);
                    }

                    p.playhead = (p.playhead + 1) % p.length;
                    break;
                }

                case State::Empty:
                case State::Armed:
                case State::Stopped:
                    break;
            }
        }

        left[s] += mixL;
        right[s] += mixR;
        phase += 1.0;
    }

    publish();
}

void LooperEngine::mixWrapped(const juce::AudioBuffer<float>& buf, int length, int playhead,
                              float gain, float& outL, float& outR) const
{
    const int fade = juce::jmin(fadeN, length / 4);
    float seam = 1.0f;
    if (fade > 0)
    {
        if (playhead < fade)
            seam = std::sin(((float) playhead / (float) fade) * juce::MathConstants<float>::halfPi);
        else if (playhead >= length - fade)
            seam = std::cos(((float) (playhead - (length - fade)) / (float) fade)
                            * juce::MathConstants<float>::halfPi);
    }

    const float g = gain * seam;
    outL += buf.getSample(0, playhead) * g;
    outR += buf.getSample(1, playhead) * g;
}

void LooperEngine::applyCommand(int index, Cmd cmd)
{
    auto& p = phrases[index];

    switch (cmd)
    {
        case Cmd::Tap:
            switch (p.state)
            {
                case State::Empty:
                    if (quantize.load(std::memory_order_relaxed) && masterPhrase() == nullptr)
                        armRecord(index);
                    else
                        startRecord(index);
                    break;
                case State::Armed:
                    p.closeAfterStart = true;
                    break;
                case State::Recording:
                    if (p.alignedRecord || p.protectSamples > 0)
                        break;
                    requestClose(index);
                    break;
                case State::Playing:
                    startOverdub(index);
                    break;
                case State::Overdubbing:
                    commitOverdub(index);
                    p.state = State::Playing;
                    break;
                case State::Stopped:
                    beginPlay(index);
                    break;
            }
            break;

        case Cmd::DoubleTap:
            if (p.state == State::Armed || p.state == State::Recording)
                clearPhrase(index);
            else if (p.state == State::Playing || p.state == State::Overdubbing)
                stopPhrase(index, false);
            break;

        case Cmd::Hold:
            if (p.state == State::Empty)
                break;
            if (p.state == State::Armed || p.state == State::Recording || p.state == State::Stopped)
                clearPhrase(index);
            else
                undoRedo(index);
            break;

        case Cmd::Stop:
            if (p.state == State::Armed || p.state == State::Recording)
                clearPhrase(index);
            else if (p.state == State::Playing || p.state == State::Overdubbing)
                stopPhrase(index, p.state == State::Overdubbing);
            break;

        case Cmd::None:
            break;
    }
}

void LooperEngine::armRecord(int index)
{
    auto& p = phrases[index];
    p.pending.clear();
    p.undoLayer.clear();
    p.committed.clear();
    p.hasUndoLayer = false;
    p.undone = false;
    p.closeWhenMin = false;
    p.closeOnBeat = false;
    p.closeAfterStart = false;
    p.alignedRecord = false;
    p.playOnMasterZero = false;
    p.recPos = 0;
    p.recorded = 0;
    p.playhead = 0;
    p.length = 0;
    p.protectSamples = 0;
    p.state = State::Armed;
}

void LooperEngine::requestClose(int index)
{
    auto& p = phrases[index];
    if (quantize.load(std::memory_order_relaxed) && ! p.alignedRecord && currentSpb > 0.0)
    {
        const int n = (int) std::floor((double) p.recPos / currentSpb);
        const double frac = (double) p.recPos / currentSpb - (double) n;
        if (n >= 1 && p.recPos >= minSamples && frac <= 0.10)
        {
            p.recPos = juce::jlimit(minSamples, p.recPos, (int) std::lround((double) n * currentSpb));
            closeRecord(index);
            return;
        }

        p.closeOnBeat = true;
        return;
    }

    if (p.recPos < minSamples)
        p.closeWhenMin = true;
    else
        closeRecord(index);
}

void LooperEngine::startRecord(int index)
{
    auto& p = phrases[index];
    p.pending.clear();
    p.undoLayer.clear();
    p.hasUndoLayer = false;
    p.undone = false;
    p.closeWhenMin = false;
    p.closeOnBeat = p.closeAfterStart;
    p.closeAfterStart = false;
    p.playOnMasterZero = false;
    p.recPos = 0;
    p.recorded = 0;
    p.playhead = 0;
    p.protectSamples = juce::jmax(1, (int) std::round(sampleRate * 0.25));

    if (auto* m = masterPhrase(); m != nullptr && m->length > 0)
    {
        p.length = m->length;
        p.alignedRecord = true;
        p.committed.clear();
        p.playhead = isActive(*m) ? m->playhead : 0;
    }
    else
    {
        p.length = 0;
        p.alignedRecord = false;
        p.committed.clear();
    }

    p.state = State::Recording;
}

void LooperEngine::closeRecord(int index)
{
    auto& p = phrases[index];
    if (p.state != State::Recording)
        return;

    if (! p.alignedRecord)
    {
        int samples = p.recPos;
        if (quantize.load(std::memory_order_relaxed) && currentSpb > 0.0)
        {
            const int n = juce::jmax(1, (int) std::lround((double) p.recPos / currentSpb));
            samples = (int) std::lround((double) n * currentSpb);
        }
        p.length = juce::jlimit(minSamples, maxSamples, samples);
        p.playhead = 0;
    }

    if (p.length <= 0)
    {
        clearPhrase(index);
        return;
    }

    if (master < 0)
        master = index;

    p.state = State::Playing;
}

void LooperEngine::startOverdub(int index)
{
    auto& p = phrases[index];
    if (p.length <= 0)
        return;
    p.pending.clear();
    p.state = State::Overdubbing;
}

void LooperEngine::commitOverdub(int index)
{
    auto& p = phrases[index];
    if (p.length <= 0)
        return;

    p.undoLayer.clear();
    for (int ch = 0; ch < 2; ++ch)
    {
        p.undoLayer.copyFrom(ch, 0, p.pending, ch, 0, p.length);
        p.committed.addFrom(ch, 0, p.pending, ch, 0, p.length);
    }
    p.pending.clear();
    p.hasUndoLayer = true;
    p.undone = false;
}

void LooperEngine::beginPlay(int index)
{
    auto& p = phrases[index];
    if (p.length <= 0)
        return;

    auto* m = masterPhrase();
    const bool wait = m != nullptr && m != &p && isActive(*m);
    if (wait)
    {
        p.playOnMasterZero = true;
        p.state = State::Stopped;
        return;
    }

    p.playhead = 0;
    p.playOnMasterZero = false;
    p.state = State::Playing;
}

void LooperEngine::stopPhrase(int index, bool commitPending)
{
    auto& p = phrases[index];
    if (commitPending && p.state == State::Overdubbing)
        commitOverdub(index);
    else if (p.state == State::Overdubbing)
        p.pending.clear();

    p.playOnMasterZero = false;
    p.state = p.length > 0 ? State::Stopped : State::Empty;
}

void LooperEngine::clearPhrase(int index)
{
    auto& p = phrases[index];
    p.committed.clear();
    p.pending.clear();
    p.undoLayer.clear();
    p.length = 0;
    p.playhead = 0;
    p.recPos = 0;
    p.recorded = 0;
    p.state = State::Empty;
    p.closeWhenMin = false;
    p.closeOnBeat = false;
    p.closeAfterStart = false;
    p.alignedRecord = false;
    p.hasUndoLayer = false;
    p.undone = false;
    p.playOnMasterZero = false;
    p.protectSamples = 0;
    if (master == index)
        reassignMaster();
}

void LooperEngine::undoRedo(int index)
{
    auto& p = phrases[index];
    if (! p.hasUndoLayer || p.length <= 0)
        return;

    const float gain = p.undone ? 1.0f : -1.0f;
    for (int ch = 0; ch < 2; ++ch)
        p.committed.addFrom(ch, 0, p.undoLayer, ch, 0, p.length, gain);
    p.undone = ! p.undone;
}

void LooperEngine::freezeSecondary()
{
    auto& p = phrases[1];
    if (p.state == State::Armed || p.state == State::Recording)
        clearPhrase(1);
    else if (p.state == State::Overdubbing)
        stopPhrase(1, false);
    else if (p.state == State::Playing)
        stopPhrase(1, false);
}

void LooperEngine::reassignMaster()
{
    master = -1;
    for (int i = 0; i < numPhrases; ++i)
        if (phrases[i].length > 0)
        {
            master = i;
            break;
        }
}

void LooperEngine::publish()
{
    for (int i = 0; i < numPhrases; ++i)
    {
        const auto& p = phrases[i];
        states[(size_t) i].store((int) p.state, std::memory_order_relaxed);
        undoFlags[(size_t) i].store(p.hasUndoLayer ? 1 : 0, std::memory_order_relaxed);
        contentFlags[(size_t) i].store(p.length > 0 ? 1 : 0, std::memory_order_relaxed);

        float head = 0.0f;
        if (p.state == State::Recording)
        {
            if (p.alignedRecord && p.length > 0)
                head = (float) p.playhead / (float) p.length;
            else if (currentSpb > 0.0 && quantize.load(std::memory_order_relaxed))
                head = std::fmod((float) ((double) p.recPos / currentSpb), 1.0f);
            else if (sampleRate > 0.0)
                head = std::fmod((float) p.recPos / (float) juce::jmax(1, (int) std::round(sampleRate * 2.0)), 1.0f);
        }
        else if (p.length > 0)
        {
            head = (float) p.playhead / (float) p.length;
        }
        heads[(size_t) i].store(head, std::memory_order_relaxed);
    }
}

bool LooperEngine::validIndex(int phrase) const
{
    return juce::isPositiveAndBelow(phrase, numPhrases);
}

bool LooperEngine::isActive(const Phrase& phrase) const
{
    return phrase.state == State::Playing || phrase.state == State::Overdubbing;
}

LooperEngine::Phrase* LooperEngine::masterPhrase()
{
    if (! validIndex(master) || phrases[master].length <= 0)
        return nullptr;
    return &phrases[master];
}
