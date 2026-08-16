#pragma once

#include <juce_core/juce_core.h>
#include <atomic>
#include <cmath>
#include <vector>

struct TunerReading
{
    float hz = 0.0f;
    float cents = 0.0f;
    int midi = 0;
    bool voiced = false;
};

class Tuner
{
public:
    void prepare(double sr)
    {
        sampleRate = sr > 0.0 ? sr : 48000.0;
        ring.assign(kRing, 0.0f);
        write = 0;
        filled = 0;
        hop = 0;
        pending.store(0, std::memory_order_relaxed);
        clear();
    }

    void reset()
    {
        std::fill(ring.begin(), ring.end(), 0.0f);
        write = 0;
        filled = 0;
        hop = 0;
        pending.store(0, std::memory_order_relaxed);
        clear();
    }

    void setArmed(bool should)
    {
        armed.store(should, std::memory_order_relaxed);
        if (! should)
        {
            pending.store(0, std::memory_order_relaxed);
            clear();
        }
    }

    bool isArmed() const { return armed.load(std::memory_order_relaxed); }

    void push(const float* x, int n)
    {
        if (! armed.load(std::memory_order_relaxed) || x == nullptr || n <= 0 || ring.empty())
            return;

        const int mask = (int) ring.size() - 1;
        for (int i = 0; i < n; ++i)
        {
            ring[(size_t) (write & mask)] = x[i];
            ++write;
            if (filled < (int) ring.size())
                ++filled;
        }
        hop += n;
        if (hop >= kHop && filled >= kRaw)
        {
            hop = 0;
            if (pending.load(std::memory_order_relaxed) == 0)
            {
                const int start = write - kRaw;
                for (int i = 0; i < kRaw; ++i)
                    snap[(size_t) i] = ring[(size_t) ((start + i) & mask)];
                pending.store(1, std::memory_order_release);
            }
        }
    }

    void analyse()
    {
        if (pending.exchange(0, std::memory_order_acquire) != 0)
            detect();
    }

    TunerReading snapshot() const
    {
        TunerReading r;
        r.hz = hzOut.load(std::memory_order_relaxed);
        r.cents = centsOut.load(std::memory_order_relaxed);
        r.midi = midiOut.load(std::memory_order_relaxed);
        r.voiced = voicedOut.load(std::memory_order_relaxed) != 0;
        return r;
    }

    static juce::String noteLabel(int midi)
    {
        static constexpr const char* names[] = {
            "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"
        };
        const int note = ((midi % 12) + 12) % 12;
        const int oct = midi / 12 - 1;
        return juce::String(names[note]) + juce::String(oct);
    }

private:
    static constexpr int kRing = 16384;
    static constexpr int kRaw = 8192;
    static constexpr int kHop = 2048;
    static constexpr int kDecim = 4;
    static constexpr int kDs = kRaw / kDecim;
    static constexpr float kMinHz = 20.0f;
    static constexpr float kMaxHz = 1400.0f;
    static constexpr float kYinThresh = 0.15f;
    static constexpr int kLockAgree = 3;
    static constexpr int kUnlockNeed = 3;
    static constexpr int kHold = 20;

    void clear()
    {
        hzOut.store(0.0f, std::memory_order_relaxed);
        centsOut.store(0.0f, std::memory_order_relaxed);
        midiOut.store(0, std::memory_order_relaxed);
        voicedOut.store(0, std::memory_order_relaxed);
        hold = 0;
        lockedMidi = 0;
        agreeMidi = 0;
        agree = 0;
        unlockStreak = 0;
        smoothCents = 0.0f;
    }

    void miss()
    {
        if (hold > 0)
        {
            --hold;
            return;
        }
        lockedMidi = 0;
        agreeMidi = 0;
        agree = 0;
        unlockStreak = 0;
        smoothCents = 0.0f;
        voicedOut.store(0, std::memory_order_relaxed);
    }

    static float foldToLock(float hz, int lockMidi)
    {
        const float lockHz = 440.0f * std::pow(2.0f, ((float) lockMidi - 69.0f) / 12.0f);
        if (lockHz < 1.0f || hz < 1.0f)
            return hz;
        while (hz > lockHz * 1.9f)
            hz *= 0.5f;
        while (hz < lockHz * 0.53f)
            hz *= 2.0f;
        return hz;
    }

    void publish(int midi, float cents)
    {
        smoothCents += 0.10f * (cents - smoothCents);
        const float shown = juce::jlimit(-50.0f, 50.0f, smoothCents);
        const float hz = 440.0f * std::pow(2.0f, ((float) midi + shown / 100.0f - 69.0f) / 12.0f);
        hzOut.store(hz, std::memory_order_relaxed);
        centsOut.store(shown, std::memory_order_relaxed);
        midiOut.store(midi, std::memory_order_relaxed);
        voicedOut.store(1, std::memory_order_relaxed);
        hold = kHold;
    }

