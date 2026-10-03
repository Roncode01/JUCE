#include "PluginEditor.h"

D2DReproVst3Editor::D2DReproVst3Editor (D2DReproVst3Processor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    for (auto* s : { &rotary1, &rotary2 })
    {
        s->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 20);
        addAndMakeVisible (s);
    }
    rotary1Attach = std::make_unique<juce::SliderParameterAttachment> (
        *processorRef.apvts.getParameter ("rotary1"), rotary1, nullptr);
    rotary2Attach = std::make_unique<juce::SliderParameterAttachment> (
        *processorRef.apvts.getParameter ("rotary2"), rotary2, nullptr);

    linear.setSliderStyle (juce::Slider::LinearHorizontal);
    linear.setTextBoxStyle (juce::Slider::TextBoxRight, false, 60, 20);
    addAndMakeVisible (linear);
    linearAttach = std::make_unique<juce::SliderParameterAttachment> (
        *processorRef.apvts.getParameter ("linear"), linear, nullptr);

    setSize (420, 320);
}

void D2DReproVst3Editor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0a0a0c));
}

void D2DReproVst3Editor::resized()
{
    auto area = getLocalBounds().reduced (30);
    auto top = area.removeFromTop (160);
    rotary1.setBounds (top.removeFromLeft (140));
    rotary2.setBounds (top.removeFromRight (140));
    area.removeFromTop (20);
    linear.setBounds (area.removeFromTop (40));
}
