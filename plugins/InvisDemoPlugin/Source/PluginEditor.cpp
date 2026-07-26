#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace {

// The catalogue a star can BE. Product knowledge, deliberately outside the atom.
struct EffectChoice { const char* name; juce::Colour colour; };

const EffectChoice kEffectCatalogue[] = {
    { "REVERB",   juce::Colour::fromRGB(0, 229, 255) },
    { "DELAY",    juce::Colour::fromRGB(255, 171, 0) },
    { "SATURATE", juce::Colour::fromRGB(224, 64, 251) },
    { "CHORUS",   juce::Colour::fromRGB(0, 230, 118) },
    { "FLANGER",  juce::Colour::fromRGB(255, 23, 68) },
    { "PHASER",   juce::Colour::fromRGB(41, 121, 255) },
    { "CRUSH",    juce::Colour::fromRGB(255, 87, 34) },
    { "FILTER",   juce::Colour::fromRGB(245, 247, 250) }
};

constexpr int kNumEffects = static_cast<int>(sizeof(kEffectCatalogue) / sizeof(kEffectCatalogue[0]));

} // namespace

// ================================ STAR PANEL ==================================================

InvisDemoPluginEditor::StarPanel::StarPanel(InvisDemoPluginEditor& o) : owner(o)
{

    std::vector<juce::String> names;
    for (const auto& e : kEffectCatalogue) names.push_back(e.name);

    effectCell.setLabel({});
    effectCell.setPopupMode(true);
    effectCell.setValueJustification(juce::Justification::centred);
    effectCell.setItems(names);
    effectCell.onIndexChanged = [this](int i, const juce::String& name) {
        if (index < 0) return;
        owner.constellation.setNodeLabel(index, name);
        owner.constellation.setNodeColour(index, kEffectCatalogue[i].colour);
    };
    addAndMakeVisible(effectCell);

    // Bipolar: the centre detent is silence, and either side of it is a polarity.
    sensitivityKnob.setKnobSize(invis::ui::InvisKnobSize::XS);
    sensitivityKnob.setLabel("SENS");
    sensitivityKnob.setValueArcOrigin(invis::ui::ValueArcOrigin::Center);
    sensitivityKnob.setDefaultValue(1.0f);
    sensitivityKnob.setStickyPositions({ 0.5f });
    sensitivityKnob.setScaleTicks({ { 0.0f, "-100" }, { 0.5f, "0" }, { 1.0f, "+100" } });
    sensitivityKnob.setValueFormatter([](float v) {
        return juce::String(juce::roundToInt((v * 2.0f - 1.0f) * 100.0f)) + "%";
    });
    sensitivityKnob.onValueChanged = [this](float v) {
        if (index >= 0) owner.constellation.setNodeSensitivity(index, v * 2.0f - 1.0f);
    };
    addAndMakeVisible(sensitivityKnob);

    addAndMakeVisible(divider);

    for (int i = 0; i < kNumEffects; ++i)
    {
        auto* b = swatches.add(new invis::ui::InvisButton());
        b->setButtonSize(invis::ui::InvisButtonSize::XS);
        b->setLedVisible(false);
        b->setLabel({});
        b->setToggleMode(false);
        b->setLedColour(kEffectCatalogue[i].colour);
        b->onClick = [this, i]() {
            if (index >= 0) owner.constellation.setNodeColour(index, kEffectCatalogue[i].colour);
        };
        addAndMakeVisible(b);
    }
}

void InvisDemoPluginEditor::StarPanel::showFor(int starIndex)
{
    index = starIndex;

    // The panel itself never goes away - only its contents do. Somewhere permanent for the
    // controls to live is the whole point; a panel that vanished took the layout with it.
    const bool has = (index >= 0);

    effectCell.setVisible(has);
    sensitivityKnob.setVisible(has);
    divider.setVisible(has);
    for (auto* b : swatches) b->setVisible(has);

    if (has)
    {
        const auto& node = owner.constellation.getNode(index);

        for (int i = 0; i < kNumEffects; ++i)
            if (node.label == kEffectCatalogue[i].name)
                effectCell.setSelectedIndex(i, juce::dontSendNotification);

        sensitivityKnob.setValue((node.sensitivity + 1.0f) * 0.5f, juce::dontSendNotification);
    }

    repaint();
}

