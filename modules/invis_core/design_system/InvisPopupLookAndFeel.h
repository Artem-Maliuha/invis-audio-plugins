#pragma once

#include "InvisTheme.h"
#include "InvisFonts.h"
#include "InvisLayout.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace invis::ui {

/**
 * The chassis's own dropdown.
 *
 * JUCE's stock popup is a light grey system list. Dropped into a dark machined panel it reads as a
 * hole punched through to the operating system - which is exactly what a menu must not do here,
 * because on this chart the menu is where you CHOOSE WHAT A STAR IS, and that is part of the
 * instrument, not part of the host.
 *
 * An item may carry a `colour`, and when it does the swatch is drawn as a lit dot rather than as a
 * square of paint: the same lamp language the rest of the panel speaks, so the colour you pick in
 * the list is recognisably the colour that will be glowing on the chart a moment later.
 */
class InvisPopupLookAndFeel : public juce::LookAndFeel_V4 {
public:
    InvisPopupLookAndFeel()
    {
        // Transparent, so JUCE treats the menu window as non-opaque and the rounded corners are
        // actually rounded instead of being cut out of a filled rectangle.
        setColour(juce::PopupMenu::backgroundColourId, juce::Colours::transparentBlack);
        setColour(juce::PopupMenu::textColourId, InvisTheme::getGlobalDefault().textPrimary);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colours::transparentBlack);
        setColour(juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    }

    int getPopupMenuBorderSize() override { return kPadding; }

    void drawPopupMenuBackground(juce::Graphics& g, int width, int height) override
    {
        const auto bounds = juce::Rectangle<float>(0.0f, 0.0f,
                                                   static_cast<float>(width),
                                                   static_cast<float>(height)).reduced(1.0f);

        g.setColour(juce::Colours::black.withAlpha(0.55f));
        g.fillRoundedRectangle(bounds.translated(0.0f, 2.0f), layout::kPanelCorner);

        g.setGradientFill(juce::ColourGradient(
            juce::Colour::fromRGB(26, 31, 39), bounds.getCentreX(), bounds.getY(),
            juce::Colour::fromRGB(18, 22, 28), bounds.getCentreX(), bounds.getBottom(), false));
        g.fillRoundedRectangle(bounds, layout::kPanelCorner);

        g.setColour(juce::Colours::white.withAlpha(0.10f));
        g.drawRoundedRectangle(bounds, layout::kPanelCorner, 1.0f);
    }

    void getIdealPopupMenuItemSize(const juce::String& text, bool isSeparator,
                                   int standardMenuItemHeight,
                                   int& idealWidth, int& idealHeight) override
    {
        if (isSeparator)
        {
            idealWidth = 60;
            idealHeight = 7;
            return;
        }

        // TIGHT. A list of eight one-word choices does not need a row of chassis furniture each -
        // the taller it is, the further the cursor has to travel to reach what it came for, and
        // the more of the chart the menu covers while you decide.
        const auto font = InvisFonts::getDisplayFont(kFontSize);
        idealWidth = juce::GlyphArrangement::getStringWidthInt(font, text) + kSwatchColumn + 2 * kPadding;
        idealHeight = standardMenuItemHeight > 0 ? standardMenuItemHeight : kItemHeight;
    }

    void drawPopupMenuSectionHeader(juce::Graphics& g, const juce::Rectangle<int>& area,
                                    const juce::String& sectionName) override
    {
        const auto theme = InvisTheme::getGlobalDefault();
        auto r = area.toFloat().reduced(static_cast<float>(kPadding), 0.0f);

        g.setFont(InvisFonts::getDisplayFont(8.5f, false));
        g.setColour(theme.textSecondary.withAlpha(0.55f));
        g.drawText(sectionName, r.withTrimmedBottom(3.0f), juce::Justification::centredLeft, false);

        const float rule = r.getBottom() - 1.0f;
        g.setColour(juce::Colours::white.withAlpha(0.07f));
        g.drawLine(r.getX(), rule, r.getRight(), rule, 1.0f);
    }

