#pragma once

#include "Appearance/Theme.h"

#include <functional>
#include <memory>

class CircleButton : public juce::Button
{
public:
    CircleButton(juce::Colour fillColour, juce::Colour glyphColour, juce::String glyph)
        : juce::Button({}), fill(fillColour), glyph(glyphColour), symbol(std::move(glyph))
    {
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
        setWantsKeyboardFocus(false);
    }

    void paintButton(juce::Graphics& g, bool highlighted, bool down) override
    {
        auto bounds = getLocalBounds().toFloat();
        const float d = juce::jmin(bounds.getWidth(), bounds.getHeight()) - (down ? 2.0f : 0.0f);
        const auto disc = bounds.withSizeKeepingCentre(d, d);
        g.setColour(highlighted ? fill.brighter(0.08f) : fill);
        g.fillEllipse(disc);
        g.setColour(glyph);
        if (auto* laf = dynamic_cast<Theme*>(&getLookAndFeel()))
            g.setFont(laf->font(16.0f, true));
        g.drawText(symbol, disc, juce::Justification::centred, false);
    }

private:
    juce::Colour fill;
    juce::Colour glyph;
    juce::String symbol;
};

namespace WindowShell
{
    constexpr float cornerRadius = 24.0f;
    constexpr float pad = 12.0f;
    constexpr float button = 28.0f;
    constexpr int barHeight = 52;

    inline juce::Path outline(juce::Rectangle<float> bounds)
    {
        juce::Path path;
        path.addRoundedRectangle(bounds, cornerRadius);
        return path;
    }

    inline bool hitTest(juce::Rectangle<int> bounds, int x, int y)
    {
        return outline(bounds.toFloat()).contains((float) x, (float) y);
    }

    inline void clip(juce::Graphics& g, juce::Rectangle<float> bounds)
    {
        g.reduceClipRegion(outline(bounds));
    }

    inline void beginDrag(juce::Component* top, const juce::MouseEvent& e, juce::ComponentDragger& dragger)
    {
        if (top != nullptr)
            dragger.startDraggingComponent(top, e);
    }

    inline void drag(juce::Component* top, const juce::MouseEvent& e, juce::ComponentDragger& dragger)
    {
        if (top != nullptr)
            dragger.dragComponent(top, e, nullptr);
    }

    class Bar : public juce::Component
    {
    public:
        explicit Bar(juce::String titleText)
            : title(std::move(titleText)),
              closeButton(Theme::flare(), Theme::onAccent(), "X")
        {
            setInterceptsMouseClicks(true, true);
            closeButton.onClick = [this]
            {
                if (onClose != nullptr)
                    onClose();
            };
            addAndMakeVisible(closeButton);
        }

        std::function<void()> onClose;

        void setTitle(juce::String next)
        {
            title = std::move(next);
            repaint();
        }

        void resized() override
        {
            const int d = (int) button;
            const int p = (int) pad;
            closeButton.setBounds(getWidth() - p - d, p, d, d);
        }

        void mouseDown(const juce::MouseEvent& e) override
        {
            if (e.eventComponent != this)
                return;
            beginDrag(getTopLevelComponent(), e, dragger);
        }

        void mouseDrag(const juce::MouseEvent& e) override
        {
            if (e.eventComponent != this)
                return;
            drag(getTopLevelComponent(), e, dragger);
        }

        void paint(juce::Graphics& g) override
        {
            auto area = getLocalBounds().toFloat().withTrimmedLeft(pad)
                            .withTrimmedRight(pad + button + 8.0f);
            if (auto* laf = dynamic_cast<Theme*>(&getLookAndFeel()))
                g.setFont(laf->titleFont(18.0f));
            else
                g.setFont(juce::FontOptions(18.0f, juce::Font::bold));
            g.setColour(Theme::starlight());
            g.drawText(title, area, juce::Justification::centredLeft, false);
        }

    private:
        juce::String title;
        CircleButton closeButton;
        juce::ComponentDragger dragger;
    };

