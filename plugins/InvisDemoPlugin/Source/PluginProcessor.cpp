#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout InvisDemoPluginProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Add module parameters
    invis::modules::InputFilterDSP::addParameters(layout, "in_filter_");
    invis::modules::OversamplingDSP::addParameters(layout, "os_");

    // Add Test Bench parameters (Trim, Pan, Output Gain)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "tb_trim", 1 }, "Trim",
        juce::NormalisableRange<float>(-24.0f, 24.0f, 0.1f), 0.0f
    ));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "tb_pan", 1 }, "Pan",
        juce::NormalisableRange<float>(-100.0f, 100.0f, 1.0f), 0.0f
    ));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "tb_output", 1 }, "Output Gain",
        juce::NormalisableRange<float>(-48.0f, 12.0f, 0.1f), 0.0f
    ));

    return layout;
}

InvisDemoPluginProcessor::InvisDemoPluginProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input", juce::AudioChannelSet::stereo(), true)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout())
{
}

bool InvisDemoPluginProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;

    return true;
}

void InvisDemoPluginProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels = static_cast<juce::uint32>(getTotalNumInputChannels());

    inputFilterDSP.prepare(spec);
    oversamplingDSP.prepare(sampleRate, samplesPerBlock, getTotalNumInputChannels());
}

void InvisDemoPluginProcessor::releaseResources()
{
    inputFilterDSP.reset();
    oversamplingDSP.reset();
}

void InvisDemoPluginProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    // Process MIDI Learn
    midiLearn.processMidi(midiMessages, apvts);

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    // 1. Synthesize 100 Hz Sine Wave Generator Signal into Audio Buffer
    const float freq = 100.0f;
    const double phaseDelta = (freq * juce::MathConstants<double>::twoPi) / currentSampleRate;

    for (int s = 0; s < numSamples; ++s)
    {
        const float sampleVal = static_cast<float>(std::sin(sinePhase)) * 0.50f; // -6 dB sine
        sinePhase += phaseDelta;
        if (sinePhase >= juce::MathConstants<double>::twoPi)
            sinePhase -= juce::MathConstants<double>::twoPi;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            buffer.setSample(ch, s, sampleVal);
        }
    }

    // 2. Oversampled DSP processing (Input Filter)
    oversamplingDSP.process(buffer, apvts, isNonRealtime(), [this, &buffer](juce::dsp::AudioBlock<float>& block) {
        inputFilterDSP.process(buffer, apvts);
    });

    // 3. Fetch Trim, Pan, Output Gain parameters from APVTS
    const float trimDb = apvts.getRawParameterValue("tb_trim")->load();
    const float panVal = apvts.getRawParameterValue("tb_pan")->load();
    const float outputDb = apvts.getRawParameterValue("tb_output")->load();

    const float trimGain = juce::Decibels::decibelsToGain(trimDb);
    const float outputGain = juce::Decibels::decibelsToGain(outputDb);

    // Constant-power Panning Coefficients (-3 dB center)
    const float panNorm = (panVal + 100.0f) / 200.0f; // 0.0 .. 1.0
    const float panAngle = panNorm * juce::MathConstants<float>::halfPi;
    const float leftPanGain = std::cos(panAngle);
    const float rightPanGain = std::sin(panAngle);

    // Apply Trim, Pan, and Output Gain to buffer
    float maxL = 0.0f;
    float maxR = 0.0f;

    for (int s = 0; s < numSamples; ++s)
    {
        if (numChannels >= 2)
        {
            float l = buffer.getSample(0, s) * trimGain * leftPanGain * outputGain;
            float r = buffer.getSample(1, s) * trimGain * rightPanGain * outputGain;
            buffer.setSample(0, s, l);
            buffer.setSample(1, s, r);
            maxL = std::max(maxL, std::abs(l));
            maxR = std::max(maxR, std::abs(r));
        }
        else if (numChannels == 1)
        {
            float m = buffer.getSample(0, s) * trimGain * outputGain;
            buffer.setSample(0, s, m);
            maxL = std::max(maxL, std::abs(m));
            maxR = maxL;
        }
    }

    // 4. Update atomic meter levels for UI (measured post Output Gain)
    outputMeterL.store(maxL, std::memory_order_relaxed);
    outputMeterR.store(maxR, std::memory_order_relaxed);
}

juce::AudioProcessorEditor* InvisDemoPluginProcessor::createEditor()
{
    return new InvisDemoPluginEditor(*this);
}

void InvisDemoPluginProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void InvisDemoPluginProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new InvisDemoPluginProcessor();
}
