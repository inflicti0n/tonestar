#pragma once

#include <juce_data_structures/juce_data_structures.h>

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

    return file;
}
