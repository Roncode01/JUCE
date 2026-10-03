#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

// Minimal pass-through processor -- this plugin does nothing to the audio. It exists purely to
// host the same dark-background / two-rotary / one-linear-slider UI as the standalone D2DRepro
// app, inside a real VST3 loaded by a real host (REAPER), to test whether being hosted changes
// anything about the Direct2D mixed-refresh-rate behaviour versus a freestanding top-level window.
class D2DReproVst3Processor : public juce::AudioProcessor
{
public:
    D2DReproVst3Processor();
    ~D2DReproVst3Processor() override = default;

    void prepareToPlay (double, int) override {}
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "D2D Repro VST3"; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override {}
    void setStateInformation (const void*, int) override {}

    juce::AudioProcessorValueTreeState apvts;

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (D2DReproVst3Processor)
};
