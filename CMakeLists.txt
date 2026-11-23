cmake_minimum_required(VERSION 3.22)
project(MUTAGEN_SEMILLA VERSION 6.0.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        9.0.5
    GIT_SHALLOW    ON)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(Semilla
    COMPANY_NAME            "na1mdiaz"
    COMPANY_WEBSITE         "https://youtube.com/@na1mdiaz"
    PLUGIN_MANUFACTURER_CODE Na1m
    PLUGIN_CODE             MtgS
    BUNDLE_ID               com.na1mdiaz.mutagen.semilla
    PRODUCT_NAME            "MUTAGEN SEMILLA"
    DESCRIPTION             "Tus acordes con sonido nuevo y melodia inventada"
    IS_SYNTH                TRUE
    NEEDS_MIDI_INPUT        TRUE
    NEEDS_MIDI_OUTPUT       FALSE
    IS_MIDI_EFFECT          FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    NEEDS_WEB_BROWSER       TRUE
    NEEDS_WEBVIEW2          TRUE
    VST3_CATEGORIES         Instrument Synth
    FORMATS                 VST3)

juce_add_binary_data(SemillaAssets SOURCES ui/index.html)
target_sources(Semilla PRIVATE Source/PluginProcessor.cpp Source/PluginEditor.cpp)
target_include_directories(Semilla PRIVATE Source)
target_compile_definitions(Semilla PUBLIC
    JUCE_WEB_BROWSER=1
    JUCE_USE_CURL=0
    JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)
target_link_libraries(Semilla
    PRIVATE SemillaAssets juce::juce_audio_utils juce::juce_gui_extra
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)
