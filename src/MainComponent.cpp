#include "MainComponent.h"
#include "AppLog.h"
#include <juce_gui_basics/juce_gui_basics.h>

MainComponent::MainComponent()
{
    AppLog::note("MainComponent ctor");
    setLookAndFeel(&lookAndFeel);
    setOpaque(true);
    setWantsKeyboardFocus(true);

    addAndMakeVisible(chrome);

    latencyLabel.setJustificationType(juce::Justification::centredRight);
    latencyLabel.setColour(juce::Label::textColourId, CuteLookAndFeel::dim());
    latencyLabel.setFont(lookAndFeel.font(13.0f));
    addAndMakeVisible(latencyLabel);

    inputChannelLabel.setJustificationType(juce::Justification::centredLeft);
    inputChannelLabel.setFont(lookAndFeel.font(15.0f, true));
    addAndMakeVisible(inputChannelLabel);

    inputChannelBox.onChange = [this]
    {
        preferredInputChannel = inputChannelBox.getSelectedItemIndex();
        processor.setInputChannel(preferredInputChannel);
        markDirty();
    };
    addAndMakeVisible(inputChannelBox);

    auto setupGainSlider = [](juce::Slider& slider, float minDb, float maxDb, float startDb)
    {
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, 18);
        slider.setRange(minDb, maxDb, 0.1);
        slider.setValue(startDb, juce::dontSendNotification);
        slider.setTextValueSuffix(" dB");
        slider.setRotaryParameters(juce::degreesToRadians(225.0f),
                                   juce::degreesToRadians(495.0f), true);
    };

    setupGainSlider(inputGain, -12.0f, 24.0f, 0.0f);
    inputGain.onValueChange = [this]
    {
        processor.setInputGainDb((float) inputGain.getValue());
        markDirty();
    };
    addAndMakeVisible(inputGain);
    inputGainLabel.setJustificationType(juce::Justification::centred);
    inputGainLabel.setFont(lookAndFeel.font(15.0f, true));
    addAndMakeVisible(inputGainLabel);

    setupGainSlider(outputGain, -60.0f, 6.0f, 0.0f);
    outputGain.onValueChange = [this]
    {
        processor.setOutputGainDb((float) outputGain.getValue());
        markDirty();
    };
    addAndMakeVisible(outputGain);
    outputGainLabel.setJustificationType(juce::Justification::centred);
    outputGainLabel.setFont(lookAndFeel.font(15.0f, true));
    addAndMakeVisible(outputGainLabel);

    cab.onChange = [this]
    {
        processor.setCabSize(cab.getSizeAmount());
        processor.setCabBack(cab.getBackAmount());
        showCurrentSlug();
        markDirty();
    };
    addAndMakeVisible(cab);

    muteButton.onClick = [this]
    {
        processor.setMuted(muteButton.getToggleState());
        markDirty();
    };
    addAndMakeVisible(muteButton);

    binauralButton.onClick = [this]
    {
        processor.setBinaural(binauralButton.getToggleState());
        showCurrentSlug();
        markDirty();
    };
    addAndMakeVisible(binauralButton);

    debugButton.onClick = [this]
    {
        if (debugButton.getToggleState())
            startDebugLog();
        else
            stopDebugLog();
        logsButton.setVisible(debugButton.getToggleState());
        resized();
    };
    addAndMakeVisible(debugButton);

    logsButton.setClickingTogglesState(false);
    logsButton.onClick = []
    {
        auto dir = AppLog::directory();
        dir.createDirectory();
        dir.revealToUser();
    };
    addAndMakeVisible(logsButton);
    logsButton.setVisible(false);

    chrome.presetsButton.onClick = [this]
    {
        applyWindowSize();
        markDirty();
    };

    chrome.advancedButton.onClick = [this]
    {
        applyWindowSize();
        markDirty();
    };

    chrome.looperButton.onClick = [this]
    {
        applyWindowSize();
        markDirty();
    };

    devicesButton.onClick = [this] { showDeviceSettings(); };
    devicesButton.setWantsKeyboardFocus(false);
    addAndMakeVisible(devicesButton);

    drawer.getCurrentSlug = [this] { return currentSlug(); };
    drawer.onLoad = [this](const juce::String& slug)
    {
        ToneSlug::Patch patch;
        if (ToneSlug::decode(slug, patch))
            applyPatch(patch);
    };
    addAndMakeVisible(drawer);

    advanced.onBpmChange = [this](float bpm)
    {
        processor.setMetroBpm(bpm);
        markDirty();
    };
    advanced.metroButton.onClick = [this]
    {
        const bool on = advanced.metroButton.getToggleState();
        processor.setMetroOn(on);
        advanced.setMetroArmed(on);
        markDirty();
    };
    advanced.setTape(&processor.getTape());
    advanced.onChanged = [this] { markDirty(); };
    advanced.onQuantizeChange = [this](bool)
    {
        markDirty();
    };
    addAndMakeVisible(advanced);

    looperDrawer.setEngine(&processor.getLooper());
    looperDrawer.onModeChange = [this]
    {
        applyWindowSize();
        markDirty();
    };
    looperDrawer.onChanged = [this] { markDirty(); };
    looperDrawer.onQuantizeChange = [this](bool)
    {
        markDirty();
    };
    addAndMakeVisible(looperDrawer);

    field.onChange = [this]
    {
        syncFieldToProcessor();
        showCurrentSlug();
        markDirty();
    };
    addAndMakeVisible(field);

    plasmaTune.onChange = [this] (const PlasmaLook& look)
    {
        field.setPlasmaLook(look);
    };
    field.setPlasmaLook(plasmaTune.getLook());
    plasmaTune.setLookAndFeel(&lookAndFeel);
    addChildComponent(plasmaTune);

    copyButton.onClick = [this] { copySlug(); };
    copyButton.setWantsKeyboardFocus(false);
    addAndMakeVisible(copyButton);

    applyButton.onClick = [this] { tryImportSlug(); };
    applyButton.setWantsKeyboardFocus(false);
    addAndMakeVisible(applyButton);

    slugLabel.setJustificationType(juce::Justification::centredLeft);
    slugLabel.setFont(lookAndFeel.font(15.0f, true));
    addAndMakeVisible(slugLabel);

    slugField.setJustification(juce::Justification::centredLeft);
    slugField.setFont(lookAndFeel.font(16.0f));
    slugField.setTextToShowWhenEmpty("paste slug", CuteLookAndFeel::dim());
    slugField.setInputRestrictions(24, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789*-_");
    slugField.onReturnKey = [this] { tryImportSlug(); };
    slugField.onFocusLost = [this]
    {
        if (slugField.getText().trim().isEmpty())
            showCurrentSlug();
        else
            tryImportSlug();
    };
    addAndMakeVisible(slugField);

    loadSettings();
    AppLog::note("settings loaded");
    setSize(520, 920);
    applyWindowSize();
    chrome.toFront(false);
    startTimerHz(30);
    AppLog::note("MainComponent ready");
}

