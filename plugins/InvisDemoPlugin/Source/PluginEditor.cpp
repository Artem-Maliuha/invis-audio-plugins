#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace {

// THE SERIES CATALOGUE, not the bench's own. Colours used to be picked one at a time here, which
// is how DELAY ended up amber and SATURATE magenta - two effects whose families the industry has
// agreed on for decades, wearing each other's colours. See InvisEffectPalette for why the three
// families sit where they do on the hue wheel.
const std::vector<invis::ui::EffectType>& catalogue() { return invis::ui::getEffectCatalogue(); }

int numEffects() { return static_cast<int>(catalogue().size()); }

} // namespace

// ================================ STAR PANEL ==================================================

InvisDemoPluginEditor::StarPanel::StarPanel(InvisDemoPluginEditor& o) : owner(o)
{

    std::vector<juce::String> names;
    for (const auto& e : catalogue()) names.push_back(e.name);

    effectCell.setLabel({});
    effectCell.setPopupMode(true);
    effectCell.setValueJustification(juce::Justification::centred);
    effectCell.setItems(names);
    effectCell.onIndexChanged = [this](int i, const juce::String& name) {
        if (index < 0) return;
        owner.constellation.setNodeLabel(index, name);
        owner.constellation.setNodeColour(index, catalogue()[static_cast<size_t>(i)].getColour());
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

    // A star can be created from three places and could be got rid of from none of them. The chart
    // has no room for a destructive target - everything there is a drag handle - so it lives here,
    // at the bottom of the panel that is already about this one star.
    deleteButton.setButtonSize(invis::ui::InvisButtonSize::XS);
    deleteButton.setLabel("DELETE");
    deleteButton.setLedVisible(false);
    deleteButton.setToggleMode(false);
    deleteButton.setDangerous(true);
    // NO CONFIRMATION. It was added because removing a star also takes its links, which rewires
    // whatever it was part of - but a dialog is the wrong answer to that. It taxes every deletion,
    // including the fourteen you make while trying things out, to insure against the rare one you
    // regret; and a prompt you dismiss by reflex protects nothing anyway. The honest fix for "the
    // chart cannot hand it back" is undo, not a question.
    //
    // The key keeps its warning colour: that costs nothing and is read before the click, not after.
    deleteButton.onClick = [this]() {
        if (index < 0) return;

        const int doomed = index;
        showFor(-1);                       // let go BEFORE the thing goes away
        owner.constellation.removeNode(doomed);
    };
    addAndMakeVisible(deleteButton);

    // Colour picking is out for now: the effect already carries its colour, so the swatch row was
    // a second way to say the same thing - and the one that could disagree with it.
}

void InvisDemoPluginEditor::StarPanel::refreshFromNode()
{
    if (index < 0) return;

    const auto& node = owner.constellation.getNode(index);

    for (int i = 0; i < numEffects(); ++i)
        if (node.label == catalogue()[static_cast<size_t>(i)].name)
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
                     static_cast<juce::Component*>(&lpfKnob),
                     static_cast<juce::Component*>(&deleteButton) })
        c->setVisible(has);

    refreshFromNode();
    repaint();
    owner.effectPanel.showFor(index);
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

    paintPanelShell(g, bounds, "STAR SETTINGS");

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

    // Each knob gets an equal half and sits centred in it, so the pair reads as a pair rather than
    // as two things that happen to be next to each other.
    auto pair = [](juce::Rectangle<int> row, InvisKnob& a, InvisKnob& b)
    {
        a.setBoundsCentredIn(row.removeFromLeft(row.getWidth() / 2));
        b.setBoundsCentredIn(row);
    };

    pair(area.removeFromTop(knobH), sensitivityKnob, dryWetKnob);
    area.removeFromTop(layout::kGapRelated);
    pair(area.removeFromTop(knobH), hpfKnob, lpfKnob);

    // Bottom of the panel, well clear of the controls: the one irreversible key here must not sit
    // in the run of things you reach for while dialling.
    area.removeFromBottom(20);
    deleteButton.setBoundsCentredIn(area.removeFromBottom(deleteButton.getIntrinsicSize().y));
}

// ================================ EFFECT PANEL ================================================

