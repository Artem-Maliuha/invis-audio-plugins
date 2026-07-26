#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

namespace invis::ui {

/**
 * WHAT COLOUR AN EFFECT IS, across the whole series.
 *
 * Not decoration and not taste. On the constellation chart colours MIX ON THE HUE WHEEL - a closed
 * cluster glows with the circular mean of its members - so a palette here is a set of coordinates
 * in a space the instrument actually computes in. Two families placed near each other produce
 * mixtures that read as a third family, and the chart starts lying about what is in a group.
 *
 * That single mechanic decides almost everything below.
 *
 * ── WHAT THE INDUSTRY ALREADY AGREES ON ──────────────────────────────────────────────────────
 *
 * Two of the three anchors are effectively settled, and fighting them costs recognition for
 * nothing:
 *
 *   TIME AND SPACE (reverb, delay)  -> BLUE / CYAN / VIOLET.
 *       Lexicon, Valhalla, FabFilter Pro-R, Eventide Blackhole, TC. Physically it reads as air,
 *       depth and distance - cool colours recede, which is the same illusion the effect is for.
 *
 *   SATURATION (drive, tape, tube)  -> RED / ORANGE / AMBER.
 *       FabFilter Saturn, Soundtoys Decapitator, every tape emulation ever shipped. It is the
 *       colour of the thing itself: a hot valve, magnetic tape, an overdriven meter.
 *
 *   MODULATION (chorus, flanger)    -> NOT SETTLED. Green, teal and purple are all in use.
 *
 * ── WHY MODULATION IS GREEN HERE, AND WHY THIS EXACT GREEN ───────────────────────────────────
 *
 * With blue (~205 deg) and orange (~22 deg) fixed, the third family has 177 deg of free arc, and
 * the choice that maximises the SMALLEST gap is its midpoint, near 115 deg. Purple would be the
 * other conventional option and is the worst available: it sits BETWEEN blue and red, so a purple
 * star mixed with a blue one lands in violet - indistinguishable from a spatial cluster.
 *
 * 118 deg specifically, rather than a deeper 140 deg green: it leans slightly yellow, which keeps
 * it away from the saturated signal green the meters use, and widens the gap to the azure family
 * at the same time.
 *
 * ── WHAT IS RESERVED AND MUST NOT BE TAKEN ───────────────────────────────────────────────────
 *
 *   ~258 deg violet   the second observer. It is not a star and must never be mistaken for one.
 *   white / silver    the first observer, same reason.
 *   pure red 350 deg  destructive controls. DELETE has to be unmistakable.
 *
 * ── WITHIN A FAMILY ──────────────────────────────────────────────────────────────────────────
 *
 * Variants stay within about +-14 deg of the anchor and separate by BRIGHTNESS and SATURATION
 * instead. This is deliberate: a cluster of two reverbs must still read as "spatial", and it only
 * does if its members average back onto their own anchor rather than drifting toward a neighbour.
 */
enum class EffectFamily {
    Spatial,        // reverbs and delays - one plugin, many algorithms
    Modulation,     // chorus, flanger, phaser, tremolo
    Saturation      // drive, tape, tube, crush
};

/** Anchor hue for a family, in degrees. The one number a new algorithm has to respect. */
inline constexpr float getFamilyHueDegrees(EffectFamily family)
{
    switch (family)
    {
        case EffectFamily::Spatial:    return 205.0f;
        case EffectFamily::Modulation: return 118.0f;
        case EffectFamily::Saturation:
        default:                       return 22.0f;
    }
}

inline const char* getFamilyName(EffectFamily family)
{
    switch (family)
    {
        case EffectFamily::Spatial:    return "SPATIAL";
        case EffectFamily::Modulation: return "MODULATION";
        case EffectFamily::Saturation:
        default:                       return "SATURATION";
    }
}

/** The family's own identity colour - for the plugin's branding, not for a star. */
inline juce::Colour getFamilyColour(EffectFamily family)
{
    return juce::Colour::fromHSV(getFamilyHueDegrees(family) / 360.0f, 0.85f, 1.0f, 1.0f);
}

/**
 * One algorithm on the chart.
 *
 * `hueOffset` is what keeps a family coherent: it is bounded, so no variant can wander into a
 * neighbouring family's arc no matter how many are added later.
 */
struct EffectType {
    const char* name;
    EffectFamily family;
    float hueOffset;      // degrees from the family anchor, kept within +-14
    float saturation;
    float brightness;

