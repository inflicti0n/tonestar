#pragma once

#include "CuteLookAndFeel.h"

#include <juce_events/juce_events.h>
#include <cmath>

class CircleButton : public juce::Button
{
public:
    CircleButton(juce::Colour fillColour, juce::Colour glyphColour, juce::String glyph)
        : juce::Button({}), fill(fillColour), glyph(glyphColour), symbol(std::move(glyph))
    {
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
        setWantsKeyboardFocus(false);
    }

    void paintButton(juce::Graphics& g, bool highlighted, bool down) override
    {
        auto bounds = getLocalBounds().toFloat();
        const float d = juce::jmin(bounds.getWidth(), bounds.getHeight()) - (down ? 2.0f : 0.0f);
        const auto disc = bounds.withSizeKeepingCentre(d, d);
        g.setColour(highlighted ? fill.brighter(0.08f) : fill);
        g.fillEllipse(disc);
        g.setColour(glyph);
        if (auto* laf = dynamic_cast<CuteLookAndFeel*>(&getLookAndFeel()))
            g.setFont(laf->font(16.0f, true));
        g.drawText(symbol, disc, juce::Justification::centred, false);
    }

private:
    juce::Colour fill;
    juce::Colour glyph;
    juce::String symbol;
};

enum class CircleIcon { Mute, Binaural, Debug, Presets, Advanced, Record, Folder, Export, Metro, Looper, Loop, Quantize, Play, Pause, Stop };

class CircleToggle : public juce::Button
{
public:
    CircleToggle(juce::String title, juce::Colour onColour, CircleIcon iconToUse,
                 juce::Colour offColour = CuteLookAndFeel::panel())
        : juce::Button({}), onFill(onColour), offFill(offColour), icon(iconToUse)
    {
        setClickingTogglesState(true);
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
        setWantsKeyboardFocus(false);
        setTooltip(std::move(title));
    }

