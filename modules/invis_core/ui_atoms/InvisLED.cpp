#include "InvisLED.h"

namespace invis::ui {

InvisLED::InvisLED()
{
    setBufferedToImage(true);
}

void InvisLED::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    if (bounds.isEmpty()) return;

    ballistics.update(0.016f); // Advance luminescence ballistics frame tick
    const float lum = ballistics.getCurrentLuminance();

    const auto center = bounds.getCentre();
    const float size = std::min(bounds.getWidth(), bounds.getHeight()) * 0.7f;
    const float radius = size * 0.5f;

    if (mountType == LEDMountType::ProtrudingDome)
    {
        // 1. Drilled Chassis Hole Bezel Collar
        g.setColour(juce::Colour::fromRGB(20, 24, 32));
        g.fillEllipse(center.x - radius - 2.0f, center.y - radius - 2.0f, (radius + 2.0f) * 2.0f, (radius + 2.0f) * 2.0f);

        g.setColour(juce::Colours::white.withAlpha(0.25f));
        g.drawEllipse(center.x - radius - 2.0f, center.y - radius - 2.0f, (radius + 2.0f) * 2.0f, (radius + 2.0f) * 2.0f, 1.0f);

        // 2. Interior Drop Shadow Hole
        g.setColour(juce::Colours::black.withAlpha(0.85f));
        g.fillEllipse(center.x - radius, center.y - radius, radius * 2.0f, radius * 2.0f);

        if (lum > 0.01f)
        {
            // 3. Volumetric 360-degree Surface Light Propagation
            g.setColour(ledColor.withAlpha(0.2f * lum));
            g.fillEllipse(center.x - radius * 2.2f, center.y - radius * 2.2f, radius * 4.4f, radius * 4.4f);

            // 4. Soft Neon Halo
            g.setColour(ledColor.withAlpha(0.5f * lum));
            g.fillEllipse(center.x - radius * 1.4f, center.y - radius * 1.4f, radius * 2.8f, radius * 2.8f);

            // 5. Contact Shadow cast by Protruding Lens Dome
            g.setColour(juce::Colours::black.withAlpha(0.4f));
            g.fillEllipse(center.x - radius + 1.0f, center.y - radius + 1.5f, radius * 2.0f, radius * 2.0f);

            // 6. Translucent Protruding Lens Crystal
            g.setColour(ledColor.brighter(0.2f));
            g.fillEllipse(center.x - radius, center.y - radius, radius * 2.0f, radius * 2.0f);

            // 7. Phosphor Core Thread
            g.setColour(juce::Colours::white.withAlpha(0.9f * lum));
            g.fillEllipse(center.x - radius * 0.4f, center.y - radius * 0.4f, radius * 0.8f, radius * 0.8f);
        }
        else
        {
            // Dim unlit dome glass
            g.setColour(juce::Colour::fromRGB(40, 48, 60));
            g.fillEllipse(center.x - radius, center.y - radius, radius * 2.0f, radius * 2.0f);
        }
    }
    else // RecessedSlot
    {
        drawLEDCapsule(g,
                       juce::Point<float>(center.x - radius * 0.8f, center.y),
                       juce::Point<float>(center.x + radius * 0.8f, center.y),
                       radius * 0.6f, ledColor, lum, LEDMountType::RecessedSlot);
    }
}

void InvisLED::drawLEDCapsule(juce::Graphics& g,
                              juce::Point<float> pStart,
                              juce::Point<float> pEnd,
                              float width,
                              juce::Colour color,
                              float luminance,
                              LEDMountType mount)
{
    const float slotWidth = std::max(4.0f, width);

    // A. Metal Chamfer Bevel Rim (Top-Left Highlight / Bottom-Right Shadow)
    const juce::Point<float> offsetBevel(0.7f, 0.7f);
    g.setColour(juce::Colours::white.withAlpha(0.28f));
    g.drawLine(juce::Line<float>(pStart - offsetBevel, pEnd - offsetBevel), slotWidth + 1.6f);

    g.setColour(juce::Colours::black.withAlpha(0.75f));
    g.drawLine(juce::Line<float>(pStart + offsetBevel, pEnd + offsetBevel), slotWidth + 1.6f);

    // B. Carved Recessed Slot Channel Interior
    g.setColour(juce::Colour::fromRGB(8, 10, 14));
    g.drawLine(juce::Line<float>(pStart, pEnd), slotWidth);

    if (luminance > 0.01f)
    {
        const float activeLum = std::min(1.8f, luminance);
        const float alphaFactor = std::min(1.0f, activeLum);

        // C. Outer Wide Subsurface Glow Field (3.8x slot width)
        g.setColour(color.withAlpha(0.22f * alphaFactor));
        g.drawLine(juce::Line<float>(pStart, pEnd), slotWidth * 3.8f);

        // D. Intense Neon Halo Diffusion Band (2.2x slot width)
        g.setColour(color.withAlpha(0.55f * alphaFactor));
        g.drawLine(juce::Line<float>(pStart, pEnd), slotWidth * 2.2f);

        // E. Full-Width Translucent LED Crystal Lens Body
        g.setColour(color.brighter(0.25f).withMultipliedAlpha(alphaFactor));
        g.drawLine(juce::Line<float>(pStart, pEnd), slotWidth);

        // F. Bright White Phosphor LED Core Thread Highlight (Attack Flash boosted)
        g.setColour(juce::Colours::white.withAlpha(std::min(1.0f, 0.95f * activeLum)));
        g.drawLine(juce::Line<float>(pStart, pEnd), std::max(1.5f, slotWidth * 0.45f));
    }
    else
    {
        // Unlit Dim Filament inside Slot
        g.setColour(juce::Colour::fromRGB(35, 42, 52).withAlpha(0.35f));
        g.drawLine(juce::Line<float>(pStart, pEnd), slotWidth * 0.5f);
    }
}

