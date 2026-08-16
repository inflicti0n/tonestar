#include "Field/ToneStar.h"
#include "Vocals/VocalCompose.h"

const char* ToneStar::axisName(int index) const
{
    if (axisCount == 5)
        return vocalAxisName(index);

    static constexpr const char* names[] = { "Clean", "Crunch", "Heavy", "Tight", "Cut", "Warm" };
    return names[juce::jlimit(0, axisCount - 1, index)];
}

void ToneStar::setAxisCount(int count)
{
    axisCount = juce::jlimit(5, maxAxes, count);
    for (int i = axisCount; i < maxAxes; ++i)
    {
        target[(size_t) i] = 0.0f;
        display[(size_t) i] = 0.0f;
    }
    syncPlasma();
    repaint();
}

ToneStar::ToneStar()
    : vblank(this, [this] { tick(); })
{
    setOpaque(false);
}

void ToneStar::resized()
{
    const auto bounds = getLocalBounds().toFloat().reduced(22.0f, 24.0f);
    centre = bounds.getCentre();
    radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.40f;
    syncPlasma();
}

void ToneStar::setFieldEnergy(FieldEnergy next)
{
    plasma.setFieldEnergy(next);
}

void ToneStar::setPlasmaLook(const PlasmaLook& next)
{
    plasma.setLook(next);
}

void ToneStar::syncPlasma()
{
    std::array<juce::Point<float>, maxAxes> values {};
    for (int i = 0; i < axisCount; ++i)
        values[(size_t) i] = spokePoint(i, display[(size_t) i]);
    for (int i = axisCount; i < maxAxes; ++i)
        values[(size_t) i] = centre;
    plasma.setShape(centre, radius, values, axisCount, getWidth(), getHeight());
}

float ToneStar::visualRadius(float value) const
{
    return innerPad + juce::jlimit(0.0f, 1.0f, value) * (1.0f - innerPad);
}

juce::Point<float> ToneStar::spokeVisual(int index, float visualT) const
{
    const float angle = -juce::MathConstants<float>::halfPi
                        + (float) index * juce::MathConstants<float>::twoPi / (float) axisCount;
    return { centre.x + std::cos(angle) * radius * visualT,
             centre.y + std::sin(angle) * radius * visualT };
}

juce::Point<float> ToneStar::spokePoint(int index, float value) const
{
    return spokeVisual(index, visualRadius(value));
}

float ToneStar::projectToSpoke(juce::Point<float> p, int index) const
{
    const auto end = spokePoint(index, 1.0f);
    const auto v = end - centre;
    const auto d = p - centre;
    const float denom = v.getDistanceFromOrigin();
    if (denom < 0.001f)
        return 0.0f;

    const float visual = d.getDotProduct(v) / (denom * radius);
    return juce::jlimit(0.0f, 1.0f, (visual - innerPad) / (1.0f - innerPad));
}

int ToneStar::hitHandle(juce::Point<float> p) const
{
    int best = -1;
    float bestDist = 22.0f;

    for (int i = 0; i < axisCount; ++i)
    {
        const float d = p.getDistanceFrom(spokePoint(i, display[(size_t) i]));
        if (d < bestDist)
        {
            bestDist = d;
            best = i;
        }
    }

    return best;
}

juce::Path ToneStar::axisPath(const std::array<juce::Point<float>, maxAxes>& points, float corner) const
{
    juce::Path path;
    for (int i = 0; i < axisCount; ++i)
    {
        const auto prev = points[(size_t) ((i + axisCount - 1) % axisCount)];
        const auto curr = points[(size_t) i];
        const auto next = points[(size_t) ((i + 1) % axisCount)];
        const float back = curr.getDistanceFrom(prev);
        const float fwd = curr.getDistanceFrom(next);
        const float r = juce::jmin(corner, back * 0.18f, fwd * 0.18f);
        const auto enter = back > 0.0f ? curr + (prev - curr) * (r / back) : curr;
        const auto leave = fwd > 0.0f ? curr + (next - curr) * (r / fwd) : curr;

        if (i == 0)
            path.startNewSubPath(enter);
        else
            path.lineTo(enter);

        path.quadraticTo(curr, leave);
    }

    path.closeSubPath();
    return path;
}

void ToneStar::setValues(const std::array<float, 6>& values, bool notify)
{
    for (int i = 0; i < axisCount; ++i)
        target[(size_t) i] = juce::jlimit(0.0f, 1.0f, values[(size_t) i]);
    for (int i = axisCount; i < maxAxes; ++i)
        target[(size_t) i] = 0.0f;

    if (notify && onChange != nullptr)
        onChange();
}

std::array<float, 6> ToneStar::getValues() const
{
    return target;
}

int ToneStar::getActiveAxis() const
{
    if (dragAxis >= 0)
        return dragAxis;
    if (hoverAxis >= 0)
        return hoverAxis;

    int best = 0;
    float peak = target[0];
    for (int i = 1; i < axisCount; ++i)
    {
        if (target[(size_t) i] > peak)
        {
            peak = target[(size_t) i];
            best = i;
        }
    }
    return best;
}

void ToneStar::tick()
{
    bool dirty = false;
    for (int i = 0; i < axisCount; ++i)
    {
        const float delta = target[(size_t) i] - display[(size_t) i];
        if (std::abs(delta) > 0.0008f)
        {
            display[(size_t) i] += delta * (dragAxis == i ? 1.0f : 0.28f);
            dirty = true;
        }
        else if (display[(size_t) i] != target[(size_t) i])
        {
            display[(size_t) i] = target[(size_t) i];
            dirty = true;
        }
    }

    syncPlasma();
    plasma.requestFrame();
    const auto plasmaSerial = plasma.frameSerial();
    if (dirty || plasmaSerial != lastPlasmaSerial)
    {
        lastPlasmaSerial = plasmaSerial;
        repaint();
    }
}

