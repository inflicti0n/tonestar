#include "Visuals/PlasmaTune.h"
#include "App/AppLog.h"
#include "Appearance/Theme.h"
#include <juce_gui_extra/juce_gui_extra.h>

namespace
{
class ColourPickerContent : public juce::Component,
                            private juce::ChangeListener
{
public:
    explicit ColourPickerContent(juce::Colour start)
        : selector(juce::ColourSelector::showColourAtTop
                   | juce::ColourSelector::showSliders
                   | juce::ColourSelector::showColourspace
                   | juce::ColourSelector::showAlphaChannel)
    {
        selector.setCurrentColour(start);
        selector.addChangeListener(this);
        addAndMakeVisible(selector);
        setSize(260, 360);
    }

    ~ColourPickerContent() override
    {
        selector.removeChangeListener(this);
    }

    void resized() override
    {
        selector.setBounds(getLocalBounds());
    }

    std::function<void(juce::Colour)> onChange;

private:
    void changeListenerCallback(juce::ChangeBroadcaster*) override
    {
        if (onChange != nullptr)
            onChange(selector.getCurrentColour());
    }

    juce::ColourSelector selector;
};
}

void PlasmaTune::Swatch::setColour(juce::Colour c)
{
    colour = c;
    repaint();
}

void PlasmaTune::Swatch::paint(juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced(1.0f);
    g.setColour(Theme::panel());
    g.fillRoundedRectangle(r, 6.0f);

    auto inner = r.reduced(3.0f);
    juce::Path clip;
    clip.addRoundedRectangle(inner, 4.0f);
    g.saveState();
    g.reduceClipRegion(clip);
    const float cell = 6.0f;
    for (float y = inner.getY(); y < inner.getBottom(); y += cell)
        for (float x = inner.getX(); x < inner.getRight(); x += cell)
        {
            const bool odd = (int) ((x - inner.getX()) / cell) + (int) ((y - inner.getY()) / cell);
            g.setColour(odd ? juce::Colour(0xff3a3f4a) : juce::Colour(0xff2a2e38));
            g.fillRect(x, y, cell, cell);
        }
    g.setColour(colour);
    g.fillRect(inner);
    g.restoreState();

    g.setColour(Theme::mist().withAlpha(0.35f));
    g.drawRoundedRectangle(r, 6.0f, 1.0f);
}

void PlasmaTune::Swatch::mouseUp(const juce::MouseEvent&)
{
    auto picker = std::make_unique<ColourPickerContent>(colour);
    picker->onChange = [this] (juce::Colour c)
    {
        setColour(c);
        if (onChange != nullptr)
            onChange();
    };
    juce::CallOutBox::launchAsynchronously(std::move(picker), getScreenBounds(), nullptr);
}

PlasmaTune::PlasmaTune()
{
    setVisible(false);
    look.loadFromFile();
    title.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(title);
    saveButton.onClick = [this]
    {
        pullFromControls();
        title.setText(look.saveToFile() ? "saved" : "save failed", juce::dontSendNotification);
        juce::Component::SafePointer<PlasmaTune> safe(this);
        juce::Timer::callAfterDelay(1200, [safe]
        {
            if (safe != nullptr)
                safe->title.setText("plasma tune", juce::dontSendNotification);
        });
    };
    addAndMakeVisible(saveButton);
    copyButton.onClick = [this]
    {
        pullFromControls();
        juce::SystemClipboard::copyTextToClipboard(look.dump());
    };
    addAndMakeVisible(copyButton);

    fluidHead.setJustificationType(juce::Justification::centredLeft);
    fluidHead.setColour(juce::Label::textColourId, Theme::dim());
    ringHead.setJustificationType(juce::Justification::centredLeft);
    ringHead.setColour(juce::Label::textColourId, Theme::dim());
    content.addAndMakeVisible(fluidHead);
    content.addAndMakeVisible(ringHead);

    int fluidCount = 0;
    const auto* fluidSpecs = plasmaSliderSpecs(fluidCount);
    addSliderRows(fluidSpecs, fluidCount);
    fluidRowCount = fluidCount;
    addColourSlot("deep + alpha", &PlasmaLook::deep);
    addColourSlot("body + alpha", &PlasmaLook::body);
    addColourSlot("core + alpha", &PlasmaLook::core);
    addColourSlot("hot + alpha", &PlasmaLook::hot);
    addColourSlot("accent + alpha", &PlasmaLook::accent);
    fluidColourCount = (int) colours.size();

    int ringCount = 0;
    const auto* ringSpecs = ringSliderSpecs(ringCount);
    addSliderRows(ringSpecs, ringCount);
    addColourSlot("line + alpha", &PlasmaLook::ringLine);
    addColourSlot("aura + alpha", &PlasmaLook::ringAura);
    addColourSlot("hot + alpha", &PlasmaLook::ringHot);

    viewport.setViewedComponent(&content, false);
    viewport.setScrollBarsShown(true, false);
    addAndMakeVisible(viewport);
    layoutReady = true;
    AppLog::note("PlasmaTune ready rows=" + juce::String((int) rows.size())
                 + " colours=" + juce::String((int) colours.size()));
}