    void drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area,
                           bool isSeparator, bool isActive, bool isHighlighted,
                           bool isTicked, bool hasSubMenu, const juce::String& text,
                           const juce::String& shortcutKeyText,
                           const juce::Drawable* icon, const juce::Colour* textColour) override
    {
        juce::ignoreUnused(shortcutKeyText, icon);

        const auto theme = InvisTheme::getGlobalDefault();
        auto r = area.toFloat().reduced(static_cast<float>(kPadding) * 0.5f, 0.5f);

        if (isSeparator)
        {
            g.setColour(juce::Colours::white.withAlpha(0.07f));
            g.drawLine(r.getX() + kPadding, r.getCentreY(),
                       r.getRight() - kPadding, r.getCentreY(), 1.0f);
            return;
        }

        // The item's own colour, when it has one, is what it will BE - so the highlight borrows it
        // rather than using one accent for everything. Hovering PHASER should already look like
        // phaser.
        const auto accent = (textColour != nullptr) ? *textColour : theme.accentPrimary;

        if (isHighlighted && isActive)
        {
            g.setColour(accent.withAlpha(0.16f));
            g.fillRoundedRectangle(r, 3.0f);
            g.setColour(accent.withAlpha(0.40f));
            g.drawRoundedRectangle(r, 3.0f, 1.0f);
        }

        auto body = r.reduced(static_cast<float>(kPadding), 0.0f);
        auto swatch = body.removeFromLeft(static_cast<float>(kSwatchColumn));

        if (textColour != nullptr)
        {
            // A LIT DOT, not a square of paint: the same lamp the chart uses, so the colour in the
            // list is recognisably the one about to be glowing on the sky.
            const auto c = swatch.getCentre();
            const float dot = 4.0f;
            const float bloom = dot * 2.6f;

            g.setGradientFill(juce::ColourGradient(
                accent.withAlpha(isHighlighted ? 0.55f : 0.30f), c.x, c.y,
                juce::Colours::transparentBlack, c.x + bloom, c.y, true));
            g.fillEllipse(c.x - bloom, c.y - bloom, bloom * 2.0f, bloom * 2.0f);

            g.setColour(accent.withAlpha(isActive ? 1.0f : 0.4f));
            g.fillEllipse(c.x - dot, c.y - dot, dot * 2.0f, dot * 2.0f);
            g.setColour(juce::Colours::white.withAlpha(isHighlighted ? 0.85f : 0.35f));
            g.fillEllipse(c.x - dot * 0.35f, c.y - dot * 0.35f, dot * 0.7f, dot * 0.7f);
        }
        else if (isTicked)
        {
            const auto c = swatch.getCentre();
            g.setColour(accent.withAlpha(0.9f));
            g.drawLine(c.x - 3.5f, c.y, c.x - 1.0f, c.y + 3.0f, 1.6f);
            g.drawLine(c.x - 1.0f, c.y + 3.0f, c.x + 4.0f, c.y - 3.5f, 1.6f);
        }

        g.setFont(InvisFonts::getDisplayFont(kFontSize));
        g.setColour(isActive ? (isHighlighted ? juce::Colours::white
                                              : theme.textPrimary.withAlpha(0.88f))
                             : theme.textSecondary.withAlpha(0.35f));
        g.drawText(text, body, juce::Justification::centredLeft, true);

        if (hasSubMenu)
        {
            const float a = body.getRight() - 8.0f;
            const float mid = body.getCentreY();

            juce::Path arrow;
            arrow.startNewSubPath(a - 3.0f, mid - 3.5f);
            arrow.lineTo(a + 1.0f, mid);
            arrow.lineTo(a - 3.0f, mid + 3.5f);

            g.setColour(theme.textSecondary.withAlpha(0.65f));
            g.strokePath(arrow, juce::PathStrokeType(1.3f, juce::PathStrokeType::curved,
                                                     juce::PathStrokeType::rounded));
        }
    }

private:
    static constexpr int   kPadding     = 6;
    static constexpr int   kSwatchColumn = 16;
    static constexpr int   kItemHeight   = 19;
    static constexpr float kFontSize     = 10.5f;
};

} // namespace invis::ui
