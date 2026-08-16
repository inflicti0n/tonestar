#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include <memory>

inline void migrateSettingsFromConstellation(juce::PropertiesFile& dest)
{
    if (dest.getBoolValue("migratedFromConstellation", false))
        return;

    juce::PropertiesFile::Options oldOpts;
    oldOpts.applicationName = "Constellation";
    oldOpts.filenameSuffix = "settings";
    oldOpts.folderName = "Constellation";
    oldOpts.osxLibrarySubFolder = "Application Support";
    oldOpts.commonToAllUsers = false;
    juce::PropertiesFile old(oldOpts);
    if (! old.getFile().existsAsFile())
    {
        dest.setValue("migratedFromConstellation", true);
        dest.saveIfNeeded();
        return;
    }

    static constexpr const char* keys[] = {
        "axis0", "axis1", "axis2", "axis3", "axis4", "axis5",
        "fx0", "fx1", "fx2", "fx3", "fx4", "fx5", "fx6", "fx7",
        "bloomShimmer", "cabSize", "cabBack", "binaural",
        "inputGain", "outputGain", "mute", "inputChannel",
        "metroOn", "metroBpm", "tapeQuantize",
        "looperOpen", "looperAdvanced", "looperQuantize",
        "looperLevel0", "looperLevel1", "presetsOpen", "advancedOpen"
    };

    for (auto* key : keys)
        dest.setValue(key, old.getValue(key, dest.getValue(key)));

    if (auto xml = std::unique_ptr<juce::XmlElement>(old.getXmlValue("presets")))
        dest.setValue("presets", xml.get());

    dest.setValue("migratedFromConstellation", true);
    dest.saveIfNeeded();
}

inline juce::PropertiesFile& appSettings()
{
    static juce::PropertiesFile file([]
    {
        juce::PropertiesFile::Options options;
        options.applicationName = "ToneStar";
        options.filenameSuffix = "settings";
        options.folderName = "ToneStar";
        options.osxLibrarySubFolder = "Application Support";
        options.commonToAllUsers = false;
        return options;
    }());
    static const bool migrated = (migrateSettingsFromConstellation(file), true);
    juce::ignoreUnused(migrated);

    return file;
}
