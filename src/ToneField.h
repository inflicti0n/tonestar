#pragma once

#include "CuteLookAndFeel.h"
#include "FieldEnergy.h"
#include "FieldSpectrum.h"
#include "FxRack.h"
#include "PlasmaLook.h"
#include "ToneStar.h"

#include <array>
#include <functional>

class ToneField : public juce::Component,
                  public juce::SettableTooltipClient
{
public:
    ToneField();

    void paint(juce::Graphics&) override;
    void paintOverChildren(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;

    void setStarValues(const std::array<float, 6>& values, bool notify);
    std::array<float, 6> getStarValues() const;
    void setFxValues(const std::array<float, 8>& values, bool notify);
    std::array<float, 8> getFxValues() const;
    void setBloomShimmer(bool shouldShimmer, bool notify);
    void setFieldEnergy(FieldEnergy next);
    void setFieldSpectrum(const FieldSpectrum& next);
    void setPlasmaLook(const PlasmaLook& next);
    bool getBloomShimmer() const { return bloomShimmer; }

    juce::String getActiveName() const;
    float getActiveAmount() const;
    bool isFxActive() const { return focusKind == Focus::Fx; }

    std::function<void()> onChange;

private:
    enum class Focus { Star, Fx };

    void tick();
    juce::Point<float> fieldCentre() const;
    float starRadius() const;
    float fxZeroRadius() const;
    float fxFullRadius() const;
    float fxAngle(int index) const;
    juce::Point<float> fxSpoke(int index, float amount) const;
    juce::Point<float> fxLabelPoint(int index) const;
    juce::Rectangle<float> fxLabelBounds(int index) const;
    float fxRadiusAtUnit(float t) const;
    float fxAngleAtUnit(float t) const;
    float spectrumAt(float t) const;
    void paintFxSpectrum(juce::Graphics& g);
    float projectToFx(juce::Point<float> p, int index) const;
    int hitFxHandle(juce::Point<float> p) const;
    bool hitBloomLabel(juce::Point<float> p) const;
    void setFxFromEvent(const juce::MouseEvent& e);
    void refreshTooltip(juce::Point<float> p);
    const char* bloomLabel() const;

    ToneStar star;
    PlasmaLook look;
    FieldSpectrum spectrum {};
    FieldSpectrum spectrumShown {};
    float ringTime = 0.0f;
    std::array<float, 8> fxTarget {};
    std::array<float, 8> fxDisplay {};
    bool bloomShimmer = false;
    int dragFx = -1;
    int hoverFx = -1;
    int lastFx = 0;
    Focus focusKind = Focus::Star;
    juce::VBlankAttachment vblank;

    static constexpr float envelopeT = 1.55f;
    static constexpr float fxFullT = 2.08f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ToneField)
};
