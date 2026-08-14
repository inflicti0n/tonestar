#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>

class LooperEngine
{
public:
    static constexpr int numPhrases = 2;
    static constexpr double minSeconds = 0.25;
    static constexpr double maxSeconds = 180.0;
    static constexpr double fadeSeconds = 0.01;

    enum class State : int
    {
        Empty = 0,
        Armed,
        Recording,
        Playing,
        Overdubbing,
        Stopped
    };

    LooperEngine();
    void prepare(double sampleRateToUse, int samplesPerBlock);
    void reset();
    void process(juce::AudioBuffer<float>& stereo, int numSamples,
                 double metroPhase, double samplesPerBeat);

    void tap(int phrase);
    void doubleTap(int phrase);
    void hold(int phrase);
    void stop(int phrase);
    void setLevel(int phrase, float value);
    void setSimpleMode(bool shouldBeSimple);
    void setArmed(int phrase);
    void setQuantize(bool shouldQuantize);
    void setBpm(float bpm);
    bool isQuantize() const { return quantize.load(std::memory_order_relaxed); }

    bool isSimpleMode() const { return simpleMode.load(std::memory_order_relaxed); }
    int getArmed() const { return armed.load(std::memory_order_relaxed); }
    State getState(int phrase) const;
    float getPlayhead01(int phrase) const;
    bool hasUndo(int phrase) const;
    float getLevel(int phrase) const;
    bool hasContent(int phrase) const;

private:
    enum class Cmd : int { None = 0, Tap, DoubleTap, Hold, Stop };

    struct Phrase
    {
        juce::AudioBuffer<float> committed;
        juce::AudioBuffer<float> pending;
        juce::AudioBuffer<float> undoLayer;
        int length = 0;
        int playhead = 0;
        int recPos = 0;
        int recorded = 0;
        State state = State::Empty;
        bool closeWhenMin = false;
        bool closeOnBeat = false;
        bool closeAfterStart = false;
        bool alignedRecord = false;
        bool hasUndoLayer = false;
        bool undone = false;
        bool playOnMasterZero = false;
        int protectSamples = 0;
        float level = 1.0f;
    };

    void post(int phrase, Cmd cmd);
    void applyCommand(int index, Cmd cmd);
    void armRecord(int index);
    void startRecord(int index);
    void closeRecord(int index);
    void requestClose(int index);
    int beatLength() const;
    void startOverdub(int index);
    void commitOverdub(int index);
    void beginPlay(int index);
    void stopPhrase(int index, bool commitPending);
    void clearPhrase(int index);
    void undoRedo(int index);
    void freezeSecondary();
    void reassignMaster();
    void publish();
    void mixWrapped(const juce::AudioBuffer<float>& buf, int length, int playhead,
                    float gain, float& outL, float& outR) const;
    bool validIndex(int phrase) const;
    bool isActive(const Phrase& phrase) const;
    Phrase* masterPhrase();

    Phrase phrases[numPhrases];
    std::array<std::atomic<int>, numPhrases> commands {};
    std::array<std::atomic<float>, numPhrases> levels {};
    std::array<std::atomic<int>, numPhrases> states {};
    std::array<std::atomic<float>, numPhrases> heads {};
    std::array<std::atomic<int>, numPhrases> undoFlags {};
    std::array<std::atomic<int>, numPhrases> contentFlags {};

    std::atomic<bool> simpleMode { true };
    std::atomic<bool> quantize { false };
    std::atomic<float> bpm { 120.0f };
    std::atomic<int> armed { 0 };
    double currentSpb = 0.0;
    bool wasSimple = true;
    int master = -1;
    int maxSamples = 0;
    int minSamples = 0;
    int fadeN = 0;
    double sampleRate = 48000.0;
};
