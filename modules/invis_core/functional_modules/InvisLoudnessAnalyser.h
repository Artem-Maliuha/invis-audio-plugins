#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <cmath>
#include <algorithm>

namespace invis::modules {

/**
 * Shared numeric analyser feeding `InvisLEDMeter`'s RMS / PEAK / LUFS readouts.
 *
 * Deliberately NOT part of the UI atom: the atom is presentational and formats whatever numbers
 * it is handed. All integration lives here, on the audio thread, and crosses to the UI through
 * relaxed atomics.
 *
 * - RMS  : ~300ms sliding mean-square. Matches the ladder's VU-ish decay so the number and the
 *          bar agree with each other instead of telling two different stories.
 * - PEAK : sample peak with an 800ms hold and a slow fall, mirroring the ladder's peak dot.
 * - LUFS : SHORT-TERM (LUFS-S, 3s, K-weighted per BS.1770). Momentary (400ms) is too jumpy to
 *          read as a digit, and Integrated needs an explicit user-facing reset this component
 *          has no business owning.
 *
 * The K-weighting stages are real filters, not a relabelling of the RMS path: a +4dB high shelf
 * at 1681.97Hz followed by a 38.14Hz high-pass, per BS.1770-4. They are redesigned for the
 * actual sample rate rather than using the published 48kHz coefficients verbatim.
 */
/**
 * The three BS.1770 loudness views. They are NOT the same measurement with different smoothing:
 * Integrated is gated and cumulative, which is why it needs its own accumulator and its own reset.
 */
enum class LufsMode {
    Momentary,  // 400ms window - reacts fast, too jumpy to read as a number for long
    ShortTerm,  // 3s window - the readable default
    Integrated  // gated, cumulative over the whole measurement
};

inline const char* getLufsModeName(LufsMode m)
{
    switch (m)
    {
        case LufsMode::Momentary:  return "LUFS-M";
        case LufsMode::Integrated: return "LUFS-I";
        case LufsMode::ShortTerm:
        default:                   return "LUFS-S";
    }
}

class InvisLoudnessAnalyser {
public:
    static constexpr float kSilenceDb = -100.0f; // Reported when below the measurement floor
    static constexpr float kLufsGateDb = -70.0f; // BS.1770 absolute gate

    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;

        kWeightShelf.prepare(spec);
        kWeightHighPass.prepare(spec);

        *kWeightShelf.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf(
            sampleRate, 1681.974450955533, 0.7071752369554196,
            juce::Decibels::decibelsToGain(3.999843853973347f));

        *kWeightHighPass.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass(
            sampleRate, 38.13547087602444, 0.5003270373238773);

        weightedBuffer.setSize(static_cast<int>(spec.numChannels),
                               static_cast<int>(spec.maximumBlockSize), false, false, true);
        reset();
    }

    void reset()
    {
        kWeightShelf.reset();
        kWeightHighPass.reset();

        rmsMeanSquare = 0.0;
        momentaryMeanSquare = 0.0;
        shortTermMeanSquare = 0.0;
        resetIntegrated();
        peakHoldLinear = 0.0f;
        peakHoldTimer = 0.0f;

        rmsDb.store(kSilenceDb, std::memory_order_relaxed);
        peakDb.store(kSilenceDb, std::memory_order_relaxed);
        lufsDb.store(kSilenceDb, std::memory_order_relaxed);
    }

    /**
     * Thread-safe request to drop the integrators and start measuring afresh.
     *
     * Used after a machine-driven level change: the 300ms RMS window still contains the level
     * from BEFORE the change, so any verification measured against it would be reading a blend of
     * the old and new gain and would under-correct.
     */
    void requestReset() { resetRequested.store(true, std::memory_order_relaxed); }

    /** Granular resets, so clicking one readout does not silently clear the others. */
    void requestRmsReset()  { rmsResetRequested.store(true, std::memory_order_relaxed); }
    void requestPeakReset() { peakResetRequested.store(true, std::memory_order_relaxed); }
    void requestLufsReset() { lufsResetRequested.store(true, std::memory_order_relaxed); }

    void setLufsMode(LufsMode mode) { lufsMode.store(mode, std::memory_order_relaxed); }
    LufsMode getLufsMode() const { return lufsMode.load(std::memory_order_relaxed); }

    /** Momentary -> Short-term -> Integrated -> Momentary. */
    LufsMode cycleLufsMode()
    {
        const auto next = [m = getLufsMode()] {
            switch (m)
            {
                case LufsMode::Momentary:  return LufsMode::ShortTerm;
                case LufsMode::ShortTerm:  return LufsMode::Integrated;
                case LufsMode::Integrated:
                default:                   return LufsMode::Momentary;
            }
        }();

        setLufsMode(next);
        return next;
    }

