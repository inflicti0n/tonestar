#include "AdvancedDrawer.h"
#include "CuteLookAndFeel.h"
#include <cmath>

void AdvancedDrawer::BpmField::paint(juce::Graphics& g)
{
    if (editing)
        return;

    if (auto* laf = dynamic_cast<CuteLookAndFeel*>(&getLookAndFeel()))
        g.setFont(laf->font(22.0f, true));
    else
        g.setFont(juce::FontOptions(22.0f, juce::Font::bold));

    g.setColour(CuteLookAndFeel::mist());
    g.drawText(juce::String(juce::roundToInt(bpm)), getLocalBounds(),
               juce::Justification::centredRight, false);
}

void AdvancedDrawer::BpmField::resized()
{
    editor.setBounds(getLocalBounds());
}

void AdvancedDrawer::BpmField::mouseDown(const juce::MouseEvent&)
{
    dragging = false;
    dragStart = bpm;
}

void AdvancedDrawer::BpmField::mouseDrag(const juce::MouseEvent& e)
{
    const auto d = e.getDistanceFromDragStart();
    if (! dragging && d < 4)
        return;

    dragging = true;
    const float delta = ((float) e.getDistanceFromDragStartX() - (float) e.getDistanceFromDragStartY()) / 4.0f;
    applyBpm(dragStart + delta, true);
}

void AdvancedDrawer::BpmField::mouseUp(const juce::MouseEvent&)
{
    if (! dragging)
        startEdit();
    dragging = false;
}

void AdvancedDrawer::BpmField::mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    float delta = wheel.deltaY != 0.0f ? wheel.deltaY : wheel.deltaX;
    if (wheel.isReversed)
        delta = -delta;

    const float step = (delta > 0.0f ? 1.0f : (delta < 0.0f ? -1.0f : 0.0f));
    if (step == 0.0f)
        return;

    applyBpm(bpm + step, true);
    if (editing)
        editor.setText(juce::String(juce::roundToInt(bpm)), juce::dontSendNotification);
}

void AdvancedDrawer::BpmField::setBpm(float value)
{
    bpm = juce::jlimit(40.0f, 240.0f, value);
    if (editing)
        editor.setText(juce::String(juce::roundToInt(bpm)), juce::dontSendNotification);
    repaint();
}

void AdvancedDrawer::BpmField::startEdit()
{
    if (editing)
        return;

    editing = true;
    if (editor.getParentComponent() == nullptr)
    {
        editor.setJustification(juce::Justification::centredRight);
        editor.setInputRestrictions(3, "0123456789");
        editor.setColour(juce::TextEditor::backgroundColourId, CuteLookAndFeel::panel());
        editor.setColour(juce::TextEditor::textColourId, CuteLookAndFeel::mist());
        editor.onReturnKey = [this] { finishEdit(); };
        editor.onFocusLost = [this] { finishEdit(); };
        addAndMakeVisible(editor);
        resized();
    }

    if (auto* laf = dynamic_cast<CuteLookAndFeel*>(&getLookAndFeel()))
        editor.setFont(laf->font(22.0f, true));
    editor.setText(juce::String(juce::roundToInt(bpm)), juce::dontSendNotification);
    editor.setVisible(true);
    editor.grabKeyboardFocus();
    editor.selectAll();
    repaint();
}

void AdvancedDrawer::BpmField::finishEdit()
{
    if (! editing)
        return;

    editing = false;
    editor.setVisible(false);
    applyBpm((float) editor.getText().getIntValue(), true);
}

void AdvancedDrawer::BpmField::applyBpm(float value, bool notify)
{
    bpm = juce::jlimit(40.0f, 240.0f, value);
    if (notify && onChange != nullptr)
        onChange(bpm);
    repaint();
}

AdvancedDrawer::Sparkle::Sparkle()
    : vblank(this, [this] { tick(); })
{
    setInterceptsMouseClicks(false, false);
}

void AdvancedDrawer::Sparkle::setArmed(bool shouldArm)
{
    armed = shouldArm;
    if (! armed)
        punch = 1.0f;
    repaint();
}

void AdvancedDrawer::Sparkle::pulse()
{
    if (armed)
        punch = 1.6f;
}

void AdvancedDrawer::Sparkle::tick()
{
    if (punch > 1.001f)
    {
        punch += (1.0f - punch) * 0.22f;
        repaint();
    }
    else if (punch != 1.0f)
    {
        punch = 1.0f;
        repaint();
    }
}

juce::Path AdvancedDrawer::Sparkle::starPath(float scale) const
{
    const auto c = getLocalBounds().toFloat().getCentre();
    const float reach = juce::jmin(getWidth(), getHeight()) * 0.28f * scale;
    juce::Path path;
    path.startNewSubPath(c.x, c.y - reach * 1.15f);
    path.cubicTo({ c.x + reach * 0.12f, c.y - reach * 0.12f },
                 { c.x + reach * 0.12f, c.y - reach * 0.12f },
                 { c.x + reach * 1.35f, c.y });
    path.cubicTo({ c.x + reach * 0.12f, c.y + reach * 0.12f },
                 { c.x + reach * 0.12f, c.y + reach * 0.12f },
                 { c.x, c.y + reach * 1.15f });
    path.cubicTo({ c.x - reach * 0.12f, c.y + reach * 0.12f },
                 { c.x - reach * 0.12f, c.y + reach * 0.12f },
                 { c.x - reach * 1.35f, c.y });
    path.cubicTo({ c.x - reach * 0.12f, c.y - reach * 0.12f },
                 { c.x - reach * 0.12f, c.y - reach * 0.12f },
                 { c.x, c.y - reach * 1.15f });
    path.closeSubPath();
    return path;
}

void AdvancedDrawer::Sparkle::paint(juce::Graphics& g)
{
    const float lit = juce::jlimit(0.0f, 1.0f, (punch - 1.0f) / 0.6f);
    const float scale = armed ? (0.72f + 0.28f * punch) : 0.62f;
    auto colour = armed
                      ? CuteLookAndFeel::dim().interpolatedWith(CuteLookAndFeel::starlight(), 0.35f + 0.65f * lit)
                      : CuteLookAndFeel::dim().withAlpha(0.45f);
    if (lit > 0.15f)
        colour = colour.interpolatedWith(CuteLookAndFeel::nova(), lit * 0.55f);

    g.setColour(colour);
    g.fillPath(starPath(scale));
    g.setColour(armed ? juce::Colours::white.withAlpha(0.55f + 0.45f * lit)
                      : CuteLookAndFeel::dim().withAlpha(0.35f));
    g.fillPath(starPath(scale * 0.38f));
}

