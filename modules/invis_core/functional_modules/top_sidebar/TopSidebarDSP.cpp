#include "TopSidebarDSP.h"

namespace invis::modules {

void TopSidebarDSP::addParameters(juce::AudioProcessorValueTreeState::ParameterLayout& layout,
                                  const juce::String& paramPrefix)
{
    // 1. Gain Stage target (digital, whole dB steps)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { paramPrefix + "gain_stage", 1 },
        "Gain Stage",
        juce::NormalisableRange<float>(kMinGainStageDb, kMaxGainStageDb, 1.0f),
        kDefaultGainStageDb,
        juce::AudioParameterFloatAttributes().withLabel("dB")
    ));

    // 2. Master Bypass
    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { paramPrefix + "bypass", 1 },
        "Bypass",
        false
    ));
}

void TopSidebarDSP::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    reset();
}

void TopSidebarDSP::reset() {}

} // namespace invis::modules
