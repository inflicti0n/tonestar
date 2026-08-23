#include "Automation/AutomationView.h"
#include "Appearance/DragTip.h"
#include "Appearance/TitleBar.h"

namespace
{
    constexpr float knobR = 4.5f;
    constexpr float hitR = 8.0f;
    constexpr float curveHit = 7.0f;
}

AutomationView::AutomationView()
{
    setOpaque(false);
    setPaintingIsUnclipped(true);
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
}

void AutomationView::setTape(TapeEngine* engineToUse) { tape = engineToUse; }
void AutomationView::setLane(int laneToUse) { lane = laneToUse; }
void AutomationView::setTimeline(TapeTimeline* timelineToUse) { timeline = timelineToUse; }

void AutomationView::refresh()
{
    repaint();
}

AutomationTrack* AutomationView::track()
{
    return tape == nullptr ? nullptr : &tape->getAutomation().track(lane);
}

const AutomationTrack* AutomationView::track() const
{
    return tape == nullptr ? nullptr : &tape->getAutomation().track(lane);
}

juce::Rectangle<float> AutomationView::graphBounds() const
{
    return getLocalBounds().toFloat()
        .withTrimmedLeft((float) TapeTimeline::waveLeft - 4.0f)
        .reduced(4.0f, 6.0f);
}

juce::Rectangle<float> AutomationView::plusBounds() const
{
    return { 8.0f, 6.0f, 22.0f, 22.0f };
}

juce::Rectangle<float> AutomationView::enableBounds() const
{
    return { 34.0f, 6.0f, 22.0f, 22.0f };
}

juce::Rectangle<float> AutomationView::clearBounds() const
{
    return { 60.0f, 6.0f, 22.0f, 22.0f };
}

juce::Rectangle<float> AutomationView::chipBounds(int group) const
{
    const int col = group % 6;
    const int row = group / 6;
    return { 8.0f + (float) col * 18.0f, 34.0f + (float) row * 16.0f, 16.0f, 14.0f };
}

bool AutomationView::groupVisible(int group) const
{
    const auto* t = track();
    if (t == nullptr)
        return false;
    return (t->visibleGroups() & (1u << (uint32_t) group)) != 0;
}

float AutomationView::valueToY(const AutomationParam& spec, float value) const
{
    const auto g = graphBounds();
    return g.getBottom() - spec.norm(value) * juce::jmax(1.0f, g.getHeight());
}

float AutomationView::yToValue(const AutomationParam& spec, float y) const
{
    const auto g = graphBounds();
    const float n = g.getHeight() > 0.0f ? (g.getBottom() - y) / g.getHeight() : 0.0f;
    return spec.fromNorm(n);
}

float AutomationView::curveYAt(int paramIndex, int sample) const
{
    const auto* t = track();
    if (t == nullptr)
        return graphBounds().getCentreY();
    const auto& spec = AutomationParam::at(paramIndex);
    return valueToY(spec, spec.get(t->evaluate(sample)));
}

juce::Rectangle<float> AutomationView::knobBounds(int keyIndex, int paramIndex) const
{
    const auto* t = track();
    if (t == nullptr || timeline == nullptr || keyIndex < 0 || keyIndex >= t->count())
        return {};
    if (! t->hasPin(keyIndex, paramIndex))
        return {};
    const auto& key = t->get(keyIndex);
    const auto& spec = AutomationParam::at(paramIndex);
    const auto g = graphBounds();
    const float x = timeline->sampleToX(key.time, g.getX());
    const float y = valueToY(spec, spec.get(key.value));
    return juce::Rectangle<float>(knobR * 2.0f, knobR * 2.0f).withCentre({ x, y });
}

int AutomationView::hitKnob(juce::Point<float> p, int& paramOut) const
{
    const auto* t = track();
    if (t == nullptr)
        return -1;
    int best = -1;
    float bestD = hitR * hitR;
    paramOut = -1;
    for (int k = 0; k < t->count(); ++k)
    {
        for (int i = 0; i < AutomationParam::count; ++i)
        {
            if (! AutomationParam::drawsCurve(i) || ! groupVisible(AutomationParam::at(i).group))
                continue;
            if (! t->hasPin(k, i))
                continue;
            const auto knob = knobBounds(k, i);
            if (knob.isEmpty())
                continue;
            const float d = p.getDistanceSquaredFrom(knob.getCentre());
            if (d <= bestD)
            {
                bestD = d;
                best = k;
                paramOut = i;
            }
        }
    }
    return best;
}