    void process(const juce::AudioBuffer<float>& buffer)
    {
        const int numSamples = buffer.getNumSamples();
        const int numCh = buffer.getNumChannels();
        if (numSamples <= 0 || numCh <= 0) return;

        if (resetRequested.exchange(false, std::memory_order_relaxed))
        {
            rmsMeanSquare = 0.0;
            momentaryMeanSquare = 0.0;
            shortTermMeanSquare = 0.0;
            resetIntegrated();
            peakHoldLinear = 0.0f;
            peakHoldTimer = 0.0f;
        }

        if (rmsResetRequested.exchange(false, std::memory_order_relaxed))
            rmsMeanSquare = 0.0;

        if (peakResetRequested.exchange(false, std::memory_order_relaxed))
        {
            peakHoldLinear = 0.0f;
            peakHoldTimer = 0.0f;
        }

        if (lufsResetRequested.exchange(false, std::memory_order_relaxed))
        {
            momentaryMeanSquare = 0.0;
            shortTermMeanSquare = 0.0;
            resetIntegrated();
        }

        const double blockSeconds = static_cast<double>(numSamples) / sampleRate;

        // --- 1. Plain mean-square + sample peak on the unweighted signal ---
        double sumSquares = 0.0;
        float blockPeak = 0.0f;

        for (int ch = 0; ch < numCh; ++ch)
        {
            const float* data = buffer.getReadPointer(ch);
            for (int i = 0; i < numSamples; ++i)
                sumSquares += static_cast<double>(data[i]) * static_cast<double>(data[i]);

            blockPeak = std::max(blockPeak, buffer.getMagnitude(ch, 0, numSamples));
        }

        const double blockMeanSquare = sumSquares / static_cast<double>(numSamples * numCh);

        // Smooth the LINEAR mean-square, never the dB value: a dB-domain follower hits log(0)
        // on digital silence and produces -inf that then poisons the whole envelope.
        rmsMeanSquare += (blockMeanSquare - rmsMeanSquare) * onePoleCoef(blockSeconds, 0.300);

        rmsDb.store(toDb(std::sqrt(rmsMeanSquare)), std::memory_order_relaxed);

        // --- 2. Peak with hold and slow fall ---
        if (blockPeak >= peakHoldLinear)
        {
            peakHoldLinear = blockPeak;
            peakHoldTimer = 0.8f;
        }
        else if (peakHoldTimer > 0.0f)
        {
            peakHoldTimer -= static_cast<float>(blockSeconds);
        }
        else
        {
            const float fallDbPerSec = 20.0f;
            const float fallenDb = toDb(peakHoldLinear) - fallDbPerSec * static_cast<float>(blockSeconds);
            peakHoldLinear = std::max(blockPeak, juce::Decibels::decibelsToGain(fallenDb));
        }

        peakDb.store(toDb(peakHoldLinear), std::memory_order_relaxed);

        // --- 3. K-weighted short-term loudness ---
        weightedBuffer.setSize(numCh, numSamples, false, false, true);
        for (int ch = 0; ch < numCh; ++ch)
            weightedBuffer.copyFrom(ch, 0, buffer, ch, 0, numSamples);

        juce::dsp::AudioBlock<float> block(weightedBuffer);
        juce::dsp::ProcessContextReplacing<float> ctx(block);
        kWeightShelf.process(ctx);
        kWeightHighPass.process(ctx);

        double weightedSum = 0.0;
        for (int ch = 0; ch < numCh; ++ch)
        {
            const float* data = weightedBuffer.getReadPointer(ch);
            for (int i = 0; i < numSamples; ++i)
                weightedSum += static_cast<double>(data[i]) * static_cast<double>(data[i]);
        }

        // BS.1770 sums per-channel mean squares with channel weights (1.0 for L/R), it does not
        // average across channels - so divide by sample count only.
        const double weightedMeanSquare = weightedSum / static_cast<double>(numSamples);

        momentaryMeanSquare += (weightedMeanSquare - momentaryMeanSquare) * onePoleCoef(blockSeconds, 0.400);
        shortTermMeanSquare += (weightedMeanSquare - shortTermMeanSquare) * onePoleCoef(blockSeconds, 3.000);

        // --- Integrated: gated and cumulative, per BS.1770-4 ---
        //
        // Accumulated as a HISTOGRAM of 400ms block loudnesses rather than a running list: the
        // integrated value needs a relative gate computed from all past blocks, and keeping every
        // block would grow without bound over a long session.
        integratedBlockTimer += blockSeconds;
        integratedBlockMeanSquare += weightedMeanSquare * blockSeconds;
        integratedBlockSeconds += blockSeconds;

        if (integratedBlockTimer >= 0.400)
        {
            const double blockMs = integratedBlockMeanSquare / std::max(1.0e-12, integratedBlockSeconds);
            addIntegratedBlock(blockMs);

            integratedBlockTimer = 0.0;
            integratedBlockMeanSquare = 0.0;
            integratedBlockSeconds = 0.0;
        }

        double activeMeanSquare = shortTermMeanSquare;
        float reported = kSilenceDb;

        switch (getLufsMode())
        {
            case LufsMode::Momentary:
                activeMeanSquare = momentaryMeanSquare;
                break;
            case LufsMode::Integrated:
                reported = computeIntegrated();
                break;
            case LufsMode::ShortTerm:
            default:
                break;
        }

        if (getLufsMode() != LufsMode::Integrated)
        {
            reported = (activeMeanSquare > 1.0e-12)
                         ? static_cast<float>(-0.691 + 10.0 * std::log10(activeMeanSquare))
                         : kSilenceDb;
        }

        lufsDb.store(reported < kLufsGateDb ? kSilenceDb : reported, std::memory_order_relaxed);
    }

