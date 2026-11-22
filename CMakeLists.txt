cmake_minimum_required(VERSION 3.22)
project(PEPELUS VERSION 1.0.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
option(PEPELUS_PLUGINS "Compilar los plugins (necesita WebView)" ON)
option(PEPELUS_TOOLS "Compilar la herramienta de pruebas offline" OFF)

if(DEFINED JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 9.0.5 GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)
endif()

function(pepelus_plugin TARGET NAME CODE DIR CATS)
    set(FMTS VST3)
    if(APPLE)
        list(APPEND FMTS AU)
    endif()
    juce_add_plugin(${TARGET}
        COMPANY_NAME "na1mdiaz" COMPANY_WEBSITE "https://youtube.com/@na1mdiaz"
        PLUGIN_MANUFACTURER_CODE Na1m PLUGIN_CODE ${CODE}
        BUNDLE_ID com.na1mdiaz.${DIR}
        PRODUCT_NAME "${NAME}" DESCRIPTION "Pepelus Sativa Operations"
        IS_SYNTH FALSE NEEDS_MIDI_INPUT FALSE NEEDS_MIDI_OUTPUT FALSE IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE COPY_PLUGIN_AFTER_BUILD FALSE
        NEEDS_WEB_BROWSER TRUE NEEDS_WEBVIEW2 TRUE
        VST3_CATEGORIES ${CATS} AU_MAIN_TYPE kAudioUnitType_Effect FORMATS ${FMTS})
    juce_add_binary_data(${TARGET}UI HEADER_NAME ${TARGET}UI.h NAMESPACE ${TARGET}UI SOURCES ui/${DIR}/index.html)
    target_sources(${TARGET} PRIVATE Source/common/PluginBase.cpp Source/${DIR}/Main.cpp)
    target_compile_definitions(${TARGET} PUBLIC JUCE_WEB_BROWSER=1 JUCE_USE_CURL=0 JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
    target_link_libraries(${TARGET} PRIVATE ${TARGET}UI juce::juce_audio_utils juce::juce_gui_extra
        PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)
endfunction()

if(PEPELUS_PLUGINS)
    pepelus_plugin(Satura "PEPELUS SATURA" PsSa satura "Fx Distortion")
    pepelus_plugin(Slowed "PEPELUS SLOWED" PsSl slowed "Fx Reverb")
    pepelus_plugin(Ninja  "PEPELUS NINJA"  PsNj ninja  "Fx Delay")
endif()

if(PEPELUS_TOOLS)
    add_executable(PepelusRender tools/Render.cpp)
    target_include_directories(PepelusRender PRIVATE Source)
endif()
