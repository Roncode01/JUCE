cmake_minimum_required(VERSION 3.22)

# Same UI as the standalone D2DRepro app, packaged as a real VST3 so it can be loaded and tested
# directly in REAPER -- tests whether being hosted (vs. a freestanding top-level window) changes
# anything about the Direct2D mixed-refresh-rate behaviour.
project(D2DReproVst3 VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

include(FetchContent)

# Testing a candidate fix: reuk's unmerged "Direct2D: Avoid rendering frames before swapchain
# becomes ready" patch (posted on the JUCE forum 2026-07-17, confirmed as of 2026-10-07 to be
# absent from both the 9.0.3 tag and current develop -- i.e. not yet in any release). It adds
# proper backpressure via the DXGI frame-latency waitable object: swap.canPresent() now blocks a
# new frame from being considered ready until DXGI signals the previous one has actually been
# consumed, instead of racing ahead of it. The thread that originally raised this (laggy mouse
# tracking on slow GPUs under heavy paint load) is a different *symptom* than our flickering, but
# the same underlying mechanism -- presenting without waiting for the prior frame's real DWM
# handoff -- is a plausible cause of exactly the kind of corrupted-frame artifacts we're seeing
# under concurrent drag + background-repaint load on a monitor whose vblank cadence doesn't match
# our assumptions. PATCH_COMMAND applies it to the fetched JUCE source during the populate step,
# verified against a real FetchContent run (not just a bare `git apply`) before use here.
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        9.0.3
    GIT_SHALLOW    ON
    PATCH_COMMAND  git apply --whitespace=nowarn
                   "${CMAKE_CURRENT_SOURCE_DIR}/patches/direct2d-frame-latency-wait.patch")
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(D2DReproVst3
    COMPANY_NAME               "Your Company"
    PLUGIN_MANUFACTURER_CODE   Yrco
    PLUGIN_CODE                D2dr        # distinct from the main plugin (Spba) and the JUCE9
                                            # eval build (Sp9e) -- installs alongside both
    BUNDLE_ID                  com.yourcompany.d2dreprovst3
    PRODUCT_NAME               "D2D Repro VST3"
    FORMATS                    VST3
    VST3_CATEGORIES            Fx Tools
    IS_SYNTH                   FALSE
    NEEDS_MIDI_INPUT           FALSE
    NEEDS_MIDI_OUTPUT          FALSE
    IS_MIDI_EFFECT              FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE)

target_sources(D2DReproVst3 PRIVATE
    Source/PluginProcessor.h
    Source/PluginProcessor.cpp
    Source/PluginEditor.h
    Source/PluginEditor.cpp)

target_compile_definitions(D2DReproVst3 PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(D2DReproVst3
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
