cmake_minimum_required(VERSION 3.22)
project(GuitarMonitor VERSION 0.1.0 LANGUAGES C CXX)

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

juce_add_gui_app(GuitarMonitor
    PRODUCT_NAME "ToneStar"
    COMPANY_NAME "tonestar"
    BUNDLE_ID com.tonestar.app
    VERSION 0.1.0
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/assets/icon.png"
    ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/assets/icon.png"
)

target_sources(GuitarMonitor PRIVATE
    src/Main.cpp
    src/MainComponent.cpp
    src/MainComponent.h
    src/GuitarProcessor.cpp
    src/GuitarProcessor.h
    src/CuteLookAndFeel.cpp
    src/CuteLookAndFeel.h
    src/WindowChrome.h
    src/ToneStar.cpp
    src/ToneStar.h
    src/ToneField.cpp
    src/ToneField.h
    src/CabWidget.cpp
    src/CabWidget.h
    src/PresetStore.h
    src/PresetDrawer.cpp
    src/PresetDrawer.h
    src/TakeRecorder.h
    src/TapeEngine.cpp
    src/TapeEngine.h
    src/AdvancedDrawer.cpp
    src/AdvancedDrawer.h
    src/TapeTimeline.h
    src/LooperEngine.cpp
    src/LooperEngine.h
    src/LooperDrawer.cpp
    src/LooperDrawer.h
    src/ToneSlug.h
    src/FxRack.h
    src/AppSettings.h
    src/AmpVoice.h
    src/ToneCompose.h
    src/Acoustics.h
    src/DebugLog.h
    src/Loudness.h
)

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

target_compile_definitions(GuitarMonitor PRIVATE
    JUCE_ASIO=1
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:GuitarMonitor,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:GuitarMonitor,JUCE_VERSION>"
)

target_link_libraries(GuitarMonitor PRIVATE
    ToneStarData
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)
