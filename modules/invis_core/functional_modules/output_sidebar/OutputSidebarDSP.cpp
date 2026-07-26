#include "OutputSidebarDSP.h"

namespace invis::modules {

void OutputSidebarDSP::addParameters(juce::AudioProcessorValueTreeState::ParameterLayout& layout, const juce::String& paramPrefix)
{
    // Output Gain (-24 dB .. +24 dB, default 0.0 dB)
    //
    // SYMMETRIC by design: a bipolar control whose arc fills from the centre is only honest if
    // unity gain actually SITS at the centre of the range. The former -48..+12 range put 0 dB at
    // 80% of the travel, so the centre-origin arc started from -18 dB and the knob read as if it
    // were mis-zeroed. This also makes the output trim a mirror of the input trim, which is what
    // the shared sidebar frame implies.
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { paramPrefix + "gain", 1 },
        "Output Gain",
        juce::NormalisableRange<float>(-24.0f, 24.0f, 0.1f),
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")
    ));
}

void OutputSidebarDSP::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    numChannels = static_cast<int>(spec.numChannels);
    analyser.prepare(spec);
    reset();
}

void OutputSidebarDSP::reset()
{
    analyser.reset();
    peakL.store(0.0f, std::memory_order_relaxed);
    peakR.store(0.0f, std::memory_order_relaxed);
}

void OutputSidebarDSP::processBlock(juce::AudioBuffer<float>& buffer, juce::AudioProcessorValueTreeState& apvts, const juce::String& paramPrefix, bool bypassed)
{
    const int numSamples = buffer.getNumSamples();
    if (numSamples <= 0) return;

    // 1. Fetch Output Gain Parameter
    const float gainDb = apvts.getRawParameterValue(paramPrefix + "gain")->load();
    const float gainLinear = juce::Decibels::decibelsToGain(gainDb);

    // 2. Apply Output Gain
    if (!bypassed && std::abs(gainLinear - 1.0f) > 0.001f)
    {
        buffer.applyGain(gainLinear);
    }

    // 3. Measure Output Peak Levels
    const float magL = (buffer.getNumChannels() > 0) ? buffer.getMagnitude(0, 0, numSamples) : 0.0f;
    const float magR = (buffer.getNumChannels() > 1) ? buffer.getMagnitude(1, 0, numSamples) : magL;

    peakL.store(magL, std::memory_order_relaxed);
    peakR.store(magR, std::memory_order_relaxed);

    // 4. Numeric readouts (RMS / PEAK / LUFS-S) for the meter
    analyser.process(buffer);
}

} // namespace invis::modules
