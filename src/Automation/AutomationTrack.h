#pragma once

#include "Automation/AutomationParam.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <vector>

struct AutomationKey
{
    int time = 0;
    VocalStamp value;
    uint32_t pins = 0;
};

class AutomationTrack
{
public:
    static constexpr int maxKeys = 64;
    static constexpr int slots = 4;

    int count() const { return (int) keys.size(); }
    bool empty() const { return keys.empty(); }
    const AutomationKey& get(int index) const { return keys[(size_t) juce::jlimit(0, count() - 1, index)]; }

    bool isEnabled() const { return enabled.load(std::memory_order_acquire) != 0; }
    void setEnabled(bool should)
    {
        enabled.store(should ? 1 : 0, std::memory_order_release);
        publish();
    }

    bool isExpanded() const { return expanded.load(std::memory_order_relaxed) != 0; }
    void setExpanded(bool should) { expanded.store(should ? 1 : 0, std::memory_order_relaxed); }

    bool automated() const
    {
        return isEnabled() && keyCount.load(std::memory_order_acquire) > 0;
    }

    uint32_t shownMask() const { return shown; }
    bool isShownAuto() const { return shownAuto; }

    void setShownMask(uint32_t mask)
    {
        shown = mask;
        shownAuto = false;
    }

    void loadShown(uint32_t mask, bool autoMode)
    {
        shown = mask;
        shownAuto = autoMode;
    }

    uint32_t differingGroups() const
    {
        uint32_t mask = 0;
        for (const auto& key : keys)
        {
            for (int p = 0; p < AutomationParam::count; ++p)
            {
                if ((key.pins & (1u << (uint32_t) p)) == 0)
                    continue;
                mask |= (1u << (uint32_t) AutomationParam::at(p).group);
            }
        }
        return mask;
    }

    uint32_t visibleGroups() const
    {
        return shownAuto ? differingGroups() : shown;
    }

    bool hasPin(int keyIndex, int paramIndex) const
    {
        if (keyIndex < 0 || keyIndex >= (int) keys.size())
            return false;
        if (paramIndex < 0 || paramIndex >= AutomationParam::count)
            return false;
        return (keys[(size_t) keyIndex].pins & (1u << (uint32_t) paramIndex)) != 0;
    }

    int insert(int time, const VocalStamp& stamp)
    {
        time = juce::jmax(0, time);
        const bool first = keys.empty();
        const VocalStamp before = first ? VocalStamp {} : evaluate(time);
        const int existing = indexAt(time);
        if (existing >= 0)
        {
            const uint32_t added = first ? 0u : diffs(before, stamp);
            keys[(size_t) existing].value = stamp;
            keys[(size_t) existing].pins |= added;
            pinFromNodes(existing, added);
            publish();
            return existing;
        }
        if ((int) keys.size() >= maxKeys)
            return -1;
        const uint32_t added = first ? 0u : diffs(before, stamp);
        keys.push_back({ time, stamp, added });
        sortKeys();
        pinFromNodes(indexAt(time), added);
        publish();
        return indexAt(time);
    }

    int insertNode(int time, int paramIndex, float value)
    {
        time = juce::jmax(0, time);
        if (paramIndex < 0 || paramIndex >= AutomationParam::count)
            return -1;
        const auto& spec = AutomationParam::at(paramIndex);
        value = juce::jlimit(spec.min, spec.max, value);
        int existing = indexAt(time);
        if (existing < 0)
        {
            if ((int) keys.size() >= maxKeys)
                return -1;
            VocalStamp stamp = evaluate(time);
            spec.set(stamp, value);
            keys.push_back({ time, stamp, 1u << (uint32_t) paramIndex });
            sortKeys();
            publish();
            return indexAt(time);
        }
        spec.set(keys[(size_t) existing].value, value);
        keys[(size_t) existing].pins |= (1u << (uint32_t) paramIndex);
        publish();
        return existing;
    }

    void remove(int index)
    {
        if (index < 0 || index >= (int) keys.size())
            return;
        keys.erase(keys.begin() + index);
        publish();
    }

    void unpin(int keyIndex, int paramIndex)
    {
        if (keyIndex < 0 || keyIndex >= (int) keys.size())
            return;
        if (paramIndex < 0 || paramIndex >= AutomationParam::count)
            return;
        auto& key = keys[(size_t) keyIndex];
        key.pins &= ~(1u << (uint32_t) paramIndex);
        const auto& spec = AutomationParam::at(paramIndex);
        spec.set(key.value, spec.get(evaluate(key.time)));
        if (key.pins == 0)
            keys.erase(keys.begin() + keyIndex);
        publish();
    }

    void moveTime(int index, int newTime)
    {
        if (index < 0 || index >= (int) keys.size())
            return;
        newTime = juce::jmax(0, newTime);
        const int clash = indexAt(newTime);
        if (clash >= 0 && clash != index)
            keys.erase(keys.begin() + clash);
        if (index > clash && clash >= 0)
            --index;
        if (index < 0 || index >= (int) keys.size())
            return;
        keys[(size_t) index].time = newTime;
        sortKeys();
        publish();
    }

