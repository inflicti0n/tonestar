#include "Field/ToneField.h"
#include "Appearance/DragTip.h"
#include "Vocals/VocalCompose.h"
#include <cmath>

ToneField::ToneField()
    : vblank(this, [this] { tick(); })
{
    setOpaque(false);
    star.setInterceptsMouseClicks(false, false);
    star.onChange = [this]
    {
        focusKind = Focus::Star;
        if (onChange != nullptr)
            onChange();
    };
    addAndMakeVisible(star);
}

void ToneField::resized()
{
    const int inset = juce::jmax(72, juce::roundToInt((float) juce::jmin(getWidth(), getHeight()) * 0.22f));
    star.setBounds(getLocalBounds().reduced(inset));
}

juce::Point<float> ToneField::fieldCentre() const
{
    return star.getBounds().toFloat().getPosition() + star.getCentre();
}

float ToneField::starRadius() const
{
    return star.getRadius();
}

float ToneField::fxZeroRadius() const
{
    return starRadius() * envelopeT;
}

float ToneField::fxFullRadius() const
{
    return starRadius() * fxFullT;
}

void ToneField::setRigMode(RigMode next)
{
    mode = next;
    const int axes = mode == RigMode::Vocals ? 5 : 6;
    fxCount = mode == RigMode::Vocals ? 6 : FxRack::numJobs;
    star.setAxisCount(axes);
    for (int i = fxCount; i < FxRack::numJobs; ++i)
    {
        fxTarget[(size_t) i] = 0.0f;
        fxDisplay[(size_t) i] = 0.0f;
    }
    if (mode == RigMode::Vocals)
        bloomShimmer = false;
    if (hoverFx >= fxCount)
        hoverFx = -1;
    if (dragFx >= fxCount)
        dragFx = -1;
    if (lastFx >= fxCount)
        lastFx = 0;
    repaint();
}

float ToneField::fxAngle(int index) const
{
    const float n = (float) juce::jmax(1, fxCount);
    return -juce::MathConstants<float>::halfPi
           + juce::MathConstants<float>::pi / n
           + (float) index * juce::MathConstants<float>::twoPi / n;
}

juce::Point<float> ToneField::fxSpoke(int index, float amount) const
{
    const float t = juce::jlimit(0.0f, 1.0f, amount);
    const float r = fxZeroRadius() + t * (fxFullRadius() - fxZeroRadius());
    const float a = fxAngle(index);
    const auto c = fieldCentre();
    return { c.x + std::cos(a) * r, c.y + std::sin(a) * r };
}

juce::Point<float> ToneField::fxLabelPoint(int index) const
{
    const float r = fxFullRadius() + 18.0f;
    const float a = fxAngle(index);
    const auto c = fieldCentre();
    return { c.x + std::cos(a) * r, c.y + std::sin(a) * r };
}

juce::Rectangle<float> ToneField::fxLabelBounds(int index) const
{
    const auto p = fxLabelPoint(index);
    return { p.x - 44.0f, p.y - 14.0f, 88.0f, 28.0f };
}

float ToneField::projectToFx(juce::Point<float> p, int index) const
{
    const auto c = fieldCentre();
    const float a = fxAngle(index);
    const auto d = p - c;
    const float along = d.x * std::cos(a) + d.y * std::sin(a);
    const float span = fxFullRadius() - fxZeroRadius();
    if (span < 0.001f)
        return 0.0f;
    return juce::jlimit(0.0f, 1.0f, (along - fxZeroRadius()) / span);
}

int ToneField::hitFxHandle(juce::Point<float> p) const
{
    int best = -1;
    float bestDist = 18.0f;

    for (int i = 0; i < fxCount; ++i)
    {
        const float d = p.getDistanceFrom(fxSpoke(i, fxDisplay[(size_t) i]));
        if (d < bestDist)
        {
            bestDist = d;
            best = i;
        }
    }

    return best;
}

bool ToneField::hitBloomLabel(juce::Point<float> p) const
{
    if (mode == RigMode::Vocals)
        return false;

    if (fxLabelBounds(FxRack::Bloom).contains(p))
        return true;

    return hitFxHandle(p) == FxRack::Bloom;
}

