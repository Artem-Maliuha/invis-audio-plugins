#pragma once

#include "InvisLED.h"
#include "../design_system/InvisThemeSupplier.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <optional>
#include <vector>

namespace invis::ui {

enum class OffPosition {
    None,
    Start, // Dedicated OFF position at start of scale
    End    // Dedicated OFF position at end of scale
};

enum class KnobScaleCurve {
    Linear,
    Logarithmic,
    InverseLogarithmic
};

enum class InvisKnobSize {
    XS, // Extra Small (Compact sub-controls)
    S,  // Small (Filter / Secondary knobs)
    M,  // Medium (Standard default control)
    L,  // Large (Main Gain / Frequency knobs)
    XL  // Extra Large (Master / Primary focal knob)
};

enum class ValueArcOrigin {
    Start,   // Value arc fills from 0.0f (Start angle) clockwise to current value (Gain, Volume, HPF)
    Center,  // Value arc fills from 0.5f (Center angle) bi-directionally (Pan, EQ Boost/Cut, Balance)
    End      // Value arc fills from 1.0f (End angle) counter-clockwise down to current value (LPF)
};

struct ScaleTick {
    float normalizedPosition { 0.0f }; // 0.0f .. 1.0f
    juce::String label;
    bool isOff { false };
};

struct StickyPoint {
    float normalizedPosition { 0.5f }; // 0.0f .. 1.0f
    float snapTolerance { 0.010f };    // Subtle magnetic capture zone
};

/**
 * Stateless Presentational Vector Knob Atom.
 * Pure visual & interaction control. Zero dependency on AudioProcessor / APVTS.
 */
class InvisKnob : public juce::Component, public juce::TextEditor::Listener {
public:
    InvisKnob();
    ~InvisKnob() override = default;

    // Value management [0.0f .. 1.0f]
    void setValue(float normalizedValue, juce::NotificationType notification = juce::sendNotificationAsync);
    float getValue() const { return currentValue; }

    // Sticky / Magnetic Snap Points (Glue detents)
    void setStickyPoints(const std::vector<StickyPoint>& points) { stickyPoints = points; }
    void setStickyPositions(const std::vector<float>& positions, float defaultTolerance = 0.010f);
    void addStickyPoint(float normalizedPosition, float tolerance = 0.010f);
    void clearStickyPoints() { stickyPoints.clear(); }
    const std::vector<StickyPoint>& getStickyPoints() const { return stickyPoints; }

    // OFF state explicit getter / setter
    void setOff(bool shouldBeOff, juce::NotificationType notification = juce::sendNotificationAsync);
    bool isOff() const { return isOffState; }

    // Scale Curve (Linear, Logarithmic, InverseLogarithmic)
    void setScaleCurve(KnobScaleCurve curve) { scaleCurve = curve; repaint(); }
    KnobScaleCurve getScaleCurve() const { return scaleCurve; }

    // Value Arc Origin (Start, Center [Bipolar Pan/EQ], End, or Custom Normalized Position)
    void setValueArcOrigin(ValueArcOrigin origin) { valueArcOrigin = origin; customArcOriginPosition.reset(); repaint(); }
    void setValueArcOriginPosition(float normalizedPosition) { customArcOriginPosition = std::clamp(normalizedPosition, 0.0f, 1.0f); repaint(); }
    ValueArcOrigin getValueArcOrigin() const { return valueArcOrigin; }

    // Optional Angle Range in Degrees (Default: 220 to 500 degrees)
    void setAngleRange(float startDegrees, float endDegrees) {
        startAngleDegrees = startDegrees;
        endAngleDegrees = endDegrees;
        repaint();
    }
    float getStartAngleDegrees() const { return startAngleDegrees; }
    float getEndAngleDegrees() const { return endAngleDegrees; }

    // Configuration for OFF position & Default value
    void setOffPosition(OffPosition position) { offPosition = position; repaint(); }
    OffPosition getOffPosition() const { return offPosition; }

    void setDefaultValue(float normalizedDefault) { defaultValue = juce::jlimit(0.0f, 1.0f, normalizedDefault); }
    float getDefaultValue() const { return defaultValue; }

