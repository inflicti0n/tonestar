#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <cmath>

enum class VocalAxis { Grit = 0, Crush, Tight, Cut, Warm };

struct VocalStamp
{
    std::array<float, 5> axes {};
    std::array<float, 6> fx {};
    int root = 0;
    bool minor = false;
};

struct VocalParams
{
    float hpfHz = 90.0f;
    float gate = 0.08f;
    float deess = 0.22f;
    float leveler = 0.18f;
    float presenceDb = 1.2f;
    float sat = 0.08f;
    float crush = 0.12f;
    float lowMidDb = 0.0f;
    float cutDb = 0.6f;
    float warmDb = 0.4f;
    float airDb = 0.8f;
};

inline VocalParams composeVocal(const std::array<float, 5>& axes)
{
    const float grit = juce::jlimit(0.0f, 1.0f, axes[0]);
    const float crush = juce::jlimit(0.0f, 1.0f, axes[1]);
    const float tight = juce::jlimit(0.0f, 1.0f, axes[2]);
    const float cut = juce::jlimit(0.0f, 1.0f, axes[3]);
    const float warm = juce::jlimit(0.0f, 1.0f, axes[4]);

    VocalParams p;
    p.hpfHz = 80.0f + tight * 70.0f + grit * 15.0f;
    p.gate = 0.06f + tight * 0.16f;
    p.deess = 0.18f + cut * 0.35f + crush * 0.08f;
    p.leveler = 0.14f + crush * 0.55f;
    p.presenceDb = 0.8f + cut * 3.4f - warm * 0.8f;
    p.sat = 0.05f + grit * 0.72f;
    p.crush = 0.08f + crush * 0.75f;
    p.lowMidDb = warm * 2.8f - tight * 2.2f - cut * 0.6f;
    p.cutDb = cut * 3.6f;
    p.warmDb = warm * 2.4f - cut * 0.4f;
    p.airDb = 0.5f + cut * 2.2f - warm * 1.8f;
    return p;
}

inline const char* vocalAxisName(int index)
{
    static constexpr const char* names[] = { "Grit", "Crush", "Tight", "Cut", "Warm" };
    return names[juce::jlimit(0, 4, index)];
}

inline const char* vocalFxName(int index)
{
    static constexpr const char* names[] = { "Tune", "Double", "Echo", "Bloom", "Stack", "Phone" };
    return names[juce::jlimit(0, 5, index)];
}

inline const char* vocalFxTip(int index)
{
    static constexpr const char* tips[] = {
        "polish to hard tune",
        "fatten to a wide stack",
        "slap to a throw",
        "room to hall",
        "third to a small choir",
        "lo-fi to telephone"
    };
    return tips[juce::jlimit(0, 5, index)];
}
