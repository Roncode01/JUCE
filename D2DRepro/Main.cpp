#include <juce_gui_basics/juce_gui_basics.h>

class ReproComponent : public juce::Component
{
public:
    ReproComponent()
    {
        // Near-black, matching the kind of dark UI background the original bug was found on --
        // white flash artifacts show up far more obviously against this than mid-grey, and it's
        // a closer match to the conditions that actually triggered it.
        setOpaque (true);

        for (auto* s : { &rotary1, &rotary2 })
        {
            s->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            s->setRange (0.0, 100.0);
            s->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 20);
            addAndMakeVisible (s);
        }

        linear.setSliderStyle (juce::Slider::LinearHorizontal);
        linear.setRange (0.0, 100.0);
        linear.setTextBoxStyle (juce::Slider::TextBoxRight, false, 60, 20);
        addAndMakeVisible (linear);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff0a0a0c));
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (30);
        auto top = area.removeFromTop (160);
        rotary1.setBounds (top.removeFromLeft (140));
        rotary2.setBounds (top.removeFromRight (140));
        area.removeFromTop (20);
        linear.setBounds (area.removeFromTop (40));
    }

private:
    juce::Slider rotary1, rotary2, linear;
};

class ReproWindow : public juce::DocumentWindow
{
public:
    ReproWindow()
        : DocumentWindow ("Direct2D Mixed-Refresh-Rate Repro",
                           juce::Colours::black,
                           DocumentWindow::allButtons)
    {
        setContentOwned (new ReproComponent(), true);
        centreWithSize (420, 320);
        setResizable (true, true);
        setVisible (true);
    }

    void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
};

class ReproApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override    { return "D2DRepro"; }
    const juce::String getApplicationVersion() override { return "1.0.0"; }
    void initialise (const juce::String&) override      { window.reset (new ReproWindow()); }
    void shutdown() override                             { window = nullptr; }

private:
    std::unique_ptr<ReproWindow> window;
};

START_JUCE_APPLICATION (ReproApplication)
