#include "InvisButton.h"
#include <cmath>

namespace invis::ui {

InvisButton::InvisButton()
{
    setRepaintsOnMouseActivity(true);
}

ButtonMetrics InvisButton::getMetrics(InvisButtonSize size)
{
    //        height  minWidth  padX   ledDia  font   corner
    switch (size)
    {
        case InvisButtonSize::XS: return { 16.0f, 38.0f, 5.0f, 4.5f,  8.0f, 3.0f };
        case InvisButtonSize::S:  return { 20.0f, 48.0f, 6.0f, 5.5f,  9.0f, 3.5f };
        case InvisButtonSize::L:  return { 30.0f, 76.0f, 9.0f, 8.0f, 12.0f, 5.0f };
        case InvisButtonSize::M:
        default:                  return { 24.0f, 60.0f, 7.0f, 6.5f, 10.5f, 4.0f };
    }
}

juce::Point<int> InvisButton::getIntrinsicSize(InvisButtonSize size, const juce::String& label, bool withLed)
{
    const auto m = getMetrics(size);

    const juce::Font font(juce::FontOptions(m.fontSize, juce::Font::bold));
    const float textWidth = juce::GlyphArrangement::getStringWidth(font, label);
    const float ledSpan = withLed ? (m.ledDiameter + layout::kIndicatorLabelGap) : 0.0f;

    // A ledless key is a compact stepper (+ / - / A / B / C) and must not inherit the wide
    // minimum meant for captioned chassis buttons.
    const float minW = withLed ? m.minWidth : (2.0f * m.paddingX + m.fontSize * 1.4f);
    const float width = std::max(minW, 2.0f * m.paddingX + ledSpan + textWidth);

    return { static_cast<int>(std::ceil(width)), static_cast<int>(std::ceil(m.height)) };
}

juce::Colour InvisButton::getLedColour() const
{
    return customLedColour.value_or(getEffectiveTheme().accentPrimary);
}

void InvisButton::setToggleState(bool shouldBeOn, juce::NotificationType notification)
{
    if (toggleState == shouldBeOn) return;

    toggleState = shouldBeOn;
    ledBallistics.setState(toggleState);

    if (notification != juce::dontSendNotification && onToggle != nullptr)
        onToggle(toggleState);

    repaint();
}

void InvisButton::setBlinking(bool shouldBlink, float rateHz)
{
    blinking = shouldBlink;
    blinkRateHz = std::max(0.2f, rateHz);

    if (!blinking)
    {
        blinkPhase = 0.0f;
        // Hand the LED back to the button's own state. Without this the ballistics keep whatever
        // the last blink edge left behind, so a blink that happened to stop on its ON half would
        // leave the lamp lit forever after the routine finished.
        ledBallistics.setState(toggleState, false);
    }

    repaint();
}

void InvisButton::tickAnimation(float deltaTimeSeconds)
{
    if (blinking)
    {
        blinkPhase += deltaTimeSeconds * blinkRateHz;
        while (blinkPhase >= 1.0f) blinkPhase -= 1.0f;

        // Square-ish duty so the blink reads as a deliberate signal, not a fade
        ledBallistics.setState(blinkPhase < 0.55f);
    }

    ledBallistics.update(deltaTimeSeconds);
    repaint();
}

void InvisButton::resized()
{
    const auto intrinsic = getIntrinsicSize();
    jassert(getWidth() == 0 || (getWidth() == intrinsic.x && getHeight() == intrinsic.y));
}

void InvisButton::mouseDown(const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
    {
        if (onSecondaryClick != nullptr)
            onSecondaryClick();
        return;
    }

    isPressed = true;
    repaint();
}

void InvisButton::mouseUp(const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) return;

    isPressed = false;

    if (!getLocalBounds().contains(e.getPosition()))
    {
        repaint();
        return;
    }

    if (toggleMode)
        setToggleState(!toggleState, juce::sendNotificationSync);
    else
        ledBallistics.setState(true); // momentary flash; released by the blink/tick logic

    if (onClick != nullptr)
        onClick();

    repaint();
}