const char* ToneField::bloomLabel() const
{
    return bloomShimmer ? "Shimmer" : "Bloom";
}

void ToneField::setStarValues(const std::array<float, 6>& values, bool notify)
{
    star.setValues(values, notify);
}

std::array<float, 6> ToneField::getStarValues() const
{
    return star.getValues();
}

void ToneField::setFxValues(const std::array<float, 8>& values, bool notify)
{
    for (int i = 0; i < fxCount; ++i)
        fxTarget[(size_t) i] = juce::jlimit(0.0f, 1.0f, values[(size_t) i]);
    for (int i = fxCount; i < FxRack::numJobs; ++i)
        fxTarget[(size_t) i] = 0.0f;

    if (notify && onChange != nullptr)
        onChange();
    repaint();
}

std::array<float, 8> ToneField::getFxValues() const
{
    return fxTarget;
}

void ToneField::setBloomShimmer(bool shouldShimmer, bool notify)
{
    bloomShimmer = shouldShimmer;
    if (notify && onChange != nullptr)
        onChange();
    repaint();
}

void ToneField::setFieldEnergy(FieldEnergy next)
{
    star.setFieldEnergy(next);
}

void ToneField::setFieldSpectrum(const FieldSpectrum& next)
{
    spectrum = next;
}

void ToneField::setPlasmaLook(const PlasmaLook& next)
{
    look = next;
    star.setPlasmaLook(next);
}

float ToneField::fxRadiusAtUnit(float t) const
{
    t -= std::floor(t);
    const float f = t * (float) fxCount;
    const int i = ((int) std::floor(f)) % fxCount;
    const int j = (i + 1) % fxCount;
    const float u = f - std::floor(f);
    const float ease = u * u * (3.0f - 2.0f * u);
    const float zeroR = fxZeroRadius();
    const float span = juce::jmax(1.0f, fxFullRadius() - zeroR);
    const float r0 = zeroR + juce::jlimit(0.0f, 1.0f, fxDisplay[(size_t) i]) * span;
    const float r1 = zeroR + juce::jlimit(0.0f, 1.0f, fxDisplay[(size_t) j]) * span;
    return r0 + (r1 - r0) * ease;
}

float ToneField::fxAngleAtUnit(float t) const
{
    t -= std::floor(t);
    const float f = t * (float) fxCount;
    const int i = ((int) std::floor(f)) % fxCount;
    const int j = (i + 1) % fxCount;
    const float u = f - std::floor(f);
    const float a0 = fxAngle(i);
    float a1 = fxAngle(j);
    if (a1 <= a0)
        a1 += juce::MathConstants<float>::twoPi;
    return a0 + (a1 - a0) * u;
}

float ToneField::spectrumAt(float t) const
{
    t -= std::floor(t);
    const float x = t * (float) FieldSpectrum::bins;
    const int i0 = ((int) std::floor(x)) & (FieldSpectrum::bins - 1);
    const int i1 = (i0 + 1) & (FieldSpectrum::bins - 1);
    const float u = x - std::floor(x);
    return spectrumShown.mag[(size_t) i0] + (spectrumShown.mag[(size_t) i1] - spectrumShown.mag[(size_t) i0]) * u;
}

juce::String ToneField::getActiveName() const
{
    if (focusKind == Focus::Fx)
    {
        const int index = dragFx >= 0 ? dragFx
                          : hoverFx >= 0 ? hoverFx
                                         : lastFx;
        if (mode == RigMode::Vocals)
            return vocalFxName(index);
        return FxRack::jobName(index, bloomShimmer);
    }

    return star.axisName(star.getActiveAxis());
}

float ToneField::getActiveAmount() const
{
    if (focusKind == Focus::Fx)
    {
        const int index = dragFx >= 0 ? dragFx
                          : hoverFx >= 0 ? hoverFx
                                         : lastFx;
        return fxTarget[(size_t) index];
    }

    const int axis = star.getActiveAxis();
    return star.getValues()[(size_t) axis];
}

