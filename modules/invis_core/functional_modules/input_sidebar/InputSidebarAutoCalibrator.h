#pragma once

#include "../../framework/InvisAutoRoutine.h"
#include <algorithm>
#include <cmath>

namespace invis::modules {

/**
 * AUTO input-stage calibration.
 *
 * Built on `framework::InvisAutoRoutine`, so its timeline, easing and running-state handling are
 * the SAME machinery the output AUTO uses. Only the measurement logic is specific to the input.
 *
 *   RESET  0.50s  filters glide back to OFF, trim back to unity - glided, never snapped
 *   HOLD   1.00s  stillness, so the meters settle on the un-processed signal before measuring
 *   HPF    3.00s  sweeps up from OFF, parking the instant it removes a *micro* amount of energy
 *   LPF    2.00s  same, sweeping down from OFF
 *   SETTLE 0.70s  the 300ms RMS window catches up with the last filter move
 *   TRIM   3.00s  input gain glides until the measured RMS lands on the gain-stage value
 *
 * Running the filters one after another rather than concurrently removes a measurement hazard:
 * with both moving at once, each one's "energy removed" reading shifts under the other's feet,
 * because the LPF is measured against the post-HPF signal.
 *
 * Pure logic, no JUCE and no APVTS: measurements are pushed in, target positions read out, and
 * the owning *UI class does the parameter writing.
 */
class InputSidebarAutoCalibrator {
public:
    static constexpr float kResetDuration  = 0.50f;
    static constexpr float kHoldDuration   = 1.00f;
    static constexpr float kHpfDuration    = 3.00f;
    static constexpr float kLpfDuration    = 2.00f;
    static constexpr float kSettleDuration = 0.70f;
    static constexpr float kTrimDuration   = 3.00f;

    /** "Barely glowing green" on the filter lamp: ~1.4 dB of energy removed at a 24 dB scale. */
    static constexpr float kMicroThreshold = 0.06f;

    /** Sweep endpoints in normalized knob space, held just clear of the OFF detents so the
        filters are actually engaged the whole way (an OFF filter removes nothing by definition
        and the sweep would never trigger). */
    static constexpr float kHpfSweepStart = 0.03f;
    static constexpr float kLpfSweepStart = 0.97f;

    static constexpr float kMaxTrimDb = 24.0f;

    /** Slow closed-loop correction after the main glide lands. */
    static constexpr float kVerifyDuration = 2.00f;
    static constexpr float kVerifySettle   = 0.60f; // let the freshly-reset RMS window refill
    static constexpr float kVerifyRatePerSec = 0.9f;

    InputSidebarAutoCalibrator() { buildRoutine(); }

    bool isRunning() const { return routine.isRunning(); }

    /** Fired once when the main glide lands, so the host can drop the metering integrators:
        the 300ms RMS window still holds the pre-correction level and would under-report. */
    std::function<void()> onRequestMeterReset;

    /** Fired when the routine starts and finishes - drive the AUTO lamp from this. */
    void setRunningChangedCallback(std::function<void(bool)> cb)
    {
        routine.onRunningChanged = std::move(cb);
    }

    bool shouldMoveHpf() const  { return isRunning() && movingHpf; }
    bool shouldMoveLpf() const  { return isRunning() && movingLpf; }
    bool shouldMoveTrim() const { return isRunning() && movingTrim; }

    float getHpfPosition() const { return hpfPos; }
    float getLpfPosition() const { return lpfPos; }
    float getTrimDb() const { return trimDb; }

    /**
     * A filter that completed its whole phase WITHOUT ever crossing the micro threshold never
     * found an edge of audibility - because the material has no content in its band, or because
     * there was no usable signal. The safe answer is then the transparent setting (OFF), NOT the
     * extreme it happened to stop at: parking a "found nothing" LPF at 1 kHz would be the single
     * most destructive outcome the routine could produce.
     */
    bool shouldForceHpfOff() const { return hpfSweptOut; }
    bool shouldForceLpfOff() const { return lpfSweptOut; }

    void start(float currentHpfPos, float currentLpfPos, float currentTrimDb)
    {
        resetHpfFrom = currentHpfPos;
        resetLpfFrom = currentLpfPos;
        resetTrimFrom = currentTrimDb;

        hpfPos = currentHpfPos;
        lpfPos = currentLpfPos;
        trimDb = currentTrimDb;

        hpfParked = lpfParked = false;
        hpfSweptOut = lpfSweptOut = false;
        trimTargetCaptured = false;

        routine.start();
    }

    void cancel() { routine.cancel(); }

