#pragma once

#include "Automation/AutomationBank.h"
#include "Tape/TapeEngine.h"
#include "Tape/TapeTimeline.h"

#include <functional>

class AutomationView : public juce::Component,
                       public juce::SettableTooltipClient
{
public:
    static constexpr int height = 132;

    AutomationView();

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    void setTape(TapeEngine* engineToUse);
    void setLane(int laneToUse);
    void setTimeline(TapeTimeline* timelineToUse);
    void refresh();

    std::function<VocalStamp()> getLiveStamp;
    std::function<void()> onChanged;
    std::function<void()> onEdited;
    std::function<void()> onViewChanged;

private:
    enum class Drag { None, Value, Time, ViewPan };

    juce::Rectangle<float> graphBounds() const;
    juce::Rectangle<float> plusBounds() const;
    juce::Rectangle<float> enableBounds() const;
    juce::Rectangle<float> clearBounds() const;
    juce::Rectangle<float> chipBounds(int group) const;
    juce::Rectangle<float> knobBounds(int keyIndex, int paramIndex) const;
    float valueToY(const AutomationParam& spec, float value) const;
    float yToValue(const AutomationParam& spec, float y) const;
    float curveYAt(int paramIndex, int sample) const;
    bool groupVisible(int group) const;
    AutomationTrack* track();
    const AutomationTrack* track() const;
    void notify();
    void insertHere();
    void addNodeAt(juce::Point<float> p, int param);
    int hitKnob(juce::Point<float> p, int& paramOut) const;
    int hitCurve(juce::Point<float> p, int& paramOut) const;
    int hitKeyLine(juce::Point<float> p) const;
    int hitChip(juce::Point<float> p) const;
    void applyHoverTooltip();
    void showDragTip();

    TapeEngine* tape = nullptr;
    TapeTimeline* timeline = nullptr;
    int lane = 0;
    Drag drag = Drag::None;
    int dragKey = -1;
    int dragParam = -1;
    int hoverKey = -1;
    int hoverParam = -1;
    int hoverChip = -1;
    int hoverCurve = -1;
    int viewPanStart = 0;
    int viewPanX = 0;
};
