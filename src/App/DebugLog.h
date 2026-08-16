#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <memory>

class DebugLog
{
public:
    static constexpr int numStages = 6;
    static constexpr const char* stageName[numStages] = {
        "in", "pre", "star", "post", "cab", "out"
    };

    struct Snapshot
    {
        std::array<float, 6> axes {};
        std::array<float, 8> fx {};
        float inDb = 0.0f;
        float outDb = 0.0f;
        float cabSize = 0.15f;
        float cabBack = 0.25f;
        float cabMakeup = 1.0f;
        float hrtfMakeup = 1.0f;
        float starMakeup = 1.0f;
        float bloomWetScale = 1.0f;
        float echoWetScale = 1.0f;
        bool shimmer = false;
        bool binaural = false;
        bool muted = false;
    };

    struct Header
    {
        double sampleRate = 48000.0;
        int blockSize = 128;
        juce::String device;
        juce::String slug;
        Snapshot patch;
    };

    static juce::File logFile()
    {
        return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
            .getChildFile("ToneStar")
            .getChildFile("debug-session.txt");
    }

    static float toDb(float linear)
    {
        return 20.0f * std::log10(juce::jmax(linear, 1.0e-9f));
    }

    bool isActive() const { return active.load(std::memory_order_relaxed); }

    void start(const Header& header)
    {
        finish();

        auto file = logFile();
        file.getParentDirectory().createDirectory();
        stream = std::make_unique<juce::FileOutputStream>(file);
        if (stream == nullptr || ! stream->openedOk())
        {
            stream.reset();
            return;
        }

        stream->setPosition(0);
        stream->truncate();

        startMs = juce::Time::currentTimeMillis();
        lastFlushMs = startMs;
        writtenHeader = true;

        resetWindow();
        for (int i = 0; i < numStages; ++i)
        {
            sessionPeak[(size_t) i].store(0.0f, std::memory_order_relaxed);
            hotCount[(size_t) i].store(0, std::memory_order_relaxed);
            clipCount[(size_t) i].store(0, std::memory_order_relaxed);
        }
        eventWrite.store(0, std::memory_order_relaxed);
        eventRead.store(0, std::memory_order_relaxed);
        droppedEvents.store(0, std::memory_order_relaxed);

        juce::String text;
        text << "# ToneStar debug\n";
        text << "started " << juce::Time::getCurrentTime().toISO8601(true) << "\n";
        text << "sr=" << juce::String(header.sampleRate, 1)
             << " block=" << header.blockSize << "\n";
        text << "device " << (header.device.isNotEmpty() ? header.device : juce::String("-")) << "\n";
        text << "slug " << header.slug << "\n";
        appendPatch(text, header.patch);
        text << "\n# windows every ~200 ms, peaks and RMS in dBFS\n";
        text << "t_ms  in  inRms  pre  preRms  star  starRms  post  postRms  cab  cabRms  out  outRms\n";
        stream->writeText(text, false, false, nullptr);
        stream->flush();

        active.store(true, std::memory_order_release);
    }

    void noteBlock(const float peaks[numStages], const float rms[numStages], int numSamples,
                   const Snapshot& patch)
    {
        if (! active.load(std::memory_order_acquire) || numSamples <= 0)
            return;

        int mask = 0;
        for (int i = 0; i < numStages; ++i)
        {
            const float p = peaks[i];
            const float r = rms[i];
            if (! std::isfinite(p) || ! std::isfinite(r))
                mask |= (1 << i) | 0x100;
            else
            {
                if (p >= 0.9f)
                    mask |= (1 << i);
                if (p >= 1.0f)
                    mask |= (1 << (i + 8));
            }

            updateMax(windowPeak[(size_t) i], p);
            addEnergy(windowEnergy[(size_t) i], (double) r * (double) r * (double) numSamples);
            updateMax(sessionPeak[(size_t) i], p);
        }

        windowSamples.fetch_add(numSamples, std::memory_order_relaxed);

        if (mask != 0)
        {
            for (int i = 0; i < numStages; ++i)
            {
                if ((mask & (1 << i)) != 0)
                    hotCount[(size_t) i].fetch_add(1, std::memory_order_relaxed);
                if ((mask & (1 << (i + 8))) != 0)
                    clipCount[(size_t) i].fetch_add(1, std::memory_order_relaxed);
            }
            pushEvent(peaks, patch, mask);
        }
    }

