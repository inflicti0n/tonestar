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
        ring.assign(4096, 0.0f);
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
        if (hop >= 2048 && filled >= 2048)
        {
            hop = 0;
            if (pending.load(std::memory_order_relaxed) == 0)
            {
                const int start = write - 2048;
                for (int i = 0; i < 2048; ++i)
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
    void clear()
    {
        hzOut.store(0.0f, std::memory_order_relaxed);
        centsOut.store(0.0f, std::memory_order_relaxed);
        midiOut.store(0, std::memory_order_relaxed);
        voicedOut.store(0, std::memory_order_relaxed);
        hold = 0;
        smoothHz = 0.0f;
    }

    void miss()
    {
        if (hold > 0)
        {
            --hold;
            return;
        }
        voicedOut.store(0, std::memory_order_relaxed);
    }

    void detect()
    {
        constexpr int decim = 4;
        constexpr int rawN = 2048;
        constexpr int ds = rawN / decim;
        float energy = 0.0f;
        float peak = 0.0f;
        for (int i = 0; i < ds; ++i)
        {
            const int i0 = i * decim;
            const float s = 0.25f * (snap[(size_t) i0]
                                     + snap[(size_t) (i0 + 1)]
                                     + snap[(size_t) (i0 + 2)]
                                     + snap[(size_t) (i0 + 3)]);
            scratch[(size_t) i] = s;
            energy += s * s;
            peak = juce::jmax(peak, std::abs(s));
        }
        energy /= (float) ds;
        if (energy < 2.0e-8f && peak < 0.003f)
        {
            miss();
            return;
        }

        const float dsSr = (float) sampleRate / (float) decim;
        const int minLag = juce::jmax(2, (int) (dsSr / 1300.0f));
        const int maxLag = juce::jmin(ds / 2 - 1, (int) (dsSr / 60.0f));
        if (maxLag <= minLag)
        {
            miss();
            return;
        }

        float best = 0.0f;
        int bestLag = 0;
        for (int lag = minLag; lag < maxLag; lag += 2)
        {
            float acc = 0.0f;
            const int count = ds - lag;
            for (int i = 0; i < count; ++i)
                acc += scratch[(size_t) i] * scratch[(size_t) (i + lag)];
            acc /= (float) count;
            if (acc > best)
            {
                best = acc;
                bestLag = lag;
            }
        }

        for (int lag = juce::jmax(minLag, bestLag - 2); lag <= juce::jmin(maxLag - 1, bestLag + 2); ++lag)
        {
            float acc = 0.0f;
            const int count = ds - lag;
            for (int i = 0; i < count; ++i)
                acc += scratch[(size_t) i] * scratch[(size_t) (i + lag)];
            acc /= (float) count;
            if (acc > best)
            {
                best = acc;
                bestLag = lag;
            }
        }

        const float corr = energy > 1.0e-12f ? best / energy : 0.0f;
        if (bestLag <= 0 || corr < 0.12f)
        {
            miss();
            return;
        }

        float lag = (float) bestLag;
        if (bestLag > minLag && bestLag + 1 < maxLag)
        {
            float ym = 0.0f, y0 = 0.0f, yp = 0.0f;
            const int count = ds - bestLag;
            for (int i = 0; i < count; ++i)
            {
                ym += scratch[(size_t) i] * scratch[(size_t) (i + bestLag - 1)];
                y0 += scratch[(size_t) i] * scratch[(size_t) (i + bestLag)];
                yp += scratch[(size_t) i] * scratch[(size_t) (i + bestLag + 1)];
            }
            const float d = ym - 2.0f * y0 + yp;
            if (std::abs(d) > 1.0e-8f)
                lag += 0.5f * (ym - yp) / d;
        }

        const float hz = dsSr / juce::jmax(1.0f, lag);
        if (hz < 60.0f || hz > 1300.0f)
        {
            miss();
            return;
        }

        if (smoothHz > 1.0f)
            smoothHz += 0.28f * (hz - smoothHz);
        else
            smoothHz = hz;

        const float midi = 69.0f + 12.0f * std::log2(smoothHz / 440.0f);
        const int nearest = (int) std::lround(midi);
        hzOut.store(smoothHz, std::memory_order_relaxed);
        centsOut.store(juce::jlimit(-50.0f, 50.0f, (midi - (float) nearest) * 100.0f),
                       std::memory_order_relaxed);
        midiOut.store(nearest, std::memory_order_relaxed);
        voicedOut.store(1, std::memory_order_relaxed);
        hold = 18;
    }

    std::vector<float> ring;
    float snap[2048] {};
    float scratch[512] {};
    int write = 0;
    int filled = 0;
    int hop = 0;
    int hold = 0;
    float smoothHz = 0.0f;
    double sampleRate = 48000.0;
    std::atomic<int> armed { 0 };
    std::atomic<int> pending { 0 };
    std::atomic<float> hzOut { 0.0f };
    std::atomic<float> centsOut { 0.0f };
    std::atomic<int> midiOut { 0 };
    std::atomic<int> voicedOut { 0 };
};
