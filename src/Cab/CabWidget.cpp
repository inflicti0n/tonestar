#include "Cab/CabWidget.h"

CabWidget::CabWidget()
    : vblank(this, [this] { tick(); })
{
    setOpaque(false);
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
    setTooltip("wheel size · click land · right-click back");
}

void CabWidget::setSizeAmount(float value, bool notify)
{
    sizeTarget = juce::jlimit(0.0f, 1.0f, value);
    if (! notify)
        sizeDisplay = sizeTarget;
    notifyIf(notify);
}

void CabWidget::setBackAmount(float value, bool notify)
{
    backTarget = juce::jlimit(0.0f, 1.0f, value);
    if (! notify)
        backDisplay = backTarget;
    notifyIf(notify);
}

void CabWidget::notifyIf(bool shouldNotify)
{
    if (shouldNotify && onChange != nullptr)
        onChange();
}

void CabWidget::punch()
{
    punchScale = 1.06f;
}

void CabWidget::cycleSize()
{
    if (sizeTarget < 0.33f)
        sizeTarget = twinLand;
    else if (sizeTarget < 0.67f)
        sizeTarget = stackLand;
    else
        sizeTarget = comboLand;

    punch();
    notifyIf(true);
}

void CabWidget::toggleBack()
{
    backTarget = backTarget < 0.5f ? closedLand : openLand;
    punch();
    notifyIf(true);
}

void CabWidget::mouseDown(const juce::MouseEvent& e)
{
    pressed = true;
    pressWasLeft = false;

    if (e.mods.isPopupMenu())
    {
        toggleBack();
        return;
    }

    pressWasLeft = e.mods.isLeftButtonDown();
}

void CabWidget::mouseUp(const juce::MouseEvent& e)
{
    const bool wasLeft = pressWasLeft;
    pressed = false;
    pressWasLeft = false;

    if (! wasLeft || e.mouseWasDraggedSinceMouseDown())
        return;

    cycleSize();
}

void CabWidget::mouseEnter(const juce::MouseEvent&)
{
    hovered = true;
}

void CabWidget::mouseExit(const juce::MouseEvent&)
{
    hovered = false;
    pressed = false;
    pressWasLeft = false;
}

void CabWidget::mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    float delta = wheel.deltaY != 0.0f ? wheel.deltaY : wheel.deltaX;
    if (wheel.isReversed)
        delta = -delta;

    const float step = (delta > 0.0f ? 1.0f : (delta < 0.0f ? -1.0f : 0.0f)) * 0.05f;
    if (step == 0.0f)
        return;

    setSizeAmount(sizeTarget + step, true);
}

void CabWidget::tick()
{
    auto chase = [](float& display, float target)
    {
        const float d = target - display;
        if (std::abs(d) > 0.0008f)
            display += d * 0.38f;
        else
            display = target;
    };

    chase(sizeDisplay, sizeTarget);
    chase(backDisplay, backTarget);

    if (punchScale > 1.001f)
        punchScale += (1.0f - punchScale) * 0.22f;
    else
        punchScale = 1.0f;

    repaint();
}

void CabWidget::fillRound(juce::Graphics& g, juce::Rectangle<float> r, float radius, juce::Colour c) const
{
    g.setColour(c);
    g.fillRoundedRectangle(r, radius);
}

void CabWidget::drawSpeaker(juce::Graphics& g, juce::Point<float> c, float radius, float openness) const
{
    const float r = juce::jmax(5.0f, radius);
    auto disc = juce::Rectangle<float>(c.x - r, c.y - r, r * 2.0f, r * 2.0f);

    if (openness > 0.08f)
    {
        g.setColour(CuteLookAndFeel::nova().interpolatedWith(CuteLookAndFeel::starlight(), 0.35f)
                        .withAlpha(0.22f + 0.45f * openness));
        g.fillEllipse(disc.expanded(2.0f));
        g.setColour(CuteLookAndFeel::voidFill().interpolatedWith(CuteLookAndFeel::panel(), 0.25f));
        g.fillEllipse(disc);
        const float hole = r * juce::jmap(openness, 0.42f, 0.62f);
        g.setColour(CuteLookAndFeel::nova().withAlpha(0.55f * openness));
        g.fillEllipse(c.x - hole, c.y - hole, hole * 2.0f, hole * 2.0f);
        g.setColour(CuteLookAndFeel::starlight().withAlpha(0.35f * openness));
        g.fillEllipse(c.x - hole * 0.45f, c.y - hole * 0.45f, hole * 0.9f, hole * 0.9f);
        return;
    }

    g.setColour(CuteLookAndFeel::voidFill());
    g.fillEllipse(disc);
    g.setColour(CuteLookAndFeel::panel().brighter(0.08f));
    g.fillEllipse(disc.reduced(r * 0.22f));
    g.setColour(CuteLookAndFeel::dim().withAlpha(0.55f));
    g.fillEllipse(c.x - r * 0.22f, c.y - r * 0.22f, r * 0.44f, r * 0.44f);
}

