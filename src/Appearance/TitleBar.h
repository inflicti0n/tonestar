#pragma once

#include "App/RigMode.h"
#include "Appearance/WindowShell.h"

#include <juce_events/juce_events.h>
#include <cmath>
#include <functional>

enum class CircleIcon { Mute, Binaural, Debug, Presets, Advanced, Record, Folder, Export, Import, Metro, Tune, Looper, Loop, Quantize, Play, Pause, Stop, Automate, Process };

inline void paintProcessStar(juce::Graphics& g, juce::Rectangle<float> r, juce::Colour colour)
{
    const auto c = r.getCentre();
    const float reach = juce::jmin(r.getWidth(), r.getHeight()) * 0.42f;
    juce::Path path;
    path.startNewSubPath(c.x, c.y - reach * 1.15f);
    path.cubicTo({ c.x + reach * 0.12f, c.y - reach * 0.12f },
                 { c.x + reach * 0.12f, c.y - reach * 0.12f },
                 { c.x + reach * 1.35f, c.y });
    path.cubicTo({ c.x + reach * 0.12f, c.y + reach * 0.12f },
                 { c.x + reach * 0.12f, c.y + reach * 0.12f },
                 { c.x, c.y + reach * 1.15f });
    path.cubicTo({ c.x - reach * 0.12f, c.y + reach * 0.12f },
                 { c.x - reach * 0.12f, c.y + reach * 0.12f },
                 { c.x - reach * 1.35f, c.y });
    path.cubicTo({ c.x - reach * 0.12f, c.y - reach * 0.12f },
                 { c.x - reach * 0.12f, c.y - reach * 0.12f },
                 { c.x, c.y - reach * 1.15f });
    path.closeSubPath();
    g.setColour(colour);
    g.fillPath(path);
}

inline void paintAutomateIcon(juce::Graphics& g, juce::Rectangle<float> r, juce::Colour colour)
{
    g.setColour(colour);
    juce::Path curve;
    curve.startNewSubPath(r.getX(), r.getBottom() - r.getHeight() * 0.22f);
    curve.quadraticTo(r.getCentreX(), r.getY() + r.getHeight() * 0.02f,
                      r.getRight(), r.getY() + r.getHeight() * 0.28f);
    g.strokePath(curve, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved,
                                            juce::PathStrokeType::rounded));
    const float d = juce::jmax(2.4f, r.getWidth() * 0.22f);
    g.fillEllipse(r.getX() + r.getWidth() * 0.06f - d * 0.5f,
                  r.getBottom() - r.getHeight() * 0.28f - d * 0.5f, d, d);
    g.fillEllipse(r.getRight() - r.getWidth() * 0.08f - d * 0.5f,
                  r.getY() + r.getHeight() * 0.28f - d * 0.5f, d, d);
}

