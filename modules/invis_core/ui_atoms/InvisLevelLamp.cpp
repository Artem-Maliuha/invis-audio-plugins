#include "InvisLevelLamp.h"

namespace invis::ui {

InvisLevelLamp::InvisLevelLamp()
{
    setInterceptsMouseClicks(false, false);
}

LampMetrics InvisLevelLamp::getMetrics(InvisLampSize size)
{
    //        diameter  bezel  bloomMargin
    switch (size)
    {
        case InvisLampSize::XS: return {  5.0f, 1.0f, 3.0f };
        case InvisLampSize::S:  return {  6.5f, 1.0f, 4.0f };
        case InvisLampSize::L:  return { 10.0f, 1.4f, 6.0f };
        case InvisLampSize::M:
        default:                return {  8.0f, 1.2f, 5.0f };
    }
}

juce::Point<int> InvisLevelLamp::getIntrinsicSize(InvisLampSize size)
{
    const auto m = getMetrics(size);
    const int d = static_cast<int>(std::ceil(m.diameter + 2.0f * (m.bezelWidth + m.bloomMargin)));
    return { d, d };
}

void InvisLevelLamp::setLevel(float normalizedLevel)
{
    ballistics.setTarget(normalizedLevel);
    repaint();
}

void InvisLevelLamp::setActive(bool shouldBeActive)
{
    if (active == shouldBeActive) return;

    active = shouldBeActive;
    if (!active)
        ballistics.reset();

    repaint();
}

void InvisLevelLamp::updateBallistics(float deltaTimeSeconds)
{
    const float before = ballistics.getCurrent();
    ballistics.update(deltaTimeSeconds);

    if (std::abs(ballistics.getCurrent() - before) > 0.0008f)
        repaint();
}

void InvisLevelLamp::resized()
{
    const auto intrinsic = getIntrinsicSize();
    jassert(getWidth() == 0 || (getWidth() == intrinsic.x && getHeight() == intrinsic.y));
}

void InvisLevelLamp::paint(juce::Graphics& g)
{
    const auto m = getMetrics(lampSize);
    const auto content = centreIntrinsic(getLocalBounds().toFloat(), getIntrinsicSize());

    drawLamp(g, content.getCentre(), m.diameter * 0.5f,
             ballistics.getCurrent(), lamp::getLuminance(ballistics.getCurrent()),
             active, mountType);
}

void InvisLevelLamp::drawLamp(juce::Graphics& g,
                              juce::Point<float> centre,
                              float radius,
                              float level,
                              float luminance,
                              bool active,
                              LEDMountType mount)
{
    const float r = std::max(2.0f, radius);
    const float lum = active ? std::max(0.0f, luminance) : 0.0f;
    const auto colour = lamp::getRampColour(level);

    // NOTE: this does not simply call InvisLED::drawLEDDot. That helper fills a white phosphor
    // core across 90% of the aperture at full luminance, which washes the lamp out to white -
    // fine for a status LED whose colour is fixed, fatal here where THE HUE IS THE READING.
    // The core below is deliberately small and capped so the ramp colour always dominates.

    if (lum > 0.04f)
    {
        const float bloomScale = std::pow(std::min(1.8f, lum), 1.6f);
        const float alphaFactor = std::min(1.0f, lum);

        // Volumetric surface spill onto the surrounding chassis
        const float outerR = r * (1.0f + 2.6f * bloomScale);
        const float outerAlpha = std::clamp(0.60f * std::pow(alphaFactor, 1.2f), 0.0f, 0.78f);

        if (outerAlpha > 0.02f)
        {
            const auto outerGlow = juce::ColourGradient(
                colour.withMultipliedAlpha(outerAlpha), centre.x, centre.y,
                juce::Colours::transparentBlack, centre.x + outerR, centre.y,
                true
            );
            g.setGradientFill(outerGlow);
            g.fillEllipse(centre.x - outerR, centre.y - outerR, outerR * 2.0f, outerR * 2.0f);
        }

        // Intense near-field diffusion
        const float innerR = r * (1.0f + 0.75f * bloomScale);
        g.setGradientFill(juce::ColourGradient(
            colour.brighter(0.30f).withMultipliedAlpha(std::min(0.95f, alphaFactor)), centre.x, centre.y,
            juce::Colours::transparentBlack, centre.x + innerR, centre.y,
            true
        ));
        g.fillEllipse(centre.x - innerR, centre.y - innerR, innerR * 2.0f, innerR * 2.0f);
    }

    if (mount == LEDMountType::RecessedSlot)
    {
        // Carved bore: interior drop shadow above, chamfered highlight below
        g.setColour(juce::Colours::black.withAlpha(0.55f));
        g.fillEllipse(centre.x - r - 1.2f, centre.y - r - 0.6f, (r + 1.2f) * 2.0f, (r + 1.2f) * 2.0f);
        g.setColour(juce::Colours::white.withAlpha(0.16f));
        g.drawEllipse(centre.x - r - 1.0f, centre.y - r - 1.0f, (r + 1.0f) * 2.0f, (r + 1.0f) * 2.0f, 0.9f);
    }
    else
    {
        // Drilled chassis hole with a polished bezel collar
        g.setColour(juce::Colours::black.withAlpha(0.65f));
        g.fillEllipse(centre.x - r - 1.4f, centre.y - r - 1.4f + 0.8f, (r + 1.4f) * 2.0f, (r + 1.4f) * 2.0f);
        g.setColour(juce::Colours::white.withAlpha(0.34f));
        g.drawEllipse(centre.x - r - 1.0f, centre.y - r - 1.0f, (r + 1.0f) * 2.0f, (r + 1.0f) * 2.0f, 1.0f);
    }

    // Dark socket floor - this is also the "extinguished" and "bypassed" appearance
    g.setColour(juce::Colour::fromRGB(6, 8, 12));
    g.fillEllipse(centre.x - r, centre.y - r, r * 2.0f, r * 2.0f);

    if (!active)
    {
        // Dead socket: a bypassed stage must not look like a powered stage doing nothing
        g.setColour(juce::Colour::fromRGB(30, 34, 42).withAlpha(0.35f));
        g.fillEllipse(centre.x - r, centre.y - r, r * 2.0f, r * 2.0f);
        return;
    }

    if (lum > 0.01f)
    {
        const float alphaFactor = std::min(1.0f, lum);

        // Translucent crystal lens carrying the ramp hue
        g.setColour(colour.brighter(0.18f).withMultipliedAlpha(alphaFactor));
        g.fillEllipse(centre.x - r, centre.y - r, r * 2.0f, r * 2.0f);

        // Restrained filament hotspot - capped so hue is never washed out to white
        const float coreAlpha = std::min(0.55f, 0.42f * lum);
        g.setColour(juce::Colours::white.withAlpha(coreAlpha));
        g.fillEllipse(centre.x - r * 0.34f, centre.y - r * 0.42f, r * 0.68f, r * 0.68f);
    }
    else
    {
        // Powered but idle: faint cold glass, distinguishable from the dead socket above
        g.setColour(juce::Colour::fromRGB(38, 46, 58).withAlpha(0.45f));
        g.fillEllipse(centre.x - r, centre.y - r, r * 2.0f, r * 2.0f);
    }
}

} // namespace invis::ui
