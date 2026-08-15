#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

struct PlasmaLook
{
    float timeScale = 1.10f;
    float viscosity = 0.06f;
    float confinement = 0.85f;
    float swirl = 0.55f;
    float inject = 0.50f;
    float dyeFade = 0.08f;
    float idleAlpha = 0.70f;
    float stormBoost = 1.00f;

    juce::Colour deep { 0xff6140c7 };
    juce::Colour body { 0xffb894fa };
    juce::Colour core { 0xfffff5e0 };
    juce::Colour hot { 0xfffffcf5 };
    juce::Colour accent { 0xfff266c0 };

    float ringAmount = 0.95f;
    float ringRotate = 0.16f;
    float ringGain = 1.85f;
    float ringSmooth = 0.16f;
    float ringThick = 2.20f;
    float ringGlow = 0.55f;
    float ringIdle = 0.28f;
    float ringFill = 0.20f;
    juce::Colour ringLine { 0xfff6efe2 };
    juce::Colour ringAura { 0x9968c4ff };
    juce::Colour ringHot { 0xfff48ca3 };

    juce::String dump() const
    {
        auto hex = [] (juce::Colour c) { return c.toDisplayString(true); };
        return "timeScale " + juce::String(timeScale, 3)
             + "\nviscosity " + juce::String(viscosity, 3)
             + "\nconfinement " + juce::String(confinement, 3)
             + "\nswirl " + juce::String(swirl, 3)
             + "\ninject " + juce::String(inject, 3)
             + "\ndyeFade " + juce::String(dyeFade, 3)
             + "\nidleAlpha " + juce::String(idleAlpha, 3)
             + "\nstormBoost " + juce::String(stormBoost, 3)
             + "\ndeep " + hex(deep)
             + "\nbody " + hex(body)
             + "\ncore " + hex(core)
             + "\nhot " + hex(hot)
             + "\naccent " + hex(accent)
             + "\nringAmount " + juce::String(ringAmount, 3)
             + "\nringRotate " + juce::String(ringRotate, 3)
             + "\nringGain " + juce::String(ringGain, 3)
             + "\nringSmooth " + juce::String(ringSmooth, 3)
             + "\nringThick " + juce::String(ringThick, 3)
             + "\nringGlow " + juce::String(ringGlow, 3)
             + "\nringIdle " + juce::String(ringIdle, 3)
             + "\nringFill " + juce::String(ringFill, 3)
             + "\nringLine " + hex(ringLine)
             + "\nringAura " + hex(ringAura)
             + "\nringHot " + hex(ringHot);
    }

    static juce::File saveFile()
    {
        auto hunt = [] (juce::File dir) -> juce::File
        {
            for (int i = 0; i < 8 && dir.exists(); ++i)
            {
                if (dir.getChildFile("CMakeLists.txt").existsAsFile()
                    && dir.getChildFile("src/StarPlasma.cpp").existsAsFile())
                    return dir.getChildFile("plasma_look.txt");
                dir = dir.getParentDirectory();
            }
            return {};
        };

        if (auto found = hunt(juce::File::getCurrentWorkingDirectory()); found != juce::File())
            return found;
        if (auto found = hunt(juce::File::getSpecialLocation(juce::File::currentExecutableFile)
                                  .getParentDirectory()); found != juce::File())
            return found;
        return juce::File::getSpecialLocation(juce::File::currentExecutableFile)
            .getSiblingFile("plasma_look.txt");
    }

    bool saveToFile() const
    {
        const auto file = saveFile();
        file.getParentDirectory().createDirectory();
        return file.replaceWithText("# ToneStar plasma look\n# " + juce::Time::getCurrentTime().toString(true, true)
                                    + "\n" + dump() + "\n");
    }

    bool loadFromFile()
    {
        const auto file = saveFile();
        if (! file.existsAsFile())
            return false;

        for (auto line : juce::StringArray::fromLines(file.loadFileAsString()))
        {
            line = line.trim();
            if (line.isEmpty() || line.startsWithChar('#'))
                continue;

            const auto key = line.upToFirstOccurrenceOf(" ", false, false);
            const auto val = line.fromFirstOccurrenceOf(" ", false, false).trim();
            auto setF = [&] (const char* name, float& dst)
            {
                if (key == name)
                    dst = val.getFloatValue();
            };
            auto setC = [&] (const char* name, juce::Colour& dst)
            {
                if (key == name)
                    dst = juce::Colour::fromString(val);
            };

            setF("timeScale", timeScale);
            setF("viscosity", viscosity);
            setF("confinement", confinement);
            setF("swirl", swirl);
            setF("inject", inject);
            setF("dyeFade", dyeFade);
            setF("idleAlpha", idleAlpha);
            setF("stormBoost", stormBoost);
            setC("deep", deep);
            setC("body", body);
            setC("core", core);
            setC("hot", hot);
            setC("accent", accent);
            setF("ringAmount", ringAmount);
            setF("ringRotate", ringRotate);
            setF("ringGain", ringGain);
            setF("ringSmooth", ringSmooth);
            setF("ringThick", ringThick);
            setF("ringGlow", ringGlow);
            setF("ringIdle", ringIdle);
            setF("ringFill", ringFill);
            setC("ringLine", ringLine);
            setC("ringAura", ringAura);
            setC("ringHot", ringHot);
        }
        return true;
    }
};

struct PlasmaSliderSpec
{
    const char* name;
    float PlasmaLook::* field;
    double min;
    double max;
    double interval;
};

inline const PlasmaSliderSpec* plasmaSliderSpecs(int& countOut)
{
    static constexpr PlasmaSliderSpec specs[] = {
        { "time",        &PlasmaLook::timeScale,    0.05, 4.0,  0.01 },
        { "viscosity",   &PlasmaLook::viscosity,    0.0,  0.50, 0.01 },
        { "confinement", &PlasmaLook::confinement,  0.0,  2.0,  0.01 },
        { "swirl",       &PlasmaLook::swirl,        0.0,  2.0,  0.01 },
        { "inject",      &PlasmaLook::inject,       0.0,  2.0,  0.01 },
        { "dye fade",    &PlasmaLook::dyeFade,      0.0,  0.80, 0.01 },
        { "idle alpha",  &PlasmaLook::idleAlpha,    0.05, 1.0,  0.01 },
        { "storm",       &PlasmaLook::stormBoost,   0.0,  2.5,  0.01 },
    };
    countOut = (int) (sizeof(specs) / sizeof(specs[0]));
    return specs;
}

inline const PlasmaSliderSpec* ringSliderSpecs(int& countOut)
{
    static constexpr PlasmaSliderSpec specs[] = {
        { "amount",     &PlasmaLook::ringAmount, 0.0,  2.0,  0.01 },
        { "rotate",     &PlasmaLook::ringRotate, 0.0,  1.2,  0.01 },
        { "gain",       &PlasmaLook::ringGain,   0.2,  3.0,  0.01 },
        { "smooth",     &PlasmaLook::ringSmooth, 0.05, 0.95, 0.01 },
        { "thickness",  &PlasmaLook::ringThick,  0.6,  6.0,  0.05 },
        { "glow",       &PlasmaLook::ringGlow,   0.0,  1.5,  0.01 },
        { "idle wobble",&PlasmaLook::ringIdle,   0.0,  1.0,  0.01 },
        { "fill",       &PlasmaLook::ringFill,   0.0,  1.0,  0.01 },
    };
    countOut = (int) (sizeof(specs) / sizeof(specs[0]));
    return specs;
}
