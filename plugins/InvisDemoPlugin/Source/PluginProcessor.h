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

    // THE CHASSIS. Input at the front, output at the back, oversampling and bypass between them -
    // identical in every plugin of the series, so it is owned rather than reassembled.
    invis::modules::InvisChassisDSP chassis;

    // WHAT THIS PLUGIN IS. Everything else on this processor is frame.
    invis::dsp::InvisConstellationEngine engine;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    void synthesiseTestTone(juce::AudioBuffer<float>& buffer);
    void chassisAndEngine(juce::AudioBuffer<float>& buffer);

    invis::modules::InputFilterDSP inputFilterDSP;

    double currentSampleRate { 44100.0 };
    double sinePhase { 0.0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InvisDemoPluginProcessor)
};
