#pragma once

#include "Appearance/Theme.h"

#include <cmath>
#include <functional>

class VocalKey : public juce::Component
{
public:
    static constexpr int kNumRoots = 12;

    VocalKey()
    {
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
        setOpaque(false);
    }

    void paint(juce::Graphics& g) override
    {
        const auto disc = discBounds();
        g.setColour(Theme::panel());
        g.fillEllipse(disc);
        g.setColour(Theme::starlight());
        if (auto* laf = dynamic_cast<Theme*>(&getLookAndFeel()))
            g.setFont(laf->font(14.0f, true));
        else
            g.setFont(juce::FontOptions(14.0f, juce::Font::bold));
        g.drawText(label(), disc, juce::Justification::centred, false);

        drawArrow(g, upBounds(), true, hover == Hit::Up);
        drawArrow(g, downBounds(), false, hover == Hit::Down);
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        dragY = e.position.y;
        dragged = false;
        press = hitAt(e.position);
        if (press == Hit::Up)
        {
            step(1);
            dragged = true;
        }
        else if (press == Hit::Down)
        {
            step(-1);
            dragged = true;
        }
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (press != Hit::Disc)
            return;

        const int delta = (int) std::lround((dragY - e.position.y) / 10.0f);
        if (delta != 0)
        {
            dragY = e.position.y;
            dragged = true;
            step(delta > 0 ? 1 : -1);
        }
    }

    void mouseUp(const juce::MouseEvent&) override
    {
        if (press == Hit::Disc && ! dragged)
        {
            minor = ! minor;
            notify();
        }
        press = Hit::None;
    }

    void mouseMove(const juce::MouseEvent& e) override
    {
        const auto next = hitAt(e.position);
        if (next != hover)
        {
            hover = next;
            repaint();
        }
    }

    void mouseExit(const juce::MouseEvent&) override
    {
        hover = Hit::None;
        repaint();
    }

    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
    {
        if (w.deltaY > 0.0f)
            step(1);
        else if (w.deltaY < 0.0f)
            step(-1);
    }

    int getRoot() const { return root; }
    bool isMinor() const { return minor; }
    void setKey(int nextRoot, bool nextMinor, bool notifyChange)
    {
        root = ((nextRoot % 12) + 12) % 12;
        minor = nextMinor;
        if (notifyChange)
            notify();
        else
            repaint();
    }

    juce::String label() const
    {
        static constexpr const char* names[] = {
            "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"
        };
        juce::String text = names[root];
        if (minor)
            text += "m";
        return text;
    }

    std::function<void()> onChange;

private:
    enum class Hit { None, Disc, Up, Down };

    juce::Rectangle<float> discBounds() const
    {
        auto r = getLocalBounds().toFloat();
        r.removeFromRight(18.0f);
        const float d = juce::jmin(r.getWidth(), r.getHeight()) - 2.0f;
        return r.withSizeKeepingCentre(d, d);
    }

    juce::Rectangle<float> arrowStrip() const
    {
        return getLocalBounds().toFloat().removeFromRight(18.0f).reduced(1.0f, 2.0f);
    }

    juce::Rectangle<float> upBounds() const
    {
        auto r = arrowStrip();
        return r.removeFromTop(r.getHeight() * 0.5f).reduced(1.0f);
    }

    juce::Rectangle<float> downBounds() const
    {
        auto r = arrowStrip();
        return r.removeFromBottom(r.getHeight() * 0.5f).reduced(1.0f);
    }

    Hit hitAt(juce::Point<float> p) const
    {
        if (upBounds().expanded(2.0f).contains(p))
            return Hit::Up;
        if (downBounds().expanded(2.0f).contains(p))
            return Hit::Down;
        if (discBounds().contains(p))
            return Hit::Disc;
        return Hit::None;
    }

    void drawArrow(juce::Graphics& g, juce::Rectangle<float> r, bool upArrow, bool lit) const
    {
        juce::Path tri;
        const float mid = r.getCentreX();
        if (upArrow)
            tri.addTriangle(mid, r.getY() + 2.0f,
                            r.getX() + 2.0f, r.getBottom() - 2.0f,
                            r.getRight() - 2.0f, r.getBottom() - 2.0f);
        else
            tri.addTriangle(mid, r.getBottom() - 2.0f,
                            r.getX() + 2.0f, r.getY() + 2.0f,
                            r.getRight() - 2.0f, r.getY() + 2.0f);
        g.setColour(lit ? Theme::starlight() : Theme::mist());
        g.fillPath(tri);
    }

    void step(int dir)
    {
        root = (root + dir + 12) % 12;
        notify();
    }

    void notify()
    {
        if (onChange != nullptr)
            onChange();
        repaint();
    }

    int root = 0;
    bool minor = false;
    float dragY = 0.0f;
    bool dragged = false;
    Hit press = Hit::None;
    Hit hover = Hit::None;
};
