#pragma once

#include "Appearance/Theme.h"
#include "Automation/AutomationParam.h"

#include <cstring>

struct DragTip
{
    static juce::String percent(float unit01)
    {
        return juce::String(juce::roundToInt(juce::jlimit(0.0f, 1.0f, unit01) * 100.0f)) + "%";
    }

    static juce::String pan(float lr)
    {
        const int n = juce::roundToInt(juce::jlimit(-1.0f, 1.0f, lr) * 100.0f);
        if (n == 0)
            return "C";
        if (n < 0)
            return "L " + juce::String(-n);
        return "R " + juce::String(n);
    }

    static juce::String signedFixed(float v, int decimals = 2)
    {
        const auto text = juce::String(v, decimals);
        if (v > -0.005f && ! text.startsWithChar('-'))
            return "+" + text;
        return text;
    }

    static juce::String autoValue(int param, float value)
    {
        const auto& spec = AutomationParam::at(param);
        if (std::strcmp(spec.id, "key.root") == 0)
        {
            static constexpr const char* names[] = {
                "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"
            };
            return names[(((int) std::lround(value) % 12) + 12) % 12];
        }
        if (std::strcmp(spec.id, "key.minor") == 0)
            return value >= 0.5f ? "Min" : "Maj";
        if (std::strcmp(spec.id, "shift.mode") == 0)
            return AutomationParam::shiftModeName((int) std::lround(value));
        if (std::strcmp(spec.id, "shift.pitch") == 0 || std::strcmp(spec.id, "shift.formant") == 0)
            return signedFixed(value);
        if (spec.min == 0.0f && spec.max == 1.0f && ! spec.stepped)
            return percent(value);
        return juce::String(value, spec.stepped ? 0 : 2);
    }

    static void show(const juce::Component& owner, juce::Point<float> anchor,
                     const juce::String& line, juce::Colour accent)
    {
        juce::StringArray lines;
        lines.add(line);
        show(owner, anchor, lines, accent);
    }

    static void show(const juce::Component& owner, juce::Point<float> anchor,
                     const juce::StringArray& lines, juce::Colour accent)
    {
        auto* top = owner.getTopLevelComponent();
        if (top == nullptr || lines.isEmpty())
            return;

        auto& layer = overlayFor(*top);
        layer.toFront(false);
        layer.anchor = top->getLocalPoint(&owner, anchor);
        layer.lines = lines;
        layer.accent = accent;
        layer.setVisible(true);
        layer.repaint();
    }

    static void hide()
    {
        if (auto layer = overlayPtr())
        {
            layer->lines.clear();
            layer->setVisible(false);
        }
    }

    static void paint(juce::Graphics& g, const juce::Component& owner,
                      juce::Point<float> anchor, const juce::String& line, juce::Colour accent)
    {
        juce::ignoreUnused(g);
        show(owner, anchor, line, accent);
    }

    static void paint(juce::Graphics& g, const juce::Component& owner,
                      juce::Point<float> anchor, const juce::StringArray& lines, juce::Colour accent)
    {
        juce::ignoreUnused(g);
        show(owner, anchor, lines, accent);
    }

private:
    class Overlay : public juce::Component
    {
    public:
        Overlay()
        {
            setInterceptsMouseClicks(false, false);
            setPaintingIsUnclipped(true);
            setOpaque(false);
        }

        void parentSizeChanged() override
        {
            if (auto* p = getParentComponent())
                setBounds(p->getLocalBounds());
        }

        void paint(juce::Graphics& g) override
        {
            if (lines.isEmpty())
                return;

            juce::Font type { juce::FontOptions(11.0f, juce::Font::bold) };
            if (auto* laf = dynamic_cast<Theme*>(&getLookAndFeel()))
                type = laf->font(11.0f, true);
            g.setFont(type);

            auto textWidth = [] (const juce::Font& font, const juce::String& text)
            {
                juce::GlyphArrangement ga;
                ga.addLineOfText(font, text, 0.0f, 0.0f);
                return ga.getBoundingBox(0, ga.getNumGlyphs(), true).getWidth();
            };

            const float padX = 8.0f;
            const float padY = 5.0f;
            const float lineH = type.getHeight() + 1.0f;
            float w = 0.0f;
            for (const auto& line : lines)
                w = juce::jmax(w, textWidth(type, line));
            w += padX * 2.0f;
            const float h = lineH * (float) lines.size() + padY * 2.0f;

            auto tip = juce::Rectangle<float>(anchor.x - w - 10.0f, anchor.y - h * 0.5f, w, h);
            if (auto* parent = getParentComponent())
            {
                if (tip.getX() < 6.0f)
                    tip.setX(anchor.x + 10.0f);
                if (tip.getY() < 6.0f)
                    tip.setY(anchor.y + 10.0f);
                else if (tip.getBottom() > (float) parent->getHeight() - 6.0f)
                    tip.setY(anchor.y - h - 10.0f);
            }

            g.setColour(Theme::voidFill().withMultipliedAlpha(0.92f));
            g.fillRoundedRectangle(tip, 6.0f);
            g.setColour(accent.withMultipliedAlpha(0.85f));
            g.drawRoundedRectangle(tip, 6.0f, 1.1f);

            auto text = tip.reduced(padX, padY);
            g.setColour(accent);
            for (int i = 0; i < lines.size(); ++i)
            {
                auto row = (i + 1 < lines.size()) ? text.removeFromTop(lineH) : text;
                g.drawText(lines[i], row, juce::Justification::centredLeft, false);
            }
        }

        juce::Point<float> anchor;
        juce::StringArray lines;
        juce::Colour accent { Theme::starlight() };
    };

    static juce::Component::SafePointer<Overlay>& overlayPtr()
    {
        static juce::Component::SafePointer<Overlay> layer;
        return layer;
    }

    static Overlay& overlayFor(juce::Component& top)
    {
        auto& layer = overlayPtr();
        if (layer == nullptr || layer->getParentComponent() != &top)
        {
            auto* next = new Overlay();
            layer = next;
            top.addAndMakeVisible(next);
            next->setBounds(top.getLocalBounds());
            next->toFront(false);
        }
        return *layer;
    }
};
