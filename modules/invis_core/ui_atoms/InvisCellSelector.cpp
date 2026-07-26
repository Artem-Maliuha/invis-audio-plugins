#include "InvisCellSelector.h"

namespace invis::ui {

InvisCellSelector::InvisCellSelector()
{
    setOpaque(false);
}

void InvisCellSelector::setItems(const std::vector<juce::String>& newItems)
{
    items = newItems;
    if (items.empty()) items = { "Option" };
    selectedIndex = juce::jlimit(0, static_cast<int>(items.size()) - 1, selectedIndex);
    previousIndex = selectedIndex;
    animProgress = 1.0f;
    repaint();
}

void InvisCellSelector::setSelectedIndex(int index, juce::NotificationType notification)
{
    if (items.empty()) return;

    const int newIdx = juce::jlimit(0, static_cast<int>(items.size()) - 1, index);
    if (selectedIndex != newIdx)
    {
        previousIndex = selectedIndex;
        selectedIndex = newIdx;

        animProgress = 0.0f;     // Start text slide cross-fade
        ledLuminance = 1.8f;     // Trigger physical text phosphor ignition flash spike

        repaint();

        if (notification != juce::dontSendNotification && onIndexChanged)
        {
            onIndexChanged(selectedIndex, getSelectedItemText());
        }
    }
}

juce::String InvisCellSelector::getSelectedItemText() const
{
    if (displayOverride.isNotEmpty())
        return displayOverride;

    if (selectedIndex >= 0 && selectedIndex < static_cast<int>(items.size()))
    {
        return items[static_cast<size_t>(selectedIndex)];
    }
    return {};
}

void InvisCellSelector::showItemPopup()
{
    juce::PopupMenu menu;
    const bool structured = (onBuildPopup != nullptr);

    if (structured)
        onBuildPopup(menu);
    else
        for (size_t i = 0; i < items.size(); ++i)
            menu.addItem(static_cast<int>(i) + 1, items[i], true, static_cast<int>(i) == selectedIndex);

    // The chassis's own dropdown, everywhere a dropdown appears. A cell that opened the system
    // list while the chart's menus wore the panel's look was two different products in one window.
    if (popupLook != nullptr) menu.setLookAndFeel(popupLook);

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this)
                                                 .withMinimumWidth(getWidth())
                                                 .withStandardItemHeight(19),
                       [this, structured](int result) {
                           isPressed = false;

                           if (result > 0)
                           {
                               if (structured)
                               {
                                   if (onPopupResult != nullptr) onPopupResult(result);
                               }
                               else
                               {
                                   setSelectedIndex(result - 1, juce::sendNotificationSync);
                               }
                           }

                           repaint();
                       });
}

void InvisCellSelector::mouseDown(const juce::MouseEvent&)
{
    isPressed = true;
    repaint();

    if (popupMode)
        showItemPopup();
}

void InvisCellSelector::mouseDrag(const juce::MouseEvent&)
{
    // Discrete click selector
}

void InvisCellSelector::mouseUp(const juce::MouseEvent& e)
{
    if (popupMode) return; // the popup callback owns the selection

    if (isPressed && getLocalBounds().contains(e.getPosition()) && !items.empty())
    {
        const int nextIdx = (selectedIndex + 1) % static_cast<int>(items.size());
        setSelectedIndex(nextIdx, juce::sendNotificationAsync);
    }

    isPressed = false;
    repaint();
}

void InvisCellSelector::mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    if (popupMode || wheel.deltaY == 0.0f || items.empty()) return;

    const int direction = (wheel.deltaY > 0.0f) ? 1 : -1;
    int nextIdx = selectedIndex + direction;
    if (nextIdx < 0) nextIdx = static_cast<int>(items.size()) - 1;
    if (nextIdx >= static_cast<int>(items.size())) nextIdx = 0;

    setSelectedIndex(nextIdx, juce::sendNotificationAsync);
}