namespace
{
    constexpr int laneH = 72;
    constexpr int laneGap = 6;
    constexpr int edgeHit = 10;
    constexpr float loopY = 3.0f;
    constexpr float loopH = 12.0f;
    constexpr float loopEdgeW = 5.0f;
}

AdvancedDrawer::LaneRow::LaneRow(int indexToUse)
    : index(indexToUse)
{
    muteButton.setWantsKeyboardFocus(false);
    muteButton.onClick = [this]
    {
        if (tape != nullptr)
        {
            tape->setMute(index, muteButton.getToggleState());
            if (onChanged != nullptr)
                onChanged();
        }
    };
    addAndMakeVisible(muteButton);

    editor.setVisible(false);
    editor.setJustification(juce::Justification::centredLeft);
    editor.onReturnKey = [this] { finishNameEdit(); };
    editor.onFocusLost = [this] { finishNameEdit(); };
    editor.setColour(juce::TextEditor::backgroundColourId, CuteLookAndFeel::voidFill());
    editor.setColour(juce::TextEditor::textColourId, CuteLookAndFeel::mist());
    addChildComponent(editor);
}

void AdvancedDrawer::LaneRow::setTape(TapeEngine* engineToUse)
{
    tape = engineToUse;
}

void AdvancedDrawer::LaneRow::setTimeline(TapeTimeline* timelineToUse)
{
    timeline = timelineToUse;
}

juce::Rectangle<float> AdvancedDrawer::LaneRow::waveBounds() const
{
    return getLocalBounds().toFloat().withTrimmedLeft((float) TapeTimeline::waveLeft - 4.0f).reduced(4.0f, 10.0f);
}

juce::Rectangle<float> AdvancedDrawer::LaneRow::nameBounds() const
{
    return juce::Rectangle<float>(36.0f, 22.0f, 50.0f, 28.0f);
}

juce::Rectangle<float> AdvancedDrawer::LaneRow::levelBounds() const
{
    return { 90.0f, 14.0f, 8.0f, 44.0f };
}

juce::Rectangle<float> AdvancedDrawer::LaneRow::levelThumb() const
{
    const float t = tape != nullptr ? juce::jlimit(0.0f, 1.0f, tape->getLevel(index)) : 1.0f;
    const auto bar = levelBounds();
    const float y = bar.getBottom() - t * bar.getHeight();
    return juce::Rectangle<float>(14.0f, 14.0f).withCentre({ bar.getCentreX(), y });
}

bool AdvancedDrawer::LaneRow::inLevel(juce::Point<float> p) const
{
    return levelBounds().expanded(10.0f, 8.0f).contains(p) || levelThumb().expanded(4.0f).contains(p);
}

void AdvancedDrawer::LaneRow::setLevelFromY(float y)
{
    if (tape == nullptr)
        return;
    const auto bar = levelBounds();
    if (bar.getHeight() <= 0.0f)
        return;
    tape->setLevel(index, (bar.getBottom() - y) / bar.getHeight());
    repaint();
}

juce::Rectangle<float> AdvancedDrawer::LaneRow::clipBounds() const
{
    if (tape == nullptr || timeline == nullptr)
        return {};

    const auto wave = waveBounds();
    int start = 0;
    int length = 0;
    const bool rec = tape->isRecording() && tape->getRecLane() == index && tape->getRecFrames() > 0;
    if (rec)
    {
        start = tape->getRecStart();
        length = tape->getRecFrames();
    }
    else
    {
        const auto view = tape->getLane(index);
        if (! view.hasClip)
            return {};
        start = view.start;
        length = juce::jmax(1, view.end - view.in);
    }

    const float x = timeline->sampleToX(start, wave.getX());
    const float r = timeline->sampleToX(start + length, wave.getX());
    return { x, wave.getY(), juce::jmax(3.0f, r - x), wave.getHeight() };
}

void AdvancedDrawer::LaneRow::drawWave(juce::Graphics& g, juce::Rectangle<float> clip,
                                       const TapeEngine::LaneView& view, int recFrames) const
{
    const int hop0 = recFrames > 0 ? 0 : juce::jlimit(0, TapeEngine::maxHops, view.in / TapeEngine::hop);
    const int hop1 = recFrames > 0
                         ? juce::jlimit(0, TapeEngine::maxHops, (recFrames + TapeEngine::hop - 1) / TapeEngine::hop)
                         : juce::jlimit(0, view.hopCount, (view.end + TapeEngine::hop - 1) / TapeEngine::hop);
    const int hopsN = juce::jmax(0, hop1 - hop0);
    if (view.hops == nullptr || hopsN <= 0 || clip.getWidth() < 2.0f)
        return;

    const int pixels = juce::jmax(1, (int) clip.getWidth());
    const float mid = clip.getCentreY();
    const float amp = clip.getHeight() * 0.46f;
    g.setColour(CuteLookAndFeel::starlight().interpolatedWith(CuteLookAndFeel::mist(), 0.25f));
    for (int x = 0; x < pixels; ++x)
    {
        const int h0 = hop0 + x * hopsN / pixels;
        const int h1 = hop0 + juce::jmax(x * hopsN / pixels + 1, (x + 1) * hopsN / pixels);
        float peak = 0.0f;
        for (int h = h0; h < h1 && h < hop1; ++h)
            peak = juce::jmax(peak, view.hops[h]);
        const float h = juce::jmax(1.0f, peak * amp);
        g.fillRect(clip.getX() + (float) x, mid - h, 1.0f, h * 2.0f);
    }
}