    /** Push this frame's measurements, then advance the routine. */
    void tick(float dt, float hpfRemoved, float lpfRemoved,
              float measuredRmsDb, float targetRmsDb, float currentTrimDb)
    {
        lastHpfRemoved = hpfRemoved;
        lastLpfRemoved = lpfRemoved;
        lastRmsDb = measuredRmsDb;
        lastTargetDb = targetRmsDb;
        liveTrimDb = currentTrimDb;

        movingHpf = movingLpf = movingTrim = false;
        routine.tick(dt);
    }

private:
    void buildRoutine()
    {
        using Phase = framework::InvisAutoRoutine::Phase;

        routine.setPhases({
            // RESET: glide everything back to transparent, from wherever the last run left it
            Phase { "RESET", kResetDuration, [this](float p, float) {
                hpfPos = ui::motion::glide(resetHpfFrom, 0.0f, p);
                lpfPos = ui::motion::glide(resetLpfFrom, 1.0f, p);
                trimDb = ui::motion::glide(resetTrimFrom, 0.0f, p);
                movingHpf = movingLpf = movingTrim = true;
            }, nullptr },

            // HOLD: nothing moves; the meters are now reading the raw incoming signal, which is
            // what every measurement below is taken against
            Phase { "HOLD", kHoldDuration, [this](float, float) {
                hpfPos = 0.0f; lpfPos = 1.0f; trimDb = 0.0f;
                movingHpf = movingLpf = movingTrim = true;
            }, nullptr },

            Phase { "HPF", kHpfDuration, [this](float p, float) {
                if (hpfParked) return;
                if (lastHpfRemoved >= kMicroThreshold) { hpfParked = true; return; }

                hpfPos = kHpfSweepStart + (1.0f - kHpfSweepStart) * ui::motion::logEase(p);
                movingHpf = true;
            }, nullptr },

            Phase { "LPF", kLpfDuration, [this](float p, float) {
                if (!hpfParked) { hpfSweptOut = true; hpfPos = 0.0f; hpfParked = true; movingHpf = true; }
                if (lpfParked) return;
                if (lastLpfRemoved >= kMicroThreshold) { lpfParked = true; return; }

                lpfPos = kLpfSweepStart - kLpfSweepStart * ui::motion::logEase(p);
                movingLpf = true;
            }, nullptr },

            Phase { "SETTLE", kSettleDuration, [this](float, float) {
                if (!lpfParked) { lpfSweptOut = true; lpfPos = 1.0f; lpfParked = true; movingLpf = true; }
            }, nullptr },

            // TRIM: the target is captured ONCE on entry. Re-deriving it every frame while the
            // trim is moving would chase a measurement that is itself responding to the trim, and
            // never settle.
            Phase { "TRIM", kTrimDuration,
                [this](float p, float) {
                    if (!trimTargetCaptured) return;
                    trimDb = ui::motion::glide(trimStartDb, trimTargetDb, p);
                    movingTrim = true;
                },
                [this]() {
                    if (lastRmsDb <= -90.0f) return; // no usable signal - leave the trim alone

                    trimStartDb = liveTrimDb;
                    trimTargetDb = std::clamp(liveTrimDb + (lastTargetDb - lastRmsDb),
                                              -kMaxTrimDb, kMaxTrimDb);
                    trimTargetCaptured = true;
                }},

            // VERIFY: a slow closed-loop check. The main glide is open-loop - it aimed at a target
            // computed before it moved - so a residual is expected. This walks it out gently
            // instead of snapping, and only AFTER the freshly reset RMS window has refilled.
            Phase { "VERIFY", kVerifyDuration,
                [this](float p, float dt) {
                    if (!trimTargetCaptured || p < (kVerifySettle / kVerifyDuration)) return;
                    if (lastRmsDb <= -90.0f) return;

                    const float error = lastTargetDb - lastRmsDb;
                    trimDb = std::clamp(trimDb + error * kVerifyRatePerSec * dt,
                                        -kMaxTrimDb, kMaxTrimDb);
                    movingTrim = true;
                },
                [this]() {
                    // Drop the integrators: the 300ms window still holds the pre-correction level
                    if (onRequestMeterReset) onRequestMeterReset();
                }}
        });
    }

    framework::InvisAutoRoutine routine;

    // Pushed in each frame
    float lastHpfRemoved { 0.0f };
    float lastLpfRemoved { 0.0f };
    float lastRmsDb { -100.0f };
    float lastTargetDb { -18.0f };
    float liveTrimDb { 0.0f };

    // Reset origins
    float resetHpfFrom { 0.0f };
    float resetLpfFrom { 1.0f };
    float resetTrimFrom { 0.0f };

    // Outputs
    float hpfPos { 0.0f };
    float lpfPos { 1.0f };
    float trimDb { 0.0f };
    bool movingHpf { false };
    bool movingLpf { false };
    bool movingTrim { false };

    bool hpfParked { false };
    bool lpfParked { false };
    bool hpfSweptOut { false };
    bool lpfSweptOut { false };

    float trimStartDb { 0.0f };
    float trimTargetDb { 0.0f };
    bool trimTargetCaptured { false };
};

} // namespace invis::modules
