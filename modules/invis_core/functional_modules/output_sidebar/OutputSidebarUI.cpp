#include "OutputSidebarUI.h"

namespace invis::modules {

OutputSidebarUI::OutputSidebarUI(juce::AudioProcessorValueTreeState& apvts, const juce::String& paramPrefix)
    : prefix(paramPrefix), apvtsRef(apvts)
{
    // 1. APVTS Attachment
    outputAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvtsRef, prefix + "gain", hiddenOutputSlider
    );

    // 2. Setup Output Gain Knob (Bipolar -24dB .. +24dB, Center origin)
    //    Unity gain sits at exactly 0.5, so the centre-origin arc starts from 0 dB and the knob
    //    reads as genuinely zeroed at rest. Mirrors the input trim knob.
    outputKnob.setKnobSize(sidebar::kKnobSize);
    outputKnob.setLabel("OUTPUT");
    outputKnob.setValueArcOrigin(ui::ValueArcOrigin::Center);
    outputKnob.setDefaultValue(0.5f); // 0.0 dB
    outputKnob.setScaleTicks({
        { 0.0f, "-24" },
        { 0.25f, "-12" },
        { 0.5f, "0" },
        { 0.75f, "+12" },
        { 1.0f, "+24" }
    });
    outputKnob.setStickyPositions({ 0.5f }); // Sticky detent at 0 dB
    outputKnob.setValueFormatter([this](float) -> juce::String {
        const float db = static_cast<float>(hiddenOutputSlider.getValue());
        return juce::String::formatted("%+.1f dB", db);
    });
    outputKnob.onValueChanged = [this](float val) {
        hiddenOutputSlider.setValue(hiddenOutputSlider.proportionOfLengthToValue(val), juce::sendNotificationSync);
    };
    addAndMakeVisible(outputKnob);

    // 3. Setup Output Peak Meter (scale on left for right sidebar placement).
    //    Same size preset as the input meter - both come from the shared sidebar contract.
    meter.setNumChannels(2);
    meter.setMeterSize(sidebar::kMeterSize);
    meter.setScalePosition(ui::MeterScalePosition::Left);
    meter.onResetRms      = [this]() { if (onRequestRmsReset) onRequestRmsReset(); };
    meter.onResetPeak     = [this]() { if (onRequestPeakReset) onRequestPeakReset(); };
    meter.onCycleLufsMode = [this]() { if (onRequestLufsModeCycle) onRequestLufsModeCycle(); };
    addAndMakeVisible(meter);

    // 4. AUTO output trim: lands the output RMS on the gain-stage target.
    autoButton.setButtonSize(ui::InvisButtonSize::XS);
    autoButton.setLabel("AUTO");
    autoButton.setLedPosition(ui::ButtonLedPosition::Left);
    autoButton.setToggleMode(false);
    autoButton.onClick = [this]() { startAutoTrim(); };
    autoTrim.setRunningChangedCallback([this](bool running) { autoButton.setBlinking(running); });
    autoTrim.onRequestMeterReset = [this]() { if (onRequestMeterReset) onRequestMeterReset(); };
    addAndMakeVisible(autoButton);

    // PARAMETER -> KNOB, so automation / preset recall / A-B compare actually move the dial
    hiddenOutputSlider.onValueChange = [this]() {
        outputKnob.setValue(static_cast<float>(
            hiddenOutputSlider.valueToProportionOfLength(hiddenOutputSlider.getValue())),
            juce::dontSendNotification);
    };

    hiddenOutputSlider.onValueChange();
}

void OutputSidebarUI::updateLevels(float peakL, float peakR)
{
    meter.setLevelsLinear(peakL, peakR);
}

void OutputSidebarUI::setReadouts(float rmsDb, float peakDb, float lufsDb)
{
    lastRmsDb = rmsDb;
    meter.setReadouts(rmsDb, peakDb, lufsDb);
}

void OutputSidebarUI::setGainStageTargetDb(float db)
{
    gainStageTargetDb = db;
    meter.setGainStageMarkerDb(db);
}

void OutputSidebarUI::startAutoTrim()
{
    if (autoTrim.isRunning()) return;

    // The reset back to unity is the routine's FIRST PHASE, glided - not applied here. Snapping
    // would click, and the Invis motion standard forbids instantaneous machine-driven recall.
    autoTrim.start(static_cast<float>(hiddenOutputSlider.getValue()));
}

void OutputSidebarUI::tickAnimations(float deltaTimeSeconds)
{
    if (autoTrim.isRunning())
    {
        autoTrim.tick(deltaTimeSeconds, lastRmsDb, gainStageTargetDb,
                      static_cast<float>(hiddenOutputSlider.getValue()));

        if (autoTrim.shouldMoveGain())
        {
            hiddenOutputSlider.setValue(autoTrim.getGainDb(), juce::sendNotificationSync);
            outputKnob.setValue(static_cast<float>(
                hiddenOutputSlider.valueToProportionOfLength(hiddenOutputSlider.getValue())),
                juce::dontSendNotification);
        }
    }

    autoButton.tickAnimation(deltaTimeSeconds);
}

void OutputSidebarUI::setLedPreset(ui::LEDColorPreset preset)
{
    const auto color = ui::getPresetColor(preset);
    outputKnob.setPointerLedColor(color);
}

void OutputSidebarUI::paint(juce::Graphics& g)
{
    const auto theme = ui::InvisThemeSupplier::getParentTheme(this);
    auto bounds = getLocalBounds().toFloat();

    // 1. Carved Sleek High-Tech Panel Background (Matching InputSidebar)
    g.setColour(juce::Colour::fromRGB(18, 22, 28).withAlpha(0.92f));
    g.fillRoundedRectangle(bounds, ui::layout::kPanelCorner);
    g.setColour(juce::Colours::white.withAlpha(0.08f));
    g.drawRoundedRectangle(bounds, ui::layout::kPanelCorner, ui::layout::kPanelStroke);
}

void OutputSidebarUI::resized()
{
    // Mirrors InputSidebarUI exactly: same padding, same knob slot height, same meter preset.
    const int knobH  = ui::InvisKnob::getIntrinsicSize(sidebar::kKnobSize).y;
    const int meterH = ui::InvisLEDMeter::getIntrinsicSize(sidebar::kMeterSize).y;

    auto area = getLocalBounds().reduced(sidebar::kPadding);

    // AUTO heads the strip, mirroring the input sidebar
    autoButton.setBoundsCentredIn(area.removeFromTop(ui::InvisButton::getIntrinsicSize(
        ui::InvisButtonSize::XS, "AUTO").y));
    area.removeFromTop(ui::layout::kGapGroup);

    // LEVEL GROUP, mirroring the input sidebar: meter on the chassis floor, gain knob sitting
    // directly on top of it, and the slack left above the pair.
    jassert(area.getHeight() >= meterH + knobH + ui::layout::kGapRelated);
    meter.setBoundsCentredIn(area.removeFromBottom(meterH));
    area.removeFromBottom(ui::layout::kGapRelated);
    outputKnob.setBoundsCentredIn(area.removeFromBottom(knobH));
}

} // namespace invis::modules
