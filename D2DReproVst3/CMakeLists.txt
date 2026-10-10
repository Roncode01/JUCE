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
# THIS IS A DIAGNOSTIC BUILD, NOT A FIX CANDIDATE.
# ============================================================================
# Added 2026-10-10. Deliberately does NOT include direct2d-occlusion-didpresent.patch (the
# second, not-yet-field-confirmed patch from the 1.20.0 candidate). That patch changes control
# flow (unconditionally resets the present-throttle on any non-S_OK success code), which would
# confound this build's purpose: finding out which actual HRESULT Present1() returns during
# screensaver, lock-screen, hibernate, sleep and display-off transitions.
#
# Earlier analysis (both the original patch author's own comments and this project's own
# reasoning) assumed the relevant code is DXGI_STATUS_OCCLUDED. A later review turned up that
# Microsoft's own documentation states DXGI_STATUS_OCCLUDED is never returned for a flip-model
# swap chain -- which is what this file's SwapChain actually creates (DXGI_SWAP_EFFECT_FLIP_
# SEQUENTIAL). So the "it's occlusion" premise behind every fix attempt so far may simply be
# wrong, and no further fix design should be trusted until the real HRESULT value(s) are known.
#
# direct2d-present-diagnostic.patch adds a small logging hook (Direct2DPresentDiagnostic::
# onPresent1Result) called right after every real Present1() call, before any existing control
# flow runs. It is purely observational -- it does not alter canPresent()/didPresent() or any
# return value -- so this build is expected to still flicker under the same triggers as before
# (screensaver, lock screen, hibernate). That's expected and not a regression: this build's only
# job is to produce %TEMP%\d2d_present_diagnostic.log with the real HRESULT values logged across
# those triggers. See README.md for what to do with that log.
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        9.0.3
    GIT_SHALLOW    ON
    PATCH_COMMAND  git apply --whitespace=nowarn
                   "${CMAKE_CURRENT_SOURCE_DIR}/patches/direct2d-frame-latency-wait.patch" &&
                   git apply --whitespace=nowarn
                   "${CMAKE_CURRENT_SOURCE_DIR}/patches/direct2d-present-diagnostic.patch")
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(D2DReproVst3
    COMPANY_NAME               "Your Company"
    PLUGIN_MANUFACTURER_CODE   Yrco
    PLUGIN_CODE                D2dg        # distinct from D2DReproVst3's other build (D2dr) and
                                            # the main plugin (Spba). PRODUCT_NAME below is
                                            # unchanged, so this still produces the same-named
                                            # .vst3 bundle and will replace whichever D2DReproVst3
                                            # build is currently installed -- this code just keeps
                                            # REAPER's own plugin-identity tracking (FX chains,
                                            # saved projects referencing this slot) distinct from
                                            # the other build's, the same way swapping 1.19.0 for
                                            # 1.20.0 already works for the main plugin.
    BUNDLE_ID                  com.yourcompany.d2dreprovst3.diagnostic
    PRODUCT_NAME               "D2D Repro VST3"   # deliberately unchanged from the other
                                            # D2DReproVst3 builds -- this is what controls the
                                            # built .vst3 bundle's filename on disk, and your CI
                                            # workflow's Copy-Item step has that filename
                                            # hardcoded. Changing it (as an earlier version of
                                            # this file did) breaks that copy step with "Cannot
                                            # find path". PLUGIN_CODE and BUNDLE_ID above are
                                            # what actually keep this installing side-by-side
                                            # with your other builds, so nothing is lost by
                                            # leaving this alone.
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
