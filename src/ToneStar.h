#pragma once

#include "CuteLookAndFeel.h"
#include "FieldEnergy.h"
#include "PlasmaLook.h"
#include "StarPlasma.h"
#include <array>
#include <cstdint>
#include <functional>

class ToneStar : public juce::Component
{
public:
    static constexpr int numAxes = 6;
    static const char* axisName(int index);

    ToneStar();

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;

    void setValues(const std::array<float, 6>& values, bool notify);
    void setFieldEnergy(FieldEnergy next);
    void setPlasmaLook(const PlasmaLook& next);
    std::array<float, 6> getValues() const;
    int getActiveAxis() const;
    bool isDragging() const { return dragAxis >= 0; }
    int getHoverAxis() const { return hoverAxis; }
    juce::Point<float> getCentre() const { return centre; }
    float getRadius() const { return radius; }

    std::function<void()> onChange;

private:
    void tick();
    void syncPlasma();
    juce::Point<float> spokeVisual(int index, float visualT) const;
    juce::Point<float> spokePoint(int index, float value) const;
    float visualRadius(float value) const;
    float projectToSpoke(juce::Point<float> p, int index) const;
    int hitHandle(juce::Point<float> p) const;
    juce::Path axisPath(const std::array<juce::Point<float>, 6>& points, float corner) const;
    void setAxisFromEvent(const juce::MouseEvent& e);

    StarPlasma plasma;
    std::array<float, 6> target { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    std::array<float, 6> display { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    int dragAxis = -1;
    int hoverAxis = -1;
    juce::Point<float> centre;
    float radius = 90.0f;
    static constexpr float innerPad = 0.42f;
    juce::VBlankAttachment vblank;
    uint32_t lastPlasmaSerial = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ToneStar)
};
