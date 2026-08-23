#include "Looper/LooperDrawer.h"
#include "Appearance/DragTip.h"
#include "Appearance/Theme.h"

namespace
{
    constexpr juce::uint32 holdMs = 1500;
    constexpr juce::uint32 doubleMs = 320;
}

LooperDrawer::PhraseStrip::PhraseStrip(int indexToUse)
    : index(indexToUse)
{
    setPaintingIsUnclipped(true);
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
}

void LooperDrawer::PhraseStrip::setAdvancedLook(bool shouldBeAdvanced)
{
    advancedLook = shouldBeAdvanced;
    resized();
    repaint();
}

void LooperDrawer::PhraseStrip::setSnapshot(LooperEngine::State stateToUse, float playheadToUse,
                                            float levelToUse, bool armedToUse, bool hasContentToUse)
{
    state = stateToUse;
    playhead = playheadToUse;
    level = levelToUse;
    armed = armedToUse;
    hasContent = hasContentToUse;
    repaint();
}

juce::Colour LooperDrawer::PhraseStrip::statusColour() const
{
    switch (state)
    {
        case LooperEngine::State::Armed:
            return Theme::flare().interpolatedWith(Theme::panel(), 0.42f);
        case LooperEngine::State::Recording:   return Theme::flare();
        case LooperEngine::State::Playing:     return Theme::starlight();
        case LooperEngine::State::Overdubbing: return Theme::nova();
        case LooperEngine::State::Stopped:     return Theme::mist();
        case LooperEngine::State::Empty:       return Theme::dim();
    }
    return Theme::dim();
}

void LooperDrawer::PhraseStrip::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    if (advancedLook)
    {
        auto fill = armed ? Theme::panel().interpolatedWith(Theme::nova(), 0.12f)
                          : Theme::panel();
        g.setColour(fill);
        g.fillRoundedRectangle(bounds, Theme::corner());
    }

    if (advancedLook)
    {
        if (auto* laf = dynamic_cast<Theme*>(&getLookAndFeel()))
            g.setFont(laf->font(16.0f, true));
        g.setColour(armed ? Theme::starlight() : Theme::dim());
        g.drawText(juce::String(index + 1), bounds.removeFromLeft(28.0f),
                   juce::Justification::centred, false);
    }

    const auto colour = statusColour();
    const float ring = juce::jmin(recBounds.getWidth(), recBounds.getHeight());
    const auto ringArea = recBounds.withSizeKeepingCentre(ring, ring);
    const auto disc = ringArea.reduced(ring * 0.22f);

    g.setColour(Theme::panel());
    g.fillEllipse(ringArea);

    if (state != LooperEngine::State::Empty || playhead > 0.0f)
    {
        juce::Path arc;
        const float from = -juce::MathConstants<float>::halfPi;
        const float to = from + juce::MathConstants<float>::twoPi * juce::jlimit(0.0f, 1.0f, playhead);
        arc.addCentredArc(ringArea.getCentreX(), ringArea.getCentreY(),
                          ring * 0.42f, ring * 0.42f, 0.0f, from, to, true);
        g.setColour(colour);
        g.strokePath(arc, juce::PathStrokeType(3.2f, juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));
    }

    g.setColour(colour);
    g.fillEllipse(disc);

    if (advancedLook)
    {
        g.setColour(hasContent ? Theme::mist() : Theme::dim());
        g.fillRoundedRectangle(stopBounds.reduced(stopBounds.getWidth() * 0.28f), 2.0f);
    }

    g.setColour(Theme::panel());
    g.fillRoundedRectangle(levelBounds, 5.0f);
    const float amount = juce::jlimit(0.0f, 1.0f, level);
    auto filled = levelBounds.withTrimmedTop(levelBounds.getHeight() * (1.0f - amount));
    g.setColour(Theme::starlight());
    g.fillRoundedRectangle(filled, 5.0f);
    g.fillEllipse(thumbBounds());
}

void LooperDrawer::PhraseStrip::resized()
{
    auto bounds = getLocalBounds().toFloat().reduced(advancedLook ? 10.0f : 4.0f, 8.0f);
    if (advancedLook)
        bounds.removeFromLeft(28.0f);

    const float rec = advancedLook ? 56.0f : 72.0f;
    recBounds = bounds.removeFromLeft(rec).withSizeKeepingCentre(rec, rec);
    bounds.removeFromLeft(12.0f);

    if (advancedLook)
    {
        stopBounds = bounds.removeFromLeft(28.0f).withSizeKeepingCentre(28.0f, 28.0f);
        bounds.removeFromLeft(12.0f);
    }
    else
    {
        stopBounds = {};
    }

    auto levelArea = bounds.removeFromRight(22.0f);
    const float trackH = juce::jmin(levelArea.getHeight(), advancedLook ? 64.0f : 88.0f);
    levelBounds = levelArea.withSizeKeepingCentre(8.0f, trackH);
}

