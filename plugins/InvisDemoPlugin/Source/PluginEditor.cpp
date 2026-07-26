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

    // 0..100%, resting at half, with the detent there. It used to rest at FULL, so the only place
    // a new star could go was down.
    sensitivityKnob.setKnobSize(invis::ui::InvisKnobSize::XS);
    sensitivityKnob.setLabel("SENS");
    sensitivityKnob.setValueArcOrigin(invis::ui::ValueArcOrigin::Center);
    sensitivityKnob.setDefaultValue(0.5f);
    sensitivityKnob.setStickyPositions({ 0.5f });
    sensitivityKnob.setScaleTicks({ { 0.0f, "0" }, { 0.5f, "50" }, { 1.0f, "100" } });
    sensitivityKnob.setValueFormatter([](float v) {
        return juce::String(juce::roundToInt(v * 100.0f)) + "%";
    });
    sensitivityKnob.onValueChanged = [this](float v) {
        if (index >= 0) owner.constellation.setNodeSensitivity(index, v);
    };
    addAndMakeVisible(sensitivityKnob);

    // THE BLOCK'S OWN CONTROLS. Band limiting and mix belong to the slot rather than to whatever
    // algorithm sits in it: every effect wants them, none should have to implement them, and a
    // preset keeps them when you swap the effect out.
    dryWetKnob.setKnobSize(invis::ui::InvisKnobSize::XS);
    dryWetKnob.setLabel("DRY/WET");
    dryWetKnob.setDefaultValue(1.0f);
    dryWetKnob.setScaleTicks({ { 0.0f, "DRY" }, { 0.5f, "50" }, { 1.0f, "WET" } });
    dryWetKnob.setValueFormatter([](float v) {
        return juce::String(juce::roundToInt(v * 100.0f)) + "%";
    });
    dryWetKnob.onValueChanged = [this](float v) {
        if (index >= 0) owner.constellation.setNodeDryWet(index, v);
    };
    addAndMakeVisible(dryWetKnob);

    // Same language as the chassis filters, so a star and the sidebar do not need translating.
    const auto hz = [](float norm, float lo, float hi) {
        return lo * std::pow(hi / lo, norm);
    };

    hpfKnob.setKnobSize(invis::ui::InvisKnobSize::XS);
    hpfKnob.setLabel("HPF");
    hpfKnob.setOffPosition(invis::ui::OffPosition::Start);
    hpfKnob.setValueArcOrigin(invis::ui::ValueArcOrigin::Start);
    hpfKnob.setDefaultValue(0.0f);
    hpfKnob.setScaleTicks({ { 0.0f, "OFF" }, { 0.5f, "200" }, { 1.0f, "2k" } });
    hpfKnob.setValueFormatter([this, hz](float v) {
        return hpfKnob.isOff() ? juce::String("OFF")
                               : juce::String(juce::roundToInt(hz(v, 20.0f, 2000.0f))) + " Hz";
    });
    hpfKnob.onValueChanged = [this](float v) {
        hpfKnob.setOff(v <= 0.001f, juce::dontSendNotification);
        if (index >= 0) owner.constellation.setNodeHpf(index, v);
    };
    addAndMakeVisible(hpfKnob);

    lpfKnob.setKnobSize(invis::ui::InvisKnobSize::XS);
    lpfKnob.setLabel("LPF");
    lpfKnob.setOffPosition(invis::ui::OffPosition::End);
    lpfKnob.setValueArcOrigin(invis::ui::ValueArcOrigin::End);
    lpfKnob.setDefaultValue(1.0f);
    lpfKnob.setScaleTicks({ { 0.0f, "1k" }, { 0.5f, "5k" }, { 1.0f, "OFF" } });
    lpfKnob.setValueFormatter([this, hz](float v) {
        return lpfKnob.isOff() ? juce::String("OFF")
                               : juce::String(juce::roundToInt(hz(v, 1000.0f, 20000.0f))) + " Hz";
    });
    lpfKnob.onValueChanged = [this](float v) {
        lpfKnob.setOff(v >= 0.999f, juce::dontSendNotification);
        if (index >= 0) owner.constellation.setNodeLpf(index, v);
    };
    addAndMakeVisible(lpfKnob);

    // Colour picking is out for now: the effect already carries its colour, so the swatch row was
    // a second way to say the same thing - and the one that could disagree with it.
}

void InvisDemoPluginEditor::StarPanel::refreshFromNode()
{
    if (index < 0) return;

    const auto& node = owner.constellation.getNode(index);

    for (int i = 0; i < kNumEffects; ++i)
        if (node.label == kEffectCatalogue[i].name)
            effectCell.setSelectedIndex(i, juce::dontSendNotification);

    sensitivityKnob.setValue(node.sensitivity, juce::dontSendNotification);
    dryWetKnob.setValue(node.dryWet, juce::dontSendNotification);

    hpfKnob.setValue(node.hpf, juce::dontSendNotification);
    hpfKnob.setOff(node.hpf <= 0.001f, juce::dontSendNotification);

    lpfKnob.setValue(node.lpf, juce::dontSendNotification);
    lpfKnob.setOff(node.lpf >= 0.999f, juce::dontSendNotification);
}