void InvisDemoPluginEditor::StarPanel::paint(juce::Graphics& g)
{
    const auto theme = invis::ui::InvisTheme::getGlobalDefault();
    auto bounds = getLocalBounds().toFloat();

    g.setColour(juce::Colours::black.withAlpha(0.55f));
    g.fillRoundedRectangle(bounds.translated(0.0f, 2.0f), invis::ui::layout::kPanelCorner);
    g.setColour(juce::Colour::fromRGB(20, 25, 32).withAlpha(0.98f));
    g.fillRoundedRectangle(bounds, invis::ui::layout::kPanelCorner);
    g.setColour(juce::Colours::white.withAlpha(0.12f));
    g.drawRoundedRectangle(bounds, invis::ui::layout::kPanelCorner, 1.0f);

    auto area = bounds.reduced(10.0f);

    if (index >= 0)
    {
        const auto& node = owner.constellation.getNode(index);
        g.setColour(node.colour.brighter(0.3f));
        g.setFont(invis::ui::InvisFonts::getDisplayFont(12.0f));
        g.drawText("STAR " + juce::String(index + 1), area.removeFromTop(14.0f),
                   juce::Justification::centredLeft, false);

        // The hint has to be here: right-drag is fast once you know it, and undiscoverable
        // until then.
        g.setColour(theme.textSecondary.withAlpha(0.45f));
        g.setFont(invis::ui::InvisFonts::getDisplayFont(8.5f, false));
        g.drawText("OR ALT / RIGHT-DRAG ON THE STAR",
                   bounds.removeFromBottom(24.0f).reduced(10.0f, 0.0f),
                   juce::Justification::centred, false);
    }
    else
    {
        g.setColour(theme.textSecondary.withAlpha(0.40f));
        g.setFont(invis::ui::InvisFonts::getDisplayFont(10.0f, false));
        g.drawText("CLICK A STAR", bounds, juce::Justification::centred, false);
    }
}

void InvisDemoPluginEditor::StarPanel::resized()
{
    using namespace invis::ui;

    auto area = getLocalBounds().reduced(10);
    area.removeFromTop(14 + layout::kGapBonded);

    effectCell.setBounds(area.removeFromTop(26));
    area.removeFromTop(layout::kGapRelated);

    sensitivityKnob.setBoundsCentredIn(
        area.removeFromTop(InvisKnob::getIntrinsicSize(InvisKnobSize::XS).y));

    divider.setBoundsInStrip(area.removeFromTop(divider.getIntrinsicThickness()));

    auto row = area.removeFromTop(16);
    const int per = row.getWidth() / std::max(1, swatches.size());
    for (auto* b : swatches)
        b->setBounds(row.removeFromLeft(per).reduced(1, 0));
}

// ==============================================================================================

