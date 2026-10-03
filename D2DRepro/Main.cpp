#include <juce_gui_basics/juce_gui_basics.h>

class ReproComponent : public juce::Component
{
public:
    ReproComponent()
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setRange (0.0, 100.0);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 20);
        addAndMakeVisible (slider);
    }

    void resized() override
    {
        slider.setBounds (getLocalBounds().reduced (40).withSizeKeepingCentre (120, 150));
    }

private:
    juce::Slider slider;
};

class ReproWindow : public juce::DocumentWindow
{
public:
    ReproWindow()
        : DocumentWindow ("Direct2D Mixed-Refresh-Rate Repro",
                           juce::Colours::darkgrey,
                           DocumentWindow::allButtons)
    {
        setContentOwned (new ReproComponent(), true);
        centreWithSize (400, 400);
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
