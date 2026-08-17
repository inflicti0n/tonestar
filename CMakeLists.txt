cmake_minimum_required(VERSION 3.22)
if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum supported macOS")
endif()
project(ToneStar VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.15
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_gui_app(ToneStar
    PRODUCT_NAME "ToneStar"
    COMPANY_NAME "ToneStar"
    BUNDLE_ID com.tonestar.app
    VERSION 0.1.0
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/assets/icon.png"
    ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/assets/icon.png"
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "ToneStar needs the guitar input."
    HARDENED_RUNTIME_ENABLED FALSE
)

target_sources(ToneStar PRIVATE
    src/App/Main.cpp
    src/App/MainComponent.cpp
    src/App/MainComponent.h
    src/App/GuitarProcessor.cpp
    src/App/GuitarProcessor.h
    src/App/AppLog.cpp
    src/App/AppLog.h
    src/App/AppSettings.h
    src/App/DiscordPresence.cpp
    src/App/DiscordPresence.h
    src/App/DebugLog.h
    src/App/RigMode.h
    src/Amp/ToneCompose.h
    src/Amp/AmpVoice.h
    src/Amp/FxRack.h
    src/Amp/Loudness.h
    src/Cab/Acoustics.h
    src/Cab/CabWidget.cpp
    src/Cab/CabWidget.h
    src/Field/ToneStar.cpp
    src/Field/ToneStar.h
    src/Field/ToneField.cpp
    src/Field/ToneField.h
    src/Presets/PresetStore.h
    src/Presets/PresetDrawer.cpp
    src/Presets/PresetDrawer.h
    src/Presets/ToneSlug.h
    src/Tape/TapeEngine.cpp
    src/Tape/TapeEngine.h
    src/Tape/AdvancedDrawer.cpp
    src/Tape/AdvancedDrawer.h
    src/Tape/TapeTimeline.h
    src/Tape/TakeRecorder.h
    src/Looper/LooperEngine.cpp
    src/Looper/LooperEngine.h
    src/Looper/LooperDrawer.cpp
    src/Looper/LooperDrawer.h
    src/Visuals/StarPlasma.cpp
    src/Visuals/StarPlasma.h
    src/Visuals/FluidSim.cpp
    src/Visuals/FluidSim.h
    src/Visuals/FieldEnergy.h
    src/Visuals/FieldSpectrum.h
    src/Visuals/SpectrumTap.cpp
    src/Visuals/SpectrumTap.h
    src/Visuals/Tuner.h
    src/Visuals/PlasmaLook.h
    src/Visuals/PlasmaTune.cpp
    src/Visuals/PlasmaTune.h
    src/Appearance/TitleBar.h
    src/Appearance/WindowShell.h
    src/Appearance/Theme.cpp
    src/Appearance/Theme.h
    src/Vocals/VocalCompose.h
    src/Vocals/VocalEngine.h
    src/Vocals/VocalFx.h
    src/Vocals/VocalKey.h
    src/Vocals/VocalPitch.h
    src/Vocals/VocalChain.h
)

target_include_directories(ToneStar PRIVATE src)

juce_add_binary_data(ToneStarData SOURCES
    assets/icon.png
    assets/fonts/Gaegu-Regular.ttf
    assets/fonts/Gaegu-Bold.ttf
    assets/fonts/SpaceGrotesk-SemiBold.ttf
    assets/irs/even_v30_sm57.wav
    assets/hrtf/sadie_d2_front_L.wav
    assets/hrtf/sadie_d2_front_R.wav
    assets/hrtf/NOTICE
)

target_compile_definitions(ToneStar PRIVATE
    $<$<BOOL:${WIN32}>:JUCE_ASIO=1>
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:ToneStar,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:ToneStar,JUCE_VERSION>"
    TONESTAR_DISCORD_APP_ID="1538926105107628102"
)

target_link_libraries(ToneStar PRIVATE
    ToneStarData
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)
