#include "InputFilterUI.h"

namespace invis::modules {

InputFilterUI::InputFilterUI(juce::AudioProcessorValueTreeState& apvts, const juce::String& prefix)
    : paramPrefix(prefix), apvtsRef(apvts)
{
    // 1. Setup attachments FIRST so hidden sliders receive APVTS NormalisableRange (20..2000 Hz / 1000..20000 Hz)
    hpfAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvtsRef, paramPrefix + "hpf_freq", hiddenHpfSlider
    );

    lpfAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvtsRef, paramPrefix + "lpf_freq", hiddenLpfSlider
    );

    // 2. Setup Knob labels, size presets (S), OFF positions, defaults, arc origins, and scale ticks
    hpfKnob.setKnobSize(ui::InvisKnobSize::S);
    hpfKnob.setLabel("HPF");
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

    lpfKnob.setKnobSize(ui::InvisKnobSize::S);
    lpfKnob.setLabel("LPF");
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

    addAndMakeVisible(hpfKnob);
    addAndMakeVisible(lpfKnob);

    // Custom Value Parsers (supports 'k' suffix like 20k, 1.5k, 500)
    hpfKnob.setValueParser([this](const juce::String& text) -> float {
        const juce::String t = text.trim().toLowerCase();
        float hz = t.initialSectionContainingOnly("0123456789.").getFloatValue();
        if (t.contains("k")) hz *= 1000.0f;
        return static_cast<float>(hiddenHpfSlider.valueToProportionOfLength(hz));
    });

    lpfKnob.setValueParser([this](const juce::String& text) -> float {
        const juce::String t = text.trim().toLowerCase();
        float hz = t.initialSectionContainingOnly("0123456789.").getFloatValue();
        if (t.contains("k")) hz *= 1000.0f;
        return static_cast<float>(hiddenLpfSlider.valueToProportionOfLength(hz));
    });

    // Sync Hidden Sliders -> Custom InvisKnobs
    hiddenHpfSlider.onValueChange = [this]() {
        const float val = static_cast<float>(hiddenHpfSlider.getValue());
        const float norm = static_cast<float>(hiddenHpfSlider.valueToProportionOfLength(val));
        hpfKnob.setValue(norm, juce::dontSendNotification);

        if (val < 1000.0f)
            hpfKnob.setValueText(juce::String(std::round(val)) + " Hz");
        else
            hpfKnob.setValueText(juce::String(val / 1000.0f, 1) + " kHz");
    };

    hiddenLpfSlider.onValueChange = [this]() {
        const float val = static_cast<float>(hiddenLpfSlider.getValue());
        const float norm = static_cast<float>(hiddenLpfSlider.valueToProportionOfLength(val));
        lpfKnob.setValue(norm, juce::dontSendNotification);

        if (val >= 19900.0f)
            lpfKnob.setValueText("20 kHz");
        else if (val < 1000.0f)
            lpfKnob.setValueText(juce::String(std::round(val)) + " Hz");
        else
            lpfKnob.setValueText(juce::String(val / 1000.0f, 1) + " kHz");
    };

    // Sync InvisKnob user drag -> Hidden Sliders (with DAW automation gesture notifications)
    hpfKnob.onDragStarted = [this]() { hiddenHpfSlider.startedDragging(); };
    hpfKnob.onDragEnded   = [this]() { hiddenHpfSlider.stoppedDragging(); };
    hpfKnob.onValueChanged = [this](float norm) {
        const double val = hiddenHpfSlider.proportionOfLengthToValue(norm);
        hiddenHpfSlider.setValue(val, juce::sendNotification);
    };

    lpfKnob.onDragStarted = [this]() { hiddenLpfSlider.startedDragging(); };
    lpfKnob.onDragEnded   = [this]() { hiddenLpfSlider.stoppedDragging(); };
    lpfKnob.onValueChanged = [this](float norm) {
        const double val = hiddenLpfSlider.proportionOfLengthToValue(norm);
        hiddenLpfSlider.setValue(val, juce::sendNotification);
    };

    // Trigger initial sync
    hiddenHpfSlider.onValueChange();
    hiddenLpfSlider.onValueChange();
}

void InputFilterUI::setModuleTheme(const ui::InvisTheme& theme)
{
    moduleTheme = theme;
    repaint();
}

ui::InvisTheme InputFilterUI::getEffectiveTheme() const
{
    if (moduleTheme.has_value())
        return *moduleTheme;

    return ui::InvisThemeSupplier::getParentTheme(this);
}

void InputFilterUI::paint(juce::Graphics& g)
{
    const auto theme = getEffectiveTheme();
    auto bounds = getLocalBounds().toFloat();

    g.setColour(theme.surfacePanel);
    g.fillRoundedRectangle(bounds, theme.cornerRadius);

    g.setColour(theme.knobTrackBg);
    g.drawRoundedRectangle(bounds.reduced(1.0f), theme.cornerRadius, 1.0f);

    // Section title scaling proportionally with component height
    const float titleHeight = std::max(16.0f, bounds.getHeight() * 0.14f);
    const float titleFontSize = std::max(9.0f, titleHeight * 0.65f);

    g.setColour(theme.textSecondary);
    g.setFont(juce::FontOptions(titleFontSize, juce::Font::bold));
    g.drawText("INPUT FILTER", bounds.removeFromTop(titleHeight), juce::Justification::centred, true);
}

void InputFilterUI::resized()
{
    auto bounds = getLocalBounds().reduced(8);
    const int titleHeight = juce::roundToInt(bounds.getHeight() * 0.14f);
    bounds.removeFromTop(titleHeight); // Proportional title offset

    const int knobWidth = bounds.getWidth() / 2;
    hpfKnob.setBounds(bounds.removeFromLeft(knobWidth).reduced(4));
    lpfKnob.setBounds(bounds.reduced(4));
}

} // namespace invis::modules
