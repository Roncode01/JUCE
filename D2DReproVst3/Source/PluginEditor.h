#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

// A small, self-contained bar that animates and repaints itself continuously at 30Hz, regardless
// of user interaction -- mirroring exactly how the real plugin's meter panels work (a dedicated
// sub-component with its own paint(), driven by its own Timer). Testing whether sustained
// repaint traffic, not just drag-triggered repaints, is a necessary ingredient for the bug: the
// real plugin always has this load running; this repro, until now, never did.
class ActivityMeter : public juce::Component, private juce::Timer
{
public:
    ActivityMeter() { startTimerHz (30); }
    ~ActivityMeter() override { stopTimer(); }

    void paint (juce::Graphics& g) override;

private:
    void timerCallback() override { phase += 0.15f; repaint(); }
    float phase = 0.0f;
};

// Exact same UI as the standalone D2DRepro app (near-black background, two rotary knobs, one
// linear slider) -- the only difference is these are now wired to real AudioParameterFloats via
// SliderParameterAttachment, same mechanism the real plugin's own knobs use, rather than being
// freestanding/unconnected sliders. Now also includes an ActivityMeter (see above) for continuous
// 30Hz repaint load.
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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (D2DReproVst3Editor)
};
