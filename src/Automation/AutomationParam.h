#pragma once

#include "Appearance/Theme.h"
#include "Vocals/VocalCompose.h"

#include <cmath>
#include <cstring>

struct AutomationParam
{
    enum class Family { Axis, Fx, Key, Shift };

    static constexpr int count = 16;
    static constexpr int groupCount = 13;

    const char* id = "";
    const char* name = "";
    int group = 0;
    float min = 0.0f;
    float max = 1.0f;
    bool stepped = false;
    float (*get)(const VocalStamp&) = nullptr;
    void (*set)(VocalStamp&, float) = nullptr;

    float norm(float value) const
    {
        const float span = juce::jmax(1.0e-6f, max - min);
        return juce::jlimit(0.0f, 1.0f, (value - min) / span);
    }

    float fromNorm(float n) const
    {
        return juce::jlimit(min, max, min + juce::jlimit(0.0f, 1.0f, n) * (max - min));
    }

    float snap(float value) const
    {
        value = juce::jlimit(min, max, value);
        if (stepped)
            return std::round(value);
        if (max - min > 8.0f)
            return juce::jlimit(min, max, std::round(value * 4.0f) * 0.25f);
        return juce::jlimit(min, max, std::round(value * 20.0f) * 0.05f);
    }

    static const AutomationParam& at(int index)
    {
        return table()[juce::jlimit(0, count - 1, index)];
    }

    static int indexOf(const char* id)
    {
        for (int i = 0; i < count; ++i)
            if (std::strcmp(table()[i].id, id) == 0)
                return i;
        return -1;
    }

    static const char* groupName(int group)
    {
        static constexpr const char* names[] = {
            "Grit", "Crush", "Tight", "Cut", "Warm",
            "Tune", "Double", "Echo", "Bloom", "Stack", "Phone",
            "Key", "Shift"
        };
        return names[juce::jlimit(0, groupCount - 1, group)];
    }

    static Family groupFamily(int group)
    {
        if (group < 5)
            return Family::Axis;
        if (group < 11)
            return Family::Fx;
        if (group == 11)
            return Family::Key;
        return Family::Shift;
    }

    static juce::Colour groupColour(int group)
    {
        group = juce::jlimit(0, groupCount - 1, group);
        juce::Colour base = Theme::starlight();
        switch (groupFamily(group))
        {
            case Family::Axis:  base = Theme::starlight(); break;
            case Family::Fx:    base = Theme::nova(); break;
            case Family::Key:   base = Theme::mist(); break;
            case Family::Shift: base = Theme::flare(); break;
        }
        const int local = group < 5 ? group : (group < 11 ? group - 5 : 0);
        static constexpr float hues[] = { 0.0f, 0.07f, -0.07f, 0.13f, -0.13f, 0.18f };
        return base.withRotatedHue(hues[local % 6]);
    }

    static juce::Colour paramColour(int index)
    {
        index = juce::jlimit(0, count - 1, index);
        const int group = table()[index].group;
        int rank = 0;
        for (int i = 0; i < index; ++i)
            if (table()[i].group == group)
                ++rank;
        auto colour = groupColour(group);
        if (rank > 0)
            colour = colour.withMultipliedAlpha(1.0f - 0.22f * (float) rank);
        return colour;
    }

    static bool drawsCurve(int index)
    {
        return std::strcmp(at(index).id, "shift.mode") != 0;
    }

    static juce::Colour shiftModeColour(int mode)
    {
        if (mode == 1)
            return Theme::nova();
        if (mode == 2)
            return Theme::flare();
        return Theme::starlight();
    }

    static const char* shiftModeName(int mode)
    {
        static constexpr const char* names[] = { "Transpose", "Robot", "Quantize" };
        return names[juce::jlimit(0, 2, mode)];
    }

    static juce::Colour curveColour(int index, const VocalStamp& stamp)
    {
        if (at(index).group == 12)
        {
            auto colour = shiftModeColour(stamp.shiftMode);
            if (std::strcmp(at(index).id, "shift.formant") == 0)
                colour = colour.withMultipliedAlpha(0.70f);
            return colour;
        }
        return paramColour(index);
    }

