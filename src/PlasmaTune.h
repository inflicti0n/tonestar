#pragma once

#include "PlasmaLook.h"
#include <functional>
#include <vector>

class PlasmaTune : public juce::Component
{
public:
    static constexpr int width = 260;

    PlasmaTune();

    void paint(juce::Graphics&) override;
    void resized() override;

    PlasmaLook getLook() const { return look; }
    std::function<void(const PlasmaLook&)> onChange;

private:
    class Swatch : public juce::Component
    {
    public:
        void setColour(juce::Colour c);
        juce::Colour getColour() const { return colour; }
        std::function<void()> onChange;

        void paint(juce::Graphics&) override;
        void mouseUp(const juce::MouseEvent&) override;

    private:
        juce::Colour colour { juce::Colours::white };
    };

    struct Row
    {
        juce::Label label;
        juce::Slider slider;
        float PlasmaLook::* field = nullptr;
    };

    void addSliderRows(const PlasmaSliderSpec* specs, int count);
    void addColourSlot(const char* name, juce::Colour PlasmaLook::* field);
    void pullFromControls();
    void push();

    PlasmaLook look;
    juce::Viewport viewport;
    juce::Component content;
    juce::Label title { {}, "plasma tune" };
    juce::TextButton saveButton { "Save" };
    juce::TextButton copyButton { "Copy" };
    juce::Label fluidHead { {}, "star fluid" };
    juce::Label ringHead { {}, "fx ring" };
    std::vector<std::unique_ptr<Row>> rows;
    int fluidRowCount = 0;
    int fluidColourCount = 0;

    struct ColourSlot
    {
        juce::Label label;
        Swatch swatch;
        juce::Slider alpha;
        juce::Colour PlasmaLook::* field = nullptr;
    };

    std::vector<std::unique_ptr<ColourSlot>> colours;
    bool layoutReady = false;
    bool layingOut = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PlasmaTune)
};