    void flushWindow(bool force = false)
    {
        if (stream == nullptr)
            return;
        if (! force && ! active.load(std::memory_order_acquire))
            return;

        drainEvents();

        const auto now = juce::Time::currentTimeMillis();
        if (! force && now - lastFlushMs < 200)
            return;

        const int n = windowSamples.exchange(0, std::memory_order_relaxed);
        lastFlushMs = now;
        if (n <= 0)
            return;

        float peaks[numStages] {};
        float rms[numStages] {};
        for (int i = 0; i < numStages; ++i)
        {
            peaks[i] = windowPeak[(size_t) i].exchange(0.0f, std::memory_order_relaxed);
            const double energy = windowEnergy[(size_t) i].exchange(0.0, std::memory_order_relaxed);
            rms[i] = (float) std::sqrt(juce::jmax(0.0, energy) / (double) n);
        }

        juce::String line;
        line << (int) (now - startMs);
        for (int i = 0; i < numStages; ++i)
            line << "  " << fmtDb(peaks[i]) << "  " << fmtDb(rms[i]);
        line << "\n";
        stream->writeText(line, false, false, nullptr);
    }

    void finish()
    {
        if (! active.exchange(false, std::memory_order_acq_rel) && stream == nullptr)
            return;

        flushWindow(true);
        drainEvents();

        if (stream != nullptr)
        {
            juce::String text;
            text << "\n# summary\n";
            text << "duration_ms=" << (int) (juce::Time::currentTimeMillis() - startMs) << "\n";
            for (int i = 0; i < numStages; ++i)
            {
                text << stageName[i]
                     << "  max=" << fmtDb(sessionPeak[(size_t) i].load(std::memory_order_relaxed))
                     << "  hot=" << hotCount[(size_t) i].load(std::memory_order_relaxed)
                     << "  clip=" << clipCount[(size_t) i].load(std::memory_order_relaxed)
                     << "\n";
            }
            const int dropped = droppedEvents.load(std::memory_order_relaxed);
            if (dropped > 0)
                text << "dropped_events=" << dropped << "\n";
            text << "ended " << juce::Time::getCurrentTime().toISO8601(true) << "\n";
            stream->writeText(text, false, false, nullptr);
            stream->flush();
            stream.reset();
        }

        writtenHeader = false;
    }

private:
    struct Event
    {
        float peaks[numStages] {};
        Snapshot patch;
        int mask = 0;
        int tMs = 0;
        std::atomic<bool> ready { false };
    };

    static void updateMax(std::atomic<float>& slot, float value)
    {
        float current = slot.load(std::memory_order_relaxed);
        while (value > current
               && ! slot.compare_exchange_weak(current, value, std::memory_order_relaxed))
        {
        }
    }

    static void addEnergy(std::atomic<double>& slot, double value)
    {
        double current = slot.load(std::memory_order_relaxed);
        while (! slot.compare_exchange_weak(current, current + value, std::memory_order_relaxed))
        {
        }
    }

    static juce::String fmtDb(float linear)
    {
        if (! std::isfinite(linear))
            return "nan";
        return juce::String(toDb(linear), 1);
    }

    static void appendPatch(juce::String& text, const Snapshot& patch)
    {
        text << "inDb=" << juce::String(patch.inDb, 1)
             << " outDb=" << juce::String(patch.outDb, 1)
             << " mute=" << (int) patch.muted << "\n";
        text << "star";
        static constexpr const char* axes[] = { "Clean", "Crunch", "Heavy", "Tight", "Cut", "Warm" };
        for (int i = 0; i < 6; ++i)
            text << " " << axes[i] << "=" << juce::String(patch.axes[(size_t) i], 3);
        text << "\nfx";
        static constexpr const char* jobs[] = {
            "Squeeze", "Talk", "Shift", "Echo", "Bloom", "Width", "Sweep", "Pulse"
        };
        for (int i = 0; i < 8; ++i)
            text << " " << jobs[i] << "=" << juce::String(patch.fx[(size_t) i], 3);
        text << " shimmer=" << (int) patch.shimmer << "\n";
        text << "cab size=" << juce::String(patch.cabSize, 3)
             << " back=" << juce::String(patch.cabBack, 3)
             << " binaural=" << (int) patch.binaural
             << " makeup=" << juce::String(patch.cabMakeup, 3)
             << " hrtf=" << juce::String(patch.hrtfMakeup, 3)
             << " starMakeup=" << juce::String(patch.starMakeup, 3)
             << " bloomWet=" << juce::String(patch.bloomWetScale, 3)
             << " echoWet=" << juce::String(patch.echoWetScale, 3) << "\n";
    }