    void setParam(int keyIndex, int paramIndex, float value)
    {
        if (keyIndex < 0 || keyIndex >= (int) keys.size())
            return;
        if (paramIndex < 0 || paramIndex >= AutomationParam::count)
            return;
        const auto& spec = AutomationParam::at(paramIndex);
        spec.set(keys[(size_t) keyIndex].value, juce::jlimit(spec.min, spec.max, value));
        keys[(size_t) keyIndex].pins |= (1u << (uint32_t) paramIndex);
        publish();
    }

    bool writeIfAtKey(int time, const VocalStamp& stamp)
    {
        const int i = indexAt(time);
        if (i < 0)
            return false;
        const uint32_t added = diffs(keys[(size_t) i].value, stamp);
        keys[(size_t) i].value = stamp;
        keys[(size_t) i].pins |= added;
        pinFromNodes(i, added);
        publish();
        return true;
    }

    int indexAt(int time) const
    {
        for (int i = 0; i < (int) keys.size(); ++i)
            if (keys[(size_t) i].time == time)
                return i;
        return -1;
    }

    void clear()
    {
        keys.clear();
        shown = 0;
        shownAuto = true;
        publish();
    }

    void inferPins()
    {
        for (auto& key : keys)
            key.pins = 0;
        if (keys.size() < 2)
        {
            publish();
            return;
        }
        for (int p = 0; p < AutomationParam::count; ++p)
        {
            const auto& spec = AutomationParam::at(p);
            for (size_t i = 1; i < keys.size(); ++i)
            {
                if (std::abs(spec.get(keys[i].value) - spec.get(keys[i - 1].value)) <= 1.0e-4f)
                    continue;
                keys[i].pins |= (1u << (uint32_t) p);
                keys[i - 1].pins |= (1u << (uint32_t) p);
            }
        }
        publish();
    }

    VocalStamp evaluate(int sample) const
    {
        if (keys.empty())
            return {};
        scratchTimes.resize(keys.size());
        scratchValues.resize(keys.size());
        scratchPins.resize(keys.size());
        for (size_t i = 0; i < keys.size(); ++i)
        {
            scratchTimes[i] = keys[i].time;
            scratchValues[i] = keys[i].value;
            scratchPins[i] = keys[i].pins;
        }
        return evaluatePinned((int) keys.size(), scratchTimes.data(), scratchValues.data(),
                              scratchPins.data(), sample);
    }

    VocalStamp evaluateAudio(int sample) const
    {
        if (! automated())
            return {};
        const int slot = live.load(std::memory_order_acquire);
        const auto& snap = ring[(size_t) juce::jlimit(0, slots - 1, slot)];
        return evaluatePinned(snap.count, snap.time, snap.value, snap.pins, sample);
    }

    bool evaluateAudio(int sample, VocalStamp& out) const
    {
        if (! automated())
            return false;
        out = evaluateAudio(sample);
        return true;
    }

    void loadKeys(const std::vector<AutomationKey>& next, bool shouldEnable)
    {
        keys = next;
        if ((int) keys.size() > maxKeys)
            keys.resize((size_t) maxKeys);
        sortKeys();
        enabled.store(shouldEnable ? 1 : 0, std::memory_order_release);
        publish();
    }

    const std::vector<AutomationKey>& all() const { return keys; }

private:
    struct Snap
    {
        int count = 0;
        int time[maxKeys] {};
        VocalStamp value[maxKeys] {};
        uint32_t pins[maxKeys] {};
    };

    static uint32_t diffs(const VocalStamp& before, const VocalStamp& after)
    {
        uint32_t mask = 0;
        for (int p = 0; p < AutomationParam::count; ++p)
            if (paramsDiffer(before, after, p))
                mask |= (1u << (uint32_t) p);
        return mask;
    }

    void pinFromNodes(int keyIndex, uint32_t added)
    {
        if (keyIndex <= 0 || added == 0)
            return;
        for (int p = 0; p < AutomationParam::count; ++p)
        {
            if ((added & (1u << (uint32_t) p)) == 0)
                continue;
            for (int i = keyIndex - 1; i >= 0; --i)
            {
                if ((keys[(size_t) i].pins & (1u << (uint32_t) p)) != 0)
                    break;
                keys[(size_t) i].pins |= (1u << (uint32_t) p);
                break;
            }
        }
    }

    void sortKeys()
    {
        std::sort(keys.begin(), keys.end(), [] (const AutomationKey& a, const AutomationKey& b)
        {
            return a.time < b.time;
        });
    }

    void publish()
    {
        const int next = (live.load(std::memory_order_relaxed) + 1) & (slots - 1);
        auto& snap = ring[(size_t) next];
        snap.count = juce::jmin(maxKeys, (int) keys.size());
        for (int i = 0; i < snap.count; ++i)
        {
            snap.time[i] = keys[(size_t) i].time;
            snap.value[i] = keys[(size_t) i].value;
            snap.pins[i] = keys[(size_t) i].pins;
        }
        live.store(next, std::memory_order_release);
        keyCount.store(snap.count, std::memory_order_release);
    }

    std::vector<AutomationKey> keys;
    mutable std::vector<int> scratchTimes;
    mutable std::vector<VocalStamp> scratchValues;
    mutable std::vector<uint32_t> scratchPins;
    Snap ring[slots] {};
    std::atomic<int> live { 0 };
    std::atomic<int> keyCount { 0 };
    std::atomic<int> enabled { 1 };
    std::atomic<int> expanded { 0 };
    uint32_t shown = 0;
    bool shownAuto = true;
};