bool LooperDrawer::PhraseStrip::inRec(juce::Point<float> p) const { return recBounds.contains(p); }
bool LooperDrawer::PhraseStrip::inStop(juce::Point<float> p) const { return advancedLook && stopBounds.contains(p); }
bool LooperDrawer::PhraseStrip::inLevel(juce::Point<float> p) const
{
    return levelBounds.expanded(10.0f, 8.0f).contains(p) || thumbBounds().expanded(4.0f).contains(p);
}

juce::Rectangle<float> LooperDrawer::PhraseStrip::thumbBounds() const
{
    const float t = juce::jlimit(0.0f, 1.0f, level);
    const float y = levelBounds.getBottom() - t * levelBounds.getHeight();
    return juce::Rectangle<float>(14.0f, 14.0f).withCentre({ levelBounds.getCentreX(), y });
}

void LooperDrawer::PhraseStrip::setLevelFromY(float y)
{
    if (levelBounds.getHeight() <= 0.0f)
        return;
    level = juce::jlimit(0.0f, 1.0f, (levelBounds.getBottom() - y) / levelBounds.getHeight());
    if (onLevel != nullptr)
        onLevel(index, level);
    DragTip::show(*this, thumbBounds().getCentre(), DragTip::percent(level), Theme::starlight());
    repaint();
}

void LooperDrawer::PhraseStrip::mouseDown(const juce::MouseEvent& e)
{
    const auto p = e.position;
    if (inLevel(p))
    {
        draggingLevel = true;
        setLevelFromY(p.y);
        return;
    }

    if (inStop(p))
    {
        stopDown = true;
        return;
    }

    recDown = true;
    sentHold = false;
    downAt = juce::Time::getMillisecondCounter();
    if (onArm != nullptr)
        onArm(index);

    const auto now = downAt;
    if (pendingTap && now - tapUpAt < doubleMs)
    {
        pendingTap = false;
        if (onDoubleTap != nullptr)
            onDoubleTap(index);
        return;
    }

    pendingTap = true;
    tapUpAt = now;
    if (onTap != nullptr)
        onTap(index);
}

void LooperDrawer::PhraseStrip::mouseDrag(const juce::MouseEvent& e)
{
    if (draggingLevel)
        setLevelFromY(e.position.y);
}

void LooperDrawer::PhraseStrip::mouseUp(const juce::MouseEvent&)
{
    if (draggingLevel)
    {
        draggingLevel = false;
        DragTip::hide();
        repaint();
        return;
    }

    if (stopDown)
    {
        stopDown = false;
        if (onStop != nullptr)
            onStop(index);
        return;
    }

    recDown = false;
}

void LooperDrawer::PhraseStrip::tick()
{
    const auto now = juce::Time::getMillisecondCounter();
    if (recDown && ! sentHold && now - downAt >= holdMs)
    {
        sentHold = true;
        pendingTap = false;
        if (onHold != nullptr)
            onHold(index);
    }

    if (pendingTap && ! recDown && now - tapUpAt >= doubleMs)
        pendingTap = false;
}

LooperDrawer::LooperDrawer()
{
    setOpaque(false);
    bindStrip(stripA);
    bindStrip(stripB);
    quantizeButton.onClick = [this]
    {
        setQuantize(quantizeButton.getToggleState(), true);
    };
    addAndMakeVisible(stripA);
    addChildComponent(stripB);
    addAndMakeVisible(quantizeButton);
}

void LooperDrawer::bindStrip(PhraseStrip& strip)
{
    strip.onTap = [this](int index)
    {
        if (engine != nullptr)
            engine->tap(index);
    };
    strip.onDoubleTap = [this](int index)
    {
        if (engine != nullptr)
            engine->doubleTap(index);
    };
    strip.onHold = [this](int index)
    {
        if (engine != nullptr)
            engine->hold(index);
    };
    strip.onStop = [this](int index)
    {
        if (engine != nullptr)
            engine->stop(index);
    };
    strip.onArm = [this](int index)
    {
        if (engine != nullptr)
            engine->setArmed(index);
        if (onChanged != nullptr)
            onChanged();
    };
    strip.onLevel = [this](int index, float value)
    {
        if (engine != nullptr)
            engine->setLevel(index, value);
        if (onChanged != nullptr)
            onChanged();
    };
}

