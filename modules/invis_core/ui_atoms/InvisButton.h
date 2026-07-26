#pragma once

#include "InvisLED.h"
#include "../design_system/InvisThemeSupplier.h"
#include "../design_system/InvisLayout.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

namespace invis::ui {

enum class InvisButtonSize {
    XS, // Inline sub-control (AUTO, RESET)
    S,
    M,  // Standard chassis button (default)
    L
};

/** Which side of the caption the status LED sits on. */
enum class ButtonLedPosition {
    Left,
    Right
};

/** Absolute physical spec of an `InvisButtonSize` preset, in design pixels. */
struct ButtonMetrics {
    float height;
    float minWidth;
    float paddingX;    // Chassis edge -> content
    float ledDiameter;
    float fontSize;
    float corner;
};

/**
 * Small Chassis Button Atom: a carved key carrying a status LED and a caption.
 *
 * Presentational and stateless - zero dependency on AudioProcessor / APVTS. State changes leave
 * through `onClick` / `onToggle`.
 *
 * The LED sits on either side of the caption (`ButtonLedPosition`), takes its colour from the
 * active theme by default, and supports a blink mode for "operation in progress" (used by the
 * input sidebar's AUTO calibration).
 *
 * PROPORTIONALITY INTEGRITY: fixed height and metrics per preset; it never stretches vertically.
 * NOTE: unlike the other atoms, the intrinsic WIDTH depends on the caption, because a button
 * must fit its own text. Re-layout the parent after changing the label.
 */
class InvisButton : public juce::Component, public InvisThemeSupplier {
public:
    InvisButton();
    ~InvisButton() override = default;

    static ButtonMetrics getMetrics(InvisButtonSize size);
    static juce::Point<int> getIntrinsicSize(InvisButtonSize size, const juce::String& label,
                                             bool withLed = true);
    juce::Point<int> getIntrinsicSize() const { return getIntrinsicSize(buttonSize, labelText, ledVisible); }

    void setBoundsCentredIn(juce::Rectangle<int> area)
    {
        setBounds(centreIntrinsic(area, getIntrinsicSize()));
    }

    void setButtonSize(InvisButtonSize size) { buttonSize = size; repaint(); }
    InvisButtonSize getButtonSize() const { return buttonSize; }

    void setLabel(const juce::String& text) { labelText = text; repaint(); }
    juce::String getLabel() const { return labelText; }

    void setLedPosition(ButtonLedPosition pos) { ledPosition = pos; repaint(); }
    ButtonLedPosition getLedPosition() const { return ledPosition; }

    /** Explicit LED colour. Without one the LED follows the theme accent. */
    void setLedColour(juce::Colour colour) { customLedColour = colour; repaint(); }

    /**
     * A key that DESTROYS something.
     *
     * Not a colour setter: the point is not "make this red", it is "this one is not like the
     * others". A destructive key that looks exactly like its neighbours is a key you will press by
     * reflex, so it wears the warning at rest - not on hover, not once pressed - and the caption
     * carries it too, because the rim alone reads as decoration.
     */
    void setDangerous(bool shouldWarn) { dangerous = shouldWarn; repaint(); }
    bool isDangerous() const { return dangerous; }
    void clearLedColour() { customLedColour.reset(); repaint(); }

    void setLedMountType(LEDMountType type) { ledMount = type; repaint(); }

    /** Hide the LED entirely, leaving a plain caption key (steppers, +/- , A/B/C slots). */
    void setLedVisible(bool shouldBeVisible) { ledVisible = shouldBeVisible; repaint(); }
    bool isLedVisible() const { return ledVisible; }

    /** Latching (toggle) vs momentary. Toggle is the default. */
    void setToggleMode(bool shouldToggle) { toggleMode = shouldToggle; }
    bool isToggleMode() const { return toggleMode; }

    void setToggleState(bool shouldBeOn, juce::NotificationType notification = juce::sendNotificationAsync);
    bool getToggleState() const { return toggleState; }

    /** Blink the LED to signal an operation in progress. Does not alter the toggle state. */
    void setBlinking(bool shouldBlink, float rateHz = 2.6f);
    bool isBlinking() const { return blinking; }

    /** Advance LED ballistics / blink phase. Drive from the editor's timer. */
    void tickAnimation(float deltaTimeSeconds = 0.016f);

    std::function<void()> onClick;
    std::function<void(bool)> onToggle;

    /** Right-click / ctrl-click. Used for per-control context menus (e.g. compare-slot copy). */
    std::function<void()> onSecondaryClick;

    InvisTheme getEffectiveTheme() const override
    {
        return customTheme.value_or(InvisThemeSupplier::getParentTheme(this));
    }

    void setThemeOverride(const InvisTheme& theme) { customTheme = theme; repaint(); }
    void clearThemeOverride() { customTheme.reset(); repaint(); }

    void paint(juce::Graphics& g) override;
    void resized() override;

    void mouseDown(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;

private:
    InvisButtonSize buttonSize { InvisButtonSize::M };
    ButtonLedPosition ledPosition { ButtonLedPosition::Left };
    LEDMountType ledMount { LEDMountType::RecessedSlot };

    juce::String labelText { "BUTTON" };
    bool toggleMode { true };
    bool toggleState { false };
    bool isPressed { false };

    bool ledVisible { true };
    bool dangerous { false };
    bool blinking { false };
    float blinkRateHz { 2.6f };
    float blinkPhase { 0.0f };

    LEDBallistics ledBallistics;

    std::optional<juce::Colour> customLedColour;
    std::optional<InvisTheme> customTheme;

    juce::Colour getLedColour() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InvisButton)
};

} // namespace invis::ui
