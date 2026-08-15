#pragma once

#include "AdvancedDrawer.h"
#include "AppSettings.h"
#include "CabWidget.h"
#include "CuteLookAndFeel.h"
#include "GuitarProcessor.h"
#include "LooperDrawer.h"
#include "PlasmaTune.h"
#include "PresetDrawer.h"
#include "PresetStore.h"
#include "ToneField.h"
#include "ToneSlug.h"
#include "WindowChrome.h"

#include <juce_audio_utils/juce_audio_utils.h>

class MainComponent : public juce::Component,
                      private juce::Timer,
                      private juce::ChangeListener
{
public:
    MainComponent();
    ~MainComponent() override;
    void startAudio();
    void applyWindowSize();
    void enableSelfTest();
    void scheduleSelfTest();

    void paint(juce::Graphics&) override;
    void resized() override;
    bool hitTest(int x, int y) override;
    bool keyPressed(const juce::KeyPress&) override;
    bool keyStateChanged(bool isKeyDown) override;

private:
    void timerCallback() override;
    void changeListenerCallback(juce::ChangeBroadcaster*) override;
    void refreshInputChannels();
    void showDeviceSettings();
    void loadSettings();
    void saveSettings();
    void markDirty();
    void syncFieldToProcessor();
    void updateLatencyLabel();
    void copySlug();
    void tryImportSlug();
    void applyPatch(const ToneSlug::Patch& patch);
    void flashSlugError();
    void showCurrentSlug();
    juce::String currentSlug() const;
    void startDebugLog();
    void stopDebugLog();
    bool isEditingText() const;
    bool looperPedalArmed() const;
    bool windowIsFocused() const;
    void releaseSpacePedal();
    void togglePlasmaTune();
    void runSelfTest();
    int windowWidth() const;
    int windowHeight() const;

    CuteLookAndFeel lookAndFeel;
    WindowChrome chrome;
    juce::AudioDeviceManager deviceManager;
    juce::AudioProcessorPlayer player;
    GuitarProcessor processor;
    ToneField field;
    CabWidget cab;
    PresetStore presetStore;
    PresetDrawer drawer { presetStore };
    AdvancedDrawer advanced;
    LooperDrawer looperDrawer;
    PlasmaTune plasmaTune;
    juce::TooltipWindow tooltipWindow { this, 400 };

    juce::Label latencyLabel;
    juce::ComboBox inputChannelBox;
    juce::Label inputChannelLabel { {}, "Input" };
    juce::Slider inputGain;
    juce::Slider outputGain;
    juce::Label inputGainLabel { {}, "In" };
    juce::Label outputGainLabel { {}, "Out" };
    CircleToggle muteButton { "Mute", CuteLookAndFeel::flare(), CircleIcon::Mute };
    CircleToggle binauralButton { "Binaural", CuteLookAndFeel::nova(), CircleIcon::Binaural };
    CircleToggle debugButton { "Debug", CuteLookAndFeel::starlight(), CircleIcon::Debug };
    juce::TextButton devicesButton { "Devices" };
    juce::TextButton copyButton { "Copy" };
    juce::TextButton applyButton { "Apply" };
    juce::TextEditor slugField;
    juce::Label slugLabel { {}, "Share" };

    juce::Rectangle<float> meterBounds;
    float meterLevel = 0.0f;
    bool meterClipped = false;
    int clipHoldTicks = 0;
    bool settingsDirty = false;
    juce::int64 dirtyAt = 0;
    int slugFlashTicks = 0;
    bool restoring = false;
    int preferredInputChannel = 0;
    bool advancedWasOpen = false;
    bool applyingWindowSize = false;
    bool spacePedalDown = false;
    bool selfTesting = false;
    bool audioStartedOk = false;
    DebugLog debugLog;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
