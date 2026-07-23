#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <invis_core/invis_core.h>

class InvisDemoPluginProcessor : public juce::AudioProcessor {
public:
    InvisDemoPluginProcessor();
    ~InvisDemoPluginProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Invis Demo Plugin"; }

    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Default"; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    invis::modules::MidiLearnModule midiLearn;

    std::atomic<float> outputMeterL { 0.0f };
    std::atomic<float> outputMeterR { 0.0f };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    invis::modules::InputFilterDSP inputFilterDSP;
    invis::modules::OversamplingDSP oversamplingDSP;

    double currentSampleRate { 44100.0 };
    double sinePhase { 0.0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InvisDemoPluginProcessor)
};
