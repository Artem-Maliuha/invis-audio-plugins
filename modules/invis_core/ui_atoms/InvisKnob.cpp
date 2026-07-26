#include "InvisKnob.h"
#include <algorithm>
#include <cmath>

namespace invis::ui {

namespace {

// Canonical pointer sweep: 220deg .. 500deg, i.e. +-140deg around 12 o'clock. Intrinsic sizing
// is always computed from this sweep so a preset's footprint is a constant, even if a caller
// narrows the range via setAngleRange().
constexpr float kSweepHalfSpanDeg = 140.0f;

// Worst-case tick-label INK extents, as multiples of the tick font size. The drawn label
// rectangle is deliberately generous (text is centred inside it, so it never truncates), which
// makes it useless for footprint maths - only the ink actually occupies space.
constexpr float kTickInkHalfWidthRatio  = 1.25f; // ~4 glyphs
constexpr float kTickInkHalfHeightRatio = 0.70f;

/** Everything the intrinsic size and the paint layout both need, derived once from metrics. */
struct KnobFootprint {
    float radius;
    float tickRadius;
    float labelRadius;
    float tickLabelWidth;
    float tickLabelHeight;
    float titleHeight;
    float valueHeight;
    float valueWidth;
    float upExtent;    // Dial centre -> top of the occupied area
    float downExtent;  // Dial centre -> bottom of the occupied area
    float halfWidth;
    int width;
    int height;
};

KnobFootprint computeFootprint(const KnobMetrics& m)
{
    KnobFootprint f;

    f.radius      = m.diameter * 0.5f;
    f.tickRadius  = f.radius + m.trackWidth * 0.5f + m.tickRingGap;
    f.labelRadius = f.tickRadius + m.tickLength + m.tickLabelGap;

    f.tickLabelWidth  = std::max(36.0f, m.tickFontSize * 3.8f);
    f.tickLabelHeight = std::max(16.0f, m.tickFontSize * 1.4f);

    f.titleHeight = m.titleFontSize * 1.35f;
    f.valueHeight = m.valueFontSize * 1.35f;
    f.valueWidth  = std::max(42.0f, m.valueFontSize * 4.2f);

    const float inkHalfW  = m.tickFontSize * kTickInkHalfWidthRatio;
    const float inkHalfH  = m.tickFontSize * kTickInkHalfHeightRatio;
    const float trackEdge = f.radius + m.trackWidth * 0.5f;

    // Horizontally the sweep crosses +-90deg, so labels reach the full label radius sideways.
    f.halfWidth = std::max({ f.labelRadius + inkHalfW, trackEdge, f.valueWidth * 0.5f });

    // Straight up (0deg) is inside the sweep, so the top reaches the full label radius. Downwards
    // the sweep stops at +-140deg, so the lowest label only reaches cos(140deg) of it - but the
    // value readout sits below the dial body and usually wins.
    const float downFactor = std::abs(std::cos(juce::degreesToRadians(kSweepHalfSpanDeg)));

    f.upExtent   = std::max(f.labelRadius + inkHalfH, trackEdge);
    f.downExtent = std::max(f.labelRadius * downFactor + inkHalfH,
                            trackEdge + m.valueGap + f.valueHeight);

    f.width  = static_cast<int>(std::ceil(f.halfWidth * 2.0f));
    f.height = static_cast<int>(std::ceil(f.titleHeight + m.titleGap + f.upExtent + f.downExtent));

    return f;
}

} // namespace

KnobMetrics InvisKnob::getMetrics(InvisKnobSize size)
{
    // The skirt, rim chamfers and 3D elevation are absolute per preset and shrink WITH the dial:
    // deriving them from a ratio + a floor (the old max(3.8f, r * 0.16f)) made small knobs wear a
    // disproportionately fat grip ring - 28% of the body on XS against 16% on XL.
    //
    //        diameter  track  led   title  value  tick  tickLen  ringGap  labelGap  titleGap  valueGap  skirt  rimGap  elev  teeth  lampDia
    switch (size)
    {
        // XS: the tick ring needs a wider standoff than a pure ratio would suggest - the label
        // BOX is centred on the ring, so a 3-glyph label like "OFF" reaches back toward the dial
        // and collides with the skirt unless the ring is pushed clear.
        case InvisKnobSize::XS: return {  34.0f, 2.0f, 3.0f,  9.5f, 10.5f,  8.0f, 2.5f, 2.5f, 7.0f, 2.0f, 3.0f, 2.2f, 0.6f, 0.40f, 36,  5.0f };
        case InvisKnobSize::S:  return {  46.0f, 2.4f, 3.4f, 10.0f, 11.0f,  8.5f, 3.0f, 2.0f, 6.0f, 2.0f, 3.0f, 3.0f, 0.8f, 0.60f, 44,  6.0f };
        case InvisKnobSize::L:  return {  84.0f, 3.6f, 4.8f, 13.5f, 14.5f, 11.0f, 4.0f, 3.0f, 8.0f, 3.0f, 3.0f, 5.6f, 1.3f, 1.40f, 64,  8.5f };
        case InvisKnobSize::XL: return { 110.0f, 4.5f, 5.8f, 15.5f, 17.5f, 12.5f, 5.0f, 3.5f, 9.0f, 4.0f, 3.0f, 7.2f, 1.6f, 1.80f, 76, 10.0f };
        case InvisKnobSize::M:
        default:                return {  62.0f, 2.8f, 4.0f, 11.5f, 12.5f,  9.5f, 3.5f, 2.5f, 7.0f, 3.0f, 3.0f, 4.2f, 1.0f, 1.00f, 56,  7.0f };
    }
}

juce::Point<int> InvisKnob::getIntrinsicSize(InvisKnobSize size)
{
    const auto f = computeFootprint(getMetrics(size));
    return { f.width, f.height };
}

KnobLayout InvisKnob::computeLayout() const
{
    const auto m = getMetrics(knobSize);
    const auto f = computeFootprint(m);

    KnobLayout l;
    l.content         = centreIntrinsic(getLocalBounds().toFloat(), { f.width, f.height });
    l.radius          = f.radius;
    l.trackWidth      = m.trackWidth;
    l.ledDotRadius    = m.ledDotRadius;
    l.titleFontSize   = m.titleFontSize;
    l.valueFontSize   = m.valueFontSize;
    l.tickFontSize    = m.tickFontSize;
    l.tickRadius      = f.tickRadius;
    l.tickLength      = m.tickLength;
    l.labelRadius     = f.labelRadius;
    l.tickLabelWidth  = f.tickLabelWidth;
    l.tickLabelHeight = f.tickLabelHeight;
    l.skirtWidth      = m.skirtWidth;
    l.rimGap          = m.rimGap;
    l.elevation       = m.elevation;
    l.numTeeth        = m.numTeeth;

    auto area = l.content;
    l.titleArea = area.removeFromTop(f.titleHeight);
    area.removeFromTop(m.titleGap);

    // Title row. With a lamp the row becomes [text][gap][lamp] laid out as ONE centred unit, so
    // the pair stays optically centred whatever the label length ("HPF" vs "DRY/WET"). The knob's
    // intrinsic width is governed by the tick-label ring, which is far wider than any title, so
    // absorbing the lamp here never changes the atom's footprint.
    l.lampRadius = m.lampDiameter * 0.5f;

    if (indicatorVisible && labelText.isNotEmpty())
    {
        const juce::Font titleFont(juce::FontOptions(m.titleFontSize, juce::Font::bold));
        const float textWidth = juce::GlyphArrangement::getStringWidth(titleFont, labelText);
        const float unitWidth = textWidth + layout::kIndicatorLabelGap + m.lampDiameter;
        const float unitLeft  = l.titleArea.getCentreX() - unitWidth * 0.5f;

        l.titleTextArea = juce::Rectangle<float>(unitLeft, l.titleArea.getY(),
                                                 textWidth, l.titleArea.getHeight());
        l.lampCentre    = { unitLeft + textWidth + layout::kIndicatorLabelGap + l.lampRadius,
                            l.titleArea.getCentreY() };
    }
    else
    {
        l.titleTextArea = l.titleArea;
        l.lampCentre    = l.titleArea.getCentre();
    }

    l.centre = { l.content.getCentreX(), area.getY() + f.upExtent };

    l.valueArea = juce::Rectangle<float>(
        l.centre.x - f.valueWidth * 0.5f,
        l.centre.y + f.radius + m.trackWidth * 0.5f + m.valueGap,
        f.valueWidth,
        f.valueHeight
    );

    return l;
}

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