void AdvancedDrawer::LaneRow::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    const bool armed = tape != nullptr && tape->getArmedLane() == index;
    const bool recHere = tape != nullptr && tape->isRecording() && tape->getRecLane() == index;
    auto fill = CuteLookAndFeel::panel();
    if (armed)
        fill = fill.interpolatedWith(CuteLookAndFeel::nova(), 0.10f);
    if (recHere)
        fill = fill.interpolatedWith(CuteLookAndFeel::flare(), 0.12f);
    g.setColour(fill);
    g.fillRoundedRectangle(bounds, CuteLookAndFeel::corner());

    if (! editing && tape != nullptr)
    {
        if (auto* laf = dynamic_cast<CuteLookAndFeel*>(&getLookAndFeel()))
            g.setFont(laf->font(14.0f, true));
        g.setColour(CuteLookAndFeel::mist());
        g.drawText(tape->getName(index), nameBounds(), juce::Justification::centredLeft, true);
    }

    const auto bar = levelBounds();
    const float amount = tape != nullptr ? juce::jlimit(0.0f, 1.0f, tape->getLevel(index)) : 1.0f;
    g.setColour(CuteLookAndFeel::voidFill());
    g.fillRoundedRectangle(bar, 5.0f);
    g.setColour(CuteLookAndFeel::starlight());
    g.fillRoundedRectangle(bar.withTrimmedTop(bar.getHeight() * (1.0f - amount)), 5.0f);
    g.fillEllipse(levelThumb());

    const auto wave = waveBounds();
    g.setColour(CuteLookAndFeel::voidFill());
    g.fillRoundedRectangle(wave, 6.0f);

    if (timeline != nullptr)
        timeline->paintGrid(g, wave);

    if (tape != nullptr)
    {
        const auto clip = clipBounds();
        const auto vis = clip.getIntersection(wave);
        if (! vis.isEmpty())
        {
            juce::Graphics::ScopedSaveState save(g);
            g.reduceClipRegion(wave.getSmallestIntegerContainer());
            auto clipFill = recHere ? CuteLookAndFeel::flare().interpolatedWith(CuteLookAndFeel::panel(), 0.35f)
                                    : CuteLookAndFeel::panel().interpolatedWith(CuteLookAndFeel::starlight(), 0.18f);
            if (tape->isMuted(index))
                clipFill = clipFill.interpolatedWith(CuteLookAndFeel::voidFill(), 0.45f);
            g.setColour(clipFill);
            g.fillRoundedRectangle(clip, 5.0f);
            const auto view = tape->getLane(index);
            drawWave(g, clip.reduced(2.0f, 2.0f), view, recHere ? tape->getRecFrames() : 0);
            if (selected && ! recHere)
            {
                g.setColour(CuteLookAndFeel::starlight());
                g.drawRoundedRectangle(clip.reduced(0.8f), 5.0f, 1.6f);
                deleteBounds = deleteBoundsFor(vis);
                g.setColour(deleteHot ? CuteLookAndFeel::flare() : CuteLookAndFeel::dim());
                if (auto* laf = dynamic_cast<CuteLookAndFeel*>(&getLookAndFeel()))
                    g.setFont(laf->font(12.0f, true));
                g.drawText("X", deleteBounds, juce::Justification::centred, false);
            }
        }

        if (timeline != nullptr)
        {
            const float px = timeline->sampleToX(tape->getPlayhead(), wave.getX());
            if (px >= wave.getX() && px <= wave.getRight())
            {
                g.setColour(CuteLookAndFeel::nova().withAlpha(0.85f));
                g.fillRect(px, wave.getY(), 1.6f, wave.getHeight());
            }
        }
    }
}

void AdvancedDrawer::LaneRow::resized()
{
    auto bounds = getLocalBounds().toFloat();
    muteButton.setBounds(juce::Rectangle<int>(10, (int) bounds.getCentreY() - 11, 22, 22));
    editor.setBounds(nameBounds().toNearestInt());
}

juce::Rectangle<float> AdvancedDrawer::LaneRow::deleteBoundsFor(juce::Rectangle<float> clip) const
{
    return { clip.getRight() - 16.0f, clip.getY() + 2.0f, 14.0f, 14.0f };
}

void AdvancedDrawer::LaneRow::setSelected(bool shouldSelect)
{
    selected = shouldSelect;
    if (! selected)
        deleteHot = false;
    repaint();
}

void AdvancedDrawer::LaneRow::mouseDown(const juce::MouseEvent& e)
{
    if (tape == nullptr)
        return;

    if (editing)
    {
        if (! nameBounds().contains(e.position))
            finishNameEdit();
        else
            return;
    }

    if (onFinishOthers != nullptr)
        onFinishOthers();

    if (inLevel(e.position))
    {
        draggingLevel = true;
        setLevelFromY(e.position.y);
        return;
    }

    if (e.mods.isPopupMenu())
    {
        if (nameBounds().contains(e.position))
            startNameEdit();
        return;
    }

    tape->setArmedLane(index);
    if (onChanged != nullptr)
        onChanged();

    if (tape->isRecording() && tape->getRecLane() == index)
        return;

    const auto p = e.position;
    const auto wave = waveBounds();
    const auto clip = clipBounds();
    const auto vis = clip.getIntersection(wave);

    if (selected && ! vis.isEmpty() && deleteBoundsFor(vis).contains(p))
    {
        if (onDeleteClip != nullptr)
            onDeleteClip(index);
        return;
    }

    if (vis.isEmpty())
    {
        if (onSelectClip != nullptr)
            onSelectClip(-1);
        return;
    }

    dragStartX = e.x;
    const bool nearRight = wave.contains(juce::Point<float>(clip.getRight(), p.y))
                           && std::abs(p.x - clip.getRight()) <= (float) edgeHit;
    const bool nearLeft = wave.contains(juce::Point<float>(clip.getX(), p.y))
                          && std::abs(p.x - clip.getX()) <= (float) edgeHit;
    if (nearLeft && (! nearRight || p.x < clip.getCentreX()))
    {
        drag = Drag::TrimIn;
        dragStartValue = tape->getLane(index).start;
        setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
        return;
    }
    if (nearRight)
    {
        const auto view = tape->getLane(index);
        drag = Drag::Trim;
        dragStartValue = view.start + (view.end - view.in);
        setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
        return;
    }

    if (vis.contains(p))
    {
        if (onSelectClip != nullptr)
            onSelectClip(index);
        drag = Drag::Move;
        dragStartValue = tape->getLane(index).start;
        setMouseCursor(juce::MouseCursor::DraggingHandCursor);
    }
    else if (onSelectClip != nullptr)
    {
        onSelectClip(-1);
    }
}

