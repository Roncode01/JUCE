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
# THIS IS THE V6 BUILD -- combines the fix now believed to actually work (v1's simple
# occlusion/didPresent fix, already deployed and clean in the real plugin, StereoSplitBandPhase-
# Adjuster 1.20.0) with the separate reentrant-teardown/ghost-window fix from v5, rebuilt from
# scratch on top of v1 instead of on top of v3.
#
# Why v1 instead of v3/v2 here: v3 (direct2d-occlusion-standby-v3.patch) was earlier confirmed
# clean on screensaver/lock-screen, but a later regression test showed it flickering again, with
# or without the teardown patch. Testing v2 (direct2d-occlusion-standby.patch, the never-before-
# built predecessor to v3) alone -- no diagnostic patch, no teardown patch -- flickered too, which
# ruled out the diagnostic and teardown patches as the cause and implicated the standby design
# itself (shared by v2 and v3). Testing v1 (direct2d-occlusion-didpresent.patch) alone -- the
# patch that's actually deployed and clean in the real plugin -- was the next isolation step.
# See the chat history / README for the full reasoning, including an independent analysis
# pinpointing the likely defect in the standby design's recovery path (pollForRecovery() setting
# didWait = true, bypassing the frame-latency wait gate for one frame right at the lock/unlock
# transition -- exactly where the flicker shows up).
#
# This build is the next step: assuming v1 turns out clean here too (confirming the standby
# design, not D2DReproVst3 itself, was the problem), this is what a real release combining both
# fixes would look like.
#
# Chain, in order:
#   1. direct2d-frame-latency-wait.patch    -- the base patch (mixed-refresh-rate fix). Unchanged.
#   2. direct2d-occlusion-didpresent.patch  -- "v1". Byte-for-byte the same patch file already
#      shipped and confirmed clean in the real plugin's v1_20_0/patches/ folder.
#   3. direct2d-reentrant-teardown-fix-v1.patch -- the Stage 2 fix (monitor-unplug/display-
#      reshuffle crash + ghost window), REBUILT from scratch against this v1-occlusion baseline
#      instead of the old v3-occlusion baseline it was originally built on. The underlying code
#      change is byte-for-byte identical in content to the version already reviewed (confirmed by
#      diffing just the added lines against the original, which matched except for one comment
#      sentence that no longer applied) -- this patch FILE is new only because the surrounding
#      line numbers shifted when rebasing off a much smaller occlusion patch (v1 adds ~7 lines to
#      this file; v3 plus the diagnostic patch it depended on added over 150). No logic changed,
#      so it keeps the same "v1" name. It's functionally independent of the occlusion fix either
#      way -- different functions, different classes, no shared state -- this rebuild was purely
#      mechanical.
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        9.0.3
    GIT_SHALLOW    ON
    PATCH_COMMAND  git apply --whitespace=nowarn
                   "${CMAKE_CURRENT_SOURCE_DIR}/patches/direct2d-frame-latency-wait.patch" &&
                   git apply --whitespace=nowarn
                   "${CMAKE_CURRENT_SOURCE_DIR}/patches/direct2d-occlusion-didpresent.patch" &&
                   git apply --whitespace=nowarn
                   "${CMAKE_CURRENT_SOURCE_DIR}/patches/direct2d-reentrant-teardown-fix-v1.patch")
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(D2DReproVst3
    COMPANY_NAME               "Your Company"
    PLUGIN_MANUFACTURER_CODE   Yrco
    PLUGIN_CODE                D2d6        # distinct from every earlier D2DReproVst3 test build
                                            # (D2dg diagnostic-only, D2dw v2-standby-test, D2d1
                                            # v1-occlusion-test, D2dv v3-standby, D2d5 v5
                                            # reentrant-teardown-on-v3, and the main plugin Spba)
                                            # -- for REAPER's own plugin-identity tracking.
                                            # PRODUCT_NAME below is deliberately unchanged (your
                                            # CI's Copy-Item step has that filename hardcoded), so
                                            # this still replaces whichever D2DReproVst3 build is
                                            # currently installed.
    BUNDLE_ID                  com.yourcompany.d2dreprovst3.v6
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