InvisDemoPluginEditor::EffectPanel::EffectPanel(InvisDemoPluginEditor& o) : owner(o)
{
    for (int i = 0; i < invis::dsp::kMaxEffectParams; ++i)
    {
        auto& k = paramKnobs[static_cast<size_t>(i)];
        k.setKnobSize(invis::ui::InvisKnobSize::XS);
        k.onValueChanged = [this, i](float v) {
            if (index < 0) return;
            owner.processorRef.engine.setStarParam(index, i, v);
        };
        addChildComponent(k);
    }
}

void InvisDemoPluginEditor::EffectPanel::showFor(int starIndex)
{
    index = starIndex;
    numShown = 0;

    if (index >= 0)
    {
        int count = 0;
        const auto* descs = invis::dsp::getAlgorithmParams(
            owner.processorRef.engine.getStarKind(index), count);

        numShown = juce::jmin(count, invis::dsp::kMaxEffectParams);

        for (int i = 0; i < numShown; ++i)
        {
            const auto& d = descs[i];
            auto& k = paramKnobs[static_cast<size_t>(i)];

            k.setLabel(d.name);
            k.setScaleTicks({ { 0.0f, {} }, { 1.0f, {} } });

            // The descriptor owns the units, so the readout is right for a new algorithm the day it
            // is written rather than the day somebody remembers to format it.
            k.setValueFormatter([d](float v) {
                return juce::String(d.toPlain(v), d.decimals) + d.suffix;
            });

            k.setValue(owner.processorRef.engine.getStarParam(index, i),
                       juce::dontSendNotification);
        }
    }

    for (int i = 0; i < invis::dsp::kMaxEffectParams; ++i)
        paramKnobs[static_cast<size_t>(i)].setVisible(i < numShown);

    resized();
    repaint();
}

void InvisDemoPluginEditor::EffectPanel::resized()
{
    using namespace invis::ui;

    auto area = getLocalBounds().reduced(10);
    area.removeFromTop(kPanelHeadingHeight + layout::kGapRelated);

    const int knobH = InvisKnob::getIntrinsicSize(InvisKnobSize::XS).y;

    for (int i = 0; i < numShown; i += 2)
    {
        auto row = area.removeFromTop(knobH);

        paramKnobs[static_cast<size_t>(i)].setBoundsCentredIn(row.removeFromLeft(row.getWidth() / 2));
        if (i + 1 < numShown)
            paramKnobs[static_cast<size_t>(i + 1)].setBoundsCentredIn(row);

        area.removeFromTop(layout::kGapRelated);
    }
}

void InvisDemoPluginEditor::EffectPanel::paint(juce::Graphics& g)
{
    const auto theme = invis::ui::InvisTheme::getGlobalDefault();
    auto bounds = getLocalBounds().toFloat();

    const bool has = index >= 0;

    StarPanel::paintPanelShell(g, bounds, has ? owner.constellation.getNode(index).label.toUpperCase()
                                              : juce::String("EFFECT"));

    if (!has)
    {
        g.setColour(theme.textSecondary.withAlpha(0.35f));
        g.setFont(invis::ui::InvisFonts::getDisplayFont(9.5f, false));
        g.drawText("CLICK A STAR", bounds, juce::Justification::centred, false);
    }
}

// ================================ SKY TOOLS ===================================================

InvisDemoPluginEditor::SkyTools::SkyTools(InvisDemoPluginEditor& o) : owner(o)
{
    setAlpha(kRestAlpha);

    channelModeCell.setLabel({});
    channelModeCell.setPopupMode(true);
    channelModeCell.setValueJustification(juce::Justification::centred);
    channelModeCell.setItems({ "L+R", "L R", "M+S", "M S" });
    channelModeCell.onIndexChanged = [this](int index, const juce::String&) {
        owner.constellation.setChannelMode(static_cast<invis::ui::ConstellationChannelMode>(index));
    };
    addAndMakeVisible(channelModeCell);

    // TWO WAYS TO ADD, because they are two different intents. One is "I want a phaser"; the
    // other is "I want something on the sky to push around". Pairing the second with a list to
    // read every time is what makes trying things out feel expensive.
    auto key = [this](invis::ui::InvisButton& b, const juce::String& label,
                      std::function<void()> action)
    {
        b.setButtonSize(invis::ui::InvisButtonSize::XS);
        b.setLabel(label);
        b.setLedVisible(false);
        b.setToggleMode(false);
        b.onClick = std::move(action);
        addAndMakeVisible(b);
    };

    // The captions carry the verb now, so the keys carry only the noun: "ADD: STAR" rather than
    // "+ STAR" sitting under a heading that already said add.
    key(addStarButton,   "STAR",   [this]() { owner.chooseEffectThen({}, &addStarButton); });
    key(addRandomButton, "RANDOM", [this]() { owner.addRandomStar(); });

    // Two shufflers, because they shuffle different things. One rebuilds the INSTRUMENT - where
    // the stars sit and how they are joined; the other only moves WHERE YOU ARE STANDING, which
    // is the fastest way to hear what an instrument you already like can do.
    // Three shufflers, because they roll three different questions. STARS is the instrument -
    // where they sit and how they are joined. SENS is how it is VOICED - how far each one carries.
    // OBSERVER is only where you are standing. Rolling them together meant you could never keep a
    // figure you liked and re-voice just that.
    key(shuffleStarsButton,    "STARS",         [this]() { owner.constellation.randomise(); });
    key(shuffleSensButton,     "SENSITIVITIES", [this]() { owner.constellation.randomiseSensitivities(); });
    key(shuffleObserverButton, "OBSERVERS",     [this]() { owner.constellation.randomiseObservers(); });

    // Children included: without this the panel never hears about the cursor arriving on a key.
    addMouseListener(this, true);
    updateAlpha();
    refreshLimit();
}

