#include "ConstellationWorkspace.h"

namespace invis::modules {

namespace {

// THE SERIES CATALOGUE, not the bench's own. Colours used to be picked one at a time here, which
// is how DELAY ended up amber and SATURATE magenta - two effects whose families the industry has
// agreed on for decades, wearing each other's colours. See InvisEffectPalette for why the three
// families sit where they do on the hue wheel.
const std::vector<ui::EffectType>& catalogue() { return ui::getEffectCatalogue(); }

int numEffects() { return static_cast<int>(catalogue().size()); }

/** Grouped by family, each entry carrying its own colour. Used by the sky AND by the inspector. */
void buildEffectMenu(juce::PopupMenu& menu)
{
    auto shown = static_cast<ui::EffectFamily>(-1);

    for (int i = 0; i < numEffects(); ++i)
    {
        const auto& e = catalogue()[static_cast<size_t>(i)];

        if (i == 0 || e.family != shown)
        {
            shown = e.family;
            menu.addSectionHeader(ui::getFamilyName(shown));
        }

        juce::PopupMenu::Item item(e.name);
        item.itemID = i + 1;
        item.colour = e.getColour();
        menu.addItem(item);
    }
}

} // namespace


// ================================ STAR PANEL ==================================================

ConstellationWorkspace::StarPanel::StarPanel(ConstellationWorkspace& o) : owner(o)
{

    std::vector<juce::String> names;
    for (const auto& e : catalogue()) names.push_back(e.name);

    effectCell.setLabel({});
    effectCell.setPopupMode(true);
    effectCell.setValueJustification(juce::Justification::centred);
    effectCell.setItems(names);
    effectCell.setPopupLookAndFeel(&owner.popupLook);

    // THE SAME LIST THE SKY OFFERS. It was a bare list of names here while clicking empty sky gave
    // family headings and a lit dot per entry - the same choice, presented as two different things,
    // in one window. Built once so it cannot drift again.
    effectCell.onBuildPopup = [](juce::PopupMenu& menu) { buildEffectMenu(menu); };
    effectCell.onPopupResult = [this](int id) {
        effectCell.setSelectedIndex(id - 1, juce::sendNotificationSync);
    };

    effectCell.onIndexChanged = [this](int i, const juce::String& name) {
        if (index < 0) return;
        owner.constellation.setNodeLabel(index, name);
        owner.constellation.setNodeColour(index, catalogue()[static_cast<size_t>(i)].getColour());
    };
    addAndMakeVisible(effectCell);

    // 0..100%, resting at half, with the detent there. It used to rest at FULL, so the only place
    // a new star could go was down.
    sensitivityKnob.setKnobSize(ui::InvisKnobSize::XS);
    sensitivityKnob.setLabel("SENS");
    sensitivityKnob.setValueArcOrigin(ui::ValueArcOrigin::Center);
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
    dryWetKnob.setKnobSize(ui::InvisKnobSize::XS);
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

    // THE SAME FILTERS AS THE CHANNEL STRIP, down to the curve.
    //
    // They were a different instrument wearing the same two names: a plain logarithmic sweep here
    // against the strip's skewed range, no detents, no OFF label, different tick marks. Two HPF
    // knobs in one window that disagree about where 350 Hz sits are two knobs you cannot trust,
    // and the number shown here has to be the number the DSP applies - see InvisEffectSlot, which
    // reads the same range.
    const auto& hpfRange = ui::starFilterRange(true);
    const auto& lpfRange = ui::starFilterRange(false);

    hpfKnob.setKnobSize(ui::InvisKnobSize::XS);
    hpfKnob.setLabel("HPF");
    hpfKnob.setOffPosition(ui::OffPosition::Start);
    hpfKnob.setValueArcOrigin(ui::ValueArcOrigin::Start);
    hpfKnob.setDefaultValue(0.0f);
    hpfKnob.setScaleTicks({
        { 0.0f, "OFF", true },
        { hpfRange.convertTo0to1(100.0f), "100" },
        { hpfRange.convertTo0to1(350.0f), "350" },
        { hpfRange.convertTo0to1(1000.0f), "1k" },
        { 1.0f, "2k" }
    });
    hpfKnob.setStickyPositions({ hpfRange.convertTo0to1(100.0f),
                                 hpfRange.convertTo0to1(350.0f),
                                 hpfRange.convertTo0to1(1000.0f) });
    hpfKnob.setValueFormatter([this, hpfRange](float v) -> juce::String {
        if (hpfKnob.isOff() || v <= 0.005f) return "OFF";

        const float hz = hpfRange.convertFrom0to1(v);
        return hz >= 1000.0f ? juce::String::formatted("%.1f kHz", hz / 1000.0f)
                             : juce::String::formatted("%.0f Hz", hz);
    });
    hpfKnob.onValueChanged = [this](float v) {
        hpfKnob.setOff(v <= 0.005f, juce::dontSendNotification);
        if (index >= 0) owner.constellation.setNodeHpf(index, v <= 0.005f ? 0.0f : v);
    };
    addAndMakeVisible(hpfKnob);

    lpfKnob.setKnobSize(ui::InvisKnobSize::XS);
    lpfKnob.setLabel("LPF");
    lpfKnob.setOffPosition(ui::OffPosition::End);
    lpfKnob.setValueArcOrigin(ui::ValueArcOrigin::End);
    lpfKnob.setDefaultValue(1.0f);
    lpfKnob.setScaleTicks({
        { 0.0f, "1k" },
        { lpfRange.convertTo0to1(5000.0f), "5k" },
        { lpfRange.convertTo0to1(12000.0f), "12k" },
        { 1.0f, "OFF", true }
    });
    lpfKnob.setStickyPositions({ lpfRange.convertTo0to1(5000.0f),
                                 lpfRange.convertTo0to1(12000.0f) });
    lpfKnob.setValueFormatter([this, lpfRange](float v) -> juce::String {
        if (lpfKnob.isOff() || v >= 0.995f) return "OFF";

        const float hz = lpfRange.convertFrom0to1(v);
        return hz >= 1000.0f ? juce::String::formatted("%.1f kHz", hz / 1000.0f)
                             : juce::String::formatted("%.0f Hz", hz);
    });
    lpfKnob.onValueChanged = [this](float v) {
        lpfKnob.setOff(v >= 0.995f, juce::dontSendNotification);
        if (index >= 0) owner.constellation.setNodeLpf(index, v >= 0.995f ? 1.0f : v);
    };
    addAndMakeVisible(lpfKnob);

    // A star can be created from three places and could be got rid of from none of them. The chart
    // has no room for a destructive target - everything there is a drag handle - so it lives here,
    // at the bottom of the panel that is already about this one star.
    deleteButton.setButtonSize(ui::InvisButtonSize::XS);
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

void ConstellationWorkspace::StarPanel::refreshFromNode()
{
    if (index < 0) return;

    const auto& node = owner.constellation.getNode(index);

    for (int i = 0; i < numEffects(); ++i)
        if (node.label == catalogue()[static_cast<size_t>(i)].name)
            effectCell.setSelectedIndex(i, juce::dontSendNotification);

    sensitivityKnob.setValue(node.sensitivity, juce::dontSendNotification);
    dryWetKnob.setValue(node.dryWet, juce::dontSendNotification);

    hpfKnob.setValue(node.hpf, juce::dontSendNotification);
    hpfKnob.setOff(node.hpf <= 0.005f, juce::dontSendNotification);

    lpfKnob.setValue(node.lpf, juce::dontSendNotification);
    lpfKnob.setOff(node.lpf >= 0.995f, juce::dontSendNotification);
}

void ConstellationWorkspace::StarPanel::showFor(int starIndex)
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

void ConstellationWorkspace::StarPanel::paintPanelShell(juce::Graphics& g,
                                                      juce::Rectangle<float> bounds,
                                                      const juce::String& heading,
                                                      juce::Colour tint)
{
    const auto theme = ui::InvisTheme::getGlobalDefault();

    // NOW THAT THE SKY HAS NO FRAME, the panels carry the structure. A heading is what makes a
    // rectangle a place rather than a container - and it is the only edge left on this side of the
    // workspace, so it has to be worth drawing.
    g.setColour(juce::Colours::black.withAlpha(0.55f));
    g.fillRoundedRectangle(bounds.translated(0.0f, 2.0f), ui::layout::kPanelCorner);
    g.setColour(juce::Colour::fromRGB(20, 25, 32).withAlpha(0.98f));
    g.fillRoundedRectangle(bounds, ui::layout::kPanelCorner);

    // THE PANEL WEARS THE EFFECT. A star's colour is how you find it on the chart, and the panel
    // that edits it was the one place in the window that did not say which star you were editing -
    // you had to read the heading. A wash from the top settles that before you read anything.
    //
    // Held to a wash rather than a fill: it has to name the star without competing with the chart,
    // which is the only place on this panel where colour carries information.
    if (!tint.isTransparent())
    {
        juce::Graphics::ScopedSaveState clip(g);
        juce::Path shape;
        shape.addRoundedRectangle(bounds, ui::layout::kPanelCorner);
        g.reduceClipRegion(shape);

        g.setGradientFill(juce::ColourGradient(
            tint.withAlpha(0.20f), bounds.getCentreX(), bounds.getY(),
            juce::Colours::transparentBlack, bounds.getCentreX(),
            bounds.getY() + bounds.getHeight() * 0.62f, false));
        g.fillRect(bounds);
    }

    g.setColour(tint.isTransparent() ? juce::Colours::white.withAlpha(0.10f)
                                     : tint.withAlpha(0.28f));
    g.drawRoundedRectangle(bounds, ui::layout::kPanelCorner, 1.0f);

    auto head = bounds.reduced(10.0f, 0.0f).withHeight(kPanelHeadingHeight).translated(0.0f, 7.0f);

    g.setFont(ui::InvisFonts::getDisplayFont(9.5f, false));
    g.setColour(tint.isTransparent() ? theme.textSecondary.withAlpha(0.55f)
                                     : tint.brighter(0.4f).withAlpha(0.85f));
    g.drawText(heading, head, juce::Justification::centredLeft, false);

    const float rule = head.getBottom() + 3.0f;
    g.setColour(tint.isTransparent() ? juce::Colours::white.withAlpha(0.08f)
                                     : tint.withAlpha(0.22f));
    g.drawLine(bounds.getX() + 10.0f, rule, bounds.getRight() - 10.0f, rule, 1.0f);
}

void ConstellationWorkspace::StarPanel::paint(juce::Graphics& g)
{
    const auto theme = ui::InvisTheme::getGlobalDefault();
    auto bounds = getLocalBounds().toFloat();

    paintPanelShell(g, bounds, "STAR SETTINGS",
                    index >= 0 ? owner.constellation.getNode(index).colour
                               : juce::Colours::transparentBlack);

    if (index >= 0)
    {
        // No "STAR 3" heading. The effect selector directly below already names what this is, in
        // the star's own colour, and an index the chart never shows is not an identity anyone can
        // use to find it again.
        //
        // The hint stays: right-drag is fast once you know it, and undiscoverable until then.
        g.setColour(theme.textSecondary.withAlpha(0.45f));
        g.setFont(ui::InvisFonts::getDisplayFont(8.5f, false));
        g.drawText("OR ALT / RIGHT-DRAG ON THE STAR",
                   bounds.removeFromBottom(24.0f).reduced(10.0f, 0.0f),
                   juce::Justification::centred, false);
    }
    else
    {
        g.setColour(theme.textSecondary.withAlpha(0.40f));
        g.setFont(ui::InvisFonts::getDisplayFont(10.0f, false));
        g.drawText("CLICK A STAR", bounds, juce::Justification::centred, false);
    }
}

void ConstellationWorkspace::StarPanel::resized()
{
    using namespace ui;

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

ConstellationWorkspace::EffectPanel::EffectPanel(ConstellationWorkspace& o) : owner(o)
{
    for (int i = 0; i < dsp::kMaxEffectParams; ++i)
    {
        auto& k = paramKnobs[static_cast<size_t>(i)];
        // One size up from the block's controls. These are the ones you actually dial while
        // listening - the block's are set once and left - and the panel has the room now that it
        // is not sharing a column with anything else.
        k.setKnobSize(ui::InvisKnobSize::S);
        // Through the CHART, not straight to the engine. Everything else about a star travels that
        // way, and a control that takes a shortcut is a control whose value is missing from the
        // saved state - which is exactly what happened to these.
        k.onValueChanged = [this, i](float v) {
            if (index < 0) return;
            owner.constellation.setNodeEffectParam(index, i, v);
        };
        addChildComponent(k);
    }
}

void ConstellationWorkspace::EffectPanel::showFor(int starIndex)
{
    index = starIndex;
    numShown = 0;

    if (index >= 0)
    {
        int count = 0;
        const auto* descs = dsp::getAlgorithmParams(
            owner.engine.getStarKind(index), count);

        numShown = juce::jmin(count, dsp::kMaxEffectParams);

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

            k.setValue(owner.constellation.getNode(index).effectParams[static_cast<size_t>(i)],
                       juce::dontSendNotification);

            // THE LAMP GOES WHERE THE MEASUREMENT IS. Only the first knob of an algorithm that
            // reports activity gets one, because only that one has a number behind it - a lamp on
            // a knob whose level is inferred from its own position would say nothing you cannot
            // already see from the pointer, which is exactly why the block's HPF and LPF have none.
            const bool reports = owner.engine.getStarActivity(index) >= 0.0f;
            k.setIndicatorLampVisible(reports && i == 0);
            k.setIndicatorActive(reports && i == 0);
        }
    }

    for (int i = 0; i < dsp::kMaxEffectParams; ++i)
        paramKnobs[static_cast<size_t>(i)].setVisible(i < numShown);

    resized();
    repaint();
}

void ConstellationWorkspace::EffectPanel::tickLamps(float dt)
{
    if (index < 0 || numShown <= 0) return;

    auto& lamp = paramKnobs[0];
    if (!lamp.isIndicatorLampVisible()) return;

    lamp.setIndicatorLevel(juce::jlimit(0.0f, 1.0f, owner.engine.getStarActivity(index)));
    lamp.updateIndicatorBallistics(dt);
}

void ConstellationWorkspace::EffectPanel::resized()
{
    using namespace ui;

    auto area = getLocalBounds().reduced(10);
    area.removeFromTop(kPanelHeadingHeight + layout::kGapRelated);

    const int knobH = InvisKnob::getIntrinsicSize(InvisKnobSize::S).y;

    for (int i = 0; i < numShown; i += 2)
    {
        auto row = area.removeFromTop(knobH);

        paramKnobs[static_cast<size_t>(i)].setBoundsCentredIn(row.removeFromLeft(row.getWidth() / 2));
        if (i + 1 < numShown)
            paramKnobs[static_cast<size_t>(i + 1)].setBoundsCentredIn(row);

        area.removeFromTop(layout::kGapRelated);
    }
}

void ConstellationWorkspace::EffectPanel::paint(juce::Graphics& g)
{
    const auto theme = ui::InvisTheme::getGlobalDefault();
    auto bounds = getLocalBounds().toFloat();

    const bool has = index >= 0;

    StarPanel::paintPanelShell(g, bounds,
                               has ? owner.constellation.getNode(index).label.toUpperCase()
                                   : juce::String("EFFECT"),
                               has ? owner.constellation.getNode(index).colour
                                   : juce::Colours::transparentBlack);

    if (!has)
    {
        g.setColour(theme.textSecondary.withAlpha(0.35f));
        g.setFont(ui::InvisFonts::getDisplayFont(9.5f, false));
        g.drawText("CLICK A STAR", bounds, juce::Justification::centred, false);
    }
}

// ================================ SKY TOOLS ===================================================

ConstellationWorkspace::SkyTools::SkyTools(ConstellationWorkspace& o) : owner(o)
{
    setAlpha(kRestAlpha);

    channelModeCell.setLabel({});
    channelModeCell.setPopupMode(true);
    channelModeCell.setValueJustification(juce::Justification::centred);
    channelModeCell.setItems({ "L+R", "L R", "M+S", "M S" });
    channelModeCell.onIndexChanged = [this](int index, const juce::String&) {
        owner.constellation.setChannelMode(static_cast<ui::ConstellationChannelMode>(index));
    };
    addAndMakeVisible(channelModeCell);

    // TWO WAYS TO ADD, because they are two different intents. One is "I want a phaser"; the
    // other is "I want something on the sky to push around". Pairing the second with a list to
    // read every time is what makes trying things out feel expensive.
    auto key = [this](ui::InvisButton& b, const juce::String& label,
                      std::function<void()> action)
    {
        b.setButtonSize(ui::InvisButtonSize::XS);
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
    // Two shufflers. STARS is the instrument - where they sit and how they are joined. OBSERVER is
    // only where you are standing, which is the fastest way to hear what a chart you already like
    // can do without disturbing it.
    key(shuffleStarsButton,    "STARS",     [this]() { owner.constellation.randomise(); });
    key(shuffleObserverButton, "OBSERVERS", [this]() { owner.constellation.randomiseObservers(); });

    // THREE WAYS TO TAKE THINGS AWAY, because a chart accumulates three different kinds of clutter.
    // UNUSED sweeps the stars nothing reaches - added to try, then stranded when the observer moved
    // on, still costing an effect instance and a vote in every hue mixture while making no sound.
    // RELATIONS keeps every star and drops the wiring, which is how you re-think a figure without
    // rebuilding it. ALL is the fresh sky.
    key(deleteUnusedButton, "UNUSED",    [this]() { owner.constellation.removeUnusedStars(); });
    key(deleteLinksButton,  "RELATIONS", [this]() { owner.constellation.clearLinks(); });

    // No ALL. Wiping the chart in one click is not a tool you reach for while working - it is the
    // one you hit by accident next to two you use constantly - and DELETE ALL STARS is what an
    // empty preset is for.

    // Children included: without this the panel never hears about the cursor arriving on a key.
    addMouseListener(this, true);
    updateAlpha();
    refreshLimit();
}

void ConstellationWorkspace::SkyTools::refreshLimit()
{
    // The keys going quiet is the whole notice here. How much room is left is said calmly at the
    // foot of the chart instead - see InvisConstellation - because a counter in the toolbar turns
    // a generous ceiling into something you feel you are spending.
    const bool room = owner.constellation.getNumNodes()
                    < ui::InvisConstellation::kMaxNodes;

    addStarButton.setEnabled(room);
    addRandomButton.setEnabled(room);
    repaint();
}

int ConstellationWorkspace::SkyTools::getRequiredWidth(int* singleRowWidth) const
{
    const auto font = ui::InvisFonts::getDisplayFont(8.0f, false);
    const auto caption = [&font](const juce::String& t)
    { return juce::GlyphArrangement::getStringWidthInt(font, t) + 4; };

    using namespace ui;

    const int mode = caption("MODE:") + layout::kGapBonded + kChannelCellWidth;

    const int add = caption("ADD:") + layout::kGapBonded
                  + addStarButton.getIntrinsicSize().x + layout::kGapBonded
                  + addRandomButton.getIntrinsicSize().x;

    const int random = caption("RANDOMIZE:") + layout::kGapBonded
                     + shuffleStarsButton.getIntrinsicSize().x + layout::kGapBonded
                     + shuffleObserverButton.getIntrinsicSize().x;

    const int erase = caption("DELETE:") + layout::kGapBonded
                    + deleteUnusedButton.getIntrinsicSize().x + layout::kGapBonded
                    + deleteLinksButton.getIntrinsicSize().x;

    const int single = mode + layout::kGapGroup + add + layout::kGapGroup
                     + random + layout::kGapGroup + erase;

    if (singleRowWidth != nullptr) *singleRowWidth = single;

    // The WIDER of the two rows, since they are right-aligned against the same edge.
    return std::max(mode + layout::kGapGroup + add,
                    random + layout::kGapGroup + erase);
}

void ConstellationWorkspace::SkyTools::paint(juce::Graphics& g)
{
    const auto theme = ui::InvisTheme::getGlobalDefault();

    // NAMES THE GROUPS, because the keys alone do not: "STAR" and "STARS" are one letter apart and
    // do opposite things - one makes a star, the other throws the whole chart in the air.
    //
    // Inline, reading as a sentence: "ADD: STAR RANDOM". A caption stacked above needs its own row
    // of height on a bar that is floating over the instrument, and the eye still has to travel
    // down to the keys to find out what it labels.
    g.setFont(ui::InvisFonts::getDisplayFont(8.0f, false));
    g.setColour(theme.textSecondary.withAlpha(0.55f));

    const auto caption = [&](const juce::String& text, juce::Rectangle<int> at)
    {
        if (!at.isEmpty())
            g.drawText(text, at.toFloat(), juce::Justification::centredRight, false);
    };

    caption("MODE:", modeCaption);
    caption("ADD:", addCaption);
    caption("RANDOMIZE:", randomCaption);
    caption("DELETE:", deleteCaption);
}

void ConstellationWorkspace::SkyTools::resized()
{
    using namespace ui;

    auto full = getLocalBounds();

    const bool oneRow = singleRow;

    auto top = full.removeFromTop(kSkyToolsRowHeight);
    juce::Rectangle<int> second;

    if (!oneRow)
    {
        full.removeFromTop(4);
        second = full.removeFromTop(kSkyToolsRowHeight);
    }

    // ONE RECT WHEN THERE IS ONE ROW. `bottom` used to be a COPY of `top`, so both halves started
    // consuming from the same right edge and drew straight over each other - the layout was
    // correct in two rows and silently overlapping in one.
    auto* area = oneRow ? &top : &second;

    // Right to left: each group places its keys, then claims the caption slot to their left, so
    // the label always ends up beside exactly what it names however the keys are sized.
    const auto captionSlot = [&area](const juce::String& text)
    {
        const auto font = ui::InvisFonts::getDisplayFont(8.0f, false);
        const int width = juce::GlyphArrangement::getStringWidthInt(font, text) + 4;

        return area->removeFromRight(width);
    };

    const auto place = [&area](ui::InvisButton& b)
    { b.setBoundsCentredIn(area->removeFromRight(b.getIntrinsicSize().x)); };

    // BOTTOM ROW: what changes a chart you already have.
    place(deleteLinksButton);
    area->removeFromRight(layout::kGapBonded);
    place(deleteUnusedButton);
    area->removeFromRight(layout::kGapBonded);
    deleteCaption = captionSlot("DELETE:");

    area->removeFromRight(layout::kGapGroup);

    place(shuffleObserverButton);
    area->removeFromRight(layout::kGapBonded);
    place(shuffleStarsButton);
    area->removeFromRight(layout::kGapBonded);
    randomCaption = captionSlot("RANDOMIZE:");

    // What puts something on the sky in the first place. In two rows it starts a new one; in a
    // single row it simply carries on to the left of what was just placed.
    if (oneRow) area->removeFromRight(layout::kGapGroup);
    else        area = &top;

    place(addRandomButton);
    area->removeFromRight(layout::kGapBonded);
    place(addStarButton);
    area->removeFromRight(layout::kGapBonded);
    addCaption = captionSlot("ADD:");

    area->removeFromRight(layout::kGapGroup);

    channelModeCell.setBounds(area->removeFromRight(kChannelCellWidth));
    area->removeFromRight(layout::kGapBonded);
    modeCaption = captionSlot("MODE:");
}


// ==============================================================================================

ConstellationWorkspace::ConstellationWorkspace(dsp::InvisConstellationEngine& e,
                                               juce::AudioProcessorValueTreeState& apvts)
    : engine(e), apvtsRef(apvts)
{

    // THE CHART IS THE ROOM. A fixed square left a band of unused panel under it; the field is the
    // instrument, so it takes every pixel the workspace is not otherwise using.
    constellation.setPadSize(ui::InvisConstellationSize::L);
    // MOVING THE OBSERVER CHANGES THE ROUTE without touching the geometry: a different star becomes
    // the entry, and the whole chain renumbers behind it. Geometry callbacks alone would miss it.
    constellation.onWeightsChanged = [this](int, const std::vector<float>&) { pushChartToEngine(); };
    addAndMakeVisible(constellation);

    constellation.onNodeClicked = [this](int index) { starPanel.showFor(index); };

    // BOTH WAYS. The panel could always write to a star; it could never learn that the star had
    // been dragged, so the two quietly disagreed until you reselected it.
    constellation.onNodeChanged = [this](int index) {
        if (starPanel.index == index) starPanel.showFor(index);
        pushChartToEngine();
        pushChartToState();
    };

    // A/B/C and preset recall replace the whole tree; the chart lives in it, so it comes back too.
    reloadFromState();
    pushChartToEngine();   // an editor opened on an empty chart still has to hand the engine a plan

    // The chart offers the SPOT; which effect lands there is the bench's knowledge, not the atom's.
    constellation.onRequestAddNode = [this](juce::Point<float> at) { chooseEffectThen(at); };
    addAndMakeVisible(starPanel);
    addAndMakeVisible(effectPanel);
    starPanel.showFor(-1);   // prepared and empty until a star is picked

    // Randomising rebuilds the chart, so whatever the inspector was holding is gone with it.
    constellation.onGeometryChanged = [this]() {
        if (starPanel.index >= constellation.getNumNodes()) starPanel.showFor(-1);
        skyTools.refreshLimit();
        pushChartToEngine();
        pushChartToState();
    };

    addAndMakeVisible(skyTools);
}

ConstellationWorkspace::~ConstellationWorkspace() = default;

void ConstellationWorkspace::resized() { layoutWorkspaceContent(); }

void ConstellationWorkspace::tick(float dt)
{
    constellation.tickAnimation(dt);
    effectPanel.tickLamps(dt);
}

void ConstellationWorkspace::pushChartToEngine()
{
    // GUARDED, because seeding a new star writes to the chart, and every one of those writes
    // reports a change that lands back here. Without this a single added star ran the whole push
    // once per parameter it owns.
    if (pushingToEngine) return;

    const juce::ScopedValueSetter<bool> guard(pushingToEngine, true);
    bool seeded = false;


    for (int i = 0; i < constellation.getNumNodes(); ++i)
    {
        const auto& node = constellation.getNode(i);

        if (engine.getStarAlgorithm(i) != node.label)
        {
            // The recipe's defaults are what makes a HALL sound like a hall. Loading them into the
            // MODEL rather than only into the slot is what lets them be saved, compared and shown.
            engine.setStarAlgorithm(i, node.label);

            for (int k = 0; k < ui::ConstellationNode::kMaxStarParams; ++k)
                constellation.setNodeEffectParam(i, k, engine.getStarParam(i, k));

            // ...including the MIX, which is per recipe rather than a global default: see
            // EffectRecipe for why one number cannot be right for a saturator and a reverb alike.
            if (const auto* recipe = dsp::findRecipe(node.label))
                constellation.setNodeDryWet(i, recipe->dryWet);

            seeded = true;
        }

        engine.setStarBlock(i, node.hpf, node.lpf, node.dryWet);

        for (int k = 0; k < ui::ConstellationNode::kMaxStarParams; ++k)
            engine.setStarParam(i, k, node.effectParams[static_cast<size_t>(k)]);
    }

    // ONE PLAN, BOTH STREAMS. In the linked modes the second is never read, but filling it costs
    // nothing and means switching to L R cannot catch the engine holding a stale route.
    dsp::RoutingPlan plan;
    plan.mode = constellation.getChannelMode();
    plan.numStreams = constellation.getNumObservers();

    for (int s = 0; s < 2; ++s)
    {
        const auto stages = constellation.getRoutingStages(s);
        auto& stream = plan.streams[s];

        stream.numStages = juce::jmin(static_cast<int>(stages.stages.size()),
                                      ui::InvisConstellation::kMaxNodes);

        for (int st = 0; st < stream.numStages; ++st)
        {
            const auto& from = stages.stages[static_cast<size_t>(st)];
            auto& to = stream.stages[st];

            to.count = juce::jmin(static_cast<int>(from.size()),
                                  ui::InvisConstellation::kMaxNodes);

            for (int e = 0; e < to.count; ++e)
                to.entries[e] = { from[static_cast<size_t>(e)].star,
                                  from[static_cast<size_t>(e)].gain };
        }
    }

    engine.setPlan(plan);

    // The seeded values are part of the chart now, so they have to reach the state tree too - the
    // callbacks that would normally carry them were the ones just suppressed.
    if (seeded) pushChartToState();
}

void ConstellationWorkspace::pushChartToState()
{
    if (restoringChart) return;

    auto& state = apvtsRef.state;
    const auto type = ui::InvisConstellation::getStateType();

    state.removeChild(state.getChildWithName(type), nullptr);
    state.appendChild(constellation.toValueTree(), nullptr);
}

void ConstellationWorkspace::reloadFromState()
{
    const auto tree = apvtsRef.state.getChildWithName(
        ui::InvisConstellation::getStateType());

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


void ConstellationWorkspace::addRandomStar()
{
    const int slot = juce::Random::getSystemRandom().nextInt(numEffects());
    const auto& choice = catalogue()[static_cast<size_t>(slot)];

    if (const int index = constellation.addNode(choice.name, choice.getColour()); index >= 0)
        starPanel.showFor(index);
}

void ConstellationWorkspace::chooseEffectThen(std::optional<juce::Point<float>> at,
                                             juce::Component* anchor)
{
    juce::PopupMenu menu;

    buildEffectMenu(menu);

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


void ConstellationWorkspace::layoutWorkspaceContent()
{
    using namespace ui;

    auto area = getLocalBounds();

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
    // THE CHART TAKES THE ROOM IT IS ACTUALLY GIVEN. Its design size was a constant, so the chart
    // stayed 560 tall while the workspace grew to whatever the sidebars needed - a band of unused
    // panel underneath, every time. It is asked for its size here, where the size is known.
    if (!area.isEmpty()) constellation.setPadDesignSize({ area.getWidth(), area.getHeight() });

    constellation.setBoundsCentredIn(area);

    // ONE ROW WHEN IT FITS, decided here and TOLD to the row - see SkyTools::setSingleRow. Measured
    // against the chart's own width, so shortening a label or dropping a key collapses the row on
    // its own rather than waiting for somebody to change a constant.
    int single = 0;
    const int twoRow = skyTools.getRequiredWidth(&single);
    const bool oneRow = single + 3 * layout::kGapRelated <= constellation.getWidth();

    skyTools.setSingleRow(oneRow);
    skyTools.setBounds(constellation.getBounds()
                           .removeFromTop((oneRow ? SkyTools::kSkyToolsRowHeight : kSkyToolsHeight)
                                          + 2 * layout::kGapRelated)
                           .removeFromRight((oneRow ? single : twoRow) + 3 * layout::kGapRelated)
                           .reduced(layout::kGapRelated));
    skyTools.toFront(false);
}

} // namespace invis::modules
