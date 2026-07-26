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
 * THE LFO SHAPES, and why exactly these five.
 *
 * The set the hardware settled on is sine, triangle, square, saw, ramp and random - the Moogerfooger
 * MF-108M lists precisely that. Ramp is a saw with its slope reversed and is the one that adds no
 * new character, so it is the one dropped: five shapes that each sound like something, rather than
 * six where two are the same idea.
 *
 * They matter differently per effect. A flanger is almost always sine or triangle, because the
 * sweep has to be even - a square makes it a two-position switch and a saw makes it a rewind. A
 * chorus is where random earns its place: irregular motion is what stops several voices sounding
 * like one detuned one.
 */
enum class LfoShape { Sine, Triangle, Square, Saw, Random, NumShapes };

inline const char* const kLfoShapeLabels[] = { "SINE", "TRI", "SQR", "SAW", "RAND" };

/** One oscillator, shared by everything that modulates. Output is -1..1. */
class Lfo {
public:
    void prepare(double sr) { sampleRate = sr; reset(); }
    void reset() { phase = 0.0f; held = 0.0f; lastQuadrant = -1; }

    void setShape(LfoShape s) { shape = s; }
    void setRate(float hz) { rate = juce::jlimit(0.005f, 40.0f, hz); }

    /** Where in the cycle to start, 0..1. Two instances an offset apart is what stereo motion is. */
    void setPhaseOffset(float turns) { offset = turns - std::floor(turns); }

    float next()
    {
        phase += rate / static_cast<float>(sampleRate);
        if (phase >= 1.0f) phase -= 1.0f;

        const float p = phase + offset >= 1.0f ? phase + offset - 1.0f : phase + offset;

        switch (shape)
        {
            case LfoShape::Sine:     return std::sin(p * juce::MathConstants<float>::twoPi);
            case LfoShape::Triangle: return 4.0f * std::abs(p - 0.5f) - 1.0f;
            case LfoShape::Square:   return p < 0.5f ? 1.0f : -1.0f;
            case LfoShape::Saw:      return 1.0f - 2.0f * p;

            case LfoShape::Random:
            default:
            {
                // Sampled TWICE a cycle, not once: at one sample per cycle the rate control barely
                // reads as a rate at all, and every unit that offers random does it on the half.
                const int quadrant = static_cast<int>(p * 2.0f);

                if (quadrant != lastQuadrant)
                {
                    lastQuadrant = quadrant;
                    held = rng.nextFloat() * 2.0f - 1.0f;
                }

                return held;
            }
        }
    }

private:
    double sampleRate { 44100.0 };
    LfoShape shape { LfoShape::Sine };
    float rate { 1.0f }, phase { 0.0f }, offset { 0.0f }, held { 0.0f };
    int lastQuadrant { -1 };
    juce::Random rng;
};

/** Stage counts a phaser is actually built with. Even numbers, because a notch needs a pair. */
inline constexpr int kPhaserStageCounts[] = { 2, 4, 6, 8, 12 };
inline const char* const kPhaserStageLabels[] = { "2", "4", "6", "8", "12" };

/**
 * PHASER - an allpass chain, which is what makes it a phaser rather than a flanger.
 *
 * It was sharing the modulated-delay algorithm, and that was simply wrong: a flanger's notches are
 * harmonically spaced because they come from a delay, and a phaser's are NOT, because they come
 * from a cascade of allpass sections whose corner is swept. That non-harmonic spacing is the entire
 * difference between the two effects, and no amount of parameter tuning on a delay produces it.
 *
 * Four stages is the Phase 90 and the Small Stone; the rest of the range is what the pedals with a
 * stage switch actually offer.
 */
class PhaserEffect : public InvisEffect {
public:
    enum Param { Rate, Depth, Centre, Feedback, Stages, Wave, Stereo, NumParams };

    void prepare(double sr, int) override
    {
        sampleRate = sr;
        lfo.prepare(sr);
        reset();
    }

    void reset() override
    {
        for (auto& z : state) z = 0.0f;
        lfo.reset();
        lastOut = 0.0f;
    }

