#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout InvisDemoPluginProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Add module parameters
    invis::modules::InputSidebarDSP::addParameters(layout, "in_side_");
    invis::modules::OutputSidebarDSP::addParameters(layout, "out_side_");
    invis::modules::InputFilterDSP::addParameters(layout, "in_filter_");
    invis::modules::OversamplingDSP::addParameters(layout, "os_");
    invis::modules::TopSidebarDSP::addParameters(layout, "top_");

    // Add Test Bench parameters (Trim, Pan, Output Gain)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "tb_trim", 1 }, "Trim",
        juce::NormalisableRange<float>(-24.0f, 24.0f, 0.1f), 0.0f
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

    inputSidebarDSP.prepare(spec);
    outputSidebarDSP.prepare(spec);
    inputFilterDSP.prepare(spec);
    oversamplingDSP.prepare(sampleRate, samplesPerBlock, getTotalNumInputChannels());
}

void InvisDemoPluginProcessor::releaseResources()
{
    inputSidebarDSP.reset();
    outputSidebarDSP.reset();
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

    // MASTER BYPASS: every stage still meters, so the panel keeps showing what is passing
    // through - it just stops altering it.
    const bool bypassed = apvts.getRawParameterValue("top_bypass")->load() > 0.5f;

    // 2. Process InputSidebar DSP (Input Trim, HPF, LPF, Input Peak Metering)
    inputSidebarDSP.processBlock(buffer, apvts, "in_side_", bypassed);

    // 3. Oversampled DSP processing (Input Filter)
    if (!bypassed)
    {
        oversamplingDSP.process(buffer, apvts, isNonRealtime(), [this, &buffer](juce::dsp::AudioBlock<float>&) {
            inputFilterDSP.process(buffer, apvts);
        });
    }

    // 3. Fetch Trim, Pan, Output Gain parameters from APVTS
    const float trimDb = apvts.getRawParameterValue("tb_trim")->load();
    const float outputDb = apvts.getRawParameterValue("tb_output")->load();

    const float trimGain = bypassed ? 1.0f : juce::Decibels::decibelsToGain(trimDb);
    const float outputGain = bypassed ? 1.0f : juce::Decibels::decibelsToGain(outputDb);

    // Apply Trim and Output Gain to buffer
    float maxL = 0.0f;
    float maxR = 0.0f;

    for (int s = 0; s < numSamples; ++s)
    {
        if (numChannels >= 2)
        {
            float l = buffer.getSample(0, s) * trimGain * outputGain;
            float r = buffer.getSample(1, s) * trimGain * outputGain;
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

    // 4. Process OutputSidebar DSP (Output Gain & Output Peak Metering)
    outputSidebarDSP.processBlock(buffer, apvts, "out_side_", bypassed);

    outputMeterL.store(outputSidebarDSP.getPeakLevelL(), std::memory_order_relaxed);
    outputMeterR.store(outputSidebarDSP.getPeakLevelR(), std::memory_order_relaxed);
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