void ToneField::tick()
{
    bool dirty = false;
    for (int i = 0; i < fxCount; ++i)
    {
        const float delta = fxTarget[(size_t) i] - fxDisplay[(size_t) i];
        if (std::abs(delta) > 0.0008f)
        {
            fxDisplay[(size_t) i] += delta * (dragFx == i ? 1.0f : 0.28f);
            dirty = true;
        }
        else if (fxDisplay[(size_t) i] != fxTarget[(size_t) i])
        {
            fxDisplay[(size_t) i] = fxTarget[(size_t) i];
            dirty = true;
        }
    }

    const float dt = 1.0f / 60.0f;
    ringTime += dt;

    const float followUp = juce::jlimit(0.25f, 0.95f, 1.0f - look.ringSmooth * 0.45f);
    const float followDn = juce::jlimit(0.10f, 0.70f, 1.0f - look.ringSmooth);
    for (int i = 0; i < FieldSpectrum::bins; ++i)
    {
        const float next = spectrum.mag[(size_t) i];
        auto& shown = spectrumShown.mag[(size_t) i];
        shown += (next - shown) * (next > shown ? followUp : followDn);
    }

    if (isShowing() && (dirty || look.ringAmount > 0.01f || look.ringIdle > 0.01f))
        repaint();
}

void ToneField::setFxFromEvent(const juce::MouseEvent& e)
{
    if (dragFx < 0)
        return;

    float value = projectToFx(e.position, dragFx);
    if (e.mods.isShiftDown())
        value = std::round(value * 20.0f) / 20.0f;

    fxTarget[(size_t) dragFx] = value;
    fxDisplay[(size_t) dragFx] = value;
    focusKind = Focus::Fx;
    if (onChange != nullptr)
        onChange();
    DragTip::show(*this, fxSpoke(dragFx, fxDisplay[(size_t) dragFx]),
                  DragTip::percent(fxDisplay[(size_t) dragFx]), Theme::rose());
    repaint();
}

void ToneField::refreshTooltip(juce::Point<float> p)
{
    if (mode == RigMode::Vocals)
    {
        const int fx = hitFxHandle(p);
        if (fx >= 0)
        {
            setTooltip(vocalFxTip(fx));
            return;
        }
        const int axis = star.getHoverAxis();
        if (axis >= 0)
        {
            setTooltip(vocalAxisTip(axis));
            return;
        }
        setTooltip({});
        return;
    }

    if (hitBloomLabel(p))
        setTooltip(FxRack::jobTip(FxRack::Bloom, bloomShimmer));
    else
        setTooltip({});
}

void ToneField::mouseDown(const juce::MouseEvent& e)
{
    const bool rightClick = e.mods.isPopupMenu() || e.mods.isRightButtonDown();
    if (rightClick)
    {
        if (mode == RigMode::Vocals)
            return;

        if (hitBloomLabel(e.position))
        {
            bloomShimmer = ! bloomShimmer;
            focusKind = Focus::Fx;
            hoverFx = FxRack::Bloom;
            lastFx = FxRack::Bloom;
            refreshTooltip(e.position);
            if (onChange != nullptr)
                onChange();
            repaint();
        }
        return;
    }

    dragFx = hitFxHandle(e.position);
    if (dragFx >= 0)
    {
        hoverFx = dragFx;
        lastFx = dragFx;
        setFxFromEvent(e);
        return;
    }

    star.mouseDown(e.getEventRelativeTo(&star));
    if (star.isDragging())
        focusKind = Focus::Star;
}

void ToneField::mouseDrag(const juce::MouseEvent& e)
{
    if (dragFx >= 0)
    {
        setFxFromEvent(e);
        return;
    }

    if (star.isDragging())
        star.mouseDrag(e.getEventRelativeTo(&star));
}

void ToneField::mouseUp(const juce::MouseEvent& e)
{
    if (dragFx >= 0)
    {
        dragFx = -1;
        DragTip::hide();
        repaint();
        return;
    }

    star.mouseUp(e.getEventRelativeTo(&star));
    DragTip::hide();
    repaint();
}

