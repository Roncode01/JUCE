cmake_minimum_required(VERSION 3.22)

# Same UI as the standalone D2DRepro app, packaged as a real VST3 so it can be loaded and tested
# directly in REAPER -- tests whether being hosted (vs. a freestanding top-level window) changes
# anything about the Direct2D mixed-refresh-rate behaviour.
project(D2DReproVst3 VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

include(FetchContent)

# reuk's unmerged "Direct2D: Avoid rendering frames before swapchain becomes ready" patch --
# already confirmed to eliminate the flicker in the VBlankAttachment variant of this repro.
#
# ============================================================================
# THIS IS THE "v1" TEST BUILD -- direct2d-occlusion-didpresent.patch, the fix that's actually
# deployed in the real plugin (StereoSplitBandPhaseAdjuster 1.20.0) and does NOT flicker there on
# screensaver/lock-screen. It has never been built against D2DReproVst3 before now.
#
# Why this matters: both later revisions of this fix -- "v2" (direct2d-occlusion-standby.patch)
# and "v3" (direct2d-occlusion-standby-v3.patch) -- have now been built against D2DReproVst3 and
# BOTH flickered on screensaver/lock-screen, even though v3 was earlier confirmed clean and v2
# had never been tested before. v2 and v3 share the same "occluded flag + pollForRecovery()
# standby loop" design; v1 does not share any of that -- it keeps presenting every frame exactly
# as before, and only changes one thing: didPresent() (which just resets the present-throttle
# flag) is now called on ANY success code Present1 returns, not only a literal S_OK. No
# standby, no DXGI_PRESENT_TEST polling, no occluded state at all.
#
# This build isolates whether v1's much simpler fix, which is already proven not to flicker in
# the real plugin, also doesn't flicker in the D2DReproVst3 harness -- or whether D2DReproVst3
# itself behaves differently from the real plugin regardless of which patch is applied.
#
# Chain, in order:
#   1. direct2d-frame-latency-wait.patch     -- the base patch (mixed-refresh-rate fix). Unchanged.
#   2. direct2d-occlusion-didpresent.patch   -- "v1". Byte-for-byte the same patch file already
#      shipped in the real plugin build (v1_20_0/patches/), just applied here against
#      D2DReproVst3 for the first time.
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        9.0.3
    GIT_SHALLOW    ON
    PATCH_COMMAND  git apply --whitespace=nowarn
                   "${CMAKE_CURRENT_SOURCE_DIR}/patches/direct2d-frame-latency-wait.patch" &&
                   git apply --whitespace=nowarn
                   "${CMAKE_CURRENT_SOURCE_DIR}/patches/direct2d-occlusion-didpresent.patch")
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(D2DReproVst3
    COMPANY_NAME               "Your Company"
    PLUGIN_MANUFACTURER_CODE   Yrco
    PLUGIN_CODE                D2d1        # distinct from the diagnostic-only build (D2dg), the
                                            # v2-standby-test build (D2dw), the v3-standby build
                                            # (D2dv), the v5 reentrant-teardown build (D2d5), the
                                            # v5-diagnostic build (D2d6), and the main plugin
                                            # (Spba) -- for REAPER's own plugin-identity
                                            # tracking. PRODUCT_NAME below is deliberately
                                            # unchanged (your CI's Copy-Item step has that
                                            # filename hardcoded), so this still replaces
                                            # whichever D2DReproVst3 build is currently installed.
    BUNDLE_ID                  com.yourcompany.d2dreprovst3.v1didpresenttest
    PRODUCT_NAME               "D2D Repro VST3"
    FORMATS                    VST3
    VST3_CATEGORIES            Fx Tools
    IS_SYNTH                   FALSE
    NEEDS_MIDI_INPUT           FALSE
    NEEDS_MIDI_OUTPUT          FALSE
    IS_MIDI_EFFECT              FALSE
    COPY_PLUGIN_AFTER_BUILD    FALSE)

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
