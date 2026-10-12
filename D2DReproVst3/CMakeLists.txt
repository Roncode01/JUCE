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
# THIS IS THE "v2" OCCLUSION-STANDBY TEST BUILD -- deliberately minimal, to isolate one specific
# question from a regression report: v3 (direct2d-occlusion-standby-v3.patch, in the other
# D2DReproVst3 builds) showed the screensaver/lock-screen flicker returning, with or without the
# separate reentrant-teardown-fix-v1 patch. This build removes everything except the two patches
# below -- no diagnostic logging patch, no teardown/reentrancy-guard patch -- to test the
# occlusion-standby design itself ("v2": direct2d-occlusion-standby.patch, the analysis-only
# revision that was never previously built/tested) in isolation, as close as possible to v1
# (direct2d-occlusion-didpresent.patch), which IS already deployed in the real plugin
# (StereoSplitBandPhaseAdjuster 1.20.0) and does not flicker on lock/screensaver there.
#
# v2 vs v3: functionally identical for a plain screensaver/lock-screen occlusion (both gate real
# presents on a literal hr == S_OK and poll via DXGI_PRESENT_TEST while occluded). v3 only adds
# handling for a genuine FAILED(hr) seen *during* that poll (clearing the occluded flag so the
# next real present's own failure path runs, instead of polling forever against a code that can
# never return S_OK) -- a case a plain lock/unlock shouldn't hit. So this build should behave the
# same as v3 for this specific scenario; if it doesn't flicker here, that points at something in
# the diagnostic or teardown patches (or their interaction) rather than the occlusion-standby
# design itself; if it still flickers, the design itself (shared by v2 and v3) is implicated,
# independent of anything either of those other patches do.
#
# Chain, in order:
#   1. direct2d-frame-latency-wait.patch -- the base patch (mixed-refresh-rate fix). Unchanged.
#   2. direct2d-occlusion-standby.patch  -- "v2". NOT the same file as
#      direct2d-occlusion-standby-v3.patch used elsewhere -- see the comment above for the one
#      functional difference. Never previously built into any D2DReproVst3 before this.
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        9.0.3
    GIT_SHALLOW    ON
    PATCH_COMMAND  git apply --whitespace=nowarn
                   "${CMAKE_CURRENT_SOURCE_DIR}/patches/direct2d-frame-latency-wait.patch" &&
                   git apply --whitespace=nowarn
                   "${CMAKE_CURRENT_SOURCE_DIR}/patches/direct2d-occlusion-standby.patch")
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(D2DReproVst3
    COMPANY_NAME               "Your Company"
    PLUGIN_MANUFACTURER_CODE   Yrco
    PLUGIN_CODE                D2dw        # distinct from the diagnostic-only build (D2dg), the
                                            # v3-standby build (D2dv), the v5 reentrant-teardown
                                            # build (D2d5), the v5-diagnostic build (D2d6), and
                                            # the main plugin (Spba) -- for REAPER's own
                                            # plugin-identity tracking. PRODUCT_NAME below is
                                            # deliberately unchanged (your CI's Copy-Item step has
                                            # that filename hardcoded), so this still replaces
                                            # whichever D2DReproVst3 build is currently installed.
    BUNDLE_ID                  com.yourcompany.d2dreprovst3.v2standbytest
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