    void paintButton(juce::Graphics& g, bool highlighted, bool down) override
    {
        auto bounds = getLocalBounds().toFloat();
        const float d = juce::jmin(bounds.getWidth(), bounds.getHeight()) - (down ? 2.0f : 0.0f);
        const auto disc = bounds.withSizeKeepingCentre(d, d);
        const bool on = getToggleState();
        auto fill = on ? onFill : offFill;
        if (highlighted)
            fill = fill.brighter(0.08f);
        g.setColour(fill);
        g.fillEllipse(disc);

        if (icon == CircleIcon::Record)
        {
            if (! on)
            {
                g.setColour(CuteLookAndFeel::flare().interpolatedWith(CuteLookAndFeel::panel(), 0.45f));
                g.fillEllipse(disc.reduced(d * 0.32f));
            }
            return;
        }

        const auto glyph = on ? CuteLookAndFeel::onAccent() : CuteLookAndFeel::dim();
        drawIcon(g, disc.reduced(d * 0.26f), glyph);
    }

private:
    void drawIcon(juce::Graphics& g, juce::Rectangle<float> r, juce::Colour colour) const
    {
        g.setColour(colour);

        if (icon == CircleIcon::Mute)
        {
            auto body = juce::Rectangle<float>(r.getX(), r.getY() + r.getHeight() * 0.28f,
                                               r.getWidth() * 0.34f, r.getHeight() * 0.44f);
            g.fillRoundedRectangle(body, 1.6f);
            juce::Path cone;
            cone.addTriangle(body.getRight() - 1.0f, r.getY() + r.getHeight() * 0.12f,
                             r.getRight(), r.getCentreY(),
                             body.getRight() - 1.0f, r.getBottom() - r.getHeight() * 0.12f);
            g.fillPath(cone);
            if (getToggleState())
            {
                g.drawLine(r.getX() + 1.0f, r.getY() + 1.0f, r.getRight() - 1.0f, r.getBottom() - 1.0f, 2.0f);
            }
        }
        else if (icon == CircleIcon::Binaural)
        {
            const float cupW = r.getWidth() * 0.28f;
            const float cupH = r.getHeight() * 0.42f;
            const float cupY = r.getY() + r.getHeight() * 0.40f;
            g.fillRoundedRectangle(r.getX(), cupY, cupW, cupH, 2.2f);
            g.fillRoundedRectangle(r.getRight() - cupW, cupY, cupW, cupH, 2.2f);
            juce::Path band;
            band.addCentredArc(r.getCentreX(), r.getY() + r.getHeight() * 0.42f,
                               r.getWidth() * 0.42f, r.getHeight() * 0.38f,
                               0.0f, juce::MathConstants<float>::pi, 0.0f, true);
            g.strokePath(band, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved,
                                                    juce::PathStrokeType::rounded));
        }
        else if (icon == CircleIcon::Presets)
        {
            const float x = r.getX();
            const float w = r.getWidth();
            const float h = r.getHeight();
            for (int i = 0; i < 3; ++i)
            {
                const float y = r.getY() + h * (0.18f + 0.28f * (float) i);
                g.fillRoundedRectangle(x, y, w, 2.0f, 1.0f);
            }
        }
        else if (icon == CircleIcon::Advanced)
        {
            const float w = r.getWidth();
            const float h = r.getHeight();
            const float barW = juce::jmax(2.0f, w * 0.18f);
            const float xs[3] { r.getX(), r.getCentreX() - barW * 0.5f, r.getRight() - barW };
            const float hs[3] { h * 0.55f, h, h * 0.72f };
            for (int i = 0; i < 3; ++i)
                g.fillRoundedRectangle(xs[i], r.getBottom() - hs[i], barW, hs[i], 1.2f);
        }
        else if (icon == CircleIcon::Record)
        {
            g.fillEllipse(r.reduced(r.getWidth() * 0.12f));
        }
        else if (icon == CircleIcon::Folder)
        {
            auto tab = juce::Rectangle<float>(r.getX(), r.getY(), r.getWidth() * 0.42f, r.getHeight() * 0.28f);
            g.fillRoundedRectangle(tab, 1.6f);
            g.fillRoundedRectangle(r.withTrimmedTop(r.getHeight() * 0.18f), 2.0f);
        }
        else if (icon == CircleIcon::Export)
        {
            const float mid = r.getCentreX();
            g.fillRoundedRectangle(r.getX(), r.getBottom() - r.getHeight() * 0.30f,
                                   r.getWidth(), r.getHeight() * 0.30f, 1.4f);
            g.fillRect(mid - 1.1f, r.getY() + r.getHeight() * 0.06f, 2.2f, r.getHeight() * 0.50f);
            juce::Path head;
            head.addTriangle(mid, r.getY() + r.getHeight() * 0.66f,
                             mid - r.getWidth() * 0.28f, r.getY() + r.getHeight() * 0.38f,
                             mid + r.getWidth() * 0.28f, r.getY() + r.getHeight() * 0.38f);
            g.fillPath(head);
        }
        else if (icon == CircleIcon::Metro)
        {
            auto tri = r.reduced(r.getWidth() * 0.22f, r.getHeight() * 0.18f);
            juce::Path body;
            body.startNewSubPath(tri.getCentreX(), tri.getY());
            body.lineTo(tri.getRight(), tri.getBottom());
            body.lineTo(tri.getX(), tri.getBottom());
            body.closeSubPath();
            g.strokePath(body, juce::PathStrokeType(1.5f, juce::PathStrokeType::curved,
                                                    juce::PathStrokeType::rounded));
            const float px = tri.getCentreX() - tri.getWidth() * 0.08f;
            g.drawLine(px, tri.getY() + tri.getHeight() * 0.06f,
                       px + tri.getWidth() * 0.28f, tri.getBottom() - tri.getHeight() * 0.16f, 1.8f);
            const float w = tri.getWidth() * 0.26f;
            g.fillEllipse(px + tri.getWidth() * 0.10f, tri.getY() + tri.getHeight() * 0.38f, w, w);
        }
        else if (icon == CircleIcon::Looper)
        {
            const auto c = r.getCentre();
            const float rad = juce::jmin(r.getWidth(), r.getHeight()) * 0.36f;
            juce::Path arc;
            const float from = juce::MathConstants<float>::pi * 0.35f;
            const float to = juce::MathConstants<float>::pi * 2.15f;
            arc.addCentredArc(c.x, c.y, rad, rad, 0.0f, from, to, true);
            g.strokePath(arc, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved,
                                                    juce::PathStrokeType::rounded));
            const float tipX = c.x + std::cos(to) * rad;
            const float tipY = c.y + std::sin(to) * rad;
            const float tx = -std::sin(to);
            const float ty = std::cos(to);
            const float nx = std::cos(to);
            const float ny = std::sin(to);
            juce::Path head;
            head.addTriangle(tipX + nx * 1.2f, tipY + ny * 1.2f,
                             tipX - tx * 4.2f - nx * 1.6f, tipY - ty * 4.2f - ny * 1.6f,
                             tipX + tx * 4.2f - nx * 1.6f, tipY + ty * 4.2f - ny * 1.6f);
            g.fillPath(head);
        }
        else if (icon == CircleIcon::Loop)
        {
            const float barW = juce::jmax(1.6f, r.getWidth() * 0.16f);
            g.fillRoundedRectangle(r.getX(), r.getY(), barW, r.getHeight(), 1.2f);
            g.fillRoundedRectangle(r.getRight() - barW, r.getY(), barW, r.getHeight(), 1.2f);
            g.fillRoundedRectangle(r.getX(), r.getY(), r.getWidth(), barW * 1.2f, 1.2f);
        }
        else if (icon == CircleIcon::Quantize)
        {
            if (auto* laf = dynamic_cast<CuteLookAndFeel*>(&getLookAndFeel()))
                g.setFont(laf->font(r.getHeight() * 0.92f, true));
            else
                g.setFont(juce::FontOptions(r.getHeight() * 0.92f, juce::Font::bold));
            g.drawText("Q", r, juce::Justification::centred, false);
        }
        else if (icon == CircleIcon::Play && ! getToggleState())
        {
            juce::Path tri;
            tri.addTriangle(r.getX() + r.getWidth() * 0.18f, r.getY(),
                            r.getRight(), r.getCentreY(),
                            r.getX() + r.getWidth() * 0.18f, r.getBottom());
            g.fillPath(tri);
        }
        else if (icon == CircleIcon::Play || icon == CircleIcon::Pause)
        {
            const float w = r.getWidth() * 0.28f;
            g.fillRoundedRectangle(r.getX(), r.getY(), w, r.getHeight(), 1.4f);
            g.fillRoundedRectangle(r.getRight() - w, r.getY(), w, r.getHeight(), 1.4f);
        }
        else if (icon == CircleIcon::Stop)
        {
            g.fillRoundedRectangle(r.reduced(r.getWidth() * 0.08f), 2.0f);
        }
        else
        {
            const float midX = r.getCentreX();
            const float midY = r.getCentreY();
            const float w = r.getWidth();
            const float h = r.getHeight();
            g.drawLine(midX, r.getY(), midX, r.getBottom(), 1.8f);
            g.drawLine(r.getX(), midY, r.getRight(), midY, 1.8f);
            g.fillEllipse(midX - w * 0.16f, midY - h * 0.16f, w * 0.32f, h * 0.32f);
        }
    }

    juce::Colour onFill;
    juce::Colour offFill;
    CircleIcon icon;
};

