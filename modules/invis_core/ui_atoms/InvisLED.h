#pragma once

#include "../design_system/InvisThemeSupplier.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace invis::ui {

enum class LEDMountType {
    RecessedSlot,     // Carved slot channel with interior drop shadows & metal chamfer bevel
    ProtrudingDome    // Protruding 3D crystal dome with drilled chassis hole bezel collar
};

enum class LEDColorPreset {
    NeonCyan,         // Ultra-modern digital cyan
    WarmAmber,        // Vintage vacuum tube & console amber
    ElectricViolet,   // Cyberpunk neon violet
    EmeraldPhosphor,  // Retro CRT & analog VU meter phosphor green
    CrimsonRuby,      // High-contrast alert crimson red
    IceWhite,         // Pure studio white phosphor
    CobaltBlue        // Modern precision sapphire blue
};

static inline juce::Colour getPresetColor(LEDColorPreset preset)
{
    switch (preset)
    {
        case LEDColorPreset::NeonCyan:        return juce::Colour::fromRGB(0, 229, 255);
        case LEDColorPreset::WarmAmber:       return juce::Colour::fromRGB(255, 152, 0);
        case LEDColorPreset::ElectricViolet:  return juce::Colour::fromRGB(224, 64, 251);
        case LEDColorPreset::EmeraldPhosphor: return juce::Colour::fromRGB(0, 230, 118);
        case LEDColorPreset::CrimsonRuby:     return juce::Colour::fromRGB(255, 23, 68);
        case LEDColorPreset::IceWhite:        return juce::Colour::fromRGB(245, 247, 250);
        case LEDColorPreset::CobaltBlue:      return juce::Colour::fromRGB(41, 121, 255);
    }
    return juce::Colour::fromRGB(0, 229, 255);
}

static inline juce::String getPresetName(LEDColorPreset preset)
{
    switch (preset)
    {
        case LEDColorPreset::NeonCyan:        return "Cyan";
        case LEDColorPreset::WarmAmber:       return "Amber";
        case LEDColorPreset::ElectricViolet:  return "Violet";
        case LEDColorPreset::EmeraldPhosphor: return "Emerald";
        case LEDColorPreset::CrimsonRuby:     return "Ruby";
        case LEDColorPreset::IceWhite:        return "White";
        case LEDColorPreset::CobaltBlue:      return "Blue";
    }
    return "Cyan";
}

/**
 * Ballistics engine for physical LED luminescence & envelope decay.
 */
class LEDBallistics {
public:
    LEDBallistics() = default;

    void setState(bool isTargetOn, bool animate = true)
    {
        if (targetState != isTargetOn)
        {
            targetState = isTargetOn;
            if (isTargetOn && animate)
            {
                // Trigger physical ignition attack flash spike (2.4x brightness)
                flashIntensity = 2.4f;
            }
        }
    }

    bool isTargetOn() const { return targetState; }
    float getCurrentLuminance() const { return currentLuminance; }

    // Advance ballistics smoothly (call on repaint/frame tick)
    void update(float deltaTimeSeconds = 0.016f)
    {
        const float targetLum = targetState ? 1.0f : 0.0f;

        // Turn-ON ignition flash decay (smooth logarithmic settling to steady state 1.0)
        if (flashIntensity > 1.0f)
        {
            flashIntensity = std::max(1.0f, flashIntensity - deltaTimeSeconds * 6.5f);
        }

        const float effectiveTarget = targetLum * flashIntensity;

        if (effectiveTarget > currentLuminance)
        {
            // Turn-ON attack speed: physical ignition warming (~20ms)
            const float attackSpeed = 45.0f;
            currentLuminance += (effectiveTarget - currentLuminance) * std::min(1.0f, deltaTimeSeconds * attackSpeed);
        }
        else
        {
            // Turn-OFF release speed: snappy physical phosphorescent decay (~30ms)
            const float decaySpeed = 32.0f;
            currentLuminance += (effectiveTarget - currentLuminance) * std::min(1.0f, deltaTimeSeconds * decaySpeed);
        }
    }

private:
    bool targetState { false };
    float currentLuminance { 0.0f };
    float flashIntensity { 1.0f };
};

/**
 * Presentational Vector LED Atom with Volumetric Ballistics & Light Propagation.
 */
class InvisLED : public juce::Component {
public:
    InvisLED();
    ~InvisLED() override = default;

    void setMountType(LEDMountType type) { mountType = type; repaint(); }
    LEDMountType getMountType() const { return mountType; }

    void setLedColor(juce::Colour color) { ledColor = color; repaint(); }
    juce::Colour getLedColor() const { return ledColor; }

    void setOn(bool shouldBeOn, bool animate = true)
    {
        ballistics.setState(shouldBeOn, animate);
        repaint();
    }
    bool isOn() const { return ballistics.isTargetOn(); }

    float getLuminance() const { return ballistics.getCurrentLuminance(); }
    void updateBallistics(float deltaTimeSeconds = 0.016f)
    {
        ballistics.update(deltaTimeSeconds);
        repaint();
    }

    // Static drawing helper to render LED optics along a line capsule or as a circular 3D dot
    static void drawLEDCapsule(juce::Graphics& g,
                               juce::Point<float> pStart,
                               juce::Point<float> pEnd,
                               float width,
                               juce::Colour color,
                               float luminance,
                               LEDMountType mount = LEDMountType::RecessedSlot);

    static void drawLEDDot(juce::Graphics& g,
                           juce::Point<float> center,
                           float radius,
                           juce::Colour color,
                           float luminance,
                           LEDMountType mount = LEDMountType::ProtrudingDome);

    void paint(juce::Graphics& g) override;

private:
    LEDMountType mountType { LEDMountType::RecessedSlot };
    juce::Colour ledColor { juce::Colour::fromRGB(0, 210, 255) };
    LEDBallistics ballistics;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InvisLED)
};

} // namespace invis::ui
