#pragma once

#include "InvisAlgorithms.h"
#include "../functional_modules/InvisFilterCascade.h"
#include <memory>

namespace invis::dsp {

enum class AlgorithmKind { Spatial, Modulation, Saturation };

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

private:
    std::unique_ptr<InvisEffect> effect;
    AlgorithmKind kind { AlgorithmKind::Spatial };
    juce::String name;

    modules::InvisFilterCascade hpf, lpf;
    bool hpfActive { false }, lpfActive { false };

    std::array<float, kMaxEffectParams> params { };
    float dryWet { 1.0f };

    std::vector<float> dryScratch;
    double sr { 44100.0 };
    int blockSize { 512 };
};

} // namespace invis::dsp
