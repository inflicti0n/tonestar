#pragma once

#include "Automation/AutomationTrack.h"
#include "Presets/ToneSlug.h"

#include <cstdint>

class AutomationBank
{
public:
    AutomationTrack& track(int lane)
    {
        return tracks[(size_t) juce::jlimit(0, TapeEngineNumLanes() - 1, lane)];
    }

    const AutomationTrack& track(int lane) const
    {
        return tracks[(size_t) juce::jlimit(0, TapeEngineNumLanes() - 1, lane)];
    }

    bool automated(int lane) const
    {
        return valid(lane) && tracks[(size_t) lane].automated();
    }

    bool evaluate(int lane, int sample, VocalStamp& out) const
    {
        if (! valid(lane))
            return false;
        return tracks[(size_t) lane].evaluateAudio(sample, out);
    }

    VocalStamp evaluateUi(int lane, int sample) const
    {
        if (! valid(lane) || ! tracks[(size_t) lane].automated())
            return {};
        return tracks[(size_t) lane].evaluate(sample);
    }

    void clear(int lane)
    {
        if (valid(lane))
            tracks[(size_t) lane].clear();
    }

    void clearAll()
    {
        for (auto& t : tracks)
            t.clear();
    }

    void saveTo(juce::XmlElement& trackNode, int lane) const
    {
        if (! valid(lane))
            return;
        const auto& t = tracks[(size_t) lane];
        if (t.empty())
            return;

        auto* node = trackNode.createNewChildElement("AUTOMATION");
        node->setAttribute("enabled", t.isEnabled() ? 1 : 0);
        node->setAttribute("expanded", t.isExpanded() ? 1 : 0);
        node->setAttribute("shown", (int) t.shownMask());
        node->setAttribute("shownAuto", t.isShownAuto() ? 1 : 0);
        for (const auto& key : t.all())
        {
            auto* child = node->createNewChildElement("KEY");
            child->setAttribute("time", key.time);
            child->setAttribute("slug", ToneSlug::encodeVocal(key.value));
            child->setAttribute("pins", (int) key.pins);
        }
    }

    void loadFrom(const juce::XmlElement& trackNode, int lane)
    {
        if (! valid(lane))
            return;
        auto& t = tracks[(size_t) lane];
        t.clear();
        t.setExpanded(false);
        t.loadShown(0, true);
        t.setEnabled(true);

        auto* node = trackNode.getChildByName("AUTOMATION");
        if (node == nullptr)
            return;

        std::vector<AutomationKey> keys;
        bool anyPins = false;
        for (auto* child = node->getFirstChildElement(); child != nullptr; child = child->getNextElement())
        {
            if (! child->hasTagName("KEY"))
                continue;
            VocalStamp stamp;
            if (! ToneSlug::decodeVocal(child->getStringAttribute("slug").trim(), stamp))
                continue;
            AutomationKey key;
            key.time = juce::jmax(0, child->getIntAttribute("time"));
            key.value = stamp;
            if (child->hasAttribute("pins"))
            {
                anyPins = true;
                key.pins = (uint32_t) juce::jmax(0, child->getIntAttribute("pins"));
            }
            keys.push_back(key);
        }
        t.loadKeys(keys, node->getBoolAttribute("enabled", true));
        if (! anyPins)
            t.inferPins();
        t.setExpanded(node->getBoolAttribute("expanded"));
        t.loadShown((uint32_t) juce::jmax(0, node->getIntAttribute("shown")),
                    node->getBoolAttribute("shownAuto", true));
    }

private:
    static constexpr int TapeEngineNumLanes() { return 8; }

    static bool valid(int lane) { return lane >= 0 && lane < TapeEngineNumLanes(); }

    AutomationTrack tracks[8];
};
