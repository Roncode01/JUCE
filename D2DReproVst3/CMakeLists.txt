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
# plugin's meter panels actually work) specifically to test the patch against *that* mechanism
# before carrying it over to the real plugin -- the VBlankAttachment swap was only ever a repro
# experiment, never something in the production code. Same patch file, same PATCH_COMMAND
# mechanism as the VBlankAttachment build; nothing about the patch itself changes between variants
# since it only touches JUCE's own swap-chain code, not ours.
#
# A second, local patch (direct2d-occlusion-didpresent.patch) is applied on top, added 2026-10-09
# after flicker was reported returning specifically after a Windows lock-screen/screensaver
# transition, fixed by closing and reopening the editor. Root cause: reuk's patch only resets its
# present-throttle latch when Present1 returns exactly S_OK, but Present1 returns a different
# (still-successful) code, DXGI_STATUS_OCCLUDED, while the window is occluded -- which a locked
# or screensaver'd session is. That left the throttle stuck "unlocked" (i.e. disabled) for the
# whole occluded period, silently reintroducing the original un-throttled race once the session
# unlocked -- until the editor was closed/reopened, which rebuilds the swap chain and its throttle
# state from scratch. The fix resets the latch on any successful Present1 (FAILED is still handled
# separately above it), not only hr == S_OK. See README.md for the verification record. NOT YET
# tested against a real lock-screen/screensaver repro (needs a Windows machine) -- this patch is
# a candidate fix pending that confirmation, same as the base patch was before its own D2DReproVst3
# verification.
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