void InvisDemoPluginEditor::SkyTools::refreshLimit()
{
    // The keys going quiet is the whole notice here. How much room is left is said calmly at the
    // foot of the chart instead - see InvisConstellation - because a counter in the toolbar turns
    // a generous ceiling into something you feel you are spending.
    const bool room = owner.constellation.getNumNodes()
                    < invis::ui::InvisConstellation::kMaxNodes;

    addStarButton.setEnabled(room);
    addRandomButton.setEnabled(room);
    repaint();
}

int InvisDemoPluginEditor::SkyTools::getRequiredWidth() const
{
    const auto font = invis::ui::InvisFonts::getDisplayFont(8.0f, false);
    const auto caption = [&font](const juce::String& t)
    { return juce::GlyphArrangement::getStringWidthInt(font, t) + 4; };

    using namespace invis::ui;

    const int mode = caption("MODE:") + layout::kGapBonded + kChannelCellWidth;

    const int add = caption("ADD:") + layout::kGapBonded
                  + addStarButton.getIntrinsicSize().x + layout::kGapBonded
                  + addRandomButton.getIntrinsicSize().x;

    const int random = caption("RANDOMIZE:") + layout::kGapBonded
                     + shuffleStarsButton.getIntrinsicSize().x + layout::kGapBonded
                     + shuffleSensButton.getIntrinsicSize().x + layout::kGapBonded
                     + shuffleObserverButton.getIntrinsicSize().x;

    return mode + layout::kGapGroup + add + layout::kGapGroup + random;
}

void InvisDemoPluginEditor::SkyTools::paint(juce::Graphics& g)
{
    const auto theme = invis::ui::InvisTheme::getGlobalDefault();

    // NAMES THE GROUPS, because the keys alone do not: "STAR" and "STARS" are one letter apart and
    // do opposite things - one makes a star, the other throws the whole chart in the air.
    //
    // Inline, reading as a sentence: "ADD: STAR RANDOM". A caption stacked above needs its own row
    // of height on a bar that is floating over the instrument, and the eye still has to travel
    // down to the keys to find out what it labels.
    g.setFont(invis::ui::InvisFonts::getDisplayFont(8.0f, false));
    g.setColour(theme.textSecondary.withAlpha(0.55f));

    const auto caption = [&](const juce::String& text, juce::Rectangle<int> at)
    {
        if (!at.isEmpty())
            g.drawText(text, at.toFloat(), juce::Justification::centredRight, false);
    };

    caption("MODE:", modeCaption);
    caption("ADD:", addCaption);
    caption("RANDOMIZE:", randomCaption);
}

