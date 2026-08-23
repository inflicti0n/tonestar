#pragma once

#include "Automation/AutomationView.h"
#include "Tape/TapeEngine.h"
#include "Tape/TapeTimeline.h"
#include "Appearance/TitleBar.h"
#include "Visuals/Tuner.h"
#include "Vocals/VocalCompose.h"

#include <functional>

class AdvancedDrawer : public juce::Component
{
public:
    static constexpr int width = 780;
    static constexpr int extraHeight = 96;

    AdvancedDrawer();

    void paint(juce::Graphics&) override;
    void resized() override;

    void setTape(TapeEngine* engineToUse);
    void refresh();
    void setBpm(float bpm, bool notify);
    float getBpm() const { return bpm; }
    void setMetroArmed(bool shouldArm) { sparkle.setArmed(shouldArm); }
    void setTunerReading(const TunerReading& reading, bool armed) { tunerFace.setReading(reading, armed); }
    void consumePulse(float pulse);
    void setQuantize(bool shouldQuantize, bool notify);
    bool isQuantize() const { return quantizeOn; }
    bool deleteSelectedClip();
    bool isEditingName() const;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    CircleToggle playButton { "Play", Theme::starlight(), CircleIcon::Play };
    CircleToggle stopButton { "Stop", Theme::mist(), CircleIcon::Stop };
    CircleToggle recordButton { "Record", Theme::flare(), CircleIcon::Record };
    CircleToggle quantizeButton { "Quantize", Theme::nova(), CircleIcon::Quantize };
    CircleToggle loopButton { "Loop", Theme::nova(), CircleIcon::Loop };
    CircleToggle folderButton { "Tape", Theme::panel(), CircleIcon::Folder };
    CircleToggle exportButton { "Export", Theme::panel(), CircleIcon::Export };
    CircleToggle importButton { "Import to current track", Theme::panel(), CircleIcon::Import };
    CircleToggle metroButton { "Metronome", Theme::nova(), CircleIcon::Metro };
    CircleToggle tunerButton { "Tuner", Theme::nova(), CircleIcon::Tune };

    std::function<void(float)> onBpmChange;
    std::function<void()> onChanged;
    std::function<void(bool)> onQuantizeChange;
    std::function<void(int)> onSelectLane;
    std::function<VocalStamp()> getLiveStamp;
    std::function<void()> onAutomationEdited;
    int getSelectedLane() const { return list.getSelectedLane(); }
    void relayoutList();

private:
    class BpmField : public juce::Component
    {
    public:
        void paint(juce::Graphics&) override;
        void resized() override;
        void mouseDown(const juce::MouseEvent&) override;
        void mouseDrag(const juce::MouseEvent&) override;
        void mouseUp(const juce::MouseEvent&) override;
        void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

        void setBpm(float value);
        float getBpm() const { return bpm; }
        std::function<void(float)> onChange;

    private:
        void startEdit();
        void finishEdit();
        void applyBpm(float value, bool notify);

        float bpm = 120.0f;
        float dragStart = 120.0f;
        bool dragging = false;
        bool editing = false;
        juce::TextEditor editor;
    };

    class Sparkle : public juce::Component
    {
    public:
        Sparkle();
        void paint(juce::Graphics&) override;
        void setArmed(bool shouldArm);
        void pulse();

    private:
        void tick();
        juce::Path starPath(float scale) const;

        bool armed = false;
        float punch = 1.0f;
        juce::VBlankAttachment vblank;
    };

    class TunerFace : public juce::Component
    {
    public:
        void paint(juce::Graphics&) override;
        void setReading(const TunerReading& next, bool shouldArm);

    private:
        TunerReading reading;
        float displayCents = 0.0f;
        bool armed = false;
        bool wasVoiced = false;
    };

    class LaneRow : public juce::Component
    {
    public:
        explicit LaneRow(int indexToUse);

        void paint(juce::Graphics&) override;
        void resized() override;
        void mouseDown(const juce::MouseEvent&) override;
        void mouseDrag(const juce::MouseEvent&) override;
        void mouseUp(const juce::MouseEvent&) override;
        void mouseDoubleClick(const juce::MouseEvent&) override;
        void mouseMove(const juce::MouseEvent&) override;
        void mouseExit(const juce::MouseEvent&) override;
        void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

        void setTape(TapeEngine* engineToUse);
        void setTimeline(TapeTimeline* timelineToUse);
        void setSelected(bool shouldSelect);
        void refresh();
        void finishNameEdit();
        bool isEditingName() const { return editing; }
        int rowHeight() const;
        void setExpanded(bool shouldExpand);
        bool isExpanded() const { return expanded; }

