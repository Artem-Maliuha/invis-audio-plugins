#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>

namespace invis::modules {

/**
 * Parameters and processing for the standard TOP sidebar - the plugin-global chassis header.
 *
 * Owns the global concerns: gain-stage target, master bypass. Oversampling parameters are NOT
 * re-declared here; the top sidebar merely presents whatever `OversamplingDSP` already registered,
 * so there is exactly one owner per parameter.
 */
class TopSidebarDSP {
public:
    TopSidebarDSP() = default;
    ~TopSidebarDSP() = default;

    /** Gain-stage target range. -18 dBFS RMS is the classic "0 VU" alignment point that analogue
        emulations are calibrated around, so it is the default. */
    static constexpr float kMinGainStageDb = -24.0f;
    static constexpr float kMaxGainStageDb = 0.0f;
    static constexpr float kDefaultGainStageDb = -18.0f;

    static void addParameters(juce::AudioProcessorValueTreeState::ParameterLayout& layout,
                              const juce::String& paramPrefix = "top_");

    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();

    /** Applies master bypass. Returns true if the plugin should skip its own processing. */
    bool isBypassed(juce::AudioProcessorValueTreeState& apvts,
                    const juce::String& paramPrefix = "top_") const
    {
        return apvts.getRawParameterValue(paramPrefix + "bypass")->load() > 0.5f;
    }

    static float getGainStageTargetDb(juce::AudioProcessorValueTreeState& apvts,
                                      const juce::String& paramPrefix = "top_")
    {
        return apvts.getRawParameterValue(paramPrefix + "gain_stage")->load();
    }

private:
    double sampleRate { 44100.0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TopSidebarDSP)
};

} // namespace invis::modules