    void setChannel(int c) override { side = juce::jlimit(0, 1, c); applyStereo(); }
    juce::Range<int> getParamRange() const override { return { 0, NumParams }; }

    void setParam(int index, float v) override
    {
        v = juce::jlimit(0.0f, 1.0f, v);

        switch (index)
        {
            case Rate:     lfo.setRate(0.05f * std::pow(12.0f / 0.05f, v)); break;
            case Depth:    depth = v; break;
            case Centre:   centre = juce::jmap(v, 200.0f, 2200.0f); break;
            // Paid for on BOTH ends, like the flanger. An allpass chain with regeneration is a
            // resonator: measured, six stages at full negative feedback reached four times the
            // input, which is not a character choice, it is a control that hands the rest of the
            // chain a signal it never asked for.
            case Feedback:
                feedback = juce::jlimit(-0.8f, 0.8f, v * 1.6f - 0.8f);
                inputTrim = 1.0f - 0.45f * std::abs(feedback);
                outputTrim = 1.0f / (1.0f + 1.7f * std::abs(feedback));
                break;

            case Stages:
                stages = kPhaserStageCounts[juce::jlimit(0, 4, static_cast<int>(v * 5.0f * 0.999f))];
                break;

            case Wave:
                lfo.setShape(static_cast<LfoShape>(
                    juce::jlimit(0, static_cast<int>(LfoShape::NumShapes) - 1,
                                 static_cast<int>(v * static_cast<float>(LfoShape::NumShapes) * 0.999f))));
                break;

            case Stereo: stereo = v; applyStereo(); break;
            default: break;
        }
    }

    void process(float* x, int n) override
    {
        for (int s = 0; s < n; ++s)
        {
            const float sweep = centre * std::pow(4.0f, lfo.next() * depth);
            const float w = juce::jlimit(0.001f, 0.45f,
                                         sweep / static_cast<float>(sampleRate));

            // One-pole allpass coefficient from the corner. Recomputed per sample because the
            // corner is what is being swept - holding it per block is where cheap phasers step.
            const float t = std::tan(juce::MathConstants<float>::pi * w);
            const float a = (t - 1.0f) / (t + 1.0f);

            float v = x[s] * inputTrim + lastOut * feedback;

            for (int i = 0; i < stages; ++i)
            {
                const float out = a * v + state[static_cast<size_t>(i)];
                state[static_cast<size_t>(i)] = v - a * out;
                v = out;
            }

            lastOut = v;
            x[s] = v * outputTrim;
        }
    }

private:
    void applyStereo() { lfo.setPhaseOffset(side == 0 ? 0.0f : stereo * 0.5f); }

    static constexpr int kMaxStages = 12;

    double sampleRate { 44100.0 };
    Lfo lfo;

    float state[kMaxStages] { };
    float depth { 0.6f }, centre { 700.0f }, feedback { 0.0f }, stereo { 0.0f }, lastOut { 0.0f };
    float inputTrim { 1.0f }, outputTrim { 1.0f };
    int stages { 4 }, side { 0 };
};

// =================================================================================================

/** Note divisions a synced time can land on, longest first, with the labels the readout shows. */
inline constexpr float kNoteFactors[] = {
    4.0f, 3.0f, 2.0f, 1.5f, 1.0f, 0.75f, 0.6667f, 0.5f, 0.375f, 0.3333f, 0.25f, 0.1667f, 0.125f
};

inline const char* const kNoteLabels[] = {
    "1/1", "1/2.", "1/2", "1/4.", "1/4", "1/8.", "1/4T", "1/8", "1/16.", "1/8T", "1/16", "1/16T", "1/32"
};

inline constexpr int kNumNoteDivisions = static_cast<int>(std::size(kNoteFactors));

