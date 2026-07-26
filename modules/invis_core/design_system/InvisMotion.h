#pragma once

#include <algorithm>
#include <cmath>

namespace invis::ui {

/**
 * INVIS MOTION STANDARD
 * =====================
 *
 * NO PARAMETER RECALL IS EVER INSTANTANEOUS.
 *
 * Whenever a control's value is set by the machine rather than by the user's hand - preset load,
 * A/B/C compare switch, AUTO calibration, reset-to-default, host automation jump - the control
 * MUST glide to its new value over a finite time. It must never snap.
 *
 * This is not decoration. A parameter that jumps discontinuously produces an audible click or
 * zipper in the signal, and on screen it destroys the user's ability to see WHAT changed: a knob
 * that teleports conveys nothing, a knob that turns tells you which direction and how far.
 *
 * Glide durations are chosen by how much is moving and how much the user needs to follow it:
 *
 *   kRecallGlideFast     0.25s  a single control being reset or nudged
 *   kRecallGlideDefault  0.45s  a preset / compare-slot recall moving many controls at once
 *   kRecallGlideSlow     1.00s  a deliberate, watch-me gesture the user is meant to read
 *
 * 0.25s is the FLOOR, not a suggestion. Below roughly that, a gain change stops being a fade and
 * starts being a step edge again.
 */
namespace motion {

inline constexpr float kRecallGlideFast    = 0.25f;
inline constexpr float kRecallGlideDefault = 0.45f;
inline constexpr float kRecallGlideSlow    = 1.00f;

/** Absolute minimum any recall glide may use. */
inline constexpr float kMinRecallGlide = 0.25f;

/**
 * Logarithmic ease-out on [0,1]: quick off the mark, decelerating into the target.
 * log10(1 + 9x) is exactly 0 at x=0 and exactly 1 at x=1, so no normalisation fudge is needed.
 * This is the house easing curve for machine-driven parameter motion.
 */
inline float logEase(float x)
{
    return std::log10(1.0f + 9.0f * std::clamp(x, 0.0f, 1.0f));
}

/** Smooth symmetric ease, for motion that should start AND stop gently. */
inline float smoothEase(float x)
{
    const float t = std::clamp(x, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

/** Convenience: glide from a to b at normalized progress p, using the house curve. */
inline float glide(float from, float to, float progress)
{
    return from + (to - from) * logEase(progress);
}

} // namespace motion
} // namespace invis::ui