void AdvancedDrawer::LaneRow::mouseDrag(const juce::MouseEvent& e)
{
    if (draggingLevel)
    {
        setLevelFromY(e.position.y);
        return;
    }

    if (tape == nullptr || timeline == nullptr || drag == Drag::None)
        return;

    const auto wave = waveBounds();
    const int delta = timeline->xToSample((float) e.x, wave.getX())
                      - timeline->xToSample((float) dragStartX, wave.getX());
    if (drag == Drag::Move)
    {
        tape->setStart(index, tape->snapSample(dragStartValue + delta));
    }
    else if (drag == Drag::TrimIn)
    {
        tape->trimLeft(index, tape->snapSample(dragStartValue + delta));
    }
    else
    {
        const auto view = tape->getLane(index);
        const int edge = tape->snapSample(dragStartValue + delta);
        tape->setEnd(index, edge - view.start + view.in);
    }
    repaint();
}

void AdvancedDrawer::LaneRow::mouseUp(const juce::MouseEvent&)
{
    if (draggingLevel || drag != Drag::None)
    {
        if (onChanged != nullptr)
            onChanged();
    }
    draggingLevel = false;
    drag = Drag::None;
    setMouseCursor(juce::MouseCursor::NormalCursor);
}

void AdvancedDrawer::LaneRow::mouseMove(const juce::MouseEvent& e)
{
    if (tape == nullptr)
        return;
    const auto wave = waveBounds();
    const auto clip = clipBounds();
    const auto vis = clip.getIntersection(wave);
    const bool hot = selected && ! vis.isEmpty() && deleteBoundsFor(vis).contains(e.position);
    if (hot != deleteHot)
    {
        deleteHot = hot;
        repaint();
    }
    const bool nearEdge = ! vis.isEmpty()
                          && ((wave.contains(juce::Point<float>(clip.getRight(), e.position.y))
                               && std::abs(e.position.x - clip.getRight()) <= (float) edgeHit)
                              || (wave.contains(juce::Point<float>(clip.getX(), e.position.y))
                                  && std::abs(e.position.x - clip.getX()) <= (float) edgeHit));
    if (hot || inLevel(e.position))
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
    else if (nearEdge)
        setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
    else if (vis.contains(e.position))
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
    else
        setMouseCursor(juce::MouseCursor::NormalCursor);
}

void AdvancedDrawer::LaneRow::mouseExit(const juce::MouseEvent&)
{
    if (deleteHot)
    {
        deleteHot = false;
        repaint();
    }
}

void AdvancedDrawer::LaneRow::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    if (inLevel(e.position) && tape != nullptr)
    {
        float delta = wheel.deltaY != 0.0f ? wheel.deltaY : wheel.deltaX;
        if (wheel.isReversed)
            delta = -delta;
        const float step = delta > 0.0f ? 0.05f : (delta < 0.0f ? -0.05f : 0.0f);
        if (step != 0.0f)
        {
            tape->setLevel(index, tape->getLevel(index) + step);
            if (onChanged != nullptr)
                onChanged();
            repaint();
        }
        return;
    }

    const auto wave = waveBounds();
    if (timeline == nullptr || ! wave.contains(e.position))
    {
        if (auto* parent = getParentComponent())
            parent->mouseWheelMove(e.getEventRelativeTo(parent), wheel);
        return;
    }

    timeline->wheel(e.position.x, wave.getX(), wheel.deltaY, wheel.deltaX,
                    wheel.isReversed, e.mods.isShiftDown());
    if (onViewChanged != nullptr)
        onViewChanged();
}

void AdvancedDrawer::LaneRow::startNameEdit()
{
    if (tape == nullptr || editing)
        return;

    editing = true;
    if (auto* laf = dynamic_cast<CuteLookAndFeel*>(&getLookAndFeel()))
        editor.setFont(laf->font(14.0f, true));
    editor.setText(tape->getName(index), juce::dontSendNotification);
    editor.setVisible(true);
    editor.grabKeyboardFocus();
    editor.selectAll();
    repaint();
}

void AdvancedDrawer::LaneRow::finishNameEdit()
{
    if (! editing)
        return;

    editing = false;
    editor.setVisible(false);
    if (tape != nullptr)
    {
        const auto name = editor.getText().trim();
        if (name.isNotEmpty())
            tape->setName(index, name);
        if (onChanged != nullptr)
            onChanged();
    }
    repaint();
}

void AdvancedDrawer::LaneRow::refresh()
{
    if (tape != nullptr)
        muteButton.setToggleState(tape->isMuted(index), juce::dontSendNotification);
    repaint();
}

AdvancedDrawer::LaneList::LaneList()
{
    for (int i = 0; i < TapeEngine::numLanes; ++i)
    {
        auto* row = rows.add(new LaneRow(i));
        row->onChanged = [this]
        {
            if (onChanged != nullptr)
                onChanged();
        };
        row->onFinishOthers = [this, i]
        {
            for (int r = 0; r < rows.size(); ++r)
                if (r != i)
                    rows[r]->finishNameEdit();
        };
        row->onSelectClip = [this](int lane)
        {
            setSelectedLane(lane);
            if (onSelectClip != nullptr)
                onSelectClip(lane);
        };
        row->onDeleteClip = [this](int lane)
        {
            if (onDeleteClip != nullptr)
                onDeleteClip(lane);
        };
        row->onViewChanged = [this]
        {
            if (onViewChanged != nullptr)
                onViewChanged();
        };
        addAndMakeVisible(row);
    }
}

void AdvancedDrawer::LaneList::finishEdits()
{
    for (auto* row : rows)
        row->finishNameEdit();
}

bool AdvancedDrawer::LaneList::isEditingName() const
{
    for (auto* row : rows)
        if (row->isEditingName())
            return true;
    return false;
}

void AdvancedDrawer::LaneList::setSelectedLane(int lane)
{
    selectedLane = lane;
    for (int i = 0; i < rows.size(); ++i)
        rows[i]->setSelected(i == lane);
}

void AdvancedDrawer::LaneList::setTape(TapeEngine* engineToUse)
{
    for (auto* row : rows)
        row->setTape(engineToUse);
}

void AdvancedDrawer::LaneList::setTimeline(TapeTimeline* timelineToUse)
{
    for (auto* row : rows)
        row->setTimeline(timelineToUse);
}

