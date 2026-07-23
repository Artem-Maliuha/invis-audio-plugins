#include "OversamplingDSP.h"

namespace invis::modules {

OversamplingDSP::OversamplingDSP(juce::String prefix)
    : paramPrefix(std::move(prefix))
{
}

void OversamplingDSP::addParameters(juce::AudioProcessorValueTreeState::ParameterLayout& layout,
                                     const juce::String& prefix)
{
    juce::StringArray factors = { "Off (1x)", "2x", "4x", "8x" };

    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { prefix + "online", 1 },
        "Online Oversampling",
        factors,
        0 // Default Off (1x)
    ));

    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { prefix + "offline", 1 },
        "Offline Oversampling",
        factors,
        0 // Default Off (1x)
    ));
}

void OversamplingDSP::prepare(double sampleRate, int maxBlockSize, int numChannels)
{
    currentSampleRate = sampleRate;
    maxBlock = maxBlockSize;
    channels = numChannels;
    reset();
}

void OversamplingDSP::reset()
{
    if (oversamplingEngine != nullptr)
        oversamplingEngine->reset();
}

void OversamplingDSP::updateEngineIfNeeded(OversamplingFactor newFactor)
{
    if (currentFactor == newFactor && oversamplingEngine != nullptr)
        return;

    currentFactor = newFactor;
    if (currentFactor == OversamplingFactor::Off)
    {
        oversamplingEngine.reset();
        return;
    }

    size_t factorIndex = static_cast<size_t>(currentFactor);
    oversamplingEngine = std::make_unique<juce::dsp::Oversampling<float>>(
        static_cast<size_t>(channels),
        factorIndex,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
        true
    );
    oversamplingEngine->initProcessing(static_cast<size_t>(maxBlock));
}

int OversamplingDSP::getLatencyInSamples() const
{
    if (oversamplingEngine != nullptr)
        return static_cast<int>(oversamplingEngine->getLatencyInSamples());

    return 0;
}

void OversamplingDSP::process(juce::AudioBuffer<float>& buffer,
                            juce::AudioProcessorValueTreeState& apvts,
                            bool isNonRealtime,
                            const std::function<void(juce::dsp::AudioBlock<float>&)>& dspProcessCallback)
{
    const int choiceIndex = isNonRealtime
        ? static_cast<int>(apvts.getRawParameterValue(paramPrefix + "offline")->load())
        : static_cast<int>(apvts.getRawParameterValue(paramPrefix + "online")->load());

    const auto desiredFactor = static_cast<OversamplingFactor>(juce::jlimit(0, 3, choiceIndex));
    updateEngineIfNeeded(desiredFactor);

    juce::dsp::AudioBlock<float> inputBlock(buffer);

    if (currentFactor == OversamplingFactor::Off || oversamplingEngine == nullptr)
    {
        if (dspProcessCallback != nullptr)
            dspProcessCallback(inputBlock);
        return;
    }

    juce::dsp::AudioBlock<float> oversampledBlock = oversamplingEngine->processSamplesUp(inputBlock);

    if (dspProcessCallback != nullptr)
        dspProcessCallback(oversampledBlock);

    oversamplingEngine->processSamplesDown(inputBlock);
}

} // namespace invis::modules
