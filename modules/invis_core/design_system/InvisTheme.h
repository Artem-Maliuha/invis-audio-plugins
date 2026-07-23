#pragma once

#include <juce_graphics/juce_graphics.h>
#include <optional>

namespace invis::ui {

/**
 * Design Tokens for Invis Audio Plugins Design System.
 * Supports fluent overrides on any layer of the component tree.
 */
struct InvisTheme {
    // Colors
    juce::Colour background      { juce::Colour::fromRGB(18, 20, 24) };
    juce::Colour surfacePanel    { juce::Colour::fromRGB(28, 31, 38) };
    juce::Colour accentPrimary   { juce::Colour::fromRGB(0, 210, 255) }; // Neon Cyan
    juce::Colour accentSecondary { juce::Colour::fromRGB(255, 128, 0) }; // Neon Orange
    juce::Colour knobTrackBg     { juce::Colour::fromRGB(45, 50, 60) };
    juce::Colour knobThumb       { juce::Colour::fromRGB(240, 245, 255) };
    juce::Colour textPrimary     { juce::Colour::fromRGB(240, 245, 255) };
    juce::Colour textSecondary   { juce::Colour::fromRGB(140, 150, 165) };

    // Geometry & Layout Tokens
    float defaultKnobSize        { 56.0f };
    float trackWidth             { 4.5f };
    float cornerRadius           { 8.0f };
    float labelFontSize          { 12.0f };
    float valueFontSize          { 11.0f };

    // Fluent override helpers
    InvisTheme withAccent(juce::Colour newAccent) const {
        InvisTheme copy = *this;
        copy.accentPrimary = newAccent;
        return copy;
    }

    InvisTheme withBackground(juce::Colour newBg) const {
        InvisTheme copy = *this;
        copy.background = newBg;
        return copy;
    }

    InvisTheme withKnobSize(float newSize) const {
        InvisTheme copy = *this;
        copy.defaultKnobSize = newSize;
        return copy;
    }

    static const InvisTheme& getGlobalDefault() {
        static InvisTheme defaultTheme;
        return defaultTheme;
    }
};

} // namespace invis::ui
