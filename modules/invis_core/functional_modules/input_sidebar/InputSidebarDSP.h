#pragma once

#include "../InvisLoudnessAnalyser.h"
#include "../InvisFilterCascade.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <memory>

namespace invis::modules {

/**
 * Reusable Audio Engine (DSP) for InputSidebar.
 * Handles Input Trim Gain, Modern Console HPF/LPF IIR filtering, and Peak Metering.
 */
class InputSidebarDSP {
public:
    InputSidebarDSP() = default;
    ~InputSidebarDSP() = default;

    static void addParameters(juce::AudioProcessorValueTreeState::ParameterLayout& layout,
                              const juce::String& paramPrefix = "in_side_");

    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();

    /**
     * @param bypassed  when true the trim and filters are NOT applied, but metering still runs -
     *                  a bypassed stage must still be able to show you what is passing through it.
     */
    void processBlock(juce::AudioBuffer<float>& buffer, juce::AudioProcessorValueTreeState& apvts,
                      const juce::String& paramPrefix = "in_side_", bool bypassed = false);

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

    // --- Filter "energy removed" indicator lamps ---
    //
    // Normalized 0..1 already: the DSP owns the dB range, so the UI layer carries no dB maths.
    // Each stage is measured across ITSELF only - the LPF's reference tap is the signal AFTER
    // the HPF, otherwise whatever the HPF cut would be double-counted into the LPF's reading
    // whenever both are engaged.
    float getHpfEnergyRemoved() const { return hpfLampLevel.load(std::memory_order_relaxed); }
    float getLpfEnergyRemoved() const { return lpfLampLevel.load(std::memory_order_relaxed); }

    /** Powered state: false = the filter is bypassed, which must read differently from
        "engaged but currently removing nothing". */
    bool isHpfEngaged() const { return hpfEngaged.load(std::memory_order_relaxed); }
    bool isLpfEngaged() const { return lpfEngaged.load(std::memory_order_relaxed); }

    /** Full-scale for the lamps. Both filters are single 2nd-order sections (12 dB/oct), so
        realistic broadband removal rarely passes ~18 dB; 24 dB leaves headroom before pinning. */
    static constexpr float kMaxRemovedDb = 24.0f;

    /** Below this input level there is no signal to have removed energy from, so the ratio is
        meaningless rather than merely noisy - the lamps are released instead of computed. */
    static constexpr float kEnergyGateDb = -60.0f;

private:
    /** Smoothed block mean-square tap. Smoothing the LINEAR power avoids the log(0) that a
        dB-domain follower hits on silence, and both taps share time constants so the ratio does
        not jitter from mismatched smoothing alone. */
    struct EnergyTap {
        double meanSquare { 0.0 };

        void reset() { meanSquare = 0.0; }

        void push(double blockMeanSquare, double blockSeconds)
        {
            const double tc = (blockMeanSquare > meanSquare) ? 0.015 : 0.300;
            meanSquare += (blockMeanSquare - meanSquare) * (1.0 - std::exp(-blockSeconds / tc));
        }

        float getDb() const
        {
            return meanSquare > 1.0e-12 ? static_cast<float>(10.0 * std::log10(meanSquare)) : -120.0f;
        }
    };

    static double computeMeanSquare(const juce::AudioBuffer<float>& buffer);

    double sampleRate { 44100.0 };
    int numChannels { 2 };

    InvisLoudnessAnalyser analyser;

    EnergyTap preTap;      // after input trim, before HPF
    EnergyTap postHpfTap;  // after HPF  -> HPF lamp reference for the LPF stage
    EnergyTap postLpfTap;  // after LPF

    std::atomic<float> hpfLampLevel { 0.0f };
    std::atomic<float> lpfLampLevel { 0.0f };
    std::atomic<bool> hpfEngaged { false };
    std::atomic<bool> lpfEngaged { false };

    InvisFilterCascade hpfFilter;
    InvisFilterCascade lpfFilter;

    std::atomic<float> peakL { 0.0f };
    std::atomic<float> peakR { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InputSidebarDSP)
};

} // namespace invis::modules
