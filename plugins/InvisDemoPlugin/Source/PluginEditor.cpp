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
    setVisible(false);

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
    if (index < 0) { hide(); return; }

    const auto& node = owner.constellation.getNode(index);

    for (int i = 0; i < kNumEffects; ++i)
        if (node.label == kEffectCatalogue[i].name)
            effectCell.setSelectedIndex(i, juce::dontSendNotification);

    sensitivityKnob.setValue((node.sensitivity + 1.0f) * 0.5f, juce::dontSendNotification);

    setVisible(true);
    toFront(false);
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
    }

    // The hint has to be here: right-drag is fast once you know it, and undiscoverable until then.
    g.setColour(theme.textSecondary.withAlpha(0.45f));
    g.setFont(invis::ui::InvisFonts::getDisplayFont(8.5f, false));
    g.drawText("OR ALT / RIGHT-DRAG ON THE STAR",
               bounds.removeFromBottom(24.0f).reduced(10.0f, 0.0f),
               juce::Justification::centred, false);
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
    : AudioProcessorEditor(&p), processorRef(p), topSidebarUI(p.apvts, "top_", "os_"), inputSidebarUI(p.apvts, "in_side_"), outputSidebarUI(p.apvts, "out_side_"), inputFilterUI(p.apvts, "in_filter_")
{
    // Mandatory Requirement 1: Resizable UI, fixed aspect, pure global zoom.
    // Everything below is laid out inside `canvas` in design pixels; `resized()` only scales it.
    // NOTE: the initial setSize() happens at the END of this constructor. Layout reads each
    // atom's intrinsic size, so it must not run before the atoms have been given their presets.
    addAndMakeVisible(canvas);

    // Setup Reusable Input & Output Sidebars
    canvas.addAndMakeVisible(topSidebarUI);

    // A catalogue with real depth, so the tree is exercised rather than assumed. Storage is not
    // implemented - selecting a preset only reports the path.
    using invis::modules::PresetNode;
    topSidebarUI.setPresetTree(PresetNode::folder("", {
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

    // 1. Setup APVTS Attachments
    trimAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, "tb_trim", hiddenTrimSlider
    );
    outputAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, "tb_output", hiddenOutputSlider
    );

    // MORPH PAD: the polygon effect editor.
    constellation.setPadSize(invis::ui::InvisConstellationSize::L);
    constellation.onWeightsChanged = [this](int observerIndex, const std::vector<float>& w) {
        // Weights are NOT normalised: outside every aura the signal is dry. Once real effects
        // exist these become parallel send levels, one set per channel stream.
        juce::ignoreUnused(observerIndex, w);
    };
    canvas.addAndMakeVisible(constellation);

    addNodeButton.setButtonSize(invis::ui::InvisButtonSize::S);
    addNodeButton.setLabel("+ ADD");
    addNodeButton.setLedVisible(false);   // it starts an action; there is no state to report
    addNodeButton.setToggleMode(false);
    addNodeButton.onClick = [this]() {
        const int slot = nextEffectSlot % kNumEffects;
        if (constellation.addNode(kEffectCatalogue[slot].name, kEffectCatalogue[slot].colour) >= 0)
            ++nextEffectSlot;
    };
    canvas.addAndMakeVisible(addNodeButton);

    randomiseButton.setButtonSize(invis::ui::InvisButtonSize::S);
    randomiseButton.setLabel("RANDOM");
    randomiseButton.setLedVisible(false);
    randomiseButton.setToggleMode(false);
    randomiseButton.onClick = [this]() { constellation.randomise(); };
    canvas.addAndMakeVisible(randomiseButton);

    constellation.onNodeClicked = [this](int index) { starPanel.showFor(index); };
    canvas.addChildComponent(starPanel);

    channelModeCell.setLabel({});
    channelModeCell.setPopupMode(true);
    channelModeCell.setValueJustification(juce::Justification::centred);
    channelModeCell.setItems({ "L+R", "L R", "M+S", "M S" });
    channelModeCell.onIndexChanged = [this](int index, const juce::String&) {
        constellation.setChannelMode(static_cast<invis::ui::ConstellationChannelMode>(index));
    };
    canvas.addAndMakeVisible(channelModeCell);

    // 1. Setup XS Demo Knob (XS Size Preset - Extra Small Sub-control)
    xsDemoKnob.setKnobSize(invis::ui::InvisKnobSize::XS);
    xsDemoKnob.setLabel("DRY/WET");
    xsDemoKnob.setValue(0.75f);
    xsDemoKnob.setScaleTicks({
        { 0.0f, "0%" },
        { 0.5f, "50%" },
        { 1.0f, "100%" }
    });
    xsDemoKnob.setValueFormatter([](float val) -> juce::String {
        return juce::String::formatted("%.0f%%", val * 100.0f);
    });
    canvas.addAndMakeVisible(xsDemoKnob);

    // 2. Setup TRIM Knob (M Size Preset - Medium Standard)
    trimKnob.setKnobSize(invis::ui::InvisKnobSize::M);
    trimKnob.setLabel("TRIM");
    trimKnob.setValueArcOrigin(invis::ui::ValueArcOrigin::Center);
    trimKnob.setDefaultValue(0.5f);
    trimKnob.setScaleTicks({
        { 0.0f, "-24" },
        { 0.25f, "-12" },
        { 0.5f, "0" },
        { 0.75f, "+12" },
        { 1.0f, "+24" }
    });
    trimKnob.setStickyPositions({ 0.5f }); // Sticky detent at 0 dB
    trimKnob.setValueFormatter([this](float val) -> juce::String {
        const float db = static_cast<float>(hiddenTrimSlider.getValue());
        return juce::String::formatted("%+.1f dB", db);
    });
    trimKnob.onValueChanged = [this](float val) {
        hiddenTrimSlider.setValue(hiddenTrimSlider.proportionOfLengthToValue(val), juce::sendNotificationSync);
    };

    // 4. Setup OUTPUT GAIN Knob (XL Size Preset - Extra Large Master Focal Control)
    outputKnob.setKnobSize(invis::ui::InvisKnobSize::XL);
    outputKnob.setLabel("OUTPUT");
    const float zeroDbPos = static_cast<float>(hiddenOutputSlider.valueToProportionOfLength(0.0));
    outputKnob.setValueArcOriginPosition(zeroDbPos); // Fills bi-directionally from 0 dB Unity Gain!
    outputKnob.setDefaultValue(zeroDbPos);
    outputKnob.setScaleTicks({
        { 0.0f, "-48" },
        { static_cast<float>(hiddenOutputSlider.valueToProportionOfLength(-24.0)), "-24" },
        { static_cast<float>(hiddenOutputSlider.valueToProportionOfLength(0.0)), "0" },
        { 1.0f, "+12" }
    });
    outputKnob.setStickyPositions({ static_cast<float>(hiddenOutputSlider.valueToProportionOfLength(0.0)) });
    outputKnob.setValueFormatter([this](float val) -> juce::String {
        const float db = static_cast<float>(hiddenOutputSlider.getValue());
        return juce::String::formatted("%+.1f dB", db);
    });
    outputKnob.onValueChanged = [this](float val) {
        hiddenOutputSlider.setValue(hiddenOutputSlider.proportionOfLengthToValue(val), juce::sendNotificationSync);
    };

    canvas.addAndMakeVisible(trimKnob);
    canvas.addAndMakeVisible(outputKnob);

    // PARAMETER -> KNOB for the bench's own dials. The sidebars grew this path already; these
    // did not, which is why A/B/C recall changed the sound while these knobs sat frozen.
    // Host automation had exactly the same problem.
    hiddenTrimSlider.onValueChange = [this]() {
        trimKnob.setValue(static_cast<float>(
            hiddenTrimSlider.valueToProportionOfLength(hiddenTrimSlider.getValue())),
            juce::dontSendNotification);
    };

    hiddenOutputSlider.onValueChange = [this]() {
        outputKnob.setValue(static_cast<float>(
            hiddenOutputSlider.valueToProportionOfLength(hiddenOutputSlider.getValue())),
            juce::dontSendNotification);
    };

    // Sync initial knob positions with APVTS
    hiddenTrimSlider.onValueChange();
    hiddenOutputSlider.onValueChange();

    // Build interactive LED color preset selector bar
    for (auto preset : colorPresets)
    {
        auto* btn = colorButtons.add(new juce::TextButton(invis::ui::getPresetName(preset)));
        btn->setClickingTogglesState(false);
        btn->setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(16, 20, 26));
        btn->setColour(juce::TextButton::textColourOffId, invis::ui::getPresetColor(preset));

        btn->onClick = [this, preset]() {
            updateAllLedPresets(preset);
        };
        canvas.addAndMakeVisible(btn);
    }

    // Build interactive Chassis Panel Theme selector bar
    struct ThemeItem { PanelTheme theme; const char* name; juce::Colour color; };
    static const ThemeItem kPanelThemes[] = {
        { PanelTheme::DarkSlateCharcoal, "Charcoal Slate", juce::Colour::fromRGB(24, 30, 40) },
        { PanelTheme::ObsidianBlack, "Obsidian Black", juce::Colour::fromRGB(14, 16, 20) },
        { PanelTheme::MidnightIndigo, "Midnight Blue", juce::Colour::fromRGB(18, 36, 68) },
        { PanelTheme::CyberViolet, "Cyber Violet", juce::Colour::fromRGB(44, 26, 68) },
        { PanelTheme::GunmetalSteel, "Gunmetal Steel", juce::Colour::fromRGB(36, 42, 52) }
    };

    for (const auto& item : kPanelThemes)
    {
        auto* btn = panelThemeButtons.add(new juce::TextButton(item.name));
        btn->setClickingTogglesState(false);
        btn->setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(16, 20, 26));
        btn->setColour(juce::TextButton::textColourOffId, item.color.brighter(0.4f));

        const auto themeEnum = item.theme;
        btn->onClick = [this, themeEnum]() {
            currentPanelTheme = themeEnum;
            repaint();
        };
        canvas.addAndMakeVisible(btn);
    }

    // LAST: every child now exists and carries its final size preset, so the first layout pass
    // can read correct intrinsic sizes.
    invis::ui::applyDesignResizeLimits(*this, kDesignWidth, kDesignHeight);
}

