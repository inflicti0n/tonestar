#include "ToneField.h"
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

float ToneField::fxAngle(int index) const
{
    return -juce::MathConstants<float>::halfPi
           + juce::MathConstants<float>::pi / 8.0f
           + (float) index * juce::MathConstants<float>::twoPi / (float) FxRack::numJobs;
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

juce::Path ToneField::fxValuePath() const
{
    const auto c = fieldCentre();
    const float zeroR = fxZeroRadius();
    const float span = juce::jmax(1.0f, fxFullRadius() - zeroR);
    constexpr int steps = 16;

    std::array<float, 8> radii {};
    for (int i = 0; i < FxRack::numJobs; ++i)
        radii[(size_t) i] = zeroR + juce::jlimit(0.0f, 1.0f, fxDisplay[(size_t) i]) * span;

    juce::Path path;

    for (int i = 0; i < FxRack::numJobs; ++i)
    {
        const int j = (i + 1) % FxRack::numJobs;
        const float a0 = fxAngle(i);
        float a1 = fxAngle(j);
        if (a1 <= a0)
            a1 += juce::MathConstants<float>::twoPi;

        const float r0 = radii[(size_t) i];
        const float r1 = radii[(size_t) j];

        for (int s = 0; s < steps; ++s)
        {
            const float t = (float) s / (float) steps;
            const float ease = t * t * (3.0f - 2.0f * t);
            const float r = juce::jmax(zeroR, r0 + (r1 - r0) * ease);
            const float a = a0 + (a1 - a0) * t;
            const juce::Point<float> p { c.x + std::cos(a) * r, c.y + std::sin(a) * r };

            if (i == 0 && s == 0)
                path.startNewSubPath(p);
            else
                path.lineTo(p);
        }
    }

    path.closeSubPath();
    return path;
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

    for (int i = 0; i < FxRack::numJobs; ++i)
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
    for (int i = 0; i < FxRack::numJobs; ++i)
        fxTarget[(size_t) i] = juce::jlimit(0.0f, 1.0f, values[(size_t) i]);

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

juce::String ToneField::getActiveName() const
{
    if (focusKind == Focus::Fx)
    {
        const int index = dragFx >= 0 ? dragFx
                          : hoverFx >= 0 ? hoverFx
                                         : lastFx;
        return FxRack::jobName(index, bloomShimmer);
    }

    return ToneStar::axisName(star.getActiveAxis());
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
    for (int i = 0; i < FxRack::numJobs; ++i)
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

    if (dirty)
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
    repaint();
}

void ToneField::refreshTooltip(juce::Point<float> p)
{
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
        return;
    }

    star.mouseUp(e.getEventRelativeTo(&star));
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
        focusKind = Focus::Star;
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

    g.setColour(CuteLookAndFeel::rose().withAlpha(0.28f));
    g.drawEllipse(c.x - zeroR, c.y - zeroR, zeroR * 2.0f, zeroR * 2.0f, 1.5f);

    g.setColour(CuteLookAndFeel::peach().withAlpha(0.22f));
    g.drawEllipse(c.x - fullR, c.y - fullR, fullR * 2.0f, fullR * 2.0f, 1.0f);

    g.setColour(CuteLookAndFeel::peach().withAlpha(0.35f));
    for (int i = 0; i < FxRack::numJobs; ++i)
        g.drawLine({ fxSpoke(i, 0.0f), fxSpoke(i, 1.0f) }, 1.0f);
}

void ToneField::paintOverChildren(juce::Graphics& g)
{
    const auto c = fieldCentre();
    const float zeroR = fxZeroRadius();
    if (zeroR >= 4.0f)
    {
        const auto outer = fxValuePath();
        juce::Path band;
        band.addPath(outer);
        band.addEllipse(c.x - zeroR, c.y - zeroR, zeroR * 2.0f, zeroR * 2.0f);
        band.setUsingNonZeroWinding(false);
        g.setColour(CuteLookAndFeel::peach().withAlpha(0.38f));
        g.fillPath(band);
        g.setColour(CuteLookAndFeel::rose().withAlpha(0.7f));
        g.strokePath(outer, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));
    }

    for (int i = 0; i < FxRack::numJobs; ++i)
    {
        const bool active = (i == dragFx || i == hoverFx);
        const auto p = fxSpoke(i, fxDisplay[(size_t) i]);
        const float r = active ? 6.2f : 5.0f;
        g.setColour(CuteLookAndFeel::card());
        g.fillEllipse(p.x - r, p.y - r, r * 2.0f, r * 2.0f);
        g.setColour(active ? CuteLookAndFeel::rose() : CuteLookAndFeel::peach());
        g.drawEllipse(p.x - r, p.y - r, r * 2.0f, r * 2.0f, active ? 2.0f : 1.5f);

        const auto label = fxLabelBounds(i);
        const juce::String name = (i == FxRack::Bloom) ? bloomLabel() : juce::String(FxRack::jobName(i));
        g.setColour(active ? CuteLookAndFeel::rose() : CuteLookAndFeel::mutedInk());
        g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
        g.drawText(name, label, juce::Justification::centred, false);
    }
}
