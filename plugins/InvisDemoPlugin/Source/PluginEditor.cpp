#include "PluginProcessor.h"
#include "PluginEditor.h"

InvisDemoPluginEditor::InvisDemoPluginEditor(InvisDemoPluginProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p), inputFilterUI(p.apvts, "in_filter_")
{
    // Mandatory Requirement 1: Resizable UI with Aspect Ratio Preservation
    setResizable(true, true);
    setResizeLimits(750, 420, 1500, 840);
    getConstrainer()->setFixedAspectRatio(760.0 / 420.0);
    setSize(760, 420);

    // Load Photorealistic 3D Hybrid Background Asset
    const auto resourceDir = juce::File(__FILE__).getParentDirectory().getChildFile("Resources");
    chassisBgImage = juce::ImageFileFormat::loadFrom(resourceDir.getChildFile("dark_metal_panel.jpg"));

    // Setup LED Peak Meter (Auto-adapts to Mono or Stereo channels, integrated SCALE & CLIP)
    ledMeter.setNumChannels(processorRef.getTotalNumInputChannels() >= 2 ? 2 : 1);
    addAndMakeVisible(ledMeter);
    startTimerHz(60);

    addAndMakeVisible(inputFilterUI);

    // 1. Setup APVTS Attachments
    trimAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, "tb_trim", hiddenTrimSlider
    );
    panAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, "tb_pan", hiddenPanSlider
    );
    outputAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, "tb_output", hiddenOutputSlider
    );

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
    addAndMakeVisible(xsDemoKnob);

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

    // 3. Setup PAN Knob (L Size Preset - Large Control)
    panKnob.setKnobSize(invis::ui::InvisKnobSize::L);
    panKnob.setLabel("PAN");
    panKnob.setValueArcOrigin(invis::ui::ValueArcOrigin::Center);
    panKnob.setDefaultValue(0.5f);
    panKnob.setScaleTicks({
        { 0.0f, "100L" },
        { 0.25f, "50L" },
        { 0.5f, "C" },
        { 0.75f, "50R" },
        { 1.0f, "100R" }
    });
    panKnob.setStickyPositions({ 0.5f }); // Sticky detent at Center
    panKnob.setValueFormatter([this](float val) -> juce::String {
        const float pan = static_cast<float>(hiddenPanSlider.getValue());
        if (std::abs(pan) < 0.5f) return "C";
        if (pan < 0.0f) return juce::String::formatted("%.0fL", std::abs(pan));
        return juce::String::formatted("%.0fR", pan);
    });
    panKnob.onValueChanged = [this](float val) {
        hiddenPanSlider.setValue(hiddenPanSlider.proportionOfLengthToValue(val), juce::sendNotificationSync);
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

    addAndMakeVisible(trimKnob);
    addAndMakeVisible(panKnob);
    addAndMakeVisible(outputKnob);

    // Sync initial knob positions with APVTS
    trimKnob.setValue(static_cast<float>(hiddenTrimSlider.valueToProportionOfLength(hiddenTrimSlider.getValue())));
    panKnob.setValue(static_cast<float>(hiddenPanSlider.valueToProportionOfLength(hiddenPanSlider.getValue())));
    outputKnob.setValue(static_cast<float>(hiddenOutputSlider.valueToProportionOfLength(hiddenOutputSlider.getValue())));

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
        addAndMakeVisible(btn);
    }
}

InvisDemoPluginEditor::~InvisDemoPluginEditor()
{
    stopTimer();
}

void InvisDemoPluginEditor::timerCallback()
{
    const float linL = processorRef.outputMeterL.load(std::memory_order_relaxed);
    const float linR = processorRef.outputMeterR.load(std::memory_order_relaxed);
    ledMeter.setLevelsLinear(linL, linR);
}

void InvisDemoPluginEditor::updateAllLedPresets(invis::ui::LEDColorPreset preset)
{
    const auto color = invis::ui::getPresetColor(preset);
    inputFilterUI.setLedPreset(preset);
    trimKnob.setPointerLedColor(color);
    panKnob.setPointerLedColor(color);
    outputKnob.setPointerLedColor(color);
}