class WindowChrome : public juce::Component
{
public:
    static constexpr float pad = 12.0f;
    static constexpr float button = 28.0f;
    static constexpr int barHeight = 52;

    CircleToggle presetsButton { "Presets", CuteLookAndFeel::nova(), CircleIcon::Presets };
    CircleToggle advancedButton { "Advanced", CuteLookAndFeel::nova(), CircleIcon::Advanced };
    CircleToggle looperButton { "Looper", CuteLookAndFeel::nova(), CircleIcon::Looper };

    WindowChrome()
        : minimiseButton(CuteLookAndFeel::panel(), CuteLookAndFeel::mist(), "-"),
          closeButton(CuteLookAndFeel::flare(), CuteLookAndFeel::onAccent(), "X"),
          vblank(this, [this] { repaint(); })
    {
        setInterceptsMouseClicks(true, true);
        minimiseButton.onClick = [this]
        {
            if (auto* top = getTopLevelComponent())
                if (auto* peer = top->getPeer())
                    peer->setMinimised(true);
        };
        closeButton.onClick = []
        {
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        };
        addAndMakeVisible(presetsButton);
        addAndMakeVisible(advancedButton);
        addAndMakeVisible(looperButton);
        addAndMakeVisible(minimiseButton);
        addAndMakeVisible(closeButton);
    }

