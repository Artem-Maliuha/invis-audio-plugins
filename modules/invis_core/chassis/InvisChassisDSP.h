#pragma once

#include "../functional_modules/input_sidebar/InputSidebarDSP.h"
#include "../functional_modules/output_sidebar/OutputSidebarDSP.h"
#include "../functional_modules/top_sidebar/TopSidebarDSP.h"
#include "../functional_modules/oversampling/OversamplingDSP.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace invis::modules {

/**
 * THE CHASSIS - the part of every plugin that is not the plugin.
 *
 * Input at the front, output at the back, and the same wiring between them every single time. Only
 * what sits BETWEEN them is a particular plugin; the ends are invariant, so they have no business
 * being reassembled by hand in each one.
 *
 * They were. Every plugin had to add three sets of parameters under the right prefixes, prepare
 * three DSP blocks, and run the stages in the correct order around its own algorithm - and the
 * demo bench proved how that ends: the meters metered nothing, the AUTO routines had no path back
 * to the analysers, and the sidebars were laid out but never even added to the editor. Nothing was
 * broken. Everything was simply never connected, because connecting it was somebody's job to
 * remember.
 *
 * So the chassis owns both ends AND THE SEAM. A plugin hands its algorithm to `process()` and
 * cannot get the order wrong, cannot forget bypass, and cannot silently skip metering: there is no
 * expressible way to write it down incorrectly.
 *
 * @see InvisChassisUI for the matching half - the panel and the pump that keeps it live.
 */
class InvisChassisDSP {
public:
    // The prefixes are FIXED, not arguments. A prefix that varies per plugin is a prefix that gets
    // mistyped, and the failure is silent: the parameter exists, the control moves, nothing hears
    // it. Presets and automation stay portable across the series as a side effect.
    static constexpr const char* kInputPrefix  = "in_side_";
    static constexpr const char* kOutputPrefix = "out_side_";
    static constexpr const char* kTopPrefix    = "top_";
    static constexpr const char* kOsPrefix     = "os_";

    /**
     * Every chassis parameter, in one call.
     *
     * A plugin adds this to its layout and then adds only its own. Forgetting one set used to mean
     * a control that moved a knob attached to nothing.
     */
    static void addParameters(juce::AudioProcessorValueTreeState::ParameterLayout& layout)
    {
        TopSidebarDSP::addParameters(layout, kTopPrefix);
        OversamplingDSP::addParameters(layout, kOsPrefix);
        InputSidebarDSP::addParameters(layout, kInputPrefix);
        OutputSidebarDSP::addParameters(layout, kOutputPrefix);
    }

    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        top.prepare(spec);
        input.prepare(spec);
        output.prepare(spec);
        oversampling.prepare(spec.sampleRate,
                             static_cast<int>(spec.maximumBlockSize),
                             static_cast<int>(spec.numChannels));
    }

    void reset()
    {
        top.reset();
        input.reset();
        output.reset();
        oversampling.reset();
    }

    /**
     * Run a block through the chassis, with the plugin's own algorithm in the middle.
     *
     * The order is the contract and it is not negotiable:
     *   input trim + band limiting + input metering
     *     -> [ the plugin, oversampled ]
     *   -> output gain + output metering
     *
     * BYPASS still meters. A bypassed panel that also goes blind tells you nothing about what is
     * passing through it, which is the one moment you most want to look.
     *
     * @param plugin  called with the buffer once, between the stages. Not called when bypassed.
     */
    template <typename PluginStage>
    void process(juce::AudioBuffer<float>& buffer,
                 juce::AudioProcessorValueTreeState& apvts,
                 bool nonRealtime,
                 PluginStage&& plugin)
    {
        const bool bypassed = top.isBypassed(apvts, kTopPrefix);

        input.processBlock(buffer, apvts, kInputPrefix, bypassed);

        if (!bypassed)
            oversampling.process(buffer, apvts, nonRealtime,
                                 [&buffer, &plugin](juce::dsp::AudioBlock<float>&)
                                 {
                                     plugin(buffer);
                                 });

        output.processBlock(buffer, apvts, kOutputPrefix, bypassed);
    }

    /** A chassis with nothing between the ends - useful for a bench, and for a plugin's first day. */
    void process(juce::AudioBuffer<float>& buffer,
                 juce::AudioProcessorValueTreeState& apvts,
                 bool nonRealtime)
    {
        process(buffer, apvts, nonRealtime, [](juce::AudioBuffer<float>&) {});
    }

    static float getGainStageTargetDb(juce::AudioProcessorValueTreeState& apvts)
    {
        return TopSidebarDSP::getGainStageTargetDb(apvts, kTopPrefix);
    }

    InputSidebarDSP& getInput()   { return input; }
    OutputSidebarDSP& getOutput() { return output; }
    TopSidebarDSP& getTop()       { return top; }

private:
    TopSidebarDSP top;
    InputSidebarDSP input;
    OutputSidebarDSP output;
    OversamplingDSP oversampling;
};

} // namespace invis::modules