        std::function<void()> onChanged;
        std::function<void()> onFinishOthers;
        std::function<void(int)> onSelectClip;
        std::function<void(int)> onDeleteClip;
        std::function<void()> onViewChanged;
        std::function<void()> onLayoutChanged;
        std::function<VocalStamp()> getLiveStamp;
        std::function<void()> onAutomationEdited;

    private:
        enum class Drag { None, Move, Trim, TrimIn, ViewPan };

        void beginViewPan(float x);
        void applyViewPan(float x);

        juce::Rectangle<int> laneBounds() const;
        juce::Rectangle<float> waveBounds() const;
        juce::Rectangle<float> nameBounds() const;
        juce::Rectangle<float> levelBounds() const;
        juce::Rectangle<float> levelThumb() const;
        juce::Rectangle<float> panBounds() const;
        juce::Rectangle<float> panThumb() const;
        juce::Rectangle<float> clipBounds() const;
        juce::Rectangle<float> deleteBoundsFor(juce::Rectangle<float> clip) const;
        bool inLevel(juce::Point<float> p) const;
        bool inPan(juce::Point<float> p) const;
        void setLevelFromX(float x);
        void setPanFromX(float x);
        void startNameEdit();
        void drawWave(juce::Graphics& g, juce::Rectangle<float> clip, const TapeEngine::LaneView& view,
                      int recFrames) const;

        int index = 0;
        TapeEngine* tape = nullptr;
        TapeTimeline* timeline = nullptr;
        AutomationView autoView;
        PieToggle pie;
        juce::TextEditor editor;
        bool editing = false;
        bool selected = false;
        bool expanded = false;
        bool deleteHot = false;
        bool draggingLevel = false;
        bool draggingPan = false;
        Drag drag = Drag::None;
        int dragStartValue = 0;
        int dragStartX = 0;
        int viewPanStart = 0;
        juce::Rectangle<float> deleteBounds;
    };

    class LaneList : public juce::Component
    {
    public:
        LaneList();
        void setTape(TapeEngine* engineToUse);
        void setTimeline(TapeTimeline* timelineToUse);
        void refresh();
        void resized() override;
        void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
        void finishEdits();
        bool isEditingName() const;
        void setSelectedLane(int lane);
        int getSelectedLane() const { return selectedLane; }
        int contentHeight() const;
        std::function<void()> onChanged;
        std::function<void(int)> onSelectClip;
        std::function<void(int)> onDeleteClip;
        std::function<void()> onViewChanged;
        std::function<void()> onLayoutChanged;
        std::function<VocalStamp()> getLiveStamp;
        std::function<void()> onAutomationEdited;

    private:
        juce::OwnedArray<LaneRow> rows;
        TapeTimeline* timeline = nullptr;
        int selectedLane = -1;
    };

    class Ruler : public juce::Component
    {
    public:
        void paint(juce::Graphics&) override;
        void mouseDown(const juce::MouseEvent&) override;
        void mouseDrag(const juce::MouseEvent&) override;
        void mouseUp(const juce::MouseEvent&) override;
        void mouseMove(const juce::MouseEvent&) override;
        void mouseExit(const juce::MouseEvent&) override;
        void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

        void setTape(TapeEngine* engineToUse) { tape = engineToUse; }
        void setTimeline(TapeTimeline* timelineToUse) { timeline = timelineToUse; }

        std::function<void()> onViewChanged;
        std::function<void()> onChanged;
        std::function<void()> onSeek;

    private:
        enum class Drag { None, Handle, LoopStart, LoopEnd, LoopMove, Pan };

        juce::Rectangle<float> handleBounds() const;
        juce::Path handlePath() const;
        juce::Rectangle<float> loopBar() const;
        juce::Rectangle<float> loopEdge(bool right) const;
        void seekTo(float x);
        void applyLoopCursor(juce::Point<float> p);

        TapeEngine* tape = nullptr;
        TapeTimeline* timeline = nullptr;
        Drag drag = Drag::None;
        int panStart = 0;
        int panStartX = 0;
        int dragLoopStart = 0;
        int dragLoopEnd = 0;
    };

    void notifyChanged();
    void applyWheel(const juce::MouseEvent&, const juce::MouseWheelDetails&, float waveX);
    void syncTimeline();
    void showExport();
    void showImport();

    TapeEngine* tape = nullptr;
    TapeTimeline timeline;
    Ruler ruler;
    LaneList list;
    juce::Viewport viewport;
    BpmField bpmField;
    Sparkle sparkle;
    TunerFace tunerFace;
    float bpm = 120.0f;
    bool quantizeOn = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AdvancedDrawer)
};
