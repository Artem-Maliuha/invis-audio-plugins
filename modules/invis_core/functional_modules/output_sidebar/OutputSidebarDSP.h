#pragma once

#include "../InvisLoudnessAnalyser.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>

namespace invis::modules {

/**
 * Reusable Audio Engine (DSP) for OutputSidebar.
 * Handles Output Gain Scaling and Peak Level Metering.
 */
class OutputSidebarDSP {
public:
    OutputSidebarDSP() = default;
    ~OutputSidebarDSP() = default;

    static void addParameters(juce::AudioProcessorValueTreeState::ParameterLayout& layout,
                              const juce::String& paramPrefix = "out_side_");

    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();

    /** @param bypassed  when true the gain is NOT applied, but metering still runs. */
    void processBlock(juce::AudioBuffer<float>& buffer, juce::AudioProcessorValueTreeState& apvts,
                      const juce::String& paramPrefix = "out_side_", bool bypassed = false);

    float getPeakLevelL() const { return peakL.load(std::memory_order_relaxed); }
    float getPeakLevelR() const { return peakR.load(std::memory_order_relaxed); }

    // --- Numeric readouts for InvisLEDMeter ---
    float getRmsDb() const  { return analyser.getRmsDb(); }
    float getPeakDb() const { return analyser.getPeakDb(); }
    float getLufsDb() const { return analyser.getLufsDb(); }

    /** Drop the metering integrators - call after a machine-driven level change. */
    void requestAnalyserReset() { analyser.requestReset(); }

    // Granular, so clicking one readout does not silently clear the others
    void requestRmsReset()  { analyser.requestRmsReset(); }
    void requestPeakReset() { analyser.requestPeakReset(); }

    LufsMode cycleLufsMode() { return analyser.cycleLufsMode(); }
    LufsMode getLufsMode() const { return analyser.getLufsMode(); }

private:
    double sampleRate { 44100.0 };
    int numChannels { 2 };

    InvisLoudnessAnalyser analyser;

    std::atomic<float> peakL { 0.0f };
    std::atomic<float> peakR { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OutputSidebarDSP)
};

} // namespace invis::modules