void AdvancedDrawer::LaneList::refresh()
{
    for (auto* row : rows)
        row->refresh();
}

void AdvancedDrawer::LaneList::resized()
{
    auto bounds = getLocalBounds();
    for (auto* row : rows)
    {
        row->setBounds(bounds.removeFromTop(laneH));
        bounds.removeFromTop(laneGap);
    }
}

juce::Rectangle<float> AdvancedDrawer::Ruler::handleBounds() const
{
    if (tape == nullptr || timeline == nullptr)
        return {};

    const float x = timeline->sampleToX(tape->getPlayhead(), (float) TapeTimeline::waveLeft);
    return { x - 7.0f, loopY, 14.0f, loopH };
}

juce::Rectangle<float> AdvancedDrawer::Ruler::loopBar() const
{
    if (tape == nullptr || timeline == nullptr || ! tape->isLoop())
        return {};

    const float waveX = (float) TapeTimeline::waveLeft;
    const float x0 = timeline->sampleToX(tape->getLoopStart(), waveX);
    const float x1 = timeline->sampleToX(tape->getLoopEnd(), waveX);
    return { x0, loopY, juce::jmax(loopEdgeW * 2.0f, x1 - x0), loopH };
}

juce::Rectangle<float> AdvancedDrawer::Ruler::loopEdge(bool right) const
{
    const auto bar = loopBar();
    if (bar.isEmpty())
        return {};
    return right ? bar.withX(bar.getRight() - loopEdgeW).withWidth(loopEdgeW)
                 : bar.withWidth(loopEdgeW);
}

void AdvancedDrawer::Ruler::seekTo(float x)
{
    if (tape == nullptr || timeline == nullptr)
        return;
    tape->setPlayhead(tape->snapSample(timeline->xToSample(x, (float) TapeTimeline::waveLeft)));
}

void AdvancedDrawer::Ruler::applyLoopCursor(juce::Point<float> p)
{
    if (handleBounds().expanded(4.0f, 2.0f).contains(p))
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
    else if (loopEdge(false).expanded(3.0f, 1.0f).contains(p)
             || loopEdge(true).expanded(3.0f, 1.0f).contains(p))
        setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
    else if (loopBar().contains(p))
        setMouseCursor(juce::MouseCursor::DraggingHandCursor);
    else
        setMouseCursor(juce::MouseCursor::NormalCursor);
}

void AdvancedDrawer::Ruler::paint(juce::Graphics& g)
{
    if (timeline == nullptr)
        return;

    const float waveX = (float) TapeTimeline::waveLeft;
    auto wave = juce::Rectangle<float>(waveX, 0.0f, (float) getWidth() - waveX, (float) getHeight());
    if (wave.getWidth() <= 1.0f)
        return;

    g.setColour(CuteLookAndFeel::voidFill());
    g.fillRoundedRectangle(wave.reduced(0.0f, 1.0f), 4.0f);

    const int spb = timeline->samplesPerBeat();
    const int vis = timeline->visibleSamples();
    const int first = (int) std::floor((double) timeline->viewStart / (double) spb) - 1;
    const int last = (int) std::ceil((double) (timeline->viewStart + vis) / (double) spb) + 1;
    const float h = (float) getHeight();

    if (auto* laf = dynamic_cast<CuteLookAndFeel*>(&getLookAndFeel()))
        g.setFont(laf->font(11.0f, true));

    for (int b = first; b <= last; ++b)
    {
        if (b < 0)
            continue;

        const float x = timeline->sampleToX(b * spb, waveX);
        if (x < wave.getX() - 1.0f || x > wave.getRight() + 1.0f)
            continue;

        const bool bar = (b % 4) == 0;
        if (bar)
        {
            g.setColour(CuteLookAndFeel::starlight().withAlpha(0.40f));
            g.fillRect(x, h - 11.0f, 1.0f, 11.0f);
            g.setColour(CuteLookAndFeel::mist().withAlpha(0.85f));
            g.drawText(juce::String(b / 4 + 1), (int) x + 3, 1, 28, 12,
                       juce::Justification::centredLeft, false);
        }
        else if (timeline->showQuarters())
        {
            g.setColour(CuteLookAndFeel::mist().withAlpha(0.22f));
            g.fillRect(x, h - 6.0f, 1.0f, 6.0f);
        }
    }

    if (timeline->showEighths())
    {
        g.setColour(CuteLookAndFeel::mist().withAlpha(0.12f));
        for (int b = first; b <= last; ++b)
        {
            if (b < 0)
                continue;
            const float x = timeline->sampleToX(b * spb + spb / 2, waveX);
            if (x < wave.getX() - 1.0f || x > wave.getRight() + 1.0f)
                continue;
            g.fillRect(x, h - 4.0f, 1.0f, 4.0f);
        }
    }

    if (tape == nullptr)
        return;

    if (tape->isLoop())
    {
        auto bar = loopBar().getIntersection(wave);
        if (! bar.isEmpty())
        {
            g.setColour(CuteLookAndFeel::nova().withAlpha(0.28f));
            g.fillRoundedRectangle(bar, 3.0f);
            auto left = loopEdge(false).getIntersection(wave);
            auto right = loopEdge(true).getIntersection(wave);
            g.setColour(CuteLookAndFeel::nova().interpolatedWith(CuteLookAndFeel::starlight(), 0.35f));
            if (! left.isEmpty())
                g.fillRoundedRectangle(left, 2.0f);
            if (! right.isEmpty())
                g.fillRoundedRectangle(right, 2.0f);
        }
    }

    const float px = timeline->sampleToX(tape->getPlayhead(), waveX);
    if (px < wave.getX() - 8.0f || px > wave.getRight() + 8.0f)
        return;

    juce::Path tri;
    tri.addTriangle(px - 6.5f, loopY, px + 6.5f, loopY, px, loopY + loopH);
    g.setColour(CuteLookAndFeel::starlight());
    g.fillPath(tri);
}