void MainComponent::startAudio()
{
    AppLog::note("audio begin");

    std::unique_ptr<juce::XmlElement> audioXml;
    const bool useSaved = AppLog::previousRunFinished();
    if (useSaved)
        audioXml = appSettings().getXmlValue("audio");
    else
        AppLog::note("previous run did not finish, skipping saved audio device");

    struct Ctx
    {
        juce::AudioDeviceManager* manager = nullptr;
        juce::XmlElement* xml = nullptr;
        juce::String error;
    };

    auto initFn = [] (void* p)
    {
        auto* ctx = static_cast<Ctx*>(p);
        ctx->error = ctx->manager->initialise(2, 2, ctx->xml, true);
    };

    Ctx ctx { &deviceManager, audioXml.get(), {} };
    unsigned int code = 0;
    AppLog::note(ctx.xml != nullptr ? "audio init saved device" : "audio init default device");
    bool ok = AppLog::runSeh(initFn, &ctx, &code) && ctx.error.isEmpty();
    if (! ok)
    {
        AppLog::note("audio init failed code=" + juce::String::toHexString((int) code)
                     + " err=" + ctx.error);
        if (useSaved)
            appSettings().removeValue("audio");
        ctx.xml = nullptr;
        ctx.error.clear();
        code = 0;
        AppLog::note("audio init fallback");
        ok = AppLog::runSeh(initFn, &ctx, &code) && ctx.error.isEmpty();
        if (! ok)
            AppLog::note("audio fallback failed code=" + juce::String::toHexString((int) code)
                         + " err=" + ctx.error);
    }

    if (! ok && ! selfTesting)
    {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                               "Audio",
                                               "Could not start audio: " + ctx.error);
    }

    player.setProcessor(&processor);
    deviceManager.addAudioCallback(&player);
    deviceManager.addChangeListener(this);
    refreshInputChannels();
    updateLatencyLabel();
    audioStartedOk = ok && deviceManager.getCurrentAudioDevice() != nullptr;
    AppLog::note(audioStartedOk ? "audio ready" : "audio ready without device");

    if (auto* device = deviceManager.getCurrentAudioDevice())
    {
        const int nIn = juce::jmax(1, device->getActiveInputChannels().countNumberOfSetBits());
        const auto names = device->getInputChannelNames();
        juce::String listed;
        for (int i = 0; i < nIn; ++i)
        {
            if (i > 0)
                listed += ", ";
            listed += i < names.size() ? names[i] : ("ch" + juce::String(i + 1));
        }

        AppLog::note("audio device=" + device->getName()
                     + " sr=" + juce::String(device->getCurrentSampleRate(), 1)
                     + " buffer=" + juce::String(device->getCurrentBufferSizeSamples())
                     + " inputs=" + juce::String(nIn)
                     + " selected=" + juce::String(inputChannelBox.getSelectedItemIndex() + 1)
                     + (listed.isNotEmpty() ? " [" + listed + "]" : juce::String()));
    }
}

