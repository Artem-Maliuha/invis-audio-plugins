#pragma once

#include "InvisLED.h"
#include "../design_system/InvisThemeSupplier.h"
#include "../design_system/InvisLayout.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

namespace invis::ui {

/**
 * Physical Ballistics Engine for Channel Strip LED Peak Meters.
 * Fast 5ms attack, smooth 300ms decay, and peak-hold retention.
 */
struct MeterBallistics {
    float currentLevelDb { -60.0f };
    float peakHoldDb     { -60.0f };
    float peakHoldTimer  { 0.0f };
    float peakLuminance  { 0.0f };  // brightness of the hanging peak dot

    void reset()
    {
        currentLevelDb = -60.0f;
        peakHoldDb = -60.0f;
        peakHoldTimer = 0.0f;
        peakLuminance = 0.0f;
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

        // 2. Peak Hold: the dot STAYS PUT and fades, rather than sliding down the ladder.
        //
        // A peak marker that immediately walks downward tells you where the peak is travelling;
        // one that hangs at the level it reached and slowly dims tells you where the peak WAS,
        // which is the question a peak indicator is actually asked. It only re-acquires a lower
        // level once it has faded out, so a loud transient leaves a visible after-image.
        // Re-acquire on a new high, or once the previous dot has faded out entirely.
        if (currentLevelDb >= peakHoldDb || peakLuminance <= 0.05f)
        {
            peakHoldDb = currentLevelDb;
            peakHoldTimer = 0.8f;   // full-brightness hold before the fade begins
            peakLuminance = 1.0f;
        }
        else if (peakHoldTimer > 0.0f)
        {
            peakHoldTimer -= deltaTimeSeconds;
        }
        else
        {
            // ~1.4s phosphorescent decay - deliberately far longer than a segment's own 200ms
            // tail, so the after-image is unmistakably the peak and not just a slow segment.
            const float decayPerSec = 1.0f / 1.4f;
            peakLuminance = std::max(0.0f, peakLuminance - deltaTimeSeconds * decayPerSec);
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

enum class InvisMeterSize {
    S, // Compact sidebar strip
    M, // Standard channel strip (default)
    L  // Primary / mastering meter
};

/**
 * Absolute physical spec of an `InvisMeterSize` preset, in design pixels.
 *
 * An LED segment is a PHYSICAL OBJECT: its height and column width are constants, never a
 * fraction of the available area. That is what stops the meter from stretching vertically when
 * the host panel grows.
 */
struct MeterMetrics {
    float padding;
    float clipHeight;   // Clip indicator lamp height
    float clipGap;      // Clip lamp -> LED ladder
    float segHeight;    // ONE LED segment (constant)
    float segGap;
    float colWidth;     // ONE LED column (constant)
    float colGap;
    float labelWidth;   // dB scale text column
    float labelFontSize;
    float zeroLabelFontSize;
    float corner;

    // --- Numeric readout block below the ladder (RMS / PEAK / LUFS) ---
    float readoutTopGap;
    float readoutRowHeight;
    float readoutRowGap;
    float readoutLabelFontSize;
    float readoutValueFontSize;
};

/**
 * High-End Channel Strip Vertical LED Peak Meter Atom.
 * Supports Mono (1 column) & Stereo (2 columns side-by-side) with authentic segment optics.
 *
 * PROPORTIONALITY INTEGRITY: the meter has a fixed intrinsic size per `InvisMeterSize` preset.
 * It will NOT stretch to fill its bounds - it renders centred at its intrinsic size and asserts
 * in Debug. Use `setBoundsCentredIn()`.
 */
class InvisLEDMeter : public juce::Component, public InvisThemeSupplier {
public:
    InvisLEDMeter();
    ~InvisLEDMeter() override = default;

    // --- Intrinsic Sizing (Strict Proportionality Integrity) ---

    void setMeterSize(InvisMeterSize size) { meterSize = size; repaint(); }
    InvisMeterSize getMeterSize() const { return meterSize; }

    static MeterMetrics getMetrics(InvisMeterSize size);

    /** The one true size of a preset, in design pixels. Independent of any parent bounds. */
    static juce::Point<int> getIntrinsicSize(InvisMeterSize size);
    juce::Point<int> getIntrinsicSize() const { return getIntrinsicSize(meterSize); }

    /** Places the meter at its intrinsic size, centred inside `area`. The only correct placer. */
    void setBoundsCentredIn(juce::Rectangle<int> area)
    {
        setBounds(centreIntrinsic(area, getIntrinsicSize()));
    }

    void resized() override;

    void setNumChannels(int numChannels);
    int getNumChannels() const { return audioChannels; }

    void setScalePosition(MeterScalePosition pos) { scalePosition = pos; repaint(); }
    MeterScalePosition getScalePosition() const { return scalePosition; }

    void setScaleType(MeterScaleType type) { scaleType = type; repaint(); }
    MeterScaleType getScaleType() const { return scaleType; }

    void setLevelsDb(float channel1Db, float channel2Db = -60.0f);
    void setLevelsLinear(float channel1Linear, float channel2Linear = 0.0f);

    /**
     * Numeric readouts below the ladder. The atom is presentational: it formats and displays,
     * it does not integrate. The DSP side owns the maths.
     *
     * @param rmsDb   Sliding-window RMS, dBFS (~300ms window reads well against the ladder).
     * @param peakDb  Held true/sample peak, dBFS. The caller owns the hold + fallback policy.
     * @param lufsDb  Short-term loudness (LUFS-S, 3s, K-weighted). LUFS-S rather than LUFS-M
     *                because a 400ms window is too jumpy to read as a number, and Integrated
     *                needs an explicit reset control this atom has no business owning.
     */
    void setReadouts(float rmsDb, float peakDb, float lufsDb);
    /**
     * Gain-stage reference marker drawn across the ladder at the target level, so the user can
     * see at a glance whether the signal is landing where the gain stage says it should.
     * Distinct from the fixed golden 0 dB line: that one is a property of the SCALE, this one is
     * a property of the SESSION and moves with the parameter.
     */
    void setGainStageMarkerDb(float db);
    void setGainStageMarkerVisible(bool shouldBeVisible);

    /**
     * PER-ROW click targets on the numeric block. The atom holds no integrators - the DSP does -
     * so it only reports the intent and the owner routes it to the analyser.
     *
     * RMS and PEAK reset. LUFS does NOT reset: it cycles M -> S -> I, because the interesting
     * question about loudness is which window you are looking through, not clearing it.
     */
    std::function<void()> onResetRms;
    std::function<void()> onResetPeak;
    std::function<void()> onCycleLufsMode;

    /** Caption for the third row, so the meter shows WHICH loudness view is on screen. */
    void setLufsLabel(const juce::String& text) { lufsLabel = text; repaint(); }

    void setRmsDb(float db);
    void setPeakDb(float db);
    void setLufsDb(float db);

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
    InvisMeterSize meterSize { InvisMeterSize::M };
    MeterScalePosition scalePosition { MeterScalePosition::Right };
    MeterScaleType scaleType { MeterScaleType::dBFS };

    MeterBallistics ballistics[2];
    float targetLevelsDb[2] { -60.0f, -60.0f };

public:
    // Doubled ladder resolution: segments are half their former height, so twice as many fit and
    // the meter reads far finer around the working level.
    static constexpr int numSegments = 40;

    /** Index of the segment sitting EXACTLY on 0.0 dB. The golden reference line and every scale
        label are keyed to segment indices, so this must stay exact. */
    static constexpr int zeroSegmentIndex = 27;

private:
    float segmentLuminance[2][numSegments] { { 0.0f } };

    float gainStageMarkerDb { -18.0f };
    bool gainStageMarkerVisible { true };

    float readoutRmsDb  { -100.0f };
    float readoutPeakDb { -100.0f };
    float readoutLufsDb { -100.0f };

    bool clipLatched[2] { false, false };
    juce::Rectangle<float> clipRectBounds[2];
    juce::Rectangle<float> readoutRowBounds[3]; // per-row click targets
    float readoutFlash[3] { 0.0f, 0.0f, 0.0f }; // per-row acknowledgement flash
    juce::String lufsLabel { "LUFS-S" };
    juce::Rectangle<float> scaleHeaderBounds;

    std::optional<InvisTheme> customTheme;

    static constexpr float minDb = -48.0f;
    static constexpr float maxDb = +6.0f;

    juce::Colour getSegmentColor(int segmentIndex) const;
    float getSegmentThresholdDb(int segmentIndex) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InvisLEDMeter)
};

} // namespace invis::ui