int AutomationView::hitCurve(juce::Point<float> p, int& paramOut) const
{
    const auto* t = track();
    if (t == nullptr || timeline == nullptr || ! graphBounds().contains(p) || t->count() <= 0)
        return -1;
    const auto graph = graphBounds();
    const int sample = timeline->xToSample(p.x, graph.getX());
    int best = -1;
    float bestD = curveHit;
    paramOut = -1;
    for (int i = 0; i < AutomationParam::count; ++i)
    {
        if (! AutomationParam::drawsCurve(i) || ! groupVisible(AutomationParam::at(i).group))
            continue;
        const float d = std::abs(p.y - curveYAt(i, sample));
        if (d < bestD)
        {
            bestD = d;
            best = i;
        }
    }
    paramOut = best;
    return best;
}

int AutomationView::hitKeyLine(juce::Point<float> p) const
{
    const auto* t = track();
    if (t == nullptr || timeline == nullptr || ! graphBounds().contains(p))
        return -1;
    const auto g = graphBounds();
    int best = -1;
    float bestD = 6.0f;
    for (int k = 0; k < t->count(); ++k)
    {
        const float x = timeline->sampleToX(t->get(k).time, g.getX());
        const float d = std::abs(p.x - x);
        if (d < bestD)
        {
            bestD = d;
            best = k;
        }
    }
    return best;
}

int AutomationView::hitChip(juce::Point<float> p) const
{
    for (int g = 0; g < AutomationParam::groupCount; ++g)
        if (chipBounds(g).contains(p))
            return g;
    return -1;
}

void AutomationView::notify()
{
    if (onChanged != nullptr)
        onChanged();
    if (onEdited != nullptr)
        onEdited();
    repaint();
}

void AutomationView::insertHere()
{
    if (tape == nullptr || getLiveStamp == nullptr)
        return;
    int time = tape->getPlayhead();
    if (tape->isQuantize())
        time = tape->snapSample(time);
    tape->getAutomation().track(lane).insert(time, getLiveStamp());
    notify();
}

void AutomationView::addNodeAt(juce::Point<float> p, int param)
{
    auto* t = track();
    if (t == nullptr || timeline == nullptr || param < 0)
        return;
    const auto graph = graphBounds();
    int time = timeline->xToSample(p.x, graph.getX());
    if (tape != nullptr && tape->isQuantize())
        time = tape->snapSample(time);
    const auto& spec = AutomationParam::at(param);
    const float curveY = curveYAt(param, time);
    const float value = std::abs(p.y - curveY) < 6.0f ? spec.get(t->evaluate(time))
                                                      : yToValue(spec, p.y);
    t->insertNode(time, param, value);
    notify();
}