MainComponent::~MainComponent()
{
    stopTimer();
    stopDebugLog();
    plasmaTune.setLookAndFeel(nullptr);
    processor.stopRecording();
    if (! selfTesting)
        saveSettings();
    deviceManager.removeChangeListener(this);
    deviceManager.removeAudioCallback(&player);
    player.setProcessor(nullptr);
    setLookAndFeel(nullptr);
}

void MainComponent::loadSettings()
{
    restoring = true;
    auto& settings = appSettings();

    std::array<float, 6> axes {};
    bool hasStar = false;
    for (int i = 0; i < 6; ++i)
    {
        const auto key = "axis" + juce::String(i);
        if (settings.containsKey(key))
        {
            axes[(size_t) i] = (float) settings.getDoubleValue(key);
            hasStar = true;
        }
    }

    if (hasStar)
    {
        field.setStarValues(axes, false);
        processor.setAxes(axes);
    }
    else
    {
        processor.setAxes(field.getStarValues());
    }

    std::array<float, 8> fx {};
    bool hasFx = false;
    for (int i = 0; i < 8; ++i)
    {
        const auto key = "fx" + juce::String(i);
        if (settings.containsKey(key))
        {
            fx[(size_t) i] = (float) settings.getDoubleValue(key);
            hasFx = true;
        }
    }

    if (hasFx)
        field.setFxValues(fx, false);
    processor.setFxAmounts(field.getFxValues());

    const bool shimmer = settings.getBoolValue("bloomShimmer", false);
    field.setBloomShimmer(shimmer, false);
    processor.setBloomShimmer(shimmer);

    cab.setSizeAmount((float) settings.getDoubleValue("cabSize", (double) Acoustics::defaultSize), false);
    cab.setBackAmount((float) settings.getDoubleValue("cabBack", (double) Acoustics::defaultBack), false);
    processor.setCabSize(cab.getSizeAmount());
    processor.setCabBack(cab.getBackAmount());

    const bool binaural = settings.getBoolValue("binaural", false);
    binauralButton.setToggleState(binaural, juce::dontSendNotification);
    processor.setBinaural(binaural);

    inputGain.setValue(settings.getDoubleValue("inputGain", 0.0), juce::dontSendNotification);
    outputGain.setValue(settings.getDoubleValue("outputGain", 0.0), juce::dontSendNotification);
    processor.setInputGainDb((float) inputGain.getValue());
    processor.setOutputGainDb((float) outputGain.getValue());

    const bool muted = settings.getBoolValue("mute", false);
    muteButton.setToggleState(muted, juce::dontSendNotification);
    processor.setMuted(muted);

    preferredInputChannel = settings.getIntValue("inputChannel", 0);
    processor.setInputChannel(preferredInputChannel);

    presetStore.load();
    drawer.rebuild();
    chrome.presetsButton.setToggleState(settings.getBoolValue("presetsOpen", false),
                                        juce::dontSendNotification);
    chrome.advancedButton.setToggleState(settings.getBoolValue("advancedOpen", false),
                                         juce::dontSendNotification);
    chrome.looperButton.setToggleState(settings.getBoolValue("looperOpen", false),
                                       juce::dontSendNotification);
    advancedWasOpen = chrome.advancedButton.getToggleState();
    looperDrawer.setAdvanced(settings.getBoolValue("looperAdvanced", false), false);
    looperDrawer.setQuantize(settings.getBoolValue("looperQuantize", false), false);
    processor.getLooper().setLevel(0, (float) settings.getDoubleValue("looperLevel0", 1.0));
    processor.getLooper().setLevel(1, (float) settings.getDoubleValue("looperLevel1", 1.0));
    processor.getTape().loadSession();
    advanced.setQuantize(settings.getBoolValue("tapeQuantize", processor.getTape().isQuantize()), false);

    const float bpm = (float) settings.getDoubleValue("metroBpm", 120.0);
    advanced.setBpm(bpm, false);
    processor.setMetroBpm(bpm);
    const bool metro = settings.getBoolValue("metroOn", false);
    advanced.metroButton.setToggleState(metro, juce::dontSendNotification);
    processor.setMetroOn(metro);
    advanced.setMetroArmed(metro);

    showCurrentSlug();
    restoring = false;
}