/**
 * DELAY - its own algorithm, not a reverb wound out to a long size.
 *
 * They were one machine here and that was a simplification that shows the moment you try to use it.
 * A reverb wants diffusion, damping and a decay you do not count; a delay wants a time you can set
 * to the bar, two of them for the two sides, a feedback path with its own colour, and repeats that
 * alternate. None of those exist in a comb bank, and stretching one until it echoes gives you an
 * echo you cannot place in time.
 *
 * PING-PONG WITHOUT CROSS-FEEDING. The two sides of a star are separate instances processed a block
 * apart, so feeding one into the other would arrive late by the buffer length - audible, and wrong
 * in a way that changes with the host's block size. Instead each side knows which one it is and
 * passes alternate repeats: odd from the left, even from the right. Fed the same signal, that is
 * what a ping-pong delay sounds like, and it is exact at any block size.
 */
class DelayEffect : public InvisEffect {
public:
    enum Param { TimeL, TimeR, Sync, Feedback, PingPong, Tone, Character, Wow, NumParams };

    void prepare(double sr, int) override
    {
        sampleRate = sr;
        line.prepare(sr, 4.4f);          // room for the doubled feedback tap at the longest time
        tone.setCutoff(6000.0f, sr);
        reset();
        updateTimes();
    }

    void reset() override { line.reset(); tone.reset(); wowPhase = 0.0f; }
    void setChannel(int c) override { side = juce::jlimit(0, 1, c); updateTimes(); }
    void setTempo(double bpm) override { tempo = bpm > 1.0 ? bpm : 0.0; updateTimes(); }

    juce::Range<int> getParamRange() const override { return { 0, NumParams }; }

    void setParam(int index, float v) override
    {
        v = juce::jlimit(0.0f, 1.0f, v);

        switch (index)
        {
            case TimeL:     timeLRaw = v; break;
            case TimeR:     timeRRaw = v; break;
            case Sync:      synced = v >= 0.5f; break;
            case Feedback:  feedback = juce::jlimit(0.0f, 0.96f, v * 0.96f); break;
            case PingPong:  pingPong = v; break;

            // The feedback path's own filter. A delay whose repeats never darken is the one thing
            // every early digital delay got wrong: each pass has to lose something.
            case Tone:      tone.setCutoff(juce::jmap(v, 700.0f, 16000.0f), sampleRate); break;

            // DIGITAL -> ANALOGUE -> TAPE as one travel: how much the feedback path softens on
            // every pass. The catalogue entries are places to stand on it, not separate code.
            // THREE MACHINES, chosen. Digital repeats clean, analogue rounds, tape rounds harder
            // and compresses. A continuous slide between them was a control nobody could aim.
            case Character:
                character = juce::jlimit(0, 2, static_cast<int>(v * 3.0f * 0.999f)) * 0.5f;
                break;
            case Wow:       wow = v; break;
            default: break;
        }

        updateTimes();
    }

    void process(float* x, int n) override
    {
        const float wowStep = 0.9f / static_cast<float>(sampleRate);
        const float wowDepth = wow * static_cast<float>(sampleRate) * 0.0016f;

        for (int s = 0; s < n; ++s)
        {
            wowPhase += wowStep;
            if (wowPhase >= 1.0f) wowPhase -= 1.0f;

            // Two sines an octave and a bit apart: one alone reads as vibrato, and tape does not
            // wobble in a way you can hum along to.
            const float flutter = (std::sin(wowPhase * juce::MathConstants<float>::twoPi) * 0.7f
                                 + std::sin(wowPhase * juce::MathConstants<float>::twoPi * 2.7f) * 0.3f)
                                * wowDepth;

            const float out = line.read(readSamples + flutter);
            float fed = tone.process(line.read(feedSamples + flutter));

            if (character > 0.01f)
                fed = juce::jmap(character, fed, std::tanh(fed * (1.0f + character * 2.2f)) * 0.85f);

            line.write(x[s] + fed * feedback);
            x[s] = out;
        }
    }

private:
    float secondsFor(float raw) const
    {
        if (synced && tempo > 0.0)
        {
            const int step = juce::jlimit(0, kNumNoteDivisions - 1,
                                          static_cast<int>(raw * kNumNoteDivisions * 0.999f));

            return static_cast<float>(60.0 / tempo) * kNoteFactors[step];
        }

        return 0.010f * std::pow(2000.0f / 10.0f, raw);   // 10 ms .. 2 s
    }

