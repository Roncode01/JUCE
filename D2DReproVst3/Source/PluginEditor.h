#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

// A small, self-contained bar that animates and repaints itself continuously, regardless of user
// interaction -- mirroring exactly how the real plugin's meter panels work (a dedicated
// sub-component with its own paint(), driven by its own Timer). setRefreshHz() lets the editor
// change its rate live, for testing whether background repaint load alone -- with no control
// being dragged at all -- can trigger flickering once pushed high enough.
class ActivityMeter : public juce::Component, private juce::Timer
{
public:
    ActivityMeter() { startTimerHz (30); }
    ~ActivityMeter() override { stopTimer(); }

    void setRefreshHz (int hz) { startTimerHz (juce::jlimit (1, 1000, hz)); }

    void paint (juce::Graphics& g) override;

private:
    void timerCallback() override { phase += 0.15f; repaint(); }
    float phase = 0.0f;
};

// Exact same UI as the standalone D2DRepro app (near-black background, two rotary knobs, one
// linear slider) -- the only difference is these are now wired to real AudioParameterFloats via
// SliderParameterAttachment, same mechanism the real plugin's own knobs use, rather than being
// freestanding/unconnected sliders. rotary1 is RotaryHorizontalVerticalDrag (dual-axis, matches
// the real plugin); rotary2 is RotaryVerticalDrag (single-axis), for isolating whether the
// dual-axis combining specifically matters. Also includes an ActivityMeter for continuous
// background repaint load, and a plain (unparameterised -- this is a test control, not something
// meant for host automation) slider that adjusts the ActivityMeter's rate live.
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
