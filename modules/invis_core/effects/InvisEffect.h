#pragma once

#include "../design_system/InvisEffectPalette.h"
#include <juce_dsp/juce_dsp.h>
#include <array>

namespace invis::dsp {

/** How many knobs an algorithm may expose. Fixed so a slot never allocates to hold them. */
inline constexpr int kMaxEffectParams = 6;

/**
 * One knob an algorithm offers.
 *
 * DESCRIBED, NOT BUILT. The algorithm says what it has and what the numbers mean; the panel builds
 * itself from that. Hand-writing a knob per algorithm in the editor is how a plugin ends up with
 * fourteen nearly-identical layouts that drift apart, and it puts product code in the way of every
 * new algorithm.
 */
struct EffectParam {
    const char* name { nullptr };
    float min { 0.0f };
    float max { 1.0f };
    float defaultValue { 0.5f };
    const char* suffix { "" };
    int decimals { 0 };
    bool skewed { false };      // true for time and frequency, where the ear is logarithmic

    float toPlain(float normalized) const
    {
        const float t = juce::jlimit(0.0f, 1.0f, normalized);

        if (skewed && min > 0.0f)
            return min * std::pow(max / min, t);

        return min + (max - min) * t;
    }

    float toNormalized(float plain) const
    {
        if (skewed && min > 0.0f && plain > 0.0f)
            return juce::jlimit(0.0f, 1.0f,
                                std::log(plain / min) / std::log(max / min));

        return juce::jlimit(0.0f, 1.0f, (plain - min) / std::max(1.0e-9f, max - min));
    }
};

/**
 * AN ALGORITHM. One per star, per stream.
 *
 * Mono by contract, and that is a decision rather than a simplification: in the dual-observer modes
 * the chart routes L and R (or M and S) along DIFFERENT chains, entering at different stars in
 * different orders. There is no coherent way for one stereo instance to be at two places in two
 * chains at once, so a star is instantiated per stream and each instance sees one channel.
 *
 * The cost is not what it looks like. In the single-observer modes a star is one stereo pair of
 * instances; in the dual modes it is two mono ones. The work is the same either way - what doubles
 * is the number of objects, not the samples processed.
 *
 * A genuinely stereo effect - ping-pong, width - cannot be honest here and would run as dual mono.
 * None of the algorithms below are, and any future one that is has to say so.
 */
class InvisEffect {
public:
    virtual ~InvisEffect() = default;

    virtual void prepare(double sampleRate, int maxBlockSize) = 0;
    virtual void reset() = 0;

    /** In place, one channel, `numSamples` long. Must never allocate. */
    virtual void process(float* samples, int numSamples) = 0;

    /** Normalized 0..1, in the order getParams() reports them. */
    virtual void setParam(int index, float normalized) = 0;

    virtual juce::Range<int> getParamRange() const = 0;

    /**
     * How hard the algorithm is WORKING, 0..1, or -1 when it has nothing to report.
     *
     * Measured, not derived from a knob. A drive control tells you where you put it; this tells
     * you what the material is doing to the curve, which is a different fact and the one worth a
     * lamp: the same setting bites on a loud passage and barely touches a quiet one.
     */
    virtual float getActivity() const { return -1.0f; }
};

/** What an algorithm exposes, looked up by name so the catalogue and the DSP cannot drift apart. */
struct EffectDefinition {
    const char* name;
    ui::EffectFamily family;
    std::array<EffectParam, kMaxEffectParams> params;
    int numParams;
};

} // namespace invis::dsp
