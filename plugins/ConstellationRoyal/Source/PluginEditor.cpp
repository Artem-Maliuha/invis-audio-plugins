#include "PluginProcessor.h"
#include "PluginEditor.h"

ConstellationRoyalEditor::ConstellationRoyalEditor(ConstellationRoyalProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p),
      chassis(p.chassis, p.apvts),
      workspace(p.engine, p.apvts),
      presets(p.apvts, "Invis Audio", "Constellation Royal")
{
    addAndMakeVisible(canvas);
    canvas.addAndMakeVisible(chassis);
    chassis.setWorkspace(workspace);

    // The instrument advances on the frame's clock, and reloads when the frame swaps state.
    chassis.onTick = [this](float dt) { workspace.tick(dt); };
    chassis.onStateRestored = [this]() { workspace.reloadFromState(); };

    // THE LIBRARY. Real asterisms, and not as decoration: the shape a constellation actually has
    // decides what it is good for. Corona Borealis is a closed arc, so it becomes a parallel bank
    // of drive; Draco is a long winding chain, so it becomes spaces you walk through in series.
    // See InvisPresetStore for where they live and why there.
    presets.refresh();
    refreshPresetTree();

    chassis.getTop().onPresetChanged = [this](int index, const juce::String&) {
        if (presets.load(index)) workspace.reloadFromState();
    };

    chassis.getTop().onSavePreset = [this]() {
        // ASYNCHRONOUS, and the name is typed rather than picked. A file chooser would put the
        // library's location in the user's hands on every save, which is exactly the thing the
        // folder convention exists to decide once.
        auto* window = new juce::AlertWindow("SAVE PRESET",
                                             "Name it. Use Category/Name to file it in a folder.",
                                             juce::MessageBoxIconType::NoIcon);

        window->addTextEditor("path", "User/My Constellation", {});
        window->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
        window->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

        window->enterModalState(true, juce::ModalCallbackFunction::create(
            [this, window](int result)
            {
                const auto typed = window->getTextEditorContents("path").trim();
                delete window;

                if (result != 1 || typed.isEmpty()) return;

                const auto slash = typed.lastIndexOfChar('/');
                const auto category = slash > 0 ? typed.substring(0, slash) : juce::String("User");
                const auto name = slash > 0 ? typed.substring(slash + 1) : typed;

                if (presets.save(category, name)) refreshPresetTree();
            }), false);
    };

    chassis.getTop().onRestoreFactory = [this]() {
        presets.restoreFactory();
        refreshPresetTree();
    };

    chassis.getTop().onRevealPresetFolder = [this]() {
        presets.getUserFolder().revealToUser();
    };

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

void ConstellationRoyalEditor::refreshPresetTree()
{
    chassis.getTop().setPresetTree(presets.getTree());
}