    void updateTimes()
    {
        const float sr = static_cast<float>(sampleRate);
        const float own = sr * secondsFor(side == 0 ? timeLRaw : timeRRaw);
        const float base = sr * secondsFor(timeLRaw);

        // PING-PONG IS A GEOMETRY, not a mute. The line recirculates at TWICE the base time, and
        // the two sides tap it at one and two - so the left hears repeats at T, 3T, 5T and the
        // right at 2T, 4T, 6T. That IS a ping-pong, and being derived rather than cross-fed it is
        // exact at any block size, where feeding one side into the other would arrive a buffer
        // late and change character with the host's settings.
        const float ppRead = base * (side == 0 ? 1.0f : 2.0f);
        const float ppFeed = base * 2.0f;

        const float maxSamples = sr * 4.0f;

        readSamples = juce::jlimit(2.0f, maxSamples, juce::jmap(pingPong, own, ppRead));
        feedSamples = juce::jlimit(2.0f, maxSamples, juce::jmap(pingPong, own, ppFeed));
    }

    double sampleRate { 44100.0 }, tempo { 0.0 };
    DelayLine line;
    OnePole tone;

    float timeLRaw { 0.35f }, timeRRaw { 0.42f };
    bool synced { false };
    float feedback { 0.4f }, pingPong { 0.0f }, character { 0.0f }, wow { 0.0f };
    float readSamples { 11025.0f }, feedSamples { 11025.0f }, wowPhase { 0.0f };
    int side { 0 };
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
    enum Param { Rate, Depth, Delay, Feedback, Wave, Stereo, HighPass, NumParams };

    void prepare(double sr, int) override
    {
        sampleRate = sr;
        line.prepare(sr, 0.05f);
        lfo.prepare(sr);
        fbCut.setCutoff(20.0f, sr);
        reset();
    }

    void reset() override { line.reset(); lfo.reset(); fbCut.reset(); lastOut = 0.0f; }
    void setChannel(int c) override { side = juce::jlimit(0, 1, c); applyStereo(); }

    void setParam(int index, float v) override
    {
        v = juce::jlimit(0.0f, 1.0f, v);

        switch (index)
        {
            case Rate:     lfo.setRate(0.05f * std::pow(12.0f / 0.05f, v)); break;   // 0.05 .. 12 Hz
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

            // A SELECTOR, not a morph. Half way between a triangle and a square is not a shape
            // anybody asked for, and a knob that lands there is a knob you cannot set.
            case Wave:
                lfo.setShape(static_cast<LfoShape>(
                    juce::jlimit(0, static_cast<int>(LfoShape::NumShapes) - 1,
                                 static_cast<int>(v * static_cast<float>(LfoShape::NumShapes) * 0.999f))));
                break;

            // The two sides an offset apart IS stereo motion - there is nothing else to it, and
            // without it a chorus on a stereo source is just the same chorus twice.
            case Stereo:   stereo = v; applyStereo(); break;

            // Keeping the bottom out of the regeneration. Standard on every flanger worth the name:
            // resonance on the low end turns into a rumble that owns the mix.
            case HighPass: fbCut.setCutoff(juce::jmap(v, 20.0f, 900.0f), sampleRate); break;
            default: break;
        }
    }

    juce::Range<int> getParamRange() const override { return { 0, NumParams }; }

    void process(float* x, int n) override
    {
        const float depthSamples = static_cast<float>(sampleRate) * 0.001f * baseMs * 0.9f * depth;
        const float baseSamples = static_cast<float>(sampleRate) * 0.001f * baseMs;

        for (int s = 0; s < n; ++s)
        {
            const float d = baseSamples + depthSamples * lfo.next();
            const float delayed = line.read(d);

            // The input is backed off as the feedback rises. A resonant flanger otherwise gains
            // over twelve decibels at the extremes - measured, not guessed - and hands the rest of
            // the chain a signal it never asked for. Character stays; the level does not run.
            line.write(x[s] * inputTrim + (lastOut - fbCut.process(lastOut)) * feedback);
            lastOut = delayed;

            x[s] = delayed * outputTrim;
        }
    }

private:
    void applyStereo() { lfo.setPhaseOffset(side == 0 ? 0.0f : stereo * 0.5f); }