void AdvancedDrawer::Ruler::mouseDown(const juce::MouseEvent& e)
{
    if (timeline == nullptr || (float) e.x < (float) TapeTimeline::waveLeft)
        return;

    panStart = timeline->viewStart;
    panStartX = e.x;
    if (handleBounds().expanded(4.0f, 2.0f).contains(e.position))
    {
        drag = Drag::Handle;
        seekTo((float) e.x);
        return;
    }

    if (tape != nullptr && tape->isLoop())
    {
        if (loopEdge(false).expanded(3.0f, 1.0f).contains(e.position))
        {
            drag = Drag::LoopStart;
            dragLoopStart = tape->getLoopStart();
            dragLoopEnd = tape->getLoopEnd();
            return;
        }
        if (loopEdge(true).expanded(3.0f, 1.0f).contains(e.position))
        {
            drag = Drag::LoopEnd;
            dragLoopStart = tape->getLoopStart();
            dragLoopEnd = tape->getLoopEnd();
            return;
        }
        if (loopBar().contains(e.position))
        {
            drag = Drag::LoopMove;
            dragLoopStart = tape->getLoopStart();
            dragLoopEnd = tape->getLoopEnd();
            return;
        }
    }

    drag = Drag::Pan;
}

void AdvancedDrawer::Ruler::mouseDrag(const juce::MouseEvent& e)
{
    if (timeline == nullptr)
        return;

    if (drag == Drag::Handle)
    {
        seekTo((float) e.x);
        repaint();
        return;
    }

    if (tape != nullptr && (drag == Drag::LoopStart || drag == Drag::LoopEnd || drag == Drag::LoopMove))
    {
        const float waveX = (float) TapeTimeline::waveLeft;
        const int delta = timeline->xToSample((float) e.x, waveX)
                          - timeline->xToSample((float) panStartX, waveX);
        if (drag == Drag::LoopStart)
            tape->setLoopRange(tape->snapSample(dragLoopStart + delta), dragLoopEnd);
        else if (drag == Drag::LoopEnd)
            tape->setLoopRange(dragLoopStart, tape->snapSample(dragLoopEnd + delta));
        else
        {
            const int start = tape->snapSample(dragLoopStart + delta);
            tape->setLoopRange(start, start + (dragLoopEnd - dragLoopStart));
        }
        repaint();
        return;
    }

    if (drag != Drag::Pan)
        return;

    const int delta = timeline->xToSample((float) panStartX, (float) TapeTimeline::waveLeft)
                      - timeline->xToSample((float) e.x, (float) TapeTimeline::waveLeft);
    timeline->viewStart = juce::jmax(0, panStart + delta);
    timeline->markNav();
    if (onViewChanged != nullptr)
        onViewChanged();
}

void AdvancedDrawer::Ruler::mouseUp(const juce::MouseEvent& e)
{
    if (drag == Drag::Pan && std::abs(e.x - panStartX) < 4 && tape != nullptr)
        seekTo((float) e.x);
    if (drag == Drag::LoopStart || drag == Drag::LoopEnd || drag == Drag::LoopMove)
    {
        if (onChanged != nullptr)
            onChanged();
    }
    else if (onViewChanged != nullptr)
    {
        onViewChanged();
    }
    drag = Drag::None;
}

void AdvancedDrawer::Ruler::mouseMove(const juce::MouseEvent& e)
{
    applyLoopCursor(e.position);
}

void AdvancedDrawer::Ruler::mouseExit(const juce::MouseEvent&)
{
    setMouseCursor(juce::MouseCursor::NormalCursor);
}

void AdvancedDrawer::Ruler::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    if (timeline == nullptr || (float) e.x < (float) TapeTimeline::waveLeft)
        return;

    timeline->wheel((float) e.x, (float) TapeTimeline::waveLeft, wheel.deltaY, wheel.deltaX,
                    wheel.isReversed, e.mods.isShiftDown());
    if (onViewChanged != nullptr)
        onViewChanged();
}

namespace
{
    juce::String secondsText(double seconds)
    {
        if (std::abs(seconds - std::round(seconds)) < 0.0005)
            return juce::String(juce::roundToInt(seconds));
        return juce::String(seconds, 2);
    }

    class TapeExportPanel : public juce::Component
    {
    public:
        explicit TapeExportPanel(TapeEngine& engineToUse)
            : tape(engineToUse)
        {
            auto style = [](juce::TextEditor& editor)
            {
                editor.setJustification(juce::Justification::centredRight);
                editor.setInputRestrictions(8, "0123456789.");
                editor.setColour(juce::TextEditor::backgroundColourId, CuteLookAndFeel::panel());
                editor.setColour(juce::TextEditor::textColourId, CuteLookAndFeel::mist());
            };

            startLabel.setText("Start (s)", juce::dontSendNotification);
            lengthLabel.setText("Length (s)", juce::dontSendNotification);
            hint.setText("Same mix as playback. 24-bit stereo WAV.", juce::dontSendNotification);
            startLabel.setColour(juce::Label::textColourId, CuteLookAndFeel::mist());
            lengthLabel.setColour(juce::Label::textColourId, CuteLookAndFeel::mist());
            hint.setColour(juce::Label::textColourId, CuteLookAndFeel::dim());
            hint.setJustificationType(juce::Justification::centredLeft);

            const double sr = juce::jmax(1.0, tape.getSampleRate());
            double start = 0.0;
            double length = (double) tape.getViewSamples() / sr;
            if (tape.isLoop() && tape.getLoopEnd() > tape.getLoopStart())
            {
                start = (double) tape.getLoopStart() / sr;
                length = (double) (tape.getLoopEnd() - tape.getLoopStart()) / sr;
            }
            length = juce::jlimit(0.05, TapeEngine::maxSeconds, length);

            style(startField);
            style(lengthField);
            startField.setText(secondsText(start), juce::dontSendNotification);
            lengthField.setText(secondsText(length), juce::dontSendNotification);

            exportButton.setButtonText("Export WAV");
            exportButton.setColour(juce::TextButton::buttonColourId, CuteLookAndFeel::nova());
            exportButton.setColour(juce::TextButton::textColourOffId, CuteLookAndFeel::onAccent());
            exportButton.onClick = [this] { chooseAndExport(); };

            addAndMakeVisible(startLabel);
            addAndMakeVisible(startField);
            addAndMakeVisible(lengthLabel);
            addAndMakeVisible(lengthField);
            addAndMakeVisible(hint);
            addAndMakeVisible(exportButton);
        }

