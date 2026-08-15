#pragma once

#include <juce_core/juce_core.h>

namespace AppLog
{
    juce::File directory();
    juce::File startupFile();
    juce::File crashFile();
    juce::File selfTestFile();

    void install();
    void note(const juce::String& step);
    void markReady();
    bool previousRunFinished();

    bool runSeh(void (*fn)(void*), void* ctx, unsigned int* exceptionCode);
}
