#pragma once

#include "InvisEffect.h"

namespace invis::dsp {

/**
 * A delay line with fractional read, sized once and never reallocated.
 *
 * Linear interpolation on purpose: the modulated algorithms sweep this read pointer continuously,
 * and a higher-order interpolator costs more than it is worth at the depths they use. What matters
 * far more is that the read never allocates and never runs off the end.
 */
class DelayLine {
public:
    void prepare(double sampleRate, float maxSeconds)
    {
        size = std::max(4, static_cast<int>(sampleRate * maxSeconds) + 4);
        buffer.assign(static_cast<size_t>(size), 0.0f);
        writeIndex = 0;
    }

    void reset() { std::fill(buffer.begin(), buffer.end(), 0.0f); writeIndex = 0; }

    void write(float x)
    {
        buffer[static_cast<size_t>(writeIndex)] = x;
        if (++writeIndex >= size) writeIndex = 0;
    }

    float read(float delaySamples) const
    {
        if (size <= 0) return 0.0f;

        const float d = juce::jlimit(1.0f, static_cast<float>(size - 2), delaySamples);
        float pos = static_cast<float>(writeIndex) - d;
        while (pos < 0.0f) pos += static_cast<float>(size);

        const int i0 = static_cast<int>(pos);
        const int i1 = (i0 + 1) % size;
        const float frac = pos - static_cast<float>(i0);

        return buffer[static_cast<size_t>(i0)] * (1.0f - frac)
             + buffer[static_cast<size_t>(i1)] * frac;
    }

private:
    std::vector<float> buffer;
    int size { 0 };
    int writeIndex { 0 };
};

/** One-pole low-pass, used as the damping in feedback paths. */
class OnePole {
public:
    void setCutoff(float hz, double sampleRate)
    {
        const float x = std::exp(-juce::MathConstants<float>::twoPi
                                 * juce::jlimit(20.0f, 18000.0f, hz)
                                 / static_cast<float>(sampleRate));
        a = x;
    }

    void reset() { z = 0.0f; }
    float process(float x) { z = x * (1.0f - a) + z * a; return z; }

private:
    float a { 0.5f }, z { 0.0f };
};

// =================================================================================================

/**
 * SPATIAL - delay and reverb as one algorithm.
 *
 * They are the same machine at different settings, which is why the family holds both: a delay is
 * a long tap you can count, a reverb is short taps you cannot. SIZE walks continuously between
 * them, so the catalogue entries (DELAY, ECHO, HALL, PLATE, CHAMBER, SPRING) are positions on that
 * travel rather than separate code.
 *
 * Structure: four allpass diffusers into three parallel comb-ish delays with damped feedback. Not a
 * large FDN - the point here is a well-behaved space that stays stable at every setting the chart
 * can reach, including a star being dragged while it rings.
 */
class SpatialEffect : public InvisEffect {
public:
    enum Param { Size, Feedback, Damping, Modulation, Width, NumParams };

    void prepare(double sr, int) override
    {
        sampleRate = sr;

        for (int i = 0; i < kNumCombs; ++i)
        {
            combs[i].prepare(sr, 1.4f);
            damp[i].setCutoff(6000.0f, sr);
        }

        for (int i = 0; i < kNumAllpass; ++i)
            allpass[i].prepare(sr, 0.06f);

        reset();
    }

    void reset() override
    {
        for (auto& c : combs) c.reset();
        for (auto& a : allpass) a.reset();
        for (auto& d : damp) d.reset();
        lfoPhase = 0.0f;
    }

    void setParam(int index, float v) override
    {
        v = juce::jlimit(0.0f, 1.0f, v);

        switch (index)
        {
            case Size:       size = v; break;
            case Feedback:   feedback = v; break;
            case Damping:    damping = v; break;
            case Modulation: modulation = v; break;
            case Width:      spread = v; break;
            default: break;
        }

        updateCoefficients();
    }

    juce::Range<int> getParamRange() const override { return { 0, NumParams }; }

