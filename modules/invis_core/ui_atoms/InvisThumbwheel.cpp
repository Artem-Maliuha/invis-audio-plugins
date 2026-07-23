#include "InvisThumbwheel.h"

namespace invis::ui {

InvisThumbwheel::InvisThumbwheel()
{
    setOpaque(false);
}

void InvisThumbwheel::setItems(const std::vector<juce::String>& newItems)
{
    items = newItems;
    if (items.empty()) items = { "Option" };
    selectedIndex = juce::jlimit(0, static_cast<int>(items.size()) - 1, selectedIndex);
    previousIndex = selectedIndex;
    animProgress = 1.0f;
    repaint();
}

void InvisThumbwheel::setSelectedIndex(int index, juce::NotificationType notification)
{
    if (items.empty()) return;

    const int newIdx = juce::jlimit(0, static_cast<int>(items.size()) - 1, index);
    if (selectedIndex != newIdx)
    {
        previousIndex = selectedIndex;
        selectedIndex = newIdx;

        animProgress = 0.0f;     // Start text slide cross-fade
        ledLuminance = 1.6f;     // Trigger physical LED ignition flash spike

        repaint();

        if (notification != juce::dontSendNotification && onIndexChanged)
        {
            onIndexChanged(selectedIndex, getSelectedItemText());
        }
    }
}

juce::String InvisThumbwheel::getSelectedItemText() const
{
    if (selectedIndex >= 0 && selectedIndex < static_cast<int>(items.size()))
    {
        return items[static_cast<size_t>(selectedIndex)];
    }
    return {};
}

void InvisThumbwheel::mouseDown(const juce::MouseEvent&)
{
    isPressed = true;
    repaint();
}

void InvisThumbwheel::mouseDrag(const juce::MouseEvent&)
{
    // No swipes or drags allowed
}

void InvisThumbwheel::mouseUp(const juce::MouseEvent& e)
{
    if (isPressed && getLocalBounds().contains(e.getPosition()) && !items.empty())
    {
        const int nextIdx = (selectedIndex + 1) % static_cast<int>(items.size());
        setSelectedIndex(nextIdx, juce::sendNotificationAsync);
    }

    isPressed = false;
    repaint();
}

void InvisThumbwheel::mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    if (wheel.deltaY == 0.0f || items.empty()) return;

    const int direction = (wheel.deltaY > 0.0f) ? 1 : -1;
    int nextIdx = selectedIndex + direction;
    if (nextIdx < 0) nextIdx = static_cast<int>(items.size()) - 1;
    if (nextIdx >= static_cast<int>(items.size())) nextIdx = 0;

    setSelectedIndex(nextIdx, juce::sendNotificationAsync);
}

void InvisThumbwheel::paint(juce::Graphics& g)
{
    // Advance animations
    bool isAnimating = false;
    if (animProgress < 1.0f)
    {
        animProgress = std::min(1.0f, animProgress + 0.016f * 12.0f); // ~80ms fast slide
        isAnimating = true;
    }
    if (ledLuminance > 1.0f)
    {
        ledLuminance = std::max(1.0f, ledLuminance - 0.016f * 20.0f); // ~30ms ignition decay
        isAnimating = true;
    }

    if (isAnimating)
    {
        juce::MessageManager::callAsync([this]() { repaint(); });
    }

    const auto theme = getEffectiveTheme();
    auto bounds = getLocalBounds().toFloat();

    // 1. Draw Optional Label
    if (labelText.isNotEmpty())
    {
        const float labelH = std::min(14.0f, bounds.getHeight() * 0.28f);
        g.setColour(theme.textSecondary.withAlpha(0.65f));
        g.setFont(juce::FontOptions(std::max(8.5f, labelH * 0.75f), juce::Font::bold));
        g.drawText(labelText, bounds.removeFromTop(labelH), juce::Justification::centred, true);
    }

    // 2. Recessed LED-Lit Parameter Cell Socket
    const auto cellRect = bounds.reduced(1.0f);

    // Carved Metal Housing Cell Background
    const auto cellBgColor = isPressed
        ? juce::Colour::fromRGB(18, 22, 28)
        : juce::Colour::fromRGB(12, 15, 20);

    g.setColour(cellBgColor);
    g.fillRoundedRectangle(cellRect, 3.5f);

    // Chamfer Bevel Ring
    g.setColour(juce::Colours::white.withAlpha(0.12f));
    g.drawRoundedRectangle(cellRect, 3.5f, 1.0f);

    // Drop shadow inside slot
    g.setColour(juce::Colours::black.withAlpha(0.40f));
    g.drawRoundedRectangle(cellRect.reduced(0.5f), 3.0f, 0.75f);

    // 3. Integrated LED Status Indicator (Left Edge Slot)
    const float ledR = std::clamp(cellRect.getHeight() * 0.20f, 2.5f, 4.0f);
    const float ledX = cellRect.getX() + std::max(6.0f, cellRect.getHeight() * 0.25f);
    const float ledY = cellRect.getCentreY();
    const auto ledCenter = juce::Point<float>(ledX, ledY);

    const juce::Colour accent = theme.accentPrimary;

    // Volumetric Soft Light Aura onto surrounding cell faceplate
    const float auraR = ledR * 3.5f;
    const auto auraGrad = juce::ColourGradient(
        accent.withAlpha(std::min(0.85f, 0.45f * ledLuminance)), ledX, ledY,
        juce::Colours::transparentBlack, ledX + auraR, ledY,
        true
    );
    g.setGradientFill(auraGrad);
    g.fillEllipse(juce::Rectangle<float>(auraR * 2.0f, auraR * 2.0f).withCentre(ledCenter));

    // Lit LED Translucent Dome Body
    g.setColour(accent.brighter(0.20f));
    g.fillEllipse(juce::Rectangle<float>(ledR * 2.0f, ledR * 2.0f).withCentre(ledCenter));

    // High-Intensity Phosphor Core Point
    g.setColour(juce::Colours::white.withAlpha(0.90f));
    g.fillEllipse(juce::Rectangle<float>(ledR * 0.90f, ledR * 0.90f).withCentre(ledCenter));

    // 4. Small Crisp Technical Font with Animated Parameter Slide Transition
    const float fontH = std::clamp(cellRect.getHeight() * 0.44f, 8.5f, 10.5f);
    g.setFont(juce::FontOptions(fontH, juce::Font::bold));

    const float textLeft = ledX + ledR + 5.0f;
    const auto textBounds = juce::Rectangle<float>(textLeft, cellRect.getY(), cellRect.getRight() - textLeft - 4.0f, cellRect.getHeight());

    // Easing curve for slide transition (smooth cubic ease-out)
    const float t = std::sin(animProgress * juce::MathConstants<float>::halfPi);
    const float slideOffset = (1.0f - t) * (cellRect.getHeight() * 0.50f);

    // Old item sliding up & fading out
    if (animProgress < 1.0f && previousIndex >= 0 && previousIndex < static_cast<int>(items.size()))
    {
        g.setColour(theme.textSecondary.withAlpha(0.70f * (1.0f - t)));
        g.drawText(items[static_cast<size_t>(previousIndex)], textBounds.translated(0.0f, -slideOffset), juce::Justification::centredLeft, true);
    }

    // New item sliding up from below & fading in
    const float newAlpha = (animProgress < 1.0f) ? t : 1.0f;
    g.setColour(theme.accentPrimary.withAlpha(newAlpha));
    g.drawText(getSelectedItemText(), textBounds.translated(0.0f, slideOffset * 0.5f), juce::Justification::centredLeft, true);
}

} // namespace invis::ui