void MainComponent::saveSettings()
{
    auto& settings = appSettings();
    const auto axes = field.getStarValues();
    for (int i = 0; i < 6; ++i)
        settings.setValue("axis" + juce::String(i), (double) axes[(size_t) i]);

    const auto fx = field.getFxValues();
    for (int i = 0; i < 8; ++i)
        settings.setValue("fx" + juce::String(i), (double) fx[(size_t) i]);

    settings.setValue("bloomShimmer", field.getBloomShimmer());
    settings.setValue("cabSize", (double) cab.getSizeAmount());
    settings.setValue("cabBack", (double) cab.getBackAmount());
    settings.setValue("binaural", binauralButton.getToggleState());

    settings.setValue("inputGain", inputGain.getValue());
    settings.setValue("outputGain", outputGain.getValue());
    settings.setValue("mute", muteButton.getToggleState());
    settings.setValue("inputChannel", preferredInputChannel);
    settings.setValue("presetsOpen", chrome.presetsButton.getToggleState());
    settings.setValue("advancedOpen", chrome.advancedButton.getToggleState());
    settings.setValue("looperOpen", chrome.looperButton.getToggleState());
    settings.setValue("looperAdvanced", looperDrawer.isAdvanced());
    settings.setValue("looperQuantize", looperDrawer.isQuantize());
    settings.setValue("looperLevel0", (double) processor.getLooper().getLevel(0));
    settings.setValue("looperLevel1", (double) processor.getLooper().getLevel(1));
    settings.setValue("metroOn", advanced.metroButton.getToggleState());
    settings.setValue("metroBpm", (double) advanced.getBpm());
    settings.setValue("tapeQuantize", advanced.isQuantize());

    if (auto xml = deviceManager.createStateXml())
        settings.setValue("audio", xml.get());

    if (auto* top = getTopLevelComponent())
    {
        settings.setValue("winX", top->getX());
        settings.setValue("winY", top->getY());
    }

    settings.saveIfNeeded();
    settingsDirty = false;
}

void MainComponent::markDirty()
{
    if (restoring || selfTesting)
        return;

    settingsDirty = true;
    dirtyAt = juce::Time::currentTimeMillis();
}

void MainComponent::syncFieldToProcessor()
{
    processor.setAxes(field.getStarValues());
    processor.setFxAmounts(field.getFxValues());
    processor.setBloomShimmer(field.getBloomShimmer());
}

void MainComponent::updateLatencyLabel()
{
    if (auto* device = deviceManager.getCurrentAudioDevice())
    {
        const double sr = device->getCurrentSampleRate();
        if (sr > 0.0)
        {
            const double ms = 1000.0 * (double) (device->getInputLatencyInSamples()
                                                 + device->getOutputLatencyInSamples()) / sr;
            latencyLabel.setText(juce::String(juce::roundToInt(ms)) + " ms",
                                 juce::dontSendNotification);
            latencyLabel.setVisible(true);
            return;
        }
    }

    latencyLabel.setVisible(false);
}

juce::String MainComponent::currentSlug() const
{
    return ToneSlug::encode({ field.getStarValues(), field.getFxValues(), field.getBloomShimmer(),
                              binauralButton.getToggleState(),
                              cab.getSizeAmount(),
                              cab.getBackAmount() });
}

void MainComponent::copySlug()
{
    const auto slug = currentSlug();
    slugField.setText(slug, juce::dontSendNotification);
    slugField.selectAll();
    juce::SystemClipboard::copyTextToClipboard(slug);
}

void MainComponent::showCurrentSlug()
{
    if (slugField.hasKeyboardFocus(true))
        return;

    slugField.setText(currentSlug(), juce::dontSendNotification);
}

void MainComponent::togglePlasmaTune()
{
    const bool show = ! plasmaTune.isVisible();
    AppLog::note(show ? "show plasma tune" : "hide plasma tune");
    plasmaTune.setVisible(show);
    applyWindowSize();
}

