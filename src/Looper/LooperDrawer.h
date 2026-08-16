#pragma once

#include "Looper/LooperEngine.h"
#include "Appearance/TitleBar.h"

#include <functional>

class LooperDrawer : public juce::Component
{
public:
    static constexpr int simpleHeight = 156;
    static constexpr int advancedHeight = 248;

    LooperDrawer();

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;

    int height() const { return advancedMode ? advancedHeight : simpleHeight; }
    bool isAdvanced() const { return advancedMode; }
    void setAdvanced(bool shouldBeAdvanced, bool notify);
    void refresh();
    void setEngine(LooperEngine* engineToUse);
    void setQuantize(bool shouldQuantize, bool notify);
    bool isQuantize() const { return quantizeOn; }
    void pedalDown();
    void pedalUp();
    int pedalPhrase() const;

    std::function<void()> onModeChange;
    std::function<void()> onChanged;
    std::function<void(bool)> onQuantizeChange;

private:
    class PhraseStrip : public juce::Component
    {
    public:
        PhraseStrip(int indexToUse);

        void paint(juce::Graphics&) override;
        void resized() override;
        void mouseDown(const juce::MouseEvent&) override;
        void mouseDrag(const juce::MouseEvent&) override;
        void mouseUp(const juce::MouseEvent&) override;
        void tick();
        void setSnapshot(LooperEngine::State stateToUse, float playheadToUse,
                         float levelToUse, bool armedToUse, bool hasContentToUse);
        void setAdvancedLook(bool shouldBeAdvanced);

        std::function<void(int)> onTap;
        std::function<void(int)> onDoubleTap;
        std::function<void(int)> onHold;
        std::function<void(int)> onStop;
        std::function<void(int)> onArm;
        std::function<void(int, float)> onLevel;

    private:
        bool inRec(juce::Point<float> p) const;
        bool inStop(juce::Point<float> p) const;
        bool inLevel(juce::Point<float> p) const;
        void setLevelFromY(float y);
        juce::Rectangle<float> thumbBounds() const;
        juce::Colour statusColour() const;

        int index = 0;
        bool advancedLook = false;
        LooperEngine::State state = LooperEngine::State::Empty;
        float playhead = 0.0f;
        float level = 1.0f;
        bool armed = false;
        bool hasContent = false;

        juce::Rectangle<float> recBounds;
        juce::Rectangle<float> stopBounds;
        juce::Rectangle<float> levelBounds;

        bool recDown = false;
        bool stopDown = false;
        bool draggingLevel = false;
        bool sentHold = false;
        bool pendingTap = false;
        juce::uint32 downAt = 0;
        juce::uint32 tapUpAt = 0;
    };

    void bindStrip(PhraseStrip& strip);
    void tickPedal();

    LooperEngine* engine = nullptr;
    PhraseStrip stripA { 0 };
    PhraseStrip stripB { 1 };
    CircleToggle quantizeButton { "Quantize", Theme::nova(), CircleIcon::Quantize };
    bool advancedMode = false;
    bool quantizeOn = false;
    juce::Rectangle<int> titleBounds;
    bool pedalHeld = false;
    bool pedalSentHold = false;
    bool pedalPendingTap = false;
    juce::uint32 pedalDownAt = 0;
    juce::uint32 pedalTapUpAt = 0;
};
