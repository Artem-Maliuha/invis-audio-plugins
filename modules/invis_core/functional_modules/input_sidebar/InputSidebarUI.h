#pragma once

#include "../../ui_atoms/InvisKnob.h"
#include "../../ui_atoms/InvisLEDMeter.h"
#include "../../ui_atoms/InvisButton.h"
#include "../../ui_atoms/InvisCellSelector.h"
#include "../../ui_atoms/InvisSeparator.h"
#include "../InvisFilterCascade.h"
#include "../../design_system/InvisThemeSupplier.h"
#include "../InvisSidebarLayout.h"
#include "InputSidebarAutoCalibrator.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

namespace invis::modules {

/**
 * Universal Reusable Input Sidebar UI Container Component.
 * Slim vertical column integrating:
 * 1. Input Peak Meter (InvisLEDMeter)
 * 2. Input Trim Knob (InvisKnob -24dB .. +24dB)
 * 3. HPF High-Pass Filter Knob (InvisKnob 20Hz .. 2kHz, OFF detent at 20Hz)
 * 4. LPF Low-Pass Filter Knob (InvisKnob 1kHz .. 20kHz, OFF detent at 20kHz)
 */
class InputSidebarUI : public juce::Component {
public:
    static constexpr int kNumKnobs = 3; // INPUT trim, HPF, LPF

    // Slope cell, in design pixels. Sits immediately under its filter knob.
    static constexpr int kSlopeCellWidth  = 72;
    static constexpr int kSlopeCellHeight = 20;
    static constexpr ui::InvisSeparatorSize kInnerSeparator = ui::InvisSeparatorSize::S;
    static constexpr ui::InvisSeparatorSize kStageSeparator = ui::InvisSeparatorSize::M;

    /** Fixed chassis width shared with OutputSidebarUI. Design pixels. */
    static int getIntrinsicWidth() { return sidebar::getIntrinsicWidth(); }
    static int getMinimumHeight()
    {
        return sidebar::getMinimumHeight(kNumKnobs, 1)
             + 2 * kSlopeCellHeight
             + ui::InvisSeparator::getIntrinsicThickness(kInnerSeparator)
             + ui::InvisSeparator::getIntrinsicThickness(kStageSeparator);
    }

    InputSidebarUI(juce::AudioProcessorValueTreeState& apvts, const juce::String& paramPrefix = "in_side_");
    ~InputSidebarUI() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void updateLevels(float peakL, float peakR);
    void setReadouts(float rmsDb, float peakDb, float lufsDb);
    void setLedPreset(ui::LEDColorPreset preset);

    /** Gain-stage target the AUTO routine trims the input to. Pushed from the top sidebar. */
    void setGainStageTargetDb(float db) { gainStageTargetDb = db; meter.setGainStageMarkerDb(db); }
    float getGainStageTargetDb() const { return gainStageTargetDb; }

    bool isAutoCalibrating() const { return autoCalibrator.isRunning(); }

    /** Raised when AUTO has just landed a value and the metering integrators should be dropped.
        The host wires this to the DSP - the UI has no business reaching into the audio thread. */
    std::function<void()> onRequestMeterReset;

    /** Per-readout intents raised by clicking the numbers on the meter. */
    std::function<void()> onRequestRmsReset;
    std::function<void()> onRequestPeakReset;
    std::function<void()> onRequestLufsModeCycle;

    /** Caption for the meter's loudness row, pushed back after the mode changes. */
    void setLufsLabel(const juce::String& text) { meter.setLufsLabel(text); }

    /**
     * Drives the HPF / LPF "energy removed" lamps.
     * Levels arrive already normalized 0..1 from InputSidebarDSP - no dB maths in the UI layer.
     */
    void updateFilterLamps(float hpfLevel, bool hpfEngaged, float lpfLevel, bool lpfEngaged);

    /** Advance lamp ballistics one frame. Call from the editor's timer. */
    void tickAnimations(float deltaTimeSeconds = 0.016f);

    ui::InvisKnob& getTrimKnob() { return trimKnob; }
    ui::InvisKnob& getHpfKnob()  { return hpfKnob; }
    ui::InvisKnob& getLpfKnob()  { return lpfKnob; }
    ui::InvisLEDMeter& getMeter() { return meter; }

private:
    juce::String prefix;
    juce::AudioProcessorValueTreeState& apvtsRef;

    ui::InvisLEDMeter meter;
    ui::InvisKnob trimKnob;
    ui::InvisKnob hpfKnob;
    ui::InvisKnob lpfKnob;
    ui::InvisButton autoButton;

    // Slope selectors sit directly UNDER their filter knob: the slope belongs to that filter,
    // and putting it anywhere else would make the pairing something you have to remember.
    ui::InvisCellSelector hpfSlopeCell;
    ui::InvisCellSelector lpfSlopeCell;

    // Dividers marking the boundaries between stages. HPF and LPF are the same concern (band
    // limiting), so the rule between them is lighter than the one before INPUT, which is where
    // the strip stops shaping and starts setting level.
    ui::InvisSeparator hpfLpfSeparator;
    ui::InvisSeparator filterTrimSeparator;

    juce::ComboBox hiddenHpfSlopeBox;
    juce::ComboBox hiddenLpfSlopeBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> hpfSlopeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> lpfSlopeAttachment;

    void setupSlopeCell(ui::InvisCellSelector& cell, juce::ComboBox& box);

    InputSidebarAutoCalibrator autoCalibrator;
    float gainStageTargetDb { -18.0f };

    // Last measurements pushed in from the DSP - the calibrator consumes these each frame
    float lastHpfRemoved { 0.0f };
    float lastLpfRemoved { 0.0f };
    float lastRmsDb { -100.0f };

    void startAutoCalibration();

    juce::Slider hiddenTrimSlider;
    juce::Slider hiddenHpfSlider;
    juce::Slider hiddenLpfSlider;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> trimAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> hpfAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lpfAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InputSidebarUI)
};

} // namespace invis::modules