void MainComponent::enableSelfTest()
{
    selfTesting = true;
    AppLog::note("self-test enabled");
}

void MainComponent::scheduleSelfTest()
{
    juce::Component::SafePointer<MainComponent> safe(this);
    juce::Timer::callAfterDelay(500, [safe]
    {
        if (safe != nullptr)
            safe->runSelfTest();
    });
}

int MainComponent::windowWidth() const
{
    return getWidth();
}

int MainComponent::windowHeight() const
{
    return getHeight();
}

void MainComponent::runSelfTest()
{
    AppLog::note("self-test begin");
    juce::StringArray lines;
    bool failed = false;

    auto check = [&] (const juce::String& name, bool ok, const juce::String& detail)
    {
        const auto line = (ok ? "PASS " : "FAIL ") + name + " " + detail;
        lines.add(line);
        AppLog::note("self-test " + line);
        if (! ok)
            failed = true;
    };

    chrome.advancedButton.setToggleState(false, juce::sendNotification);
    chrome.presetsButton.setToggleState(false, juce::sendNotification);
    chrome.looperButton.setToggleState(false, juce::sendNotification);
    if (plasmaTune.isVisible())
        togglePlasmaTune();

    const int baseW = windowWidth();
    const int baseH = windowHeight();
    check("baseline", baseW == 520 && baseH == 920,
          juce::String(baseW) + "x" + juce::String(baseH));

    chrome.advancedButton.setToggleState(true, juce::sendNotification);
    check("advanced-open", advanced.isVisible() && windowWidth() == 520 + AdvancedDrawer::width,
          juce::String(windowWidth()) + "x" + juce::String(windowHeight()));
    chrome.advancedButton.setToggleState(false, juce::sendNotification);
    check("advanced-close", ! advanced.isVisible() && windowWidth() == 520,
          juce::String(windowWidth()));

    chrome.presetsButton.setToggleState(true, juce::sendNotification);
    check("presets-open", drawer.isVisible() && windowWidth() == 520 + PresetDrawer::width,
          juce::String(windowWidth()));
    chrome.presetsButton.setToggleState(false, juce::sendNotification);
    check("presets-close", ! drawer.isVisible() && windowWidth() == 520,
          juce::String(windowWidth()));

    chrome.looperButton.setToggleState(true, juce::sendNotification);
    check("looper-open", looperDrawer.isVisible() && windowHeight() == 920 + looperDrawer.height(),
          juce::String(windowWidth()) + "x" + juce::String(windowHeight()));
    chrome.looperButton.setToggleState(false, juce::sendNotification);
    check("looper-close", ! looperDrawer.isVisible() && windowHeight() == 920,
          juce::String(windowHeight()));

    const int beforePlasma = windowWidth();
    togglePlasmaTune();
    check("plasma-open", plasmaTune.isVisible() && plasmaTune.getWidth() >= 40
          && windowWidth() == beforePlasma + PlasmaTune::width,
          "vis=" + juce::String((int) plasmaTune.isVisible())
          + " panelW=" + juce::String(plasmaTune.getWidth())
          + " winW=" + juce::String(windowWidth()));

    const auto shortcut = juce::KeyPress('p',
        juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::shiftModifier, 0);
    keyPressed(shortcut);
    check("shortcut-close", ! plasmaTune.isVisible() && windowWidth() == beforePlasma,
          juce::String(windowWidth()));
    keyPressed(shortcut);
    check("shortcut-open", plasmaTune.isVisible() && plasmaTune.getWidth() >= 40
          && windowWidth() == beforePlasma + PlasmaTune::width,
          juce::String(windowWidth()));

    if (! audioStartedOk)
        lines.add("WARN audio not started");

    AppLog::note("self-test wait for fft");
    juce::Component::SafePointer<MainComponent> safe(this);
    juce::Timer::callAfterDelay(1500, [safe, lines, failed]
    {
        if (safe == nullptr)
            return;

        auto report = lines;
        report.add(failed ? "FAIL" : "PASS");
        AppLog::directory().createDirectory();
        AppLog::selfTestFile().replaceWithText(report.joinIntoString("\n") + "\n");
        AppLog::note(failed ? "self-test failed" : "self-test passed");

        if (auto* app = juce::JUCEApplication::getInstance())
        {
            app->setApplicationReturnValue(failed ? 1 : 0);
            app->systemRequestedQuit();
        }
    });
}