    void detect()
    {
        float energy = 0.0f;
        float peak = 0.0f;
        for (int i = 0; i < kDs; ++i)
        {
            const int i0 = i * kDecim;
            const float s = 0.25f * (snap[(size_t) i0]
                                     + snap[(size_t) (i0 + 1)]
                                     + snap[(size_t) (i0 + 2)]
                                     + snap[(size_t) (i0 + 3)]);
            scratch[(size_t) i] = s;
            energy += s * s;
            peak = juce::jmax(peak, std::abs(s));
        }
        energy /= (float) kDs;
        const bool locked = lockedMidi > 0;
        if (locked)
        {
            if (energy < 1.5e-9f && peak < 0.0004f)
            {
                miss();
                return;
            }
        }
        else if (energy < 5.0e-8f && peak < 0.004f)
        {
            miss();
            return;
        }

        const float dsSr = (float) sampleRate / (float) kDecim;
        const int minLag = juce::jmax(2, (int) std::ceil(dsSr / kMaxHz));
        const int maxLag = juce::jmin(kDs / 2 - 1, (int) std::floor(dsSr / kMinHz));
        if (maxLag <= minLag)
        {
            miss();
            return;
        }

        yin[0] = 1.0f;
        for (int tau = 1; tau <= maxLag; ++tau)
        {
            float sum = 0.0f;
            const int count = kDs - tau;
            for (int j = 0; j < count; ++j)
            {
                const float d = scratch[(size_t) j] - scratch[(size_t) (j + tau)];
                sum += d * d;
            }
            yin[(size_t) tau] = sum;
        }

        float running = 0.0f;
        for (int tau = 1; tau <= maxLag; ++tau)
        {
            running += yin[(size_t) tau];
            yin[(size_t) tau] *= (float) tau / juce::jmax(1.0e-12f, running);
        }

        int tauEst = -1;
        for (int tau = minLag; tau < maxLag; ++tau)
        {
            if (yin[(size_t) tau] < kYinThresh)
            {
                while (tau + 1 <= maxLag && yin[(size_t) (tau + 1)] < yin[(size_t) tau])
                    ++tau;
                tauEst = tau;
                break;
            }
        }
        if (tauEst < 0)
        {
            if (locked)
            {
                hold = kHold;
                return;
            }
            miss();
            return;
        }

        float lag = (float) tauEst;
        if (tauEst > minLag && tauEst < maxLag)
        {
            const float s0 = yin[(size_t) (tauEst - 1)];
            const float s1 = yin[(size_t) tauEst];
            const float s2 = yin[(size_t) (tauEst + 1)];
            const float denom = 2.0f * (s0 - 2.0f * s1 + s2);
            if (std::abs(denom) > 1.0e-8f)
                lag += (s0 - s2) / denom;
        }

        float hz = dsSr / juce::jmax(1.0f, lag);
        if (hz < kMinHz || hz > kMaxHz)
        {
            if (locked)
            {
                hold = kHold;
                return;
            }
            miss();
            return;
        }

        if (lockedMidi > 0)
            hz = foldToLock(hz, lockedMidi);

        const float midiFloat = 69.0f + 12.0f * std::log2(hz / 440.0f);
        const int nearest = (int) std::lround(midiFloat);

        if (lockedMidi <= 0)
        {
            if (agreeMidi == nearest)
                ++agree;
            else
            {
                agree = 1;
                agreeMidi = nearest;
            }
            if (agree < kLockAgree)
                return;
            lockedMidi = nearest;
            smoothCents = juce::jlimit(-50.0f, 50.0f, (midiFloat - (float) lockedMidi) * 100.0f);
            publish(lockedMidi, smoothCents);
            return;
        }

        const float centsVsLock = (midiFloat - (float) lockedMidi) * 100.0f;
        if (std::abs(centsVsLock) <= 50.0f)
        {
            unlockStreak = 0;
            publish(lockedMidi, juce::jlimit(-50.0f, 50.0f, centsVsLock));
            return;
        }

        ++unlockStreak;
        if (unlockStreak < kUnlockNeed)
            return;

        lockedMidi = nearest;
        agree = kLockAgree;
        agreeMidi = nearest;
        unlockStreak = 0;
        smoothCents = juce::jlimit(-50.0f, 50.0f, (midiFloat - (float) lockedMidi) * 100.0f);
        publish(lockedMidi, smoothCents);
    }

    std::vector<float> ring;
    float snap[kRaw] {};
    float scratch[kDs] {};
    float yin[kDs / 2] {};
    int write = 0;
    int filled = 0;
    int hop = 0;
    int hold = 0;
    int lockedMidi = 0;
    int agreeMidi = 0;
    int agree = 0;
    int unlockStreak = 0;
    float smoothCents = 0.0f;
    double sampleRate = 48000.0;
    std::atomic<int> armed { 0 };
    std::atomic<int> pending { 0 };
    std::atomic<float> hzOut { 0.0f };
    std::atomic<float> centsOut { 0.0f };
    std::atomic<int> midiOut { 0 };
    std::atomic<int> voicedOut { 0 };
};
