cmake_minimum_required(VERSION 3.22)
project(BlezzTune VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE: usa una carpeta local si se pasa -DJUCE_DIR=..., si no la descarga sola
if(DEFINED JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(BlezzTune
    COMPANY_NAME "Yiorblezz"
    PLUGIN_MANUFACTURER_CODE Yblz
    PLUGIN_CODE Bztn
    FORMATS VST3
    PRODUCT_NAME "BLEZZ TUNE"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Fx Pitch\ Shift Dynamics
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(BlezzTune PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(BlezzTune PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

if(UNIX AND NOT APPLE)
    # solo para compilar de prueba en Linux sin todas las librerías X11
    target_compile_definitions(BlezzTune PUBLIC
        JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0 JUCE_USE_XRANDR=0 JUCE_USE_XSHM=0 JUCE_USE_XRENDER=0)
endif()

target_link_libraries(BlezzTune
    PRIVATE
        juce::juce_audio_processors
        juce::juce_dsp
        juce::juce_gui_basics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