void ToneField::mouseMove(const juce::MouseEvent& e)
{
    const int nextFx = hitFxHandle(e.position);
    if (nextFx >= 0)
    {
        if (hoverFx != nextFx)
        {
            hoverFx = nextFx;
            lastFx = nextFx;
            focusKind = Focus::Fx;
            star.mouseExit(e.getEventRelativeTo(&star));
            repaint();
        }
        refreshTooltip(e.position);
        return;
    }

    if (hitBloomLabel(e.position))
    {
        if (hoverFx != FxRack::Bloom)
        {
            hoverFx = FxRack::Bloom;
            lastFx = FxRack::Bloom;
            focusKind = Focus::Fx;
            star.mouseExit(e.getEventRelativeTo(&star));
            repaint();
        }
        refreshTooltip(e.position);
        return;
    }

    if (hoverFx >= 0)
    {
        hoverFx = -1;
        repaint();
    }
    setTooltip({});

    star.mouseMove(e.getEventRelativeTo(&star));
    if (star.getHoverAxis() >= 0)
    {
        focusKind = Focus::Star;
        refreshTooltip(e.position);
    }
}

void ToneField::mouseExit(const juce::MouseEvent& e)
{
    hoverFx = -1;
    setTooltip({});
    star.mouseExit(e.getEventRelativeTo(&star));
    repaint();
}

void ToneField::mouseDoubleClick(const juce::MouseEvent& e)
{
    const int fx = hitFxHandle(e.position);
    if (fx >= 0)
    {
        fxTarget[(size_t) fx] = 0.0f;
        fxDisplay[(size_t) fx] = 0.0f;
        focusKind = Focus::Fx;
        hoverFx = fx;
        lastFx = fx;
        if (onChange != nullptr)
            onChange();
        repaint();
        return;
    }

    star.mouseDoubleClick(e.getEventRelativeTo(&star));
}

void ToneField::paint(juce::Graphics& g)
{
    const auto c = fieldCentre();
    const float zeroR = fxZeroRadius();
    const float fullR = fxFullRadius();
    if (zeroR < 4.0f)
        return;

    g.setColour(Theme::rose().withAlpha(0.28f));
    g.drawEllipse(c.x - zeroR, c.y - zeroR, zeroR * 2.0f, zeroR * 2.0f, 1.5f);

    g.setColour(Theme::peach().withAlpha(0.22f));
    g.drawEllipse(c.x - fullR, c.y - fullR, fullR * 2.0f, fullR * 2.0f, 1.0f);

    g.setColour(Theme::peach().withAlpha(0.35f));
    for (int i = 0; i < fxCount; ++i)
        g.drawLine({ fxSpoke(i, 0.0f), fxSpoke(i, 1.0f) }, 1.0f);
}

void ToneField::paintOverChildren(juce::Graphics& g)
{
    const auto c = fieldCentre();
    const float zeroR = fxZeroRadius();
    if (zeroR >= 4.0f)
        paintFxSpectrum(g);

    for (int i = 0; i < fxCount; ++i)
    {
        const bool active = (i == dragFx || i == hoverFx);
        const auto p = fxSpoke(i, fxDisplay[(size_t) i]);
        const float r = active ? 6.2f : 5.0f;
        g.setColour(Theme::card());
        g.fillEllipse(p.x - r, p.y - r, r * 2.0f, r * 2.0f);
        g.setColour(active ? Theme::rose() : Theme::peach());
        g.drawEllipse(p.x - r, p.y - r, r * 2.0f, r * 2.0f, active ? 2.0f : 1.5f);

        const auto label = fxLabelBounds(i);
        const juce::String name = mode == RigMode::Vocals
                                      ? juce::String(vocalFxName(i))
                                      : ((i == FxRack::Bloom) ? bloomLabel() : juce::String(FxRack::jobName(i)));
        g.setColour(active ? Theme::rose() : Theme::mutedInk());
        g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
        g.drawText(name, label, juce::Justification::centred, false);
    }
}