InvisDemoPluginEditor::InvisDemoPluginEditor(InvisDemoPluginProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p), chassis(p.chassis, p.apvts)
{
    // Mandatory Requirement 1: Resizable UI, fixed aspect, pure global zoom.
    // Everything below is laid out inside `canvas` in design pixels; `resized()` only scales it.
    // NOTE: the initial setSize() happens at the END of this constructor. Layout reads each
    // atom's intrinsic size, so it must not run before the atoms have been given their presets.
    addAndMakeVisible(canvas);

    // THE FRAME, ASSEMBLED AND LIVE. It adds its own sidebars, wires every callback back to the
    // audio side and runs its own clock; the bench hands it one component and is done.
    canvas.addAndMakeVisible(chassis);
    chassis.setWorkspace(workspace);

    // The bench's own instrument still needs advancing, on the same clock as the frame.
    chassis.onTick = [this](float dt) { constellation.tickAnimation(dt); };

    // A catalogue with real depth, so the tree is exercised rather than assumed. Storage is not
    // implemented - selecting a preset only reports the path.
    using invis::modules::PresetNode;
    chassis.getTop().setPresetTree(PresetNode::folder("", {
        PresetNode::preset("Init"),
        PresetNode::folder("Zodiac", {
            PresetNode::preset("Aries"),
            PresetNode::preset("Gemini"),
            PresetNode::preset("Leo"),
            PresetNode::preset("Libra"),
            PresetNode::preset("Scorpius"),
        }),
        PresetNode::folder("Northern", {
            PresetNode::folder("Bright", {
                PresetNode::preset("Cygnus"),
                PresetNode::preset("Lyra"),
                PresetNode::preset("Aquila"),
            }),
            PresetNode::preset("Cassiopeia"),
            PresetNode::preset("Draco"),
            PresetNode::preset("Ursa Major"),
        }),
        PresetNode::folder("Southern", {
            PresetNode::preset("Orion"),
            PresetNode::preset("Carina"),
            PresetNode::preset("Centaurus"),
            PresetNode::preset("Crux"),
        }),
        PresetNode::folder("Deep Sky", {
            PresetNode::preset("Andromeda"),
            PresetNode::preset("Pleiades"),
            PresetNode::preset("Vega"),
        }),
    }));

    // MORPH PAD: the polygon effect editor.
    constellation.setPadSize(invis::ui::InvisConstellationSize::L);
    constellation.onWeightsChanged = [this](int observerIndex, const std::vector<float>& w) {
        // Weights are NOT normalised: outside every aura the signal is dry. Once real effects
        // exist these become parallel send levels, one set per channel stream.
        juce::ignoreUnused(observerIndex, w);
    };
    workspace.addAndMakeVisible(constellation);

    addNodeButton.setButtonSize(invis::ui::InvisButtonSize::S);
    addNodeButton.setLabel("+ ADD");
    addNodeButton.setLedVisible(false);   // it starts an action; there is no state to report
    addNodeButton.setToggleMode(false);
    addNodeButton.onClick = [this]() {
        const int slot = nextEffectSlot % kNumEffects;
        if (constellation.addNode(kEffectCatalogue[slot].name, kEffectCatalogue[slot].colour) >= 0)
            ++nextEffectSlot;
    };
    workspace.addAndMakeVisible(addNodeButton);

    randomiseButton.setButtonSize(invis::ui::InvisButtonSize::S);
    randomiseButton.setLabel("RANDOM");
    randomiseButton.setLedVisible(false);
    randomiseButton.setToggleMode(false);
    randomiseButton.onClick = [this]() { constellation.randomise(); };
    workspace.addAndMakeVisible(randomiseButton);

    constellation.onNodeClicked = [this](int index) { starPanel.showFor(index); };
    workspace.addAndMakeVisible(starPanel);
    starPanel.showFor(-1);   // prepared and empty until a star is picked

    // Randomising rebuilds the chart, so whatever the inspector was holding is gone with it.
    constellation.onGeometryChanged = [this]() {
        if (starPanel.index >= constellation.getNumNodes()) starPanel.showFor(-1);
    };

    channelModeCell.setLabel({});
    channelModeCell.setPopupMode(true);
    channelModeCell.setValueJustification(juce::Justification::centred);
    channelModeCell.setItems({ "L+R", "L R", "M+S", "M S" });
    channelModeCell.onIndexChanged = [this](int index, const juce::String&) {
        constellation.setChannelMode(static_cast<invis::ui::ConstellationChannelMode>(index));
    };
    workspace.addAndMakeVisible(channelModeCell);

    // LAST: every child now exists and carries its final size preset, so the first layout pass
    // can read correct intrinsic sizes.
    invis::ui::applyDesignResizeLimits(*this, designSize.x, designSize.y);
}

InvisDemoPluginEditor::~InvisDemoPluginEditor() = default;

void InvisDemoPluginEditor::paint(juce::Graphics& g)
{
    // The editor itself only fills the letterbox area; all artwork lives on the scaled canvas.
    g.fillAll(juce::Colours::black);
}

