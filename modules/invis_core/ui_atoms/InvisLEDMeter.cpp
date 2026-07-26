#include "InvisLEDMeter.h"
#include <cmath>
#include <vector>

namespace invis::ui {

// 40-segment ladder, index 0 = bottom (-48 dBFS) .. index 39 = top (+6 dBFS).
// Perceptual weighting is preserved from the old 20-segment table: coarse 4-5 dB steps down in
// the noise floor, tightening to 1 dB through the working range and 0.5 dB around unity, where
// mix decisions are actually made. Index 27 is EXACTLY 0.0 dB - the golden reference line and
// every scale label key off that, so it must not drift.
static const float kSegmentDbTable[InvisLEDMeter::numSegments] = {
    -48.0f, -43.0f, -38.0f, -34.0f, -31.0f, -28.0f, -25.0f, -22.0f,
    -20.0f, -18.0f, -16.0f, -14.0f, -12.0f, -11.0f, -10.0f,  -9.0f,
     -8.0f,  -7.0f,  -6.0f,  -5.0f,  -4.0f,  -3.0f,  -2.5f,  -2.0f,
     -1.5f,  -1.0f,  -0.5f,   0.0f,  +0.5f,  +1.0f,  +1.5f,  +2.0f,
     +2.5f,  +3.0f,  +3.5f,  +4.0f,  +4.5f,  +5.0f,  +5.5f,  +6.0f
};

/**
 * Scale labelling.
 *
 * BELOW 0 dB the ladder is anchored on the GAIN STAGE value: that is the level you are aiming at,
 * so the working range should be readable relative to it, and "am I 3 dB hot?" becomes a glance
 * rather than a subtraction. The step is finer than the headroom step because this is where the
 * decisions actually happen.
 *
 * ABOVE 0 dB the marks are FIXED absolute headroom references and do NOT move with the gain
 * stage. Anchoring them too made the whole top of the scale jump every time the target was
 * nudged, which is exactly the wrong behaviour: headroom to clipping is a property of the
 * converter, not of your chosen operating level.
 */
static constexpr float kLabelStepBelowZeroDb = 3.0f;
static const float kFixedHeadroomMarksDb[] = { 3.0f, 6.0f };

/**
 * Offset added to the dBFS threshold for each displayed scale.
 * Replaces four hand-maintained label tables that had to be rewritten whenever the ladder
 * changed - and which were mutually inconsistent (their VU and dBU columns did not correspond
 * to any single alignment convention).
 */
static float getScaleOffsetDb(MeterScaleType type)
{
    switch (type)
    {
        case MeterScaleType::dBU: return 18.0f; // EBU: 0 dBFS = +18 dBu
        case MeterScaleType::VU:  return 20.0f; // SMPTE: 0 VU = -20 dBFS
        case MeterScaleType::PPM: return  9.0f; // EBU alignment: -9 dBFS = PPM test
        case MeterScaleType::dBFS:
        default:                  return  0.0f;
    }
}

InvisLEDMeter::InvisLEDMeter()
{
    setOpaque(false);
}

void InvisLEDMeter::setNumChannels(int numChannels)
{
    audioChannels = juce::jlimit(1, 2, numChannels);
    repaint();
}

void InvisLEDMeter::setLevelsDb(float channel1Db, float channel2Db)
{
    // Mirror mono signal to both columns if audioChannels == 1
    const float ch2Db = (audioChannels >= 2) ? channel2Db : channel1Db;

    targetLevelsDb[0] = juce::jlimit(minDb - 12.0f, maxDb + 12.0f, channel1Db);
    targetLevelsDb[1] = juce::jlimit(minDb - 12.0f, maxDb + 12.0f, ch2Db);

    // Sticky Clip Latch Physics
    if (channel1Db >= 0.0f) clipLatched[0] = true;
    if (ch2Db >= 0.0f) clipLatched[1] = true;

    repaint();
}

void InvisLEDMeter::setLevelsLinear(float channel1Linear, float channel2Linear)
{
    const float ch1Db = (channel1Linear > 0.00001f) ? juce::Decibels::gainToDecibels(channel1Linear) : -60.0f;
    const float ch2Db = (channel2Linear > 0.00001f) ? juce::Decibels::gainToDecibels(channel2Linear) : -60.0f;
    setLevelsDb(ch1Db, ch2Db);
}

void InvisLEDMeter::mouseDown(const juce::MouseEvent& e)
{
    const auto p = e.position;

    // Reset sticky Left CLIP lamp if clicked
    if (clipRectBounds[0].contains(p))
    {
        clipLatched[0] = false;
        repaint();
        return;
    }

    // Reset sticky Right CLIP lamp if clicked
    if (clipRectBounds[1].contains(p))
    {
        clipLatched[1] = false;
        repaint();
        return;
    }

    // Click a NUMBER to act on that figure alone.
    //
    // Checked BEFORE the catch-all clip reset below, otherwise clicking the numbers would
    // silently clear the clip lamps as a side effect.
    for (int row = 0; row < 3; ++row)
    {
        if (!readoutRowBounds[row].contains(p)) continue;

        readoutFlash[row] = 1.0f;
        repaint();

        if (row == 0 && onResetRms != nullptr)            onResetRms();
        else if (row == 1 && onResetPeak != nullptr)      onResetPeak();
        else if (row == 2 && onCycleLufsMode != nullptr)  onCycleLufsMode();

        return;
    }

    // Reset both if clicked anywhere else on the meter
    if (isClipLatched())
    {
        resetClip();
    }

    // Toggle measurement scale if clicked inside scaleHeaderBounds
    if (scaleHeaderBounds.contains(p))
    {
        switch (scaleType)
        {
            case MeterScaleType::dBFS: setScaleType(MeterScaleType::dBU); break;
            case MeterScaleType::dBU:  setScaleType(MeterScaleType::VU);  break;
            case MeterScaleType::VU:   setScaleType(MeterScaleType::PPM); break;
            case MeterScaleType::PPM:  setScaleType(MeterScaleType::dBFS); break;
        }
        return;
    }
}

float InvisLEDMeter::getSegmentThresholdDb(int segmentIndex) const
{
    const int idx = juce::jlimit(0, numSegments - 1, segmentIndex);
    return kSegmentDbTable[idx];
}

juce::Colour InvisLEDMeter::getSegmentColor(int segmentIndex) const
{
    const float db = getSegmentThresholdDb(segmentIndex);

    // Standard Professional Console Color Zones:
    // 1. Red Zone: Strictly ABOVE 0 dB (> 0.0 dBFS)
    if (db > 0.05f)
        return juce::Colour::fromRGB(255, 23, 68);    // Crimson Ruby Overload / Red Zone

    // 2. Amber Zone: Nominal Headroom (-12 dB .. 0 dB)
    if (db >= -12.0f)
        return juce::Colour::fromRGB(255, 171, 0);   // Warm Amber Gold Headroom

    // 3. Green Zone: Normal Operating Level (< -12 dB)
    return juce::Colour::fromRGB(0, 230, 118);       // Emerald Green Normal Level
}

MeterMetrics InvisLEDMeter::getMetrics(InvisMeterSize size)
{
    //        pad  clipH  clipGap  segH  segGap  colW  colGap  labelW  labelFont  zeroFont  corner  roTop  roRow  roGap  roLabelF  roValueF
    switch (size)
    {
        case InvisMeterSize::S: return { 2.5f, 11.0f, 2.5f, 2.5f, 0.9f,  9.0f, 2.0f, 18.0f,  7.0f,  8.5f, 3.0f, 4.0f,  9.0f, 1.0f, 6.0f,  7.5f };
        case InvisMeterSize::L: return { 3.5f, 17.0f, 3.5f, 4.5f, 1.5f, 15.0f, 3.5f, 26.0f, 10.0f, 12.0f, 5.0f, 6.0f, 13.0f, 2.0f, 8.0f, 10.5f };
        case InvisMeterSize::M:
        default:                return { 3.0f, 14.0f, 3.0f, 3.5f, 1.2f, 12.0f, 3.0f, 22.0f,  8.5f, 10.5f, 4.0f, 5.0f, 11.0f, 1.5f, 7.0f,  9.0f };
    }
}

/** Height of the RMS / PEAK / LUFS block, including the gap that separates it from the ladder. */
static float getReadoutBlockHeight(const MeterMetrics& m)
{
    return m.readoutTopGap + 3.0f * m.readoutRowHeight + 2.0f * m.readoutRowGap;
}

juce::Point<int> InvisLEDMeter::getIntrinsicSize(InvisMeterSize size)
{
    const auto m = getMetrics(size);

    const float w = 2.0f * m.padding + m.labelWidth + 2.0f * m.colWidth + m.colGap;
    const float h = 2.0f * m.padding + m.clipHeight + m.clipGap
                  + numSegments * m.segHeight + (numSegments - 1) * m.segGap
                  + getReadoutBlockHeight(m);

    return { static_cast<int>(std::ceil(w)), static_cast<int>(std::ceil(h)) };
}

void InvisLEDMeter::setReadouts(float rmsDb, float peakDb, float lufsDb)
{
    readoutRmsDb = rmsDb;
    readoutPeakDb = peakDb;
    readoutLufsDb = lufsDb;
    repaint();
}

void InvisLEDMeter::setGainStageMarkerDb(float db)
{
    if (std::abs(gainStageMarkerDb - db) < 0.01f) return;
    gainStageMarkerDb = db;
    repaint();
}

void InvisLEDMeter::setGainStageMarkerVisible(bool shouldBeVisible)
{
    if (gainStageMarkerVisible == shouldBeVisible) return;
    gainStageMarkerVisible = shouldBeVisible;
    repaint();
}

void InvisLEDMeter::setRmsDb(float db)  { readoutRmsDb = db; repaint(); }
void InvisLEDMeter::setPeakDb(float db) { readoutPeakDb = db; repaint(); }
void InvisLEDMeter::setLufsDb(float db) { readoutLufsDb = db; repaint(); }

void InvisLEDMeter::resized()
{
    // STRICT PROPORTIONALITY INTEGRITY: see InvisKnob::resized(). The meter letterboxes rather
    // than stretching, so a wrong rect is cosmetic, not fatal - but it must not go unnoticed.
    const auto intrinsic = getIntrinsicSize();
    jassert(getWidth() == 0 || (getWidth() == intrinsic.x && getHeight() == intrinsic.y));
}

void InvisLEDMeter::paint(juce::Graphics& g)
{
    // Advance ballistics smooth frame tick
    ballistics[0].process(targetLevelsDb[0], 0.016f);
    ballistics[1].process(targetLevelsDb[1], 0.016f);

    const auto theme = InvisTheme::getGlobalDefault();
    const auto metrics = getMetrics(meterSize);

    // Never stretch: render at the preset's intrinsic size, centred in whatever bounds we got.
    const auto bounds = centreIntrinsic(getLocalBounds().toFloat(), getIntrinsicSize());
    if (bounds.getWidth() <= 0.0f || bounds.getHeight() <= 0.0f) return;

    // 1. Carved Exterior Metal Housing & Bezel Frame
    g.setColour(juce::Colour::fromRGB(12, 15, 20));
    g.fillRoundedRectangle(bounds, metrics.corner);
    g.setColour(juce::Colours::white.withAlpha(0.12f));
    g.drawRoundedRectangle(bounds, metrics.corner, 1.0f);

    const float padding = metrics.padding;
    auto innerArea = bounds.reduced(padding);

    // Reserve the numeric readout block at the bottom before anything else claims height.
    auto readoutArea = innerArea.removeFromBottom(getReadoutBlockHeight(metrics));

    // 2. DISTINCT DUAL RECESSED `CLIP` INDICATOR LAMPS (Left & Right Channels)
    const float clipH = metrics.clipHeight;
    auto clipModuleArea = innerArea.removeFromTop(clipH).reduced(1.0f, 1.0f);
    innerArea.removeFromTop(metrics.clipGap); // Gap before LED bar section

    const bool showLabels = true;
    const float labelWidth = metrics.labelWidth;

    auto clipBarsArea = clipModuleArea;
    if (showLabels)
    {
        if (scalePosition == MeterScalePosition::Left)
            clipBarsArea.removeFromLeft(labelWidth);
        else
            clipBarsArea.removeFromRight(labelWidth);
    }

    const float lampGap = 2.0f;
    const float lampWidth = (clipBarsArea.getWidth() - lampGap) / 2.0f;

    for (int ch = 0; ch < 2; ++ch)
    {
        clipRectBounds[ch] = juce::Rectangle<float>(clipBarsArea.getX() + ch * (lampWidth + lampGap), clipBarsArea.getY(), lampWidth, clipH);
        const bool isChClipped = clipLatched[ch];

        if (isChClipped)
        {
            // Latched Bright Crimson Ruby Glow
            g.setColour(juce::Colour::fromRGB(255, 23, 68));
            g.fillRoundedRectangle(clipRectBounds[ch], 2.5f);

            // Volumetric Soft Aura
            const auto auraGrad = juce::ColourGradient(
                juce::Colour::fromRGB(255, 23, 68).withAlpha(0.60f), clipRectBounds[ch].getCentreX(), clipRectBounds[ch].getCentreY(),
                juce::Colours::transparentBlack, clipRectBounds[ch].getCentreX() + lampWidth * 1.2f, clipRectBounds[ch].getCentreY(),
                true
            );
            g.setGradientFill(auraGrad);
            g.fillRoundedRectangle(clipRectBounds[ch].expanded(2.0f, 1.5f), 3.0f);

            // High-Intensity Phosphor Core Thread inside Clip Lamp
            g.setColour(juce::Colours::white.withAlpha(0.95f));
            g.fillRoundedRectangle(clipRectBounds[ch].reduced(lampWidth * 0.20f, clipH * 0.25f), 1.5f);
        }
        else
        {
            // Unlit Dim Recessed Clip Lamp Socket
            g.setColour(juce::Colour::fromRGB(22, 12, 16));
            g.fillRoundedRectangle(clipRectBounds[ch], 2.5f);
            g.setColour(juce::Colours::white.withAlpha(0.10f));
            g.drawRoundedRectangle(clipRectBounds[ch], 2.5f, 1.0f);
        }
    }

    // Chamfered CNC Metal Separator Line above LED Strip
    g.setColour(juce::Colours::white.withAlpha(0.12f));
    g.drawHorizontalLine(juce::roundToInt(innerArea.getY() - 1.5f), innerArea.getX(), innerArea.getRight());

    // Reserve side of innerArea for dB scale text labels if width permits
    auto barArea = innerArea;
    juce::Rectangle<float> labelArea;
    if (showLabels)
    {
        if (scalePosition == MeterScalePosition::Left)
            labelArea = barArea.removeFromLeft(labelWidth);
        else
            labelArea = barArea.removeFromRight(labelWidth);
    }

    const int numColumns = 2; // Always draw 2 parallel LED columns (mono mirrors signal to both columns)
    const float colGap = metrics.colGap;
    const float colWidth = metrics.colWidth;

    // Constant physical segment pitch - the LED ladder does NOT rubber-band with the panel.
    const float segGap = metrics.segGap;
    const float segHeight = metrics.segHeight;

    auto segmentCentreY = [&](int idx) {
        const float segY = barArea.getBottom() - (idx + 1) * segHeight - idx * segGap;
        return segY + segHeight * 0.5f;
    };

    // Continuous dB -> y. Interpolates between bracketing segments rather than snapping to the
    // nearest: the ladder's dB steps are deliberately non-uniform, so snapping would misplace a
    // label by up to half a segment exactly where the scale is finest.
    auto dbToY = [&](float db) {
        if (db <= kSegmentDbTable[0]) return segmentCentreY(0);
        if (db >= kSegmentDbTable[numSegments - 1]) return segmentCentreY(numSegments - 1);

        for (int i = 0; i < numSegments - 1; ++i)
        {
            const float lo = kSegmentDbTable[i];
            const float hi = kSegmentDbTable[i + 1];

            if (db >= lo && db <= hi)
            {
                const float t = (hi > lo) ? (db - lo) / (hi - lo) : 0.0f;
                return segmentCentreY(i) + t * (segmentCentreY(i + 1) - segmentCentreY(i));
            }
        }

        return segmentCentreY(numSegments - 1);
    };

    const auto gainStageColour = juce::Colour::fromRGB(0, 200, 255);

    float zeroDbCenterY = 0.0f;

    for (int ch = 0; ch < numColumns; ++ch)
    {
        const float colX = barArea.getX() + ch * (colWidth + colGap);
        const float curLevelDb = ballistics[ch].currentLevelDb;
        const float peakDb = ballistics[ch].peakHoldDb;

        // Peak hold marks exactly ONE segment: the highest whose threshold the peak has reached.
        // The old fixed 1.8 dB tolerance was calibrated for 20 coarse segments; against the new
        // 0.5 dB steps near unity it would light seven segments at once.
        int peakSegIdx = -1;
        for (int i = 0; i < numSegments; ++i)
            if (peakDb >= getSegmentThresholdDb(i))
                peakSegIdx = i;

        for (int i = 0; i < numSegments; ++i)
        {
            // Draw from bottom (index 0) to top (index numSegments - 1)
            const float segY = barArea.getBottom() - (i + 1) * segHeight - i * segGap;
            const float segDb = getSegmentThresholdDb(i);
            const juce::Colour segColor = getSegmentColor(i);

            if (i == zeroSegmentIndex) // STATIONARY ZERO POSITION
            {
                zeroDbCenterY = segY + segHeight * 0.5f;
            }

            const bool isLit = (curLevelDb >= segDb);
            const bool isPeakHold = (i == peakSegIdx) && (ballistics[ch].peakLuminance > 0.02f);

            // Update per-segment physical phosphorescent decay tail
            if (isLit)
            {
                segmentLuminance[ch][i] = std::min(1.0f, segmentLuminance[ch][i] + 0.35f);
            }
            else
            {
                segmentLuminance[ch][i] = std::max(0.0f, segmentLuminance[ch][i] - 0.065f);
            }

            // The hanging peak dot carries its OWN slowly-decaying brightness; where it sits on
            // top of an already-lit segment, whichever is brighter wins.
            const float activeLum = isPeakHold
                                      ? std::max(segmentLuminance[ch][i], ballistics[ch].peakLuminance)
                                      : segmentLuminance[ch][i];
            const auto segRect = juce::Rectangle<float>(colX, segY, colWidth, segHeight);

            if (activeLum > 0.01f)
            {
                // Volumetric Soft Light Aura onto surrounding frame
                const float glowR = segHeight * 1.5f;
                const auto glowGrad = juce::ColourGradient(
                    segColor.withAlpha(0.38f * activeLum), segRect.getCentreX(), segRect.getCentreY(),
                    juce::Colours::transparentBlack, segRect.getCentreX() + colWidth * 1.2f, segRect.getCentreY(),
                    true
                );
                g.setGradientFill(glowGrad);
                g.fillRoundedRectangle(segRect.expanded(2.0f, 1.0f), 2.0f);

                // Lit Translucent LED Segment Body with phosphorescent tail
                g.setColour(segColor.brighter(0.25f).withAlpha(activeLum));
                g.fillRoundedRectangle(segRect, 1.5f);

                // High-Intensity Phosphor Core Thread
                g.setColour(juce::Colours::white.withAlpha(0.88f * activeLum));
                g.fillRoundedRectangle(segRect.reduced(colWidth * 0.22f, segHeight * 0.22f), 1.0f);
            }
            else
            {
                // Unlit Dim Recessed LED Segment Socket
                g.setColour(juce::Colour::fromRGB(22, 28, 36).withAlpha(0.65f));
                g.fillRoundedRectangle(segRect, 1.5f);
                g.setColour(juce::Colours::black.withAlpha(0.4f));
                g.drawRoundedRectangle(segRect, 1.5f, 0.75f);
            }
        }
    }

    // 3. PROMINENT FIXED GOLDEN ZERO dB REFERENCE LINE
    if (zeroDbCenterY > 0.0f)
    {
        g.setColour(juce::Colour::fromRGB(255, 215, 0).withAlpha(0.90f));
        g.drawLine(barArea.getX() - 2.0f, zeroDbCenterY, barArea.getRight() + 2.0f, zeroDbCenterY, 1.5f);
    }

    // 3b. GAIN STAGE TARGET MARKER
    //
    // Interpolated between bracketing segments rather than snapped to the nearest one: the target
    // is a continuous dB value that will rarely coincide with a segment threshold, and snapping
    // would misreport it by up to half a segment exactly where the ladder is finest.
    if (gainStageMarkerVisible)
    {
        const float markerY = dbToY(gainStageMarkerDb);

        {
            const auto markerColour = gainStageColour;
            const float x1 = barArea.getX() - 2.0f;
            const float x2 = barArea.getRight() + 2.0f;

            // Dashed, so it never reads as another hard scale line like the golden zero
            const float dash[] = { 3.0f, 2.5f };
            juce::Path line;
            line.startNewSubPath(x1, markerY);
            line.lineTo(x2, markerY);

            juce::PathStrokeType(1.2f).createDashedStroke(line, line, dash, 2);
            g.setColour(markerColour.withAlpha(0.85f));
            g.strokePath(line, juce::PathStrokeType(1.2f));

            // Small solid tick on the scale side so the marker is findable at a glance
            const float tickX = (scalePosition == MeterScalePosition::Left) ? x1 : x2;
            const float tickDir = (scalePosition == MeterScalePosition::Left) ? -3.5f : 3.5f;
            g.drawLine(tickX, markerY, tickX + tickDir, markerY, 2.0f);
        }
    }

    // 4. SCALE LABELS, ANCHORED ON THE GAIN STAGE
    //
    // Positions are generated as gainStage +- n * kLabelStepDb rather than taken from a fixed set
    // of segment indices, so the ladder is read RELATIVE to the level you are aiming at. Because
    // the dB-per-segment mapping is deliberately non-uniform, equal dB steps are NOT equal pixel
    // steps: the lower labels would collide. A minimum-spacing pass therefore drops whatever will
    // not fit, in priority order, so the important marks always survive.
    if (showLabels)
    {
        const float scaleOffsetDb = getScaleOffsetDb(scaleType);
        // Tuned against the plain label font, not the larger reference-mark font: keying it to
        // the big one made the pass drop marks that had ample room, leaving visible dead zones
        // either side of the gain stage.
        const float minSpacing = metrics.labelFontSize * 1.25f;

        struct Mark { float db; float y; bool isGainStage; bool isZero; };
        std::vector<Mark> candidates;

        auto addCandidate = [&](float db, bool isGainStage, bool isZero) {
            if (db < kSegmentDbTable[0] - 0.01f || db > kSegmentDbTable[numSegments - 1] + 0.01f)
                return;
            candidates.push_back({ db, dbToY(db), isGainStage, isZero });
        };

        // Priority order: the gain stage always survives, then 0 dBFS (the absolute ceiling
        // reference), then the fixed headroom marks, then the gain-stage-relative marks working
        // outward. Headroom outranks the relative grid because a moving clip reference is worse
        // than a missing intermediate tick.
        addCandidate(gainStageMarkerDb, true, false);
        addCandidate(0.0f, false, true);

        for (float db : kFixedHeadroomMarksDb)
            addCandidate(db, false, false);

        for (int n = 1; n <= 20; ++n)
        {
            // Below zero only: above it the scale is absolute, not relative to the target.
            const float up = gainStageMarkerDb + n * kLabelStepBelowZeroDb;
            const float down = gainStageMarkerDb - n * kLabelStepBelowZeroDb;

            if (up < -0.05f) addCandidate(up, false, false);
            addCandidate(down, false, false);
        }

        std::vector<Mark> accepted;
        for (const auto& c : candidates)
        {
            bool collides = false;
            for (const auto& a : accepted)
            {
                if (std::abs(a.y - c.y) < minSpacing) { collides = true; break; }
            }
            if (!collides) accepted.push_back(c);
        }

        for (const auto& mark : accepted)
        {
            const float displayDb = mark.db + scaleOffsetDb;
            const juce::String labelText = (std::abs(displayDb) < 0.05f)
                                             ? juce::String("0")
                                             : juce::String::formatted("%+.0f", displayDb);

            const float fontSize = (mark.isGainStage || mark.isZero)
                                     ? metrics.zeroLabelFontSize : metrics.labelFontSize;
            g.setFont(juce::FontOptions(fontSize, juce::Font::bold));

            juce::Colour labelColor = theme.textSecondary.withAlpha(0.7f);
            if (mark.isGainStage)      labelColor = gainStageColour;               // matches the marker line
            else if (mark.isZero)      labelColor = juce::Colour::fromRGB(255, 215, 0);
            else if (mark.db > 0.05f)  labelColor = juce::Colour::fromRGB(255, 23, 68);
            else if (mark.db >= -12.0f) labelColor = juce::Colour::fromRGB(255, 171, 0);

            const auto textRect = juce::Rectangle<float>(labelArea.getX() + 1.0f,
                                                         mark.y - fontSize * 0.6f,
                                                         labelArea.getWidth() - 1.0f,
                                                         fontSize * 1.2f);

            // Emissive halo on the two reference marks so they read above the plain scale ticks
            if (mark.isGainStage || mark.isZero)
            {
                g.setColour(labelColor.withAlpha(0.40f));
                g.drawText(labelText, textRect.translated(-0.5f, -0.5f), juce::Justification::centredLeft, false);
            }

            g.setColour(labelColor);
            g.drawText(labelText, textRect, juce::Justification::centredLeft, false);
        }
    }

    // 5. NUMERIC READOUT BLOCK: RMS / PEAK / LUFS - also the click target that resets them
    {
        readoutArea.removeFromTop(metrics.readoutTopGap);

        // Engraved separator between the optical ladder and the numeric section
        g.setColour(juce::Colours::black.withAlpha(0.55f));
        g.drawHorizontalLine(juce::roundToInt(readoutArea.getY() - metrics.readoutTopGap * 0.5f),
                             readoutArea.getX(), readoutArea.getRight());
        g.setColour(juce::Colours::white.withAlpha(0.09f));
        g.drawHorizontalLine(juce::roundToInt(readoutArea.getY() - metrics.readoutTopGap * 0.5f + 1.0f),
                             readoutArea.getX(), readoutArea.getRight());

        struct Readout { juce::String label; float db; };
        const Readout rows[3] = {
            { "RMS",     readoutRmsDb },
            { "PEAK",    readoutPeakDb },
            { lufsLabel, readoutLufsDb }
        };

        for (int i = 0; i < 3; ++i)
        {
            auto rowArea = readoutArea.removeFromTop(metrics.readoutRowHeight);
            if (i < 2) readoutArea.removeFromTop(metrics.readoutRowGap);

            readoutRowBounds[i] = rowArea;

            // Per-row acknowledgement flash: the click only changes DSP state, so without a
            // visible press the control would feel dead.
            if (readoutFlash[i] > 0.01f)
            {
                g.setColour(theme.accentPrimary.withAlpha(0.20f * readoutFlash[i]));
                g.fillRoundedRectangle(rowArea.expanded(1.0f, 0.0f), 2.0f);

                readoutFlash[i] = std::max(0.0f, readoutFlash[i] - 0.10f);
                juce::MessageManager::callAsync([this]() { repaint(); });
            }

            g.setColour(theme.textSecondary.withAlpha(0.55f));
            g.setFont(juce::FontOptions(metrics.readoutLabelFontSize, juce::Font::bold));
            g.drawText(rows[i].label, rowArea, juce::Justification::centredLeft, false);

            const float db = rows[i].db;
            const bool isSilent = (db <= -99.0f);

            // PEAK turns crimson once it has been over the top - the number should carry the
            // same warning the ladder does, without needing the clip lamp to be read separately.
            juce::Colour valueColour = theme.textPrimary.withAlpha(0.90f);
            if (isSilent)                          valueColour = theme.textSecondary.withAlpha(0.35f);
            else if (i == 1 && db > -0.05f)        valueColour = juce::Colour::fromRGB(255, 23, 68);
            else if (i == 1 && db > -3.0f)         valueColour = juce::Colour::fromRGB(255, 171, 0);

            g.setColour(valueColour);
            g.setFont(juce::FontOptions("Courier New", metrics.readoutValueFontSize, juce::Font::bold));
            g.drawText(isSilent ? juce::String("-" + juce::String(juce::CharPointer_UTF8("\xe2\x88\x9e")))
                                : juce::String::formatted("%.1f", db),
                       rowArea, juce::Justification::centredRight, false);
        }
    }

    // Schedule continuous smooth repaint if ballistics level is active
    if (ballistics[0].currentLevelDb > -58.0f || ballistics[1].currentLevelDb > -58.0f)
    {
        juce::MessageManager::callAsync([this]() { repaint(); });
    }
}

} // namespace invis::ui