void ToneField::paintFxSpectrum(juce::Graphics& g)
{
    if (! isShowing())
        return;

    const auto c = fieldCentre();
    const float zeroR = fxZeroRadius();
    const float span = juce::jmax(1.0f, fxFullRadius() - zeroR);
    constexpr int points = 128;
    std::array<juce::Point<float>, points + 1> osc {};
    std::array<float, points + 1> level {};

    float treble = 0.0f;
    constexpr int trebleFrom = FieldSpectrum::bins * 2 / 3;
    for (int i = trebleFrom; i < FieldSpectrum::bins; ++i)
        treble += spectrumShown.mag[(size_t) i];
    treble /= (float) juce::jmax(1, FieldSpectrum::bins - trebleFrom);
    treble = juce::jlimit(0.0f, 1.0f, treble * look.ringGain);

    auto hash01 = [] (int n) -> float
    {
        unsigned x = (unsigned) n * 747796405u + 2891336453u;
        x = (x ^ (x >> 16)) * 0x45d9f3bu;
        return (float) (x & 0xffffu) / 65535.0f;
    };

    for (int i = 0; i <= points; ++i)
    {
        const float t = (float) i / (float) points;
        const float a = fxAngleAtUnit(t);
        const float baseR = fxRadiusAtUnit(t);
        const float spec = juce::jlimit(0.0f, 1.0f, spectrumAt(t) * look.ringGain);
        const float bass = juce::jlimit(0.0f, 1.0f, 1.0f - t * 1.6f);
        const float high = t * t;
        const float h1 = hash01(i);
        const float h2 = hash01(i * 19 + 7);
        const float h3 = hash01(i * 47 + 31);
        const float waveA = std::sin(ringTime * (2.5f + 2.4f * h1) + a * (9.0f + 7.0f * h2) + h1 * 6.28318f);
        const float waveB = std::sin(ringTime * (4.6f + 3.8f * h2) - a * (16.0f + 10.0f * h1) + h2 * 4.2f);
        const float waveC = std::sin(ringTime * (7.8f + 5.5f * h3) + a * 23.0f + (float) i * 0.41f);
        const float surface = 0.48f * waveA + 0.34f * waveB + 0.18f * waveC;
        const float idle = look.ringIdle * span * 0.055f * surface;
        const float kick = look.ringAmount * spec * span * (0.38f + 0.55f * spec);
        const float shake = look.ringAmount * (0.28f + 0.72f * spec) * span * 0.11f * surface;
        const float flutter = look.ringAmount * spec * high * span * 0.22f
                              * std::sin(ringTime * (12.0f + 28.0f * high + 6.0f * h3) + a * (11.0f + 8.0f * h1));
        const float throb = look.ringAmount * bass * spec * span * 0.10f
                            * std::sin(ringTime * (4.8f + 2.2f * h2) + a * (5.0f + 4.0f * h3));
        const float spray = look.ringAmount * treble * high * span * 0.14f
                            * std::sin(ringTime * (18.0f + 10.0f * h1) + a * 19.0f + h3 * 6.28318f);
        const float r = juce::jmax(zeroR * 0.88f, baseR + kick + idle + shake + flutter + throb + spray);
        osc[(size_t) i] = { c.x + std::cos(a) * r, c.y + std::sin(a) * r };
        level[(size_t) i] = juce::jlimit(0.0f, 1.0f, spec * (0.75f + 0.45f * high) + treble * high * 0.25f);
    }

    if (look.ringFill > 0.001f)
    {
        juce::Path band;
        band.startNewSubPath(osc[0]);
        for (int i = 1; i <= points; ++i)
            band.lineTo(osc[(size_t) i]);
        band.closeSubPath();
        band.addEllipse(c.x - zeroR, c.y - zeroR, zeroR * 2.0f, zeroR * 2.0f);
        band.setUsingNonZeroWinding(false);
        g.setColour(look.ringLine.withMultipliedAlpha(look.ringFill * 0.55f));
        g.fillPath(band);
    }

    if (look.ringGlow > 0.001f)
    {
        juce::Path glow;
        glow.startNewSubPath(osc[0]);
        for (int i = 1; i <= points; ++i)
            glow.lineTo(osc[(size_t) i]);
        g.setColour(look.ringAura.withMultipliedAlpha(juce::jlimit(0.0f, 1.0f, look.ringGlow * 0.55f)));
        g.strokePath(glow, juce::PathStrokeType(look.ringThick * 3.1f, juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));
    }

    for (int i = 0; i < points; ++i)
    {
        const float heat = juce::jlimit(0.0f, 1.0f, level[(size_t) i]);
        const auto colour = look.ringLine.interpolatedWith(look.ringHot, heat * 0.85f);
        g.setColour(colour);
        g.drawLine({ osc[(size_t) i], osc[(size_t) i + 1] }, look.ringThick);
    }
}
