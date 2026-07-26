#pragma once

#include "../ui_atoms/InvisKnob.h"
#include "../ui_atoms/InvisLEDMeter.h"
#include "../ui_atoms/InvisButton.h"
#include "../design_system/InvisLayout.h"
#include <algorithm>

namespace invis::modules {

/**
 * UNIVERSAL PLUGIN FRAME - SIDEBAR GEOMETRY CONTRACT
 * ==================================================
 *
 * The Input (left) and Output (right) sidebars form the standard chassis frame shared by every
 * Invis plugin. Both derive every dimension from THIS header, so they are symmetric by
 * construction - it is not possible for one side to end up wider, or for the two meters to come
 * out different sizes, because neither sidebar is allowed to invent its own numbers.
 *
 * All values are design pixels. The host editor scales the whole tree with one transform.
 */
namespace sidebar {

inline constexpr ui::InvisKnobSize  kKnobSize  = ui::InvisKnobSize::XS;
inline constexpr ui::InvisMeterSize kMeterSize = ui::InvisMeterSize::M;

inline constexpr int kPadding = ui::layout::kPanelPadding;
inline constexpr int kGap     = ui::layout::kGapM;

/** Fixed sidebar width: wide enough for the widest atom it hosts, and never anything else. */
inline int getIntrinsicWidth()
{
    return std::max(ui::InvisKnob::getIntrinsicSize(kKnobSize).x,
                    ui::InvisLEDMeter::getIntrinsicSize(kMeterSize).x) + 2 * kPadding;
}

/** Minimum height that fits `numKnobs` stacked knobs, `numButtons` button rows and the meter. */
inline int getMinimumHeight(int numKnobs, int numButtons = 0)
{
    const int knobH   = ui::InvisKnob::getIntrinsicSize(kKnobSize).y;
    const int meterH  = ui::InvisLEDMeter::getIntrinsicSize(kMeterSize).y;
    const int buttonH = ui::InvisButton::getIntrinsicSize(ui::InvisButtonSize::XS, "AUTO").y;

    return 2 * kPadding + numKnobs * knobH + numButtons * buttonH + meterH
         + (numKnobs + numButtons) * kGap;
}

} // namespace sidebar
} // namespace invis::modules
