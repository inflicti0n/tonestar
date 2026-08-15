#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <array>
#include <atomic>

class TapeEngine
{
public:
    static constexpr int numLanes = 8;
    static constexpr double maxSeconds = 240.0;
    static constexpr int hop = 512;
    static constexpr int maxHops = (int) (maxSeconds * 96000.0 / (double) hop) + 16;

    enum class Transport : int
    {
        Stopped = 0,
        Playing,
        Paused,
        Armed,
        Recording
    };

    TapeEngine();
    ~TapeEngine();

    void prepare(double sampleRateToUse, int samplesPerBlock);
    void reset();
    void halt();
    void shutdown();
    void process(juce::AudioBuffer<float>& stereo, int numSamples,
                 double metroPhase, double samplesPerBeat);

    void play();
    void pause();
    void stop();
    void record();
    void stopRecord();
    void setArmedLane(int lane);
    void setMute(int lane, bool shouldMute);
    void setLevel(int lane, float level);
    void setPan(int lane, float pan);
    void setName(int lane, const juce::String& name);
    void setStart(int lane, int startSample);
    void setEnd(int lane, int endSample);
    void trimLeft(int lane, int newTimelineStart);
    void setQuantize(bool shouldQuantize);
    void clearLane(int lane);
    void setPlayhead(int sample);
    void setTimelineView(float pixelsPerBeat, int viewStart);
    void setLoop(bool shouldLoop);
    void setLoopRange(int startSample, int endSample);

    Transport getTransport() const { return (Transport) transportPub.load(std::memory_order_relaxed); }
    bool isRecording() const;
    bool isArmed() const { return getTransport() == Transport::Armed; }
    bool isPlaying() const;
    bool isQuantize() const { return quantize.load(std::memory_order_relaxed); }
    bool isLoop() const { return loopOn.load(std::memory_order_relaxed); }
    int getLoopStart() const { return loopStart.load(std::memory_order_relaxed); }
    int getLoopEnd() const { return loopEnd.load(std::memory_order_relaxed); }
    int getPlayhead() const { return playheadPub.load(std::memory_order_relaxed); }
    int getArmedLane() const { return armedLane.load(std::memory_order_relaxed); }
    int getRecLane() const { return recLanePub.load(std::memory_order_relaxed); }
    int getRecFrames() const { return recFramesPub.load(std::memory_order_relaxed); }
    int getRecStart() const { return recStartPub.load(std::memory_order_relaxed); }
    int getViewSamples() const;
    float getPixelsPerBeat() const { return pixelsPerBeat; }
    int getViewStart() const { return viewStart; }
    double getSampleRate() const { return sampleRate; }
    int snapSample(int sample) const;
    juce::String getName(int lane) const;
    bool isMuted(int lane) const;
    float getLevel(int lane) const;
    float getPan(int lane) const;
    bool hasClip(int lane) const;

    struct LaneView
    {
        bool hasClip = false;
        bool mute = false;
        int start = 0;
        int in = 0;
        int end = 0;
        int fileFrames = 0;
        int hopCount = 0;
        const float* hops = nullptr;
    };

    LaneView getLane(int lane) const;
    bool takeDirty();

    bool exportMix(const juce::File& dest, double startSeconds, double lengthSeconds,
                   juce::String& error);

    void loadSession();
    void saveSession();

    static juce::File tapeDirectory();
    static juce::File laneFile(int lane);

private:
    enum class Cmd : int { None = 0, Play, Pause, Stop, Rec, RecStop };

    struct Lane
    {
        juce::AudioBuffer<float> audio;
        std::atomic<int> start { 0 };
        std::atomic<int> in { 0 };
        std::atomic<int> end { 0 };
        std::atomic<int> fileFrames { 0 };
        std::atomic<int> hasClip { 0 };
        std::atomic<int> mute { 0 };
        std::atomic<float> level { 1.0f };
        std::atomic<float> pan { 0.0f };
        std::atomic<int> hopCount { 0 };
        float hops[maxHops] {};
        juce::String name;
    };

    void post(Cmd cmd);
    void apply(Cmd cmd);
    void beginRecord();
    void requestClose();
    void finishRecord();
    void noteHop(Lane& lane, int pos, float peak);
    void mixLane(const Lane& lane, int head, float& outL, float& outR) const;
    bool valid(int lane) const;
    void ensureLane(int lane);
    int beatLength() const;
    int beatOrDefault() const;

    Lane lanes[numLanes];
    juce::AudioBuffer<float> guitar;

    std::atomic<int> command { (int) Cmd::None };
    std::atomic<int> transportPub { (int) Transport::Stopped };
    std::atomic<int> playheadPub { 0 };
    std::atomic<int> recLanePub { 0 };
    std::atomic<int> recFramesPub { 0 };
    std::atomic<int> recStartPub { 0 };
    std::atomic<int> armedLane { 0 };
    std::atomic<bool> quantize { false };
    std::atomic<bool> loopOn { false };
    std::atomic<int> loopStart { 0 };
    std::atomic<int> loopEnd { 0 };
    std::atomic<bool> metaDirty { false };
    std::atomic<bool> audioDirty { false };
    std::atomic<int> beatSamples { 0 };
    std::atomic<int> seekSample { 0 };
    std::atomic<bool> hasSeek { false };
    float pixelsPerBeat = 48.0f;
    int viewStart = 0;

    Transport transport = Transport::Stopped;
    int playhead = 0;
    int recLane = 0;
    int recFrames = 0;
    int recStart = 0;
    bool closeOnBeat = false;
    bool wasPlaying = false;
    double currentSpb = 0.0;
    int maxSamples = 0;
    int minSamples = 0;
    double sampleRate = 48000.0;
};