    // Single source of geometric truth: identical maths feeds rendering and hit-testing, and the
    // atom always draws at its intrinsic size centred in whatever bounds it was given, so it can
    // never be stretched, squashed or shrunk by its container.
    const auto layout = computeLayout();

    const auto center = layout.centre;
    const float radius = layout.radius;
    const float dynamicTrackWidth = layout.trackWidth;
    const float titleFontSize = layout.titleFontSize;
    const float valueFontSize = layout.valueFontSize;
    const float fixedLedDotRadius = layout.ledDotRadius;

    // Draw Top Title Label with Constant Font Size
    if (labelText.isNotEmpty())
    {
        g.setColour(theme.textPrimary);
        g.setFont(juce::FontOptions(titleFontSize, juce::Font::bold));
        g.drawText(labelText, layout.titleTextArea, juce::Justification::centred, true);
    }

    // Indicator lamp beside the title. Drawn inline rather than as a child component so the
    // volumetric spill lands on the knob's own surface instead of being clipped at a child edge.
    if (indicatorVisible)
    {
        const float lampLevel = indicatorBallistics.getCurrent();

        InvisLevelLamp::drawLamp(g, layout.lampCentre, layout.lampRadius,
                                 lampLevel, lamp::getLuminance(lampLevel),
                                 indicatorActive, indicatorMount);
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

    // Draw Scale Ticks & Labels around knob (constant ring geometry from the size preset)
    const float tickLength   = layout.tickLength;
    const float tickRadius   = layout.tickRadius;
    const float labelRadius  = layout.labelRadius;
    const float tickFontSize = layout.tickFontSize;

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

            const float tickRectWidth = layout.tickLabelWidth;
            const float tickRectHeight = layout.tickLabelHeight;

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
            // 1. Physical 3D Elevation Drop Shadows onto chassis faceplate (projection down-right).
            //    Offsets scale with the preset so a small knob sits low, a large knob stands proud.
            const float elev = layout.elevation;

            g.setColour(juce::Colours::black.withAlpha(0.40f));
            g.fillEllipse(center.x - innerRadius + 4.0f * elev, center.y - innerRadius + 7.0f * elev, innerRadius * 2.0f, innerRadius * 2.0f);
            g.setColour(juce::Colours::black.withAlpha(0.65f));
            g.fillEllipse(center.x - innerRadius + 2.0f * elev, center.y - innerRadius + 4.5f * elev, innerRadius * 2.0f, innerRadius * 2.0f);
            g.setColour(juce::Colours::black.withAlpha(0.85f));
            g.fillEllipse(center.x - innerRadius + 1.0f * elev, center.y - innerRadius + 2.5f * elev, innerRadius * 2.0f, innerRadius * 2.0f);

            // 2. Outer Flange Skirt Rim (Polished Dark Bakelite & Vintage Bronze Base)
            const auto skirtGradient = juce::ColourGradient(
                juce::Colour::fromRGB(74, 62, 52), center.x - innerRadius * 0.7f, center.y - innerRadius * 0.7f,
                juce::Colour::fromRGB(18, 14, 12), center.x + innerRadius * 0.8f, center.y + innerRadius * 0.8f,
                false
            );
            g.setGradientFill(skirtGradient);
            g.fillEllipse(center.x - innerRadius, center.y - innerRadius, innerRadius * 2.0f, innerRadius * 2.0f);

            // 3. 3D Dark Bakelite Knurled Grip Ring ("skirt") - constant thickness per size preset
            const float knurlOuterR = innerRadius - layout.rimGap;
            const float knurlInnerR = std::max(innerRadius * 0.25f, knurlOuterR - layout.skirtWidth);
            const int numTeeth = layout.numTeeth;

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
            const float moatRadius = knurlInnerR - layout.rimGap;
            if (moatRadius > 0.0f)
            {
                g.setColour(juce::Colour::fromRGB(12, 10, 8));
                g.fillEllipse(center.x - moatRadius, center.y - moatRadius, moatRadius * 2.0f, moatRadius * 2.0f);
            }

            // 5. Photorealistic Almost Black Carbon-Obsidian Cap with Fine CNC Micro-Texture
            const float capRadius = moatRadius - layout.rimGap;
            if (capRadius > 0.0f)
            {
                const float heightShiftY = -1.8f * elev; // Optical 3D Perspective Elevation
                const juce::Point<float> capCenter(center.x, center.y + heightShiftY);

                // 3D Drop Shadow under Carbon cap
                g.setColour(juce::Colours::black.withAlpha(0.75f));
                g.fillEllipse(capCenter.x - capRadius, capCenter.y - capRadius + 2.5f * elev, capRadius * 2.0f, capRadius * 2.0f);

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

                // Polished Vintage Bronze / Gunmetal Bevel Rim Edge (thins down with the preset)
                const float bevelStroke = std::max(0.6f, 1.25f * elev);
                const float bevelInset  = std::max(0.4f, 0.75f * elev);

                g.setColour(juce::Colour::fromRGB(180, 155, 125).withAlpha(0.65f));
                g.drawEllipse(capCenter.x - capRadius, capCenter.y - capRadius, capRadius * 2.0f, capRadius * 2.0f, bevelStroke);
                g.setColour(juce::Colours::black.withAlpha(0.70f));
                g.drawEllipse(capCenter.x - capRadius + bevelInset, capCenter.y - capRadius + bevelInset, (capRadius - bevelInset) * 2.0f, (capRadius - bevelInset) * 2.0f, std::max(0.5f, 1.0f * elev));

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
    valueTextArea = layout.valueArea;

    if (displayText.isNotEmpty())
    {
        g.setColour(isOffState ? theme.accentSecondary : activeAccentColor);
        g.setFont(juce::FontOptions(valueFontSize, isOffState ? juce::Font::bold : juce::Font::plain));
        g.drawText(displayText, valueTextArea, juce::Justification::centred, true);
    }

    // If LED ballistics is actively decaying, schedule next frame repaint for butter-smooth 60fps animation
    if (pointerLedBallistics.getCurrentLuminance() > 0.005f && isOffState)
    {
        juce::MessageManager::callAsync([this]() { repaint(); });
    }

    // THE BINDING, said plainly. Small, dim and out of the way of the value - it is a fact about
    // the control, not a reading from it.
    if (midiCc >= 0)
    {
        auto box = layout.content;
        box = box.removeFromBottom(layout.valueFontSize * 1.15f);

        g.setFont(InvisFonts::getDisplayFont(layout.tickFontSize * 0.95f, false));
        g.setColour(theme.textSecondary.withAlpha(0.55f));
        g.drawText("CC " + juce::String(midiCc), box, juce::Justification::centredRight, false);
    }
}

void InvisKnob::updateIndicatorBallistics(float deltaTimeSeconds)
{
    if (!indicatorVisible) return;

    const float before = indicatorBallistics.getCurrent();
    indicatorBallistics.update(deltaTimeSeconds);

    if (std::abs(indicatorBallistics.getCurrent() - before) > 0.0008f)
        repaint();
}

void InvisKnob::resized()
{
    // STRICT PROPORTIONALITY INTEGRITY: a size preset IS a size. If this fires, a container laid
    // the knob out with an arbitrary rect instead of setBoundsCentredIn() / getIntrinsicSize().
    // The knob will render correctly regardless (it letterboxes inside the given bounds) - the
    // assert exists so the layout bug is visible at development time rather than shipped.
    const auto intrinsic = getIntrinsicSize();
    jassert(getWidth() == 0 || (getWidth() == intrinsic.x && getHeight() == intrinsic.y));

    valueTextArea = computeLayout().valueArea;

    if (inlineEditor != nullptr)
    {
        inlineEditor->setBounds(valueTextArea.toNearestInt());
    }
}

void InvisKnob::mouseDown(const juce::MouseEvent& e)
{
    // Before anything else: a right-click is never a value change, and treating it as the start of
    // a drag would move the control you were only asking about.
    if (e.mods.isPopupMenu())
    {
        if (onSecondaryClick) onSecondaryClick();
        return;
    }

    if (valueTextArea.contains(e.position))
    {
        showInlineEditor();
        return;
    }

    // Check if user clicked directly on any ScaleTick label or mark.
    // Uses the exact same layout as paint() - previously this recomputed the geometry with
    // different constants, so the hit zones drifted away from the drawn labels.
    const auto layout = computeLayout();

    if (layout.radius > 0.0f)
    {
        const auto center = layout.centre;

        const float totalStartAngle = juce::degreesToRadians(startAngleDegrees);
        const float totalEndAngle   = juce::degreesToRadians(endAngleDegrees);
        const float gapAngle        = 0.32f;

        float activeStartAngle = totalStartAngle;
        float activeEndAngle   = totalEndAngle;

        if (offPosition == OffPosition::Start) activeStartAngle = totalStartAngle + gapAngle;
        else if (offPosition == OffPosition::End) activeEndAngle = totalEndAngle - gapAngle;

        const float labelRadius = layout.labelRadius;
        const float tickRectWidth = layout.tickLabelWidth;
        const float tickRectHeight = layout.tickLabelHeight;

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
