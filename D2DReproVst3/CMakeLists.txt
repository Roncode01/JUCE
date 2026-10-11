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
# THIS IS THE V5 BUILD -- adds a fix for the monitor-unplug/display-reshuffle crash and the
# "ghost window" that could outlive even the host process, on top of everything v3 already
# fixed (confirmed: "no flickering observed" on screensaver/lock/hibernate).
# ============================================================================
# Chain, in order:
#   1. direct2d-frame-latency-wait.patch      -- the base patch (mixed-refresh-rate fix).
#   2. direct2d-present-diagnostic.patch      -- kept in place; logs every Present1() result
#      under "Present1 normal" and a parallel DXGI_PRESENT_TEST probe under "Present1 test", to
#      %TEMP%\d2d_present_diagnostic.log.
#   3. direct2d-occlusion-standby-v3.patch    -- the confirmed fix for the screensaver/lock-
#      screen/hibernate flicker (DXGI_STATUS_OCCLUDED). Unchanged from the build you already
#      tested with "no flickering observed."
#   4. direct2d-reentrant-teardown-fix-v1.patch -- NEW. Fixes a separate, pre-existing JUCE
#      defect found by reading JUCE's own adapter-management code (DxgiAdapters/
#      Direct2DGraphicsContext::Pimpl/Direct2DHwndContext::HwndPimpl), unrelated to the
#      swap-chain occlusion logic above and present in stock, unpatched JUCE 9.0.3. It's its own
#      fix lineage -- "v1" -- not a successor to the occlusion-standby versions.
#
#      The defect: WM_DISPLAYCHANGE/WM_SETTINGCHANGE (fired on monitor unplug or any display
#      topology change) are delivered via SendMessage -- a *sent*, not posted, message. If the
#      message thread happens to be blocked inside a call that synchronises with the DWM
#      (IDXGISwapChain::ResizeBuffers, IDXGISwapChain1::Present1, IDCompositionDevice::Commit --
#      all reachable from one ordinary paint cycle), Windows can deliver that sent message
#      reentrantly, nested inside the very call that's still using this object's Direct2D/DXGI
#      members. The display-change handler then calls teardown(), which nulls out the device
#      context and swap chain out from under the interrupted outer call, which goes on to
#      dereference them -- a real crash, not a flicker or a freeze. Separately, teardown() also
#      only ever dropped this process's own COM references to the DirectComposition visual/
#      target without explicitly detaching its content from the DWM first -- since
#      CreateTargetForHwnd's target is tied to the window at the OS/compositor level, not purely
#      owned by this process, an incomplete teardown (made more likely by the same reentrancy)
#      could leave a static, window-shaped outline on screen that outlives this window and even
#      this process, clearing only once Windows itself restarts. Both symptoms were reported
#      directly: an intermittent crash/freeze on monitor unplug or dual-monitor reshuffle, and a
#      ghost outline that persisted after the host DAW had been closed entirely.
#
#      The fix has two parts, both in this one patch:
#      - A reentrancy guard: Direct2DGraphicsContext::Pimpl gains enterReentrancyGuard()/
#        leaveReentrancyGuard() (a nesting counter), and adapterCreated()/adapterRemoved() defer
#        teardown() instead of running it immediately if the guard is held, running it for real
#        once released. HwndPimpl holds that guard across prepare() and across the whole
#        startFrame()..finishFrame() span -- i.e. every place a reentrant display-change
#        notification could land mid-call.
#      - An explicit composition-teardown sequence: CompositionTree now has a real destructor
#        that detaches its content (SetContent(nullptr)), clears the visual root
#        (SetRoot(nullptr)), and commits that empty state (Commit()) before releasing its COM
#        references, so the DWM is actually told to stop compositing the visual instead of
#        relying on reference-counting to imply it.
#
#      Neither of these touches SwapChain's create()/resize() logic at all -- no swap-chain
#      recreation code here. See README.md for the full reasoning, what this still doesn't
#      cover (the GPU-device-removed gap), and how to test.
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        9.0.3
    GIT_SHALLOW    ON
    PATCH_COMMAND  git apply --whitespace=nowarn
                   "${CMAKE_CURRENT_SOURCE_DIR}/patches/direct2d-frame-latency-wait.patch" &&
                   git apply --whitespace=nowarn
                   "${CMAKE_CURRENT_SOURCE_DIR}/patches/direct2d-present-diagnostic.patch" &&
                   git apply --whitespace=nowarn
                   "${CMAKE_CURRENT_SOURCE_DIR}/patches/direct2d-occlusion-standby-v3.patch" &&
                   git apply --whitespace=nowarn
                   "${CMAKE_CURRENT_SOURCE_DIR}/patches/direct2d-reentrant-teardown-fix-v1.patch")
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(D2DReproVst3
    COMPANY_NAME               "Your Company"
    PLUGIN_MANUFACTURER_CODE   Yrco
    PLUGIN_CODE                D2d5        # distinct from the diagnostic-only build (D2dg), the
                                            # earlier D2DReproVst3 build (D2dr), the v3 standby
                                            # build (D2dv), and the main plugin (Spba) -- for
                                            # REAPER's own plugin-identity tracking. PRODUCT_NAME
                                            # below is deliberately unchanged (your CI's
                                            # Copy-Item step has that filename hardcoded), so this
                                            # still replaces whichever D2DReproVst3 build is
                                            # currently installed.
    BUNDLE_ID                  com.yourcompany.d2dreprovst3.v5reentrantteardown
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
