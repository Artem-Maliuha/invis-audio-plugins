#pragma once

#include "InvisLED.h"
#include "../design_system/InvisLayout.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <algorithm>
#include <cmath>

namespace invis::ui {

enum class InvisLampSize {
    XS, // Inline beside a compact control label
    S,
    M,  // Standard module status lamp (default)
    L   // Primary / focal indicator
};

/** Absolute physical spec of an `InvisLampSize` preset, in design pixels. */
struct LampMetrics {
    float diameter;    // Emitting aperture
    float bezelWidth;  // Chassis collar around the aperture
    float bloomMargin; // Surface area reserved for volumetric spill
};

/**
 * Ballistics for a CONTINUOUS level lamp.
 *
 * Deliberately NOT `LEDBallistics`: that engine models a binary on/off indicator and fires a
 * 2.4x ignition flash on every transition to ON. A gain-reduction or filter-energy lamp is fed a
 * continuously varying level, so an ignition flash would fire on every transient and make the
 * lamp unreadable. This is a plain asymmetric follower instead - quick enough to catch the hit,
 * slow enough on release to actually be legible.
 */
class LampBallistics {
public:
    void setTarget(float normalizedLevel) { target = std::clamp(normalizedLevel, 0.0f, 1.0f); }
    float getTarget() const { return target; }
    float getCurrent() const { return current; }

    void reset()
    {
        target = 0.0f;
        current = 0.0f;
    }

    void update(float deltaTimeSeconds = 0.016f)
    {
        // ~40ms attack: fast enough to register a transient hit without visually clicking.
        // ~380ms release: the eye needs the dwell time to read colour AND brightness.
        const float speed = (target > current) ? 25.0f : 2.6f;
        current += (target - current) * std::min(1.0f, deltaTimeSeconds * speed);

        if (std::abs(target - current) < 0.0005f)
            current = target;
    }

    bool isSettled() const { return current <= 0.0005f && target <= 0.0005f; }

private:
    float target { 0.0f };
    float current { 0.0f };
};

/**
 * Level -> colour ramp: extinguished -> green -> amber -> crimson.
 *
 * Interpolation is a two-segment RGB lerp with AMBER AS AN EXPLICIT WAYPOINT. A direct
 * green->red lerp in sRGB passes through muddy olive around the midpoint, which reads as
 * "dirty" rather than "warning"; routing through amber keeps every intermediate colour on the
 * natural incandescent hue arc, so no HSV round-trip is needed.
 *
 * Hue and brightness are driven SEPARATELY: hue encodes how far into the danger zone the value
 * is, luminance encodes magnitude. A small amount reads as a dim green ember, a large one as a
 * blazing red.
 */
namespace lamp {

inline constexpr float kGreenEnd  = 0.45f; // pure green up to here
inline constexpr float kAmberPeak = 0.75f; // pure amber here, red beyond

inline juce::Colour getRampColour(float level)
{
    const float l = std::clamp(level, 0.0f, 1.0f);

    const auto green  = juce::Colour::fromRGB(0, 230, 118);
    const auto amber  = juce::Colour::fromRGB(255, 171, 0);
    const auto crimson = juce::Colour::fromRGB(255, 23, 68);

    if (l <= kGreenEnd)
        return green;

    if (l <= kAmberPeak)
        return green.interpolatedWith(amber, (l - kGreenEnd) / (kAmberPeak - kGreenEnd));

    return amber.interpolatedWith(crimson, (l - kAmberPeak) / (1.0f - kAmberPeak));
}

/** Emission strength for a level. Rises steeply out of darkness, then saturates. */
inline float getLuminance(float level)
{
    const float l = std::clamp(level, 0.0f, 1.0f);
    if (l <= 0.004f) return 0.0f;

    // Floor of 0.28 so the first perceptible amount is already a readable ember rather than an
    // ambiguous almost-black; overdrive past 1.0 at the very top for a physical blaze.
    return 0.28f + 1.02f * std::pow(l, 0.6f);
}

/**
 * dB -> normalized lamp level. Lives with the CALLER, not inside the atom: the atom stays a dumb
 * 0..1 device, while the meaning of the dB figure (gain reduction, energy removed, clip depth)
 * belongs to whoever measured it.
 */
inline float levelFromDb(float amountDb, float fullScaleDb = 24.0f)
{
    if (fullScaleDb <= 0.0f) return 0.0f;
    return std::clamp(amountDb / fullScaleDb, 0.0f, 1.0f);
}

} // namespace lamp

/**
 * Single-Lamp Level Indicator Atom.
 *
 * Fed a normalized "voltage" 0..1, it lights along the extinguished -> green -> amber -> red
 * ramp. Purely presentational and stateless: zero dependency on AudioProcessor / APVTS.
 *
 * Use cases: energy removed by a filter, compressor gain reduction, clipper reduction depth.
 *
 * PROPORTIONALITY INTEGRITY: fixed intrinsic size per preset, letterboxes rather than stretching.
 */
class InvisLevelLamp : public juce::Component {
public:
    InvisLevelLamp();
    ~InvisLevelLamp() override = default;

    static LampMetrics getMetrics(InvisLampSize size);
    static juce::Point<int> getIntrinsicSize(InvisLampSize size);
    juce::Point<int> getIntrinsicSize() const { return getIntrinsicSize(lampSize); }

    void setBoundsCentredIn(juce::Rectangle<int> area)
    {
        setBounds(centreIntrinsic(area, getIntrinsicSize()));
    }

    void setLampSize(InvisLampSize size) { lampSize = size; repaint(); }
    InvisLampSize getLampSize() const { return lampSize; }

    void setMountType(LEDMountType type) { mountType = type; repaint(); }
    LEDMountType getMountType() const { return mountType; }

    /** The "voltage": 0.0 = extinguished, 1.0 = full red blaze. */
    void setLevel(float normalizedLevel);
    float getLevel() const { return ballistics.getTarget(); }

    /**
     * Powered state. `false` means the host stage is BYPASSED - the lamp goes to a dead socket,
     * which must read differently from "powered but currently removing nothing".
     */
    void setActive(bool shouldBeActive);
    bool isActive() const { return active; }

    /** Advance ballistics one frame. Drive from the editor's timer. */
    void updateBallistics(float deltaTimeSeconds = 0.016f);

    /** Renders the lamp optics. Used by the component AND by atoms that host a lamp inline
        (e.g. InvisKnob), where drawing directly avoids clipping the volumetric spill. */
    static void drawLamp(juce::Graphics& g,
                         juce::Point<float> centre,
                         float radius,
                         float level,
                         float luminance,
                         bool active,
                         LEDMountType mount = LEDMountType::ProtrudingDome);

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    InvisLampSize lampSize { InvisLampSize::M };
    LEDMountType mountType { LEDMountType::ProtrudingDome };
    LampBallistics ballistics;
    bool active { true };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InvisLevelLamp)
};

} // namespace invis::ui