        void lookAndFeelChanged() override
        {
            if (auto* laf = dynamic_cast<CuteLookAndFeel*>(&getLookAndFeel()))
            {
                startLabel.setFont(laf->font(16.0f, true));
                lengthLabel.setFont(laf->font(16.0f, true));
                hint.setFont(laf->font(14.0f, true));
                startField.setFont(laf->font(16.0f, true));
                lengthField.setFont(laf->font(16.0f, true));
            }
        }

        void paint(juce::Graphics& g) override
        {
            g.fillAll(CuteLookAndFeel::voidFill());
        }

        void resized() override
        {
            auto bounds = getLocalBounds().reduced(20, 16);
            auto row = bounds.removeFromTop(28);
            startLabel.setBounds(row.removeFromLeft(96));
            startField.setBounds(row.removeFromRight(88));
            bounds.removeFromTop(10);
            row = bounds.removeFromTop(28);
            lengthLabel.setBounds(row.removeFromLeft(96));
            lengthField.setBounds(row.removeFromRight(88));
            bounds.removeFromTop(12);
            hint.setBounds(bounds.removeFromTop(22));
            bounds.removeFromTop(10);
            exportButton.setBounds(bounds.removeFromTop(32).removeFromRight(120));
        }

    private:
        void chooseAndExport()
        {
            const double start = juce::jlimit(0.0, TapeEngine::maxSeconds, startField.getText().getDoubleValue());
            const double length = juce::jlimit(0.05, TapeEngine::maxSeconds, lengthField.getText().getDoubleValue());
            startField.setText(secondsText(start), juce::dontSendNotification);
            lengthField.setText(secondsText(length), juce::dontSendNotification);

            auto chooser = std::make_shared<juce::FileChooser>("Export tape",
                                                               TapeEngine::tapeDirectory().getChildFile("mix.wav"),
                                                               "*.wav");
            const auto chooserFlags = juce::FileBrowserComponent::saveMode
                                      | juce::FileBrowserComponent::canSelectFiles
                                      | juce::FileBrowserComponent::warnAboutOverwriting;
            chooser->launchAsync(chooserFlags, [this, chooser, start, length](const juce::FileChooser& picked)
            {
                auto file = picked.getResult();
                if (file == juce::File())
                    return;
                if (! file.hasFileExtension(".wav"))
                    file = file.withFileExtension(".wav");

                juce::String error;
                if (! tape.exportMix(file, start, length, error))
                {
                    juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                                           "Export tape", error);
                    return;
                }

                file.revealToUser();
                if (auto* window = findParentComponentOfClass<juce::DialogWindow>())
                    window->exitModalState(1);
            });
        }

        TapeEngine& tape;
        juce::Label startLabel, lengthLabel, hint;
        juce::TextEditor startField, lengthField;
        juce::TextButton exportButton;
    };
}

AdvancedDrawer::AdvancedDrawer()
{
    setOpaque(false);
    playButton.setClickingTogglesState(false);
    stopButton.setClickingTogglesState(false);
    folderButton.setClickingTogglesState(false);
    exportButton.setClickingTogglesState(false);

    folderButton.onClick = []
    {
        const auto dir = TapeEngine::tapeDirectory();
        if (dir.isDirectory())
            dir.revealToUser();
    };
    exportButton.onClick = [this] { showExport(); };

    playButton.onClick = [this]
    {
        if (tape == nullptr)
            return;
        if (tape->isPlaying())
            tape->pause();
        else
            tape->play();
    };
    stopButton.onClick = [this]
    {
        if (tape != nullptr)
            tape->stop();
    };
    recordButton.onClick = [this]
    {
        if (tape == nullptr)
            return;
        if (recordButton.getToggleState())
        {
            if (tape->hasClip(tape->getArmedLane()))
            {
                recordButton.setToggleState(false, juce::dontSendNotification);
                return;
            }
            tape->record();
        }
        else
        {
            tape->stopRecord();
        }
    };
    quantizeButton.onClick = [this]
    {
        setQuantize(quantizeButton.getToggleState(), true);
    };
    loopButton.onClick = [this]
    {
        if (tape == nullptr)
            return;
        tape->setLoop(loopButton.getToggleState());
        notifyChanged();
        ruler.repaint();
    };

    bpmField.onChange = [this](float value)
    {
        bpm = value;
        syncTimeline();
        ruler.repaint();
        list.refresh();
        if (onBpmChange != nullptr)
            onBpmChange(value);
    };

    ruler.setTimeline(&timeline);
    ruler.onChanged = [this] { notifyChanged(); };
    ruler.onViewChanged = [this]
    {
        if (tape != nullptr)
            tape->setTimelineView(timeline.pixelsPerBeat, timeline.viewStart);
        ruler.repaint();
        list.refresh();
    };

    list.setTimeline(&timeline);
    list.onChanged = [this] { notifyChanged(); };
    list.onViewChanged = [this]
    {
        if (tape != nullptr)
            tape->setTimelineView(timeline.pixelsPerBeat, timeline.viewStart);
        ruler.repaint();
        list.refresh();
    };
    list.onDeleteClip = [this](int lane)
    {
        if (tape == nullptr || (tape->isRecording() && tape->getRecLane() == lane))
            return;
        tape->clearLane(lane);
        list.setSelectedLane(-1);
        notifyChanged();
    };
    viewport.setViewedComponent(&list, false);
    viewport.setScrollBarsShown(true, false);
    viewport.getVerticalScrollBar().setColour(juce::ScrollBar::thumbColourId, CuteLookAndFeel::dim());
    viewport.getVerticalScrollBar().setColour(juce::ScrollBar::trackColourId, juce::Colours::transparentBlack);

    addAndMakeVisible(playButton);
    addAndMakeVisible(stopButton);
    addAndMakeVisible(recordButton);
    addAndMakeVisible(quantizeButton);
    addAndMakeVisible(loopButton);
    addAndMakeVisible(folderButton);
    addAndMakeVisible(exportButton);
    addAndMakeVisible(ruler);
    addAndMakeVisible(viewport);
    addAndMakeVisible(metroButton);
    addAndMakeVisible(bpmField);
    addAndMakeVisible(sparkle);
}

void AdvancedDrawer::setTape(TapeEngine* engineToUse)
{
    tape = engineToUse;
    list.setTape(tape);
    list.setTimeline(&timeline);
    ruler.setTape(tape);
    ruler.setTimeline(&timeline);
    if (tape != nullptr)
    {
        timeline.pixelsPerBeat = tape->getPixelsPerBeat();
        timeline.viewStart = tape->getViewStart();
        loopButton.setToggleState(tape->isLoop(), juce::dontSendNotification);
    }
    syncTimeline();
}

