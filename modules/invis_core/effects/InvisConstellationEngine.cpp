#include "InvisConstellationEngine.h"

namespace invis::dsp {

void InvisConstellationEngine::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    maxBlock = static_cast<int>(spec.maximumBlockSize);

    for (int s = 0; s < 2; ++s)
        for (int i = 0; i < kMaxStars; ++i)
        {
            if (slots[s][i] == nullptr) slots[s][i] = std::make_unique<InvisEffectSlot>();
            slots[s][i]->prepare(sampleRate, maxBlock);
        }

    stageBuffer.setSize(1, maxBlock, false, true, true);
    sumBuffer.setSize(1, maxBlock, false, true, true);
    streamBuffer.setSize(2, maxBlock, false, true, true);

    ready.store(true, std::memory_order_release);
}

void InvisConstellationEngine::reset()
{
    for (int s = 0; s < 2; ++s)
        for (int i = 0; i < kMaxStars; ++i)
            if (slots[s][i] != nullptr) slots[s][i]->reset();
}

void InvisConstellationEngine::setPlan(const RoutingPlan& plan)
{
    const int spare = 1 - livePlan.load(std::memory_order_relaxed);

    plans[spare] = plan;
    livePlan.store(spare, std::memory_order_release);
}

void InvisConstellationEngine::setStarAlgorithm(int star, const juce::String& effectName)
{
    if (star < 0 || star >= kMaxStars) return;

    // BOTH STREAMS, always. They are two instances of the same star and must never disagree about
    // what they are - a chart where L is a reverb and R is a flanger is not something the UI can
    // express, so it must not be something the engine can hold.
    for (int s = 0; s < 2; ++s)
        if (slots[s][star] != nullptr) slots[s][star]->setAlgorithm(effectName);
}

void InvisConstellationEngine::setStarBlock(int star, float hpf, float lpf, float dryWet)
{
    if (star < 0 || star >= kMaxStars) return;

    for (int s = 0; s < 2; ++s)
        if (slots[s][star] != nullptr)
        {
            slots[s][star]->setFilters(hpf, lpf);
            slots[s][star]->setDryWet(dryWet);
        }
}

void InvisConstellationEngine::setStarParam(int star, int paramIndex, float normalized)
{
    if (star < 0 || star >= kMaxStars) return;

    for (int s = 0; s < 2; ++s)
        if (slots[s][star] != nullptr) slots[s][star]->setParam(paramIndex, normalized);
}

juce::String InvisConstellationEngine::getStarAlgorithm(int star) const
{
    if (star < 0 || star >= kMaxStars || slots[0][star] == nullptr) return {};
    return slots[0][star]->getAlgorithmName();
}

float InvisConstellationEngine::getStarParam(int star, int paramIndex) const
{
    if (star < 0 || star >= kMaxStars || slots[0][star] == nullptr) return 0.0f;
    return slots[0][star]->getParam(paramIndex);
}

AlgorithmKind InvisConstellationEngine::getStarKind(int star) const
{
    if (star < 0 || star >= kMaxStars || slots[0][star] == nullptr) return AlgorithmKind::Spatial;
    return slots[0][star]->getKind();
}

float InvisConstellationEngine::getStarActivity(int star) const
{
    if (star < 0 || star >= kMaxStars) return -1.0f;

    float best = -1.0f;
    for (int s = 0; s < 2; ++s)
        if (slots[s][star] != nullptr) best = std::max(best, slots[s][star]->getActivity());

    return best;
}

void InvisConstellationEngine::runStream(float* samples, int numSamples,
                                          const RoutingPlan::Stream& stream, int streamIndex)
{
    if (stream.numStages <= 0) return;

    auto* stage = stageBuffer.getWritePointer(0);
    auto* sum = sumBuffer.getWritePointer(0);

    for (int st = 0; st < stream.numStages; ++st)
    {
        const auto& s = stream.stages[st];
        if (s.count <= 0) continue;

        juce::FloatVectorOperations::clear(sum, numSamples);
        float sent = 0.0f;

        // PARALLEL WITHIN A STAGE. Each member gets a COPY of what arrived - it is a send, not a
        // hand-off - and the outputs add. Feeding them in series here would quietly turn every
        // closed figure back into a chain, which is the one distinction the chart exists to draw.
        for (int e = 0; e < s.count; ++e)
        {
            const auto& entry = s.entries[e];
            if (entry.star < 0 || entry.star >= kMaxStars) continue;

            auto* slot = slots[streamIndex][entry.star].get();
            if (slot == nullptr || !slot->isReady()) continue;

            juce::FloatVectorOperations::copy(stage, samples, numSamples);
            slot->process(stage, numSamples);
            juce::FloatVectorOperations::addWithMultiply(sum, stage, entry.gain, numSamples);

            sent += entry.gain;
        }

        // A SEND, NOT A FADER. The chart amount decides how much of this stage you HEAR, so what
        // is not sent stays as it arrived: the effect blends in as the observer approaches instead
        // of the whole path getting quieter as it walks away.
        //
        // At a full send - which is what every hop between stages is - nothing is kept and the
        // stage output is purely what it produced, exactly as the routing law says.
        const float keep = 1.0f - juce::jlimit(0.0f, 1.0f, sent);

        juce::FloatVectorOperations::multiply(samples, keep, numSamples);
        juce::FloatVectorOperations::add(samples, sum, numSamples);
    }
}

void InvisConstellationEngine::process(juce::AudioBuffer<float>& buffer)
{
    if (!ready.load(std::memory_order_acquire)) return;

    const int n = buffer.getNumSamples();
    const int channels = buffer.getNumChannels();
    if (n <= 0 || channels <= 0 || n > maxBlock) return;

    const auto& plan = plans[livePlan.load(std::memory_order_acquire)];
    if (plan.streams[0].numStages <= 0 && plan.streams[1].numStages <= 0) return;

    const bool split = ui::constellationModeHasTwoObservers(plan.mode);
    const bool midSide = (plan.mode == ui::ConstellationChannelMode::MidSide
                       || plan.mode == ui::ConstellationChannelMode::MidSideLinked);

    auto* left = buffer.getWritePointer(0);
    auto* right = channels > 1 ? buffer.getWritePointer(1) : nullptr;

    // M/S is an encode, not a mode: the chart routes two streams either way, and the only question
    // is which two. Doing it here rather than inside the engine's loop keeps the routing code from
    // needing to know that mid and side are not channels.
    if (midSide && right != nullptr)
        for (int i = 0; i < n; ++i)
        {
            const float m = (left[i] + right[i]) * 0.5f;
            const float s = (left[i] - right[i]) * 0.5f;
            left[i] = m; right[i] = s;
        }

    if (split && right != nullptr)
    {
        runStream(left, n, plan.streams[0], 0);
        runStream(right, n, plan.streams[1], 1);
    }
    else
    {
        // One listener: both channels take the SAME route, but through their own instances, so a
        // stereo image survives - two mono reverbs fed L and R are still a stereo reverb.
        runStream(left, n, plan.streams[0], 0);
        if (right != nullptr) runStream(right, n, plan.streams[0], 1);
    }

    if (midSide && right != nullptr)
        for (int i = 0; i < n; ++i)
        {
            const float m = left[i], s = right[i];
            left[i] = m + s; right[i] = m - s;
        }
}

} // namespace invis::dsp
