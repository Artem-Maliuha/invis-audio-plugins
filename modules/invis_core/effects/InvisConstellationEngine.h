#pragma once

#include "InvisEffectSlot.h"
#include "../ui_atoms/InvisConstellation.h"
#include <atomic>

namespace invis::dsp {

/**
 * THE ROUTING, AS THE AUDIO THREAD SEES IT.
 *
 * Flat, fixed-size and trivially copyable, because it crosses a thread boundary every time you move
 * a star. The chart's own structures - figures, clusters, links - are derived on demand and full of
 * vectors; asking the audio thread to walk them would mean either allocating in the callback or
 * locking against a UI that changes on every mouse move.
 *
 * A stage is a set of slots that run IN PARALLEL and sum. Stages run in series. That is the whole
 * routing law expressed in the one shape a callback can execute without thinking.
 */
struct RoutingPlan {
    struct Entry { int star { -1 }; float gain { 0.0f }; };

    struct Stage {
        int count { 0 };
        Entry entries[ui::InvisConstellation::kMaxNodes];
    };

    struct Stream {
        int numStages { 0 };
        Stage stages[ui::InvisConstellation::kMaxNodes];
    };

    Stream streams[2];
    int numStreams { 1 };
    ui::ConstellationChannelMode mode { ui::ConstellationChannelMode::Linked };
};

/**
 * Runs the chart.
 *
 * ONE SLOT PER STAR PER STREAM. See InvisEffect for why a star cannot be one stereo instance: in
 * the split modes the two streams enter the chart at different stars and walk it in different
 * orders, so an instance would have to be in two places in two chains at once.
 */
class InvisConstellationEngine {
public:
    static constexpr int kMaxStars = ui::InvisConstellation::kMaxNodes;

    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();

    /** Message thread. Publishes a new route for the callback to pick up on its next block. */
    void setPlan(const RoutingPlan& plan);

    /** Message thread. Allocates. */
    /** Host tempo, so a synced delay lands on the bar. Audio thread; cheap when unchanged. */
    void setTempo(double bpm);

    void setStarAlgorithm(int star, const juce::String& effectName);
    void setStarBlock(int star, float hpf, float lpf, float dryWet);
    void setStarParam(int star, int paramIndex, float normalized);

    juce::String getStarAlgorithm(int star) const;
    float getStarParam(int star, int paramIndex) const;
    AlgorithmKind getStarKind(int star) const;

    /** The louder of the two streams: a star working hard on one channel is working hard. */
    float getStarActivity(int star) const;

    void process(juce::AudioBuffer<float>& buffer);

private:
    void runStream(float* samples, int numSamples, const RoutingPlan::Stream& stream, int streamIndex);

    // [stream][star]. Two full sets, because the streams are two different chains.
    std::unique_ptr<InvisEffectSlot> slots[2][kMaxStars];

    // Double buffered with an atomic index: the writer fills the spare and flips, the reader takes
    // whichever index it sees. No lock, and a plan is never read half-written.
    RoutingPlan plans[2];
    std::atomic<int> livePlan { 0 };

    juce::AudioBuffer<float> stageBuffer, sumBuffer, streamBuffer;

    double sampleRate { 44100.0 };
    int maxBlock { 512 };
    std::atomic<bool> ready { false };
    double lastTempo { -1.0 };
};

} // namespace invis::dsp