    float getRmsDb() const  { return rmsDb.load(std::memory_order_relaxed); }
    float getPeakDb() const { return peakDb.load(std::memory_order_relaxed); }
    float getLufsDb() const { return lufsDb.load(std::memory_order_relaxed); }

private:
    static float toDb(float linear)
    {
        return linear > 1.0e-6f ? juce::Decibels::gainToDecibels(linear) : kSilenceDb;
    }

    /** One-pole coefficient for a given time constant, stable for any block size. */
    static double onePoleCoef(double blockSeconds, double timeConstantSeconds)
    {
        return 1.0 - std::exp(-blockSeconds / timeConstantSeconds);
    }

    double sampleRate { 44100.0 };

    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>> kWeightShelf;
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>> kWeightHighPass;
    juce::AudioBuffer<float> weightedBuffer;

    // --- Integrated loudness: gated histogram (0.1 LU bins from -70 to +5 LUFS) ---
    static constexpr int kNumHistBins = 751;
    static constexpr double kHistFloorLufs = -70.0;
    static constexpr double kHistBinLu = 0.1;

    void resetIntegrated()
    {
        std::fill(std::begin(histogram), std::end(histogram), 0);
        integratedBlockTimer = 0.0;
        integratedBlockMeanSquare = 0.0;
        integratedBlockSeconds = 0.0;
    }

    void addIntegratedBlock(double meanSquare)
    {
        if (meanSquare <= 1.0e-12) return;

        const double lufs = -0.691 + 10.0 * std::log10(meanSquare);
        if (lufs < kHistFloorLufs) return; // absolute gate: below -70 LUFS the block does not count

        const int bin = juce::jlimit(0, kNumHistBins - 1,
                                     static_cast<int>((lufs - kHistFloorLufs) / kHistBinLu));
        ++histogram[bin];
    }

    static double binToLufs(int bin) { return kHistFloorLufs + bin * kHistBinLu; }

    float computeIntegrated() const
    {
        // Pass 1: ungated mean of everything above the absolute gate
        double sumPower = 0.0;
        juce::uint64 count = 0;

        for (int i = 0; i < kNumHistBins; ++i)
        {
            if (histogram[i] == 0) continue;
            sumPower += histogram[i] * std::pow(10.0, (binToLufs(i) + 0.691) / 10.0);
            count += histogram[i];
        }

        if (count == 0) return kSilenceDb;

        const double ungatedLufs = -0.691 + 10.0 * std::log10(sumPower / static_cast<double>(count));

        // Pass 2: relative gate at -10 LU below the ungated mean
        const double relativeGate = ungatedLufs - 10.0;

        double gatedPower = 0.0;
        juce::uint64 gatedCount = 0;

        for (int i = 0; i < kNumHistBins; ++i)
        {
            if (histogram[i] == 0 || binToLufs(i) < relativeGate) continue;
            gatedPower += histogram[i] * std::pow(10.0, (binToLufs(i) + 0.691) / 10.0);
            gatedCount += histogram[i];
        }

        if (gatedCount == 0) return kSilenceDb;

        return static_cast<float>(-0.691 + 10.0 * std::log10(gatedPower / static_cast<double>(gatedCount)));
    }

    double rmsMeanSquare { 0.0 };
    double momentaryMeanSquare { 0.0 };
    double shortTermMeanSquare { 0.0 };

    juce::uint64 histogram[kNumHistBins] {};
    double integratedBlockTimer { 0.0 };
    double integratedBlockMeanSquare { 0.0 };
    double integratedBlockSeconds { 0.0 };

    float peakHoldLinear { 0.0f };
    float peakHoldTimer { 0.0f };

    std::atomic<LufsMode> lufsMode { LufsMode::ShortTerm };
    std::atomic<bool> rmsResetRequested { false };
    std::atomic<bool> peakResetRequested { false };
    std::atomic<bool> lufsResetRequested { false };
    std::atomic<bool> resetRequested { false };
    std::atomic<float> rmsDb  { kSilenceDb };
    std::atomic<float> peakDb { kSilenceDb };
    std::atomic<float> lufsDb { kSilenceDb };
};

} // namespace invis::modules