void InvisDemoPluginEditor::StarPanel::showFor(int starIndex)
{
    index = starIndex;

    // The panel itself never goes away - only its contents do. Somewhere permanent for the
    // controls to live is the whole point; a panel that vanished took the layout with it.
    const bool has = (index >= 0);

    for (auto* c : { static_cast<juce::Component*>(&effectCell),
                     static_cast<juce::Component*>(&sensitivityKnob),
                     static_cast<juce::Component*>(&dryWetKnob),
                     static_cast<juce::Component*>(&hpfKnob),
                     static_cast<juce::Component*>(&lpfKnob) })
        c->setVisible(has);

    refreshFromNode();
    repaint();
}

void InvisDemoPluginEditor::StarPanel::paintPanelShell(juce::Graphics& g,
                                                      juce::Rectangle<float> bounds,
                                                      const juce::String& heading)
{
    const auto theme = invis::ui::InvisTheme::getGlobalDefault();

    // NOW THAT THE SKY HAS NO FRAME, the panels carry the structure. A heading is what makes a
    // rectangle a place rather than a container - and it is the only edge left on this side of the
    // workspace, so it has to be worth drawing.
    g.setColour(juce::Colours::black.withAlpha(0.55f));
    g.fillRoundedRectangle(bounds.translated(0.0f, 2.0f), invis::ui::layout::kPanelCorner);
    g.setColour(juce::Colour::fromRGB(20, 25, 32).withAlpha(0.98f));
    g.fillRoundedRectangle(bounds, invis::ui::layout::kPanelCorner);
    g.setColour(juce::Colours::white.withAlpha(0.10f));
    g.drawRoundedRectangle(bounds, invis::ui::layout::kPanelCorner, 1.0f);

    auto head = bounds.reduced(10.0f, 0.0f).withHeight(kPanelHeadingHeight).translated(0.0f, 7.0f);

    g.setFont(invis::ui::InvisFonts::getDisplayFont(9.5f, false));
    g.setColour(theme.textSecondary.withAlpha(0.55f));
    g.drawText(heading, head, juce::Justification::centredLeft, false);

    const float rule = head.getBottom() + 3.0f;
    g.setColour(juce::Colours::white.withAlpha(0.08f));
    g.drawLine(bounds.getX() + 10.0f, rule, bounds.getRight() - 10.0f, rule, 1.0f);
}