    void resetWindow()
    {
        for (int i = 0; i < numStages; ++i)
        {
            windowPeak[(size_t) i].store(0.0f, std::memory_order_relaxed);
            windowEnergy[(size_t) i].store(0.0, std::memory_order_relaxed);
        }
        windowSamples.store(0, std::memory_order_relaxed);
    }

    void pushEvent(const float peaks[numStages], const Snapshot& patch, int mask)
    {
        const uint32_t write = eventWrite.load(std::memory_order_relaxed);
        const uint32_t read = eventRead.load(std::memory_order_acquire);
        if (write - read >= (uint32_t) eventCap)
        {
            droppedEvents.fetch_add(1, std::memory_order_relaxed);
            return;
        }

        auto& slot = events[write % (uint32_t) eventCap];
        if (slot.ready.load(std::memory_order_acquire))
        {
            droppedEvents.fetch_add(1, std::memory_order_relaxed);
            return;
        }

        for (int i = 0; i < numStages; ++i)
            slot.peaks[i] = peaks[i];
        slot.patch = patch;
        slot.mask = mask;
        slot.tMs = (int) (juce::Time::currentTimeMillis() - startMs);
        slot.ready.store(true, std::memory_order_release);
        eventWrite.store(write + 1, std::memory_order_release);
    }

    void drainEvents()
    {
        if (stream == nullptr)
            return;

        while (true)
        {
            const uint32_t read = eventRead.load(std::memory_order_relaxed);
            const uint32_t write = eventWrite.load(std::memory_order_acquire);
            if (read == write)
                break;

            auto& slot = events[read % (uint32_t) eventCap];
            if (! slot.ready.load(std::memory_order_acquire))
                break;

            juce::String line;
            line << "EVENT t_ms=" << slot.tMs << " stages=";
            bool first = true;
            for (int i = 0; i < numStages; ++i)
            {
                if ((slot.mask & (1 << i)) == 0 && (slot.mask & (1 << (i + 8))) == 0)
                    continue;
                if (! first)
                    line << ",";
                first = false;
                line << stageName[i];
                if ((slot.mask & (1 << (i + 8))) != 0)
                    line << "*";
            }
            if ((slot.mask & 0x100) != 0)
                line << (first ? "nan" : ",nan");
            line << " peaks";
            for (int i = 0; i < numStages; ++i)
                line << " " << stageName[i] << "=" << fmtDb(slot.peaks[i]);
            line << "\n";
            appendPatch(line, slot.patch);
            stream->writeText(line, false, false, nullptr);

            slot.ready.store(false, std::memory_order_release);
            eventRead.store(read + 1, std::memory_order_release);
        }
    }

    std::atomic<bool> active { false };
    std::array<std::atomic<float>, numStages> windowPeak {};
    std::array<std::atomic<double>, numStages> windowEnergy {};
    std::atomic<int> windowSamples { 0 };
    std::array<std::atomic<float>, numStages> sessionPeak {};
    std::array<std::atomic<int>, numStages> hotCount {};
    std::array<std::atomic<int>, numStages> clipCount {};

    static constexpr int eventCap = 64;
    Event events[eventCap];
    std::atomic<uint32_t> eventWrite { 0 };
    std::atomic<uint32_t> eventRead { 0 };
    std::atomic<int> droppedEvents { 0 };

    std::unique_ptr<juce::FileOutputStream> stream;
    juce::int64 startMs = 0;
    juce::int64 lastFlushMs = 0;
    bool writtenHeader = false;
};