void AutomationView::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour(Theme::voidFill().interpolatedWith(Theme::panel(), 0.55f));
    g.fillRoundedRectangle(bounds.reduced(2.0f, 1.0f), 8.0f);

    const auto* t = track();
    const bool on = t != nullptr && t->isEnabled();

    auto drawBtn = [&] (juce::Rectangle<float> r, bool lit, juce::Colour colour)
    {
        g.setColour(lit ? colour : Theme::panel());
        g.fillEllipse(r);
    };

    drawBtn(plusBounds(), true, Theme::starlight());
    g.setColour(Theme::onAccent());
    if (auto* laf = dynamic_cast<Theme*>(&getLookAndFeel()))
        g.setFont(laf->font(14.0f, true));
    g.drawText("+", plusBounds(), juce::Justification::centred, false);

    drawBtn(enableBounds(), on, Theme::nova());
    paintAutomateIcon(g, enableBounds().reduced(5.0f), on ? Theme::onAccent() : Theme::dim());

    drawBtn(clearBounds(), t != nullptr && ! t->empty(), Theme::flare());
    g.setColour(t != nullptr && ! t->empty() ? Theme::onAccent() : Theme::dim());
    g.drawText("X", clearBounds(), juce::Justification::centred, false);

    const uint32_t visible = t != nullptr ? t->visibleGroups() : 0;
    for (int i = 0; i < AutomationParam::groupCount; ++i)
    {
        const auto chip = chipBounds(i);
        const bool shown = (visible & (1u << (uint32_t) i)) != 0;
        auto colour = AutomationParam::groupColour(i);
        g.setColour(shown ? colour : colour.withMultipliedAlpha(0.22f));
        g.fillRoundedRectangle(chip, 3.0f);
        if (hoverChip == i)
        {
            g.setColour(Theme::mist().withAlpha(0.55f));
            g.drawRoundedRectangle(chip.reduced(0.5f), 3.0f, 1.0f);
        }
    }

    const auto graph = graphBounds();
    g.setColour(Theme::voidFill());
    g.fillRoundedRectangle(graph, 6.0f);
    if (timeline != nullptr)
        timeline->paintGrid(g, graph);

    if (t == nullptr || timeline == nullptr)
        return;

    {
        juce::Graphics::ScopedSaveState save(g);
        g.reduceClipRegion(graph.getSmallestIntegerContainer());

    for (int k = 0; k < t->count(); ++k)
    {
        const float x = timeline->sampleToX(t->get(k).time, graph.getX());
        if (x < graph.getX() - 2.0f || x > graph.getRight() + 2.0f)
            continue;
        g.setColour(Theme::mist().withAlpha(k == hoverKey || k == dragKey ? 0.45f : 0.18f));
        g.fillRect(x, graph.getY(), 1.2f, graph.getHeight());
    }

    if (tape != nullptr)
    {
        const float px = timeline->sampleToX(tape->getPlayhead(), graph.getX());
        if (px >= graph.getX() && px <= graph.getRight())
        {
            g.setColour(Theme::nova().withAlpha(0.85f));
            g.fillRect(px, graph.getY(), 1.6f, graph.getHeight());
        }
    }

    const int left = timeline->xToSample(graph.getX(), graph.getX());
    const int right = timeline->xToSample(graph.getRight(), graph.getX());

    auto strokeSeg = [&] (float x0, float y0, juce::Colour c0, float x1, float y1, juce::Colour c1, float thick)
    {
        if (c0 == c1)
        {
            g.setColour(c0);
            g.drawLine(x0, y0, x1, y1, thick);
            return;
        }
        g.setGradientFill(juce::ColourGradient(c0, x0, y0, c1, x1, y1, false));
        g.drawLine(x0, y0, x1, y1, thick);
    };

    for (int p = 0; p < AutomationParam::count; ++p)
    {
        const auto& spec = AutomationParam::at(p);
        if (! AutomationParam::drawsCurve(p) || ! groupVisible(spec.group) || t->count() <= 0)
            continue;

        struct Node { float x = 0.0f; float y = 0.0f; juce::Colour colour; };
        juce::Array<Node> nodes;
        const auto addNode = [&] (int sample)
        {
            const auto stamp = t->evaluate(sample);
            nodes.add({ timeline->sampleToX(sample, graph.getX()),
                        valueToY(spec, spec.get(stamp)),
                        AutomationParam::curveColour(p, stamp) });
        };

        addNode(juce::jmin(left, t->get(0).time));
        for (int k = 0; k < t->count(); ++k)
            addNode(t->get(k).time);
        addNode(juce::jmax(right, t->get(t->count() - 1).time));

        const float thick = hoverCurve == p ? 2.4f : 1.5f;
        for (int i = 1; i < nodes.size(); ++i)
            strokeSeg(nodes[i - 1].x, nodes[i - 1].y, nodes[i - 1].colour,
                      nodes[i].x, nodes[i].y, nodes[i].colour, thick);
    }

    for (int k = 0; k < t->count(); ++k)
    {
        for (int p = 0; p < AutomationParam::count; ++p)
        {
            if (! AutomationParam::drawsCurve(p) || ! groupVisible(AutomationParam::at(p).group))
                continue;
            if (! t->hasPin(k, p))
                continue;
            const auto knob = knobBounds(k, p);
            if (! knob.intersects(graph))
                continue;
            const bool hot = (k == hoverKey && p == hoverParam) || (k == dragKey && p == dragParam);
            g.setColour(AutomationParam::curveColour(p, t->get(k).value));
            g.fillEllipse(hot ? knob.expanded(1.2f) : knob);
            g.setColour(Theme::voidFill().withAlpha(0.35f));
            g.drawEllipse(knob.reduced(1.0f), 1.0f);
        }
    }
    }
}

