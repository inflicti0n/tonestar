#pragma once

#include <juce_dsp/juce_dsp.h>
#include <algorithm>
#include <array>
#include <cmath>

// LUT v1: six star weights only. Pair/triple slots are reserved even when 0.
static constexpr int kLutVersion = 1;
static_assert(kLutVersion >= 1, "tone LUT version");
static constexpr int kNumAxes = 6;
static constexpr int kNumPairs = 15;
static constexpr int kNumTriples = 20;

enum class ToneAxis { Clean = 0, Crunch, Heavy, Tight, Cut, Warm };
enum class MixKind { Add, SoftOr, Lane };

struct AmpParams
{
    float drive1 = 1.05f;
    float drive2 = 1.0f;
    float drive3 = 1.0f;
    float stageMix = 1.0f;
    float hard = 0.0f;
    float sag = 0.0f;
    float hpfHz = 48.0f;
    float lowMidHz = 350.0f;
    float lowMidDb = 0.0f;
    float lowMidQ = 0.9f;
    float midHz = 800.0f;
    float midDb = 0.0f;
    float midQ = 0.85f;
    float presHz = 2000.0f;
    float presDb = 0.0f;
    float presQ = 0.8f;
    float fizzHz = 6500.0f;
    float fizzDb = 0.0f;
    float cabHz = 11000.0f;
    float gate = 0.0f;
    float makeup = 1.0f;
};

enum class LutParam
{
    Drive1, Drive2, Drive3, StageMix, Hard,
    Sag, Gate,
    HpfHz,
    LowMidHz, LowMidDb, LowMidQ,
    MidHz, MidDb, MidQ,
    PresHz, PresDb, PresQ,
    FizzHz, FizzDb,
    CabHz,
    Count
};

static constexpr int kNumLutParams = (int) LutParam::Count;

struct LutRow
{
    const char* name = "";
    float base = 0.0f;
    float axis[kNumAxes] {};
    float pair[kNumPairs] {};
    float triple[kNumTriples] {};
    MixKind mix = MixKind::Add;
    float lo = 0.0f;
    float hi = 1.0f;
};

inline int pairIndex(int i, int j)
{
    if (i > j)
        std::swap(i, j);
    int n = 0;
    for (int a = 0; a < kNumAxes; ++a)
        for (int b = a + 1; b < kNumAxes; ++b)
        {
            if (a == i && b == j)
                return n;
            ++n;
        }
    return 0;
}

inline int tripleIndex(int i, int j, int k)
{
    int ids[3] = { i, j, k };
    if (ids[0] > ids[1]) std::swap(ids[0], ids[1]);
    if (ids[1] > ids[2]) std::swap(ids[1], ids[2]);
    if (ids[0] > ids[1]) std::swap(ids[0], ids[1]);

    int n = 0;
    for (int a = 0; a < kNumAxes; ++a)
        for (int b = a + 1; b < kNumAxes; ++b)
            for (int c = b + 1; c < kNumAxes; ++c)
            {
                if (a == ids[0] && b == ids[1] && c == ids[2])
                    return n;
                ++n;
            }
    return 0;
}

inline LutRow makeRow(const char* name, float base,
                      std::array<float, 6> axis, MixKind mix, float lo, float hi)
{
    LutRow row;
    row.name = name;
    row.base = base;
    for (int i = 0; i < kNumAxes; ++i)
        row.axis[i] = axis[(size_t) i];
    row.mix = mix;
    row.lo = lo;
    row.hi = hi;
    return row;
}

inline void setPair(LutRow& row, ToneAxis a, ToneAxis b, float value)
{
    row.pair[pairIndex((int) a, (int) b)] = value;
}

inline void setTriple(LutRow& row, ToneAxis a, ToneAxis b, ToneAxis c, float value)
{
    row.triple[tripleIndex((int) a, (int) b, (int) c)] = value;
}