void MainComponent::applyWindowSize()
{
    if (applyingWindowSize)
        return;
    applyingWindowSize = true;
    const struct Reset
    {
        bool& flag;
        ~Reset() { flag = false; }
    } reset { applyingWindowSize };

    const bool adv = chrome.advancedButton.getToggleState();
    const bool pre = chrome.presetsButton.getToggleState();
    const bool loop = chrome.looperButton.getToggleState();
    const bool tune = plasmaTune.isVisible();
    advanced.setVisible(adv);
    drawer.setVisible(pre);
    looperDrawer.setVisible(loop);

    const int leftW = adv ? AdvancedDrawer::width : 0;
    const int rightW = pre ? PresetDrawer::width : 0;
    const int tuneW = tune ? PlasmaTune::width : 0;
    const int loopH = loop ? looperDrawer.height() : 0;
    const int w = 520 + leftW + rightW + tuneW;
    const int h = 920 + loopH;

    if (auto* top = getTopLevelComponent())
    {
        int x = top->getX();
        if (adv && ! advancedWasOpen)
            x -= AdvancedDrawer::width;
        else if (! adv && advancedWasOpen)
            x += AdvancedDrawer::width;

        if (auto* display = juce::Desktop::getInstance().getDisplays().getDisplayForRect(top->getBounds()))
        {
            const int minX = display->userBounds.getX() - juce::jmax(0, w - 160);
            const int maxX = display->userBounds.getRight() - 160;
            x = juce::jlimit(minX, maxX, x);
        }

        top->setTopLeftPosition(x, top->getY());
        top->setSize(w, h);
    }

    setSize(w, h);
    advancedWasOpen = adv;
    if (loop)
        grabKeyboardFocus();
    else if (spacePedalDown)
    {
        spacePedalDown = false;
        looperDrawer.pedalUp();
    }
}

void MainComponent::startDebugLog()
{
    DebugLog::Header header;
    header.sampleRate = processor.getSampleRate();
    header.blockSize = processor.getBlockSize();
    if (auto* device = deviceManager.getCurrentAudioDevice())
    {
        header.device = device->getName();
        header.sampleRate = device->getCurrentSampleRate();
        header.blockSize = device->getCurrentBufferSizeSamples();
    }
    header.slug = currentSlug();
    header.patch = processor.captureDebugSnapshot();
    debugLog.start(header);
    processor.setDebugLog(&debugLog);
}

void MainComponent::stopDebugLog()
{
    processor.setDebugLog(nullptr);
    debugLog.finish();
}

void MainComponent::tryImportSlug()
{
    ToneSlug::Patch patch;
    if (! ToneSlug::decode(slugField.getText(), patch))
    {
        if (slugField.getText().trim().isNotEmpty())
            flashSlugError();
        return;
    }

    applyPatch(patch);
}

void MainComponent::applyPatch(const ToneSlug::Patch& patch)
{
    field.setStarValues(patch.axes, false);
    field.setFxValues(patch.fx, false);
    field.setBloomShimmer(patch.bloomShimmer, false);
    cab.setSizeAmount(patch.cabSize, false);
    cab.setBackAmount(patch.cabBack, false);
    binauralButton.setToggleState(patch.binaural, juce::dontSendNotification);
    processor.setCabSize(patch.cabSize);
    processor.setCabBack(patch.cabBack);
    processor.setBinaural(patch.binaural);
    syncFieldToProcessor();
    markDirty();
    slugField.setColour(juce::TextEditor::backgroundColourId, CuteLookAndFeel::panel());
    slugField.setColour(juce::TextEditor::textColourId, CuteLookAndFeel::mist());
    showCurrentSlug();
}

void MainComponent::flashSlugError()
{
    slugField.setColour(juce::TextEditor::backgroundColourId, CuteLookAndFeel::nova());
    slugField.setColour(juce::TextEditor::textColourId, CuteLookAndFeel::onAccent());
    slugFlashTicks = 18;
    slugField.repaint();
}

bool MainComponent::isEditingText() const
{
    if (auto* c = juce::Component::getCurrentlyFocusedComponent())
        return dynamic_cast<juce::TextEditor*>(c) != nullptr
            || dynamic_cast<juce::ComboBox*>(c) != nullptr;
    return false;
}

bool MainComponent::windowIsFocused() const
{
    if (! juce::Process::isForegroundProcess())
        return false;

    if (auto* top = getTopLevelComponent())
    {
        if (auto* peer = top->getPeer())
            if (peer->isMinimised())
                return false;
        return top->isShowing() && top->hasKeyboardFocus(true);
    }

    return hasKeyboardFocus(true);
}