    // Scale Ticks & Labels around knob arc
    void setScaleTicks(const std::vector<ScaleTick>& ticks) { scaleTicks = ticks; repaint(); }
    void setScaleLabels(const juce::StringArray& labels); // Auto-distributes labels along scale

    // Text formatting & parsing helpers
    void setLabel(const juce::String& newLabel) { labelText = newLabel; repaint(); }
    void setValueText(const juce::String& text) { valueText = text; repaint(); }

    void setValueFormatter(std::function<juce::String(float normValue)> formatter) { valueFormatter = std::move(formatter); repaint(); }
    void setValueParser(std::function<float(const juce::String& text)> parser) { valueParser = std::move(parser); }

    // Callbacks for container / DAW automation gestures
    std::function<void(float)> onValueChanged;
    std::function<void()> onDragStarted;
    std::function<void()> onDragEnded;

    // Pointer LED Color Override
    void setPointerLedColor(juce::Colour color) { customPointerColor = color; repaint(); }
    void clearPointerLedColor() { customPointerColor.reset(); repaint(); }

    // Optional 3D Knob Cap Asset
    void setKnobCapImage(const juce::Image& image) { knobCapImage = image; repaint(); }
    void clearKnobCapImage() { knobCapImage = juce::Image(); repaint(); }

    // 3D Filmstrip Engine (Pre-rendered 3D Knob Frame Sequence)
    void setFilmstrip(const juce::Image& spriteSheetImage, int numFrames, bool isVertical = true)
    {
        filmstripImage = spriteSheetImage;
        filmstripFrames = std::max(1, numFrames);
        filmstripIsVertical = isVertical;
        repaint();
    }
    void clearFilmstrip()
    {
        filmstripImage = juce::Image();
        filmstripFrames = 0;
        repaint();
    }
    bool hasFilmstrip() const { return filmstripImage.isValid() && filmstripFrames > 0; }

    // Knob Size Preset (XS, S, M, L, XL - Default: M)
    void setKnobSize(InvisKnobSize size) { knobSize = size; repaint(); }
    InvisKnobSize getKnobSize() const { return knobSize; }

    // Theme Overrides
    void setThemeOverride(const InvisTheme& theme) { customTheme = theme; repaint(); }
    void clearThemeOverride() { customTheme.reset(); repaint(); }
    InvisTheme getTheme() const;

    // JUCE Component Overrides
    void paint(juce::Graphics& g) override;
    void resized() override;

    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

    // TextEditor Listener Overrides for inline value editing
    void textEditorReturnKeyPressed(juce::TextEditor& editor) override;
    void textEditorFocusLost(juce::TextEditor& editor) override;
    void textEditorEscapeKeyPressed(juce::TextEditor& editor) override;

private:
    float currentValue { 0.0f };
    float defaultValue { 0.0f };
    bool isOffState { false };

    OffPosition offPosition { OffPosition::None };
    KnobScaleCurve scaleCurve { KnobScaleCurve::Linear };
    InvisKnobSize knobSize { InvisKnobSize::M };
    ValueArcOrigin valueArcOrigin { ValueArcOrigin::Start };
    std::optional<float> customArcOriginPosition;

    float startAngleDegrees { 220.0f };
    float endAngleDegrees   { 500.0f };

    juce::String labelText;
    juce::String valueText;

    std::vector<ScaleTick> scaleTicks;
    std::vector<StickyPoint> stickyPoints;
    LEDBallistics pointerLedBallistics;
    std::function<juce::String(float)> valueFormatter;
    std::function<float(const juce::String&)> valueParser;

    std::optional<InvisTheme> customTheme;
    std::optional<juce::Colour> customPointerColor;
    juce::Image knobCapImage;

    juce::Image filmstripImage;
    int filmstripFrames { 0 };
    bool filmstripIsVertical { true };

    juce::Point<float> lastMousePos;
    bool isDragging { false };
    float rawDragValue { 0.0f };

    juce::Rectangle<float> valueTextArea;
    std::unique_ptr<juce::TextEditor> inlineEditor;

    float applyCurve(float input) const;
    float removeCurve(float input) const;

    juce::String getEffectiveValueText() const;
    void showInlineEditor();
    void commitInlineEditorValue();
    void dismissInlineEditor();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InvisKnob)
};

} // namespace invis::ui
