#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>

class Theme : public juce::LookAndFeel_V4
{
public:
    static juce::Colour voidFill() { return juce::Colour(0xff0b0e14); }
    static juce::Colour panel() { return juce::Colour(0xff141821); }
    static juce::Colour starlight() { return juce::Colour(0xffe8d5a3); }
    static juce::Colour mist() { return juce::Colour(0xffc8d0dc); }
    static juce::Colour dim() { return juce::Colour(0xff7a8494); }
    static juce::Colour nova() { return juce::Colour(0xfff0a36b); }
    static juce::Colour flare() { return juce::Colour(0xffe85d6a); }
    static juce::Colour onAccent() { return juce::Colour(0xff0b0e14); }

    static juce::Colour cream() { return juce::Colour(0xfffff3ea); }
    static juce::Colour card() { return juce::Colour(0xfffff7f2); }
    static juce::Colour blush() { return juce::Colour(0xffffd8c4); }
    static juce::Colour peach() { return juce::Colour(0xffffb89a); }
    static juce::Colour pink() { return juce::Colour(0xffffb3c6); }
    static juce::Colour rose() { return juce::Colour(0xfff48ca3); }
    static juce::Colour coral() { return juce::Colour(0xffe85d6a); }
    static juce::Colour ink() { return mist(); }
    static juce::Colour mutedInk() { return dim(); }
    static juce::Colour meterTrack() { return juce::Colour(0xffffe0d4); }
    static float corner() { return 8.0f; }

    Theme();

    juce::Font font(float height, bool bold = false) const;
    juce::Font titleFont(float height) const;
    juce::Typeface::Ptr getTypefaceForFont(const juce::Font& font) override;
    juce::Font getTextButtonFont(juce::TextButton&, int) override { return font(16.0f, true); }
    juce::Font getLabelFont(juce::Label&) override { return font(15.0f); }
    juce::Font getComboBoxFont(juce::ComboBox&) override { return font(15.0f); }
    juce::Font getPopupMenuFont() override { return font(15.0f); }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                          juce::Slider&) override
    {
        const auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(8.0f);
        const auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto centre = bounds.getCentre();
        const auto toAngle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
        const float lineW = 7.0f;
        const float arcR = juce::jmax(1.0f, radius - lineW * 0.5f);
        const auto well = juce::Rectangle<float>(centre.x - radius, centre.y - radius,
                                                 radius * 2.0f, radius * 2.0f);

        g.setColour(panel());
        g.fillEllipse(well);

        juce::Path valueArc;
        valueArc.addCentredArc(centre.x, centre.y, arcR, arcR, 0.0f,
                               rotaryStartAngle, toAngle, true);
        g.setColour(starlight());
        g.strokePath(valueArc, juce::PathStrokeType(lineW, juce::PathStrokeType::curved,
                                                    juce::PathStrokeType::rounded));

        const auto tick = juce::Point<float>(centre.x + std::sin(toAngle) * arcR,
                                             centre.y - std::cos(toAngle) * arcR);
        g.setColour(nova());
        g.fillEllipse(tick.x - lineW * 0.5f, tick.y - lineW * 0.5f, lineW, lineW);
    }

    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        auto bounds = button.getLocalBounds().toFloat();
        auto colour = backgroundColour;

        if (shouldDrawButtonAsDown)
            colour = colour.brighter(0.08f);
        else if (shouldDrawButtonAsHighlighted)
            colour = colour.brighter(0.06f);

        g.setColour(colour);
        g.fillRoundedRectangle(bounds, corner());
    }

    void drawComboBox(juce::Graphics& g, int width, int height, bool,
                      int, int, int, int, juce::ComboBox& box) override
    {
        auto bounds = juce::Rectangle<int>(0, 0, width, height).toFloat();
        g.setColour(findColour(juce::ComboBox::backgroundColourId));
        g.fillRoundedRectangle(bounds, corner());

        const auto arrow = juce::Rectangle<float>(width - 22.0f, 0.0f, 14.0f, (float) height);
        juce::Path p;
        p.addTriangle(arrow.getX(), arrow.getCentreY() - 3.0f,
                      arrow.getRight(), arrow.getCentreY() - 3.0f,
                      arrow.getCentreX(), arrow.getCentreY() + 4.0f);
        g.setColour(box.findColour(juce::ComboBox::arrowColourId));
        g.fillPath(p);
    }

    void drawTickBox(juce::Graphics& g, juce::Component&, float x, float y, float w, float h,
                     bool ticked, bool, bool, bool) override
    {
        auto bounds = juce::Rectangle<float>(x, y, w, h).reduced(1.0f);
        g.setColour(ticked ? nova() : panel());
        g.fillRoundedRectangle(bounds, 4.0f);
    }

    void fillTextEditorBackground(juce::Graphics& g, int, int, juce::TextEditor& editor) override
    {
        g.setColour(editor.findColour(juce::TextEditor::backgroundColourId));
        g.fillRoundedRectangle(editor.getLocalBounds().toFloat(), corner());
    }

    void drawTextEditorOutline(juce::Graphics&, int, int, juce::TextEditor&) override {}

private:
    juce::Typeface::Ptr regularFace;
    juce::Typeface::Ptr boldFace;
    juce::Typeface::Ptr titleFace;
};