void MainComponent::releaseSpacePedal()
{
    if (! spacePedalDown)
        return;
    spacePedalDown = false;
    looperDrawer.pedalUp();
}

bool MainComponent::looperPedalArmed() const
{
    return looperDrawer.isVisible() && ! isEditingText() && windowIsFocused();
}

bool MainComponent::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress('p', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::shiftModifier, 0))
    {
        togglePlasmaTune();
        return true;
    }

    if (looperPedalArmed() && key == juce::KeyPress::spaceKey)
        return true;

    if (advanced.isVisible() && windowIsFocused() && ! isEditingText() && ! advanced.isEditingName()
        && (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey))
        return advanced.deleteSelectedClip();

    return false;
}

bool MainComponent::keyStateChanged(bool)
{
    if (! looperPedalArmed())
    {
        releaseSpacePedal();
        return false;
    }

    const bool down = juce::KeyPress::isKeyCurrentlyDown(juce::KeyPress::spaceKey);
    if (down == spacePedalDown)
        return down;

    spacePedalDown = down;
    if (down)
        looperDrawer.pedalDown();
    else
        looperDrawer.pedalUp();
    return true;
}

bool MainComponent::hitTest(int x, int y)
{
    juce::Path outline;
    outline.addRoundedRectangle(getLocalBounds().toFloat(), 24.0f);
    return outline.contains((float) x, (float) y);
}

void MainComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    juce::Path outline;
    outline.addRoundedRectangle(bounds, 24.0f);
    g.reduceClipRegion(outline);

    g.setColour(CuteLookAndFeel::voidFill());
    g.fillRect(bounds);

    juce::Random rng { 0xC057E11A };
    for (int i = 0; i < 72; ++i)
    {
        const float x = rng.nextFloat() * bounds.getWidth();
        const float y = rng.nextFloat() * bounds.getHeight();
        const float a = 0.12f + rng.nextFloat() * 0.33f;
        g.setColour(CuteLookAndFeel::starlight().withAlpha(a));
        g.fillRect(x, y, 1.2f, 1.2f);
    }

    if (! meterBounds.isEmpty())
    {
        g.setColour(CuteLookAndFeel::panel());
        g.fillRoundedRectangle(meterBounds, 4.0f);

        const float filled = juce::jlimit(0.0f, 1.0f, meterLevel);
        auto level = meterBounds.withWidth(meterBounds.getWidth() * filled);
        g.setColour(meterClipped || filled > 0.9f ? CuteLookAndFeel::flare() : CuteLookAndFeel::starlight());
        g.fillRoundedRectangle(level, 4.0f);

        auto pip = juce::Rectangle<float>(meterBounds.getRight() - meterBounds.getHeight(),
                                          meterBounds.getY(),
                                          meterBounds.getHeight(),
                                          meterBounds.getHeight());
        g.setColour(meterClipped ? CuteLookAndFeel::flare() : CuteLookAndFeel::panel());
        g.fillRoundedRectangle(pip, 4.0f);
    }
}

