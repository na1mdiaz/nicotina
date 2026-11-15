cmake_minimum_required(VERSION 3.22)

project(NICOTINA VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")

# ------------------------------------------------------------------------------
# JUCE: usa una copia local si pasas -DJUCE_DIR=/ruta/a/JUCE,
# si no, se descarga sola desde GitHub.
# ------------------------------------------------------------------------------
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

# ------------------------------------------------------------------------------
# Plugin
# ------------------------------------------------------------------------------
set(NICOTINA_FORMATS VST3)
if(APPLE)
    list(APPEND NICOTINA_FORMATS AU)
endif()

juce_add_plugin(Nicotina
    COMPANY_NAME            "na1mdiaz"
    COMPANY_WEBSITE         "https://youtube.com/@na1mdiaz"
    PLUGIN_MANUFACTURER_CODE Na1m
    PLUGIN_CODE             Nctn
    BUNDLE_ID               com.na1mdiaz.nicotina
    PRODUCT_NAME            "NICOTINA"
    DESCRIPTION             "8-bit crush system"
    IS_SYNTH                FALSE
    NEEDS_MIDI_INPUT        FALSE
    NEEDS_MIDI_OUTPUT       FALSE
    IS_MIDI_EFFECT          FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES         Fx Distortion
    AU_MAIN_TYPE            kAudioUnitType_Effect
    FORMATS                 ${NICOTINA_FORMATS})

juce_add_binary_data(NicotinaAssets SOURCES
    assets/fonts/ps2p.ttf
    assets/fonts/silk-b.ttf
    assets/fonts/silk-r.ttf
    assets/fonts/dotg.ttf
    assets/fonts/yuji.ttf
    assets/fonts/inter900.ttf
    assets/fonts/inter800.ttf)

set(NICOTINA_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Scene.cpp
    Source/Gfx.cpp)

target_sources(Nicotina PRIVATE ${NICOTINA_SOURCES})

target_compile_definitions(Nicotina PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_ALSA=0
    JUCE_JACK=0)

target_link_libraries(Nicotina
    PRIVATE
        NicotinaAssets
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ------------------------------------------------------------------------------
# Herramienta opcional: renderiza la interfaz a PNG (solo para desarrollo)
#   cmake -DNICOTINA_PREVIEW=ON ...
# ------------------------------------------------------------------------------
option(NICOTINA_PREVIEW "Build the UI preview renderer" OFF)
if(NICOTINA_PREVIEW)
    juce_add_console_app(NicotinaPreview PRODUCT_NAME "NicotinaPreview")
    target_sources(NicotinaPreview PRIVATE tools/Preview.cpp ${NICOTINA_SOURCES})
    target_include_directories(NicotinaPreview PRIVATE Source)
    target_compile_definitions(NicotinaPreview PRIVATE
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_ALSA=0 JUCE_JACK=0
        NICOTINA_PREVIEW_BUILD=1
        JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0 JUCE_USE_XRENDER=0 JUCE_USE_XSHM=0 JUCE_USE_XINPUT=0
        JucePlugin_Name="NICOTINA")
    target_link_libraries(NicotinaPreview PRIVATE
        NicotinaAssets juce::juce_audio_utils juce::juce_dsp
        juce::juce_recommended_config_flags)
endif()