void AutomationView::showDragTip()
{
    auto* t = track();
    if (t == nullptr || drag != Drag::Value || dragKey < 0 || dragParam < 0 || dragKey >= t->count())
        return;
    const auto knob = knobBounds(dragKey, dragParam);
    if (knob.isEmpty())
        return;
    const auto& key = t->get(dragKey);
    const auto& spec = AutomationParam::at(dragParam);
    DragTip::show(*this, knob.getCentre(),
                  DragTip::autoValue(dragParam, spec.get(key.value)),
                  AutomationParam::curveColour(dragParam, key.value));
}

void AutomationView::resized() {}

void AutomationView::mouseDown(const juce::MouseEvent& e)
{
    if (tape == nullptr)
        return;

    if (plusBounds().contains(e.position))
    {
        insertHere();
        return;
    }
    if (enableBounds().contains(e.position))
    {
        if (auto* t = track())
        {
            t->setEnabled(! t->isEnabled());
            notify();
        }
        return;
    }
    if (clearBounds().contains(e.position))
    {
        if (auto* t = track())
        {
            t->clear();
            notify();
        }
        return;
    }

    const int chip = hitChip(e.position);
    if (chip >= 0)
    {
        if (auto* t = track())
        {
            uint32_t mask = t->isShownAuto() ? t->differingGroups() : t->shownMask();
            mask ^= (1u << (uint32_t) chip);
            t->setShownMask(mask);
            notify();
        }
        return;
    }

    if (e.mods.isMiddleButtonDown() && e.position.x >= (float) TapeTimeline::waveLeft
        && timeline != nullptr)
    {
        drag = Drag::ViewPan;
        viewPanStart = timeline->viewStart;
        viewPanX = e.x;
        setMouseCursor(juce::MouseCursor::DraggingHandCursor);
        return;
    }

    if (e.mods.isPopupMenu())
    {
        int param = -1;
        const int key = hitKnob(e.position, param);
        if (key >= 0 && param >= 0)
        {
            if (auto* t = track())
            {
                t->unpin(key, param);
                notify();
            }
        }
        return;
    }

    int param = -1;
    const int knob = hitKnob(e.position, param);
    if (knob >= 0 && param >= 0)
    {
        drag = Drag::Value;
        dragKey = knob;
        dragParam = param;
        showDragTip();
        return;
    }

    const int line = hitKeyLine(e.position);
    if (line >= 0)
    {
        drag = Drag::Time;
        dragKey = line;
        dragParam = -1;
        return;
    }

    if (hitCurve(e.position, param) >= 0)
        return;

    if (timeline != nullptr
        && (graphBounds().contains(e.position) || e.position.x >= (float) TapeTimeline::waveLeft))
    {
        drag = Drag::ViewPan;
        viewPanStart = timeline->viewStart;
        viewPanX = e.x;
        setMouseCursor(juce::MouseCursor::DraggingHandCursor);
    }
}

