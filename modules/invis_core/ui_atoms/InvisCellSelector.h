#pragma once

#include "../design_system/InvisThemeSupplier.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace invis::ui {

/**
 * Stateless Presentational LED Parameter Cell Selector Atom.
 * Compact carved technical slot with LED ignition flash, square technical typography, and emissive phosphor glow.
 */
class InvisCellSelector : public juce::Component, public InvisThemeSupplier {
public:
    InvisCellSelector();
    ~InvisCellSelector() override = default;

    void setItems(const std::vector<juce::String>& newItems);
    const std::vector<juce::String>& getItems() const { return items; }

    void setSelectedIndex(int index, juce::NotificationType notification = juce::sendNotificationAsync);
    int getSelectedIndex() const { return selectedIndex; }
    juce::String getSelectedItemText() const;

    void setLabel(const juce::String& text) { labelText = text; repaint(); }
    juce::String getLabel() const { return labelText; }

    InvisTheme getEffectiveTheme() const override
    {
        return customTheme.value_or(InvisTheme::getGlobalDefault());
    }

    void setThemeOverride(const InvisTheme& theme) { customTheme = theme; repaint(); }
    void clearThemeOverride() { customTheme.reset(); repaint(); }

    std::function<void(int newIndex, const juce::String& itemText)> onIndexChanged;

    void paint(juce::Graphics& g) override;

    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

private:
    std::vector<juce::String> items { "dBFS", "dBU", "VU", "PPM" };
    int selectedIndex { 0 };
    int previousIndex { 0 };

    juce::String labelText;
    std::optional<InvisTheme> customTheme;

    float animProgress { 1.0f }; // 0.0 = start transition, 1.0 = completed
    float ledLuminance { 1.0f }; // 1.6 = ignition flash spike, 1.0 = steady lit state

    bool isPressed { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InvisCellSelector)
};

// Convenient alias for backward compatibility / descriptive UI usage
using InvisLEDCell = InvisCellSelector;
using InvisThumbwheel = InvisCellSelector;

} // namespace invis::ui
