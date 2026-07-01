cmake_minimum_required(VERSION 3.22)
project(Ironclad VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(Ironclad
    COMPANY_NAME "CortiCorp"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    PLUGIN_MANUFACTURER_CODE Cort
    PLUGIN_CODE Irnc
    FORMATS VST3 Standalone
    PRODUCT_NAME "Ironclad"
    COPY_PLUGIN_AFTER_BUILD FALSE
)

juce_generate_juce_header(Ironclad)

target_sources(Ironclad
    PRIVATE
        src/PluginProcessor.cpp
        src/PluginEditor.cpp
)

target_include_directories(Ironclad
    PRIVATE
        src
)

target_compile_definitions(Ironclad
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(Ironclad
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
