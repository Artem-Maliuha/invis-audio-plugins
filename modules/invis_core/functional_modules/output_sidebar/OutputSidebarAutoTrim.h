#pragma once

#include "../../framework/InvisAutoRoutine.h"
#include <algorithm>

namespace invis::modules {

/**
 * AUTO output-stage trim.
 *
 * Built on the SAME `framework::InvisAutoRoutine` as the input calibrator, so it inherits the
 * identical character: glided reset, a hold to let the meters settle, and a logarithmically
 * decelerating glide onto the result. It previously had none of that - it was two bare value
 * jumps - which is exactly the kind of per-module divergence the framework layer exists to stop.
 *
 *   RESET  0.50s  output gain glides back to unity
 *   HOLD   1.00s  stillness, so the meters settle on the un-trimmed signal
 *   SETTLE 0.70s  the 300ms RMS window catches up
 *   GAIN   3.00s  output gain glides until the measured RMS lands on the gain-stage value
 *
 * There is no sweep here: the output stage has no filters to place, only a level to land.
 */
class OutputSidebarAutoTrim {
public:
    static constexpr float kResetDuration  = 0.50f;
    static constexpr float kHoldDuration   = 1.00f;
    static constexpr float kSettleDuration = 0.70f;
    static constexpr float kGainDuration   = 3.00f;

    static constexpr float kMaxGainDb = 24.0f;

    /** Slow closed-loop correction after the main glide lands. */
    static constexpr float kVerifyDuration = 2.00f;
    static constexpr float kVerifySettle   = 0.60f; // let the freshly-reset RMS window refill
    static constexpr float kVerifyRatePerSec = 0.9f;

    OutputSidebarAutoTrim() { buildRoutine(); }

    bool isRunning() const { return routine.isRunning(); }

    void setRunningChangedCallback(std::function<void(bool)> cb)
    {
        routine.onRunningChanged = std::move(cb);
    }

    /** Fired once when the main glide lands, so the host can drop the metering integrators:
        the 300ms RMS window still holds the pre-correction level and would under-report. */
    std::function<void()> onRequestMeterReset;

    bool shouldMoveGain() const { return isRunning() && movingGain; }
    float getGainDb() const { return gainDb; }

    void start(float currentGainDb)
    {
        resetGainFrom = currentGainDb;
        gainDb = currentGainDb;
        targetCaptured = false;
        routine.start();
    }

    void cancel() { routine.cancel(); }

    void tick(float dt, float measuredRmsDb, float targetRmsDb, float currentGainDb)
    {
        lastRmsDb = measuredRmsDb;
        lastTargetDb = targetRmsDb;
        liveGainDb = currentGainDb;

        movingGain = false;
        routine.tick(dt);
    }

private:
    void buildRoutine()
    {
        using Phase = framework::InvisAutoRoutine::Phase;

        routine.setPhases({
            Phase { "RESET", kResetDuration, [this](float p, float) {
                gainDb = ui::motion::glide(resetGainFrom, 0.0f, p);
                movingGain = true;
            }, nullptr },

            Phase { "HOLD", kHoldDuration, [this](float, float) {
                gainDb = 0.0f;
                movingGain = true;
            }, nullptr },

            Phase { "SETTLE", kSettleDuration, nullptr, nullptr },

            Phase { "GAIN", kGainDuration,
                [this](float p, float) {
                    if (!targetCaptured) return;
                    gainDb = ui::motion::glide(startGainDb, targetGainDb, p);
                    movingGain = true;
                },
                [this]() {
                    if (lastRmsDb <= -90.0f) return; // no usable signal - leave the gain alone

                    startGainDb = liveGainDb;
                    targetGainDb = std::clamp(liveGainDb + (lastTargetDb - lastRmsDb),
                                              -kMaxGainDb, kMaxGainDb);
                    targetCaptured = true;
                }},

            // VERIFY: same slow closed-loop check as the input side - the main glide is open-loop,
            // so a residual is expected and gets walked out rather than snapped.
            Phase { "VERIFY", kVerifyDuration,
                [this](float p, float dt) {
                    if (!targetCaptured || p < (kVerifySettle / kVerifyDuration)) return;
                    if (lastRmsDb <= -90.0f) return;

                    const float error = lastTargetDb - lastRmsDb;
                    gainDb = std::clamp(gainDb + error * kVerifyRatePerSec * dt,
                                        -kMaxGainDb, kMaxGainDb);
                    movingGain = true;
                },
                [this]() {
                    if (onRequestMeterReset) onRequestMeterReset();
                }}
        });
    }

    framework::InvisAutoRoutine routine;

    float lastRmsDb { -100.0f };
    float lastTargetDb { -18.0f };
    float liveGainDb { 0.0f };

    float resetGainFrom { 0.0f };
    float gainDb { 0.0f };
    bool movingGain { false };

    float startGainDb { 0.0f };
    float targetGainDb { 0.0f };
    bool targetCaptured { false };
};

} // namespace invis::modules
