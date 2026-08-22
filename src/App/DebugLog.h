#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>

class DebugLog
{
public:
    static constexpr int numStages = 6;
    static constexpr const char* stageName[numStages] = {
        "in", "pre", "star", "post", "cab", "out"
    };
    static constexpr int maskNan = 0x100;
    static constexpr int maskXrun = 0x200;
    static constexpr int maskNOver = 0x400;
    static constexpr int maskVocal = 0x800;

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
        bool vocals = false;
        std::array<float, 6> vocalFx {};
        int vocalRoot = 0;
        bool vocalMinor = false;
    };

    struct VocalTrace
    {
        int dblWraps = 0;
        int stackWraps = 0;
        int harmWraps = 0;
        int harmStarts = 0;
        int harmBlocked = 0;
        int tuneResets = 0;
        int hzJumps = 0;
        int lockLost = 0;
        int lockGained = 0;
        float hz = 0.0f;
        bool locked = false;
    };

    struct Header
    {
        double sampleRate = 48000.0;
        int blockSize = 128;
        int preparedMax = 2048;
        juce::String deviceType;
        juce::String device;
        int activeIns = 0;
        int activeOuts = 0;
        int selectedInput = 1;
        juce::String cpu;
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
    int sessionXruns() const { return sessionXrunCount.load(std::memory_order_relaxed); }

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
        sessionStackWraps.store(0, std::memory_order_relaxed);
        sessionDblWraps.store(0, std::memory_order_relaxed);
        sessionHarmWraps.store(0, std::memory_order_relaxed);
        sessionHarmBlocked.store(0, std::memory_order_relaxed);
        sessionHzJumps.store(0, std::memory_order_relaxed);
        sessionLockLost.store(0, std::memory_order_relaxed);
        sessionXrunCount.store(0, std::memory_order_relaxed);
        sessionNOverCount.store(0, std::memory_order_relaxed);
        sessionCbUsMax.store(0, std::memory_order_relaxed);
        sessionNMax.store(0, std::memory_order_relaxed);

        juce::String text;
        text << "# ToneStar debug\n";
        text << "started " << juce::Time::getCurrentTime().toISO8601(true) << "\n";
        text << "sr=" << juce::String(header.sampleRate, 1)
             << " block=" << header.blockSize
             << " maxBlock=" << header.preparedMax << "\n";
        text << "deviceType " << (header.deviceType.isNotEmpty() ? header.deviceType : juce::String("-")) << "\n";
        text << "device " << (header.device.isNotEmpty() ? header.device : juce::String("-")) << "\n";
        text << "io in=" << header.activeIns
             << " out=" << header.activeOuts
             << " selected=" << header.selectedInput << "\n";
        text << "cpu " << (header.cpu.isNotEmpty() ? header.cpu : juce::String("-")) << "\n";
        text << "slug " << header.slug << "\n";
        appendPatch(text, header.patch);
        text << "\n# windows every ~200 ms, peaks and RMS in dBFS\n";
        text << "t_ms  n_min  n_max  cb_us_avg  cb_us_max  budget_us  xruns  n_over"
             << "  in  inRms  pre  preRms  star  starRms  post  postRms  cab  cabRms  out  outRms"
             << "  hz  lock  dblW  stackW  harmW  harmStart  harmBlock  tuneReset  hzJump  lockLost  lockGain\n";
        stream->writeText(text, false, false, nullptr);
        stream->flush();

        active.store(true, std::memory_order_release);
    }

    void noteBlock(const float peaks[numStages], const float rms[numStages], int numSamples,
                   int cbUs, int preparedMax, double sampleRate, const Snapshot& patch)
    {
        if (! active.load(std::memory_order_acquire) || numSamples <= 0)
            return;

        int mask = 0;
        for (int i = 0; i < numStages; ++i)
        {
            const float p = peaks[i];
            const float r = rms[i];
            if (! std::isfinite(p) || ! std::isfinite(r))
                mask |= (1 << i) | maskNan;
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

        const int budgetUs = sampleRate > 0.0
                                 ? juce::jmax(1, (int) std::lround(1.0e6 * (double) numSamples / sampleRate))
                                 : 1;
        const bool xrun = cbUs > budgetUs;
        const bool nOver = numSamples > preparedMax;
        if (xrun)
        {
            mask |= maskXrun;
            windowXruns.fetch_add(1, std::memory_order_relaxed);
            sessionXrunCount.fetch_add(1, std::memory_order_relaxed);
        }
        if (nOver)
        {
            mask |= maskNOver;
            windowNOver.fetch_add(1, std::memory_order_relaxed);
            sessionNOverCount.fetch_add(1, std::memory_order_relaxed);
        }

        windowSamples.fetch_add(numSamples, std::memory_order_relaxed);
        windowBlocks.fetch_add(1, std::memory_order_relaxed);
        windowCbUsSum.fetch_add(cbUs, std::memory_order_relaxed);
        lastSampleRate.store(sampleRate, std::memory_order_relaxed);
        updateMaxInt(windowCbUsMax, cbUs);
        updateMaxInt(sessionCbUsMax, cbUs);
        updateMinInt(windowNMin, numSamples);
        updateMaxInt(windowNMax, numSamples);
        updateMaxInt(sessionNMax, numSamples);

        if (mask != 0)
        {
            for (int i = 0; i < numStages; ++i)
            {
                if ((mask & (1 << i)) != 0)
                    hotCount[(size_t) i].fetch_add(1, std::memory_order_relaxed);
                if ((mask & (1 << (i + 8))) != 0)
                    clipCount[(size_t) i].fetch_add(1, std::memory_order_relaxed);
            }
            pushEvent(peaks, patch, mask, cbUs, numSamples, budgetUs, preparedMax);
        }
    }

    void noteVocal(const VocalTrace& trace)
    {
        if (! active.load(std::memory_order_acquire))
            return;

        windowDblWraps.fetch_add(trace.dblWraps, std::memory_order_relaxed);
        windowStackWraps.fetch_add(trace.stackWraps, std::memory_order_relaxed);
        windowHarmWraps.fetch_add(trace.harmWraps, std::memory_order_relaxed);
        windowHarmStarts.fetch_add(trace.harmStarts, std::memory_order_relaxed);
        windowHarmBlocked.fetch_add(trace.harmBlocked, std::memory_order_relaxed);
        windowTuneResets.fetch_add(trace.tuneResets, std::memory_order_relaxed);
        windowHzJumps.fetch_add(trace.hzJumps, std::memory_order_relaxed);
        windowLockLost.fetch_add(trace.lockLost, std::memory_order_relaxed);
        windowLockGained.fetch_add(trace.lockGained, std::memory_order_relaxed);
        lastHz.store(trace.hz, std::memory_order_relaxed);
        lastLocked.store(trace.locked ? 1 : 0, std::memory_order_relaxed);

        sessionDblWraps.fetch_add(trace.dblWraps, std::memory_order_relaxed);
        sessionStackWraps.fetch_add(trace.stackWraps, std::memory_order_relaxed);
        sessionHarmWraps.fetch_add(trace.harmWraps, std::memory_order_relaxed);
        sessionHarmBlocked.fetch_add(trace.harmBlocked, std::memory_order_relaxed);
        sessionHzJumps.fetch_add(trace.hzJumps, std::memory_order_relaxed);
        sessionLockLost.fetch_add(trace.lockLost, std::memory_order_relaxed);

        const bool splice = trace.dblWraps > 0 || trace.stackWraps > 0
                            || trace.lockLost > 0 || trace.harmBlocked > 0;
        if (splice)
            pushVocalEvent(trace);
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
        const int blocks = windowBlocks.exchange(0, std::memory_order_relaxed);
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

        const int nMinRaw = windowNMin.exchange(std::numeric_limits<int>::max(), std::memory_order_relaxed);
        const int nMax = windowNMax.exchange(0, std::memory_order_relaxed);
        const int cbSum = windowCbUsSum.exchange(0, std::memory_order_relaxed);
        const int cbMax = windowCbUsMax.exchange(0, std::memory_order_relaxed);
        const int xruns = windowXruns.exchange(0, std::memory_order_relaxed);
        const int nOver = windowNOver.exchange(0, std::memory_order_relaxed);
        const int cbAvg = blocks > 0 ? cbSum / blocks : 0;
        const int nMin = nMinRaw == std::numeric_limits<int>::max() ? 0 : nMinRaw;
        const int typicalN = nMax > 0 ? nMax : nMin;
        const double sr = lastSampleRate.load(std::memory_order_relaxed);
        const int budgetUs = typicalN > 0 && sr > 0.0
                                 ? juce::jmax(1, (int) std::lround(1.0e6 * (double) typicalN / sr))
                                 : 0;

        juce::String line;
        line << (int) (now - startMs)
             << "  " << nMin
             << "  " << nMax
             << "  " << cbAvg
             << "  " << cbMax
             << "  " << budgetUs
             << "  " << xruns
             << "  " << nOver;
        for (int i = 0; i < numStages; ++i)
            line << "  " << fmtDb(peaks[i]) << "  " << fmtDb(rms[i]);
        line << "  " << juce::String(lastHz.load(std::memory_order_relaxed), 1)
             << "  " << lastLocked.load(std::memory_order_relaxed)
             << "  " << windowDblWraps.exchange(0, std::memory_order_relaxed)
             << "  " << windowStackWraps.exchange(0, std::memory_order_relaxed)
             << "  " << windowHarmWraps.exchange(0, std::memory_order_relaxed)
             << "  " << windowHarmStarts.exchange(0, std::memory_order_relaxed)
             << "  " << windowHarmBlocked.exchange(0, std::memory_order_relaxed)
             << "  " << windowTuneResets.exchange(0, std::memory_order_relaxed)
             << "  " << windowHzJumps.exchange(0, std::memory_order_relaxed)
             << "  " << windowLockLost.exchange(0, std::memory_order_relaxed)
             << "  " << windowLockGained.exchange(0, std::memory_order_relaxed);
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
            text << "xruns=" << sessionXrunCount.load(std::memory_order_relaxed)
                 << " n_over=" << sessionNOverCount.load(std::memory_order_relaxed)
                 << " cb_us_max=" << sessionCbUsMax.load(std::memory_order_relaxed)
                 << " n_max=" << sessionNMax.load(std::memory_order_relaxed) << "\n";
            for (int i = 0; i < numStages; ++i)
            {
                text << stageName[i]
                     << "  max=" << fmtDb(sessionPeak[(size_t) i].load(std::memory_order_relaxed))
                     << "  hot=" << hotCount[(size_t) i].load(std::memory_order_relaxed)
                     << "  clip=" << clipCount[(size_t) i].load(std::memory_order_relaxed)
                     << "\n";
            }
            text << "vocal_splices stackW=" << sessionStackWraps.load(std::memory_order_relaxed)
                 << " dblW=" << sessionDblWraps.load(std::memory_order_relaxed)
                 << " harmW=" << sessionHarmWraps.load(std::memory_order_relaxed)
                 << " harmBlock=" << sessionHarmBlocked.load(std::memory_order_relaxed)
                 << " hzJump=" << sessionHzJumps.load(std::memory_order_relaxed)
                 << " lockLost=" << sessionLockLost.load(std::memory_order_relaxed) << "\n";
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
        int cbUs = 0;
        int n = 0;
        int budgetUs = 0;
        int preparedMax = 0;
        VocalTrace vocal {};
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

    static void updateMaxInt(std::atomic<int>& slot, int value)
    {
        int current = slot.load(std::memory_order_relaxed);
        while (value > current
               && ! slot.compare_exchange_weak(current, value, std::memory_order_relaxed))
        {
        }
    }

    static void updateMinInt(std::atomic<int>& slot, int value)
    {
        int current = slot.load(std::memory_order_relaxed);
        while (value < current
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
            "Squeeze", "Talk", "Shift", "Echo", "Bloom", "Thicken", "Sweep", "Pulse"
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
        if (patch.vocals)
        {
            text << "vocal";
            static constexpr const char* jobs[] = {
                "Tune", "Double", "Echo", "Bloom", "Stack", "Phone"
            };
            for (int i = 0; i < 6; ++i)
                text << " " << jobs[i] << "=" << juce::String(patch.vocalFx[(size_t) i], 3);
            text << " root=" << patch.vocalRoot
                 << " minor=" << (int) patch.vocalMinor << "\n";
        }
    }

    void resetWindow()
    {
        for (int i = 0; i < numStages; ++i)
        {
            windowPeak[(size_t) i].store(0.0f, std::memory_order_relaxed);
            windowEnergy[(size_t) i].store(0.0, std::memory_order_relaxed);
        }
        windowSamples.store(0, std::memory_order_relaxed);
        windowBlocks.store(0, std::memory_order_relaxed);
        windowCbUsSum.store(0, std::memory_order_relaxed);
        windowCbUsMax.store(0, std::memory_order_relaxed);
        windowNMin.store(std::numeric_limits<int>::max(), std::memory_order_relaxed);
        windowNMax.store(0, std::memory_order_relaxed);
        windowXruns.store(0, std::memory_order_relaxed);
        windowNOver.store(0, std::memory_order_relaxed);
        windowDblWraps.store(0, std::memory_order_relaxed);
        windowStackWraps.store(0, std::memory_order_relaxed);
        windowHarmWraps.store(0, std::memory_order_relaxed);
        windowHarmStarts.store(0, std::memory_order_relaxed);
        windowHarmBlocked.store(0, std::memory_order_relaxed);
        windowTuneResets.store(0, std::memory_order_relaxed);
        windowHzJumps.store(0, std::memory_order_relaxed);
        windowLockLost.store(0, std::memory_order_relaxed);
        windowLockGained.store(0, std::memory_order_relaxed);
        lastHz.store(0.0f, std::memory_order_relaxed);
        lastLocked.store(0, std::memory_order_relaxed);
    }

    void pushEvent(const float peaks[numStages], const Snapshot& patch, int mask,
                   int cbUs, int n, int budgetUs, int preparedMax)
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
        slot.cbUs = cbUs;
        slot.n = n;
        slot.budgetUs = budgetUs;
        slot.preparedMax = preparedMax;
        slot.vocal = {};
        slot.ready.store(true, std::memory_order_release);
        eventWrite.store(write + 1, std::memory_order_release);
    }

    void pushVocalEvent(const VocalTrace& trace)
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

        slot.mask = maskVocal;
        slot.tMs = (int) (juce::Time::currentTimeMillis() - startMs);
        slot.vocal = trace;
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
            line << "EVENT t_ms=" << slot.tMs;
            if ((slot.mask & maskXrun) != 0)
                line << " xrun cb_us=" << slot.cbUs << " budget_us=" << slot.budgetUs;
            if ((slot.mask & maskNOver) != 0)
                line << " n_over n=" << slot.n << " prepared=" << slot.preparedMax;
            if ((slot.mask & maskVocal) != 0)
            {
                line << " vocal_splice hz=" << juce::String(slot.vocal.hz, 1)
                     << " lock=" << (int) slot.vocal.locked
                     << " stackW=" << slot.vocal.stackWraps
                     << " dblW=" << slot.vocal.dblWraps
                     << " harmW=" << slot.vocal.harmWraps
                     << " harmBlock=" << slot.vocal.harmBlocked
                     << " hzJump=" << slot.vocal.hzJumps
                     << " lockLost=" << slot.vocal.lockLost
                     << " lockGain=" << slot.vocal.lockGained
                     << "\n";
                stream->writeText(line, false, false, nullptr);
                slot.ready.store(false, std::memory_order_release);
                eventRead.store(read + 1, std::memory_order_release);
                continue;
            }
            line << " stages=";
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
            if ((slot.mask & maskNan) != 0)
                line << (first ? "nan" : ",nan");
            if (first && (slot.mask & maskNan) == 0)
                line << "-";
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
    std::atomic<int> windowBlocks { 0 };
    std::atomic<int> windowCbUsSum { 0 };
    std::atomic<int> windowCbUsMax { 0 };
    std::atomic<int> windowNMin { std::numeric_limits<int>::max() };
    std::atomic<int> windowNMax { 0 };
    std::atomic<int> windowXruns { 0 };
    std::atomic<int> windowNOver { 0 };
    std::atomic<int> windowDblWraps { 0 };
    std::atomic<int> windowStackWraps { 0 };
    std::atomic<int> windowHarmWraps { 0 };
    std::atomic<int> windowHarmStarts { 0 };
    std::atomic<int> windowHarmBlocked { 0 };
    std::atomic<int> windowTuneResets { 0 };
    std::atomic<int> windowHzJumps { 0 };
    std::atomic<int> windowLockLost { 0 };
    std::atomic<int> windowLockGained { 0 };
    std::atomic<float> lastHz { 0.0f };
    std::atomic<int> lastLocked { 0 };
    std::atomic<int> sessionDblWraps { 0 };
    std::atomic<int> sessionStackWraps { 0 };
    std::atomic<int> sessionHarmWraps { 0 };
    std::atomic<int> sessionHarmBlocked { 0 };
    std::atomic<int> sessionHzJumps { 0 };
    std::atomic<int> sessionLockLost { 0 };
    std::atomic<double> lastSampleRate { 48000.0 };
    std::array<std::atomic<float>, numStages> sessionPeak {};
    std::array<std::atomic<int>, numStages> hotCount {};
    std::array<std::atomic<int>, numStages> clipCount {};
    std::atomic<int> sessionXrunCount { 0 };
    std::atomic<int> sessionNOverCount { 0 };
    std::atomic<int> sessionCbUsMax { 0 };
    std::atomic<int> sessionNMax { 0 };

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
