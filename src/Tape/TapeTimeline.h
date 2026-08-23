#pragma once

#include "Appearance/Theme.h"
#include <cmath>

struct TapeTimeline
{
    static constexpr float minPx = 8.0f;
    static constexpr float maxPx = 240.0f;
    static constexpr float defaultPx = 48.0f;
    static constexpr int waveLeft = 168;

    float pixelsPerBeat = defaultPx;
    int viewStart = 0;
    float bpm = 120.0f;
    double sampleRate = 48000.0;
    float width = 1.0f;
    juce::int64 navAt = 0;

    void sync(float waveW, float bpmToUse, double sr)
    {
        width = juce::jmax(1.0f, waveW);
        bpm = juce::jlimit(40.0f, 240.0f, bpmToUse);
        sampleRate = sr > 0.0 ? sr : 48000.0;
    }

    int samplesPerBeat() const
    {
        if (bpm <= 0.0f || sampleRate <= 0.0)
            return 1;
        return juce::jmax(1, (int) std::lround(sampleRate * 60.0 / (double) bpm));
    }

    int visibleSamples() const
    {
        return juce::jmax(1, (int) std::lround((double) width * (double) samplesPerBeat()
                                               / (double) juce::jmax(1.0f, pixelsPerBeat)));
    }

    float sampleToX(int sample, float waveX) const
    {
        return waveX + (float) (sample - viewStart) * pixelsPerBeat / (float) samplesPerBeat();
    }

    int xToSample(float x, float waveX) const
    {
        return juce::jmax(0, viewStart + (int) std::lround((double) (x - waveX)
                                                           * (double) samplesPerBeat()
                                                           / (double) juce::jmax(1.0f, pixelsPerBeat)));
    }

    void markNav() { navAt = juce::Time::currentTimeMillis(); }

    bool isNavigating() const
    {
        return juce::Time::currentTimeMillis() - navAt < 500;
    }

    void zoomAt(float anchorX, float waveX, float factor)
    {
        const int anchor = xToSample(anchorX, waveX);
        pixelsPerBeat = juce::jlimit(minPx, maxPx, pixelsPerBeat * factor);
        viewStart = juce::jmax(0, anchor - (int) std::lround((double) (anchorX - waveX)
                                                             * (double) samplesPerBeat()
                                                             / (double) pixelsPerBeat));
        markNav();
    }

    void panSamples(int delta)
    {
        viewStart = juce::jmax(0, viewStart - delta);
        markNav();
    }

    void panByDrag(int startView, float startX, float nowX, float waveX)
    {
        const int delta = xToSample(startX, waveX) - xToSample(nowX, waveX);
        viewStart = juce::jmax(0, startView + delta);
        markNav();
    }

    void wheel(float x, float waveX, float deltaY, float deltaX, bool reversed, bool shift)
    {
        float delta = deltaY != 0.0f ? deltaY : deltaX;
        if (reversed)
            delta = -delta;
        if (delta == 0.0f)
            return;

        if (shift || std::abs(deltaX) > std::abs(deltaY) + 1.0e-4f)
        {
            panSamples((int) std::lround((double) delta * (double) visibleSamples() * 0.12));
            return;
        }

        zoomAt(x, waveX, juce::jlimit(0.5f, 2.0f, std::pow(1.15f, delta * 3.0f)));
    }

    void followPlayhead(int playhead)
    {
        if (isNavigating())
            return;

        const int vis = visibleSamples();
        if (playhead > viewStart + vis)
            viewStart = juce::jmax(0, playhead - (int) std::lround((double) vis * 0.85));
        else if (playhead < viewStart)
            viewStart = juce::jmax(0, playhead);
    }

    bool showQuarters() const { return pixelsPerBeat >= 8.0f; }
    bool showEighths() const { return pixelsPerBeat >= 24.0f; }

    void paintGrid(juce::Graphics& g, juce::Rectangle<float> wave) const
    {
        const int spb = samplesPerBeat();
        const int vis = visibleSamples();
        const int first = (int) std::floor((double) viewStart / (double) spb) - 1;
        const int last = (int) std::ceil((double) (viewStart + vis) / (double) spb) + 1;

        for (int b = first; b <= last; ++b)
        {
            if (b < 0)
                continue;

            const float x = sampleToX(b * spb, wave.getX());
            if (x < wave.getX() - 1.0f || x > wave.getRight() + 1.0f)
                continue;

            const bool bar = (b % 4) == 0;
            if (bar)
                g.setColour(Theme::starlight().withAlpha(0.32f));
            else if (showQuarters())
                g.setColour(Theme::mist().withAlpha(0.16f));
            else
                continue;

            g.fillRect(x, wave.getY(), 1.0f, wave.getHeight());
        }

        if (! showEighths())
            return;

        g.setColour(Theme::mist().withAlpha(0.10f));
        for (int b = first; b <= last; ++b)
        {
            if (b < 0)
                continue;
            const float x = sampleToX(b * spb + spb / 2, wave.getX());
            if (x < wave.getX() - 1.0f || x > wave.getRight() + 1.0f)
                continue;
            g.fillRect(x, wave.getY(), 1.0f, wave.getHeight());
        }
    }
};