void AutomationView::mouseDrag(const juce::MouseEvent& e)
{
    if (drag == Drag::ViewPan && timeline != nullptr)
    {
        timeline->panByDrag(viewPanStart, (float) viewPanX, e.position.x, graphBounds().getX());
        if (onViewChanged != nullptr)
            onViewChanged();
        return;
    }

    auto* t = track();
    if (t == nullptr || timeline == nullptr || drag == Drag::None || dragKey < 0)
        return;

    if (drag == Drag::Value && dragParam >= 0)
    {
        const auto& spec = AutomationParam::at(dragParam);
        float value = yToValue(spec, e.position.y);
        if (e.mods.isCtrlDown() || juce::ModifierKeys::getCurrentModifiers().isCtrlDown())
            value = spec.snap(value);
        t->setParam(dragKey, dragParam, value);
        notify();
        showDragTip();
        return;
    }

    if (drag == Drag::Time)
    {
        const auto graph = graphBounds();
        int time = timeline->xToSample(e.position.x, graph.getX());
        if (tape != nullptr && tape->isQuantize())
            time = tape->snapSample(time);
        t->moveTime(dragKey, time);
        dragKey = t->indexAt(time);
        notify();
    }
}

void AutomationView::mouseUp(const juce::MouseEvent&)
{
    drag = Drag::None;
    dragKey = -1;
    dragParam = -1;
    setMouseCursor(juce::MouseCursor::NormalCursor);
    DragTip::hide();
    repaint();
}

void AutomationView::mouseDoubleClick(const juce::MouseEvent& e)
{
    int param = -1;
    if (hitKnob(e.position, param) >= 0)
        return;
    if (hitCurve(e.position, param) >= 0 && param >= 0)
        addNodeAt(e.position, param);
}

void AutomationView::applyHoverTooltip()
{
    if (hoverChip >= 0)
        setTooltip(AutomationParam::groupName(hoverChip));
    else if (hoverKey >= 0 && hoverParam >= 0)
        setTooltip(juce::String(AutomationParam::at(hoverParam).name) + " · Drag · Right-click remove");
    else if (hoverCurve >= 0)
        setTooltip(juce::String(AutomationParam::at(hoverCurve).name) + " · Double-click to add");
    else if (plusBounds().contains(getMouseXYRelative().toFloat()))
        setTooltip("Add snapshot at playhead");
    else if (enableBounds().contains(getMouseXYRelative().toFloat()))
        setTooltip("Enable automation");
    else if (clearBounds().contains(getMouseXYRelative().toFloat()))
        setTooltip("Clear automation");
    else
        setTooltip({});
}

void AutomationView::mouseMove(const juce::MouseEvent& e)
{
    int param = -1;
    const int knob = hitKnob(e.position, param);
    int curve = -1;
    int curveParam = -1;
    if (knob < 0)
        curve = hitCurve(e.position, curveParam);
    const int chip = hitChip(e.position);
    if (knob != hoverKey || param != hoverParam || chip != hoverChip || curveParam != hoverCurve)
    {
        hoverKey = knob;
        hoverParam = param;
        hoverChip = chip;
        hoverCurve = curveParam;
        applyHoverTooltip();
        juce::ignoreUnused(curve);
        repaint();
    }

    if (plusBounds().contains(e.position) || enableBounds().contains(e.position)
        || clearBounds().contains(e.position) || chip >= 0 || knob >= 0 || curve >= 0
        || hitKeyLine(e.position) >= 0)
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
    else if (graphBounds().contains(e.position) || e.position.x >= (float) TapeTimeline::waveLeft)
        setMouseCursor(juce::MouseCursor::DraggingHandCursor);
    else
        setMouseCursor(juce::MouseCursor::NormalCursor);
}

void AutomationView::mouseExit(const juce::MouseEvent&)
{
    hoverKey = -1;
    hoverParam = -1;
    hoverChip = -1;
    hoverCurve = -1;
    setTooltip({});
    setMouseCursor(juce::MouseCursor::NormalCursor);
    repaint();
}

void AutomationView::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    const auto graph = graphBounds();
    if (timeline != nullptr && e.position.x >= (float) TapeTimeline::waveLeft)
    {
        timeline->wheel(e.position.x, graph.getX(), wheel.deltaY, wheel.deltaX,
                        wheel.isReversed, e.mods.isShiftDown());
        if (onViewChanged != nullptr)
            onViewChanged();
        return;
    }
    if (auto* parent = getParentComponent())
        parent->mouseWheelMove(e.getEventRelativeTo(parent), wheel);
}
