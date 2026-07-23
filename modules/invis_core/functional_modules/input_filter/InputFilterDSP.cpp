#include "InputFilterDSP.h"

namespace invis::modules {

InputFilterDSP::InputFilterDSP(juce::String paramPrefix)
    : prefix(std::move(paramPrefix))
{
}

void InputFilterDSP::addParameters(juce::AudioProcessorValueTreeState::ParameterLayout& layout,
                                    const juce::String& prefix,
                                    const InputFilterConfig& config)
{
    auto hpfRange = juce::NormalisableRange<float>(20.0f, 2000.0f, 1.0f, 0.35f); // Skewed logarithmic
    auto lpfRange = juce::NormalisableRange<float>(1000.0f, 20000.0f, 1.0f, 0.35f);

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { prefix + "hpf_freq", 1 },
        "High Pass Filter",
        hpfRange,
        config.defaultHpfFreq,
        juce::AudioParameterFloatAttributes().withLabel("Hz")
    ));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { prefix + "lpf_freq", 1 },
        "Low Pass Filter",
        lpfRange,
        config.defaultLpfFreq,
        juce::AudioParameterFloatAttributes().withLabel("Hz")
    ));
}

void InputFilterDSP::prepare(const juce::dsp::ProcessSpec& spec)
{
    currentSampleRate = spec.sampleRate;
    filterChain.prepare(spec);
    reset();
}

void InputFilterDSP::reset()
{
    filterChain.reset();
}

void InputFilterDSP::process(juce::AudioBuffer<float>& buffer, juce::AudioProcessorValueTreeState& apvts)
{
    const float hpfFreq = apvts.getRawParameterValue(prefix + "hpf_freq")->load();
    const float lpfFreq = apvts.getRawParameterValue(prefix + "lpf_freq")->load();

    auto hpfCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass(currentSampleRate, hpfFreq, 0.707f);
    auto lpfCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass(currentSampleRate, lpfFreq, 0.707f);

    *filterChain.get<0>().coefficients = *hpfCoeffs;
    *filterChain.get<1>().coefficients = *lpfCoeffs;

    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> context(block);
    filterChain.process(context);
}

} // namespace invis::modules
