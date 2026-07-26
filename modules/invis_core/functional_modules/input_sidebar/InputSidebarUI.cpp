#include "InputSidebarUI.h"

namespace invis::modules {

InputSidebarUI::InputSidebarUI(juce::AudioProcessorValueTreeState& apvts, const juce::String& paramPrefix)
    : prefix(paramPrefix), apvtsRef(apvts)
{
    // 1. Attachments FIRST so hidden sliders query APVTS NormalisableRanges
    trimAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvtsRef, prefix + "trim", hiddenTrimSlider
    );
    hpfAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvtsRef, prefix + "hpf_freq", hiddenHpfSlider
    );
    lpfAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvtsRef, prefix + "lpf_freq", hiddenLpfSlider
    );

    // 2. Setup Input Meter (size preset comes from the shared sidebar contract, so the input and
    //    output meters are guaranteed identical)
    meter.setNumChannels(2);
    meter.setMeterSize(sidebar::kMeterSize);
    meter.setScalePosition(ui::MeterScalePosition::Right);
    meter.onResetRms      = [this]() { if (onRequestRmsReset) onRequestRmsReset(); };
    meter.onResetPeak     = [this]() { if (onRequestPeakReset) onRequestPeakReset(); };
    meter.onCycleLufsMode = [this]() { if (onRequestLufsModeCycle) onRequestLufsModeCycle(); };
    addAndMakeVisible(meter);

    // 3. Setup Input Trim Knob (Bipolar -24dB .. +24dB, Center origin)
    trimKnob.setKnobSize(sidebar::kKnobSize);
    trimKnob.setLabel("INPUT");
    trimKnob.setValueArcOrigin(ui::ValueArcOrigin::Center);
    trimKnob.setDefaultValue(0.5f); // 0.0 dB
    trimKnob.setScaleTicks({
        { 0.0f, "-24" },
        { 0.25f, "-12" },
        { 0.5f, "0" },
        { 0.75f, "+12" },
        { 1.0f, "+24" }
    });
    trimKnob.setStickyPositions({ 0.5f }); // Sticky detent at 0 dB
    trimKnob.setValueFormatter([this](float) -> juce::String {
        const float db = static_cast<float>(hiddenTrimSlider.getValue());
        return juce::String::formatted("%+.1f dB", db);
    });
    trimKnob.onValueChanged = [this](float val) {
        hiddenTrimSlider.setValue(hiddenTrimSlider.proportionOfLengthToValue(val), juce::sendNotificationSync);
    };

    // 4. Setup HPF Knob (20Hz .. 2kHz, OFF detent at 20Hz)
    hpfKnob.setKnobSize(sidebar::kKnobSize);
    hpfKnob.setLabel("HPF");
    hpfKnob.setIndicatorLampVisible(true); // shows how much energy this filter actually removed
    hpfKnob.setOffPosition(ui::OffPosition::Start);
    hpfKnob.setValueArcOrigin(ui::ValueArcOrigin::Start);
    hpfKnob.setDefaultValue(0.0f);
    hpfKnob.setScaleTicks({
        { 0.0f, "OFF", true },
        { static_cast<float>(hiddenHpfSlider.valueToProportionOfLength(100.0)), "100" },
        { static_cast<float>(hiddenHpfSlider.valueToProportionOfLength(350.0)), "350" },
        { static_cast<float>(hiddenHpfSlider.valueToProportionOfLength(1000.0)), "1k" },
        { 1.0f, "2k" }
    });
    hpfKnob.setStickyPositions({
        static_cast<float>(hiddenHpfSlider.valueToProportionOfLength(100.0)),
        static_cast<float>(hiddenHpfSlider.valueToProportionOfLength(350.0)),
        static_cast<float>(hiddenHpfSlider.valueToProportionOfLength(1000.0))
    });
    hpfKnob.setValueFormatter([this](float val) -> juce::String {
        const float hz = static_cast<float>(hiddenHpfSlider.getValue());
        if (val <= 0.005f || hz <= 20.5f) return "OFF";
        return hz >= 1000.0f ? juce::String::formatted("%.1f kHz", hz / 1000.0f) : juce::String::formatted("%.0f Hz", hz);
    });
    hpfKnob.onValueChanged = [this](float val) {
        if (val <= 0.005f)
        {
            hiddenHpfSlider.setValue(20.0, juce::sendNotificationSync);
            hpfKnob.setOff(true, juce::dontSendNotification);
        }
        else
        {
            hiddenHpfSlider.setValue(hiddenHpfSlider.proportionOfLengthToValue(val), juce::sendNotificationSync);
            hpfKnob.setOff(false, juce::dontSendNotification);
        }
    };

    // 5. Setup LPF Knob (1kHz .. 20kHz, OFF detent at 20kHz)
    lpfKnob.setKnobSize(sidebar::kKnobSize);
    lpfKnob.setLabel("LPF");
    lpfKnob.setIndicatorLampVisible(true);
    lpfKnob.setOffPosition(ui::OffPosition::End);
    lpfKnob.setValueArcOrigin(ui::ValueArcOrigin::End);
    lpfKnob.setDefaultValue(1.0f);
    lpfKnob.setScaleTicks({
        { 0.0f, "1k" },
        { static_cast<float>(hiddenLpfSlider.valueToProportionOfLength(5000.0)), "5k" },
        { static_cast<float>(hiddenLpfSlider.valueToProportionOfLength(12000.0)), "12k" },
        { 1.0f, "OFF", true }
    });
    lpfKnob.setStickyPositions({
        static_cast<float>(hiddenLpfSlider.valueToProportionOfLength(5000.0)),
        static_cast<float>(hiddenLpfSlider.valueToProportionOfLength(12000.0))
    });
    lpfKnob.setValueFormatter([this](float val) -> juce::String {
        const float hz = static_cast<float>(hiddenLpfSlider.getValue());
        if (val >= 0.995f || hz >= 19950.0f) return "OFF";
        return hz >= 1000.0f ? juce::String::formatted("%.1f kHz", hz / 1000.0f) : juce::String::formatted("%.0f Hz", hz);
    });
    lpfKnob.onValueChanged = [this](float val) {
        if (val >= 0.995f)
        {
            hiddenLpfSlider.setValue(20000.0, juce::sendNotificationSync);
            lpfKnob.setOff(true, juce::dontSendNotification);
        }
        else
        {
            hiddenLpfSlider.setValue(hiddenLpfSlider.proportionOfLengthToValue(val), juce::sendNotificationSync);
            lpfKnob.setOff(false, juce::dontSendNotification);
        }
    };

    // 5b. Slope selectors. ComboBoxAttachment does not populate the box, so the items must be
    //     added before attaching or the selection silently never changes.
    for (auto* box : { &hiddenHpfSlopeBox, &hiddenLpfSlopeBox })
        for (int i = 0; i < kNumFilterSlopes; ++i)
            box->addItem(getFilterSlopeName(static_cast<FilterSlope>(i)), i + 1);

    hpfSlopeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        apvtsRef, prefix + "hpf_slope", hiddenHpfSlopeBox);
    lpfSlopeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        apvtsRef, prefix + "lpf_slope", hiddenLpfSlopeBox);

    setupSlopeCell(hpfSlopeCell, hiddenHpfSlopeBox);
    setupSlopeCell(lpfSlopeCell, hiddenLpfSlopeBox);

    hpfLpfSeparator.setSeparatorSize(kInnerSeparator);
    filterTrimSeparator.setSeparatorSize(kStageSeparator);
    addAndMakeVisible(hpfLpfSeparator);
    addAndMakeVisible(filterTrimSeparator);

    // 6. AUTO calibration button: sweeps HPF/LPF to the edge of audibility, then trims the input
    //    onto the gain-stage target.
    autoButton.setButtonSize(ui::InvisButtonSize::XS);
    autoButton.setLabel("AUTO");
    autoButton.setLedPosition(ui::ButtonLedPosition::Left);
    autoButton.setToggleMode(false); // momentary: it starts a process, it is not a state
    autoButton.onClick = [this]() { startAutoCalibration(); };
    autoCalibrator.setRunningChangedCallback([this](bool running) { autoButton.setBlinking(running); });
    autoCalibrator.onRequestMeterReset = [this]() { if (onRequestMeterReset) onRequestMeterReset(); };
    addAndMakeVisible(autoButton);

    addAndMakeVisible(trimKnob);
    addAndMakeVisible(hpfKnob);
    addAndMakeVisible(lpfKnob);

    // PARAMETER -> KNOB. Without this the dials only ever move when the user drags them: host
    // automation, preset recall and A/B/C compare would all change the sound while the panel sat
    // frozen. dontSendNotification on the way back closes the loop safely.
    hiddenTrimSlider.onValueChange = [this]() {
        trimKnob.setValue(static_cast<float>(
            hiddenTrimSlider.valueToProportionOfLength(hiddenTrimSlider.getValue())),
            juce::dontSendNotification);
    };

    hiddenHpfSlider.onValueChange = [this]() {
        const double hz = hiddenHpfSlider.getValue();
        hpfKnob.setValue(static_cast<float>(hiddenHpfSlider.valueToProportionOfLength(hz)),
                         juce::dontSendNotification);
        hpfKnob.setOff(hz <= 20.5, juce::dontSendNotification);
    };

    hiddenLpfSlider.onValueChange = [this]() {
        const double hz = hiddenLpfSlider.getValue();
        lpfKnob.setValue(static_cast<float>(hiddenLpfSlider.valueToProportionOfLength(hz)),
                         juce::dontSendNotification);
        lpfKnob.setOff(hz >= 19950.0, juce::dontSendNotification);
    };

    // Sync initial positions with APVTS
    hiddenTrimSlider.onValueChange();
    hiddenHpfSlider.onValueChange();
    hiddenLpfSlider.onValueChange();
}

