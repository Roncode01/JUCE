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
# already confirmed (2026-10-07) to eliminate the flicker in the VBlankAttachment variant of this
# repro. This build goes back to the plain Timer-driven ActivityMeter (matching how the real
# plugin's meter panels actually work).
#
# ============================================================================
# THIS IS THE V3 FIX CANDIDATE (first real fix attempt built on confirmed diagnostic data).
# ============================================================================
# Chain, in order:
#   1. direct2d-frame-latency-wait.patch  -- the base patch (mixed-refresh-rate fix).
#   2. direct2d-present-diagnostic.patch  -- kept in place deliberately (not removed for this
#      round): logs every Present1() result under "Present1 normal" and a parallel
#      DXGI_PRESENT_TEST probe under "Present1 test", to %TEMP%\d2d_present_diagnostic.log. Still
#      useful here to confirm this build behaves as expected -- see README.md for what the log
#      will and won't show once this patch is also active (paint() stops running while occluded,
#      so the log goes quiet during standby itself; that's expected, not a bug).
#   3. direct2d-occlusion-standby-v3.patch -- the actual fix, now built on confirmed data rather
#      than a guess. The 2026-10-11 diagnostic run on this exact build showed the real HRESULT
#      is DXGI_STATUS_OCCLUDED for all three flicker triggers (screensaver, lock screen,
#      hibernate), and that a DXGI_PRESENT_TEST probe tracks real recovery accurately for all
#      three. This patch adds an explicit occluded/standby state to SwapChain: on any non-S_OK
#      success (confirmed to be DXGI_STATUS_OCCLUDED here), it stops presenting for real and
#      polls via DXGI_PRESENT_TEST until recovery, instead of either (a) disabling the
#      present-throttle outright (the first, flawed fix -- risks a lost frame-latency semaphore
#      count) or (b) leaving it latched "unlocked" indefinitely (patch 1 alone -- the original
#      screensaver/lock/hibernate bug). "v3" (vs. the "v2" design reviewed earlier) adds one
#      narrow fix on top of that design, caught on review: the recovery poll itself can return a
#      genuine failure (e.g. DXGI_ERROR_DEVICE_REMOVED), which is not occlusion and should not be
#      treated as "still occluded, keep polling forever" -- v3 exits standby on a real failure so
#      the next real Present1 attempt's own existing FAILED(hr) handling deals with it, rather
#      than hanging. See README.md for the full detail and what's still open after this.
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        9.0.3
    GIT_SHALLOW    ON
    PATCH_COMMAND  git apply --whitespace=nowarn
                   "${CMAKE_CURRENT_SOURCE_DIR}/patches/direct2d-frame-latency-wait.patch" &&
                   git apply --whitespace=nowarn
                   "${CMAKE_CURRENT_SOURCE_DIR}/patches/direct2d-present-diagnostic.patch" &&
                   git apply --whitespace=nowarn
                   "${CMAKE_CURRENT_SOURCE_DIR}/patches/direct2d-occlusion-standby-v3.patch")
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(D2DReproVst3
    COMPANY_NAME               "Your Company"
    PLUGIN_MANUFACTURER_CODE   Yrco
    PLUGIN_CODE                D2dv        # distinct from the diagnostic-only build (D2dg), the
                                            # other D2DReproVst3 build (D2dr), and the main plugin
                                            # (Spba) -- for REAPER's own plugin-identity tracking.
                                            # PRODUCT_NAME below is deliberately unchanged (same
                                            # reasoning as the diagnostic build -- your CI's
                                            # Copy-Item step has that filename hardcoded), so this
                                            # still replaces whichever D2DReproVst3 build is
                                            # currently installed.
    BUNDLE_ID                  com.yourcompany.d2dreprovst3.v3standby
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
