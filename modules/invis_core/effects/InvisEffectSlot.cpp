#include "InvisEffectSlot.h"
#include "../design_system/InvisEffectPalette.h"

namespace invis::dsp {

namespace {

const EffectParam kReverbParams[] = {
    { "SIZE",     8.0f,  900.0f, 0.35f, " MS", 0, true  },
    { "DECAY",    0.0f,  100.0f, 0.45f, "%",   0, false },
    { "DAMPING",  0.0f,  100.0f, 0.40f, "%",   0, false },
    { "MOD",      0.0f,  100.0f, 0.20f, "%",   0, false },
    { "SPREAD",   0.0f,  100.0f, 0.60f, "%",   0, false },
};

const char* const kSyncLabels[] = { "MS", "SYNC" };

const EffectParam kDelayParams[] = {
    { "TIME L",   10.0f, 2000.0f, 0.35f, " MS", 0, true  },
    { "TIME R",   10.0f, 2000.0f, 0.42f, " MS", 0, true  },
    { "SYNC",      0.0f,    1.0f, 0.00f, "",    0, false, 2, kSyncLabels },
    { "FEEDBACK",  0.0f,  100.0f, 0.40f, "%",   0, false },
    { "PING PONG", 0.0f,  100.0f, 0.00f, "%",   0, false },
    { "TONE",      0.0f,  100.0f, 0.70f, "%",   0, false },
    { "CHARACTER", 0.0f,  100.0f, 0.00f, "%",   0, false },
    { "WOW",       0.0f,  100.0f, 0.00f, "%",   0, false },
};

const EffectParam kModulationParams[] = {
    { "RATE",     0.05f,  12.0f, 0.40f, " HZ", 2, true  },
    { "DEPTH",    0.0f,  100.0f, 0.50f, "%",   0, false },
    { "DELAY",    0.4f,   26.0f, 0.30f, " MS", 1, false },
    { "FEEDBACK", -100.0f, 100.0f, 0.50f, "%", 0, false },
    { "SHAPE",    0.0f,  100.0f, 0.00f, "%",   0, false },
};

const EffectParam kSaturationParams[] = {
    { "DRIVE",     1.0f,  40.0f, 0.30f, "X",  1, true  },
    { "CHARACTER", 0.0f, 100.0f, 0.00f, "%",  0, false },
    { "BIAS",   -100.0f, 100.0f, 0.50f, "%",  0, false },
    { "TONE",      0.0f, 100.0f, 0.70f, "%",  0, false },
};

// The recipes. A family is one COLOUR; an entry is a place to stand in one of its algorithms.
//
// The trailing number is the block's DRY/WET, and it is per entry for a reason - see EffectRecipe.
// TREMOLO is the one modulation entry at full wet: it is an amplitude effect, and mixing it with
// dry does not soften it, it cancels it.
const EffectRecipe kRecipes[] = {
    // REVERBS                              size  decay damp  mod   spread
    { "HALL",    AlgorithmKind::Reverb,   { 0.30f, 0.72f, 0.45f, 0.28f, 0.75f }, 0.35f },
    { "CHAMBER", AlgorithmKind::Reverb,   { 0.22f, 0.62f, 0.55f, 0.16f, 0.60f }, 0.35f },
    { "PLATE",   AlgorithmKind::Reverb,   { 0.16f, 0.68f, 0.20f, 0.10f, 0.35f }, 0.38f },
    { "SPRING",  AlgorithmKind::Reverb,   { 0.12f, 0.74f, 0.62f, 0.55f, 0.20f }, 0.40f },

    // DELAYS. One algorithm, three characters and a routing - which is what a delay actually is.
    //                                     timeL  timeR  sync   fb     pp     tone   char   wow
    { "DIGITAL", AlgorithmKind::Delay,   { 0.35f, 0.42f, 0.00f, 0.38f, 0.00f, 0.92f, 0.00f, 0.00f }, 0.32f },
    { "ANALOG",  AlgorithmKind::Delay,   { 0.38f, 0.45f, 0.00f, 0.45f, 0.00f, 0.55f, 0.55f, 0.10f }, 0.34f },
    { "TAPE",    AlgorithmKind::Delay,   { 0.40f, 0.47f, 0.00f, 0.50f, 0.00f, 0.38f, 0.85f, 0.42f }, 0.34f },
    { "PINGPONG",AlgorithmKind::Delay,   { 0.35f, 0.35f, 0.00f, 0.44f, 1.00f, 0.62f, 0.30f, 0.08f }, 0.36f },

    //                                        rate  depth delay fb    shape
    { "CHORUS",  AlgorithmKind::Modulation, { 0.28f, 0.45f, 0.42f, 0.50f, 0.00f }, 0.50f },
    { "FLANGER", AlgorithmKind::Modulation, { 0.22f, 0.80f, 0.06f, 0.82f, 0.85f }, 0.50f },
    { "PHASER",  AlgorithmKind::Modulation, { 0.34f, 0.65f, 0.02f, 0.18f, 0.40f }, 0.50f },
    { "TREMOLO", AlgorithmKind::Modulation, { 0.55f, 0.95f, 0.00f, 0.50f, 1.00f }, 1.00f },

    //                                        drive char  bias  tone
    { "TAPE SAT",AlgorithmKind::Saturation, { 0.28f, 0.10f, 0.50f, 0.45f }, 1.00f },
    { "TUBE",    AlgorithmKind::Saturation, { 0.34f, 0.22f, 0.62f, 0.72f }, 1.00f },
    { "DRIVE",   AlgorithmKind::Saturation, { 0.55f, 0.55f, 0.50f, 0.80f }, 1.00f },
    { "CRUSH",   AlgorithmKind::Saturation, { 0.82f, 0.92f, 0.50f, 0.90f }, 1.00f },
};

} // namespace

const EffectParam* getAlgorithmParams(AlgorithmKind kind, int& count)
{
    switch (kind)
    {
        case AlgorithmKind::Modulation:
            count = static_cast<int>(std::size(kModulationParams));
            return kModulationParams;

        case AlgorithmKind::Saturation:
            count = static_cast<int>(std::size(kSaturationParams));
            return kSaturationParams;

        case AlgorithmKind::Delay:
            count = static_cast<int>(std::size(kDelayParams));
            return kDelayParams;

        case AlgorithmKind::Reverb:
        default:
            count = static_cast<int>(std::size(kReverbParams));
            return kReverbParams;
    }
}

const EffectRecipe* findRecipe(const juce::String& name)
{
    for (const auto& r : kRecipes)
        if (name == r.name) return &r;

    return nullptr;
}

// =================================================================================================

void InvisEffectSlot::prepare(double sampleRate, int maxBlockSize)
{
    sr = sampleRate;
    blockSize = std::max(1, maxBlockSize);
    dryScratch.assign(static_cast<size_t>(blockSize), 0.0f);

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(blockSize);
    spec.numChannels = 1;

    hpf.prepare(spec);
    lpf.prepare(spec);

    if (effect != nullptr) effect->prepare(sampleRate, blockSize);
}

void InvisEffectSlot::reset()
{
    hpf.reset();
    lpf.reset();
    if (effect != nullptr) effect->reset();
}

void InvisEffectSlot::setAlgorithm(const juce::String& effectName)
{
    const auto* recipe = findRecipe(effectName);
    if (recipe == nullptr) return;

    // Rebuilt only when the FAMILY changes. Swapping HALL for PLATE is a settings change, and
    // tearing down the delay lines for it would cut the tail of something you are listening to.
    if (effect == nullptr || recipe->kind != kind || name.isEmpty())
    {
        switch (recipe->kind)
        {
            case AlgorithmKind::Delay:      effect = std::make_unique<DelayEffect>(); break;
            case AlgorithmKind::Modulation: effect = std::make_unique<ModulationEffect>(); break;
            case AlgorithmKind::Saturation: effect = std::make_unique<SaturationEffect>(); break;
            case AlgorithmKind::Reverb:
            default:                        effect = std::make_unique<SpatialEffect>(); break;
        }

        kind = recipe->kind;
        effect->prepare(sr, blockSize);
        effect->setChannel(channel);
        effect->setTempo(tempo);
    }

    name = effectName;

    for (int i = 0; i < kMaxEffectParams; ++i)
        setParam(i, recipe->defaults[static_cast<size_t>(i)]);
}

void InvisEffectSlot::setParam(int index, float normalized)
{
    if (index < 0 || index >= kMaxEffectParams) return;

    params[static_cast<size_t>(index)] = juce::jlimit(0.0f, 1.0f, normalized);
    if (effect != nullptr) effect->setParam(index, params[static_cast<size_t>(index)]);
}

float InvisEffectSlot::getParam(int index) const
{
    return (index >= 0 && index < kMaxEffectParams) ? params[static_cast<size_t>(index)] : 0.0f;
}

void InvisEffectSlot::setFilters(float hpfNormalized, float lpfNormalized)
{
    // Same mapping the chassis filters use, so a star and the sidebar speak one language.
    hpfActive = hpfNormalized > 0.005f;
    lpfActive = lpfNormalized < 0.995f;

    // THE SAME CURVE THE KNOB DRAWS. See ui::starFilterRange - the mapping lives in one place so
    // the frequency shown and the frequency applied cannot disagree.
    if (hpfActive)
        hpf.update(ui::starFilterRange(true).convertFrom0to1(juce::jlimit(0.0f, 1.0f, hpfNormalized)),
                   modules::FilterSlope::Slope12, true);

    if (lpfActive)
        lpf.update(ui::starFilterRange(false).convertFrom0to1(juce::jlimit(0.0f, 1.0f, lpfNormalized)),
                   modules::FilterSlope::Slope12, false);
}

void InvisEffectSlot::process(float* samples, int numSamples)
{
    if (effect == nullptr || numSamples <= 0) return;

    if (static_cast<int>(dryScratch.size()) < numSamples)
        dryScratch.assign(static_cast<size_t>(numSamples), 0.0f);

    // THE DRY TAP, TAKEN FIRST. Everything below only shapes the copy that feeds the algorithm.
    for (int i = 0; i < numSamples; ++i)
        dryScratch[static_cast<size_t>(i)] = samples[i];

    juce::AudioBuffer<float> view(&samples, 1, numSamples);

    if (hpfActive) hpf.process(view);
    if (lpfActive) lpf.process(view);

    effect->process(samples, numSamples);

    for (int i = 0; i < numSamples; ++i)
    {
        const float wet = samples[i];
        samples[i] = dryScratch[static_cast<size_t>(i)] * (1.0f - dryWet) + wet * dryWet;

        // A star can be dragged while its own tail is feeding back, and a chain can put several
        // of them in series. One guard here is cheaper than trusting every algorithm's stability
        // at every setting the chart can reach in one gesture.
        if (!std::isfinite(samples[i])) samples[i] = 0.0f;
    }
}

} // namespace invis::dsp
