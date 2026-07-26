#pragma once

#include "../design_system/InvisThemeSupplier.h"
#include "../design_system/InvisFonts.h"
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

    /**
     * Dropdown mode: a click opens a popup list instead of stepping through items by drag/wheel.
     * Right for enumerations the user picks from (oversampling factor), wrong for values that are
     * naturally scrubbed (a dB target), so it is opt-in rather than the default.
     */
    void setPopupMode(bool shouldUsePopup) { popupMode = shouldUsePopup; }
    bool isPopupMode() const { return popupMode; }

    /**
     * Optional STRUCTURED popup. When set, popup mode shows this menu instead of the flat item
     * list, so a single cell can front a composite parameter (e.g. one OVERSAMPLING control whose
     * menu carries separate ONLINE and OFFLINE sections). The atom stays generic: it knows how to
     * open a menu and report the chosen id, and nothing about what the ids mean.
     */
    std::function<void(juce::PopupMenu&)> onBuildPopup;
    std::function<void(int resultId)> onPopupResult;

    /** Value alignment. Left reads well in a wide labelled slot; a narrow numeric cell wants
        centred, or the value hangs off one edge with dead space beside it. */
    void setValueJustification(juce::Justification j) { valueJustification = j; repaint(); }

    /** Free-form display text, for composite values that no single item can express. */
    void setDisplayTextOverride(const juce::String& text) { displayOverride = text; repaint(); }
    void clearDisplayTextOverride() { displayOverride.clear(); repaint(); }
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
    bool popupMode { false };
    juce::String displayOverride;
    juce::Justification valueJustification { juce::Justification::centredLeft };

    void showItemPopup();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InvisCellSelector)
};

// Descriptive alias: the same atom read as a labelled LED cell.
// NOTE: there is deliberately NO `InvisThumbwheel` alias here. A thumbwheel is an edge-on
// rotating wheel, not a discrete cell selector - aliasing them made two different controls look
// like one, and collided with the (dead, uncompiled) InvisThumbwheel class that used to exist.
using InvisLEDCell = InvisCellSelector;

} // namespace invis::ui
