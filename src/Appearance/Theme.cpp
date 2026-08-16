#include "Appearance/Theme.h"
#include "BinaryData.h"

Theme::Theme()
{
    regularFace = juce::Typeface::createSystemTypefaceFor(BinaryData::GaeguRegular_ttf,
                                                          BinaryData::GaeguRegular_ttfSize);
    boldFace = juce::Typeface::createSystemTypefaceFor(BinaryData::GaeguBold_ttf,
                                                       BinaryData::GaeguBold_ttfSize);
    titleFace = juce::Typeface::createSystemTypefaceFor(BinaryData::SpaceGroteskSemiBold_ttf,
                                                        BinaryData::SpaceGroteskSemiBold_ttfSize);

    if (regularFace != nullptr)
        setDefaultSansSerifTypeface(regularFace);

    setColour(juce::ResizableWindow::backgroundColourId, voidFill());
    setColour(juce::Label::textColourId, mist());
    setColour(juce::Slider::textBoxTextColourId, mist());
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::ComboBox::backgroundColourId, panel());
    setColour(juce::ComboBox::textColourId, mist());
    setColour(juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    setColour(juce::ComboBox::arrowColourId, nova());
    setColour(juce::PopupMenu::backgroundColourId, panel());
    setColour(juce::PopupMenu::textColourId, mist());
    setColour(juce::PopupMenu::highlightedBackgroundColourId, nova());
    setColour(juce::PopupMenu::highlightedTextColourId, onAccent());
    setColour(juce::TextButton::buttonColourId, panel());
    setColour(juce::TextButton::buttonOnColourId, nova());
    setColour(juce::TextButton::textColourOffId, mist());
    setColour(juce::TextButton::textColourOnId, onAccent());
    setColour(juce::ToggleButton::textColourId, mist());
    setColour(juce::ToggleButton::tickColourId, nova());
    setColour(juce::ToggleButton::tickDisabledColourId, dim());
    setColour(juce::TextEditor::backgroundColourId, panel());
    setColour(juce::TextEditor::textColourId, mist());
    setColour(juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    setColour(juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::TextEditor::highlightedTextColourId, onAccent());
    setColour(juce::TextEditor::highlightColourId, nova());
}

juce::Font Theme::font(float height, bool bold) const
{
    const auto face = (bold && boldFace != nullptr) ? boldFace : regularFace;
    if (face == nullptr)
        return juce::Font(juce::FontOptions(height));

    return juce::Font(juce::FontOptions(face).withHeight(height));
}

juce::Font Theme::titleFont(float height) const
{
    if (titleFace == nullptr)
        return font(height, true);

    return juce::Font(juce::FontOptions(titleFace).withHeight(height))
        .withExtraKerningFactor(0.04f);
}

juce::Typeface::Ptr Theme::getTypefaceForFont(const juce::Font& font)
{
    if (font.isBold() && boldFace != nullptr)
        return boldFace;

    if (regularFace != nullptr)
        return regularFace;

    return juce::LookAndFeel_V4::getTypefaceForFont(font);
}