    void process(float* x, int n) override
    {
        const float lfoStep = 0.6f / static_cast<float>(sampleRate);

        for (int s = 0; s < n; ++s)
        {
            lfoPhase += lfoStep;
            if (lfoPhase >= 1.0f) lfoPhase -= 1.0f;

            const float wobble = std::sin(lfoPhase * juce::MathConstants<float>::twoPi)
                               * modulation * 14.0f;

            float in = x[s];

            // Diffusion first: without it a short size rings as discrete taps instead of a space.
            for (int i = 0; i < kNumAllpass; ++i)
            {
                const float d = allpassSamples[i];
                const float delayed = allpass[i].read(d);
                const float v = in - 0.62f * delayed;

                allpass[i].write(v);
                in = delayed + 0.62f * v;
            }

            float sum = 0.0f;

            for (int i = 0; i < kNumCombs; ++i)
            {
                const float delayed = combs[i].read(combSamples[i] + wobble * (i == 1 ? -1.0f : 1.0f));
                sum += delayed;

                const float fed = damp[i].process(delayed) * fbGain;
                combs[i].write(in + fed);
            }

            // A REVERB HAS GAIN. Three combs feeding themselves back build up well past unity - the
            // average across them is not enough, and measured it reached nearly twice the input at
            // full feedback. Backed off by the build-up its own feedback implies, so turning the
            // tail up lengthens it instead of also making it louder.
            x[s] = sum * outputTrim;
        }
    }

private:
    void updateCoefficients()
    {
        // 8 ms to 900 ms. Below ~40 ms the taps fuse into a room; above ~150 ms you start counting
        // them, which is exactly where a reverb becomes a delay.
        const float seconds = 0.008f * std::pow(900.0f / 8.0f, size);
        const float base = static_cast<float>(sampleRate) * seconds;

        // Mutually irrational-ish ratios, so the taps do not line up into a pitched flutter.
        static constexpr float kRatios[kNumCombs] = { 1.0f, 0.7548f, 0.5637f };

        for (int i = 0; i < kNumCombs; ++i)
        {
            combSamples[i] = base * juce::jmap(spread, 1.0f, kRatios[i]);
            damp[i].setCutoff(juce::jmap(1.0f - damping, 900.0f, 16000.0f), sampleRate);
        }

        static constexpr float kAllpassMs[kNumAllpass] = { 4.7f, 8.3f, 13.1f, 19.7f };
        for (int i = 0; i < kNumAllpass; ++i)
            allpassSamples[i] = static_cast<float>(sampleRate) * kAllpassMs[i] * 0.001f;

        // Hard ceiling below unity. The chart can drive several of these in series, and a feedback
        // that reaches 1.0 anywhere in that chain runs away with no user action to stop it.
        fbGain = juce::jlimit(0.0f, 0.94f, feedback * 0.94f);
        // The coefficient is MEASURED, not guessed: at full feedback the three combs came to about
        // 1.9x the input, so the divisor has to reach 1.92 there and 1.0 at no feedback. Guessing
        // high, as I did first, makes turning the tail up quieter - the opposite fault.
        outputTrim = 1.0f / (static_cast<float>(kNumCombs) * (1.0f + 0.98f * fbGain));
    }

    static constexpr int kNumCombs = 3;
    static constexpr int kNumAllpass = 4;

    double sampleRate { 44100.0 };

    DelayLine combs[kNumCombs], allpass[kNumAllpass];
    OnePole damp[kNumCombs];

    float combSamples[kNumCombs] { 0, 0, 0 };
    float allpassSamples[kNumAllpass] { 0, 0, 0, 0 };

    float size { 0.5f }, feedback { 0.5f }, damping { 0.4f }, modulation { 0.2f }, spread { 0.6f };
    float fbGain { 0.45f }, lfoPhase { 0.0f }, outputTrim { 0.33f };
};

// =================================================================================================

/**
 * MODULATION - chorus, flanger and vibrato from one modulated delay.
 *
 * The three differ only in delay range and how much of the output is fed back: a flanger is a
 * chorus with a shorter delay and feedback, and a vibrato is a chorus with no dry path at all. The
 * dry path is NOT here - the slot owns dry/wet - so what is left really is one machine.
 */
class ModulationEffect : public InvisEffect {
public:
    enum Param { Rate, Depth, Delay, Feedback, Shape, NumParams };

    void prepare(double sr, int) override
    {
        sampleRate = sr;
        line.prepare(sr, 0.05f);
        reset();
    }