inline void paintMuteIcon(juce::Graphics& g, juce::Rectangle<float> r, juce::Colour colour, bool muted)
{
    // Heroicons 24 outline: speaker-wave / speaker-x-mark (MIT).
    static const char* wave =
        "M19.114 5.636a9 9 0 0 1 0 12.728M16.463 8.288a5.25 5.25 0 0 1 0 7.424M6.75 8.25l4.72-4.72a.75.75 0 0 1 1.28.53v15.88a.75.75 0 0 1-1.28.53l-4.72-4.72H4.51c-.88 0-1.704-.507-1.938-1.354A9.009 9.009 0 0 1 2.25 12c0-.83.112-1.633.322-2.396C2.806 8.756 3.63 8.25 4.51 8.25H6.75Z";
    static const char* xmark =
        "M17.25 9.75 19.5 12m0 0 2.25 2.25M19.5 12l2.25-2.25M19.5 12l-2.25 2.25m-10.5-6 4.72-4.72a.75.75 0 0 1 1.28.53v15.88a.75.75 0 0 1-1.28.53l-4.72-4.72H4.51c-.88 0-1.704-.507-1.938-1.354A9.009 9.009 0 0 1 2.25 12c0-.83.112-1.633.322-2.396C2.806 8.756 3.63 8.25 4.51 8.25H6.75Z";

    auto path = juce::Drawable::parseSVGPath(muted ? xmark : wave);
    const float size = juce::jmin(r.getWidth(), r.getHeight());
    const float scale = size / 24.0f;
    const auto t = juce::AffineTransform::scale(scale, scale)
                       .translated(r.getCentreX() - 12.0f * scale,
                                   r.getCentreY() - 12.0f * scale);
    g.setColour(colour);
    g.strokePath(path, juce::PathStrokeType(juce::jmax(1.4f, 1.5f * scale),
                                            juce::PathStrokeType::curved,
                                            juce::PathStrokeType::rounded), t);
}

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
            paintMuteIcon(g, r, colour, getToggleState());
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
            g.fillRoundedRectangle(r.getX(), r.getBottom() - r.getHeight() * 0.28f,
                                   r.getWidth(), r.getHeight() * 0.28f, 1.4f);
            g.fillRect(mid - 1.1f, r.getY() + r.getHeight() * 0.22f, 2.2f, r.getHeight() * 0.50f);
            juce::Path head;
            head.addTriangle(mid, r.getY() + r.getHeight() * 0.02f,
                             mid - r.getWidth() * 0.30f, r.getY() + r.getHeight() * 0.36f,
                             mid + r.getWidth() * 0.30f, r.getY() + r.getHeight() * 0.36f);
            g.fillPath(head);
        }
        else if (icon == CircleIcon::Import)
        {
            const float mid = r.getCentreX();
            g.fillRoundedRectangle(r.getX(), r.getBottom() - r.getHeight() * 0.28f,
                                   r.getWidth(), r.getHeight() * 0.28f, 1.4f);
            g.fillRect(mid - 1.1f, r.getY() + r.getHeight() * 0.06f, 2.2f, r.getHeight() * 0.42f);
            juce::Path head;
            head.addTriangle(mid, r.getY() + r.getHeight() * 0.70f,
                             mid - r.getWidth() * 0.30f, r.getY() + r.getHeight() * 0.38f,
                             mid + r.getWidth() * 0.30f, r.getY() + r.getHeight() * 0.38f);
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
        else if (icon == CircleIcon::Automate)
        {
            paintAutomateIcon(g, r, colour);
        }
        else if (icon == CircleIcon::Process)
        {
            paintProcessStar(g, r.reduced(1.2f), colour);
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

class PieToggle : public juce::Component,
                  public juce::SettableTooltipClient
{
public:
    enum class Slice { Process, Mute, Automate, None };

    PieToggle()
    {
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
        setWantsKeyboardFocus(false);
        setRepaintsOnMouseActivity(true);
    }

    void setProcessOn(bool on)
    {
        if (processOn == on)
            return;
        processOn = on;
        repaint();
    }

    void setMuteOn(bool on)
    {
        if (muteOn == on)
            return;
        muteOn = on;
        repaint();
    }

    void setAutoOn(bool on)
    {
        if (autoOn == on)
            return;
        autoOn = on;
        repaint();
    }

    void setProcessEnabled(bool on)
    {
        if (processEnabled == on)
            return;
        processEnabled = on;
        if (! processEnabled)
            processOn = false;
        repaint();
    }

    bool isProcessOn() const { return processOn; }
    bool isMuteOn() const { return muteOn; }
    bool isAutoOn() const { return autoOn; }

    std::function<void(bool)> onProcess;
    std::function<void(bool)> onMute;
    std::function<void(bool)> onAutomate;

    void paint(juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        const float d = juce::jmin(bounds.getWidth(), bounds.getHeight());
        const auto disc = bounds.withSizeKeepingCentre(d, d);
        const auto c = disc.getCentre();
        const float overlap = 0.008f;
        const float pi = juce::MathConstants<float>::pi;

        const auto offFill = Theme::panel().interpolatedWith(Theme::voidFill(), 0.68f);
        g.setColour(offFill);
        g.fillEllipse(disc);

        auto drawSlice = [&](float from, float to, bool on, juce::Colour colour, bool enabled, bool lit)
        {
            juce::Path p;
            p.addPieSegment(disc, from - overlap, to + overlap, 0.0f);
            auto fill = on ? colour : offFill;
            if (! enabled)
                fill = Theme::voidFill();
            if (lit && enabled)
                fill = fill.brighter(0.08f);
            g.setColour(fill);
            g.fillPath(p);
        };

        drawSlice(-pi / 3.0f, pi / 3.0f, processOn && processEnabled, Theme::starlight(),
                  processEnabled, hover == Slice::Process);
        drawSlice(pi / 3.0f, pi, autoOn, Theme::nova(), true, hover == Slice::Automate);
        drawSlice(pi, pi * 5.0f / 3.0f, muteOn, Theme::flare(), true, hover == Slice::Mute);

        auto iconAt = [&](float midAngle)
        {
            const float sliceD = disc.getWidth();
            const float rad = sliceD * 0.27f;
            const float s = sliceD * 0.28f;
            return juce::Rectangle<float>(s, s).withCentre({ c.x + std::sin(midAngle) * rad,
                                                            c.y - std::cos(midAngle) * rad });
        };

        const auto processGlyph = (processOn && processEnabled) ? Theme::onAccent()
            : (processEnabled ? Theme::dim() : Theme::dim().withAlpha(0.35f));
        paintProcessStar(g, iconAt(0.0f), processGlyph);
        paintMuteIcon(g, iconAt(pi * 4.0f / 3.0f), muteOn ? Theme::onAccent() : Theme::dim(), muteOn);
        paintAutomateIcon(g, iconAt(pi * 2.0f / 3.0f), autoOn ? Theme::onAccent() : Theme::dim());
    }

    void mouseMove(const juce::MouseEvent& e) override
    {
        const auto next = sliceAt(e.position);
        if (next == hover)
            return;
        hover = next;
        setTooltip(tooltipFor(next));
        repaint();
    }

    void mouseExit(const juce::MouseEvent&) override
    {
        if (hover == Slice::None)
            return;
        hover = Slice::None;
        setTooltip({});
        repaint();
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        switch (sliceAt(e.position))
        {
            case Slice::Process:
                if (! processEnabled)
                    return;
                processOn = ! processOn;
                if (onProcess != nullptr)
                    onProcess(processOn);
                break;
            case Slice::Mute:
                muteOn = ! muteOn;
                if (onMute != nullptr)
                    onMute(muteOn);
                break;
            case Slice::Automate:
                autoOn = ! autoOn;
                if (onAutomate != nullptr)
                    onAutomate(autoOn);
                break;
            case Slice::None:
                break;
        }
        repaint();
    }

private:
    static juce::String tooltipFor(Slice s)
    {
        if (s == Slice::Process)
            return "Process";
        if (s == Slice::Mute)
            return "Mute";
        if (s == Slice::Automate)
            return "Automation";
        return {};
    }

    Slice sliceAt(juce::Point<float> p) const
    {
        const auto bounds = getLocalBounds().toFloat();
        const float d = juce::jmin(bounds.getWidth(), bounds.getHeight());
        const auto c = bounds.getCentre();
        const float dx = p.x - c.x;
        const float dy = p.y - c.y;
        const float dist = std::sqrt(dx * dx + dy * dy);
        if (dist > d * 0.5f)
            return Slice::None;

        float ang = std::atan2(dx, -dy);
        if (ang < 0.0f)
            ang += juce::MathConstants<float>::twoPi;

        const float pi = juce::MathConstants<float>::pi;
        if (ang < pi / 3.0f || ang > pi * 5.0f / 3.0f)
            return Slice::Process;
        if (ang < pi)
            return Slice::Automate;
        return Slice::Mute;
    }

    bool processOn = false;
    bool muteOn = false;
    bool autoOn = false;
    bool processEnabled = true;
    Slice hover = Slice::None;
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
