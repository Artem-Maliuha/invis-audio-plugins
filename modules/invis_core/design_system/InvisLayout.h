#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace invis::ui {

/**
 * INVIS FIXED DESIGN GRID & GLOBAL ZOOM SCALING STANDARD
 * ======================================================
 *
 * Every Invis plugin editor is authored ONCE at a fixed design resolution, measured in
 * "design pixels" (dp). Resizing the plugin window is a pure zoom: a single
 * `AffineTransform::scale(k)` applied to one root canvas component.
 *
 * Consequences (these are the rules, not suggestions):
 *   1. ALL layout code inside the canvas is written in absolute design pixels.
 *      Percentage-of-parent layout (`bounds.getHeight() * 0.155f`) is FORBIDDEN — mixing
 *      absolute paint constants with relative layout is exactly what makes proportions drift.
 *   2. Font sizes, stroke widths, LED radii and paddings are absolute constants too. They are
 *      correct by construction because the transform scales them along with everything else.
 *   3. UI atoms own their size. An atom exposes `getIntrinsicSize()` and refuses to deform:
 *      if a caller hands it the wrong bounds it renders centred at its intrinsic size
 *      (letterbox) and fires a `jassert` in Debug. See `centreIntrinsic`.
 */
namespace layout {

// Raw spacing scale (design pixels), for CHROME - panel padding, insets, row gutters, anywhere
// the question is "how much air", not "how are these two things related".
inline constexpr int kGapXS = 4;
inline constexpr int kGapS  = 6;
inline constexpr int kGapM  = 10;
inline constexpr int kGapL  = 16;
inline constexpr int kGapXL = 24;

/**
 * SEMANTIC SPACING - the scale to use between CONTROLS.
 * =====================================================
 *
 * Pick by RELATIONSHIP, never by how it happens to look in the one panel you are staring at.
 * Spacing is the only thing telling the user which controls belong together, so choosing it by
 * eye per panel produces a chassis where the same relationship reads differently in every strip.
 *
 *   kGapBonded   a control and its own satellite - a knob and the cell that configures it.
 *                They are one control wearing two parts.
 *   kGapRelated  two controls serving one concern - a trim and the meter that shows its result.
 *                Distinct controls, same thought.
 *   kGapGroup    separate concerns sharing a panel. Usually prefer an InvisSeparator here: it
 *                carries its own margins and says the boundary out loud.
 *   kGapSection  major sections of a frame.
 *
 * HARD RULE - NOTHING TOUCHES. Two atoms may never be adjacent with zero space between them.
 * `kMinControlGap` is the floor. In practice the mistake looks like two consecutive
 * `removeFromTop()` / `removeFromBottom()` calls with nothing in between: if you write that, you
 * have skipped a spacing decision rather than made one.
 */
inline constexpr int kGapBonded   = 4;
inline constexpr int kGapRelated  = 8;
inline constexpr int kGapGroup    = 14;
inline constexpr int kGapSection  = 22;
inline constexpr int kMinControlGap = 4;

// Panel chrome
inline constexpr int   kPanelPadding = 6;
inline constexpr float kPanelCorner  = 6.0f;
inline constexpr float kPanelStroke  = 1.0f;

/**
 * Standard clearance between a text label and an indicator lamp / LED sitting beside it.
 *
 * Deliberately ONE constant rather than a per-size-preset value: the gap is optical breathing
 * room between two adjacent elements, not a property of the control it happens to be attached
 * to, so every label+indicator pair in the system reads with identical rhythm regardless of
 * whether it sits next to an XS or an XL control.
 *
 * Convention: the indicator sits to the RIGHT of the label, and the pair is centred as one unit.
 */
inline constexpr float kIndicatorLabelGap = 5.0f;

// Window zoom limits relative to the design resolution.
inline constexpr float kMinZoom = 0.75f;
inline constexpr float kMaxZoom = 2.00f;

} // namespace layout

/** Centres a non-deformable intrinsic size inside an available area. */
inline juce::Rectangle<int> centreIntrinsic(juce::Rectangle<int> area, juce::Point<int> intrinsic)
{
    return area.withSizeKeepingCentre(intrinsic.x, intrinsic.y);
}

inline juce::Rectangle<float> centreIntrinsic(juce::Rectangle<float> area, juce::Point<int> intrinsic)
{
    return area.withSizeKeepingCentre(static_cast<float>(intrinsic.x), static_cast<float>(intrinsic.y));
}

/**
 * Applies the global zoom to a root canvas component.
 *
 * The canvas keeps constant design-pixel bounds; only the transform changes. Call this from
 * the host editor's `resized()`. Returns the applied scale factor.
 *
 * With a fixed-aspect constrainer installed the letterbox offsets are zero, but they are
 * computed anyway so the canvas stays centred if the host is ever driven off-aspect.
 */
inline float applyDesignZoom(juce::Component& canvas, int designWidth, int designHeight,
                             juce::Rectangle<int> hostArea)
{
    jassert(designWidth > 0 && designHeight > 0);

    const float k = std::min(static_cast<float>(hostArea.getWidth())  / static_cast<float>(designWidth),
                             static_cast<float>(hostArea.getHeight()) / static_cast<float>(designHeight));

    const float offsetX = hostArea.getX() + (hostArea.getWidth()  - designWidth  * k) * 0.5f;
    const float offsetY = hostArea.getY() + (hostArea.getHeight() - designHeight * k) * 0.5f;

    canvas.setBounds(0, 0, designWidth, designHeight);
    canvas.setTransform(juce::AffineTransform::scale(k).translated(offsetX, offsetY));

    return k;
}

/**
 * Installs the fixed aspect ratio + zoom limits implied by a design resolution.
 *
 * Templated on the editor type so this header stays free of `juce_audio_processors` — UI atoms
 * include it, and atoms must never see AudioProcessor/APVTS headers.
 */
template <typename EditorType>
inline void applyDesignResizeLimits(EditorType& editor, int designWidth, int designHeight)
{
    editor.setResizable(true, true);
    editor.setResizeLimits(juce::roundToInt(designWidth  * layout::kMinZoom),
                           juce::roundToInt(designHeight * layout::kMinZoom),
                           juce::roundToInt(designWidth  * layout::kMaxZoom),
                           juce::roundToInt(designHeight * layout::kMaxZoom));

    if (auto* constrainer = editor.getConstrainer())
        constrainer->setFixedAspectRatio(static_cast<double>(designWidth) / static_cast<double>(designHeight));

    editor.setSize(designWidth, designHeight);
}

} // namespace invis::ui