    void reset() override { line.reset(); phase = 0.0f; lastOut = 0.0f; }

    void setParam(int index, float v) override
    {
        v = juce::jlimit(0.0f, 1.0f, v);

        switch (index)
        {
            case Rate:     rate = 0.05f * std::pow(12.0f / 0.05f, v); break;   // 0.05 .. 12 Hz
            case Depth:    depth = v; break;
            case Delay:    baseMs = juce::jmap(v, 0.4f, 26.0f); break;
            // BIPOLAR, and the centre is OFF. Half feedback is a plain chorus; either end is a
            // resonant flanger, and the two ends do not sound the same - negative feedback
            // notches where positive peaks.
            case Feedback:
                feedback = juce::jlimit(-0.85f, 0.85f, v * 1.70f - 0.85f);

                // Both ends of the path. Trimming only the input still let the resonance build
                // three times over: what a comb does to a steady tone is a gain, and it has to be
                // paid for on the way out as well.
                inputTrim = 1.0f - 0.55f * std::abs(feedback);
                outputTrim = 1.0f / (1.0f + 1.6f * std::abs(feedback));
                break;
            case Shape:    shape = v; break;
            default: break;
        }
    }

    juce::Range<int> getParamRange() const override { return { 0, NumParams }; }

    void process(float* x, int n) override
    {
        const float step = rate / static_cast<float>(sampleRate);
        const float depthSamples = static_cast<float>(sampleRate) * 0.001f * baseMs * 0.9f * depth;
        const float baseSamples = static_cast<float>(sampleRate) * 0.001f * baseMs;

        for (int s = 0; s < n; ++s)
        {
            phase += step;
            if (phase >= 1.0f) phase -= 1.0f;

            // Sine to triangle. A triangle sweeps at constant speed, which is what gives a flanger
            // its even jet rather than the pause a sine leaves at each turning point.
            const float sine = std::sin(phase * juce::MathConstants<float>::twoPi);
            const float tri = 4.0f * std::abs(phase - 0.5f) - 1.0f;
            const float lfo = juce::jmap(shape, sine, tri);

            const float d = baseSamples + depthSamples * lfo;
            const float delayed = line.read(d);

            // The input is backed off as the feedback rises. A resonant flanger otherwise gains
            // over twelve decibels at the extremes - measured, not guessed - and hands the rest of
            // the chain a signal it never asked for. Character stays; the level does not run.
            line.write(x[s] * inputTrim + lastOut * feedback);
            lastOut = delayed;

            x[s] = delayed * outputTrim;
        }
    }

private:
    double sampleRate { 44100.0 };
    DelayLine line;

    float rate { 0.8f }, depth { 0.5f }, baseMs { 8.0f }, feedback { 0.0f }, shape { 0.0f };
    float phase { 0.0f }, lastOut { 0.0f };
    float inputTrim { 1.0f }, outputTrim { 1.0f };
};

// =================================================================================================

/**
 * SATURATION - one shaper, several curves.
 *
 * DRIVE sets how hard the signal hits the curve and CHARACTER walks between them, so TAPE, TUBE,
 * DRIVE and CRUSH are positions on a continuum rather than four functions.
 *
 * OUTPUT COMPENSATION IS NOT OPTIONAL HERE. Driving a stage harder makes it louder as well as
 * dirtier, and on this chart a star can be pushed while you are listening to it - without
 * compensation you would hear the level and call it the effect. Gain is backed off by the curve's
 * own gain at unity so the character changes and the loudness does not.
 */
class SaturationEffect : public InvisEffect {
public:
    enum Param { Drive, Character, Bias, Tone, NumParams };

    void prepare(double sr, int) override
    {
        sampleRate = sr;
        tilt.setCutoff(4000.0f, sr);
        dcBlockA = 1.0f - (20.0f * juce::MathConstants<float>::twoPi / static_cast<float>(sr));
        reset();
    }

    void reset() override { tilt.reset(); dcX = dcY = 0.0f; }

