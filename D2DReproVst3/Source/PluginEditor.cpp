#include "PluginEditor.h"

void ActivityMeter::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0a0a0c));
    auto level = 0.5f + 0.5f * std::sin (phase);
    auto bounds = getLocalBounds().toFloat().reduced (2.0f);
    g.setColour (juce::Colours::darkgrey);
    g.drawRect (bounds, 1.0f);
    g.setColour (juce::Colours::orange);
    g.fillRect (bounds.reduced (2.0f).removeFromLeft (bounds.getWidth() * level));
}

D2DReproVst3Editor::D2DReproVst3Editor (D2DReproVst3Processor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    // rotary1: unchanged -- RotaryHorizontalVerticalDrag, the style every control in the real
    // plugin uses, and the one already confirmed to flicker in this repro.
    rotary1.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    rotary1.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 20);
    addAndMakeVisible (rotary1);
    rotary1Attach = std::make_unique<juce::SliderParameterAttachment> (
        *processorRef.apvts.getParameter ("rotary1"), rotary1, nullptr);

    // rotary2: single-axis vertical-only drag, no horizontal contribution -- isolates whether
    // the dual-axis combining specifically (as opposed to rotary controls generally) matters.
    rotary2.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    rotary2.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 20);
    addAndMakeVisible (rotary2);
    rotary2Attach = std::make_unique<juce::SliderParameterAttachment> (
        *processorRef.apvts.getParameter ("rotary2"), rotary2, nullptr);

    linear.setSliderStyle (juce::Slider::LinearHorizontal);
    linear.setTextBoxStyle (juce::Slider::TextBoxRight, false, 60, 20);
    addAndMakeVisible (linear);
    linearAttach = std::make_unique<juce::SliderParameterAttachment> (
        *processorRef.apvts.getParameter ("linear"), linear, nullptr);

    addAndMakeVisible (activityMeter);

    setSize (420, 380);
}

void D2DReproVst3Editor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0a0a0c));

    auto drawLabel = [&] (const juce::String& text, juce::Rectangle<int> knobBounds)
    {
        g.setColour (juce::Colours::lightgrey);
        g.setFont (juce::FontOptions (14.0f));
        g.drawText (text, knobBounds.withY (knobBounds.getY() - 20).withHeight (18),
                    juce::Justification::centred, false);
    };
    drawLabel ("H+V drag", rotary1.getBounds());
    drawLabel ("V-only drag", rotary2.getBounds());
}

void D2DReproVst3Editor::resized()
{
    auto area = getLocalBounds().reduced (30);
    area.removeFromTop (20);   // room for the knob labels drawn in paint()
    auto top = area.removeFromTop (160);
    rotary1.setBounds (top.removeFromLeft (140));
    rotary2.setBounds (top.removeFromRight (140));
    area.removeFromTop (20);
    linear.setBounds (area.removeFromTop (40));
    area.removeFromTop (20);
    activityMeter.setBounds (area.removeFromTop (24));
}
