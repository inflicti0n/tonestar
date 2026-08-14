#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <atomic>

class TakeRecorder : private juce::Thread
{
public:
    TakeRecorder() : Thread("takes") {}
    ~TakeRecorder() override { stop(); }

    static juce::File takesDirectory()
    {
        auto dir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                       .getChildFile("ToneStar")
                       .getChildFile("takes");
        dir.createDirectory();
        return dir;
    }

    bool start(double sampleRateToUse)
    {
        stop();

        const auto dir = takesDirectory();
        if (! dir.isDirectory())
            return false;

        const auto file = dir.getChildFile(juce::Time::getCurrentTime().formatted("%Y%m%d-%H%M%S") + ".wav");
        std::unique_ptr<juce::OutputStream> stream(file.createOutputStream());
        if (stream == nullptr)
            return false;

        juce::WavAudioFormat wav;
        const auto options = juce::AudioFormatWriterOptions{}
                                 .withSampleRate(sampleRateToUse)
                                 .withNumChannels(2)
                                 .withBitsPerSample(32);
        writer = wav.createWriterFor(stream, options);
        if (writer == nullptr)
            return false;

        fifo.reset();
        recording.store(true, std::memory_order_release);
        startThread(juce::Thread::Priority::low);
        return true;
    }

    void stop()
    {
        recording.store(false, std::memory_order_release);
        signalThreadShouldExit();
        notify();
        stopThread(4000);
        drain();
        writer.reset();
    }

    bool isRecording() const { return recording.load(std::memory_order_acquire); }

    void push(const juce::AudioBuffer<float>& src, int numSamples)
    {
        if (! recording.load(std::memory_order_acquire) || numSamples <= 0)
            return;

        int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
        fifo.prepareToWrite(numSamples, start1, size1, start2, size2);
        if (size1 + size2 < numSamples)
        {
            fifo.finishedWrite(0);
            return;
        }

        copyChunk(src, 0, start1, size1);
        copyChunk(src, size1, start2, size2);
        fifo.finishedWrite(size1 + size2);
        notify();
    }

private:
    void run() override
    {
        while (! threadShouldExit())
        {
            drain();
            wait(20);
        }
        drain();
    }

    void copyChunk(const juce::AudioBuffer<float>& src, int srcOffset, int dest, int count)
    {
        if (count <= 0)
            return;

        for (int ch = 0; ch < 2; ++ch)
            fifoBuf.copyFrom(ch, dest, src, juce::jmin(ch, src.getNumChannels() - 1), srcOffset, count);
    }

    void drain()
    {
        if (writer == nullptr)
            return;

        while (true)
        {
            int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
            fifo.prepareToRead(2048, start1, size1, start2, size2);
            if (size1 + size2 <= 0)
            {
                fifo.finishedRead(0);
                break;
            }

            if (size1 > 0)
                writer->writeFromAudioSampleBuffer(fifoBuf, start1, size1);
            if (size2 > 0)
                writer->writeFromAudioSampleBuffer(fifoBuf, start2, size2);
            fifo.finishedRead(size1 + size2);
        }
    }

    static constexpr int fifoSamples = 48000 * 2;
    juce::AbstractFifo fifo { fifoSamples };
    juce::AudioBuffer<float> fifoBuf { 2, fifoSamples };
    std::unique_ptr<juce::AudioFormatWriter> writer;
    std::atomic<bool> recording { false };
};