inline std::array<LutRow, kNumLutParams> buildToneLut()
{
    using A = ToneAxis;
    std::array<LutRow, kNumLutParams> t {};

    // Columns: Clean, Crunch, Heavy, Tight, Cut, Warm
    t[(int) LutParam::Drive1] = makeRow("drive1", 1.05f,
        { -0.35f, 8.45f, 16.45f, 0.0f, 0.0f, 4.95f }, MixKind::Add, 0.45f, 20.0f);
    setPair(t[(int) LutParam::Drive1], A::Clean, A::Crunch, -1.5f);
    setPair(t[(int) LutParam::Drive1], A::Clean, A::Heavy, -4.0f);
    setPair(t[(int) LutParam::Drive1], A::Crunch, A::Heavy, -3.0f);

    t[(int) LutParam::Drive2] = makeRow("drive2", 1.0f,
        { 0.0f, 2.8f, 9.8f, 0.0f, 0.0f, 2.2f }, MixKind::Add, 0.6f, 14.0f);
    setPair(t[(int) LutParam::Drive2], A::Crunch, A::Heavy, -1.5f);
    setPair(t[(int) LutParam::Drive2], A::Clean, A::Heavy, -1.2f);

    t[(int) LutParam::Drive3] = makeRow("drive3", 1.0f,
        { 0.0f, 0.0f, 6.2f, 0.0f, 0.0f, 0.0f }, MixKind::Add, 0.6f, 10.0f);
    setPair(t[(int) LutParam::Drive3], A::Clean, A::Heavy, -1.0f);

    t[(int) LutParam::StageMix] = makeRow("stageMix", 1.0f,
        { 0.0f, 1.0f, 2.0f, 0.0f, 0.0f, 1.0f }, MixKind::Add, 1.0f, 3.0f);
    setPair(t[(int) LutParam::StageMix], A::Clean, A::Heavy, -0.25f);
    setPair(t[(int) LutParam::StageMix], A::Crunch, A::Heavy, -0.15f);

    t[(int) LutParam::Hard] = makeRow("hard", 0.0f,
        { 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, -0.05f }, MixKind::SoftOr, 0.0f, 1.0f);
    setPair(t[(int) LutParam::Hard], A::Clean, A::Heavy, -0.15f);
    setPair(t[(int) LutParam::Hard], A::Heavy, A::Warm, -0.10f);

    t[(int) LutParam::Sag] = makeRow("sag", 0.0f,
        { -0.05f, 0.65f, 0.05f, -0.60f, 0.0f, 0.40f }, MixKind::Add, 0.0f, 0.85f);
    setPair(t[(int) LutParam::Sag], A::Crunch, A::Tight, -0.15f);
    setPair(t[(int) LutParam::Sag], A::Crunch, A::Warm, 0.10f);
    setPair(t[(int) LutParam::Sag], A::Clean, A::Crunch, -0.08f);

    t[(int) LutParam::Gate] = makeRow("gate", 0.0f,
        { 0.0f, 0.0f, 0.0f, 0.42f, 0.0f, 0.0f }, MixKind::SoftOr, 0.0f, 0.6f);

    t[(int) LutParam::HpfHz] = makeRow("hpfHz", 48.0f,
        { 0.0f, 12.0f, 77.0f, 112.0f, 0.0f, 0.0f }, MixKind::Add, 30.0f, 180.0f);
    setPair(t[(int) LutParam::HpfHz], A::Heavy, A::Tight, 15.0f);
    setTriple(t[(int) LutParam::HpfHz], A::Heavy, A::Tight, A::Cut, 8.0f);

    t[(int) LutParam::LowMidHz] = makeRow("lowMidHz", 350.0f,
        { 0.0f, -10.0f, -20.0f, -10.0f, 50.0f, -70.0f }, MixKind::Lane, 250.0f, 450.0f);

    t[(int) LutParam::LowMidDb] = makeRow("lowMidDb", 0.0f,
        { 0.0f, 4.5f, -12.0f, -10.0f, -3.0f, 5.0f }, MixKind::Add, -16.0f, 8.0f);
    setPair(t[(int) LutParam::LowMidDb], A::Heavy, A::Cut, 3.0f);
    setPair(t[(int) LutParam::LowMidDb], A::Crunch, A::Warm, 1.5f);
    setPair(t[(int) LutParam::LowMidDb], A::Tight, A::Warm, 1.0f);
    setTriple(t[(int) LutParam::LowMidDb], A::Crunch, A::Tight, A::Warm, 0.5f);

    t[(int) LutParam::LowMidQ] = makeRow("lowMidQ", 0.9f,
        { 0.0f, 0.0f, 0.2f, 0.05f, 0.1f, -0.1f }, MixKind::Add, 0.5f, 1.4f);

    t[(int) LutParam::MidHz] = makeRow("midHz", 800.0f,
        { 100.0f, -50.0f, 100.0f, 50.0f, 100.0f, -80.0f }, MixKind::Lane, 600.0f, 1200.0f);

    t[(int) LutParam::MidDb] = makeRow("midDb", 0.0f,
        { 0.0f, 9.5f, 1.0f, 1.5f, 12.0f, 2.0f }, MixKind::Add, -6.0f, 14.0f);
    setPair(t[(int) LutParam::MidDb], A::Clean, A::Cut, 1.5f);
    setPair(t[(int) LutParam::MidDb], A::Crunch, A::Heavy, 1.0f);
    setPair(t[(int) LutParam::MidDb], A::Heavy, A::Cut, 1.5f);
    setPair(t[(int) LutParam::MidDb], A::Tight, A::Cut, 0.5f);
    setTriple(t[(int) LutParam::MidDb], A::Clean, A::Heavy, A::Cut, 1.0f);
    setTriple(t[(int) LutParam::MidDb], A::Clean, A::Tight, A::Cut, 1.0f);
    setTriple(t[(int) LutParam::MidDb], A::Crunch, A::Cut, A::Warm, 1.0f);
    setTriple(t[(int) LutParam::MidDb], A::Heavy, A::Tight, A::Cut, 1.0f);

    t[(int) LutParam::MidQ] = makeRow("midQ", 0.85f,
        { 0.0f, 0.05f, 0.0f, 0.0f, 0.50f, 0.0f }, MixKind::Add, 0.5f, 1.6f);

    t[(int) LutParam::PresHz] = makeRow("presHz", 2000.0f,
        { 1200.0f, -300.0f, -450.0f, 200.0f, -200.0f, 200.0f }, MixKind::Lane, 1400.0f, 3600.0f);

    t[(int) LutParam::PresDb] = makeRow("presDb", 0.0f,
        { 8.0f, 3.5f, 9.0f, 4.0f, 7.0f, -3.5f }, MixKind::Add, -8.0f, 12.0f);
    setPair(t[(int) LutParam::PresDb], A::Clean, A::Heavy, 2.0f);
    setPair(t[(int) LutParam::PresDb], A::Clean, A::Cut, 1.5f);
    setPair(t[(int) LutParam::PresDb], A::Clean, A::Tight, 0.5f);
    setPair(t[(int) LutParam::PresDb], A::Heavy, A::Tight, 2.0f);
    setPair(t[(int) LutParam::PresDb], A::Heavy, A::Cut, 1.0f);
    setPair(t[(int) LutParam::PresDb], A::Tight, A::Cut, 1.0f);
    setTriple(t[(int) LutParam::PresDb], A::Clean, A::Heavy, A::Tight, 1.0f);
    setTriple(t[(int) LutParam::PresDb], A::Clean, A::Heavy, A::Cut, 1.0f);
    setTriple(t[(int) LutParam::PresDb], A::Clean, A::Tight, A::Cut, 1.0f);
    setTriple(t[(int) LutParam::PresDb], A::Heavy, A::Tight, A::Cut, 1.5f);

    t[(int) LutParam::PresQ] = makeRow("presQ", 0.8f,
        { 0.0f, 0.0f, -0.05f, 0.05f, 0.1f, 0.0f }, MixKind::Add, 0.5f, 1.3f);

    t[(int) LutParam::FizzHz] = makeRow("fizzHz", 6500.0f,
        { 500.0f, -300.0f, -500.0f, 0.0f, 0.0f, 0.0f }, MixKind::Lane, 5000.0f, 8000.0f);

    t[(int) LutParam::FizzDb] = makeRow("fizzDb", 0.0f,
        { 2.0f, -1.0f, -2.0f, 0.0f, 1.0f, -8.0f }, MixKind::Add, -12.0f, 4.0f);
    setPair(t[(int) LutParam::FizzDb], A::Heavy, A::Warm, -1.0f);

    t[(int) LutParam::CabHz] = makeRow("cabHz", 11000.0f,
        { 200.0f, -5000.0f, -7200.0f, 0.0f, 0.0f, -6700.0f }, MixKind::SoftOr, 2500.0f, 13000.0f);

    return t;
}