void InputSidebarUI::setupSlopeCell(ui::InvisCellSelector& cell, juce::ComboBox& box)
{
    cell.setLabel({});          // the knob above it is the label
    cell.setPopupMode(true);    // eight options is too many to cycle through by clicking
    cell.setValueJustification(juce::Justification::centred);

    cell.onBuildPopup = [&box](juce::PopupMenu& menu) {
        const int active = std::max(0, box.getSelectedItemIndex());

        for (int i = 0; i < kNumFilterSlopes; ++i)
        {
            // The two character slopes are set apart: they are not "more dB", they are a
            // different intent, and burying them in the numeric list hides that.
            if (i == static_cast<int>(FilterSlope::Resonant24))
            {
                menu.addSeparator();
                menu.addSectionHeader("CHARACTER");
            }

            menu.addItem(i + 1, getFilterSlopeName(static_cast<FilterSlope>(i)), true, i == active);
        }
    };

    cell.onPopupResult = [this, &box, &cell](int id) {
        box.setSelectedItemIndex(id - 1, juce::sendNotificationSync);
        cell.setDisplayTextOverride(getFilterSlopeName(static_cast<FilterSlope>(id - 1)));
    };

    box.onChange = [&box, &cell]() {
        cell.setDisplayTextOverride(
            getFilterSlopeName(static_cast<FilterSlope>(std::max(0, box.getSelectedItemIndex()))));
    };
    box.onChange();

    addAndMakeVisible(cell);
}

