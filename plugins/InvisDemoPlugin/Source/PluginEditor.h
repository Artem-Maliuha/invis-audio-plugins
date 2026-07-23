#pragma once

#include "PluginProcessor.h"
#include <invis_core/invis_core.h>

class InvisDemoPluginEditor : public juce::AudioProcessorEditor {
public:
    explicit InvisDemoPluginEditor(InvisDemoPluginProcessor& p);
    ~InvisDemoPluginEditor() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    InvisDemoPluginProcessor& processorRef;

    invis::modules::InputFilterUI inputFilterUI;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InvisDemoPluginEditor)
};