    juce::Colour getColour() const
    {
        float h = getFamilyHueDegrees(family) + juce::jlimit(-14.0f, 14.0f, hueOffset);
        h = std::fmod(h + 360.0f, 360.0f) / 360.0f;

        return juce::Colour::fromHSV(h, saturation, brightness, 1.0f);
    }
};

/**
 * The series catalogue.
 *
 * Ordered by family so a menu built from it groups itself, and so the eye can see the families are
 * three bands rather than a scatter.
 */
inline const std::vector<EffectType>& getEffectCatalogue()
{
    static const std::vector<EffectType> catalogue {
        // SPATIAL - the room. Reverbs and delays are two machines (see AlgorithmKind) and ONE
        // colour: the ear groups them as space, and the palette follows the ear rather than the
        // code. Deeper and less saturated as the space gets larger, because a big space is heard
        // as further away and cool colours recede.
        { "HALL",     EffectFamily::Spatial,   5.0f, 0.78f, 0.92f },
        { "CHAMBER",  EffectFamily::Spatial,  -2.0f, 0.86f, 0.98f },
        { "PLATE",    EffectFamily::Spatial, -10.0f, 0.92f, 1.00f },
        { "SPRING",   EffectFamily::Spatial,  12.0f, 0.70f, 0.88f },

        { "DIGITAL",  EffectFamily::Spatial, -12.0f, 0.98f, 1.00f },
        { "ANALOG",   EffectFamily::Spatial,  -4.0f, 0.88f, 0.94f },
        { "TAPE",     EffectFamily::Spatial,   8.0f, 0.74f, 0.88f },
        { "PINGPONG", EffectFamily::Spatial,  -8.0f, 0.94f, 0.97f },

        // MODULATION - movement. Brighter as the motion gets faster and shallower.
        { "CHORUS",  EffectFamily::Modulation,  6.0f, 0.80f, 0.94f },
        { "FLANGER", EffectFamily::Modulation, -9.0f, 0.90f, 1.00f },
        { "PHASER",  EffectFamily::Modulation, 12.0f, 0.72f, 0.90f },
        { "TREMOLO", EffectFamily::Modulation, -3.0f, 0.86f, 0.86f },
        { "VIBRATO", EffectFamily::Modulation,  9.0f, 0.78f, 0.92f },

        // SATURATION - heat. Redder and darker as it gets more destructive, which is the same way
        // a real overload behaves: amber first, then red.
        { "TAPE SAT", EffectFamily::Saturation, 10.0f, 0.86f, 1.00f },
        { "TUBE",     EffectFamily::Saturation,  2.0f, 0.92f, 0.96f },
        { "DRIVE",    EffectFamily::Saturation, -7.0f, 0.96f, 0.92f },
        { "CRUSH",    EffectFamily::Saturation,-13.0f, 1.00f, 0.84f },
    };

    return catalogue;
}

/**
 * The filter ranges a STAR's block uses - identical to the channel strip's, on purpose.
 *
 * Defined once, here, so the knob that draws the number and the DSP that applies it cannot end up
 * on different curves. They were: a plain logarithmic sweep in the panel against a skewed range in
 * the strip, which put 350 Hz in two different places on two knobs with the same name.
 */
inline const juce::NormalisableRange<float>& starFilterRange(bool highPass)
{
    static const juce::NormalisableRange<float> hp { 20.0f, 2000.0f, 1.0f, 0.4f };
    static const juce::NormalisableRange<float> lp { 1000.0f, 20000.0f, 1.0f, 0.4f };

    return highPass ? hp : lp;
}

/**
 * The COMBINE plugin's identity - the one that hosts all three families.
 *
 * Deliberately colourless. Mixing three families 120 degrees apart on the wheel lands near grey no
 * matter how it is weighted, so any hue chosen for it would be a claim it cannot keep. Silver says
 * "all of them" honestly, and stays out of every family's arc.
 */
inline juce::Colour getCombineColour() { return juce::Colour::fromRGB(226, 232, 240); }

} // namespace invis::ui