inline const std::array<LutRow, kNumLutParams>& toneLut()
{
    static const auto table = buildToneLut();
    return table;
}

inline float softOrCombine(const LutRow& row, const std::array<float, 6>& w)
{
    const float posSpan = juce::jmax(row.hi - row.base, 1.0e-4f);
    const float negSpan = juce::jmax(row.base - row.lo, 1.0e-4f);
    float accPos = 0.0f;
    float accNeg = 0.0f;

    for (int i = 0; i < kNumAxes; ++i)
    {
        const float t = row.axis[i] * w[(size_t) i];
        if (t > 0.0f)
        {
            const float u = juce::jlimit(0.0f, 1.0f, t / posSpan);
            accPos = accPos + u - accPos * u;
        }
        else if (t < 0.0f)
        {
            const float u = juce::jlimit(0.0f, 1.0f, -t / negSpan);
            accNeg = accNeg + u - accNeg * u;
        }
    }

    return row.base + accPos * posSpan - accNeg * negSpan;
}

inline float evaluateRow(const LutRow& row, const std::array<float, 6>& w)
{
    float v = 0.0f;

    if (row.mix == MixKind::SoftOr)
        v = softOrCombine(row, w);
    else
    {
        v = row.base;
        for (int i = 0; i < kNumAxes; ++i)
            v += row.axis[i] * w[(size_t) i];
    }

    int p = 0;
    for (int i = 0; i < kNumAxes; ++i)
        for (int j = i + 1; j < kNumAxes; ++j, ++p)
            v += row.pair[p] * w[(size_t) i] * w[(size_t) j];

    int t = 0;
    for (int i = 0; i < kNumAxes; ++i)
        for (int j = i + 1; j < kNumAxes; ++j)
            for (int k = j + 1; k < kNumAxes; ++k, ++t)
                v += row.triple[t] * w[(size_t) i] * w[(size_t) j] * w[(size_t) k];

    return juce::jlimit(row.lo, row.hi, v);
}