void InvisButton::paint(juce::Graphics& g)
{
    const auto theme = getEffectiveTheme();
    const auto m = getMetrics(buttonSize);

    // Never stretch: render at the intrinsic size centred in whatever bounds we were given.
    auto content = centreIntrinsic(getLocalBounds().toFloat(), getIntrinsicSize());
    if (content.getWidth() <= 0.0f || content.getHeight() <= 0.0f) return;

    const bool lit = toggleState || blinking;
    const auto ledColour = getLedColour();
    const float lum = ledBallistics.getCurrentLuminance();

    // 1. Carved key seat: the chassis recess the key sits in
    g.setColour(juce::Colours::black.withAlpha(0.55f));
    g.fillRoundedRectangle(content.translated(0.0f, 1.0f), m.corner);

    // 2. Key cap, riding up when idle and pressed flush when held
    auto cap = isPressed ? content.translated(0.0f, 1.0f) : content;

    // A ledless key (A/B/C slot, +/- stepper) has no lamp to carry its state, so the CAP itself
    // must show it - otherwise the active compare slot is indistinguishable from the other two.
    const bool tintCap = lit && !ledVisible;

    const auto capTop = tintCap ? ledColour.withMultipliedBrightness(0.45f).withSaturation(0.55f)
                                : (lit ? juce::Colour::fromRGB(38, 46, 58) : juce::Colour::fromRGB(30, 35, 43));
    const auto capBottom = tintCap ? ledColour.withMultipliedBrightness(0.22f).withSaturation(0.60f)
                                   : (lit ? juce::Colour::fromRGB(22, 27, 35) : juce::Colour::fromRGB(17, 20, 26));

    g.setGradientFill(juce::ColourGradient(capTop, cap.getCentreX(), cap.getY(),
                                            capBottom, cap.getCentreX(), cap.getBottom(), false));
    g.fillRoundedRectangle(cap, m.corner);

    // 3. Chamfered bevel: bright top edge, dark bottom edge
    g.setColour(juce::Colours::white.withAlpha(isPressed ? 0.06f : 0.14f));
    g.drawRoundedRectangle(cap.reduced(0.5f), m.corner, 1.0f);

    // Emissive rim wash while the key is active
    if (lum > 0.02f || tintCap)
    {
        const float wash = tintCap ? 0.55f : std::min(0.42f, 0.30f * lum);
        g.setColour(ledColour.withMultipliedAlpha(wash));
        g.drawRoundedRectangle(cap.reduced(0.5f), m.corner, 1.0f);
    }

    // 4. Content row: [led][gap][caption] or [caption][gap][led], centred as one unit
    const juce::Font font(juce::FontOptions(m.fontSize, juce::Font::bold));
    const float textWidth = juce::GlyphArrangement::getStringWidth(font, labelText);
    const float ledSpan = ledVisible ? (m.ledDiameter + layout::kIndicatorLabelGap) : 0.0f;
    const float unitWidth = ledSpan + textWidth;
    const float unitLeft = cap.getCentreX() - unitWidth * 0.5f;

    juce::Point<float> ledCentre;
    juce::Rectangle<float> textArea;

    if (!ledVisible)
    {
        textArea = juce::Rectangle<float>(unitLeft, cap.getY(), textWidth, cap.getHeight());
        ledCentre = cap.getCentre();
    }
    else if (ledPosition == ButtonLedPosition::Left)
    {
        ledCentre = { unitLeft + m.ledDiameter * 0.5f, cap.getCentreY() };
        textArea = juce::Rectangle<float>(unitLeft + m.ledDiameter + layout::kIndicatorLabelGap,
                                          cap.getY(), textWidth, cap.getHeight());
    }
    else
    {
        textArea = juce::Rectangle<float>(unitLeft, cap.getY(), textWidth, cap.getHeight());
        ledCentre = { unitLeft + textWidth + layout::kIndicatorLabelGap + m.ledDiameter * 0.5f,
                      cap.getCentreY() };
    }

    if (ledVisible)
        InvisLED::drawLEDDot(g, ledCentre, m.ledDiameter * 0.5f, ledColour, lum, ledMount);

    g.setColour(tintCap ? ledColour.brighter(0.45f)
                        : (lit ? theme.textPrimary : theme.textSecondary.withAlpha(0.75f)));
    g.setFont(font);
    g.drawText(labelText, textArea, juce::Justification::centred, false);
}

} // namespace invis::ui
