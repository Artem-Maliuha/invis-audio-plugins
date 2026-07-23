#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <memory>

namespace invis::modules {

enum class OversamplingFactor {
    Off = 0, // 1x
    X2  = 1, // 2x
    X4  = 2, // 4x
    X8  = 3  // 8x
};

class OversamplingDSP {
public:
    explicit OversamplingDSP(juce::String prefix = "os_");

    static void addParameters(juce::AudioProcessorValueTreeState::ParameterLayout& layout,
                              const juce::String& prefix = "os_");

    void prepare(double sampleRate, int maxBlockSize, int numChannels);
    void reset();

    // Process wrapper taking a lambda function for the core DSP processing at oversampled rate
    void process(juce::AudioBuffer<float>& buffer,
                 juce::AudioProcessorValueTreeState& apvts,
                 bool isNonRealtime,
                 const std::function<void(juce::dsp::AudioBlock<float>&)>& dspProcessCallback);

    int getLatencyInSamples() const;

private:
    juce::String paramPrefix;
    double currentSampleRate { 44100.0 };
    int maxBlock { 512 };
    int channels { 2 };

    OversamplingFactor currentFactor { OversamplingFactor::Off };
    std::unique_ptr<juce::dsp::Oversampling<float>> oversamplingEngine;

    void updateEngineIfNeeded(OversamplingFactor newFactor);
};

} // namespace invis::modules