void InputSidebarUI::updateLevels(float peakL, float peakR)
{
    meter.setLevelsLinear(peakL, peakR);
}

void InputSidebarUI::setReadouts(float rmsDb, float peakDb, float lufsDb)
{
    lastRmsDb = rmsDb;
    meter.setReadouts(rmsDb, peakDb, lufsDb);
}

void InputSidebarUI::updateFilterLamps(float hpfLevel, bool hpfEngaged, float lpfLevel, bool lpfEngaged)
{
    lastHpfRemoved = hpfLevel;
    lastLpfRemoved = lpfLevel;

    hpfKnob.setIndicatorActive(hpfEngaged);
    hpfKnob.setIndicatorLevel(hpfLevel);

    lpfKnob.setIndicatorActive(lpfEngaged);
    lpfKnob.setIndicatorLevel(lpfLevel);
}

void InputSidebarUI::startAutoCalibration()
{
    if (autoCalibrator.isRunning()) return;

    // Every run starts from a clean slate - both filters back to OFF, trim back to unity - but the
    // reset is GLIDED by the calibrator's first phase, not applied here. Snapping the values would
    // click, and the Invis motion standard forbids instantaneous machine-driven recall.
    // Without the reset the routine would sweep out from wherever the last run left things, so a
    // second press could only ever narrow the band further - it could never re-open it.
    // Force the gentlest slope for the duration of the measurement.
    //
    // This is not a convenience, it is a correctness requirement: the resonant slope ADDS energy
    // at the corner, so "how much did this filter remove" would read wrong - possibly negative -
    // and the sweep would park in the wrong place. A steep slope has the opposite problem, biting
    // suddenly instead of gradually, which makes the micro-threshold crossing imprecise.
    // 6 dB/oct is the only honest probe. It is left there afterwards; picking a character slope
    // is a creative decision the user makes AFTER the level has been established.
    hiddenHpfSlopeBox.setSelectedItemIndex(static_cast<int>(FilterSlope::Slope6), juce::sendNotificationSync);
    hiddenLpfSlopeBox.setSelectedItemIndex(static_cast<int>(FilterSlope::Slope6), juce::sendNotificationSync);

    autoCalibrator.start(hpfKnob.getValue(), lpfKnob.getValue(),
                         static_cast<float>(hiddenTrimSlider.getValue()));
}

