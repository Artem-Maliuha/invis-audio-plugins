#include "InvisKnob.h"
#include <cmath>

namespace invis::ui {

InvisKnob::InvisKnob()
{
    setRepaintsOnMouseActivity(true);
}

InvisTheme InvisKnob::getTheme() const
{
    if (customTheme.has_value())
        return *customTheme;

    return InvisThemeSupplier::getParentTheme(this);
}

void InvisKnob::setScaleLabels(const juce::StringArray& labels)
{
    scaleTicks.clear();
    const int count = labels.size();
    if (count == 0) return;

    for (int i = 0; i < count; ++i)
    {
        const float pos = (count > 1) ? static_cast<float>(i) / static_cast<float>(count - 1) : 0.0f;
        const bool isOffTick = (offPosition == OffPosition::Start && i == 0) ||
                              (offPosition == OffPosition::End && i == count - 1) ||
                              (labels[i].equalsIgnoreCase("OFF"));
        scaleTicks.push_back({ pos, labels[i], isOffTick });
    }
    repaint();
}

float InvisKnob::applyCurve(float norm) const
{
    const float clamped = juce::jlimit(0.0f, 1.0f, norm);
    switch (scaleCurve)
    {
        case KnobScaleCurve::Logarithmic:
            return (std::pow(10.0f, clamped) - 1.0f) / 9.0f;
        case KnobScaleCurve::InverseLogarithmic:
            return 1.0f - (std::pow(10.0f, 1.0f - clamped) - 1.0f) / 9.0f;
        case KnobScaleCurve::Linear:
        default:
            return clamped;
    }
}

float InvisKnob::removeCurve(float input) const
{
    const float clamped = juce::jlimit(0.0f, 1.0f, input);
    switch (scaleCurve)
    {
        case KnobScaleCurve::Logarithmic:
            return std::log10(1.0f + clamped * 9.0f);
        case KnobScaleCurve::InverseLogarithmic:
            return 1.0f - std::log10(1.0f + (1.0f - clamped) * 9.0f);
        case KnobScaleCurve::Linear:
        default:
            return clamped;
    }
}

juce::String InvisKnob::getEffectiveValueText() const
{
    if (isOffState)
        return "OFF";

    if (valueFormatter != nullptr)
        return valueFormatter(currentValue);

    return valueText;
}

void InvisKnob::setOff(bool shouldBeOff, juce::NotificationType notification)
{
    if (isOffState != shouldBeOff)
    {
        isOffState = shouldBeOff;
        repaint();

        if (notification != juce::dontSendNotification && onValueChanged != nullptr)
        {
            onValueChanged(currentValue);
        }
    }
}

void InvisKnob::setValue(float normalizedValue, juce::NotificationType notification)
{
    const float clamped = juce::jlimit(0.0f, 1.0f, normalizedValue);
    if (!isDragging)
    {
        rawDragValue = clamped;
    }

    if (isOffState || std::abs(currentValue - clamped) > 0.0001f)
    {
        isOffState = false;
        currentValue = clamped;
        repaint();

        if (notification != juce::dontSendNotification && onValueChanged != nullptr)
        {
            onValueChanged(currentValue);
        }
    }
}

void InvisKnob::paint(juce::Graphics& g)
{
    const auto theme = getTheme();
    auto bounds = getLocalBounds().toFloat();
    if (bounds.getWidth() <= 0 || bounds.getHeight() <= 0) return;

    const juce::String displayText = getEffectiveValueText();

    // Preset Size Specifications (XS, S, M, L, XL - Default: M)
    float titleFontSize = 11.5f;
    float valueFontSize = 12.5f;
    float fixedLedDotRadius = 4.0f;
    float fixedTrackWidth = 2.8f;
    float diameterRatio = 0.58f;

    switch (knobSize)
    {
        case InvisKnobSize::XS:
            titleFontSize = 9.5f;
            valueFontSize = 10.5f;
            fixedLedDotRadius = 3.0f;
            fixedTrackWidth = 2.0f;
            diameterRatio = 0.52f;
            break;
        case InvisKnobSize::S:
            titleFontSize = 10.0f;
            valueFontSize = 11.0f;
            fixedLedDotRadius = 3.4f;
            fixedTrackWidth = 2.4f;
            diameterRatio = 0.60f;
            break;
        case InvisKnobSize::M:
        default:
            titleFontSize = 11.5f;
            valueFontSize = 12.5f;
            fixedLedDotRadius = 4.0f;
            fixedTrackWidth = 2.8f;
            diameterRatio = 0.68f;
            break;
        case InvisKnobSize::L:
            titleFontSize = 13.5f;
            valueFontSize = 14.5f;
            fixedLedDotRadius = 4.8f;
            fixedTrackWidth = 3.6f;
            diameterRatio = 0.78f;
            break;
        case InvisKnobSize::XL:
            titleFontSize = 15.5f;
            valueFontSize = 17.5f;
            fixedLedDotRadius = 5.8f;
            fixedTrackWidth = 4.5f;
            diameterRatio = 0.88f;
            break;
    }

    const float labelHeight = labelText.isNotEmpty() ? titleFontSize * 1.35f : 0.0f;
    const float valueHeight = displayText.isNotEmpty() ? valueFontSize * 1.35f : 0.0f;

    const auto knobArea = bounds.withTrimmedTop(labelHeight).withTrimmedBottom(valueHeight);
    const float diameter = std::min(knobArea.getWidth(), knobArea.getHeight()) * diameterRatio;
    if (diameter <= 0.0f) return;

    const auto center = knobArea.getCentre();
    const float radius = diameter * 0.5f;
    const float dynamicTrackWidth = fixedTrackWidth;

    // Draw Top Title Label with Constant Font Size
    if (labelText.isNotEmpty())
    {
        g.setColour(theme.textPrimary);
        g.setFont(juce::FontOptions(titleFontSize, juce::Font::bold));
        g.drawText(labelText, bounds.removeFromTop(labelHeight), juce::Justification::centred, true);
    }

    // Arc Angles Setup with Customizable Angle Range (in Degrees)
    const float totalStartAngle = juce::degreesToRadians(startAngleDegrees);
    const float totalEndAngle   = juce::degreesToRadians(endAngleDegrees);
    const float gapAngle        = 0.32f; // ~18 degrees visual detent gap for OFF

    float activeStartAngle = totalStartAngle;
    float activeEndAngle   = totalEndAngle;

    if (offPosition == OffPosition::Start)
    {
        activeStartAngle = totalStartAngle + gapAngle;
    }
    else if (offPosition == OffPosition::End)
    {
        activeEndAngle = totalEndAngle - gapAngle;
    }

    // Pointer angle calculation
    float currentAngle = activeStartAngle;

    if (isOffState)
    {
        currentAngle = (offPosition == OffPosition::End) ? totalEndAngle : totalStartAngle;
    }
    else
    {
        currentAngle = activeStartAngle + currentValue * (activeEndAngle - activeStartAngle);
    }

    // Draw Background track arc for active audio range
    juce::Path bgPath;
    bgPath.addCentredArc(center.x, center.y, radius, radius, 0.0f, activeStartAngle, activeEndAngle, true);
    g.setColour(theme.knobTrackBg);
    g.strokePath(bgPath, juce::PathStrokeType(dynamicTrackWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Calculate Value Arc Origin Angle (Supports customArcOriginPosition)
    float arcOriginNorm = 0.0f;
    if (customArcOriginPosition.has_value())
    {
        arcOriginNorm = customArcOriginPosition.value();
    }
    else if (valueArcOrigin == ValueArcOrigin::Center)
    {
        arcOriginNorm = 0.5f;
    }
    else if (valueArcOrigin == ValueArcOrigin::End)
    {
        arcOriginNorm = 1.0f;
    }

    const float arcOriginAngle = activeStartAngle + arcOriginNorm * (activeEndAngle - activeStartAngle);

    const juce::Colour activeAccentColor = customPointerColor.value_or(theme.accentPrimary);

    // Update LED ballistics for both value arc and pointer dot
    pointerLedBallistics.setState(!isOffState);
    pointerLedBallistics.update(0.016f);
    const float lum = pointerLedBallistics.getCurrentLuminance();

    // Calculate last active audio angle (ignoring the OFF visual detent gap)
    const float lastActiveAngle = activeStartAngle + currentValue * (activeEndAngle - activeStartAngle);
    const float arcTargetAngle  = isOffState ? lastActiveAngle : currentAngle;

    // Draw Emissive Glowing LED Value Arc Track (Inherits LED Ballistics & Volumetric Bloom)
    if (lum > 0.01f && std::abs(arcTargetAngle - arcOriginAngle) > 0.005f)
    {
        juce::Path valuePath;
        const float fromAngle = std::min(arcOriginAngle, arcTargetAngle);
        const float toAngle   = std::max(arcOriginAngle, arcTargetAngle);
        valuePath.addCentredArc(center.x, center.y, radius, radius, 0.0f, fromAngle, toAngle, true);

        const float alphaFactor = std::min(1.0f, lum);

        // Pass 1: Soft Volumetric Surface Bloom Field (2.5x track width)
        g.setColour(activeAccentColor.withMultipliedAlpha(0.24f * alphaFactor));
        g.strokePath(valuePath, juce::PathStrokeType(dynamicTrackWidth * 2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Pass 2: Intense Neon Diffusion Core (1.6x track width)
        g.setColour(activeAccentColor.withMultipliedAlpha(0.55f * alphaFactor));
        g.strokePath(valuePath, juce::PathStrokeType(dynamicTrackWidth * 1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Pass 3: Thin Sleek Translucent Crystal Light Guide Track
        g.setColour(activeAccentColor.brighter(0.25f).withMultipliedAlpha(alphaFactor));
        g.strokePath(valuePath, juce::PathStrokeType(dynamicTrackWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Pass 4: High-Intensity White Phosphor Core Thread
        g.setColour(juce::Colours::white.withAlpha(std::min(1.0f, 0.90f * lum)));
        g.strokePath(valuePath, juce::PathStrokeType(std::max(0.8f, dynamicTrackWidth * 0.35f), juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Draw Scale Ticks & Labels around knob (Proportional Projections)
    const float tickLength = std::max(2.5f, radius * 0.08f);
    const float tickRadius = radius + dynamicTrackWidth * 0.5f + std::max(2.0f, radius * 0.05f);
    const float labelRadius = tickRadius + tickLength + std::max(6.0f, radius * 0.22f);
    const float tickFontSize = std::clamp(radius * 0.22f, 9.0f, 15.0f);

    for (const auto& tick : scaleTicks)
    {
        float tickAngle = activeStartAngle + tick.normalizedPosition * (activeEndAngle - activeStartAngle);

        if (offPosition == OffPosition::Start && tick.isOff)
            tickAngle = totalStartAngle;
        else if (offPosition == OffPosition::End && tick.isOff)
            tickAngle = totalEndAngle;

        const juce::Point<float> tickStart(
            center.x + std::sin(tickAngle) * tickRadius,
            center.y - std::cos(tickAngle) * tickRadius
        );
        const juce::Point<float> tickEnd(
            center.x + std::sin(tickAngle) * (tickRadius + tickLength),
            center.y - std::cos(tickAngle) * (tickRadius + tickLength)
        );

        g.setColour(tick.isOff ? theme.accentSecondary : theme.textSecondary.withAlpha(0.6f));
        g.drawLine(juce::Line<float>(tickStart, tickEnd), 1.2f);

        if (tick.label.isNotEmpty())
        {
            const juce::Point<float> labelCenter(
                center.x + std::sin(tickAngle) * labelRadius,
                center.y - std::cos(tickAngle) * labelRadius
            );

            const float tickRectWidth = std::max(36.0f, tickFontSize * 3.8f);
            const float tickRectHeight = std::max(16.0f, tickFontSize * 1.4f);

            juce::Rectangle<float> labelRect(labelCenter.x - tickRectWidth * 0.5f, labelCenter.y - tickRectHeight * 0.5f, tickRectWidth, tickRectHeight);
            g.setFont(juce::FontOptions(tickFontSize, tick.isOff ? juce::Font::bold : juce::Font::plain));
            g.drawText(tick.label, labelRect, juce::Justification::centred, false);
        }
    }

    // Inner 3D Volumetric Complex Dial Face
    const float innerRadius = std::max(6.0f, radius - dynamicTrackWidth - 1.5f);
    if (innerRadius > 4.0f)
    {
        if (hasFilmstrip())
        {
            // 3D Drop Shadow under Filmstrip knob
            g.setColour(juce::Colours::black.withAlpha(0.65f));
            g.fillEllipse(center.x - innerRadius + 1.0f, center.y - innerRadius + 3.0f, innerRadius * 2.0f, innerRadius * 2.0f);

            // Compute frame index [0 .. filmstripFrames - 1]
            const int frameIdx = juce::jlimit(0, filmstripFrames - 1, juce::roundToInt(currentValue * static_cast<float>(filmstripFrames - 1)));

            const int imgW = filmstripImage.getWidth();
            const int imgH = filmstripImage.getHeight();

            const int frameW = filmstripIsVertical ? imgW : (imgW / filmstripFrames);
            const int frameH = filmstripIsVertical ? (imgH / filmstripFrames) : imgH;

            const int srcX = filmstripIsVertical ? 0 : (frameIdx * frameW);
            const int srcY = filmstripIsVertical ? (frameIdx * frameH) : 0;

            const float destDiam = innerRadius * 2.08f;
            const auto destRect = juce::Rectangle<float>(center.x - destDiam * 0.5f, center.y - destDiam * 0.5f, destDiam, destDiam);

            g.drawImage(filmstripImage,
                         juce::roundToInt(destRect.getX()), juce::roundToInt(destRect.getY()),
                         juce::roundToInt(destRect.getWidth()), juce::roundToInt(destRect.getHeight()),
                         srcX, srcY, frameW, frameH, false);

            // 3D Encapsulated Circular LED Pointer Dot with Phosphor Bloom
            const float pointerR = innerRadius * 0.65f;
            const float dotRadius = std::max(3.5f, innerRadius * 0.13f);
            const juce::Point<float> dotCenter(
                center.x + std::sin(currentAngle) * pointerR,
                center.y - std::cos(currentAngle) * pointerR
            );

            pointerLedBallistics.setState(!isOffState);
            pointerLedBallistics.update(0.016f);

            const juce::Colour activeLedColor = customPointerColor.value_or(theme.accentPrimary);
            InvisLED::drawLEDDot(g, dotCenter, dotRadius, activeLedColor, pointerLedBallistics.getCurrentLuminance(), LEDMountType::ProtrudingDome);
        }
        else if (knobCapImage.isValid())
        {
            // 3D Drop Shadow under turned metal knob
            g.setColour(juce::Colours::black.withAlpha(0.65f));
            g.fillEllipse(center.x - innerRadius + 1.0f, center.y - innerRadius + 3.0f, innerRadius * 2.0f, innerRadius * 2.0f);

            // Clip drawing region to perfect anti-aliased circle around knob body
            g.saveState();
            juce::Path circleClip;
            circleClip.addEllipse(center.x - innerRadius, center.y - innerRadius, innerRadius * 2.0f, innerRadius * 2.0f);
            g.reduceClipRegion(circleClip);

            // Rotate & render 3D titanium knob cap render
            const float knobDiam = innerRadius * 2.38f; // Scale slightly past clip boundary to ensure 0 black borders!
            auto transform = juce::AffineTransform::translation(-knobCapImage.getWidth() * 0.5f, -knobCapImage.getHeight() * 0.5f)
                                 .scaled(knobDiam / knobCapImage.getWidth(), knobDiam / knobCapImage.getHeight())
                                 .rotated(currentAngle)
                                 .translated(center.x, center.y);

            g.drawImageTransformed(knobCapImage, transform, true);
            g.restoreState();

            // 3D Encapsulated Circular LED Pointer Dot with Phosphor Bloom
            const float pointerR = innerRadius * 0.62f;
            const float dotRadius = std::max(3.5f, innerRadius * 0.13f);
            const juce::Point<float> dotCenter(
                center.x + std::sin(currentAngle) * pointerR,
                center.y - std::cos(currentAngle) * pointerR
            );

            pointerLedBallistics.setState(!isOffState);
            pointerLedBallistics.update(0.016f);

            const juce::Colour activeLedColor = customPointerColor.value_or(theme.accentPrimary);
            InvisLED::drawLEDDot(g, dotCenter, dotRadius, activeLedColor, pointerLedBallistics.getCurrentLuminance(), LEDMountType::ProtrudingDome);
        }
        else
        {
            // 1. Physical 3D Elevation Drop Shadows onto chassis faceplate (Casting 8mm height projection down-right)
            g.setColour(juce::Colours::black.withAlpha(0.40f));
            g.fillEllipse(center.x - innerRadius + 4.0f, center.y - innerRadius + 7.0f, innerRadius * 2.0f, innerRadius * 2.0f);
            g.setColour(juce::Colours::black.withAlpha(0.65f));
            g.fillEllipse(center.x - innerRadius + 2.0f, center.y - innerRadius + 4.5f, innerRadius * 2.0f, innerRadius * 2.0f);
            g.setColour(juce::Colours::black.withAlpha(0.85f));
            g.fillEllipse(center.x - innerRadius + 1.0f, center.y - innerRadius + 2.5f, innerRadius * 2.0f, innerRadius * 2.0f);

            // 2. Outer Flange Skirt Rim (Polished Dark Bakelite & Vintage Bronze Base)
            const auto skirtGradient = juce::ColourGradient(
                juce::Colour::fromRGB(74, 62, 52), center.x - innerRadius * 0.7f, center.y - innerRadius * 0.7f,
                juce::Colour::fromRGB(18, 14, 12), center.x + innerRadius * 0.8f, center.y + innerRadius * 0.8f,
                false
            );
            g.setGradientFill(skirtGradient);
            g.fillEllipse(center.x - innerRadius, center.y - innerRadius, innerRadius * 2.0f, innerRadius * 2.0f);

            // 3. 3D Dark Bakelite Knurled Grip Ring (64 Dense Micro-Rib Teeth)
            const float knurlOuterR = innerRadius - 1.0f;
            const float knurlInnerR = knurlOuterR - std::max(3.8f, innerRadius * 0.16f);
            const int numTeeth = 64;

            for (int i = 0; i < numTeeth; ++i)
            {
                const float toothAngle = currentAngle + static_cast<float>(i) * (juce::MathConstants<float>::twoPi / static_cast<float>(numTeeth));
                // Studio Top-Left Light Source (-45 deg) with smooth physical specular curve
                const float rawLight = (std::cos(toothAngle + 0.785f) + 1.0f) * 0.5f; // 0.0 .. 1.0
                const float specular = std::pow(rawLight, 1.8f);
                const float ambient = 0.12f + rawLight * 0.40f + specular * 0.40f;

                const juce::Point<float> p1(center.x + std::sin(toothAngle) * knurlInnerR, center.y - std::cos(toothAngle) * knurlInnerR);
                const juce::Point<float> p2(center.x + std::sin(toothAngle) * knurlOuterR, center.y - std::cos(toothAngle) * knurlOuterR);

                const juce::uint8 rVal = static_cast<juce::uint8>(juce::jlimit(16.0f, 130.0f, 18.0f + ambient * 110.0f));
                const juce::uint8 gVal = static_cast<juce::uint8>(juce::jlimit(14.0f, 115.0f, 15.0f + ambient * 98.0f));
                const juce::uint8 bVal = static_cast<juce::uint8>(juce::jlimit(10.0f, 95.0f, 12.0f + ambient * 80.0f));
                const auto toothColor = juce::Colour::fromRGB(rVal, gVal, bVal);

                g.setColour(toothColor);
                g.drawLine(juce::Line<float>(p1, p2), 1.3f);
            }

            // 4. Deep Recessed Shadow Moat under Ivory Cap
            const float moatRadius = knurlInnerR - 1.0f;
            if (moatRadius > 0.0f)
            {
                g.setColour(juce::Colour::fromRGB(12, 10, 8));
                g.fillEllipse(center.x - moatRadius, center.y - moatRadius, moatRadius * 2.0f, moatRadius * 2.0f);
            }

            // 5. Photorealistic Almost Black Carbon-Obsidian Cap with Fine CNC Micro-Texture
            const float capRadius = moatRadius - std::max(1.0f, innerRadius * 0.04f);
            if (capRadius > 0.0f)
            {
                const float heightShiftY = -1.8f; // Optical 3D Perspective Elevation
                const juce::Point<float> capCenter(center.x, center.y + heightShiftY);

                // 3D Drop Shadow under Carbon cap
                g.setColour(juce::Colours::black.withAlpha(0.75f));
                g.fillEllipse(capCenter.x - capRadius, capCenter.y - capRadius + 2.5f, capRadius * 2.0f, capRadius * 2.0f);

                // Photorealistic Almost Black Anodized Obsidian-Carbon Cap Gradient
                const auto capGradient = juce::ColourGradient(
                    juce::Colour::fromRGB(46, 52, 62), capCenter.x - capRadius * 0.65f, capCenter.y - capRadius * 0.65f,
                    juce::Colour::fromRGB(14, 16, 20), capCenter.x + capRadius * 0.75f, capCenter.y + capRadius * 0.75f,
                    false
                );
                g.setGradientFill(capGradient);
                g.fillEllipse(capCenter.x - capRadius, capCenter.y - capRadius, capRadius * 2.0f, capRadius * 2.0f);

                // Fine Micro-Engineered CNC Lathe Texture Rings
                for (float r = capRadius * 0.20f; r < capRadius * 0.94f; r += 2.0f)
                {
                    g.setColour(juce::Colours::white.withAlpha(0.06f));
                    g.drawEllipse(capCenter.x - r, capCenter.y - r, r * 2.0f, r * 2.0f, 0.70f);
                    g.setColour(juce::Colours::black.withAlpha(0.25f));
                    g.drawEllipse(capCenter.x - r + 0.4f, capCenter.y - r + 0.4f, r * 2.0f, r * 2.0f, 0.40f);
                }

                // Polished Vintage Bronze / Gunmetal Bevel Rim Edge
                g.setColour(juce::Colour::fromRGB(180, 155, 125).withAlpha(0.65f));
                g.drawEllipse(capCenter.x - capRadius, capCenter.y - capRadius, capRadius * 2.0f, capRadius * 2.0f, 1.25f);
                g.setColour(juce::Colours::black.withAlpha(0.70f));
                g.drawEllipse(capCenter.x - capRadius + 0.75f, capCenter.y - capRadius + 0.75f, (capRadius - 0.75f) * 2.0f, (capRadius - 0.75f) * 2.0f, 1.0f);

                // 6. Encapsulated 3D Circular LED Dot Pointer in Recessed Bronze Collar
                const float pointerR = capRadius * 0.70f;
                const float dotRadius = fixedLedDotRadius;

                const juce::Point<float> dotCenter(
                    capCenter.x + std::sin(currentAngle) * pointerR,
                    capCenter.y - std::cos(currentAngle) * pointerR
                );

                // Recessed Dark Bronze Collar under LED dot on Black Cap
                g.setColour(juce::Colour::fromRGB(24, 20, 16).withAlpha(0.85f));
                g.fillEllipse(dotCenter.x - dotRadius - 1.2f, dotCenter.y - dotRadius - 1.2f, (dotRadius + 1.2f) * 2.0f, (dotRadius + 1.2f) * 2.0f);

                pointerLedBallistics.setState(!isOffState);
                pointerLedBallistics.update(0.016f);

                const juce::Colour activeLedColor = customPointerColor.value_or(theme.accentPrimary);
                InvisLED::drawLEDDot(g, dotCenter, dotRadius, activeLedColor, pointerLedBallistics.getCurrentLuminance(), LEDMountType::ProtrudingDome);
            }
        }
    }

    // Store & Draw Value Text directly below the dial face (Tight Proximity & Accent Color)
    if (displayText.isNotEmpty())
    {
        const float textWidth = std::max(42.0f, valueFontSize * 4.2f);
        const float textY = center.y + radius + dynamicTrackWidth * 0.5f + 3.0f;
        valueTextArea = juce::Rectangle<float>(center.x - textWidth * 0.5f, textY, textWidth, valueHeight);

        g.setColour(isOffState ? theme.accentSecondary : activeAccentColor);
        g.setFont(juce::FontOptions(valueFontSize, isOffState ? juce::Font::bold : juce::Font::plain));
        g.drawText(displayText, valueTextArea, juce::Justification::centred, true);
    }

    // If LED ballistics is actively decaying, schedule next frame repaint for butter-smooth 60fps animation
    if (pointerLedBallistics.getCurrentLuminance() > 0.005f && isOffState)
    {
        juce::MessageManager::callAsync([this]() { repaint(); });
    }
}

void InvisKnob::resized()
{
    if (inlineEditor != nullptr)
    {
        inlineEditor->setBounds(valueTextArea.toNearestInt());
    }
}

void InvisKnob::mouseDown(const juce::MouseEvent& e)
{
    if (valueTextArea.contains(e.position))
    {
        showInlineEditor();
        return;
    }

    // Check if user clicked directly on any ScaleTick label or mark
    auto bounds = getLocalBounds().toFloat();
    const juce::String displayText = getEffectiveValueText();
    const float labelHeight = labelText.isNotEmpty() ? std::max(14.0f, bounds.getHeight() * 0.15f) : 0.0f;
    const float valueHeight = displayText.isNotEmpty() ? std::max(12.0f, bounds.getHeight() * 0.14f) : 0.0f;
    const auto knobArea = bounds.withTrimmedTop(labelHeight).withTrimmedBottom(valueHeight);
    const float diameter = std::min(knobArea.getWidth(), knobArea.getHeight()) * 0.55f;

    if (diameter > 0.0f)
    {
        const auto center = knobArea.getCentre();
        const float radius = diameter * 0.5f;
        const float dynamicTrackWidth = std::max(2.5f, diameter * 0.08f);

        const float totalStartAngle = juce::degreesToRadians(startAngleDegrees);
        const float totalEndAngle   = juce::degreesToRadians(endAngleDegrees);
        const float gapAngle        = 0.32f;

        float activeStartAngle = totalStartAngle;
        float activeEndAngle   = totalEndAngle;

        if (offPosition == OffPosition::Start) activeStartAngle = totalStartAngle + gapAngle;
        else if (offPosition == OffPosition::End) activeEndAngle = totalEndAngle - gapAngle;

        const float tickLength = std::max(2.5f, radius * 0.08f);
        const float tickRadius = radius + dynamicTrackWidth * 0.5f + std::max(2.0f, radius * 0.05f);
        const float labelRadius = tickRadius + tickLength + std::max(6.0f, radius * 0.22f);
        const float tickFontSize = std::clamp(radius * 0.22f, 9.0f, 15.0f);
        const float tickRectWidth = std::max(36.0f, tickFontSize * 3.8f);
        const float tickRectHeight = std::max(16.0f, tickFontSize * 1.4f);

        for (const auto& tick : scaleTicks)
        {
            float tickAngle = activeStartAngle + tick.normalizedPosition * (activeEndAngle - activeStartAngle);
            if (offPosition == OffPosition::Start && tick.isOff) tickAngle = totalStartAngle;
            else if (offPosition == OffPosition::End && tick.isOff) tickAngle = totalEndAngle;

            const juce::Point<float> labelCenter(
                center.x + std::sin(tickAngle) * labelRadius,
                center.y - std::cos(tickAngle) * labelRadius
            );

            const juce::Rectangle<float> labelRect(
                labelCenter.x - tickRectWidth * 0.5f,
                labelCenter.y - tickRectHeight * 0.5f,
                tickRectWidth,
                tickRectHeight
            );

            if (labelRect.expanded(4.0f).contains(e.position))
            {
                if (onDragStarted != nullptr) onDragStarted();

                if (tick.isOff)
                {
                    setOff(true, juce::sendNotification);
                }
                else
                {
                    setValue(tick.normalizedPosition, juce::sendNotification);
                }

                if (onDragEnded != nullptr) onDragEnded();
                return;
            }
        }
    }

    lastMousePos = e.position;
    isDragging = true;
    rawDragValue = isOffState ? (offPosition == OffPosition::End ? 1.05f : -0.05f) : currentValue;

    if (onDragStarted != nullptr)
        onDragStarted();
}

void InvisKnob::setStickyPositions(const std::vector<float>& positions, float defaultTolerance)
{
    stickyPoints.clear();
    for (float pos : positions)
    {
        stickyPoints.push_back({ juce::jlimit(0.0f, 1.0f, pos), defaultTolerance });
    }
}

void InvisKnob::addStickyPoint(float normalizedPosition, float tolerance)
{
    stickyPoints.push_back({ juce::jlimit(0.0f, 1.0f, normalizedPosition), tolerance });
}

void InvisKnob::mouseDrag(const juce::MouseEvent& e)
{
    if (!isDragging) return;

    const float deltaY = lastMousePos.y - e.position.y;
    lastMousePos = e.position;

    float stepDelta = deltaY * (e.mods.isShiftDown() ? 0.001f : 0.005f);

    // Tactile Magnetic Friction Engine: dampen drag speed inside magnetic capture zone
    if (!e.mods.isShiftDown() && !stickyPoints.empty())
    {
        for (const auto& sp : stickyPoints)
        {
            const float dist = std::abs(rawDragValue - sp.normalizedPosition);
            const float magnetTolerance = std::max(0.025f, sp.snapTolerance * 2.0f);

            if (dist < magnetTolerance)
            {
                // Apply 75% drag resistance inside magnetic detent zone
                stepDelta *= 0.25f;
                break;
            }
        }
    }

    rawDragValue += stepDelta;

    // Check OFF threshold detent snapping (ultra-light response)
    if (offPosition == OffPosition::Start && rawDragValue <= -0.003f)
    {
        setOff(true, juce::sendNotification);
    }
    else if (offPosition == OffPosition::End && rawDragValue >= 1.003f)
    {
        setOff(true, juce::sendNotification);
    }
    else
    {
        float targetVal = rawDragValue;

        // Firm magnetic lock when close to sticky point
        if (!e.mods.isShiftDown() && !stickyPoints.empty())
        {
            for (const auto& sp : stickyPoints)
            {
                if (std::abs(rawDragValue - sp.normalizedPosition) < 0.020f)
                {
                    targetVal = sp.normalizedPosition; // Lock smoothly to sticky point!
                    break;
                }
            }
        }

        setValue(targetVal, juce::sendNotification);
    }
}

void InvisKnob::mouseUp(const juce::MouseEvent&)
{
    if (isDragging)
    {
        isDragging = false;
        if (onDragEnded != nullptr)
            onDragEnded();
    }
}

void InvisKnob::mouseDoubleClick(const juce::MouseEvent&)
{
    if (onDragStarted != nullptr)
        onDragStarted();

    setValue(defaultValue, juce::sendNotification);

    if (onDragEnded != nullptr)
        onDragEnded();
}

void InvisKnob::mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    const float delta = wheel.deltaY * (wheel.isReversed ? -0.05f : 0.05f);
    setValue(currentValue + delta, juce::sendNotification);
}

void InvisKnob::showInlineEditor()
{
    if (inlineEditor != nullptr) return;

    const auto theme = getTheme();
    inlineEditor = std::make_unique<juce::TextEditor>();
    inlineEditor->setFont(juce::FontOptions(std::max(10.0f, valueTextArea.getHeight() * 0.85f)));
    inlineEditor->setText(getEffectiveValueText(), false);
    inlineEditor->selectAll();
    inlineEditor->setJustification(juce::Justification::centred);
    inlineEditor->setColour(juce::TextEditor::backgroundColourId, theme.surfacePanel);
    inlineEditor->setColour(juce::TextEditor::outlineColourId, theme.accentPrimary);
    inlineEditor->setColour(juce::TextEditor::textColourId, theme.textPrimary);
    inlineEditor->addListener(this);

    addAndMakeVisible(*inlineEditor);
    inlineEditor->setBounds(valueTextArea.toNearestInt());
    inlineEditor->grabKeyboardFocus();
}

void InvisKnob::commitInlineEditorValue()
{
    if (inlineEditor == nullptr) return;

    const juce::String text = inlineEditor->getText().trim().toUpperCase();

    if (text == "OFF")
    {
        setOff(true, juce::sendNotification);
    }
    else if (valueParser != nullptr)
    {
        const float norm = valueParser(text);
        setValue(norm, juce::sendNotification);
    }
    else
    {
        const float parsed = text.getFloatValue();
        setValue(parsed, juce::sendNotification);
    }

    dismissInlineEditor();
}

void InvisKnob::dismissInlineEditor()
{
    if (inlineEditor != nullptr)
    {
        inlineEditor->removeListener(this);
        removeChildComponent(inlineEditor.get());
        inlineEditor.reset();
        repaint();
    }
}

void InvisKnob::textEditorReturnKeyPressed(juce::TextEditor&)
{
    commitInlineEditorValue();
}

void InvisKnob::textEditorFocusLost(juce::TextEditor&)
{
    commitInlineEditorValue();
}

void InvisKnob::textEditorEscapeKeyPressed(juce::TextEditor&)
{
    dismissInlineEditor();
}

} // namespace invis::ui
