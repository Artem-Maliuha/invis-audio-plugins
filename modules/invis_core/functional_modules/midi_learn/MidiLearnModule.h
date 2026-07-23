#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <unordered_map>
#include <optional>

namespace invis::modules {

/**
 * Assignable MIDI Learn Module.
 * Binds incoming MIDI CC controllers to APVTS plugin parameters.
 */
class MidiLearnModule {
public:
    MidiLearnModule() = default;

    void startLearning(const juce::String& parameterID);
    void cancelLearning();
    bool isLearning() const { return targetParameterForLearning.has_value(); }

    void bindCcToParameter(int ccNumber, const juce::String& parameterID);
    void unbindCc(int ccNumber);

    void processMidi(const juce::MidiBuffer& midiBuffer, juce::AudioProcessorValueTreeState& apvts);

private:
    std::optional<juce::String> targetParameterForLearning;
    std::unordered_map<int, juce::String> ccToParamMap;
};

} // namespace invis::modules