    double sampleRate { 44100.0 };
    DelayLine line;
    Lfo lfo;
    OnePole fbCut;

    float depth { 0.5f }, baseMs { 8.0f }, feedback { 0.0f }, stereo { 0.0f };
    float lastOut { 0.0f }, inputTrim { 1.0f }, outputTrim { 1.0f };
    int side { 0 };
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
    enum Param { Drive, Type, Bias, Tone, Focus, NumParams };

    void prepare(double sr, int) override
    {
        sampleRate = sr;
        tilt.setCutoff(4000.0f, sr);
        emphasis.setCutoff(120.0f, sr);
        dcBlockA = 1.0f - (20.0f * juce::MathConstants<float>::twoPi / static_cast<float>(sr));
        reset();
    }

    void reset() override { tilt.reset(); emphasis.reset(); dcX = dcY = 0.0f; }

    void setParam(int index, float v) override
    {
        v = juce::jlimit(0.0f, 1.0f, v);

        switch (index)
        {
            case Drive:     drive = juce::jmap(v, 1.0f, 40.0f); break;
            // A TYPE, not a travel. "Sixty per cent of the way from tube to fold" is not a sound
            // anyone can ask for, and a knob that lands there is the reason these were unusable.
            case Type:
                curve = juce::jlimit(0, static_cast<int>(Curve::NumCurves) - 1,
                                     static_cast<int>(v * static_cast<float>(Curve::NumCurves) * 0.999f));
                break;

            // WHERE it bites. Saturation applied flat colours everything equally, which is the one
            // thing analogue never does - pre-emphasis is how you aim it at the part that should
            // break up while the rest stays intact.
            case Focus:
                focus = v;
                emphasis.setCutoff(juce::jmap(v, 120.0f, 6000.0f), sampleRate);
                break;
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
            // Aimed before it is driven. The emphasis decides WHAT breaks up; it is not a tone
            // control on the way out, which is what TONE below is for.
            const float low = emphasis.process(x[s]);
            const float aimed = juce::jmap(focus, x[s], x[s] - low);

            float v = shape(aimed * drive + bias) * makeup;

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
            const float tilted = tilt.process(v);
            x[s] = juce::jmap(tone, tilted, v);
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
    /** The five curves, each a different way of running out of room. */
    enum class Curve { Tube, Tape, Transistor, Clip, Fold, NumCurves };

    float shape(float v) const
    {
        switch (static_cast<Curve>(curve))
        {
            // Asymmetric and soft: a valve leans on one half of the wave harder than the other,
            // which is where its even harmonics come from.
            // The offset is subtracted EXACTLY, not to three decimal places. A curve that returns
            // 0.0004 for an input of zero puts DC on the output of every instance of it, and a
            // chart can hold sixteen - silence in has to be silence out or the plugin hums.
            case Curve::Tube:
            {
                constexpr float lean = 0.12f;
                return std::tanh(v * 0.9f + lean) - std::tanh(lean);
            }

            // Symmetric, softest knee - compresses before it distorts.
            case Curve::Tape:       return std::tanh(v * 0.75f);

            // Harder knee than tape and odd-harmonic: silicon does not ease into it.
            case Curve::Transistor: return v / (1.0f + std::abs(v));

            case Curve::Clip:       return juce::jlimit(-1.0f, 1.0f, v * 0.72f);

            // Past the limit it turns back on itself. Not warmth - damage, which is the point.
            case Curve::Fold:
            default:                return std::sin(juce::jlimit(-6.0f, 6.0f, v));
        }
    }

    double sampleRate { 44100.0 };
    OnePole tilt;

    OnePole emphasis;
    float drive { 4.0f }, bias { 0.0f }, tone { 0.7f }, makeup { 1.0f }, focus { 0.0f };
    float activity { 0.0f };
    int curve { 0 };
    float dcBlockA { 0.999f }, dcX { 0.0f }, dcY { 0.0f };
};

} // namespace invis::dsp