void InvisCellSelector::paint(juce::Graphics& g)
{
    // Advance continuous animation frames
    bool isAnimating = false;
    if (animProgress < 1.0f)
    {
        animProgress = std::min(1.0f, animProgress + 0.016f * 14.0f); // ~70ms slick slide
        isAnimating = true;
    }
    if (ledLuminance > 1.0f)
    {
        ledLuminance = std::max(1.0f, ledLuminance - 0.016f * 20.0f); // ~35ms ignition decay
        isAnimating = true;
    }

    if (isAnimating)
    {
        juce::MessageManager::callAsync([this]() { repaint(); });
    }

    const auto theme = getEffectiveTheme();
    const auto cellRect = getLocalBounds().toFloat().reduced(1.0f);

    // 1. Carved Technical Metal Slot Housing Body
    const auto cellBgColor = isPressed
        ? juce::Colour::fromRGB(18, 22, 28)
        : juce::Colour::fromRGB(10, 13, 18);

    g.setColour(cellBgColor);
    g.fillRoundedRectangle(cellRect, 4.0f);

    // Chamfer Bevel Highlight Edge
    g.setColour(juce::Colours::white.withAlpha(0.14f));
    g.drawRoundedRectangle(cellRect, 4.0f, 1.0f);

    // Deep Interior Slot Drop Shadow
    g.setColour(juce::Colours::black.withAlpha(0.50f));
    g.drawRoundedRectangle(cellRect.reduced(0.5f), 3.5f, 0.75f);

    const auto innerPadding = cellRect.reduced(5.0f, 3.0f);

    // 2. Render Integrated Label INSIDE the cell (Top area)
    const float labelH = innerPadding.getHeight() * 0.38f;
    if (labelText.isNotEmpty())
    {
        const auto labelRect = juce::Rectangle<float>(innerPadding.getX(), innerPadding.getY(), innerPadding.getWidth(), labelH);
        g.setColour(theme.textSecondary.withAlpha(0.60f));
        g.setFont(juce::FontOptions("Courier New", std::clamp(labelH * 0.85f, 7.5f, 9.5f), juce::Font::bold));
        g.drawText(labelText, labelRect, juce::Justification::centredLeft, true);
    }

    // 3. Render Active Parameter Value INSIDE the cell with HIGH-INTENSITY EMISSIVE PHOSPHOR GLOW
    const float valueY = labelText.isNotEmpty() ? (innerPadding.getY() + labelH + 1.0f) : innerPadding.getY();
    const float valueH = innerPadding.getBottom() - valueY;
    const auto valueBounds = juce::Rectangle<float>(innerPadding.getX(), valueY, innerPadding.getWidth(), valueH);

    // Segment-display typeface for the value. Uppercase only, which is what the face supports
    // and what a hardware readout does anyway.
    const float fontH = std::clamp(valueH * 0.80f, 8.5f, 13.0f);
    g.setFont(InvisFonts::getDisplayFont(fontH));

    // Cubic ease-out transition curve
    const float t = std::sin(animProgress * juce::MathConstants<float>::halfPi);
    const float slideOffset = (1.0f - t) * (valueH * 0.60f);

    const juce::Colour accent = theme.accentPrimary;

    // Old item sliding up & fading out
    if (animProgress < 1.0f && previousIndex >= 0 && previousIndex < static_cast<int>(items.size()))
    {
        g.setColour(theme.textSecondary.withAlpha(0.65f * (1.0f - t)));
        g.drawText(items[static_cast<size_t>(previousIndex)].toUpperCase(),
                   valueBounds.translated(0.0f, -slideOffset), valueJustification, false);
    }

    // New item sliding up from below, with the phosphor radiance the rest of the chassis uses
    const float newAlpha = (animProgress < 1.0f) ? t : 1.0f;
    const juce::String textToDraw = getSelectedItemText().toUpperCase();

    const float haloAlpha = std::min(0.95f, 0.45f * newAlpha * ledLuminance);
    g.setColour(accent.withAlpha(haloAlpha));
    g.drawText(textToDraw, valueBounds.translated(-0.75f, -0.75f + slideOffset * 0.5f), valueJustification, false);
    g.drawText(textToDraw, valueBounds.translated(0.75f, 0.75f + slideOffset * 0.5f), valueJustification, false);

    g.setColour(accent.brighter(0.20f).withAlpha(newAlpha));
    g.drawText(textToDraw, valueBounds.translated(0.0f, slideOffset * 0.5f), valueJustification, false);
}

} // namespace invis::ui
