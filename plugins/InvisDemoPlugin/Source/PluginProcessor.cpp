#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout InvisDemoPluginProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Add module parameters
    invis::modules::InputFilterDSP::addParameters(layout, "in_filter_");
    invis::modules::OversamplingDSP::addParameters(layout, "os_");

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

    // Oversampled DSP processing (automatically switches Online vs Offline based on isNonRealtime())
    oversamplingDSP.process(buffer, apvts, isNonRealtime(), [this, &buffer](juce::dsp::AudioBlock<float>& block) {
        // Apply input filter DSP
        inputFilterDSP.process(buffer, apvts);
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