void AdvancedDrawer::setQuantize(bool shouldQuantize, bool notify)
{
    quantizeOn = shouldQuantize;
    quantizeButton.setToggleState(shouldQuantize, juce::dontSendNotification);
    if (tape != nullptr)
        tape->setQuantize(shouldQuantize);
    if (notify && onQuantizeChange != nullptr)
        onQuantizeChange(shouldQuantize);
}

void AdvancedDrawer::refresh()
{
    if (tape != nullptr)
    {
        const auto t = tape->getTransport();
        const bool playing = t == TapeEngine::Transport::Playing
                             || t == TapeEngine::Transport::Recording;
        playButton.setToggleState(playing, juce::dontSendNotification);
        playButton.setTooltip(playing ? "Pause" : "Play");
        const bool rec = tape->isRecording();
        if (recordButton.getToggleState() != rec)
            recordButton.setToggleState(rec, juce::dontSendNotification);
        if (loopButton.getToggleState() != tape->isLoop())
            loopButton.setToggleState(tape->isLoop(), juce::dontSendNotification);
        if (tape->takeDirty())
            notifyChanged();
        const int selected = list.getSelectedLane();
        if (selected >= 0 && ! tape->hasClip(selected))
            list.setSelectedLane(-1);
    }
    syncTimeline();
    if (tape != nullptr && tape->isPlaying())
        timeline.followPlayhead(tape->getPlayhead());
    ruler.repaint();
    list.refresh();
}

void AdvancedDrawer::notifyChanged()
{
    if (tape != nullptr)
    {
        tape->setTimelineView(timeline.pixelsPerBeat, timeline.viewStart);
        tape->saveSession();
    }
    if (onChanged != nullptr)
        onChanged();
}

void AdvancedDrawer::mouseDown(const juce::MouseEvent&)
{
    list.finishEdits();
}

bool AdvancedDrawer::isEditingName() const
{
    return list.isEditingName();
}

bool AdvancedDrawer::deleteSelectedClip()
{
    const int lane = list.getSelectedLane();
    if (tape == nullptr || lane < 0)
        return false;
    if (tape->isRecording() && tape->getRecLane() == lane)
        return false;
    if (! tape->hasClip(lane))
        return false;
    tape->clearLane(lane);
    list.setSelectedLane(-1);
    notifyChanged();
    return true;
}

void AdvancedDrawer::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour(CuteLookAndFeel::voidFill().interpolatedWith(CuteLookAndFeel::panel(), 0.35f));
    g.fillRoundedRectangle(bounds.reduced(8.0f, 12.0f), 12.0f);

    if (auto* laf = dynamic_cast<CuteLookAndFeel*>(&getLookAndFeel()))
        g.setFont(laf->font(15.0f, true));
    g.setColour(CuteLookAndFeel::dim());
    g.drawText("Tape", 16, 16, getWidth() - 32, 28, juce::Justification::centredLeft, false);
}

void AdvancedDrawer::resized()
{
    auto bounds = getLocalBounds().reduced(16, 16);
    bounds.removeFromTop(36);

    auto metro = bounds.removeFromBottom(80);
    const auto metroRow = metro;
    metroButton.setBounds(metro.removeFromLeft(28).withSizeKeepingCentre(28, 28));
    bpmField.setBounds(metro.removeFromRight(52).withSizeKeepingCentre(52, 28));
    sparkle.setBounds(metroRow.withSizeKeepingCentre(80, 80));

    bounds.removeFromBottom(10);
    auto transport = bounds.removeFromTop(28);
    auto place = [&transport](CircleToggle& button)
    {
        button.setBounds(transport.removeFromLeft(28));
        transport.removeFromLeft(8);
    };
    place(playButton);
    place(stopButton);
    place(recordButton);
    place(quantizeButton);
    place(loopButton);
    place(folderButton);
    place(exportButton);

    bounds.removeFromTop(10);
    ruler.setBounds(bounds.removeFromTop(22));
    bounds.removeFromTop(4);
    viewport.setBounds(bounds);
    const int contentH = TapeEngine::numLanes * (laneH + laneGap) - laneGap;
    list.setSize(juce::jmax(1, viewport.getMaximumVisibleWidth() - 4), contentH);
    syncTimeline();
}

void AdvancedDrawer::showExport()
{
    if (tape == nullptr)
        return;
    if (tape->isRecording())
    {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                               "Export tape", "Stop recording first.");
        return;
    }

    auto* panel = new TapeExportPanel(*tape);
    panel->setLookAndFeel(&getLookAndFeel());
    panel->setSize(340, 196);

    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned(panel);
    options.dialogTitle = "Export tape";
    options.dialogBackgroundColour = CuteLookAndFeel::voidFill();
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = false;
    options.launchAsync();
}

void AdvancedDrawer::applyWheel(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel, float waveX)
{
    timeline.wheel((float) e.x, waveX, wheel.deltaY, wheel.deltaX, wheel.isReversed, e.mods.isShiftDown());
    if (tape != nullptr)
        tape->setTimelineView(timeline.pixelsPerBeat, timeline.viewStart);
    ruler.repaint();
    list.refresh();
}

void AdvancedDrawer::syncTimeline()
{
    const float waveW = juce::jmax(1.0f, (float) list.getWidth() - (float) TapeTimeline::waveLeft - 4.0f);
    const double sr = tape != nullptr ? tape->getSampleRate() : 48000.0;
    timeline.sync(waveW, bpm, sr);
}

void AdvancedDrawer::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    const float waveX = (float) (viewport.getX() + TapeTimeline::waveLeft);
    if ((float) e.x < waveX)
        return;
    applyWheel(e, wheel, waveX);
}

void AdvancedDrawer::setBpm(float value, bool notify)
{
    bpm = juce::jlimit(40.0f, 240.0f, value);
    bpmField.setBpm(bpm);
    syncTimeline();
    ruler.repaint();
    list.refresh();
    if (notify && onBpmChange != nullptr)
        onBpmChange(bpm);
}

void AdvancedDrawer::consumePulse(float pulse)
{
    if (pulse > 0.5f)
        sparkle.pulse();
}
