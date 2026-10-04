#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

// A small, self-contained bar that animates and repaints itself continuously, regardless of user
// interaction -- mirroring the real plugin's meter panels, but now driven by VBlankAttachment
// instead of Timer, following the pattern JUCE's own Direct2D developer (attila, on the JUCE
// forum) recommends specifically for this class of problem: a Timer's repaint() request and its
// actual paint() are not guaranteed to land in the same frame, whereas VBlankAttachment
// guarantees they do. Testing whether that synchronisation gap -- not just repaint volume -- is
// what the earlier Timer-based version was actually exposing.
//
// There's no reliable cross-platform way to query the display's true refresh rate through
// VBlankAttachment (confirmed on the JUCE forum thread this is based on), so setApproxTargetHz()
// approximates a target rate by skipping vblank callbacks, assuming a 60Hz display as the
// baseline divisor -- an approximation, not an exact Hz, but enough to explore a comparable
// range to the earlier Timer-based version.
class ActivityMeter : public juce::Component
{
public:
    ActivityMeter() = default;

    void setApproxTargetHz (int hz)
    {
        skipEvery = juce::jmax (1, (int) std::round (60.0 / (double) juce::jlimit (1, 1000, hz)));
        tickCounter = 0;
    }

    void paint (juce::Graphics& g) override;

private:
    void onVBlank (double)
    {
        if (++tickCounter >= skipEvery)
        {
            tickCounter = 0;
            phase += 0.15f;
            repaint();
        }
    }

    int skipEvery = 2;   // ~30Hz at the assumed 60Hz baseline, matching the earlier version's start
    int tickCounter = 0;
    float phase = 0.0f;

    juce::VBlankAttachment vblank { this, [this] (double t) { onVBlank (t); } };
};

// Exact same UI as the standalone D2DRepro app (near-black background, two rotary knobs, one
// linear slider) -- the only difference is these are now wired to real AudioParameterFloats via
// SliderParameterAttachment, same mechanism the real plugin's own knobs use, rather than being
// freestanding/unconnected sliders. rotary1 is RotaryHorizontalVerticalDrag (dual-axis, matches
// the real plugin); rotary2 is RotaryVerticalDrag (single-axis), for isolating whether the
// dual-axis combining specifically matters. Also includes an ActivityMeter (now VBlank-driven,
// see above) for continuous background repaint load, and a plain (unparameterised -- this is a
// test control, not something meant for host automation) slider that adjusts its approximate
// rate live.
class D2DReproVst3Editor : public juce::AudioProcessorEditor
{
public:
    explicit D2DReproVst3Editor (D2DReproVst3Processor&);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    D2DReproVst3Processor& processorRef;

    juce::Slider rotary1, rotary2, linear;
    std::unique_ptr<juce::SliderParameterAttachment> rotary1Attach, rotary2Attach, linearAttach;
    ActivityMeter activityMeter;
    juce::Slider timerFreqSlider;   // plain -- drives activityMeter's rate directly, not a parameter

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (D2DReproVst3Editor)
};