void InvisDemoPluginEditor::StarPanel::paint(juce::Graphics& g)
{
    const auto theme = invis::ui::InvisTheme::getGlobalDefault();
    auto bounds = getLocalBounds().toFloat();

    paintPanelShell(g, bounds, "STAR");

    if (index >= 0)
    {
        // No "STAR 3" heading. The effect selector directly below already names what this is, in
        // the star's own colour, and an index the chart never shows is not an identity anyone can
        // use to find it again.
        //
        // The hint stays: right-drag is fast once you know it, and undiscoverable until then.
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
    area.removeFromTop(kPanelHeadingHeight + layout::kGapRelated);

    effectCell.setBounds(area.removeFromTop(26));
    area.removeFromTop(layout::kGapGroup);

    // Two rows of two. WHAT the block sends first, then WHAT IT SENDS THROUGH - the dry tap is
    // taken before the filters, so the mix reads above them rather than after.
    const int knobH = InvisKnob::getIntrinsicSize(InvisKnobSize::XS).y;

    auto pair = [](juce::Rectangle<int> row, InvisKnob& a, InvisKnob& b)
    {
        a.setBoundsCentredIn(row.removeFromLeft(row.getWidth() / 2));
        b.setBoundsCentredIn(row);
    };

    pair(area.removeFromTop(knobH), sensitivityKnob, dryWetKnob);
    area.removeFromTop(layout::kGapRelated);
    pair(area.removeFromTop(knobH), hpfKnob, lpfKnob);
}

// ================================ CHART PANEL =================================================

InvisDemoPluginEditor::ChartPanel::ChartPanel(InvisDemoPluginEditor& o) : owner(o)
{
    randomiseButton.setButtonSize(invis::ui::InvisButtonSize::S);
    randomiseButton.setLabel("RANDOM");
    randomiseButton.setLedVisible(false);
    randomiseButton.setToggleMode(false);
    randomiseButton.onClick = [this]() { owner.constellation.randomise(); };
    addAndMakeVisible(randomiseButton);

    channelModeCell.setLabel({});
    channelModeCell.setPopupMode(true);
    channelModeCell.setValueJustification(juce::Justification::centred);
    channelModeCell.setItems({ "L+R", "L R", "M+S", "M S" });
    channelModeCell.onIndexChanged = [this](int index, const juce::String&) {
        owner.constellation.setChannelMode(static_cast<invis::ui::ConstellationChannelMode>(index));
    };
    addAndMakeVisible(channelModeCell);
}

void InvisDemoPluginEditor::ChartPanel::paint(juce::Graphics& g)
{
    StarPanel::paintPanelShell(g, getLocalBounds().toFloat(), "CHART");

    const auto theme = invis::ui::InvisTheme::getGlobalDefault();
    g.setColour(theme.textSecondary.withAlpha(0.40f));
    g.setFont(invis::ui::InvisFonts::getDisplayFont(8.5f, false));
    g.drawText("CLICK EMPTY SKY TO ADD",
               getLocalBounds().toFloat().removeFromBottom(20.0f).reduced(10.0f, 0.0f),
               juce::Justification::centred, false);
}

void InvisDemoPluginEditor::ChartPanel::resized()
{
    using namespace invis::ui;

    auto area = getLocalBounds().reduced(10);
    area.removeFromTop(kPanelHeadingHeight + layout::kGapRelated);

    channelModeCell.setBounds(area.removeFromTop(24));
    area.removeFromTop(layout::kGapRelated);
    randomiseButton.setBoundsCentredIn(area.removeFromTop(randomiseButton.getIntrinsicSize().y));
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

    constellation.onNodeClicked = [this](int index) { starPanel.showFor(index); };

    // BOTH WAYS. The panel could always write to a star; it could never learn that the star had
    // been dragged, so the two quietly disagreed until you reselected it.
    constellation.onNodeChanged = [this](int index) {
        if (starPanel.index == index) starPanel.showFor(index);
        pushChartToState();
    };

    // A/B/C and preset recall replace the whole tree; the chart lives in it, so it comes back too.
    chassis.onStateRestored = [this]() { pullChartFromState(); };
    pullChartFromState();

    // The chart offers the SPOT; which effect lands there is the bench's knowledge, not the atom's.
    constellation.onRequestAddNode = [this](juce::Point<float> at) { chooseEffectThen(at); };
    workspace.addAndMakeVisible(starPanel);
    starPanel.showFor(-1);   // prepared and empty until a star is picked

    // Randomising rebuilds the chart, so whatever the inspector was holding is gone with it.
    constellation.onGeometryChanged = [this]() {
        if (starPanel.index >= constellation.getNumNodes()) starPanel.showFor(-1);
        pushChartToState();
    };

    workspace.addAndMakeVisible(chartPanel);

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

void InvisDemoPluginEditor::pushChartToState()
{
    if (restoringChart) return;

    auto& state = processorRef.apvts.state;
    const auto type = invis::ui::InvisConstellation::getStateType();

    state.removeChild(state.getChildWithName(type), nullptr);
    state.appendChild(constellation.toValueTree(), nullptr);
}

void InvisDemoPluginEditor::pullChartFromState()
{
    const auto tree = processorRef.apvts.state.getChildWithName(
        invis::ui::InvisConstellation::getStateType());

    if (!tree.isValid()) return;

    const juce::ScopedValueSetter<bool> guard(restoringChart, true);
    constellation.restoreFromValueTree(tree);
    starPanel.showFor(-1);
}

void InvisDemoPluginEditor::chooseEffectThen(std::optional<juce::Point<float>> at)
{
    juce::PopupMenu menu;
    menu.addSectionHeader("NEW STAR");

    for (int i = 0; i < kNumEffects; ++i)
    {
        juce::PopupMenu::Item item(kEffectCatalogue[i].name);
        item.itemID = i + 1;
        item.colour = kEffectCatalogue[i].colour;
        menu.addItem(item);
    }

    // UNDER THE CURSOR, where the star is about to appear. A menu anchored to a button somewhere
    // else makes you look away from the spot you just chose, and then look back to find out
    // whether it landed there.
    auto options = juce::PopupMenu::Options().withMinimumWidth(140)
                                             .withStandardItemHeight(22);

    if (at.has_value())
    {
        const auto screen = constellation.localPointToGlobal(
            constellation.toPixelsFromNormalized(*at)).roundToInt();

        options = options.withTargetScreenArea({ screen.x, screen.y, 1, 1 });
    }
    else
    {
        options = options.withTargetComponent(&chartPanel);
    }

    // Dismissing adds NOTHING. Picking the effect is the act of creating the star, so backing out
    // of the choice has to back out of the creation too.
    menu.showMenuAsync(options, [this, at](int result)
    {
        if (result <= 0 || result > kNumEffects) return;

        const auto& choice = kEffectCatalogue[result - 1];
        const int index = at.has_value() ? constellation.addNodeAt(choice.name, choice.colour, *at)
                                         : constellation.addNode(choice.name, choice.colour);

        if (index >= 0) starPanel.showFor(index);
    });
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
    inspectorColumn.removeFromTop(layout::kGapGroup);

    chartPanel.setBounds(inspectorColumn.removeFromTop(kChartPanelHeight));

    // NOTHING ABOVE THE SKY. The chart starts at the top of the workspace and simply is the
    // field - it has no frame any more, so anything parked over it would read as a lid on it.
    const auto padSize = constellation.getIntrinsicSize();
    constellation.setBoundsCentredIn(area.removeFromTop(padSize.y));
}
