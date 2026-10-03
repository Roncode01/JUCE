#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

// Exact same UI as the standalone D2DRepro app (near-black background, two rotary knobs, one
// linear slider) -- the only difference is these are now wired to real AudioParameterFloats via
// SliderParameterAttachment, same mechanism the real plugin's own knobs use, rather than being
// freestanding/unconnected sliders.
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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (D2DReproVst3Editor)
};