void InputSidebarUI::tickAnimations(float deltaTimeSeconds)
{
    hpfKnob.updateIndicatorBallistics(deltaTimeSeconds);
    lpfKnob.updateIndicatorBallistics(deltaTimeSeconds);

    if (autoCalibrator.isRunning())
    {
        autoCalibrator.tick(deltaTimeSeconds, lastHpfRemoved, lastLpfRemoved,
                            lastRmsDb, gainStageTargetDb,
                            static_cast<float>(hiddenTrimSlider.getValue()));

        // Drive the knobs through their normal value path so the APVTS write, the OFF-detent
        // handling and the on-screen dial all stay consistent with a manual move.
        if (autoCalibrator.shouldMoveHpf())
        {
            hpfKnob.setOff(false, juce::dontSendNotification);
            hpfKnob.setValue(autoCalibrator.getHpfPosition(), juce::sendNotificationSync);
        }

        if (autoCalibrator.shouldMoveLpf())
        {
            lpfKnob.setOff(false, juce::dontSendNotification);
            lpfKnob.setValue(autoCalibrator.getLpfPosition(), juce::sendNotificationSync);
        }

        // A filter that swept its whole phase without ever biting is returned to OFF rather than
        // left parked at its most destructive extreme.
        if (autoCalibrator.shouldForceHpfOff() && !hpfKnob.isOff())
        {
            hpfKnob.setValue(0.0f, juce::sendNotificationSync);
            hpfKnob.setOff(true, juce::dontSendNotification);
        }

        if (autoCalibrator.shouldForceLpfOff() && !lpfKnob.isOff())
        {
            lpfKnob.setValue(1.0f, juce::sendNotificationSync);
            lpfKnob.setOff(true, juce::dontSendNotification);
        }

        if (autoCalibrator.shouldMoveTrim())
        {
            hiddenTrimSlider.setValue(autoCalibrator.getTrimDb(), juce::sendNotificationSync);
            trimKnob.setValue(static_cast<float>(
                hiddenTrimSlider.valueToProportionOfLength(hiddenTrimSlider.getValue())),
                juce::dontSendNotification);
        }

    }

    autoButton.tickAnimation(deltaTimeSeconds);
}

