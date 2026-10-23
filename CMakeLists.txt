cmake_minimum_required(VERSION 3.22)
project(DrumSlicer VERSION 1.14.0)
set(CMAKE_CXX_STANDARD 17)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.15)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(DrumSlicer
    COMPANY_NAME "Pouria Motabean"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_MANUFACTURER_CODE Pmau
    PLUGIN_CODE Dslc
    FORMATS VST3
    PRODUCT_NAME "ReKit"
    VST3_CATEGORIES Instrument Drum
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(DrumSlicer PRIVATE Source/PluginProcessor.cpp Source/WinRaise.cpp)

# Optional skin images: put PNGs in /Assets (faceplate.png, wood_left.png, ...)
file(GLOB ASSET_FILES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/Assets/*.png)
if(ASSET_FILES)
    juce_add_binary_data(DrumSlicerAssets SOURCES ${ASSET_FILES})
    set_target_properties(DrumSlicerAssets PROPERTIES POSITION_INDEPENDENT_CODE ON)
    target_link_libraries(DrumSlicer PRIVATE DrumSlicerAssets)
    target_compile_definitions(DrumSlicer PRIVATE HAS_ASSETS=1)
else()
    target_compile_definitions(DrumSlicer PRIVATE HAS_ASSETS=0)
endif()

target_compile_definitions(DrumSlicer PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_USE_MP3AUDIOFORMAT=1)

target_link_libraries(DrumSlicer
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_formats
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Command line version of the same engine (no JUCE): peel_cli loop.wav outdir
add_executable(peel_cli tools/peel_cli.cpp)