void CabWidget::drawCabinet(juce::Graphics& g, juce::Rectangle<float> body, int speakers, float openness,
                            bool withPanel) const
{
    const float rad = 7.0f;
    const float hover = hovered ? 0.08f : 0.0f;

    if (openness > 0.04f)
    {
        const float peek = juce::jmap(openness, 8.0f, 14.0f);
        auto rear = body.translated(peek, peek * 0.45f);
        fillRound(g, rear, rad, CuteLookAndFeel::starlight().withAlpha(0.18f + 0.28f * openness));
        fillRound(g, rear.reduced(5.0f, 6.0f), rad - 2.0f,
                  CuteLookAndFeel::nova().withAlpha(0.16f + 0.32f * openness));
    }

    fillRound(g, body, rad, CuteLookAndFeel::panel().brighter(hover));

    auto baffle = body;
    if (withPanel)
    {
        const float panelH = juce::jmax(14.0f, body.getHeight() * 0.18f);
        auto panel = body.removeFromTop(panelH);
        fillRound(g, panel, rad, CuteLookAndFeel::voidFill().interpolatedWith(CuteLookAndFeel::panel(), 0.35f));
        const int knobs = speakers <= 1 ? 3 : 5;
        const float ky = panel.getCentreY();
        const float kr = juce::jmin(3.2f, panel.getHeight() * 0.16f);
        const float span = panel.getWidth() * 0.42f;
        const float x0 = panel.getCentreX() - span * 0.5f;
        for (int i = 0; i < knobs; ++i)
        {
            const float t = knobs == 1 ? 0.5f : (float) i / (float) (knobs - 1);
            g.setColour(CuteLookAndFeel::dim().interpolatedWith(CuteLookAndFeel::nova(), hover));
            g.fillEllipse(x0 + span * t - kr, ky - kr, kr * 2.0f, kr * 2.0f);
        }
        baffle = body.withTrimmedTop(3.0f);
    }

    baffle = baffle.reduced(speakers >= 4 ? 10.0f : 12.0f, speakers >= 4 ? 8.0f : 10.0f);

    const auto place = [&](int count)
    {
        if (count <= 1)
        {
            drawSpeaker(g, baffle.getCentre(), juce::jmin(baffle.getWidth(), baffle.getHeight()) * 0.36f, openness);
            return;
        }

        if (count == 2)
        {
            const float r = juce::jmin(baffle.getWidth() * 0.22f, baffle.getHeight() * 0.38f);
            const float midY = baffle.getCentreY();
            const float x0 = baffle.getX() + baffle.getWidth() * 0.27f;
            const float x1 = baffle.getX() + baffle.getWidth() * 0.73f;
            drawSpeaker(g, { x0, midY }, r, openness);
            drawSpeaker(g, { x1, midY }, r, openness);
            return;
        }

        const float r = juce::jmin(baffle.getWidth(), baffle.getHeight()) * 0.18f;
        const float xs[2] { baffle.getX() + baffle.getWidth() * 0.28f,
                            baffle.getX() + baffle.getWidth() * 0.72f };
        const float ys[2] { baffle.getY() + baffle.getHeight() * 0.28f,
                            baffle.getY() + baffle.getHeight() * 0.72f };
        for (float y : ys)
            for (float x : xs)
                drawSpeaker(g, { x, y }, r, openness);
    };

    place(speakers);

    const float footW = juce::jmax(8.0f, body.getWidth() * 0.08f);
    const float footH = 4.0f;
    const float footY = body.getBottom() - 1.0f;
    fillRound(g, { body.getX() + body.getWidth() * 0.14f, footY, footW, footH }, 2.0f,
              CuteLookAndFeel::voidFill());
    fillRound(g, { body.getRight() - body.getWidth() * 0.14f - footW, footY, footW, footH }, 2.0f,
              CuteLookAndFeel::voidFill());
}