void MainComponent::resized()
{
    constexpr int mainW = 520;
    constexpr int mainH = 920;
    const int leftW = advanced.isVisible() ? AdvancedDrawer::width : 0;
    const int tuneW = plasmaTune.isVisible() ? PlasmaTune::width : 0;
    const int loopH = looperDrawer.isVisible() ? looperDrawer.height() : 0;
    chrome.setBounds(0, 0, getWidth(), WindowChrome::barHeight);
    advanced.setBounds(0, WindowChrome::barHeight, leftW,
                       mainH - WindowChrome::barHeight);
    plasmaTune.setBounds(leftW + mainW, WindowChrome::barHeight, PlasmaTune::width,
                         mainH - WindowChrome::barHeight);
    drawer.setBounds(leftW + mainW + tuneW, WindowChrome::barHeight, PresetDrawer::width,
                     mainH - WindowChrome::barHeight);
    looperDrawer.setBounds(0, mainH, getWidth(), loopH);

    auto bounds = juce::Rectangle<int>(leftW, 0, mainW, mainH).reduced(28, 16);
    bounds.removeFromTop(WindowChrome::barHeight);

    auto meterRow = bounds.removeFromTop(16);
    latencyLabel.setBounds(meterRow.removeFromRight(56));
    meterBounds = meterRow.toFloat().withSizeKeepingCentre((float) meterRow.getWidth(), 10.0f);
    bounds.removeFromTop(8);

    field.setBounds(bounds.removeFromTop(430));
    bounds.removeFromTop(8);

    auto share = bounds.removeFromTop(32);
    slugLabel.setBounds(share.removeFromLeft(52));
    copyButton.setBounds(share.removeFromLeft(64));
    share.removeFromLeft(6);
    applyButton.setBounds(share.removeFromRight(64));
    share.removeFromRight(6);
    slugField.setBounds(share);

    bounds.removeFromTop(8);
    auto channelRow = bounds.removeFromTop(28);
    inputChannelLabel.setBounds(channelRow.removeFromLeft(52));
    inputChannelBox.setBounds(channelRow);

    bounds.removeFromTop(6);
    cab.setBounds(bounds.removeFromTop(160));

    bounds.removeFromTop(6);
    auto knobs = bounds.removeFromTop(100);
    auto inArea = knobs.removeFromLeft(knobs.getWidth() / 2);
    inputGain.setBounds(inArea.withTrimmedBottom(18));
    inputGainLabel.setBounds(inArea.removeFromBottom(18));
    outputGain.setBounds(knobs.withTrimmedBottom(18));
    outputGainLabel.setBounds(knobs.removeFromBottom(18));

    bounds.removeFromTop(6);
    auto buttons = bounds.removeFromTop(28);
    muteButton.setBounds(buttons.removeFromLeft(28));
    buttons.removeFromLeft(8);
    binauralButton.setBounds(buttons.removeFromLeft(28));
    buttons.removeFromLeft(8);
    debugButton.setBounds(buttons.removeFromLeft(28));
    if (logsButton.isVisible())
    {
        buttons.removeFromLeft(8);
        logsButton.setBounds(buttons.removeFromLeft(28));
    }
    devicesButton.setBounds(buttons.removeFromRight(88));
}

void MainComponent::timerCallback()
{
    const float peak = processor.getPeak();
    meterLevel = juce::jmax(peak, meterLevel * 0.82f);
    field.setFieldEnergy(processor.getFieldEnergy());
    field.setFieldSpectrum(processor.getFieldSpectrum());
    if (processor.takeClip())
        clipHoldTicks = 30;
    else if (clipHoldTicks > 0)
        --clipHoldTicks;
    meterClipped = clipHoldTicks > 0;
    if (debugLog.isActive())
        debugLog.flushWindow();
    if (! meterBounds.isEmpty())
        repaint(meterBounds.toNearestInt().expanded(2, 2));
    updateLatencyLabel();

    if (slugFlashTicks > 0 && --slugFlashTicks == 0)
    {
        slugField.setColour(juce::TextEditor::backgroundColourId, CuteLookAndFeel::panel());
        slugField.setColour(juce::TextEditor::textColourId, CuteLookAndFeel::mist());
        slugField.repaint();
    }

    if (! selfTesting && settingsDirty && juce::Time::currentTimeMillis() - dirtyAt > 400)
        saveSettings();

    advanced.consumePulse(processor.takeMetroPulse());
    if (advanced.isVisible())
        advanced.refresh();
    if (spacePedalDown && ! looperPedalArmed())
        releaseSpacePedal();
    if (looperDrawer.isVisible())
        looperDrawer.refresh();
}

void MainComponent::changeListenerCallback(juce::ChangeBroadcaster*)
{
    refreshInputChannels();
    updateLatencyLabel();
    markDirty();
}

void MainComponent::refreshInputChannels()
{
    inputChannelBox.clear(juce::dontSendNotification);

    int numInputs = 2;
    if (auto* device = deviceManager.getCurrentAudioDevice())
        numInputs = juce::jmax(1, device->getActiveInputChannels().countNumberOfSetBits());

    for (int i = 0; i < numInputs; ++i)
        inputChannelBox.addItem("Channel " + juce::String(i + 1), i + 1);

    const int select = juce::jlimit(0, numInputs - 1, preferredInputChannel);
    inputChannelBox.setSelectedItemIndex(select, juce::dontSendNotification);
    processor.setInputChannel(select);
}

void MainComponent::showDeviceSettings()
{
    auto* selector = new juce::AudioDeviceSelectorComponent(deviceManager,
                                                            1, 16, 1, 2,
                                                            false, false, true, false);
    selector->setSize(500, 420);
    selector->setLookAndFeel(&lookAndFeel);

    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned(selector);
    options.dialogTitle = "Devices";
    options.dialogBackgroundColour = CuteLookAndFeel::voidFill();
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = true;
    options.launchAsync();
}
