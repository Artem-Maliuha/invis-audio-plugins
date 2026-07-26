#pragma once

#include "../design_system/InvisThemeSupplier.h"
#include "../design_system/InvisFonts.h"
#include "../design_system/InvisLayout.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <optional>

namespace invis::ui {

enum class InvisStepperSize {
    XS,
    S,
    M, // Standard header field (default)
    L
};

/** Absolute physical spec of an `InvisStepperSize` preset, in design pixels. */
struct StepperMetrics {
    float height;
    float stepZoneWidth;  // width of each of the - / + end zones
    float valueCharWidth; // advance per value glyph, used to size the centre well
    float fontSize;
    float glyphSize;      // the - / + marks
    float corner;
};

/**
 * Numeric Stepper Field Atom.
 *
 * ONE carved control, not a cell flanked by two loose buttons: the minus and plus zones are
 * integral end-caps of the same housing, so the whole thing reads and behaves as a single
 * parameter field rather than three widgets that happen to sit next to each other.
 *
 * Interaction:
 *   - click a end zone   -> step by one increment
 *   - drag the centre    -> scrub
 *   - wheel              -> step
 *   - DOUBLE-CLICK centre-> type the value directly
 *
 * Presentational and stateless: zero dependency on AudioProcessor / APVTS. Values leave through
 * `onValueChanged`.
 *
 * PROPORTIONALITY INTEGRITY: fixed intrinsic size per preset + digit budget; letterboxes rather
 * than stretching.
 */
class InvisStepperField : public juce::Component,
                          public InvisThemeSupplier,
                          public juce::TextEditor::Listener {
public:
    InvisStepperField();
    ~InvisStepperField() override = default;

    static StepperMetrics getMetrics(InvisStepperSize size);

    /** @param maxValueChars widest value the field must hold, e.g. 6 for "-24 dB". */
    static juce::Point<int> getIntrinsicSize(InvisStepperSize size, int maxValueChars);
    juce::Point<int> getIntrinsicSize() const { return getIntrinsicSize(stepperSize, maxValueChars); }

    void setBoundsCentredIn(juce::Rectangle<int> area)
    {
        setBounds(centreIntrinsic(area, getIntrinsicSize()));
    }

    void setStepperSize(InvisStepperSize size) { stepperSize = size; repaint(); }
    void setMaxValueChars(int chars) { maxValueChars = std::max(2, chars); repaint(); }

    // --- LIST MODE ---
    //
    // The same control with a different value source. A field with prev/next end caps and a value
    // between them is ONE control; whether that value is a number or a name is a detail, and
    // splitting it into two atoms would duplicate the chrome, the hit-testing and the metrics.
    void setItems(const juce::StringArray& newItems);
    void setSelectedIndex(int index, juce::NotificationType notification = juce::sendNotificationAsync);
    int getSelectedIndex() const { return selectedIndex; }
    bool isListMode() const { return listMode; }

    std::function<void(int)> onIndexChanged;

    /** Middle-click popup, for a value with more structure than a flat list (a preset tree). */
    std::function<void(juce::PopupMenu&)> onBuildPopup;
    std::function<void(int resultId)> onPopupResult;

    /** Free-form text shown instead of the selected item. */
    void setDisplayTextOverride(const juce::String& text) { displayOverride = text; repaint(); }

    void setRange(double newMin, double newMax, double newStep);
    void setValue(double newValue, juce::NotificationType notification = juce::sendNotificationAsync);
    double getValue() const { return value; }

    void setSuffix(const juce::String& text) { suffix = text; repaint(); }
    void setNumDecimals(int decimals) { numDecimals = std::max(0, decimals); repaint(); }

    /** Overrides the default "%.Nf<suffix>" rendering. */
    void setValueFormatter(std::function<juce::String(double)> formatter)
    {
        valueFormatter = std::move(formatter);
        repaint();
    }

    void setLabel(const juce::String& text) { labelText = text; repaint(); }

    std::function<void(double)> onValueChanged;

    InvisTheme getEffectiveTheme() const override
    {
        return customTheme.value_or(InvisThemeSupplier::getParentTheme(this));
    }

    void setThemeOverride(const InvisTheme& theme) { customTheme = theme; repaint(); }

    void paint(juce::Graphics& g) override;
    void resized() override;

    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

    void textEditorReturnKeyPressed(juce::TextEditor& editor) override;
    void textEditorFocusLost(juce::TextEditor& editor) override;
    void textEditorEscapeKeyPressed(juce::TextEditor& editor) override;

private:
    struct Layout {
        juce::Rectangle<float> content;
        juce::Rectangle<float> minusZone;
        juce::Rectangle<float> plusZone;
        juce::Rectangle<float> valueZone;
    };

    Layout computeLayout() const;

    juce::String getDisplayText() const;
    void step(int direction);
    void showPopup();
    void showInlineEditor();
    void commitInlineEditor();
    void dismissInlineEditor();

    InvisStepperSize stepperSize { InvisStepperSize::M };
    int maxValueChars { 6 };

    bool listMode { false };
    juce::StringArray items;
    int selectedIndex { 0 };
    juce::String displayOverride;

    double value { 0.0 };
    double minValue { -24.0 };
    double maxValue { 0.0 };
    double stepSize { 1.0 };
    int numDecimals { 0 };
    juce::String suffix { " dB" };
    juce::String labelText;

    bool minusPressed { false };
    bool plusPressed { false };
    bool scrubbing { false };
    double scrubStartValue { 0.0 };

    std::function<juce::String(double)> valueFormatter;
    std::optional<InvisTheme> customTheme;
    std::unique_ptr<juce::TextEditor> inlineEditor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InvisStepperField)
};

} // namespace invis::ui
