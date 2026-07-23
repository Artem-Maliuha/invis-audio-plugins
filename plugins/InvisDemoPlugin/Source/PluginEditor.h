#pragma once

#include "PluginProcessor.h"
#include <invis_core/invis_core.h>

class InvisDemoPluginEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit InvisDemoPluginEditor(InvisDemoPluginProcessor& p);
    ~InvisDemoPluginEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;

    InvisDemoPluginProcessor& processorRef;

    // Test Bench Controls & Modules
    invis::ui::InvisLEDMeter ledMeter;
    invis::modules::InputFilterUI inputFilterUI;

    invis::ui::InvisKnob xsDemoKnob;
    invis::ui::InvisKnob trimKnob;
    invis::ui::InvisKnob panKnob;
    invis::ui::InvisKnob outputKnob;

    juce::Slider hiddenTrimSlider;
    juce::Slider hiddenPanSlider;
    juce::Slider hiddenOutputSlider;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> trimAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> panAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outputAttachment;

    std::vector<invis::ui::LEDColorPreset> colorPresets {
        invis::ui::LEDColorPreset::NeonCyan,
        invis::ui::LEDColorPreset::WarmAmber,
        invis::ui::LEDColorPreset::ElectricViolet,
        invis::ui::LEDColorPreset::EmeraldPhosphor,
        invis::ui::LEDColorPreset::CrimsonRuby,
        invis::ui::LEDColorPreset::IceWhite,
        invis::ui::LEDColorPreset::CobaltBlue
    };
    juce::OwnedArray<juce::TextButton> colorButtons;

    void updateAllLedPresets(invis::ui::LEDColorPreset preset);

    juce::Image chassisBgImage;
    juce::Image knobCapImage;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InvisDemoPluginEditor)
};