InvisDemoPluginEditor::~InvisDemoPluginEditor()
{
    stopTimer();
}

void InvisDemoPluginEditor::timerCallback()
{
    auto& inDsp = processorRef.inputSidebarDSP;
    auto& outDsp = processorRef.outputSidebarDSP;

    inputSidebarUI.updateLevels(inDsp.getPeakLevelL(), inDsp.getPeakLevelR());
    inputSidebarUI.setReadouts(inDsp.getRmsDb(), inDsp.getPeakDb(), inDsp.getLufsDb());

    outputSidebarUI.updateLevels(outDsp.getPeakLevelL(), outDsp.getPeakLevelR());
    outputSidebarUI.setReadouts(outDsp.getRmsDb(), outDsp.getPeakDb(), outDsp.getLufsDb());

    // Filter "energy removed" lamps beside the HPF / LPF labels
    inputSidebarUI.updateFilterLamps(inDsp.getHpfEnergyRemoved(), inDsp.isHpfEngaged(),
                                     inDsp.getLpfEnergyRemoved(), inDsp.isLpfEngaged());

    // Gain-stage target is a global (top sidebar) concern that both AUTO routines consume, and
    // that both meters draw as a reference marker.
    const float gainStageDb = topSidebarUI.getGainStageTargetDb();
    inputSidebarUI.setGainStageTargetDb(gainStageDb);
    outputSidebarUI.setGainStageTargetDb(gainStageDb);

    const float dt = 1.0f / 60.0f;
    inputSidebarUI.tickAnimations(dt);
    constellation.tickAnimation(dt);
    outputSidebarUI.tickAnimations(dt);
    topSidebarUI.tickAnimations(dt);
}

