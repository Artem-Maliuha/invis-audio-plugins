#pragma once

#include "../../ui_atoms/InvisKnob.h"
#include "../../ui_atoms/InvisLEDMeter.h"
#include "../../ui_atoms/InvisButton.h"
#include "OutputSidebarAutoTrim.h"
#include "../../design_system/InvisThemeSupplier.h"
#include "../InvisSidebarLayout.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

namespace invis::modules {

/**
 * Universal Reusable Output Sidebar UI Container Component.
 * Full-height right vertical column integrating:
 * 1. Output Gain Knob (InvisKnobSize::XS, -48dB .. +12dB)
 * 2. Output Peak Meter (InvisLEDMeter with scale on left)
 */
class OutputSidebarUI : public juce::Component {
public:
    static constexpr int kNumKnobs = 1; // OUTPUT gain

    /** Fixed chassis width shared with InputSidebarUI. Design pixels. */
    static int getIntrinsicWidth() { return sidebar::getIntrinsicWidth(); }
    static int getMinimumHeight()  { return sidebar::getMinimumHeight(kNumKnobs, 1); } // + AUTO row

    OutputSidebarUI(juce::AudioProcessorValueTreeState& apvts, const juce::String& paramPrefix = "out_side_");
    ~OutputSidebarUI() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void updateLevels(float peakL, float peakR);
    void setReadouts(float rmsDb, float peakDb, float lufsDb);

    /** Gain-stage target the AUTO routine trims the output to. Pushed from the top sidebar. */
    void setGainStageTargetDb(float db);
    float getGainStageTargetDb() const { return gainStageTargetDb; }

    /** Advance AUTO / LED animation. Call from the editor's timer. */
    void tickAnimations(float deltaTimeSeconds = 0.016f);

    bool isAutoTrimming() const { return autoTrim.isRunning(); }

    /** Raised when AUTO has just landed a value and the metering integrators should be dropped. */
    std::function<void()> onRequestMeterReset;

    /** Per-readout intents raised by clicking the numbers on the meter. */
    std::function<void()> onRequestRmsReset;
    std::function<void()> onRequestPeakReset;
    std::function<void()> onRequestLufsModeCycle;

    void setLufsLabel(const juce::String& text) { meter.setLufsLabel(text); }
    void setLedPreset(ui::LEDColorPreset preset);

    ui::InvisKnob& getOutputKnob() { return outputKnob; }
    ui::InvisLEDMeter& getMeter() { return meter; }

private:
    juce::String prefix;
    juce::AudioProcessorValueTreeState& apvtsRef;

    ui::InvisKnob outputKnob;
    ui::InvisLEDMeter meter;
    ui::InvisButton autoButton;

    OutputSidebarAutoTrim autoTrim;
    float gainStageTargetDb { -18.0f };
    float lastRmsDb { -100.0f };

    void startAutoTrim();

    juce::Slider hiddenOutputSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outputAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OutputSidebarUI)
};

} // namespace invis::modules