    class Frame : public juce::Component
    {
    public:
        Frame(juce::String title, std::unique_ptr<juce::Component> bodyToUse)
            : bar(std::move(title)), body(std::move(bodyToUse))
        {
            setOpaque(true);
            bar.onClose = [this]
            {
                if (auto* window = findParentComponentOfClass<juce::DialogWindow>())
                    window->exitModalState(0);
            };
            addAndMakeVisible(bar);
            if (body != nullptr)
                addAndMakeVisible(*body);
        }

        bool hitTest(int x, int y) override
        {
            return WindowShell::hitTest(getLocalBounds(), x, y);
        }

        void paint(juce::Graphics& g) override
        {
            auto bounds = getLocalBounds().toFloat();
            clip(g, bounds);
            g.setColour(Theme::voidFill());
            g.fillRect(bounds);
        }

        void resized() override
        {
            auto bounds = getLocalBounds();
            bar.setBounds(bounds.removeFromTop(barHeight));
            if (body != nullptr)
                body->setBounds(bounds);
        }

    private:
        Bar bar;
        std::unique_ptr<juce::Component> body;
    };

    class AlertBody : public juce::Component
    {
    public:
        explicit AlertBody(juce::String messageToUse)
            : message(std::move(messageToUse))
        {
            ok.setButtonText("OK");
            ok.setColour(juce::TextButton::buttonColourId, Theme::nova());
            ok.setColour(juce::TextButton::textColourOffId, Theme::onAccent());
            ok.onClick = [this]
            {
                if (auto* window = findParentComponentOfClass<juce::DialogWindow>())
                    window->exitModalState(1);
            };
            addAndMakeVisible(ok);
        }

        void lookAndFeelChanged() override
        {
            if (auto* laf = dynamic_cast<Theme*>(&getLookAndFeel()))
                ok.setLookAndFeel(laf);
        }

        void paint(juce::Graphics& g) override
        {
            auto bounds = getLocalBounds().reduced(20, 12);
            bounds.removeFromBottom(42);
            if (auto* laf = dynamic_cast<Theme*>(&getLookAndFeel()))
                g.setFont(laf->font(16.0f, true));
            else
                g.setFont(juce::FontOptions(16.0f));
            g.setColour(Theme::mist());
            g.drawFittedText(message, bounds, juce::Justification::centredLeft, 4);
        }

        void resized() override
        {
            auto bounds = getLocalBounds().reduced(20, 12);
            ok.setBounds(bounds.removeFromBottom(32).removeFromRight(88));
        }

    private:
        juce::String message;
        juce::TextButton ok;
    };

    class Dialog : public juce::DialogWindow
    {
    public:
        Dialog(juce::String name)
            : juce::DialogWindow(std::move(name), Theme::voidFill(), true, false)
        {
            setUsingNativeTitleBar(false);
            setTitleBarHeight(0);
            setDropShadowEnabled(true);
            setOpaque(true);
        }

        void closeButtonPressed() override
        {
            setVisible(false);
        }
    };

    inline void launchDialog(juce::String title, std::unique_ptr<juce::Component> body,
                             int bodyW, int bodyH, bool resizable, juce::LookAndFeel* laf)
    {
        auto* frame = new Frame(title, std::move(body));
        frame->setSize(bodyW, barHeight + bodyH);
        if (laf != nullptr)
            frame->setLookAndFeel(laf);

        auto* window = new Dialog(std::move(title));
        if (laf != nullptr)
            window->setLookAndFeel(laf);
        window->setContentOwned(frame, true);
        window->setResizable(resizable, false);
        window->addToDesktop();
        window->centreAroundComponent(nullptr, window->getWidth(), window->getHeight());
        window->setVisible(true);
        window->enterModalState(true, nullptr, true);
    }

    inline void showAlert(juce::String title, juce::String message, juce::LookAndFeel* laf)
    {
        launchDialog(std::move(title), std::make_unique<AlertBody>(std::move(message)),
                     360, 132, false, laf);
    }
}
