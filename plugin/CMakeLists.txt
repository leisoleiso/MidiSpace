cmake_minimum_required(VERSION 3.22)
project(MidiSpace VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# JUCE framework (cloned alongside this project).
# JUCE 9 bundles a complete VST3 SDK (juce_audio_processors_headless/format_types/VST3_SDK),
# so no external SDK path is required for the VST3 format.
add_subdirectory(../JUCE JUCE)

juce_add_plugin(MidiSpacePlugin
    PRODUCT_NAME            "MidiSpace"
    COMPANY_NAME            "MidiSpace"
    VERSION                 "0.1.0"
    FORMATS                 Standalone VST3
    IS_SYNTH                TRUE
    NEEDS_MIDI_INPUT        TRUE
    NEEDS_MIDI_OUTPUT       TRUE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Juce
    PLUGIN_CODE             MidS)

target_sources(MidiSpacePlugin PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp
    src/CanvasComponent.cpp
    src/HttpModelInterface.cpp
    src/GenerationManager.cpp)

target_include_directories(MidiSpacePlugin PRIVATE src)

# Standalone format needs the audio devices + utils modules for its host wrapper.
target_link_libraries(MidiSpacePlugin PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_devices)

# Fresh plugin with no legacy VST2 to stay compatible with: skip the VST2/VST3
# parameter-ID migration check.
target_compile_definitions(MidiSpacePlugin PUBLIC JUCE_VST3_CAN_REPLACE_VST2=0)
