cmake_minimum_required(VERSION 3.22)

project(MUTAGEN VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
option(MUTAGEN_PLUGIN "Compilar el plugin (necesita WebView)" ON)
option(MUTAGEN_TOOLS "Compilar la herramienta de pruebas offline" OFF)

if(DEFINED JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        9.0.5
        GIT_SHALLOW    ON)
    FetchContent_MakeAvailable(JUCE)
endif()

if(MUTAGEN_PLUGIN)
    set(MUTAGEN_FORMATS VST3)
    if(APPLE)
        list(APPEND MUTAGEN_FORMATS AU)
    endif()

    juce_add_plugin(Mutagen
        COMPANY_NAME            "na1mdiaz"
        COMPANY_WEBSITE         "https://youtube.com/@na1mdiaz"
        PLUGIN_MANUFACTURER_CODE Na1m
        PLUGIN_CODE             Mtgn
        BUNDLE_ID               com.na1mdiaz.mutagen
        PRODUCT_NAME            "MUTAGEN"
        DESCRIPTION             "Gross + destrozo + granos + geneticas"
        IS_SYNTH                FALSE
        NEEDS_MIDI_INPUT        FALSE
        NEEDS_MIDI_OUTPUT       FALSE
        IS_MIDI_EFFECT          FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS TRUE
        COPY_PLUGIN_AFTER_BUILD FALSE
        NEEDS_WEB_BROWSER       TRUE
        NEEDS_WEBVIEW2          TRUE
        VST3_CATEGORIES         Fx Distortion Delay
        AU_MAIN_TYPE            kAudioUnitType_Effect
        FORMATS                 ${MUTAGEN_FORMATS})

    juce_add_binary_data(MutagenAssets SOURCES ui/index.html)

    target_sources(Mutagen PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)

    target_compile_definitions(Mutagen PUBLIC
        JUCE_WEB_BROWSER=1
        JUCE_USE_CURL=0
        JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0)

    target_link_libraries(Mutagen
        PRIVATE
            MutagenAssets
            juce::juce_audio_utils
            juce::juce_gui_extra
            juce::juce_dsp
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)
endif()

if(MUTAGEN_TOOLS)
    juce_add_console_app(MutagenRender PRODUCT_NAME "MutagenRender")
    target_sources(MutagenRender PRIVATE tools/Render.cpp)
    target_compile_definitions(MutagenRender PRIVATE JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0)
    target_link_libraries(MutagenRender PRIVATE juce::juce_audio_formats juce::juce_core
        PUBLIC juce::juce_recommended_config_flags)
endif()