void InvisDemoPluginEditor::SkyTools::resized()
{
    using namespace invis::ui;

    auto area = getLocalBounds();

    // Right to left: each group places its keys, then claims the caption slot to their left, so
    // the label always ends up beside exactly what it names however the keys are sized.
    const auto captionSlot = [&area](const juce::String& text)
    {
        const auto font = invis::ui::InvisFonts::getDisplayFont(8.0f, false);
        const int width = juce::GlyphArrangement::getStringWidthInt(font, text) + 4;

        return area.removeFromRight(width);
    };

    shuffleObserverButton.setBoundsCentredIn(
        area.removeFromRight(shuffleObserverButton.getIntrinsicSize().x));
    area.removeFromRight(layout::kGapBonded);
    shuffleSensButton.setBoundsCentredIn(
        area.removeFromRight(shuffleSensButton.getIntrinsicSize().x));
    area.removeFromRight(layout::kGapBonded);
    shuffleStarsButton.setBoundsCentredIn(
        area.removeFromRight(shuffleStarsButton.getIntrinsicSize().x));
    area.removeFromRight(layout::kGapBonded);
    randomCaption = captionSlot("RANDOMIZE:");

    area.removeFromRight(layout::kGapGroup);

    addRandomButton.setBoundsCentredIn(area.removeFromRight(addRandomButton.getIntrinsicSize().x));
    area.removeFromRight(layout::kGapBonded);
    addStarButton.setBoundsCentredIn(area.removeFromRight(addStarButton.getIntrinsicSize().x));
    area.removeFromRight(layout::kGapBonded);
    addCaption = captionSlot("ADD:");

    area.removeFromRight(layout::kGapGroup);

    channelModeCell.setBounds(area.removeFromRight(kChannelCellWidth));
    area.removeFromRight(layout::kGapBonded);
    modeCaption = captionSlot("MODE:");
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

    // THE CHART IS THE ROOM. A fixed square left a band of unused panel under it; the field is the
    // instrument, so it takes every pixel the workspace is not otherwise using.
    constellation.setPadSize(invis::ui::InvisConstellationSize::L);
    constellation.setPadDesignSize({
        kWorkspaceWidth - kStarPanelWidth - invis::ui::layout::kGapGroup,
        std::max(kWorkspaceHeight, invis::modules::InputSidebarUI::getMinimumHeight()) });
    // MOVING THE OBSERVER CHANGES THE ROUTE without touching the geometry: a different star becomes
    // the entry, and the whole chain renumbers behind it. Geometry callbacks alone would miss it.
    constellation.onWeightsChanged = [this](int, const std::vector<float>&) { pushChartToEngine(); };
    workspace.addAndMakeVisible(constellation);

    constellation.onNodeClicked = [this](int index) { starPanel.showFor(index); };

    // BOTH WAYS. The panel could always write to a star; it could never learn that the star had
    // been dragged, so the two quietly disagreed until you reselected it.
    constellation.onNodeChanged = [this](int index) {
        if (starPanel.index == index) starPanel.showFor(index);
        pushChartToEngine();
        pushChartToState();
    };

    // A/B/C and preset recall replace the whole tree; the chart lives in it, so it comes back too.
    chassis.onStateRestored = [this]() { pullChartFromState(); };
    pullChartFromState();
    pushChartToEngine();   // an editor opened on an empty chart still has to hand the engine a plan

    // The chart offers the SPOT; which effect lands there is the bench's knowledge, not the atom's.
    constellation.onRequestAddNode = [this](juce::Point<float> at) { chooseEffectThen(at); };
    workspace.addAndMakeVisible(starPanel);
    workspace.addAndMakeVisible(effectPanel);
    starPanel.showFor(-1);   // prepared and empty until a star is picked

    // Randomising rebuilds the chart, so whatever the inspector was holding is gone with it.
    constellation.onGeometryChanged = [this]() {
        if (starPanel.index >= constellation.getNumNodes()) starPanel.showFor(-1);
        skyTools.refreshLimit();
        pushChartToEngine();
        pushChartToState();
    };

    workspace.addAndMakeVisible(skyTools);

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

void InvisDemoPluginEditor::pushChartToEngine()
{
    auto& engine = processorRef.engine;

    for (int i = 0; i < constellation.getNumNodes(); ++i)
    {
        const auto& node = constellation.getNode(i);

        if (engine.getStarAlgorithm(i) != node.label)
            engine.setStarAlgorithm(i, node.label);

        engine.setStarBlock(i, node.hpf, node.lpf, node.dryWet);
    }

    // ONE PLAN, BOTH STREAMS. In the linked modes the second is never read, but filling it costs
    // nothing and means switching to L R cannot catch the engine holding a stale route.
    invis::dsp::RoutingPlan plan;
    plan.mode = constellation.getChannelMode();
    plan.numStreams = constellation.getNumObservers();

    for (int s = 0; s < 2; ++s)
    {
        const auto stages = constellation.getRoutingStages(s);
        auto& stream = plan.streams[s];

        stream.numStages = juce::jmin(static_cast<int>(stages.stages.size()),
                                      invis::ui::InvisConstellation::kMaxNodes);

        for (int st = 0; st < stream.numStages; ++st)
        {
            const auto& from = stages.stages[static_cast<size_t>(st)];
            auto& to = stream.stages[st];

            to.count = juce::jmin(static_cast<int>(from.size()),
                                  invis::ui::InvisConstellation::kMaxNodes);

            for (int e = 0; e < to.count; ++e)
                to.entries[e] = { from[static_cast<size_t>(e)].star,
                                  from[static_cast<size_t>(e)].gain };
        }
    }

    engine.setPlan(plan);
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
    pushChartToEngine();

    // The channel mode came back with the chart, so the control that sets it has to come back too
    // - otherwise the observer says M+S while the panel still reads L+R.
    skyTools.channelModeCell.setSelectedIndex(static_cast<int>(constellation.getChannelMode()),
                                                juce::dontSendNotification);
    starPanel.showFor(-1);
}

void InvisDemoPluginEditor::addRandomStar()
{
    const int slot = juce::Random::getSystemRandom().nextInt(numEffects());
    const auto& choice = catalogue()[static_cast<size_t>(slot)];

    if (const int index = constellation.addNode(choice.name, choice.getColour()); index >= 0)
        starPanel.showFor(index);
}

void InvisDemoPluginEditor::chooseEffectThen(std::optional<juce::Point<float>> at,
                                             juce::Component* anchor)
{
    juce::PopupMenu menu;

    // GROUPED BY FAMILY. The catalogue is ordered so the three bands fall out on their own, and a
    // header at each boundary makes the grouping something you read rather than infer from hue.
    invis::ui::EffectFamily shown = static_cast<invis::ui::EffectFamily>(-1);

    for (int i = 0; i < numEffects(); ++i)
    {
        const auto& e = catalogue()[static_cast<size_t>(i)];

        if (i == 0 || e.family != shown)
        {
            shown = e.family;
            menu.addSectionHeader(invis::ui::getFamilyName(shown));
        }

        juce::PopupMenu::Item item(e.name);
        item.itemID = i + 1;
        item.colour = e.getColour();
        menu.addItem(item);
    }

    // UNDER THE CURSOR, where the star is about to appear. A menu anchored to a button somewhere
    // else makes you look away from the spot you just chose, and then look back to find out
    // whether it landed there.
    menu.setLookAndFeel(&popupLook);

    auto options = juce::PopupMenu::Options().withMinimumWidth(132)
                                             .withStandardItemHeight(19);

    if (at.has_value())
    {
        const auto screen = constellation.localPointToGlobal(
            constellation.toPixelsFromNormalized(*at)).roundToInt();

        options = options.withTargetScreenArea({ screen.x, screen.y, 1, 1 });
    }
    else
    {
        // The KEY that asked, not the row it lives in. Anchoring to the row put the list under its
        // left end, which is a different place from the button you actually pressed.
        options = options.withTargetComponent(anchor != nullptr
                                                  ? anchor
                                                  : static_cast<juce::Component*>(&skyTools));
    }

    // Dismissing adds NOTHING. Picking the effect is the act of creating the star, so backing out
    // of the choice has to back out of the creation too.
    menu.showMenuAsync(options, [this, at](int result)
    {
        if (result <= 0 || result > numEffects()) return;

        const auto& choice = catalogue()[static_cast<size_t>(result - 1)];
        const auto colour = choice.getColour();
        const int index = at.has_value() ? constellation.addNodeAt(choice.name, colour, *at)
                                         : constellation.addNode(choice.name, colour);

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

    // The effect takes whatever is left below: its contents are unknown until the algorithms
    // exist, so reserving a fixed height would be guessing at something we do not know yet.
    effectPanel.setBounds(inspectorColumn);

    // NOTHING ABOVE THE SKY. The chart starts at the top of the workspace and simply is the
    // field - its tools float ON it rather than sitting in a row that would read as a lid.
    constellation.setBoundsCentredIn(area);

    skyTools.setBounds(constellation.getBounds()
                           .removeFromTop(kSkyToolsHeight + 2 * layout::kGapRelated)
                           .removeFromRight(skyTools.getRequiredWidth() + 3 * layout::kGapRelated)
                           .reduced(layout::kGapRelated));
    skyTools.toFront(false);
}
