#pragma once

#include "Appearance/DragTip.h"
#include "Appearance/Theme.h"

#include <cmath>
#include <functional>

class VocalShiftPad : public juce::Component
{
public:
    VocalShiftPad()
    {
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
        setOpaque(false);
        setPaintingIsUnclipped(true);
    }

    float getPitch() const { return pitch; }
    float getFormant() const { return formant; }
    int getMode() const { return mode; }

    void setShift(float nextPitch, float nextFormant, int nextMode, bool notifyChange)
    {
        pitch = snapIf(nextPitch, false);
        formant = snapIf(nextFormant, false);
        mode = juce::jlimit(0, 2, nextMode);
        if (notifyChange)
            notify();
        else
            repaint();
    }

    const char* modeName() const
    {
        static constexpr const char* names[] = { "Transpose", "Robot", "Quantize" };
        return names[mode];
    }

    juce::Colour modeColour() const
    {
        if (mode == 1)
            return Theme::nova();
        if (mode == 2)
            return Theme::flare();
        return Theme::starlight();
    }

    std::function<void()> onChange;

    void paint(juce::Graphics& g) override
    {
        auto box = padBounds();
        const auto line = modeColour();

        g.setColour(Theme::panel());
        g.fillRoundedRectangle(box, 10.0f);

        const auto frame = box.reduced(0.8f);
        const auto inner = box.reduced(10.0f);

        g.setColour(line.withMultipliedAlpha(0.22f));
        paintRangeSquare(g, inner, box, 10.0f);
        paintRangeSquare(g, inner, box, 5.0f);
        g.setColour(line.withMultipliedAlpha(0.10f));
        g.drawLine(frame.getCentreX(), frame.getY(), frame.getCentreX(), frame.getBottom(), 0.6f);
        g.drawLine(frame.getX(), frame.getCentreY(), frame.getRight(), frame.getCentreY(), 0.6f);

        g.setColour(line.withMultipliedAlpha(0.85f));
        g.drawRoundedRectangle(frame, 10.0f, 1.6f);

        const auto puck = puckBounds();
        g.setColour(line);
        g.fillEllipse(puck);
        g.setColour(Theme::voidFill().withMultipliedAlpha(0.35f));
        g.drawEllipse(puck.reduced(1.2f), 1.1f);

        auto title = getLocalBounds().toFloat().removeFromTop(16.0f);
        if (auto* laf = dynamic_cast<Theme*>(&getLookAndFeel()))
            g.setFont(laf->font(11.0f, true));
        else
            g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        g.setColour(line);
        g.drawText(modeName(), title, juce::Justification::centred, false);

    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu())
        {
            mode = (mode + 1) % 3;
            notify();
            return;
        }

        dragging = true;
        shiftWasDown = false;
        axisLock = AxisLock::None;
        grabOffset = {};

        if (puckBounds().expanded(6.0f).contains(e.position))
        {
            grabOffset = e.position - puckCentre();
        }
        else
        {
            setFromPoint(e.position, snapHeld(e), AxisLock::None);
        }

        beginAxisAnchor(e.position);
        applyAxisLock(e);
        showDragTip();
        repaint();
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (! dragging)
            return;
        applyAxisLock(e);
        setFromPoint(e.position - grabOffset, snapHeld(e), axisLock);
    }

    void mouseUp(const juce::MouseEvent&) override
    {
        dragging = false;
        shiftWasDown = false;
        axisLock = AxisLock::None;
        grabOffset = {};
        DragTip::hide();
        repaint();
    }

    void mouseDoubleClick(const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu())
            return;
        if (! puckBounds().expanded(6.0f).contains(e.position))
            return;
        pitch = 0.0f;
        formant = 0.0f;
        notify();
    }