    void resized() override
    {
        const int d = (int) button;
        const int p = (int) pad;
        presetsButton.setBounds(p, p, d, d);
        advancedButton.setBounds(p + d + 8, p, d, d);
        looperButton.setBounds(p + (d + 8) * 2, p, d, d);
        closeButton.setBounds(getWidth() - p - d, p, d, d);
        minimiseButton.setBounds(getWidth() - p - d - 8 - d, p, d, d);
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (e.eventComponent != this)
            return;
        if (auto* top = getTopLevelComponent())
            dragger.startDraggingComponent(top, e);
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (e.eventComponent != this)
            return;
        if (auto* top = getTopLevelComponent())
            dragger.dragComponent(top, e, nullptr);
    }

    void paint(juce::Graphics& g) override
    {
        const float t = (float) juce::Time::getMillisecondCounterHiRes() * 0.001f;
        const float side = pad + button * 3.0f + 8.0f * 2.0f + 16.0f;
        auto area = getLocalBounds().toFloat().reduced(side, 0.0f);
        if (auto* laf = dynamic_cast<CuteLookAndFeel*>(&getLookAndFeel()))
            g.setFont(laf->titleFont(34.0f));
        else
            g.setFont(juce::FontOptions(34.0f, juce::Font::bold));

        const float cx = area.getCentreX();
        const float cy = area.getCentreY();
        const float shift = std::sin(t * 1.35f) * 56.0f;
        juce::ColourGradient grad(CuteLookAndFeel::starlight(), cx - 90.0f + shift, cy,
                                  CuteLookAndFeel::nova(), cx + 90.0f + shift, cy, false);
        grad.addColour(0.42, juce::Colour(0xfffff4d4));
        grad.addColour(0.68, CuteLookAndFeel::nova().brighter(0.15f));
        g.setGradientFill(grad);
        g.drawText("ToneStar", area, juce::Justification::centred, false);

        juce::Random rng { 0x5A4E };
        for (int i = 0; i < 10; ++i)
        {
            const float px = area.getX() + 18.0f + rng.nextFloat() * (area.getWidth() - 36.0f);
            const float py = area.getY() + 8.0f + rng.nextFloat() * (area.getHeight() - 16.0f);
            const float twinkle = 0.15f + 0.75f * (0.5f + 0.5f * std::sin(t * (3.2f + (float) i * 0.47f) + (float) i));
            g.setColour(CuteLookAndFeel::starlight().withAlpha(twinkle));
            const float s = 1.2f + rng.nextFloat() * 1.6f;
            g.fillEllipse(px, py, s, s);
        }
    }

private:
    juce::ComponentDragger dragger;
    CircleButton minimiseButton;
    CircleButton closeButton;
    juce::VBlankAttachment vblank;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WindowChrome)
};