void PlasmaTune::addSliderRows(const PlasmaSliderSpec* specs, int count)
{
    for (int i = 0; i < count; ++i)
    {
        auto row = std::make_unique<Row>();
        row->field = specs[i].field;
        row->label.setText(specs[i].name, juce::dontSendNotification);
        row->label.setJustificationType(juce::Justification::centredLeft);
        row->slider.setSliderStyle(juce::Slider::LinearHorizontal);
        row->slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 52, 16);
        row->slider.setRange(specs[i].min, specs[i].max, specs[i].interval);
        row->slider.setValue((double) (look.*(specs[i].field)), juce::dontSendNotification);
        row->slider.onValueChange = [this] { push(); };
        content.addAndMakeVisible(row->label);
        content.addAndMakeVisible(row->slider);
        rows.push_back(std::move(row));
    }
}

void PlasmaTune::addColourSlot(const char* name, juce::Colour PlasmaLook::* field)
{
    auto slot = std::make_unique<ColourSlot>();
    slot->field = field;
    const auto colour = look.*field;
    slot->label.setText(name, juce::dontSendNotification);
    slot->label.setJustificationType(juce::Justification::centredLeft);
    slot->swatch.setColour(colour);
    slot->swatch.onChange = [this, raw = slot.get()]
    {
        raw->alpha.setValue((double) raw->swatch.getColour().getFloatAlpha(),
                            juce::dontSendNotification);
        push();
    };
    slot->alpha.setSliderStyle(juce::Slider::LinearHorizontal);
    slot->alpha.setTextBoxStyle(juce::Slider::TextBoxRight, false, 52, 16);
    slot->alpha.setRange(0.0, 1.0, 0.01);
    slot->alpha.setValue((double) colour.getFloatAlpha(), juce::dontSendNotification);
    slot->alpha.onValueChange = [this, raw = slot.get()]
    {
        raw->swatch.setColour(raw->swatch.getColour().withAlpha((float) raw->alpha.getValue()));
        push();
    };
    content.addAndMakeVisible(slot->label);
    content.addAndMakeVisible(slot->swatch);
    content.addAndMakeVisible(slot->alpha);
    colours.push_back(std::move(slot));
}

void PlasmaTune::pullFromControls()
{
    for (auto& row : rows)
        look.*(row->field) = (float) row->slider.getValue();
    for (auto& slot : colours)
        look.*(slot->field) = slot->swatch.getColour().withAlpha((float) slot->alpha.getValue());
}

void PlasmaTune::push()
{
    pullFromControls();
    if (onChange != nullptr)
        onChange(look);
}

void PlasmaTune::paint(juce::Graphics& g)
{
    g.fillAll(Theme::voidFill());
    g.setColour(Theme::panel());
    g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(6.0f), 10.0f);
}

void PlasmaTune::resized()
{
    if (! layoutReady || layingOut || getWidth() < 40 || getHeight() < 40)
        return;

    layingOut = true;
    const struct Reset
    {
        bool& flag;
        ~Reset() { flag = false; }
    } reset { layingOut };

    auto bounds = getLocalBounds().reduced(12, 10);
    if (bounds.getWidth() < 20 || bounds.getHeight() < 40)
        return;

    auto head = bounds.removeFromTop(28);
    copyButton.setBounds(head.removeFromRight(52));
    head.removeFromRight(6);
    saveButton.setBounds(head.removeFromRight(52));
    head.removeFromRight(6);
    title.setBounds(head);
    bounds.removeFromTop(6);

    constexpr int rowH = 36;
    constexpr int colourH = 52;
    constexpr int sectionH = 22;
    const int contentH = sectionH * 2 + rowH * (int) rows.size() + colourH * (int) colours.size() + 16;
    const int innerW = juce::jmax(1, bounds.getWidth() - 14);
    auto area = juce::Rectangle<int>(0, 0, innerW, contentH);

    auto placeRow = [&] (Row& row)
    {
        auto slot = area.removeFromTop(rowH);
        row.label.setBounds(slot.removeFromTop(14));
        row.slider.setBounds(slot);
    };
    auto placeColour = [&] (ColourSlot& slot)
    {
        auto row = area.removeFromTop(colourH);
        slot.label.setBounds(row.removeFromTop(14));
        slot.swatch.setBounds(row.removeFromLeft(40));
        row.removeFromLeft(8);
        slot.alpha.setBounds(row);
    };

    fluidHead.setBounds(area.removeFromTop(sectionH));
    for (int i = 0; i < fluidRowCount && i < (int) rows.size(); ++i)
        placeRow(*rows[(size_t) i]);
    for (int i = 0; i < fluidColourCount && i < (int) colours.size(); ++i)
        placeColour(*colours[(size_t) i]);

    area.removeFromTop(6);
    ringHead.setBounds(area.removeFromTop(sectionH));
    for (int i = fluidRowCount; i < (int) rows.size(); ++i)
        placeRow(*rows[(size_t) i]);
    for (int i = fluidColourCount; i < (int) colours.size(); ++i)
        placeColour(*colours[(size_t) i]);

    content.setSize(innerW, contentH);
    viewport.setBounds(bounds);
}
