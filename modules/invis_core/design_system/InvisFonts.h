#pragma once

#include <juce_graphics/juce_graphics.h>

namespace invis::ui {

/**
 * Typeface registry for the design system.
 *
 * The DISPLAY face is used for every technical VALUE readout on the chassis - slope cells, gain
 * stage, preset, oversampling, meter numbers.
 *
 * WHY NOT A LITERAL 14-SEGMENT FACE: that was tried and rejected. A segment typeface has a very
 * wide advance (~0.86 em against ~0.45 for a condensed face) and its glyphs are built from gapped
 * strokes, so at panel sizes it is simultaneously BIGGER and LESS legible - the exact opposite of
 * what a compact readout needs. The segment look is only affordable on large, short fields.
 *
 * NOR A CONDENSED INDUSTRIAL FACE: DIN Condensed was tried next and rejected for the opposite
 * reason - it is so narrow that it reads as vertically stretched, which is its own kind of
 * illegible.
 *
 * What actually works at panel sizes is a face ENGINEERED for small on-screen text: large
 * x-height, open counters, generous spacing, normal proportions. Tahoma is the canonical example
 * (Matthew Carter, drawn for screen legibility at small pixel sizes) and is compact enough to fit
 * these fields without being narrow.
 *
 * A host may override the choice by registering its own typeface; otherwise the best available
 * condensed face is picked from the system, falling back to the default sans if none is present.
 */
class InvisFonts {
public:
    /** Optional: hand in a bundled typeface to make the look identical across platforms. */
    static void registerDisplayTypeface(const void* data, size_t sizeInBytes)
    {
        displayTypeface() = juce::Typeface::createSystemTypefaceFor(data, sizeInBytes);
    }

    static bool hasDisplayTypeface() { return displayTypeface() != nullptr; }

    static juce::Font getDisplayFont(float height, bool bold = true)
    {
        if (auto tf = displayTypeface())
            return juce::Font(juce::FontOptions(tf).withHeight(height));

        return juce::Font(juce::FontOptions(getBestCondensedName(), height,
                                            bold ? juce::Font::bold : juce::Font::plain));
    }

private:
    /**
     * First available small-text face on this system.
     *
     * NOTE: portability compromise. Tahoma ships with both macOS and Windows, so this particular
     * chain is unusually safe - but a released plugin should still bundle an OFL equivalent
     * (Roboto, Noto Sans) and register it rather than trusting the host OS.
     */
    static const juce::String& getBestCondensedName()
    {
        static const juce::String chosen = [] {
            const juce::StringArray preferred {
                "Tahoma", "Verdana", "Lucida Grande", "Menlo", "Helvetica Neue"
            };

            const auto available = juce::Font::findAllTypefaceNames();
            for (const auto& name : preferred)
                if (available.contains(name))
                    return name;

            return juce::String{};
        }();

        return chosen;
    }

    static juce::Typeface::Ptr& displayTypeface()
    {
        static juce::Typeface::Ptr tf;
        return tf;
    }
};

} // namespace invis::ui
