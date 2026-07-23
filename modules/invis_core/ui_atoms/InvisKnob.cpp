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
    rawDragValue = clamped;
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

    const float labelHeight = labelText.isNotEmpty() ? std::max(12.0f, bounds.getHeight() * 0.14f) : 0.0f;
    const float valueHeight = displayText.isNotEmpty() ? std::max(10.0f, bounds.getHeight() * 0.13f) : 0.0f;

    const auto knobArea = bounds.withTrimmedTop(labelHeight).withTrimmedBottom(valueHeight);
    const float diameter = std::min(knobArea.getWidth(), knobArea.getHeight()) - 22.0f;
    if (diameter <= 0.0f) return;

    const auto center = knobArea.getCentre();
    const float radius = diameter * 0.5f;
    const float dynamicTrackWidth = std::max(2.0f, diameter * 0.08f);

    // Draw Top Label
    if (labelText.isNotEmpty())
    {
        g.setColour(theme.textPrimary);
        g.setFont(juce::FontOptions(std::max(10.0f, labelHeight * 0.8f), juce::Font::bold));
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

    // Draw Active value arc
    if (!isOffState && currentAngle > activeStartAngle)
    {
        juce::Path valuePath;
        valuePath.addCentredArc(center.x, center.y, radius, radius, 0.0f, activeStartAngle, currentAngle, true);
        g.setColour(theme.accentPrimary);
        g.strokePath(valuePath, juce::PathStrokeType(dynamicTrackWidth + 0.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Draw Scale Ticks & Labels around knob
    const float tickLength = 4.0f;
    const float tickRadius = radius + dynamicTrackWidth * 0.5f + 3.0f;
    const float labelRadius = tickRadius + tickLength + 8.0f;

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

            juce::Rectangle<float> labelRect(labelCenter.x - 14.0f, labelCenter.y - 7.0f, 28.0f, 14.0f);
            g.setFont(juce::FontOptions(std::max(8.0f, diameter * 0.12f), tick.isOff ? juce::Font::bold : juce::Font::plain));
            g.drawText(tick.label, labelRect, juce::Justification::centred, false);
        }
    }

    // Inner 3D Volumetric Complex Dial Face
    const float innerRadius = radius - dynamicTrackWidth - 3.0f;
    if (innerRadius > 10.0f)
    {
        // 1. Ambient & Contact Drop Shadows
        g.setColour(juce::Colours::black.withAlpha(0.35f));
        g.fillEllipse(center.x - innerRadius + 2.0f, center.y - innerRadius + 4.0f, innerRadius * 2.0f, innerRadius * 2.0f);
        g.setColour(juce::Colours::black.withAlpha(0.55f));
        g.fillEllipse(center.x - innerRadius + 1.0f, center.y - innerRadius + 2.0f, innerRadius * 2.0f, innerRadius * 2.0f);

        // 2. Outer Flange Skirt Rim (Brushed Metal Base)
        const auto skirtGradient = juce::ColourGradient(
            juce::Colour::fromRGB(110, 120, 135), center.x - innerRadius, center.y - innerRadius,
            juce::Colour::fromRGB(15, 18, 24), center.x + innerRadius, center.y + innerRadius,
            false
        );
        g.setGradientFill(skirtGradient);
        g.fillEllipse(center.x - innerRadius, center.y - innerRadius, innerRadius * 2.0f, innerRadius * 2.0f);

        // 3. Knurled Grip Ring (32 Directional Rib Teeth Rotating with Knob)
        const float knurlOuterR = innerRadius - 1.5f;
        const float knurlInnerR = knurlOuterR - std::max(3.0f, innerRadius * 0.12f);
        const int numTeeth = 32;

        for (int i = 0; i < numTeeth; ++i)
        {
            const float toothAngle = currentAngle + static_cast<float>(i) * (juce::MathConstants<float>::twoPi / static_cast<float>(numTeeth));
            const float lightFactor = std::max(0.18f, (std::sin(toothAngle - 0.785f) + 1.0f) * 0.5f);

            const juce::Point<float> p1(center.x + std::sin(toothAngle) * knurlInnerR, center.y - std::cos(toothAngle) * knurlInnerR);
            const juce::Point<float> p2(center.x + std::sin(toothAngle) * knurlOuterR, center.y - std::cos(toothAngle) * knurlOuterR);

            const auto toothColor = juce::Colour::fromRGB(
                static_cast<juce::uint8>(20 + lightFactor * 130),
                static_cast<juce::uint8>(24 + lightFactor * 140),
                static_cast<juce::uint8>(30 + lightFactor * 155)
            );

            g.setColour(toothColor);
            g.drawLine(juce::Line<float>(p1, p2), 1.8f);
        }

        // 4. Deep Recessed Shadow Moat
        const float moatRadius = knurlInnerR - 1.0f;
        if (moatRadius > 0.0f)
        {
            g.setColour(juce::Colour::fromRGB(10, 12, 16));
            g.fillEllipse(center.x - moatRadius, center.y - moatRadius, moatRadius * 2.0f, moatRadius * 2.0f);
        }

        // 5. Center CNC Machined Anisotropic Aluminum Cap
        const float capRadius = moatRadius - std::max(2.0f, innerRadius * 0.08f);
        if (capRadius > 0.0f)
        {
            const auto capGradient = juce::ColourGradient(
                theme.surfacePanel.brighter(0.45f), center.x - capRadius * 0.6f, center.y - capRadius * 0.6f,
                theme.surfacePanel.darker(0.85f), center.x + capRadius * 0.7f, center.y + capRadius * 0.7f,
                false
            );
            g.setGradientFill(capGradient);
            g.fillEllipse(center.x - capRadius, center.y - capRadius, capRadius * 2.0f, capRadius * 2.0f);

            // Concentric CNC Lathe Micro-Grooves
            g.setColour(juce::Colours::white.withAlpha(0.04f));
            for (float r = capRadius * 0.3f; r < capRadius * 0.9f; r += 2.5f)
            {
                g.drawEllipse(center.x - r, center.y - r, r * 2.0f, r * 2.0f, 0.75f);
            }

            // Bevel Top Highlight Chamfer
            g.setColour(theme.textPrimary.withAlpha(0.2f));
            g.drawEllipse(center.x - capRadius, center.y - capRadius, capRadius * 2.0f, capRadius * 2.0f, 1.0f);
            g.setColour(juce::Colours::black.withAlpha(0.4f));
            g.drawEllipse(center.x - capRadius + 1.0f, center.y - capRadius + 1.0f, (capRadius - 1.0f) * 2.0f, (capRadius - 1.0f) * 2.0f, 1.0f);

            // 6. Encapsulated 3D LED Capsule Pointer Slot
            const float pointerStartR = capRadius * 0.25f;
            const float pointerEndR   = capRadius * 0.82f;

            const juce::Point<float> pStart(
                center.x + std::sin(currentAngle) * pointerStartR,
                center.y - std::cos(currentAngle) * pointerStartR
            );

            const juce::Point<float> pEnd(
                center.x + std::sin(currentAngle) * pointerEndR,
                center.y - std::cos(currentAngle) * pointerEndR
            );

            // Pointer Slot Shadow / Recess
            g.setColour(juce::Colours::black.withAlpha(0.7f));
            g.drawLine(juce::Line<float>(pStart, pEnd), std::max(3.0f, dynamicTrackWidth * 0.7f));

            if (!isOffState)
            {
                g.setColour(theme.accentPrimary.withAlpha(0.25f));
                g.drawLine(juce::Line<float>(pStart, pEnd), std::max(5.0f, dynamicTrackWidth * 1.2f));

                g.setColour(theme.accentPrimary.withAlpha(0.55f));
                g.drawLine(juce::Line<float>(pStart, pEnd), std::max(3.0f, dynamicTrackWidth * 0.75f));

                g.setColour(theme.accentPrimary.brighter(0.4f));
                g.drawLine(juce::Line<float>(pStart, pEnd), std::max(1.5f, dynamicTrackWidth * 0.4f));
            }
            else
            {
                g.setColour(theme.textSecondary.withAlpha(0.4f));
                g.drawLine(juce::Line<float>(pStart, pEnd), std::max(1.5f, dynamicTrackWidth * 0.4f));
            }
        }
    }

    // Store & Draw Value Text at Bottom
    if (displayText.isNotEmpty())
    {
        valueTextArea = bounds.removeFromBottom(valueHeight);
        g.setColour(isOffState ? theme.accentSecondary : theme.textSecondary);
        g.setFont(juce::FontOptions(std::max(9.0f, valueHeight * 0.85f), isOffState ? juce::Font::bold : juce::Font::plain));
        g.drawText(displayText, valueTextArea, juce::Justification::centred, true);
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

    lastMousePos = e.position;
    isDragging = true;
    rawDragValue = isOffState ? (offPosition == OffPosition::End ? 1.05f : -0.05f) : currentValue;

    if (onDragStarted != nullptr)
        onDragStarted();
}

void InvisKnob::mouseDrag(const juce::MouseEvent& e)
{
    if (!isDragging) return;

    const float deltaY = lastMousePos.y - e.position.y;
    lastMousePos = e.position;

    const float sensitivity = e.mods.isShiftDown() ? 0.001f : 0.005f;
    rawDragValue += deltaY * sensitivity;

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
        setValue(rawDragValue, juce::sendNotification);
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
