#pragma once

#include "InvisLED.h"
#include "../design_system/InvisThemeSupplier.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace invis::ui {

/**
 * Physical Ballistics Engine for Channel Strip LED Peak Meters.
 * Fast 5ms attack, smooth 300ms decay, and peak-hold retention.
 */
struct MeterBallistics {
    float currentLevelDb { -60.0f };
    float peakHoldDb     { -60.0f };
    float peakHoldTimer  { 0.0f };

    void reset()
    {
        currentLevelDb = -60.0f;
        peakHoldDb = -60.0f;
        peakHoldTimer = 0.0f;
    }

    void process(float targetDb, float deltaTimeSeconds = 0.016f)
    {
        // 1. Level Attack & Decay
        if (targetDb > currentLevelDb)
        {
            // Fast attack (~5ms)
            const float attackSpeed = 60.0f;
            currentLevelDb += (targetDb - currentLevelDb) * std::min(1.0f, deltaTimeSeconds * attackSpeed);
        }
        else
        {
            // Smooth channel strip decay (~300ms, approx 14 dB / sec for a rich phosphorescent decay tail)
            const float decayRateDbPerSec = 14.0f;
            currentLevelDb = std::max(targetDb, currentLevelDb - deltaTimeSeconds * decayRateDbPerSec);
        }

        // 2. Peak Hold Logic
        if (currentLevelDb >= peakHoldDb)
        {
            peakHoldDb = currentLevelDb;
            peakHoldTimer = 0.8f; // Hold for 800ms
        }
        else
        {
            if (peakHoldTimer > 0.0f)
            {
                peakHoldTimer -= deltaTimeSeconds;
            }
            else
            {
                // Fall after hold
                const float peakFallRate = 28.0f;
                peakHoldDb = std::max(currentLevelDb, peakHoldDb - deltaTimeSeconds * peakFallRate);
            }
        }
    }
};

enum class MeterScalePosition {
    Left,
    Right
};

enum class MeterScaleType {
    dBFS,
    dBU,
    VU,
    PPM
};

/**
 * High-End Channel Strip Vertical LED Peak Meter Atom.
 * Supports Mono (1 column) & Stereo (2 columns side-by-side) with authentic segment optics.
 */
class InvisLEDMeter : public juce::Component, public InvisThemeSupplier {
public:
    InvisLEDMeter();
    ~InvisLEDMeter() override = default;

    void setNumChannels(int numChannels);
    int getNumChannels() const { return audioChannels; }

    void setScalePosition(MeterScalePosition pos) { scalePosition = pos; repaint(); }
    MeterScalePosition getScalePosition() const { return scalePosition; }

    void setScaleType(MeterScaleType type) { scaleType = type; repaint(); }
    MeterScaleType getScaleType() const { return scaleType; }

    void setLevelsDb(float channel1Db, float channel2Db = -60.0f);
    void setLevelsLinear(float channel1Linear, float channel2Linear = 0.0f);

    void resetClip()
    {
        clipLatched[0] = false;
        clipLatched[1] = false;
        repaint();
    }

    bool isClipLatched() const { return clipLatched[0] || clipLatched[1]; }

    InvisTheme getEffectiveTheme() const override
    {
        return customTheme.value_or(InvisTheme::getGlobalDefault());
    }

    void setThemeOverride(const InvisTheme& theme) { customTheme = theme; repaint(); }
    void clearThemeOverride() { customTheme.reset(); repaint(); }

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;

private:
    int audioChannels { 2 }; // Audio input channels (1 = Mono, 2 = Stereo)
    MeterScalePosition scalePosition { MeterScalePosition::Right };
    MeterScaleType scaleType { MeterScaleType::dBFS };

    MeterBallistics ballistics[2];
    float targetLevelsDb[2] { -60.0f, -60.0f };
    float segmentLuminance[2][20] { { 0.0f } };

    bool clipLatched[2] { false, false };
    juce::Rectangle<float> clipRectBounds[2];
    juce::Rectangle<float> scaleHeaderBounds;

    std::optional<InvisTheme> customTheme;

    static constexpr int numSegments = 20;
    static constexpr float minDb = -48.0f;
    static constexpr float maxDb = +6.0f;

    juce::Colour getSegmentColor(int segmentIndex) const;
    float getSegmentThresholdDb(int segmentIndex) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InvisLEDMeter)
};

} // namespace invis::ui
