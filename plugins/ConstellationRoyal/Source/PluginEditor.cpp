#include "PluginProcessor.h"
#include "PluginEditor.h"

ConstellationRoyalEditor::ConstellationRoyalEditor(ConstellationRoyalProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p),
      chassis(p.chassis, p.apvts),
      workspace(p.engine, p.apvts)
{
    addAndMakeVisible(canvas);
    canvas.addAndMakeVisible(chassis);
    chassis.setWorkspace(workspace);

    // The instrument advances on the frame's clock, and reloads when the frame swaps state.
    chassis.onTick = [this](float dt) { workspace.tick(dt); };
    chassis.onStateRestored = [this]() { workspace.reloadFromState(); };

    // Presets are named after real constellations, which is the series' own convention. Storage is
    // not implemented yet - selecting one only reports its path.
    using invis::modules::PresetNode;
    chassis.getTop().setPresetTree(PresetNode::folder("", {
        PresetNode::preset("Init"),
        PresetNode::folder("Zodiac", {
            PresetNode::preset("Aries"), PresetNode::preset("Gemini"),
            PresetNode::preset("Leo"), PresetNode::preset("Libra"),
        }),
        PresetNode::folder("Northern", {
            PresetNode::preset("Cygnus"), PresetNode::preset("Lyra"),
            PresetNode::preset("Cassiopeia"), PresetNode::preset("Draco"),
        }),
        PresetNode::folder("Southern", {
            PresetNode::preset("Orion"), PresetNode::preset("Carina"),
            PresetNode::preset("Crux"),
        }),
    }));

    // LAST: every child now exists and carries its final size preset, so the first layout pass can
    // read correct intrinsic sizes.
    invis::ui::applyDesignResizeLimits(*this, designSize.x, designSize.y);
}

void ConstellationRoyalEditor::paint(juce::Graphics& g)
{
    // The editor itself only fills the letterbox area; all artwork lives on the scaled canvas.
    g.fillAll(juce::Colours::black);
}

void ConstellationRoyalEditor::paintCanvas(juce::Graphics& g)
{
    const auto bounds = canvas.getLocalBounds().toFloat();

    g.setGradientFill(juce::ColourGradient(
        juce::Colour::fromRGB(18, 22, 28), bounds.getCentreX(), 0.0f,
        juce::Colour::fromRGB(12, 14, 18), bounds.getCentreX(), bounds.getBottom(), false));
    g.fillAll();

    g.setGradientFill(juce::ColourGradient(
        juce::Colour::fromRGB(40, 52, 70).withAlpha(0.20f),
        bounds.getCentreX(), bounds.getCentreY() * 0.85f,
        juce::Colours::transparentBlack, bounds.getCentreX(), bounds.getBottom(), true));
    g.fillAll();

    g.setGradientFill(juce::ColourGradient(
        juce::Colours::transparentBlack, bounds.getCentreX(), bounds.getCentreY(),
        juce::Colours::black.withAlpha(0.40f), 0.0f, 0.0f, true));
    g.fillAll();
}

void ConstellationRoyalEditor::resized()
{
    // The ONLY responsive maths in the whole editor: one zoom factor for the whole tree.
    invis::ui::applyDesignZoom(canvas, designSize.x, designSize.y, getLocalBounds());
}

void ConstellationRoyalEditor::layoutCanvas()
{
    chassis.setBounds(canvas.getLocalBounds());
}
