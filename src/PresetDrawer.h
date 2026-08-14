#pragma once

#include "PresetStore.h"
#include "WindowChrome.h"

#include <functional>

class PresetDrawer : public juce::Component
{
public:
    static constexpr int width = 228;

    explicit PresetDrawer(PresetStore& storeToUse);

    void paint(juce::Graphics&) override;
    void resized() override;

    void rebuild();
    void scrollToEnd();

    std::function<juce::String()> getCurrentSlug;
    std::function<void(const juce::String&)> onLoad;

private:
    class Row : public juce::Component
    {
    public:
        Row(PresetStore& storeToUse, int indexToUse,
            std::function<void(const juce::String&)> onLoadToUse,
            std::function<void(int)> onDeleteToUse);

        void paint(juce::Graphics&) override;
        void resized() override;
        void mouseDown(const juce::MouseEvent&) override;
        void mouseDoubleClick(const juce::MouseEvent&) override;
        void mouseMove(const juce::MouseEvent&) override;
        void mouseExit(const juce::MouseEvent&) override;

    private:
        void startEdit();
        void finishEdit();
        void applyLiveName();

        PresetStore& store;
        int index = 0;
        juce::String fallback;
        juce::TextEditor editor;
        bool editing = false;
        std::function<void(const juce::String&)> onLoad;
        std::function<void(int)> onDelete;
        juce::Rectangle<float> deleteBounds;
        bool deleteHot = false;
    };

    class List : public juce::Component
    {
    public:
        void rebuild(PresetStore& store,
                     std::function<void(const juce::String&)> onLoad,
                     std::function<void(int)> onDelete);
        void resized() override;

    private:
        juce::OwnedArray<Row> rows;
    };

    void addCurrent();

    PresetStore& store;
    CircleButton plusButton;
    List list;
    juce::Viewport viewport;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetDrawer)
};