private:
    static constexpr float kMin = -12.0f;
    static constexpr float kMax = 12.0f;

    juce::Rectangle<float> padBounds() const
    {
        auto r = getLocalBounds().toFloat();
        r.removeFromTop(16.0f);
        const float s = juce::jmin(r.getWidth(), r.getHeight());
        return r.withSizeKeepingCentre(s, s).reduced(1.0f);
    }

    juce::Rectangle<float> innerPad() const
    {
        return padBounds().reduced(10.0f);
    }

    static void paintRangeSquare(juce::Graphics& g, juce::Rectangle<float> inner,
                                 juce::Rectangle<float> box, float semitones)
    {
        const float span = kMax - kMin;
        const float scale = (semitones * 2.0f) / juce::jmax(1.0f, span);
        const auto mid = inner.withSizeKeepingCentre(inner.getWidth() * scale, inner.getHeight() * scale);
        const float corner = 10.0f * (mid.getWidth() / juce::jmax(1.0f, box.getWidth()));

        juce::Path path;
        path.addRoundedRectangle(mid, corner);
        juce::Path dotted;
        const float dashes[] = { 2.0f, 2.0f };
        juce::PathStrokeType(1.0f).createDashedStroke(dotted, path, dashes, 2);
        g.fillPath(dotted);
    }

    juce::Point<float> puckCentre() const
    {
        const auto inner = innerPad();
        const float nx = (formant - kMin) / (kMax - kMin);
        const float ny = 1.0f - (pitch - kMin) / (kMax - kMin);
        return { inner.getX() + nx * inner.getWidth(),
                 inner.getY() + ny * inner.getHeight() };
    }

    juce::Rectangle<float> puckBounds() const
    {
        const auto c = puckCentre();
        return { c.x - 6.0f, c.y - 6.0f, 12.0f, 12.0f };
    }

    void showDragTip()
    {
        juce::StringArray lines;
        lines.add("Pitch  " + DragTip::signedFixed(pitch));
        lines.add("Formant  " + DragTip::signedFixed(formant));
        DragTip::show(*this, puckCentre(), lines, modeColour());
    }

    enum class AxisLock { None, Pending, Horz, Vert };

    static bool snapHeld(const juce::MouseEvent& e)
    {
        return e.mods.isCtrlDown()
               || juce::ModifierKeys::getCurrentModifiers().isCtrlDown();
    }

    static bool shiftHeld(const juce::MouseEvent& e)
    {
        return e.mods.isShiftDown()
               || juce::ModifierKeys::getCurrentModifiers().isShiftDown();
    }

    static float snapIf(float value, bool shouldSnap)
    {
        value = juce::jlimit(kMin, kMax, value);
        if (! shouldSnap)
            return value;
        return juce::jlimit(kMin, kMax, std::round(value * 4.0f) * 0.25f);
    }

    void beginAxisAnchor(juce::Point<float> p)
    {
        axisAnchorPos = p;
        axisAnchorPitch = pitch;
        axisAnchorFormant = formant;
        axisLock = AxisLock::Pending;
    }

    void applyAxisLock(const juce::MouseEvent& e)
    {
        const bool shift = shiftHeld(e);
        if (shift && ! shiftWasDown)
            beginAxisAnchor(e.position);
        if (! shift)
            axisLock = AxisLock::None;
        shiftWasDown = shift;

        if (axisLock != AxisLock::Pending)
            return;

        const float dx = std::abs(e.position.x - axisAnchorPos.x);
        const float dy = std::abs(e.position.y - axisAnchorPos.y);
        constexpr float kDead = 3.0f;
        if (dx < kDead && dy < kDead)
            return;

        axisLock = dx >= dy ? AxisLock::Horz : AxisLock::Vert;
    }

    void setFromPoint(juce::Point<float> p, bool snap, AxisLock lock)
    {
        if (lock == AxisLock::Pending)
        {
            pitch = axisAnchorPitch;
            formant = axisAnchorFormant;
            notify();
            return;
        }

        const auto inner = innerPad();
        const float nx = juce::jlimit(0.0f, 1.0f, (p.x - inner.getX()) / juce::jmax(1.0f, inner.getWidth()));
        const float ny = juce::jlimit(0.0f, 1.0f, (p.y - inner.getY()) / juce::jmax(1.0f, inner.getHeight()));
        float nextFormant = snapIf(kMin + nx * (kMax - kMin), snap);
        float nextPitch = snapIf(kMin + (1.0f - ny) * (kMax - kMin), snap);

        if (lock == AxisLock::Horz)
            nextPitch = axisAnchorPitch;
        else if (lock == AxisLock::Vert)
            nextFormant = axisAnchorFormant;

        pitch = nextPitch;
        formant = nextFormant;
        notify();
    }

    void notify()
    {
        if (onChange != nullptr)
            onChange();
        if (dragging)
            showDragTip();
        repaint();
    }

    float pitch = 0.0f;
    float formant = 0.0f;
    int mode = 0;
    bool dragging = false;
    bool shiftWasDown = false;
    AxisLock axisLock = AxisLock::None;
    juce::Point<float> grabOffset;
    juce::Point<float> axisAnchorPos;
    float axisAnchorPitch = 0.0f;
    float axisAnchorFormant = 0.0f;
};
