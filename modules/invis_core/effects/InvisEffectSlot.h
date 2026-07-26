#pragma once

#include "InvisAlgorithms.h"
#include "../functional_modules/InvisFilterCascade.h"
#include <memory>

namespace invis::dsp {

/**
 * REVERB AND DELAY ARE NOT ONE ALGORITHM.
 *
 * They were, on the reasoning that a delay is a reverb at a long size - which is true of the maths
 * and false of the instrument. A reverb wants diffusion, damping and a decay you do not count; a
 * delay wants a time you can set to the bar, two of them for the two sides, a feedback path with
 * its own colour, and repeats that alternate. Stretching a comb bank until it echoes gives you an
 * echo you cannot place in time and cannot shape on its way round.
 *
 * They stay in one COLOUR family - the ear groups them as space, and the palette follows the ear -
 * while being two machines underneath.
 */
enum class AlgorithmKind { Reverb, Delay, Modulation, Phaser, Saturation };

/** What an algorithm's knobs are called and what their numbers mean. Display side only. */
const EffectParam* getAlgorithmParams(AlgorithmKind kind, int& count);

/**
 * A catalogue entry, as DSP.
 *
 * The names in InvisEffectPalette are the product's; these are the settings that make them sound
 * like what they are called. HALL and DELAY are the same algorithm at different sizes, and TAPE and
 * CRUSH the same curve at different characters - so a recipe is a set of defaults, not a class.
 */
struct EffectRecipe {
    const char* name;
    AlgorithmKind kind;
    std::array<float, kMaxEffectParams> defaults;

    /**
     * The block's mix, and it belongs to the RECIPE rather than to a global default.
     *
     * A star at the entry gets its balance from the observer's send, so its mix hardly matters
     * there. In the middle of a chain the hop is a full send and the chart gives it no mix control
     * at all - so this is the only thing deciding how much of the effect is in what passes
     * through, and one value cannot be right for every algorithm.
     *
     * A saturator IS the sound at its stage and wants all of it. A reverb at full wet replaces
     * what arrived instead of surrounding it. And a flanger at full wet stops being a flanger:
     * comb interference is what you get from MIXING dry with delayed, so removing the dry removes
     * the effect and leaves a detuned copy.
     */
    float dryWet { 1.0f };
};

/** Null when the name is not in the catalogue - a chart restored from an older state may hold one. */
const EffectRecipe* findRecipe(const juce::String& name);

/**
 * THE BLOCK A STAR IS, as opposed to the algorithm inside it.
 *
 * Every star has band limiting and a mix whatever algorithm is dropped in, so the slot owns them
 * and no algorithm has to implement them. The topology is the one the chart promises:
 *
 *     in ──┬──────────────────────────────► dry
 *          └── HPF ── LPF ── [ algorithm ] ► wet
 *
 * THE DRY TAP IS TAKEN BEFORE THE FILTERS. That is the whole point of them being here: they shape
 * what FEEDS the effect without touching the signal that bypasses it, so you can send only the top
 * of a sound into a reverb and keep the bottom dry and intact. Filtering after the split would just
 * be an EQ on the output, which the chassis already has.
 */
class InvisEffectSlot {
public:
    void prepare(double sampleRate, int maxBlockSize);
    void reset();

    /** Message thread. Allocates; never call from the audio callback. */
    void setAlgorithm(const juce::String& effectName);

    juce::String getAlgorithmName() const { return name; }
    AlgorithmKind getKind() const { return kind; }

    void setParam(int index, float normalized);
    float getParam(int index) const;

    /** Which side of the pair this slot is. Ping-pong needs to know; nothing else does. */
    void setChannel(int c) { channel = c; if (effect != nullptr) effect->setChannel(c); }

    /** Host tempo, for anything that syncs to the bar. */
    void setTempo(double bpm) { tempo = bpm; if (effect != nullptr) effect->setTempo(bpm); }

    void setFilters(float hpfNormalized, float lpfNormalized);
    void setDryWet(float mix) { dryWet = juce::jlimit(0.0f, 1.0f, mix); }

    /**
     * In place, one channel, AT UNITY.
     *
     * The slot does not know how much of it you are hearing - that is the chart's business, and it
     * belongs to the engine's crossfade rather than to a gain in here. Scaling the block's own
     * input by the chart amount turned proximity into a VOLUME on the whole path: walk away and the
     * source itself faded out, walk up and the effect appeared at once instead of blending in.
     */
    void process(float* samples, int numSamples);

    bool isReady() const { return effect != nullptr; }

    /** 0..1 while the algorithm has something to report, -1 when it has not. Audio thread writes. */
    float getActivity() const { return effect != nullptr ? effect->getActivity() : -1.0f; }

private:
    std::unique_ptr<InvisEffect> effect;
    AlgorithmKind kind { AlgorithmKind::Reverb };
    juce::String name;
    int channel { 0 };
    double tempo { 0.0 };

    modules::InvisFilterCascade hpf, lpf;
    bool hpfActive { false }, lpfActive { false };

    std::array<float, kMaxEffectParams> params { };
    float dryWet { 1.0f };

    std::vector<float> dryScratch;
    double sr { 44100.0 };
    int blockSize { 512 };
};

} // namespace invis::dsp