void InvisDemoPluginEditor::updateAllLedPresets(invis::ui::LEDColorPreset preset)
{
    const auto color = invis::ui::getPresetColor(preset);
    inputSidebarUI.setLedPreset(preset);
    outputSidebarUI.setLedPreset(preset);
    inputFilterUI.setLedPreset(preset);
    trimKnob.setPointerLedColor(color);
    outputKnob.setPointerLedColor(color);
}

void InvisDemoPluginEditor::paint(juce::Graphics& g)
{
    // The editor itself only fills the letterbox area; all artwork lives on the scaled canvas.
    g.fillAll(juce::Colours::black);
}

void InvisDemoPluginEditor::paintCanvas(juce::Graphics& g)
{
    const auto theme = invis::ui::InvisTheme::getGlobalDefault();
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

    // Header & Footer areas, in design pixels (identical maths to layoutCanvas()).
    const int sidebarWidth = invis::modules::InputSidebarUI::getIntrinsicWidth();

    auto mainArea = bounds.reduced(static_cast<float>(kOuterMargin));
    mainArea.removeFromTop(static_cast<float>(invis::modules::TopSidebarUI::getIntrinsicHeight()
                                              + invis::ui::layout::kGapM));
    mainArea.removeFromLeft(static_cast<float>(sidebarWidth + invis::ui::layout::kGapM));
    mainArea.removeFromRight(static_cast<float>(sidebarWidth + invis::ui::layout::kGapM));

    const auto headerRect = mainArea.removeFromTop(static_cast<float>(kHeaderHeight));

    // Engraved Metal Title Frame
    const auto accent = theme.accentPrimary;
    g.setFont(juce::FontOptions("Courier New", kHeaderFontSize, juce::Font::bold));

    g.setColour(juce::Colours::black.withAlpha(0.45f));
    g.drawText("INVIS AUDIO TEST BENCH", headerRect.translated(0.0f, 1.0f), juce::Justification::centred, true);

    // Double Phosphor Glow Title
    g.setColour(accent.withAlpha(0.35f));
    g.drawText("INVIS AUDIO TEST BENCH", headerRect.translated(-0.5f, -0.5f), juce::Justification::centred, true);
    g.setColour(accent.brighter(0.2f));
    g.drawText("INVIS AUDIO TEST BENCH", headerRect, juce::Justification::centred, true);

    // Bottom Controls Bar Titles
    auto footerRect = mainArea.removeFromBottom(static_cast<float>(kFooterHeight));

    g.setColour(theme.textSecondary.withAlpha(0.65f));
    g.setFont(juce::FontOptions(kFooterFontSize, juce::Font::plain));

    g.drawText("CHASSIS THEME:", footerRect.removeFromTop(static_cast<float>(kFooterLabelHeight)),
               juce::Justification::centred, true);
    footerRect.removeFromTop(static_cast<float>(kFooterButtonHeight + invis::ui::layout::kGapM));

    g.drawText("LED ACCENT PRESET:", footerRect.removeFromTop(static_cast<float>(kFooterLabelHeight)),
               juce::Justification::centred, true);
}

