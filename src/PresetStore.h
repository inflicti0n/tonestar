#pragma once

#include "AppSettings.h"

#include <vector>

class PresetStore
{
public:
    struct Entry
    {
        juce::String name;
        juce::String slug;
    };

    void load()
    {
        items.clear();
        nextIndex = 1;

        std::unique_ptr<juce::XmlElement> xml(appSettings().getXmlValue("presets"));
        if (xml == nullptr)
            return;

        nextIndex = juce::jmax(1, xml->getIntAttribute("next", 1));
        for (auto* node = xml->getFirstChildElement(); node != nullptr; node = node->getNextElement())
        {
            if (! node->hasTagName("PRESET"))
                continue;

            items.push_back({ node->getStringAttribute("name"),
                              node->getStringAttribute("slug") });
        }
    }

    void save() const
    {
        auto xml = std::make_unique<juce::XmlElement>("PRESETS");
        xml->setAttribute("next", nextIndex);
        for (const auto& item : items)
        {
            auto* node = xml->createNewChildElement("PRESET");
            node->setAttribute("name", item.name);
            node->setAttribute("slug", item.slug);
        }

        appSettings().setValue("presets", xml.get());
        appSettings().saveIfNeeded();
    }

    const Entry& add(const juce::String& slug)
    {
        items.push_back({ "Preset #" + juce::String(nextIndex++), slug });
        save();
        return items.back();
    }

    void setName(int index, const juce::String& name)
    {
        if (! juce::isPositiveAndBelow(index, (int) items.size()))
            return;

        items[(size_t) index].name = name;
        save();
    }

    void remove(int index)
    {
        if (! juce::isPositiveAndBelow(index, (int) items.size()))
            return;

        items.erase(items.begin() + (size_t) index);
        save();
    }

    const std::vector<Entry>& get() const { return items; }
    int size() const { return (int) items.size(); }

    const Entry* at(int index) const
    {
        if (! juce::isPositiveAndBelow(index, (int) items.size()))
            return nullptr;
        return &items[(size_t) index];
    }

private:
    std::vector<Entry> items;
    int nextIndex = 1;
};