void InvisDemoPluginEditor::paint(juce::Graphics& g)
{
    const auto theme = invis::ui::InvisTheme::getGlobalDefault();
    const auto bounds = getLocalBounds().toFloat();

    // 1. Render Photorealistic 3D Brushed Metal Chassis Background
    if (chassisBgImage.isValid())
    {
        g.drawImage(chassisBgImage, bounds, juce::RectanglePlacement::fillDestination);
        
        // Dark studio vignette overlay
        const auto vignetteGrad = juce::ColourGradient(
            juce::Colours::transparentBlack, bounds.getCentreX(), bounds.getCentreY(),
            juce::Colours::black.withAlpha(0.45f), 0.0f, 0.0f, true
        );
        g.setGradientFill(vignetteGrad);
        g.fillAll();
    }
    else
    {
        g.fillAll(theme.background);
    }

    // Header scaling proportionally with editor height
    auto topArea = bounds;
    const float headerHeight = bounds.getHeight() * 0.11f;
    const float headerFontSize = headerHeight * 0.52f;
    const auto headerRect = topArea.removeFromTop(headerHeight);

    // Engraved Metal Title Frame
    g.setColour(juce::Colours::black.withAlpha(0.45f));
    g.drawText("INVIS AUDIO TEST BENCH", headerRect.translated(0.0f, 1.0f), juce::Justification::centred, true);

    const auto accent = theme.accentPrimary;
    g.setFont(juce::FontOptions("Courier New", headerFontSize, juce::Font::bold));

    // Double Phosphor Glow Title
    g.setColour(accent.withAlpha(0.35f));
    g.drawText("INVIS AUDIO TEST BENCH", headerRect.translated(-0.5f, -0.5f), juce::Justification::centred, true);
    g.setColour(accent.brighter(0.2f));
    g.drawText("INVIS AUDIO TEST BENCH", headerRect, juce::Justification::centred, true);

    // Bottom Color Palette Title
    auto b = bounds;
    const float footerHeight = b.getHeight() * 0.17f;
    auto footerRect = b.removeFromBottom(footerHeight);
    g.setColour(theme.textSecondary.withAlpha(0.7f));
    g.setFont(juce::FontOptions(std::max(9.5f, footerHeight * 0.20f), juce::Font::plain));
    g.drawText("SELECT LED COLOR PRESET:", footerRect.removeFromTop(footerHeight * 0.25f), juce::Justification::centred, true);
}

void InvisDemoPluginEditor::resized()
{
    auto bounds = getLocalBounds().reduced(juce::roundToInt(getWidth() * 0.025f));
    const int headerHeight = juce::roundToInt(getHeight() * 0.10f);
    const int footerHeight = juce::roundToInt(getHeight() * 0.17f);

    bounds.removeFromTop(headerHeight);
    auto footerArea = bounds.removeFromBottom(footerHeight);

    // Layout Main Workspace:
    // 1. Far Left: Proportional ~1.5x Wider LED Meter (with Dual L/R CLIP Lamps)
    const float aspectScale = static_cast<float>(getHeight()) / 420.0f;
    const int meterWidth = juce::roundToInt(58.0f * aspectScale);
    auto meterArea = bounds.removeFromLeft(meterWidth);
    ledMeter.setBounds(meterArea.reduced(2, 4));

    // 2. Center: InputFilterUI (Contains HPF & LPF knobs set to S size preset)
    const int filterWidth = juce::roundToInt(bounds.getWidth() * 0.36f);
    auto leftArea = bounds.removeFromLeft(filterWidth);
    auto rightArea = bounds;

    inputFilterUI.setBounds(leftArea.reduced(3));

    // 3. Right Area: 4 Knobs showcasing XS, M, L, XL size presets (Proportional Column Widths)
    const float totalUnits = 0.90f + 1.0f + 1.15f + 1.45f;
    const float unitW = rightArea.getWidth() / totalUnits;

    xsDemoKnob.setBounds(rightArea.removeFromLeft(juce::roundToInt(unitW * 0.90f)).reduced(1));
    trimKnob.setBounds(rightArea.removeFromLeft(juce::roundToInt(unitW * 1.0f)).reduced(2));
    panKnob.setBounds(rightArea.removeFromLeft(juce::roundToInt(unitW * 1.15f)).reduced(2));
    outputKnob.setBounds(rightArea.removeFromLeft(juce::roundToInt(unitW * 1.45f)).reduced(2));

    // Position Taller LED Color Buttons in footer bar
    footerArea.removeFromTop(juce::roundToInt(footerHeight * 0.22f));
    const int numBtns = colorButtons.size();
    if (numBtns > 0)
    {
        const int btnWidth = juce::roundToInt(footerArea.getWidth() / static_cast<float>(numBtns));
        for (int i = 0; i < numBtns; ++i)
        {
            if (auto* btn = colorButtons[i])
            {
                btn->setBounds(footerArea.removeFromLeft(btnWidth).reduced(2, 1));
            }
        }
    }
}
