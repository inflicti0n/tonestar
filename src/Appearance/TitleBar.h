#pragma once

#include "App/RigMode.h"
#include "Appearance/WindowShell.h"

#include <juce_events/juce_events.h>
#include <cmath>
#include <functional>

enum class CircleIcon { Mute, Binaural, Debug, Presets, Advanced, Record, Folder, Export, Metro, Tune, Looper, Loop, Quantize, Play, Pause, Stop };

class CircleToggle : public juce::Button
{
public:
    CircleToggle(juce::String title, juce::Colour onColour, CircleIcon iconToUse,
                 juce::Colour offColour = Theme::panel())
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
                g.setColour(Theme::flare().interpolatedWith(Theme::panel(), 0.45f));
                g.fillEllipse(disc.reduced(d * 0.32f));
            }
            return;
        }

        const auto glyph = on ? Theme::onAccent() : Theme::dim();
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
        else if (icon == CircleIcon::Tune)
        {
            const float midX = r.getCentreX();
            g.drawLine(midX, r.getY(), midX, r.getBottom(), 1.6f);
            g.drawLine(r.getX(), r.getCentreY(), r.getRight(), r.getCentreY(), 1.2f);
            const float d = r.getWidth() * 0.28f;
            g.fillEllipse(midX - d * 0.5f, r.getCentreY() - d * 0.5f, d, d);
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
            if (auto* laf = dynamic_cast<Theme*>(&getLookAndFeel()))
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

class TitleBar : public juce::Component
{
public:
    static constexpr float pad = WindowShell::pad;
    static constexpr float button = WindowShell::button;
    static constexpr int barHeight = WindowShell::barHeight;

    CircleToggle presetsButton { "Presets", Theme::nova(), CircleIcon::Presets };
    CircleToggle advancedButton { "Advanced", Theme::nova(), CircleIcon::Advanced };
    CircleToggle looperButton { "Looper", Theme::nova(), CircleIcon::Looper };
    std::function<void()> onModeChange;

    RigMode getMode() const { return mode; }
    void setMode(RigMode next, bool notify)
    {
        if (mode == next)
            return;
        mode = next;
        repaint();
        if (notify && onModeChange != nullptr)
            onModeChange();
    }

    TitleBar()
        : shellBar({}),
          minimiseButton(Theme::panel(), Theme::mist(), "-"),
          guitarWord("GUITAR", RigMode::Guitar, *this),
          vocalsWord("VOCALS", RigMode::Vocals, *this),
          vblank(this, [this] { repaint(); })
    {
        setInterceptsMouseClicks(true, true);
        shellBar.onClose = []
        {
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        };
        minimiseButton.onClick = [this]
        {
            if (auto* top = getTopLevelComponent())
                if (auto* peer = top->getPeer())
                    peer->setMinimised(true);
        };
        addAndMakeVisible(shellBar);
        addAndMakeVisible(presetsButton);
        addAndMakeVisible(advancedButton);
        addAndMakeVisible(looperButton);
        addAndMakeVisible(guitarWord);
        addAndMakeVisible(pipe);
        addAndMakeVisible(vocalsWord);
        addAndMakeVisible(minimiseButton);
    }

    void resized() override
    {
        shellBar.setBounds(getLocalBounds());
        const int d = (int) button;
        const int p = (int) pad;
        presetsButton.setBounds(p, p, d, d);
        advancedButton.setBounds(p + d + 8, p, d, d);
        looperButton.setBounds(p + (d + 8) * 2, p, d, d);
        minimiseButton.setBounds(getWidth() - p - d - 8 - d, p, d, d);
        layoutModeWords();
    }

    void lookAndFeelChanged() override
    {
        layoutModeWords();
    }

    void paint(juce::Graphics& g) override
    {
        const float t = (float) juce::Time::getMillisecondCounterHiRes() * 0.001f;
        const float rightSide = pad + button * 2.0f + 8.0f + 8.0f;
        auto bar = getLocalBounds().toFloat();

        juce::Font titleFont = juce::FontOptions(22.0f, juce::Font::bold);
        juce::Font modeFont = juce::FontOptions(16.0f, juce::Font::bold);
        if (auto* laf = dynamic_cast<Theme*>(&getLookAndFeel()))
        {
            titleFont = laf->titleFont(22.0f);
            modeFont = laf->titleFont(16.0f);
        }

        g.setFont(modeFont);
        auto drawMode = [&] (const juce::Rectangle<float>& r, const juce::String& text, bool on)
        {
            g.setColour(on ? Theme::starlight() : Theme::dim());
            g.drawText(text, r, juce::Justification::centred, false);
        };
        drawMode(guitarWord.getBounds().toFloat(), "GUITAR", mode == RigMode::Guitar);
        g.setColour(Theme::dim());
        g.drawText("|", pipe.getBounds().toFloat(), juce::Justification::centred, false);
        drawMode(vocalsWord.getBounds().toFloat(), "VOCALS", mode == RigMode::Vocals);

        auto titleArea = bar.withTrimmedLeft((float) vocalsWord.getRight() + 8.0f)
                             .withTrimmedRight(rightSide);
        const float cx = titleArea.getCentreX();
        const float cy = titleArea.getCentreY();
        const float shift = std::sin(t * 1.35f) * 56.0f;
        juce::ColourGradient grad(Theme::starlight(), cx - 90.0f + shift, cy,
                                  Theme::nova(), cx + 90.0f + shift, cy, false);
        grad.addColour(0.42, juce::Colour(0xfffff4d4));
        grad.addColour(0.68, Theme::nova().brighter(0.15f));

        g.setFont(titleFont);
        g.setGradientFill(grad);
        g.drawText("ToneStar", titleArea, juce::Justification::centred, false);

        juce::Random rng { 0x5A4E };
        for (int i = 0; i < 10; ++i)
        {
            if (titleArea.getWidth() < 40.0f)
                break;
            const float px = titleArea.getX() + 8.0f + rng.nextFloat() * (titleArea.getWidth() - 16.0f);
            const float py = titleArea.getY() + 8.0f + rng.nextFloat() * (titleArea.getHeight() - 16.0f);
            const float twinkle = 0.15f + 0.75f * (0.5f + 0.5f * std::sin(t * (3.2f + (float) i * 0.47f) + (float) i));
            g.setColour(Theme::starlight().withAlpha(twinkle));
            const float s = 1.2f + rng.nextFloat() * 1.6f;
            g.fillEllipse(px, py, s, s);
        }
    }

private:
    class ModeWord : public juce::Component
    {
    public:
        ModeWord(juce::String textToUse, RigMode modeToUse, TitleBar& ownerToUse)
            : text(std::move(textToUse)), wordMode(modeToUse), owner(ownerToUse)
        {
            setMouseCursor(juce::MouseCursor::PointingHandCursor);
        }

        void mouseDown(const juce::MouseEvent&) override
        {
            owner.setMode(wordMode, true);
        }

        void paint(juce::Graphics&) override {}

    private:
        juce::String text;
        RigMode wordMode;
        TitleBar& owner;
    };

    class Pipe : public juce::Component
    {
    public:
        void paint(juce::Graphics&) override {}
    };

    void layoutModeWords()
    {
        juce::Font modeFont = juce::FontOptions(16.0f, juce::Font::bold);
        if (auto* laf = dynamic_cast<Theme*>(&getLookAndFeel()))
            modeFont = laf->titleFont(16.0f);

        auto textWidth = [] (const juce::Font& font, const juce::String& text)
        {
            juce::GlyphArrangement ga;
            ga.addLineOfText(font, text, 0.0f, 0.0f);
            return ga.getBoundingBox(0, ga.getNumGlyphs(), true).getWidth();
        };

        const int guitarW = juce::roundToInt(textWidth(modeFont, "GUITAR")) + 8;
        const int midW = juce::roundToInt(textWidth(modeFont, " | ")) + 4;
        const int vocalsW = juce::roundToInt(textWidth(modeFont, "VOCALS")) + 8;
        const int x0 = (int) (pad + button * 3.0f + 8.0f * 2.0f + 12.0f);
        guitarWord.setBounds(x0, 0, guitarW, getHeight());
        pipe.setBounds(x0 + guitarW, 0, midW, getHeight());
        vocalsWord.setBounds(x0 + guitarW + midW, 0, vocalsW, getHeight());
    }

    WindowShell::Bar shellBar;
    CircleButton minimiseButton;
    ModeWord guitarWord;
    Pipe pipe;
    ModeWord vocalsWord;
    juce::VBlankAttachment vblank;
    RigMode mode = RigMode::Guitar;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TitleBar)
};