inline AmpParams composeParams(const std::array<float, 6>& axes)
{
    std::array<float, 6> w {};
    for (int i = 0; i < kNumAxes; ++i)
        w[(size_t) i] = juce::jlimit(0.0f, 1.0f, axes[(size_t) i]);

    const auto& lut = toneLut();
    AmpParams p;
    p.drive1 = evaluateRow(lut[(int) LutParam::Drive1], w);
    p.drive2 = evaluateRow(lut[(int) LutParam::Drive2], w);
    p.drive3 = evaluateRow(lut[(int) LutParam::Drive3], w);
    p.stageMix = evaluateRow(lut[(int) LutParam::StageMix], w);
    p.hard = evaluateRow(lut[(int) LutParam::Hard], w);
    p.sag = evaluateRow(lut[(int) LutParam::Sag], w);
    p.gate = evaluateRow(lut[(int) LutParam::Gate], w);
    p.makeup = 1.0f;
    p.hpfHz = evaluateRow(lut[(int) LutParam::HpfHz], w);
    p.lowMidHz = evaluateRow(lut[(int) LutParam::LowMidHz], w);
    p.lowMidDb = evaluateRow(lut[(int) LutParam::LowMidDb], w);
    p.lowMidQ = evaluateRow(lut[(int) LutParam::LowMidQ], w);
    p.midHz = evaluateRow(lut[(int) LutParam::MidHz], w);
    p.midDb = evaluateRow(lut[(int) LutParam::MidDb], w);
    p.midQ = evaluateRow(lut[(int) LutParam::MidQ], w);
    p.presHz = evaluateRow(lut[(int) LutParam::PresHz], w);
    p.presDb = evaluateRow(lut[(int) LutParam::PresDb], w);
    p.presQ = evaluateRow(lut[(int) LutParam::PresQ], w);
    p.fizzHz = evaluateRow(lut[(int) LutParam::FizzHz], w);
    p.fizzDb = evaluateRow(lut[(int) LutParam::FizzDb], w);
    p.cabHz = evaluateRow(lut[(int) LutParam::CabHz], w);
    return p;
}

