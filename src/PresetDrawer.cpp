#include "PresetDrawer.h"
#include "CuteLookAndFeel.h"

namespace
{
    constexpr int rowH = 32;
    constexpr int rowGap = 6;
}

PresetDrawer::Row::Row(PresetStore& storeToUse, int indexToUse,
                       std::function<void(const juce::String&)> onLoadToUse,
                       std::function<void(int)> onDeleteToUse)
    : store(storeToUse), index(indexToUse), onLoad(std::move(onLoadToUse)),
      onDelete(std::move(onDeleteToUse))
{
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
    editor.setVisible(false);
    editor.setJustification(juce::Justification::centredLeft);
    editor.onTextChange = [this] { applyLiveName(); };
    editor.onReturnKey = [this] { finishEdit(); };
    editor.onFocusLost = [this] { finishEdit(); };
    editor.setColour(juce::TextEditor::backgroundColourId, CuteLookAndFeel::voidFill());
    editor.setColour(juce::TextEditor::textColourId, CuteLookAndFeel::mist());
    addChildComponent(editor);
}

void PresetDrawer::Row::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour(CuteLookAndFeel::panel());
    g.fillRoundedRectangle(bounds, CuteLookAndFeel::corner());

    if (editing)
        return;

    if (auto* laf = dynamic_cast<CuteLookAndFeel*>(&getLookAndFeel()))
        g.setFont(laf->font(15.0f, true));
    else
        g.setFont(juce::FontOptions(15.0f, juce::Font::bold));

    const auto* item = store.at(index);
    g.setColour(CuteLookAndFeel::mist());
    g.drawText(item != nullptr ? item->name : juce::String(),
               bounds.reduced(10.0f, 0.0f).withTrimmedRight(28.0f),
               juce::Justification::centredLeft, true);

    deleteBounds = juce::Rectangle<float>(bounds.getRight() - 26.0f, bounds.getCentreY() - 9.0f, 18.0f, 18.0f);
    g.setColour(deleteHot ? CuteLookAndFeel::flare() : CuteLookAndFeel::dim());
    if (auto* laf = dynamic_cast<CuteLookAndFeel*>(&getLookAndFeel()))
        g.setFont(laf->font(14.0f, true));
    g.drawText("X", deleteBounds, juce::Justification::centred, false);
}

void PresetDrawer::Row::resized()
{
    auto bounds = getLocalBounds().toFloat();
    deleteBounds = juce::Rectangle<float>(bounds.getRight() - 26.0f, bounds.getCentreY() - 9.0f, 18.0f, 18.0f);
    auto edit = getLocalBounds().reduced(6, 4);
    edit.removeFromRight(24);
    editor.setBounds(edit);
}

void PresetDrawer::Row::mouseDown(const juce::MouseEvent& e)
{
    if (! editing && deleteBounds.contains(e.position))
    {
        if (onDelete != nullptr)
            onDelete(index);
        return;
    }

    if (e.mods.isPopupMenu())
        startEdit();
}

void PresetDrawer::Row::mouseDoubleClick(const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu() || editing || deleteBounds.contains(e.position))
        return;

    const auto* item = store.at(index);
    if (item != nullptr && onLoad != nullptr)
        onLoad(item->slug);
}

void PresetDrawer::Row::mouseMove(const juce::MouseEvent& e)
{
    const bool hot = ! editing && deleteBounds.contains(e.position);
    if (hot != deleteHot)
    {
        deleteHot = hot;
        repaint();
    }
}

void PresetDrawer::Row::mouseExit(const juce::MouseEvent&)
{
    if (deleteHot)
    {
        deleteHot = false;
        repaint();
    }
}

void PresetDrawer::Row::startEdit()
{
    const auto* item = store.at(index);
    if (item == nullptr)
        return;

    fallback = item->name;
    editing = true;
    editor.setText(item->name, juce::dontSendNotification);
    if (auto* laf = dynamic_cast<CuteLookAndFeel*>(&getLookAndFeel()))
        editor.setFont(laf->font(15.0f, true));
    editor.setVisible(true);
    editor.grabKeyboardFocus();
    editor.selectAll();
    repaint();
}

void PresetDrawer::Row::applyLiveName()
{
    const auto name = editor.getText().trim();
    if (name.isNotEmpty())
        store.setName(index, name);
}

void PresetDrawer::Row::finishEdit()
{
    if (! editing)
        return;

    editing = false;
    editor.setVisible(false);

    const auto name = editor.getText().trim();
    if (name.isEmpty())
        store.setName(index, fallback);
    else
        store.setName(index, name);

    repaint();
}

void PresetDrawer::List::rebuild(PresetStore& store,
                                std::function<void(const juce::String&)> onLoad,
                                std::function<void(int)> onDelete)
{
    rows.clear();
    for (int i = 0; i < store.size(); ++i)
    {
        auto* row = rows.add(new Row(store, i, onLoad, onDelete));
        addAndMakeVisible(row);
    }
    resized();
}

void PresetDrawer::List::resized()
{
    auto bounds = getLocalBounds();
    for (auto* row : rows)
    {
        row->setBounds(bounds.removeFromTop(rowH));
        bounds.removeFromTop(rowGap);
    }
}

PresetDrawer::PresetDrawer(PresetStore& storeToUse)
    : store(storeToUse),
      plusButton(CuteLookAndFeel::panel(), CuteLookAndFeel::mist(), "+")
{
    setOpaque(false);
    plusButton.setTooltip("save current as preset");
    plusButton.onClick = [this] { addCurrent(); };
    addAndMakeVisible(plusButton);

    viewport.setViewedComponent(&list, false);
    viewport.setScrollBarsShown(true, false);
    viewport.getVerticalScrollBar().setColour(juce::ScrollBar::thumbColourId, CuteLookAndFeel::dim());
    viewport.getVerticalScrollBar().setColour(juce::ScrollBar::trackColourId, juce::Colours::transparentBlack);
    addAndMakeVisible(viewport);
}

void PresetDrawer::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour(CuteLookAndFeel::voidFill().interpolatedWith(CuteLookAndFeel::panel(), 0.35f));
    g.fillRoundedRectangle(bounds.reduced(8.0f, 12.0f), 12.0f);

    if (auto* laf = dynamic_cast<CuteLookAndFeel*>(&getLookAndFeel()))
        g.setFont(laf->font(15.0f, true));
    g.setColour(CuteLookAndFeel::dim());
    g.drawText("Presets", 48, 16, getWidth() - 60, 28, juce::Justification::centredLeft, false);
}

void PresetDrawer::resized()
{
    auto bounds = getLocalBounds().reduced(16, 16);
    plusButton.setBounds(bounds.removeFromTop(28).removeFromLeft(28));
    bounds.removeFromTop(10);
    viewport.setBounds(bounds);
    const int contentH = juce::jmax(bounds.getHeight(),
                                    store.size() * (rowH + rowGap) - (store.size() > 0 ? rowGap : 0));
    list.setSize(juce::jmax(1, viewport.getMaximumVisibleWidth() - 4), contentH);
}

void PresetDrawer::rebuild()
{
    list.rebuild(store, onLoad, [this](int index)
    {
        store.remove(index);
        rebuild();
    });
    resized();
}

void PresetDrawer::scrollToEnd()
{
    viewport.setViewPosition(0, juce::jmax(0, list.getHeight() - viewport.getHeight()));
}

void PresetDrawer::addCurrent()
{
    if (getCurrentSlug == nullptr)
        return;

    store.add(getCurrentSlug());
    rebuild();
    scrollToEnd();
}
