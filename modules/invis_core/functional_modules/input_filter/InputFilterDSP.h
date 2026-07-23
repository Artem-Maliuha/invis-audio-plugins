#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

namespace invis::modules {

struct InputFilterConfig {
    float defaultHpfFreq { 20.0f };
    float defaultLpfFreq { 20000.0f };
};

class InputFilterDSP {
public:
    explicit InputFilterDSP(juce::String paramPrefix = "in_filter_");

    static void addParameters(juce::AudioProcessorValueTreeState::ParameterLayout& layout,
                              const juce::String& prefix = "in_filter_",
                              const InputFilterConfig& config = {});

    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();
    void process(juce::AudioBuffer<float>& buffer, juce::AudioProcessorValueTreeState& apvts);

private:
    juce::String prefix;
    juce::dsp::ProcessorChain<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Filter<float>> filterChain;
    double currentSampleRate { 44100.0 };
};

} // namespace invis::modules