inline float makeupFromParams(const AmpParams& p)
{
    const float s2 = juce::jlimit(0.0f, 1.0f, p.stageMix - 1.0f);
    const float s3 = juce::jlimit(0.0f, 1.0f, p.stageMix - 2.0f);
    const float clipLoud = 0.55f / (0.35f + 0.12f * p.drive1 + 0.10f * p.drive2 * s2
                                    + 0.08f * p.drive3 * s3 + 0.15f * p.hard * s3);
    const float eqDb = 0.45f * p.lowMidDb + 0.70f * p.midDb + 0.50f * p.presDb + 0.20f * p.fizzDb;
    const float eqLoud = juce::Decibels::decibelsToGain(eqDb);
    const float cabLoud = 0.65f + 0.35f * (p.cabHz / 11000.0f);
    const float hpfLoud = 0.85f + 0.15f * (1.0f - (p.hpfHz - 30.0f) / 150.0f);
    const float sagLoud = 1.0f / (1.0f + 0.35f * p.sag);
    const float raw = 1.0f / juce::jmax(1.0e-4f, clipLoud * eqLoud * cabLoud * hpfLoud * sagLoud);

    static const float baseRaw = []
    {
        const auto b = AmpParams {};
        const float bs2 = juce::jlimit(0.0f, 1.0f, b.stageMix - 1.0f);
        const float bs3 = juce::jlimit(0.0f, 1.0f, b.stageMix - 2.0f);
        const float bClip = 0.55f / (0.35f + 0.12f * b.drive1 + 0.10f * b.drive2 * bs2
                                     + 0.08f * b.drive3 * bs3 + 0.15f * b.hard * bs3);
        const float bEq = juce::Decibels::decibelsToGain(0.45f * b.lowMidDb + 0.70f * b.midDb
                                                         + 0.50f * b.presDb + 0.20f * b.fizzDb);
        const float bCab = 0.65f + 0.35f * (b.cabHz / 11000.0f);
        const float bHpf = 0.85f + 0.15f * (1.0f - (b.hpfHz - 30.0f) / 150.0f);
        const float bSag = 1.0f / (1.0f + 0.35f * b.sag);
        return 1.0f / juce::jmax(1.0e-4f, bClip * bEq * bCab * bHpf * bSag);
    }();

    return juce::jlimit(0.12f, 2.4f, raw / baseRaw);
}

inline AmpParams lerpParams(const AmpParams& a, const AmpParams& b, float t)
{
    t = juce::jlimit(0.0f, 1.0f, t);
    AmpParams p;
    const auto mix = [t] (float x, float y) { return x + (y - x) * t; };
    p.drive1 = mix(a.drive1, b.drive1);
    p.drive2 = mix(a.drive2, b.drive2);
    p.drive3 = mix(a.drive3, b.drive3);
    p.stageMix = mix(a.stageMix, b.stageMix);
    p.hard = mix(a.hard, b.hard);
    p.sag = mix(a.sag, b.sag);
    p.hpfHz = mix(a.hpfHz, b.hpfHz);
    p.lowMidHz = mix(a.lowMidHz, b.lowMidHz);
    p.lowMidDb = mix(a.lowMidDb, b.lowMidDb);
    p.lowMidQ = mix(a.lowMidQ, b.lowMidQ);
    p.midHz = mix(a.midHz, b.midHz);
    p.midDb = mix(a.midDb, b.midDb);
    p.midQ = mix(a.midQ, b.midQ);
    p.presHz = mix(a.presHz, b.presHz);
    p.presDb = mix(a.presDb, b.presDb);
    p.presQ = mix(a.presQ, b.presQ);
    p.fizzHz = mix(a.fizzHz, b.fizzHz);
    p.fizzDb = mix(a.fizzDb, b.fizzDb);
    p.cabHz = mix(a.cabHz, b.cabHz);
    p.gate = mix(a.gate, b.gate);
    p.makeup = mix(a.makeup, b.makeup);
    return p;
}

inline AmpParams baseParams()
{
    return {};
}
