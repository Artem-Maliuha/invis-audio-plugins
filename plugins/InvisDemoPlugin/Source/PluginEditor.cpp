#include "PluginProcessor.h"
#include "PluginEditor.h"

InvisDemoPluginEditor::InvisDemoPluginEditor(InvisDemoPluginProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p), inputFilterUI(p.apvts, "in_filter_")
{
    // Mandatory Requirement 1: Resizable UI with Aspect Ratio Preservation
    setResizable(true, true);
    setResizeLimits(400, 304, 1200, 912);
    getConstrainer()->setFixedAspectRatio(500.0 / 380.0);
    setSize(500, 380);

    addAndMakeVisible(inputFilterUI);
}

void InvisDemoPluginEditor::paint(juce::Graphics& g)
{
    const auto theme = invis::ui::InvisTheme::getGlobalDefault();
    g.fillAll(theme.background);

    // Header scaling proportionally with editor height
    const float headerHeight = getLocalBounds().getHeight() * 0.12f;
    const float headerFontSize = headerHeight * 0.55f;

    g.setColour(theme.accentPrimary);
    g.setFont(juce::FontOptions(headerFontSize, juce::Font::bold));
    g.drawText("INVIS AUDIO DEMO", getLocalBounds().removeFromTop(headerHeight), juce::Justification::centred, true);
}

void InvisDemoPluginEditor::resized()
{
    auto bounds = getLocalBounds().reduced(juce::roundToInt(getWidth() * 0.035f));
    const int headerHeight = juce::roundToInt(getHeight() * 0.12f);
    bounds.removeFromTop(headerHeight); // Proportional header space

    // Dynamically scale and center module container with window dimensions
    const int moduleWidth = juce::jlimit(240, 700, static_cast<int>(bounds.getWidth() * 0.65f));
    const int moduleHeight = juce::jlimit(160, 500, static_cast<int>(bounds.getHeight() * 0.75f));
    
    inputFilterUI.setBounds(bounds.withSizeKeepingCentre(moduleWidth, moduleHeight));
}
