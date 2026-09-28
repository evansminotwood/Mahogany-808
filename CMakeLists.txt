cmake_minimum_required(VERSION 3.22)

project(Forge808 VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(FORGE808_BUILD_TESTS "Build the headless render test" ON)

# JUCE is fetched automatically. To use a local checkout instead, configure with
#   -DFETCHCONTENT_SOURCE_DIR_JUCE=/path/to/JUCE
include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.9
    GIT_SHALLOW    ON)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(Forge808
    COMPANY_NAME                "Mahogany"
    PRODUCT_NAME                "Mahogany 808"
    BUNDLE_ID                   "com.mahogany.mahogany808"
    PLUGIN_MANUFACTURER_CODE    Frg8
    PLUGIN_CODE                 F808
    FORMATS                     VST3 Standalone
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Instrument Synth Drum)

# The linker's ad-hoc signature is made before JUCE finishes the bundles (e.g. writes
# moduleinfo.json into the VST3), which leaves the signature broken. Re-sign them afterwards.
if(APPLE)
    foreach(bundle Forge808_VST3 Forge808_Standalone)
        add_custom_command(TARGET ${bundle} POST_BUILD
            COMMAND codesign --force --sign - "$<TARGET_BUNDLE_DIR:${bundle}>")
    endforeach()
endif()

target_sources(Forge808 PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(Forge808 PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Forge808
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

if(FORGE808_BUILD_TESTS)
    juce_add_console_app(Forge808Tests PRODUCT_NAME "Forge808Tests")
    target_sources(Forge808Tests PRIVATE
        tests/RenderTest.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)
    target_include_directories(Forge808Tests PRIVATE Source)
    target_compile_definitions(Forge808Tests PRIVATE
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=1)
    target_link_libraries(Forge808Tests PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_recommended_config_flags)
endif()