void LooperDrawer::setEngine(LooperEngine* engineToUse)
{
    engine = engineToUse;
    if (engine != nullptr)
        engine->setQuantize(quantizeOn);
    refresh();
}

void LooperDrawer::setQuantize(bool shouldQuantize, bool notify)
{
    quantizeOn = shouldQuantize;
    quantizeButton.setToggleState(shouldQuantize, juce::dontSendNotification);
    if (engine != nullptr)
        engine->setQuantize(shouldQuantize);
    if (notify && onQuantizeChange != nullptr)
        onQuantizeChange(shouldQuantize);
    repaint();
}

void LooperDrawer::setAdvanced(bool shouldBeAdvanced, bool notify)
{
    if (advancedMode == shouldBeAdvanced)
        return;

    advancedMode = shouldBeAdvanced;
    stripA.setAdvancedLook(advancedMode);
    stripB.setAdvancedLook(advancedMode);
    stripB.setVisible(advancedMode);
    if (engine != nullptr)
        engine->setSimpleMode(! advancedMode);
    resized();
    if (notify && onModeChange != nullptr)
        onModeChange();
    repaint();
}

void LooperDrawer::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour(Theme::voidFill().interpolatedWith(Theme::panel(), 0.35f));
    g.fillRoundedRectangle(bounds.reduced(8.0f, 8.0f), 12.0f);

    if (auto* laf = dynamic_cast<Theme*>(&getLookAndFeel()))
        g.setFont(laf->font(15.0f, true));
    g.setColour(Theme::dim());
    g.drawText("Looper", titleBounds.toFloat(), juce::Justification::centredLeft, false);
}

void LooperDrawer::resized()
{
    auto bounds = getLocalBounds().reduced(16, 12);
    auto titleRow = bounds.removeFromTop(28);
    quantizeButton.setBounds(titleRow.removeFromRight(28));
    titleRow.removeFromRight(8);
    titleBounds = titleRow;
    bounds.removeFromTop(4);

    if (advancedMode)
    {
        auto top = bounds.removeFromTop((bounds.getHeight() - 8) / 2);
        bounds.removeFromTop(8);
        stripA.setBounds(top);
        stripB.setBounds(bounds);
    }
    else
    {
        stripA.setBounds(bounds);
    }
}

void LooperDrawer::mouseDown(const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu() && titleBounds.contains(e.getPosition()))
        setAdvanced(! advancedMode, true);
}

int LooperDrawer::pedalPhrase() const
{
    if (! advancedMode || engine == nullptr)
        return 0;
    return engine->getArmed();
}

void LooperDrawer::pedalDown()
{
    if (pedalHeld)
        return;

    pedalHeld = true;
    pedalSentHold = false;
    pedalDownAt = juce::Time::getMillisecondCounter();

    const int phrase = pedalPhrase();
    if (pedalPendingTap && pedalDownAt - pedalTapUpAt < doubleMs)
    {
        pedalPendingTap = false;
        if (engine != nullptr)
            engine->doubleTap(phrase);
        return;
    }

    pedalPendingTap = true;
    pedalTapUpAt = pedalDownAt;
    if (engine != nullptr)
        engine->tap(phrase);
}

void LooperDrawer::pedalUp()
{
    pedalHeld = false;
}

void LooperDrawer::tickPedal()
{
    const auto now = juce::Time::getMillisecondCounter();
    if (pedalHeld && ! pedalSentHold && now - pedalDownAt >= holdMs)
    {
        pedalSentHold = true;
        pedalPendingTap = false;
        if (engine != nullptr)
            engine->hold(pedalPhrase());
    }

    if (pedalPendingTap && ! pedalHeld && now - pedalTapUpAt >= doubleMs)
        pedalPendingTap = false;
}

void LooperDrawer::refresh()
{
    stripA.tick();
    if (advancedMode)
        stripB.tick();
    tickPedal();

    if (engine == nullptr)
        return;

    const int armed = engine->getArmed();
    stripA.setSnapshot(engine->getState(0), engine->getPlayhead01(0),
                       engine->getLevel(0), advancedMode && armed == 0,
                       engine->hasContent(0));
    if (advancedMode)
        stripB.setSnapshot(engine->getState(1), engine->getPlayhead01(1),
                           engine->getLevel(1), armed == 1,
                           engine->hasContent(1));
}