void InvisLED::drawLEDDot(juce::Graphics& g,
                          juce::Point<float> center,
                          float radius,
                          juce::Colour color,
                          float luminance,
                          LEDMountType mount)
{
    const float r = std::max(2.5f, radius);
    const float activeLum = std::max(0.0f, luminance);

    if (activeLum > 0.04f)
    {
        // Non-linear luminescence response curve for physical optical bloom expansion
        const float bloomScale = std::pow(std::min(1.8f, activeLum), 1.6f);
        const float alphaFactor = std::min(1.0f, activeLum);

        // Dynamic Outer Ambient Bloom: Rich, clearly visible radius (expands smoothly up to 3.8x)
        const float outerR = r * (1.0f + 2.8f * bloomScale);
        const float outerAlpha = std::clamp(0.65f * std::pow(alphaFactor, 1.2f), 0.0f, 0.82f);

        if (outerR > r * 1.05f && outerAlpha > 0.02f)
        {
            const auto outerGlow = juce::ColourGradient(
                color.withMultipliedAlpha(outerAlpha), center.x, center.y,
                juce::Colours::transparentBlack, center.x + outerR, center.y,
                true
            );
            g.setGradientFill(outerGlow);
            g.fillEllipse(center.x - outerR, center.y - outerR, outerR * 2.0f, outerR * 2.0f);
        }

        // Dynamic Inner Intense Core Bloom: Vibrant core radius (expands up to 1.8x)
        const float innerR = r * (1.0f + 0.8f * bloomScale);
        const float innerAlpha = std::clamp(0.90f * alphaFactor, 0.0f, 0.98f);

        if (innerR > r * 1.02f && innerAlpha > 0.04f)
        {
            const auto innerGlow = juce::ColourGradient(
                color.brighter(0.35f).withMultipliedAlpha(innerAlpha), center.x, center.y,
                juce::Colours::transparentBlack, center.x + innerR, center.y,
                true
            );
            g.setGradientFill(innerGlow);
            g.fillEllipse(center.x - innerR, center.y - innerR, innerR * 2.0f, innerR * 2.0f);
        }
    }

    // C. Clean Metal Bezel Ring Collar (High-key Chamfer Rim)
    g.setColour(juce::Colours::white.withAlpha(0.35f));
    g.drawEllipse(center.x - r - 1.0f, center.y - r - 1.0f, (r + 1.0f) * 2.0f, (r + 1.0f) * 2.0f, 1.0f);

    // D. Interior Carved Dark Socket Hole
    g.setColour(juce::Colour::fromRGB(6, 8, 12));
    g.fillEllipse(center.x - r, center.y - r, r * 2.0f, r * 2.0f);

    if (luminance > 0.01f)
    {
        const float activeLum = std::min(1.8f, luminance);
        const float alphaFactor = std::min(1.0f, activeLum);

        // E. Vibrant Translucent Crystal Bulb Lens Body
        g.setColour(color.brighter(0.3f).withMultipliedAlpha(alphaFactor));
        g.fillEllipse(center.x - r, center.y - r, r * 2.0f, r * 2.0f);

        // F. High-Intensity White Phosphor Core Ignition Point
        g.setColour(juce::Colours::white.withAlpha(std::min(1.0f, 0.98f * activeLum)));
        g.fillEllipse(center.x - r * 0.45f, center.y - r * 0.45f, r * 0.9f, r * 0.9f);
    }
    else
    {
        // Unlit Dim Crystal Dome
        g.setColour(juce::Colour::fromRGB(35, 42, 52).withAlpha(0.4f));
        g.fillEllipse(center.x - r, center.y - r, r * 2.0f, r * 2.0f);
    }
}

} // namespace invis::ui