void InputSidebarUI::setLedPreset(ui::LEDColorPreset preset)
{
    const auto color = ui::getPresetColor(preset);
    trimKnob.setPointerLedColor(color);
    hpfKnob.setPointerLedColor(color);
    lpfKnob.setPointerLedColor(color);
}

void InputSidebarUI::paint(juce::Graphics& g)
{
    const auto theme = ui::InvisThemeSupplier::getParentTheme(this);
    auto bounds = getLocalBounds().toFloat();

    // 1. Carved Sleek High-Tech Panel Background
    g.setColour(juce::Colour::fromRGB(18, 22, 28).withAlpha(0.92f));
    g.fillRoundedRectangle(bounds, ui::layout::kPanelCorner);
    g.setColour(juce::Colours::white.withAlpha(0.08f));
    g.drawRoundedRectangle(bounds, ui::layout::kPanelCorner, ui::layout::kPanelStroke);
}

void InputSidebarUI::resized()
{
    // Design-pixel layout. Every atom is placed at its own intrinsic size and centred in its
    // slot - nothing is ever scaled to fill. Leftover height becomes air, not distortion.
    const int knobH  = ui::InvisKnob::getIntrinsicSize(sidebar::kKnobSize).y;
    const int meterH = ui::InvisLEDMeter::getIntrinsicSize(sidebar::kMeterSize).y;

    auto area = getLocalBounds().reduced(sidebar::kPadding);

    // AUTO heads the strip: it is the command that sets everything below it, so it reads as the
    // section header rather than as one more control in the chain.
    autoButton.setBoundsCentredIn(area.removeFromTop(ui::InvisButton::getIntrinsicSize(
        ui::InvisButtonSize::XS, "AUTO").y));
    area.removeFromTop(ui::layout::kGapGroup);

    // Then SIGNAL FLOW top to bottom: HPF -> LPF -> INPUT trim -> METER.
    // The meter sits last so it shows what actually entered the plugin, after filtering and trim.
    hpfKnob.setBoundsCentredIn(area.removeFromTop(knobH));
    area.removeFromTop(ui::layout::kGapBonded);
    hpfSlopeCell.setBounds(area.removeFromTop(kSlopeCellHeight).withSizeKeepingCentre(kSlopeCellWidth, kSlopeCellHeight));

    // The separator owns its own clearance, so no gap is added around it here.
    hpfLpfSeparator.setBoundsInStrip(area.removeFromTop(hpfLpfSeparator.getIntrinsicThickness()));

    lpfKnob.setBoundsCentredIn(area.removeFromTop(knobH));
    area.removeFromTop(ui::layout::kGapBonded);
    lpfSlopeCell.setBounds(area.removeFromTop(kSlopeCellHeight).withSizeKeepingCentre(kSlopeCellWidth, kSlopeCellHeight));

    filterTrimSeparator.setBoundsInStrip(area.removeFromTop(filterTrimSeparator.getIntrinsicThickness()));

    // LEVEL GROUP, built from the chassis floor upwards: meter at the bottom, trim sitting
    // directly on top of it. Trim and meter are one thought - "set the level, watch the level" -
    // so the slack in the strip belongs ABOVE them, between filtering and levelling, not driven
    // like a wedge between a control and the readout it drives.
    jassert(area.getHeight() >= meterH + knobH + ui::layout::kGapRelated);
    meter.setBoundsCentredIn(area.removeFromBottom(meterH));
    area.removeFromBottom(ui::layout::kGapRelated);
    trimKnob.setBoundsCentredIn(area.removeFromBottom(knobH));
}

} // namespace invis::modules
