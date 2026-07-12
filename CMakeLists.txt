cmake_minimum_required(VERSION 3.22)
project(Ironclad VERSION 0.4.7)

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

# UI artwork compiled straight into the binary as BinaryData, so the plugin
# never touches the filesystem to find its skin. This is what makes it work
# from Program Files\Common Files\VST3 (and any other host) and shippable.
set(ICLAD_UI images/Ironclad_UI_Implementation_Package)
juce_add_binary_data(IroncladAssets
    SOURCES
        images/guitar_types_2_cut.png
        ${ICLAD_UI}/Knobs/output_knob_core_2x.png
        ${ICLAD_UI}/Knobs/drive_knob_core_2x.png
        ${ICLAD_UI}/Screen/bezel_frame_2x.png
        ${ICLAD_UI}/Screen/display_panel_full_2x.png
        ${ICLAD_UI}/Textures/Black_Metal/bottom_trim_2x.png
        ${ICLAD_UI}/Textures/Black_Metal/outer_frame_top_2x.png
        ${ICLAD_UI}/Textures/Chrome/chrome_center_bezel_2x.png
        ${ICLAD_UI}/Textures/Chrome/chrome_left_bevel_2x.png
        ${ICLAD_UI}/Textures/Lighting/bottom_red_glow_2x.png
        ${ICLAD_UI}/Textures/Lighting/left_vertical_led_2x.png
        ${ICLAD_UI}/Textures/Lighting/right_vertical_led_2x.png
        ${ICLAD_UI}/Textures/Lighting/top_accent_slashes_2x.png
        ${ICLAD_UI}/Textures/Mesh/top_hex_grille_2x.png
        ${ICLAD_UI}/Textures/Red_Brushed_Aluminum/red_brushed_tile_512.png
        ${ICLAD_UI}/Textures/Rubber/left_armor_grip_2x.png
        ${ICLAD_UI}/Textures/Rubber/right_armor_grip_2x.png
)

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
        IroncladAssets
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
