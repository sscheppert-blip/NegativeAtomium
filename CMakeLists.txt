cmake_minimum_required(VERSION 3.28)

project(NegativeAtomium VERSION 0.5.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
endif()

option(NA_BUILD_TESTS "Build the test runner" ON)
option(NA_COPY_PLUGIN "Copy the built plugin into the user's plugin folder" OFF)

# ── Dependencies (fetched into the build tree, never committed) ─────────────
include(FetchContent)

FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        9.0.3
    GIT_SHALLOW    TRUE
    EXCLUDE_FROM_ALL)

FetchContent_Declare(clap_juce_extensions
    GIT_REPOSITORY https://github.com/free-audio/clap-juce-extensions.git
    GIT_TAG        55525c9858d4b25687be7759a5e0f70eccef218e
    EXCLUDE_FROM_ALL)

FetchContent_MakeAvailable(JUCE clap_juce_extensions)

# ── Plugin ──────────────────────────────────────────────────────────────────
juce_add_plugin(NegativeAtomium
    PRODUCT_NAME             "NegativeAtomium"
    COMPANY_NAME             "NegativeAtomium"
    BUNDLE_ID                "com.negativeatomium.synth"
    PLUGIN_MANUFACTURER_CODE Ngat
    PLUGIN_CODE              NgA1
    FORMATS                  VST3 AU Standalone
    IS_SYNTH                 TRUE
    NEEDS_MIDI_INPUT         TRUE
    NEEDS_MIDI_OUTPUT        FALSE
    IS_MIDI_EFFECT           FALSE
    VST3_CATEGORIES          Instrument Synth
    COPY_PLUGIN_AFTER_BUILD  ${NA_COPY_PLUGIN})

set(NA_DSP_SOURCES
    Source/dsp/SynthEngine.cpp)

target_sources(NegativeAtomium PRIVATE
    ${NA_DSP_SOURCES}
    Source/Parameters.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/LookAndFeel.cpp)

target_include_directories(NegativeAtomium PRIVATE Source)

target_compile_definitions(NegativeAtomium PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(NegativeAtomium
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

clap_juce_extensions_plugin(TARGET NegativeAtomium
    CLAP_ID       "com.negativeatomium.synth"
    CLAP_FEATURES instrument synthesizer mono)

# JUCE writes moduleinfo.json into the VST3 after signing, which breaks the ad-hoc
# signature; Apple Silicon hosts refuse to load bundles with invalid signatures.
if(APPLE)
    foreach(bundle NegativeAtomium_VST3 NegativeAtomium_CLAP NegativeAtomium_AU NegativeAtomium_Standalone)
        add_custom_command(TARGET ${bundle} POST_BUILD
            COMMAND codesign --force --deep --sign - "$<TARGET_BUNDLE_DIR:${bundle}>"
            VERBATIM)
    endforeach()
endif()

# ── Tests ───────────────────────────────────────────────────────────────────
if(NA_BUILD_TESTS)
    juce_add_console_app(NegativeAtomiumTests PRODUCT_NAME "NegativeAtomiumTests")

    target_sources(NegativeAtomiumTests PRIVATE
        ${NA_DSP_SOURCES}
        Source/Parameters.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/LookAndFeel.cpp
        tests/SkinExport.cpp
        tests/Tests.cpp)

    target_include_directories(NegativeAtomiumTests PRIVATE Source)

    target_compile_definitions(NegativeAtomiumTests PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_PLUGINHOST_VST3=1
        JucePlugin_Name="NegativeAtomium")

    target_link_libraries(NegativeAtomiumTests
        PRIVATE
            juce::juce_audio_processors
            juce::juce_audio_formats
            juce::juce_dsp
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)
endif()