    static const AutomationParam* table()
    {
        static const AutomationParam rows[count] = {
            { "axis.grit",   "Grit",    0,  0.0f,  1.0f, false,
              [] (const VocalStamp& s) { return s.axes[0]; },
              [] (VocalStamp& s, float v) { s.axes[0] = juce::jlimit(0.0f, 1.0f, v); } },
            { "axis.crush",  "Crush",   1,  0.0f,  1.0f, false,
              [] (const VocalStamp& s) { return s.axes[1]; },
              [] (VocalStamp& s, float v) { s.axes[1] = juce::jlimit(0.0f, 1.0f, v); } },
            { "axis.tight",  "Tight",   2,  0.0f,  1.0f, false,
              [] (const VocalStamp& s) { return s.axes[2]; },
              [] (VocalStamp& s, float v) { s.axes[2] = juce::jlimit(0.0f, 1.0f, v); } },
            { "axis.cut",    "Cut",     3,  0.0f,  1.0f, false,
              [] (const VocalStamp& s) { return s.axes[3]; },
              [] (VocalStamp& s, float v) { s.axes[3] = juce::jlimit(0.0f, 1.0f, v); } },
            { "axis.warm",   "Warm",    4,  0.0f,  1.0f, false,
              [] (const VocalStamp& s) { return s.axes[4]; },
              [] (VocalStamp& s, float v) { s.axes[4] = juce::jlimit(0.0f, 1.0f, v); } },
            { "fx.tune",     "Tune",    5,  0.0f,  1.0f, false,
              [] (const VocalStamp& s) { return s.fx[0]; },
              [] (VocalStamp& s, float v) { s.fx[0] = juce::jlimit(0.0f, 1.0f, v); } },
            { "fx.double",   "Double",  6,  0.0f,  1.0f, false,
              [] (const VocalStamp& s) { return s.fx[1]; },
              [] (VocalStamp& s, float v) { s.fx[1] = juce::jlimit(0.0f, 1.0f, v); } },
            { "fx.echo",     "Echo",    7,  0.0f,  1.0f, false,
              [] (const VocalStamp& s) { return s.fx[2]; },
              [] (VocalStamp& s, float v) { s.fx[2] = juce::jlimit(0.0f, 1.0f, v); } },
            { "fx.bloom",    "Bloom",   8,  0.0f,  1.0f, false,
              [] (const VocalStamp& s) { return s.fx[3]; },
              [] (VocalStamp& s, float v) { s.fx[3] = juce::jlimit(0.0f, 1.0f, v); } },
            { "fx.stack",    "Stack",   9,  0.0f,  1.0f, false,
              [] (const VocalStamp& s) { return s.fx[4]; },
              [] (VocalStamp& s, float v) { s.fx[4] = juce::jlimit(0.0f, 1.0f, v); } },
            { "fx.phone",    "Phone",  10,  0.0f,  1.0f, false,
              [] (const VocalStamp& s) { return s.fx[5]; },
              [] (VocalStamp& s, float v) { s.fx[5] = juce::jlimit(0.0f, 1.0f, v); } },
            { "key.root",    "Root",   11,  0.0f, 11.0f, true,
              [] (const VocalStamp& s) { return (float) s.root; },
              [] (VocalStamp& s, float v) { s.root = ((int) std::lround(v) % 12 + 12) % 12; } },
            { "key.minor",   "Minor",  11,  0.0f,  1.0f, true,
              [] (const VocalStamp& s) { return s.minor ? 1.0f : 0.0f; },
              [] (VocalStamp& s, float v) { s.minor = v >= 0.5f; } },
            { "shift.pitch",   "Pitch",    12, -12.0f, 12.0f, false,
              [] (const VocalStamp& s) { return s.pitch; },
              [] (VocalStamp& s, float v) { s.pitch = juce::jlimit(-12.0f, 12.0f, v); } },
            { "shift.formant", "Formant",  12, -12.0f, 12.0f, false,
              [] (const VocalStamp& s) { return s.formant; },
              [] (VocalStamp& s, float v) { s.formant = juce::jlimit(-12.0f, 12.0f, v); } },
            { "shift.mode",    "Mode",     12,  0.0f,  2.0f, true,
              [] (const VocalStamp& s) { return (float) s.shiftMode; },
              [] (VocalStamp& s, float v) { s.shiftMode = juce::jlimit(0, 2, (int) std::lround(v)); } },
        };
        return rows;
    }
};

inline VocalStamp interpolateStamps(const VocalStamp& a, const VocalStamp& b, float t)
{
    VocalStamp out = a;
    t = juce::jlimit(0.0f, 1.0f, t);
    for (int i = 0; i < AutomationParam::count; ++i)
    {
        const auto& p = AutomationParam::at(i);
        if (p.stepped || p.get == nullptr || p.set == nullptr)
            continue;
        p.set(out, p.get(a) + (p.get(b) - p.get(a)) * t);
    }
    return out;
}

inline VocalStamp evaluateKeys(int count, const int* times, const VocalStamp* values, int sample)
{
    if (count <= 0 || times == nullptr || values == nullptr)
        return {};
    if (sample <= times[0] || count == 1)
        return values[0];
    if (sample >= times[count - 1])
        return values[count - 1];

    int i = 0;
    while (i + 1 < count && times[i + 1] <= sample)
        ++i;
    const int span = times[i + 1] - times[i];
    if (span <= 0)
        return values[i];
    const float t = (float) (sample - times[i]) / (float) span;
    return interpolateStamps(values[i], values[i + 1], t);
}

inline bool paramsDiffer(const VocalStamp& a, const VocalStamp& b, int param)
{
    const auto& spec = AutomationParam::at(param);
    return std::abs(spec.get(a) - spec.get(b)) > 1.0e-4f;
}

inline VocalStamp evaluatePinned(int count, const int* times, const VocalStamp* values,
                                 const uint32_t* pins, int sample)
{
    if (count <= 0 || times == nullptr || values == nullptr)
        return {};
    if (pins == nullptr)
        return evaluateKeys(count, times, values, sample);

    VocalStamp out = values[0];
    int tbuf[64];
    VocalStamp vbuf[64];
    for (int p = 0; p < AutomationParam::count; ++p)
    {
        const auto& spec = AutomationParam::at(p);
        int n = 0;
        for (int i = 0; i < count && n < 64; ++i)
        {
            if ((pins[i] & (1u << (uint32_t) p)) == 0)
                continue;
            tbuf[n] = times[i];
            vbuf[n] = values[i];
            ++n;
        }
        if (n <= 0)
        {
            spec.set(out, spec.get(values[0]));
            continue;
        }
        spec.set(out, spec.get(evaluateKeys(n, tbuf, vbuf, sample)));
    }
    return out;
}
