#pragma once

#include "Acoustics.h"
#include "AmpVoice.h"
#include "DebugLog.h"
#include "FxRack.h"
#include "LooperEngine.h"
#include "TapeEngine.h"
#include "ToneCompose.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>

class GuitarProcessor : public juce::AudioProcessor
{
public:
    static constexpr int numAxes = 6;

    GuitarProcessor();
    ~GuitarProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }

    const juce::String getName() const override { return "ToneStar"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}

    void setInputGainDb(float db) { inputGainDb.store(db, std::memory_order_relaxed); }
    void setOutputGainDb(float db) { outputGainDb.store(db, std::memory_order_relaxed); }
    void setMuted(bool shouldMute) { muted.store(shouldMute, std::memory_order_relaxed); }
    void setInputChannel(int channel) { inputChannel.store(channel, std::memory_order_relaxed); }
    void setAxis(int index, float value);
    void setAxes(const std::array<float, 6>& values);
    void setFx(int index, float value);
    void setFxAmounts(const std::array<float, 8>& values);
    void setBloomShimmer(bool shouldShimmer);
    void setCabSize(float value) { acoustics.setCabSize(value); }
    void setCabBack(float value) { acoustics.setCabBack(value); }
    void setBinaural(bool shouldUse) { acoustics.setBinaural(shouldUse); }
    void setDebugLog(DebugLog* log) { debugLog.store(log, std::memory_order_release); }
    void setMetroOn(bool shouldClick) { metroOn.store(shouldClick, std::memory_order_relaxed); }
    void setMetroBpm(float bpm) { metroBpm.store(juce::jlimit(40.0f, 240.0f, bpm), std::memory_order_relaxed); }
    bool startRecording();
    void stopRecording();
    bool isRecording() const { return tape.isRecording(); }
    TapeEngine& getTape() { return tape; }
    bool getMetroOn() const { return metroOn.load(std::memory_order_relaxed); }
    float getMetroBpm() const { return metroBpm.load(std::memory_order_relaxed); }
    float takeMetroPulse() { return metroPulse.exchange(0.0f, std::memory_order_relaxed); }
    LooperEngine& getLooper() { return looper; }

    float getInputGainDb() const { return inputGainDb.load(std::memory_order_relaxed); }
    float getOutputGainDb() const { return outputGainDb.load(std::memory_order_relaxed); }
    bool isMuted() const { return muted.load(std::memory_order_relaxed); }
    int getInputChannel() const { return inputChannel.load(std::memory_order_relaxed); }
    float getAxis(int index) const;
    std::array<float, 6> getAxes() const;
    float getFx(int index) const;
    std::array<float, 8> getFxAmounts() const;
    bool getBloomShimmer() const;
    float getCabSize() const { return acoustics.getCabSize(); }
    float getCabBack() const { return acoustics.getCabBack(); }
    bool getBinaural() const { return acoustics.getBinaural(); }
    float getCabMakeup() const { return acoustics.getCabMakeup(); }
    float getHrtfMakeup() const { return acoustics.getHrtfMakeup(); }
    float getStarMakeup() const { return engine.getMakeup(); }
    float getBloomWetScale() const { return fx.getBloomWetScale(); }
    float getEchoWetScale() const { return fx.getEchoWetScale(); }
    float getPeak() const { return peak.exchange(0.0f, std::memory_order_relaxed); }
    bool takeClip() { return clipped.exchange(false, std::memory_order_relaxed); }
    DebugLog::Snapshot captureDebugSnapshot() const;

private:

    std::atomic<float> inputGainDb { 0.0f };
    std::atomic<float> outputGainDb { 0.0f };
    std::atomic<bool> muted { false };
    std::atomic<int> inputChannel { 0 };
    std::array<std::atomic<float>, 6> axes;
    mutable std::atomic<float> peak { 0.0f };
    std::atomic<bool> clipped { false };

    juce::AudioBuffer<float> monoBuffer;
    juce::AudioBuffer<float> stereoBuffer;
    AmpEngine engine;
    FxRack fx;
    Acoustics acoustics;
    TapeEngine tape;
    LooperEngine looper;
    std::atomic<DebugLog*> debugLog { nullptr };
    std::atomic<bool> metroOn { false };
    std::atomic<float> metroBpm { 120.0f };
    std::atomic<float> metroPulse { 0.0f };
    double currentSampleRate = 48000.0;
    double metroSample = 0.0;
    int clickSample = 100000;
    int beatIndex = 3;
    int clickLength = 384;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GuitarProcessor)
};