    void setParam(int index, float v) override
    {
        v = juce::jlimit(0.0f, 1.0f, v);

        switch (index)
        {
            case Drive:     drive = juce::jmap(v, 1.0f, 40.0f); break;
            case Character: character = v; break;
            case Bias:      bias = juce::jmap(v, -0.35f, 0.35f); break;
            case Tone:      tone = v; tilt.setCutoff(juce::jmap(v, 700.0f, 16000.0f), sampleRate); break;
            default: break;
        }

        // MEASURED OVER A CYCLE, not at one point. A single probe works for a monotonic curve and
        // is meaningless for a folding one: sin() happens to pass through zero at some drives, so
        // the probe read "this curve removes everything" and the compensation shot up. The fold
        // then arrived four times louder than the soft curve beside it.
        double energy = 0.0;
        constexpr int kProbe = 128;

        for (int i = 0; i < kProbe; ++i)
        {
            const float phase = juce::MathConstants<float>::twoPi
                              * static_cast<float>(i) / static_cast<float>(kProbe);
            const float v = shape(0.7071f * std::sin(phase) * drive + bias);
            energy += static_cast<double>(v) * v;
        }

        // AGAINST THE PROBE'S OWN RMS, not its amplitude. Comparing the output's RMS to 0.7071 -
        // which is the probe's PEAK - asked the curve to hit a level 3 dB above what went in, so
        // every saturated star handed the chain roughly double the signal. On a chart that can put
        // several in series that is what turns "some effect" into "overloaded".
        constexpr float kProbeRms = 0.7071f * 0.7071f;   // a sine's rms is its amplitude / sqrt(2)

        const float outRms = static_cast<float>(std::sqrt(energy / kProbe));
        makeup = kProbeRms / std::max(0.05f, outRms);
    }

    juce::Range<int> getParamRange() const override { return { 0, NumParams }; }

    float getActivity() const override { return activity; }

    void process(float* x, int n) override
    {
        double bent = 0.0, straight = 0.0;

        for (int s = 0; s < n; ++s)
        {
            float v = shape(x[s] * drive + bias) * makeup;

            // HOW FAR THE CURVE BENT IT, against what a straight wire would have passed. This is
            // the honest answer to "is it saturating": at low drive the two paths agree and the
            // lamp stays dark however loud the material is, and it lights when the shape is
            // actually being used.
            bent += static_cast<double>(v - x[s]) * (v - x[s]);
            straight += static_cast<double>(x[s]) * x[s];

            // The bias, and any asymmetry the curve introduces, leave DC behind. Left in it would
            // stack up through a chain of stars and eat headroom for nothing audible.
            const float hp = v - dcX + dcBlockA * dcY;
            dcX = v; dcY = hp;
            v = hp;

            // Tone as a tilt rather than a filter: saturation without it just gets brighter, and
            // the darker half is most of what "tape" means.
            const float low = tilt.process(v);
            x[s] = juce::jmap(tone, low, v);
        }

        if (straight > 1.0e-9)
        {
            const float ratio = static_cast<float>(std::sqrt(bent / straight));
            const float target = juce::jlimit(0.0f, 1.0f, ratio * 1.6f);

            // Fast up, slow down, like every other lamp on the panel: a lamp that follows the
            // material sample for sample is a flicker, not a reading.
            activity += (target - activity) * (target > activity ? 0.35f : 0.06f);
        }
        else
        {
            activity *= 0.90f;
        }
    }

private:
    /** tanh -> soft clip -> hard fold, walked by CHARACTER. */
    float shape(float v) const
    {
        const float soft = std::tanh(v);
        const float clipped = juce::jlimit(-1.0f, 1.0f, v * 0.72f);

        // Past two thirds the curve starts folding, which is where it stops being warmth and
        // becomes damage - CRUSH lives up here.
        const float folded = std::sin(juce::jlimit(-6.0f, 6.0f, v));

        if (character <= 0.5f)  return juce::jmap(character * 2.0f, soft, clipped);

        return juce::jmap((character - 0.5f) * 2.0f, clipped, folded);
    }

    double sampleRate { 44100.0 };
    OnePole tilt;

    float drive { 4.0f }, character { 0.0f }, bias { 0.0f }, tone { 0.7f }, makeup { 1.0f };
    float activity { 0.0f };
    float dcBlockA { 0.999f }, dcX { 0.0f }, dcY { 0.0f };
};

} // namespace invis::dsp