void InvisDemoPluginEditor::resized()
{
    // The ONLY responsive maths in the whole editor: one zoom factor for the whole tree.
    invis::ui::applyDesignZoom(canvas, kDesignWidth, kDesignHeight, getLocalBounds());
}

void InvisDemoPluginEditor::layoutCanvas()
{
    using namespace invis::ui;
    using namespace invis::modules;

    auto area = canvas.getLocalBounds().reduced(kOuterMargin);

    // 1. TOP SIDEBAR: spans the FULL width above the channel strips. Everything it carries is
    //    plugin-global rather than signal-path, so it outranks the left/right sidebars.
    topSidebarUI.setBounds(area.removeFromTop(TopSidebarUI::getIntrinsicHeight()));
    area.removeFromTop(layout::kGapM);

    // 2. STANDARD CHASSIS FRAME: full-height sidebars at their fixed intrinsic width.
    const int sidebarWidth = InputSidebarUI::getIntrinsicWidth();

    inputSidebarUI.setBounds(area.removeFromLeft(sidebarWidth));
    outputSidebarUI.setBounds(area.removeFromRight(sidebarWidth));
    area.removeFromLeft(layout::kGapM);
    area.removeFromRight(layout::kGapM);

    jassert(area.getHeight() >= InputSidebarUI::getMinimumHeight());

    // 2. CENTER WORKSPACE between header and footer
    area.removeFromTop(kHeaderHeight);
    auto footerArea = area.removeFromBottom(kFooterHeight);

    // 3. Workspace row: filter panel + 4 knobs, each at its intrinsic size, leftover split as
    //    equal gaps. Nothing here is a fraction of the parent.
    const auto filterSize = InputFilterUI::getIntrinsicSize();

    const std::array<juce::Point<int>, 3> knobSizes {
        InvisKnob::getIntrinsicSize(InvisKnobSize::XS),
        InvisKnob::getIntrinsicSize(InvisKnobSize::M),
        InvisKnob::getIntrinsicSize(InvisKnobSize::XL)
    };

    int contentWidth = filterSize.x;
    for (const auto& s : knobSizes) contentWidth += s.x;

    const int numGaps = 1 + static_cast<int>(knobSizes.size()); // between panel and each knob
    const int gap = std::max(layout::kGapS, (area.getWidth() - contentWidth) / numGaps);

    jassert(contentWidth + numGaps * layout::kGapS <= area.getWidth()); // workspace too narrow

    auto row = area;
    inputFilterUI.setBounds(centreIntrinsic(row.removeFromLeft(filterSize.x), filterSize));
    row.removeFromLeft(gap);

    // Morph pad takes the middle of the workspace, with its ADD key directly above it - the key
    // belongs to the pad, so it is bonded to it rather than floating in the row.
    {
        const auto padSize = constellation.getIntrinsicSize();
        const auto btnSize = addNodeButton.getIntrinsicSize();

        auto padColumn = row.removeFromLeft(padSize.x);
        row.removeFromLeft(gap);

        auto stack = padColumn.withSizeKeepingCentre(
            padSize.x, btnSize.y + layout::kGapBonded + padSize.y);

        auto topRow = stack.removeFromTop(btnSize.y);
        channelModeCell.setBounds(topRow.removeFromRight(kChannelCellWidth)
                                        .withSizeKeepingCentre(kChannelCellWidth, btnSize.y));
        topRow.removeFromRight(layout::kGapRelated);

        addNodeButton.setBoundsCentredIn(topRow.removeFromLeft(addNodeButton.getIntrinsicSize().x));
        topRow.removeFromLeft(layout::kGapBonded);
        randomiseButton.setBoundsCentredIn(topRow.removeFromLeft(randomiseButton.getIntrinsicSize().x));


        stack.removeFromTop(layout::kGapBonded);
        const auto padBounds = stack.removeFromTop(padSize.y);
        constellation.setBoundsCentredIn(padBounds);

        // The inspector floats over the chart's bottom-left, where stars are least likely to be
        // hiding: it is a transient panel, so it must not force the workspace to reserve room.
        starPanel.setBounds(padBounds.getX() + layout::kGapRelated,
                            padBounds.getBottom() - kStarPanelHeight - layout::kGapRelated,
                            kStarPanelWidth, kStarPanelHeight);
    }

    invis::ui::InvisKnob* const knobs[] = { &xsDemoKnob, &trimKnob, &outputKnob };
    for (size_t i = 0; i < knobSizes.size(); ++i)
    {
        knobs[i]->setBoundsCentredIn(row.removeFromLeft(knobSizes[i].x));
        row.removeFromLeft(gap);
    }

    // 4. Footer: two labelled button rows at constant heights
    auto layoutButtonRow = [](juce::Rectangle<int> rowArea, juce::OwnedArray<juce::TextButton>& buttons)
    {
        const int count = buttons.size();
        if (count == 0) return;

        const int btnW = rowArea.getWidth() / count;
        for (int i = 0; i < count; ++i)
        {
            if (auto* btn = buttons[i])
                btn->setBounds(rowArea.removeFromLeft(btnW).reduced(2, 0));
        }
    };

    footerArea.removeFromTop(kFooterLabelHeight);
    layoutButtonRow(footerArea.removeFromTop(kFooterButtonHeight), panelThemeButtons);

    footerArea.removeFromTop(layout::kGapM);
    footerArea.removeFromTop(kFooterLabelHeight);
    layoutButtonRow(footerArea.removeFromTop(kFooterButtonHeight), colorButtons);
}
