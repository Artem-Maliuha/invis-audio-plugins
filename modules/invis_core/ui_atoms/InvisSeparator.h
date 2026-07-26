#pragma once

#include "../design_system/InvisThemeSupplier.h"
#include "../design_system/InvisLayout.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace invis::ui {

enum class InvisSeparatorSize {
    S,  // inside a dense group
    M,  // between related controls (default)
    L   // between sections that are genuinely different concerns
};

enum class InvisSeparatorOrientation {
    Horizontal,
    Vertical
};

/**
 * Absolute physical spec of an `InvisSeparatorSize` preset, in design pixels.
 *
 * `margin` is the clearance the separator reserves on BOTH sides of its line. It is part of the
 * atom, not of the container: a divider whose spacing is negotiated at each call site ends up
 * with different air around it in every panel, which is exactly the drift this design system
 * exists to prevent.
 */
struct SeparatorMetrics {
    float margin;     // clearance above and below the line
    float inset;      // pull-in from the container's edges
    float thickness;
};

/**
 * Engraved Divider Atom.
 *
 * A line with spacing rules. Rendered as a cut in the chassis - a dark groove with a light
 * chamfer catching the light below it - rather than a flat stroke, matching the engraved seams
 * used elsewhere on the panel.
 *
 * PROPORTIONALITY INTEGRITY: the thickness across the line is fixed by the preset. It stretches
 * only ALONG its length, which is the one axis where stretching is meaningful for a line.
 */
class InvisSeparator : public juce::Component, public InvisThemeSupplier {
public:
    InvisSeparator() { setInterceptsMouseClicks(false, false); }
    ~InvisSeparator() override = default;

    static SeparatorMetrics getMetrics(InvisSeparatorSize size)
    {
        //        margin  inset  thickness
        switch (size)
        {
            case InvisSeparatorSize::S: return { 4.0f,  8.0f, 1.0f };
            case InvisSeparatorSize::L: return { 12.0f, 2.0f, 1.0f };
            case InvisSeparatorSize::M:
            default:                    return { 8.0f,  5.0f, 1.0f };
        }
    }

    /** Total space the separator occupies across its short axis, margins included. */
    static int getIntrinsicThickness(InvisSeparatorSize size)
    {
        const auto m = getMetrics(size);
        return static_cast<int>(std::ceil(2.0f * m.margin + m.thickness + 1.0f)); // +1 for the chamfer
    }

    int getIntrinsicThickness() const { return getIntrinsicThickness(separatorSize); }

    /** Places the separator across `area`, consuming exactly its intrinsic thickness. */
    void setBoundsInStrip(juce::Rectangle<int> area)
    {
        setBounds(orientation == InvisSeparatorOrientation::Horizontal
                    ? area.withHeight(getIntrinsicThickness()).withY(area.getY())
                    : area.withWidth(getIntrinsicThickness()).withX(area.getX()));
    }

    void setSeparatorSize(InvisSeparatorSize size) { separatorSize = size; repaint(); }
    InvisSeparatorSize getSeparatorSize() const { return separatorSize; }

    void setOrientation(InvisSeparatorOrientation o) { orientation = o; repaint(); }

    /** Optional accent tint, for a divider that also marks a signal-flow boundary. */
    void setTint(juce::Colour c) { tint = c; repaint(); }
    void clearTint() { tint.reset(); repaint(); }

    InvisTheme getEffectiveTheme() const override
    {
        return customTheme.value_or(InvisThemeSupplier::getParentTheme(this));
    }

    void setThemeOverride(const InvisTheme& theme) { customTheme = theme; repaint(); }

    void paint(juce::Graphics& g) override
    {
        const auto m = getMetrics(separatorSize);
        const auto bounds = getLocalBounds().toFloat();

        const auto groove = tint.has_value() ? tint->withAlpha(0.55f)
                                             : juce::Colours::black.withAlpha(0.55f);
        const auto chamfer = tint.has_value() ? tint->withAlpha(0.22f)
                                              : juce::Colours::white.withAlpha(0.08f);

        if (orientation == InvisSeparatorOrientation::Horizontal)
        {
            const float y = bounds.getCentreY();
            const float x1 = bounds.getX() + m.inset;
            const float x2 = bounds.getRight() - m.inset;

            g.setColour(groove);
            g.fillRect(x1, y - m.thickness * 0.5f, x2 - x1, m.thickness);
            g.setColour(chamfer);
            g.fillRect(x1, y + m.thickness * 0.5f, x2 - x1, 1.0f);
        }
        else
        {
            const float x = bounds.getCentreX();
            const float y1 = bounds.getY() + m.inset;
            const float y2 = bounds.getBottom() - m.inset;

            g.setColour(groove);
            g.fillRect(x - m.thickness * 0.5f, y1, m.thickness, y2 - y1);
            g.setColour(chamfer);
            g.fillRect(x + m.thickness * 0.5f, y1, 1.0f, y2 - y1);
        }
    }

private:
    InvisSeparatorSize separatorSize { InvisSeparatorSize::M };
    InvisSeparatorOrientation orientation { InvisSeparatorOrientation::Horizontal };
    std::optional<juce::Colour> tint;
    std::optional<InvisTheme> customTheme;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InvisSeparator)
};

} // namespace invis::ui