void InvisDemoPluginEditor::paintCanvas(juce::Graphics& g)
{
    const auto bounds = canvas.getLocalBounds().toFloat();

    // 1. Render Procedural Premium Studio Panel Background Based on currentPanelTheme
    juce::Colour c1, c2, bloomColor;

    switch (currentPanelTheme)
    {
        case PanelTheme::DarkSlateCharcoal:
            c1 = juce::Colour::fromRGB(18, 22, 28);
            c2 = juce::Colour::fromRGB(12, 14, 18);
            bloomColor = juce::Colour::fromRGB(40, 52, 70).withAlpha(0.20f);
            break;

        case PanelTheme::ObsidianBlack:
            c1 = juce::Colour::fromRGB(14, 16, 20);
            c2 = juce::Colour::fromRGB(8, 9, 12);
            bloomColor = juce::Colour::fromRGB(30, 35, 45).withAlpha(0.15f);
            break;

        case PanelTheme::MidnightIndigo:
            c1 = juce::Colour::fromRGB(16, 28, 48);
            c2 = juce::Colour::fromRGB(8, 14, 26);
            bloomColor = juce::Colour::fromRGB(30, 80, 150).withAlpha(0.25f);
            break;

        case PanelTheme::CyberViolet:
            c1 = juce::Colour::fromRGB(32, 18, 48);
            c2 = juce::Colour::fromRGB(14, 8, 24);
            bloomColor = juce::Colour::fromRGB(120, 40, 160).withAlpha(0.28f);
            break;

        case PanelTheme::GunmetalSteel:
        default:
            c1 = juce::Colour::fromRGB(26, 32, 40);
            c2 = juce::Colour::fromRGB(16, 18, 24);
            bloomColor = juce::Colour::fromRGB(60, 75, 95).withAlpha(0.22f);
            break;
    }

    // Top-to-bottom studio lighting gradient
    const auto bgGrad = juce::ColourGradient(
        c1, bounds.getCentreX(), 0.0f,
        c2, bounds.getCentreX(), bounds.getBottom(),
        false
    );
    g.setGradientFill(bgGrad);
    g.fillAll();

    // Studio ambient center radial bloom
    const auto bloomGrad = juce::ColourGradient(
        bloomColor, bounds.getCentreX(), bounds.getCentreY() * 0.85f,
        juce::Colours::transparentBlack, bounds.getCentreX(), bounds.getBottom(),
        true
    );
    g.setGradientFill(bloomGrad);
    g.fillAll();

    // Dark studio vignette border
    const auto vignetteGrad = juce::ColourGradient(
        juce::Colours::transparentBlack, bounds.getCentreX(), bounds.getCentreY(),
        juce::Colours::black.withAlpha(0.40f), 0.0f, 0.0f, true
    );
    g.setGradientFill(vignetteGrad);
    g.fillAll();

}

void InvisDemoPluginEditor::resized()
{
    // The ONLY responsive maths in the whole editor: one zoom factor for the whole tree.
    invis::ui::applyDesignZoom(canvas, designSize.x, designSize.y, getLocalBounds());
}

void InvisDemoPluginEditor::layoutCanvas()
{
    // The frame owns the frame. Where the sidebars go, how wide they are and what is left over is
    // the chassis's arithmetic, and duplicating it here is how the two drift apart.
    chassis.setBounds(canvas.getLocalBounds());
}

void InvisDemoPluginEditor::layoutWorkspace()
{
    using namespace invis::ui;

    auto area = workspace.getLocalBounds();

    // The inspector takes a fixed column on the right, OUTSIDE the star field. A panel that
    // overlapped the chart put the editing controls on top of the thing being edited.
    auto inspectorColumn = area.removeFromRight(kStarPanelWidth);
    area.removeFromRight(layout::kGapGroup);

    starPanel.setBounds(inspectorColumn.removeFromTop(kStarPanelHeight));

    // The chart, with its keys directly above it - the keys belong to the chart, so they are
    // bonded to it rather than floating in the row.
    const auto padSize = constellation.getIntrinsicSize();
    const auto btnSize = addNodeButton.getIntrinsicSize();

    auto stack = area.withSizeKeepingCentre(padSize.x,
                                            btnSize.y + layout::kGapBonded + padSize.y);

    auto keyRow = stack.removeFromTop(btnSize.y);
    channelModeCell.setBounds(keyRow.removeFromRight(kChannelCellWidth)
                                    .withSizeKeepingCentre(kChannelCellWidth, btnSize.y));
    keyRow.removeFromRight(layout::kGapRelated);

    addNodeButton.setBoundsCentredIn(keyRow.removeFromLeft(addNodeButton.getIntrinsicSize().x));
    keyRow.removeFromLeft(layout::kGapBonded);
    randomiseButton.setBoundsCentredIn(keyRow.removeFromLeft(randomiseButton.getIntrinsicSize().x));

    stack.removeFromTop(layout::kGapBonded);
    constellation.setBoundsCentredIn(stack.removeFromTop(padSize.y));
}