void ToneStar::setAxisFromEvent(const juce::MouseEvent& e)
{
    if (dragAxis < 0)
        return;

    float value = projectToSpoke(e.position, dragAxis);
    if (e.mods.isShiftDown())
        value = std::round(value * 20.0f) / 20.0f;

    target[(size_t) dragAxis] = value;
    display[(size_t) dragAxis] = value;
    if (onChange != nullptr)
        onChange();
    repaint();
}

void ToneStar::mouseDown(const juce::MouseEvent& e)
{
    dragAxis = hitHandle(e.position);
    if (dragAxis >= 0)
        setAxisFromEvent(e);
}

void ToneStar::mouseDrag(const juce::MouseEvent& e)
{
    setAxisFromEvent(e);
}

void ToneStar::mouseUp(const juce::MouseEvent&)
{
    dragAxis = -1;
}

void ToneStar::mouseMove(const juce::MouseEvent& e)
{
    const int next = hitHandle(e.position);
    if (next != hoverAxis)
    {
        hoverAxis = next;
        repaint();
    }
}

void ToneStar::mouseExit(const juce::MouseEvent&)
{
    hoverAxis = -1;
    repaint();
}

void ToneStar::mouseDoubleClick(const juce::MouseEvent& e)
{
    const int axis = hitHandle(e.position);
    if (axis < 0)
        return;

    target[(size_t) axis] = 0.0f;
    display[(size_t) axis] = 0.0f;
    if (onChange != nullptr)
        onChange();
    repaint();
}

void ToneStar::paint(juce::Graphics& g)
{
    std::array<juce::Point<float>, maxAxes> outer {}, values {};
    for (int i = 0; i < axisCount; ++i)
    {
        outer[(size_t) i] = spokePoint(i, 1.0f);
        values[(size_t) i] = spokePoint(i, display[(size_t) i]);
    }

    g.setColour(Theme::meterTrack().withAlpha(0.85f));
    for (float value : { 0.0f, 0.5f, 1.0f })
    {
        std::array<juce::Point<float>, maxAxes> pts {};
        for (int i = 0; i < axisCount; ++i)
            pts[(size_t) i] = spokePoint(i, value);
        g.strokePath(axisPath(pts, 5.0f), juce::PathStrokeType(1.1f));
    }

    g.setColour(Theme::peach().withAlpha(0.45f));
    for (int i = 0; i < axisCount; ++i)
        g.drawLine({ spokePoint(i, 0.0f), outer[(size_t) i] }, 1.0f);

    const auto starPath = axisPath(values, 7.0f);
    g.setColour(Theme::peach().withAlpha(0.35f));
    g.fillPath(starPath);

    const auto plasmaFrame = plasma.copyFrame();
    if (plasmaFrame.isValid())
    {
        const auto dest = getLocalBounds().toFloat();
        g.saveState();
        g.reduceClipRegion(starPath);
        g.drawImage(plasmaFrame, dest);
        g.setOpacity(0.55f);
        g.drawImage(plasmaFrame, dest);
        g.restoreState();
    }

    g.setColour(Theme::rose());
    g.strokePath(starPath, juce::PathStrokeType(2.0f, juce::PathStrokeType::mitered,
                                                juce::PathStrokeType::butt));

    for (int i = 0; i < axisCount; ++i)
    {
        const bool active = (i == dragAxis || i == hoverAxis);
        const auto p = values[(size_t) i];
        const float r = active ? 7.0f : 5.5f;
        g.setColour(Theme::card());
        g.fillEllipse(p.x - r, p.y - r, r * 2.0f, r * 2.0f);
        g.setColour(Theme::rose());
        g.drawEllipse(p.x - r, p.y - r, r * 2.0f, r * 2.0f, active ? 2.2f : 1.6f);

        g.setColour(active ? Theme::rose() : Theme::ink());
        g.setFont(juce::FontOptions(15.0f, juce::Font::bold));
        if (axisCount == 6)
        {
            struct Place { float t; float ox; float oy; float deg; };
            static constexpr Place place[] = {
                { 1.08f,  0.0f,  -7.0f,   0.0f },
                { 1.07f,  6.0f,  -5.0f,  45.0f },
                { 1.07f,  6.0f,   6.0f, -45.0f },
                { 1.08f,  0.0f,   7.0f,   0.0f },
                { 1.07f, -6.0f,   6.0f,  45.0f },
                { 1.07f, -6.0f,  -5.0f, -45.0f }
            };
            const auto& pl = place[i];
            const auto label = spokeVisual(i, pl.t) + juce::Point<float>(pl.ox, pl.oy);
            g.saveState();
            g.addTransform(juce::AffineTransform::rotation(juce::degreesToRadians(pl.deg), label.x, label.y));
            g.drawText(axisName(i), juce::Rectangle<float>(label.x - 36.0f, label.y - 10.0f, 72.0f, 20.0f),
                       juce::Justification::centred, false);
            g.restoreState();
        }
        else
        {
            const auto label = spokeVisual(i, 1.12f);
            g.drawText(axisName(i), juce::Rectangle<float>(label.x - 36.0f, label.y - 10.0f, 72.0f, 20.0f),
                       juce::Justification::centred, false);
        }
    }
}
