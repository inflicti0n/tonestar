#pragma once

#include "Cab/Acoustics.h"
#include "Appearance/Theme.h"

#include <functional>

class CabWidget : public juce::Component,
                  public juce::SettableTooltipClient
{
public:
    static constexpr float comboLand = 0.15f;
    static constexpr float twinLand = 0.50f;
    static constexpr float stackLand = 0.85f;
    static constexpr float openLand = 0.25f;
    static constexpr float closedLand = 0.75f;

    CabWidget();

    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseEnter(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    void setSizeAmount(float value, bool notify);
    void setBackAmount(float value, bool notify);
    float getSizeAmount() const { return sizeTarget; }
    float getBackAmount() const { return backTarget; }

    std::function<void()> onChange;

private:
    void tick();
    void notifyIf(bool shouldNotify);
    void punch();
    void cycleSize();
    void toggleBack();
    void fillRound(juce::Graphics& g, juce::Rectangle<float> r, float radius, juce::Colour c) const;
    void drawSpeaker(juce::Graphics& g, juce::Point<float> c, float radius, float openness) const;
    void drawCabinet(juce::Graphics& g, juce::Rectangle<float> body, int speakers, float openness,
                     bool withPanel) const;
    void drawHead(juce::Graphics& g, juce::Rectangle<float> body) const;

    float sizeTarget = Acoustics::defaultSize;
    float sizeDisplay = Acoustics::defaultSize;
    float backTarget = Acoustics::defaultBack;
    float backDisplay = Acoustics::defaultBack;
    float punchScale = 1.0f;
    bool hovered = false;
    bool pressed = false;
    bool pressWasLeft = false;
    juce::VBlankAttachment vblank;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CabWidget)
};
