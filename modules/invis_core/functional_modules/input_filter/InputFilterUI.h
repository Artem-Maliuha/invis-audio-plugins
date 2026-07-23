#pragma once

#include "../../ui_atoms/InvisKnob.h"
#include "../../design_system/InvisThemeSupplier.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace invis::modules {

/**
 * Functional Module Container UI for InputFilter.
 * Binds 2 InvisKnobs to APVTS parameters, handles DAW gesture state, and responsive layout.
 */
class InputFilterUI : public juce::Component, public ui::InvisThemeSupplier {
public:
    InputFilterUI(juce::AudioProcessorValueTreeState& apvts, const juce::String& prefix = "in_filter_");
    ~InputFilterUI() override = default;

    void setModuleTheme(const ui::InvisTheme& theme);
    ui::InvisTheme getEffectiveTheme() const override;

    void setLedPreset(ui::LEDColorPreset preset)
    {
        const auto color = ui::getPresetColor(preset);
        hpfKnob.setPointerLedColor(color);
        lpfKnob.setPointerLedColor(color);
    }

    void setKnobCapImage(const juce::Image& image)
    {
        hpfKnob.setKnobCapImage(image);
        lpfKnob.setKnobCapImage(image);
    }

    void setFilmstrip(const juce::Image& spriteSheetImage, int numFrames, bool isVertical = true)
    {
        hpfKnob.setFilmstrip(spriteSheetImage, numFrames, isVertical);
        lpfKnob.setFilmstrip(spriteSheetImage, numFrames, isVertical);
    }

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    juce::String paramPrefix;
    juce::AudioProcessorValueTreeState& apvtsRef;

    ui::InvisKnob hpfKnob;
    ui::InvisKnob lpfKnob;

    juce::Slider hiddenHpfSlider;
    juce::Slider hiddenLpfSlider;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> hpfAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lpfAttachment;

    std::optional<ui::InvisTheme> moduleTheme;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InputFilterUI)
};

} // namespace invis::modules
