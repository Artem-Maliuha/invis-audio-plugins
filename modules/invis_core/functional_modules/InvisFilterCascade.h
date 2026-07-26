#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <cmath>

namespace invis::modules {

/**
 * Selectable filter slope, shared by every HPF/LPF in the system.
 *
 * The standard entries are plain Butterworth cascades. The last two are deliberate character
 * options rather than "more of the same":
 *
 *   Resonant24  - 24 dB/oct with the final section's Q pushed up, producing a ~+6 dB peak right
 *                 at the corner. This is the classic console trick: cutting below the corner
 *                 while simultaneously emphasising the note just above it, so a kick or a bass
 *                 gets tighter AND more present instead of merely thinner.
 *   Brickwall96 - 16th order. Surgical rather than musical: used to remove content outright with
 *                 almost no transition band, at the cost of heavy phase smear near the corner.
 */
enum class FilterSlope {
    Slope6,
    Slope12,
    Slope18,
    Slope24,
    Slope36,
    Slope48,
    Resonant24,
    Brickwall96
};

inline constexpr int kNumFilterSlopes = 8;

inline const char* getFilterSlopeName(FilterSlope s)
{
    switch (s)
    {
        case FilterSlope::Slope6:      return "6 dB/oct";
        case FilterSlope::Slope18:     return "18 dB/oct";
        case FilterSlope::Slope24:     return "24 dB/oct";
        case FilterSlope::Slope36:     return "36 dB/oct";
        case FilterSlope::Slope48:     return "48 dB/oct";
        case FilterSlope::Resonant24:  return "24 RESO";
        case FilterSlope::Brickwall96: return "96 BRICK";
        case FilterSlope::Slope12:
        default:                       return "12 dB/oct";
    }
}

/** Compact label for the sidebar cell, where "12 dB/oct" will not fit. */
inline const char* getFilterSlopeShortName(FilterSlope s)
{
    switch (s)
    {
        case FilterSlope::Slope6:      return "6DB";
        case FilterSlope::Slope18:     return "18DB";
        case FilterSlope::Slope24:     return "24DB";
        case FilterSlope::Slope36:     return "36DB";
        case FilterSlope::Slope48:     return "48DB";
        case FilterSlope::Resonant24:  return "24RES";
        case FilterSlope::Brickwall96: return "96BRK";
        case FilterSlope::Slope12:
        default:                       return "12DB";
    }
}

/** Filter order (in 6 dB/oct units) for a slope. */
inline int getFilterSlopeOrder(FilterSlope s)
{
    switch (s)
    {
        case FilterSlope::Slope6:      return 1;
        case FilterSlope::Slope18:     return 3;
        case FilterSlope::Slope24:     return 4;
        case FilterSlope::Slope36:     return 6;
        case FilterSlope::Slope48:     return 8;
        case FilterSlope::Resonant24:  return 4;
        case FilterSlope::Brickwall96: return 16;
        case FilterSlope::Slope12:
        default:                       return 2;
    }
}

/**
 * A cascade of IIR sections implementing a selectable-slope high-pass or low-pass.
 *
 * Section Qs are COMPUTED from the Butterworth pole formula rather than pulled from a hardcoded
 * table, so adding a slope means adding one enum entry and nothing else. An odd order gets one
 * real pole (a first-order section) plus (N-1)/2 quadratic sections.
 */
class InvisFilterCascade {
public:
    static constexpr int kMaxSections = 8;   // 8 biquads = 96 dB/oct
    static constexpr float kResonantQ = 2.0f; // ~+6 dB peak at the corner

    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        firstOrder.prepare(spec);
        for (auto& s : sections) s.prepare(spec);
        reset();
    }

    void reset()
    {
        firstOrder.reset();
        for (auto& s : sections) s.reset();
        currentFreq = -1.0f;
        currentSlope = FilterSlope::Slope12;
        currentIsHighPass = true;
    }

    /** Rebuilds coefficients only when something actually changed. */
    void update(float frequency, FilterSlope slope, bool isHighPass)
    {
        if (std::abs(currentFreq - frequency) < 0.1f
            && slope == currentSlope
            && isHighPass == currentIsHighPass)
            return;

        currentFreq = frequency;
        currentSlope = slope;
        currentIsHighPass = isHighPass;

        const int order = getFilterSlopeOrder(slope);
        usesFirstOrder = (order % 2) != 0;
        numSections = order / 2;
        jassert(numSections <= kMaxSections);

        if (usesFirstOrder)
        {
            *firstOrder.state = isHighPass
                ? *juce::dsp::IIR::Coefficients<float>::makeFirstOrderHighPass(sampleRate, frequency)
                : *juce::dsp::IIR::Coefficients<float>::makeFirstOrderLowPass(sampleRate, frequency);
        }

        for (int i = 0; i < numSections; ++i)
        {
            float q = butterworthQ(order, i);

            // Resonant variant: lift ONLY the highest-Q section. Raising them all would push the
            // whole passband around instead of producing a single defined peak at the corner.
            if (slope == FilterSlope::Resonant24 && i == numSections - 1)
                q = kResonantQ;

            *sections[static_cast<size_t>(i)].state = isHighPass
                ? *juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, frequency, q)
                : *juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, frequency, q);
        }
    }

    void process(juce::AudioBuffer<float>& buffer)
    {
        juce::dsp::AudioBlock<float> block(buffer);
        juce::dsp::ProcessContextReplacing<float> ctx(block);

        if (usesFirstOrder)
            firstOrder.process(ctx);

        for (int i = 0; i < numSections; ++i)
            sections[static_cast<size_t>(i)].process(ctx);
    }

private:
    /**
     * Q of quadratic section `index` of a Butterworth filter of the given order.
     *
     * Even order N: Q_i = 1 / (2 cos((2i+1)pi / 2N))
     * Odd order  N: one real pole plus Q_i = 1 / (2 cos((i+1)pi / N))
     */
    static float butterworthQ(int order, int index)
    {
        const auto pi = juce::MathConstants<double>::pi;

        const double angle = (order % 2 == 0)
            ? (2.0 * index + 1.0) * pi / (2.0 * order)
            : (index + 1.0) * pi / order;

        return static_cast<float>(1.0 / (2.0 * std::cos(angle)));
    }

    using Duplicator = juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                                      juce::dsp::IIR::Coefficients<float>>;

    double sampleRate { 44100.0 };
    Duplicator firstOrder;
    std::array<Duplicator, kMaxSections> sections;

    int numSections { 1 };
    bool usesFirstOrder { false };

    float currentFreq { -1.0f };
    FilterSlope currentSlope { FilterSlope::Slope12 };
    bool currentIsHighPass { true };
};

} // namespace invis::modules