void CabWidget::drawHead(juce::Graphics& g, juce::Rectangle<float> body) const
{
    const float rad = 6.0f;
    fillRound(g, body, rad, CuteLookAndFeel::panel().brighter(hovered ? 0.06f : 0.0f));
    auto panel = body.reduced(6.0f, 4.0f);
    fillRound(g, panel, 4.0f, CuteLookAndFeel::voidFill().interpolatedWith(CuteLookAndFeel::panel(), 0.28f));

    const int knobs = 6;
    const float ky = panel.getCentreY();
    const float kr = juce::jmin(3.0f, panel.getHeight() * 0.18f);
    const float span = panel.getWidth() * 0.62f;
    const float x0 = panel.getCentreX() - span * 0.5f;
    for (int i = 0; i < knobs; ++i)
    {
        const float t = (float) i / (float) (knobs - 1);
        g.setColour(CuteLookAndFeel::dim());
        g.fillEllipse(x0 + span * t - kr, ky - kr, kr * 2.0f, kr * 2.0f);
    }
}

void CabWidget::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    auto caption = bounds.removeFromBottom(22.0f);
    auto stage = bounds.reduced(10.0f, 6.0f);

    const float size = sizeDisplay;
    const float openness = 1.0f - backDisplay;
    const float bounce = pressed ? 1.0f : 0.0f;

    float nw = 0.50f;
    float nh = 0.78f;
    float stackT = 0.0f;
    if (size <= 0.5f)
    {
        const float t = size / 0.5f;
        nw = juce::jmap(t, 0.50f, 0.84f);
        nh = juce::jmap(t, 0.78f, 0.68f);
    }
    else
    {
        const float t = (size - 0.5f) / 0.5f;
        nw = juce::jmap(t, 0.84f, 0.58f);
        nh = juce::jmap(t, 0.68f, 0.92f);
        stackT = t;
    }

    auto body = juce::Rectangle<float>(stage.getWidth() * nw, stage.getHeight() * nh)
                    .withCentre({ stage.getCentreX(), stage.getCentreY() + bounce });
    body = body.withSizeKeepingCentre(body.getWidth() * punchScale, body.getHeight() * punchScale);

    const int land = sizeTarget < 0.33f ? 1 : (sizeTarget < 0.67f ? 2 : 4);

    if (stackT < 0.12f)
    {
        drawCabinet(g, body, land == 4 ? 2 : land, openness, true);
    }
    else
    {
        const float headH = juce::jmax(18.0f, body.getHeight() * juce::jmap(stackT, 0.16f, 0.22f));
        auto head = body.removeFromTop(headH);
        body.removeFromTop(juce::jmap(stackT, 2.0f, 5.0f));
        drawHead(g, head);
        drawCabinet(g, body, land == 4 ? 4 : 2, openness, false);
    }

    if (auto* laf = dynamic_cast<CuteLookAndFeel*>(&getLookAndFeel()))
        g.setFont(laf->font(15.0f, true));
    else
        g.setFont(juce::FontOptions(15.0f, juce::Font::bold));

    const bool isOpen = backTarget < 0.5f;
    const auto sizeName = juce::String(Acoustics::sizeLand(sizeTarget));
    const auto backName = juce::String(Acoustics::backLand(backTarget));
    const float mid = caption.getCentreX();
    g.setColour(CuteLookAndFeel::mist());
    g.drawText(sizeName, caption.withTrimmedRight(caption.getWidth() * 0.5f + 8.0f)
                                .withRight(mid - 8.0f),
               juce::Justification::centredRight, false);
    g.setColour(isOpen ? CuteLookAndFeel::nova() : CuteLookAndFeel::dim());
    g.drawText(backName, caption.withTrimmedLeft(caption.getWidth() * 0.5f)
                                .withX(mid + 8.0f),
               juce::Justification::centredLeft, false);
}
