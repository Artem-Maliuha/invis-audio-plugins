#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout
ConstellationRoyalProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // The frame, in one call and under fixed prefixes. The chart is not made of parameters - it
    // lives in the state tree, so it travels with presets and compare slots without needing a
    // parameter per star.
    invis::modules::InvisChassisDSP::addParameters(layout);

    return layout;
}

ConstellationRoyalProcessor::ConstellationRoyalProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout())
{
}

void ConstellationRoyalProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels = static_cast<juce::uint32>(getTotalNumInputChannels());

    chassis.prepare(spec);
    engine.prepare(spec);
}

void ConstellationRoyalProcessor::releaseResources()
{
    chassis.reset();
    engine.reset();
}

bool ConstellationRoyalProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    return layouts.getMainInputChannelSet() == out;
}

void ConstellationRoyalProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                               juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    midiLearn.processMidi(midi, apvts);

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear(ch, 0, buffer.getNumSamples());

    // Tempo, so a synced delay lands on the bar. Read every block because a host can change it
    // mid-transport, and the engine ignores it when it has not moved.
    if (auto* head = getPlayHead())
        if (const auto pos = head->getPosition())
            if (const auto bpm = pos->getBpm()) engine.setTempo(*bpm);

    chassis.process(buffer, apvts, isNonRealtime(), [this](juce::AudioBuffer<float>& b)
    {
        engine.process(b);
    });
}

juce::AudioProcessorEditor* ConstellationRoyalProcessor::createEditor()
{
    return new ConstellationRoyalEditor(*this);
}

void ConstellationRoyalProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void ConstellationRoyalProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ConstellationRoyalProcessor();
}
