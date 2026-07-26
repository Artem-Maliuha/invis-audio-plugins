#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <invis_core/invis_core.h>

/**
 * CONSTELLATION ROYAL.
 *
 * The chassis at both ends, the constellation engine between them, and nothing else. There is no
 * test generator: this one is fed by whatever the host sends it, which is the difference between a
 * plugin and a bench.
 */
class ConstellationRoyalProcessor : public juce::AudioProcessor {
public:
    ConstellationRoyalProcessor();
    ~ConstellationRoyalProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Constellation Royal"; }

    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }

    // The spatial family rings for a long time at large sizes; the host has to be told so a bounce
    // does not cut the tail off at the last bar.
    double getTailLengthSeconds() const override { return 6.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Default"; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    invis::modules::MidiLearnModule midiLearn;
    invis::modules::InvisChassisDSP chassis;
    invis::dsp::InvisConstellationEngine engine;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ConstellationRoyalProcessor)
};
