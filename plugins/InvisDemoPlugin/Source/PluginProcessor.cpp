#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout InvisDemoPluginProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // The frame, in one call and under fixed prefixes. Adding these by hand, four calls with four
    // string literals, is how a control ends up attached to a parameter nothing reads.
    invis::modules::InvisChassisDSP::addParameters(layout);

    // ...and then only what this plugin itself is.
    invis::modules::InputFilterDSP::addParameters(layout, "in_filter_");

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

    chassis.prepare(spec);
    inputFilterDSP.prepare(spec);
}

void InvisDemoPluginProcessor::releaseResources()
{
    chassis.reset();
    inputFilterDSP.reset();
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

    // THE CHASSIS RUNS THE BLOCK. Order, bypass and metering are its contract, not this file's:
    // everything between the ends is all a plugin gets to decide, and it says so by being the only
    // thing passed in.
    chassis.process(buffer, apvts, isNonRealtime(), [this](juce::AudioBuffer<float>& b)
    {
        inputFilterDSP.process(b, apvts);
    });
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
