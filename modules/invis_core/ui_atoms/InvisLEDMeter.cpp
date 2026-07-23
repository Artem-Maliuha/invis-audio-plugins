#include "InvisLEDMeter.h"

namespace invis::ui {

static const float kSegmentDbTable[20] = {
    -48.0f, -42.0f, -36.0f, -30.0f, -25.0f, -20.0f, -16.0f, -12.0f,
    -9.0f,  -6.0f,  -4.0f,  -2.0f,  -1.0f,   0.0f,   +1.0f,  +2.0f,
    +3.0f,  +4.0f,  +5.0f,  +6.0f
};

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

void InvisLEDMeter::paint(juce::Graphics& g)
{
    // Advance ballistics smooth frame tick
    ballistics[0].process(targetLevelsDb[0], 0.016f);
    ballistics[1].process(targetLevelsDb[1], 0.016f);

    const auto theme = InvisTheme::getGlobalDefault();
    const auto bounds = getLocalBounds().toFloat();

    // 1. Carved Exterior Metal Housing & Bezel Frame
    g.setColour(juce::Colour::fromRGB(12, 15, 20));
    g.fillRoundedRectangle(bounds, 4.0f);
    g.setColour(juce::Colours::white.withAlpha(0.12f));
    g.drawRoundedRectangle(bounds, 4.0f, 1.0f);

    const float padding = 3.0f;
    auto innerArea = bounds.reduced(padding);

    // 2. DISTINCT DUAL RECESSED `CLIP` INDICATOR LAMPS (Left & Right Channels)
    const float clipH = std::clamp(innerArea.getHeight() * 0.062f, 16.0f, 22.0f);
    auto clipModuleArea = innerArea.removeFromTop(clipH).reduced(1.0f, 1.0f);
    innerArea.removeFromTop(3.0f); // Gap before LED bar section

    const bool showLabels = innerArea.getWidth() > 28.0f;
    const float labelWidth = showLabels ? std::clamp(innerArea.getWidth() * 0.36f, 16.0f, 24.0f) : 0.0f;

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
    const float colGap = 2.5f;
    const float colWidth = (barArea.getWidth() - colGap) / 2.0f;

    const float segGap = 2.0f;
    const float segHeight = (barArea.getHeight() - (numSegments - 1) * segGap) / static_cast<float>(numSegments);

    float zeroDbCenterY = 0.0f;

    for (int ch = 0; ch < numColumns; ++ch)
    {
        const float colX = barArea.getX() + ch * (colWidth + colGap);
        const float curLevelDb = ballistics[ch].currentLevelDb;
        const float peakDb = ballistics[ch].peakHoldDb;

        for (int i = 0; i < numSegments; ++i)
        {
            // Draw from bottom (index 0) to top (index numSegments - 1)
            const float segY = barArea.getBottom() - (i + 1) * segHeight - i * segGap;
            const float segDb = getSegmentThresholdDb(i);
            const juce::Colour segColor = getSegmentColor(i);

            if (i == 13) // 0.0 dB segment index (STATIONARY ZERO POSITION)
            {
                zeroDbCenterY = segY + segHeight * 0.5f;
            }

            const bool isLit = (curLevelDb >= segDb);
            const bool isPeakHold = (std::abs(peakDb - segDb) < 1.8f && peakDb >= segDb);

            // Update per-segment physical phosphorescent decay tail
            if (isLit)
            {
                segmentLuminance[ch][i] = std::min(1.0f, segmentLuminance[ch][i] + 0.35f);
            }
            else
            {
                segmentLuminance[ch][i] = std::max(0.0f, segmentLuminance[ch][i] - 0.065f);
            }

            const float activeLum = isPeakHold ? 1.0f : segmentLuminance[ch][i];
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

    // 4. Render Dynamic Scale Text Labels aligned to STATIONARY segment centers
    if (showLabels)
    {
        struct LabelItem { int segIdx; const char* label; };

        static const LabelItem kLabelsDbfs[] = {
            { 19, "+6" }, { 16, "+3" }, { 13, "0" }, { 9, "-6" }, { 7, "-12" }, { 4, "-24" }, { 2, "-36" }, { 0, "-48" }
        };
        static const LabelItem kLabelsDbu[] = {
            { 19, "+24" }, { 16, "+21" }, { 13, "+18" }, { 9, "+12" }, { 7, "+6" }, { 4, "0" }, { 2, "-12" }, { 0, "-30" }
        };
        static const LabelItem kLabelsVu[] = {
            { 19, "+5" }, { 16, "+3" }, { 13, "0" }, { 9, "-3" }, { 7, "-6" }, { 4, "-10" }, { 2, "-20" }, { 0, "-34" }
        };
        static const LabelItem kLabelsPpm[] = {
            { 19, "+7" }, { 16, "+5" }, { 13, "+3" }, { 9, "0" }, { 7, "-3" }, { 4, "-9" }, { 2, "-15" }, { 0, "-27" }
        };

        const LabelItem* kLabels = kLabelsDbfs;
        switch (scaleType)
        {
            case MeterScaleType::dBFS: kLabels = kLabelsDbfs; break;
            case MeterScaleType::dBU:  kLabels = kLabelsDbu; break;
            case MeterScaleType::VU:   kLabels = kLabelsVu; break;
            case MeterScaleType::PPM:  kLabels = kLabelsPpm; break;
        }

        for (int idx = 0; idx < 8; ++idx)
        {
            const auto& item = kLabels[idx];
            const float segY = barArea.getBottom() - (item.segIdx + 1) * segHeight - item.segIdx * segGap;
            const float segCenterY = segY + segHeight * 0.5f;
            const float segDb = getSegmentThresholdDb(item.segIdx);

            const bool isZeroMark = (item.segIdx == 13);
            const float fontSize = isZeroMark ? std::clamp(segHeight * 1.15f, 9.0f, 12.0f) : std::clamp(segHeight * 0.95f, 8.0f, 10.5f);
            g.setFont(juce::FontOptions(fontSize, juce::Font::bold));

            juce::Colour labelColor = theme.textSecondary.withAlpha(0.7f);
            if (isZeroMark)
            {
                labelColor = juce::Colour::fromRGB(255, 215, 0); // Bright Gold Zero Mark
            }
            else if (segDb > 0.05f)
            {
                labelColor = juce::Colour::fromRGB(255, 23, 68);  // Crimson Red (> 0 dB)
            }
            else if (segDb >= -12.0f)
            {
                labelColor = juce::Colour::fromRGB(255, 171, 0); // Warm Amber (-12 dB .. 0 dB)
            }

            g.setColour(labelColor);

            const auto textRect = juce::Rectangle<float>(labelArea.getX() + 1.0f, segCenterY - fontSize * 0.6f, labelArea.getWidth() - 1.0f, fontSize * 1.2f);

            // Extra glow for zero reference mark label
            if (isZeroMark)
            {
                g.setColour(juce::Colour::fromRGB(255, 171, 0).withAlpha(0.40f));
                g.drawText(item.label, textRect.translated(-0.5f, -0.5f), juce::Justification::centredLeft, false);
                g.setColour(labelColor);
            }

            g.drawText(item.label, textRect, juce::Justification::centredLeft, false);
        }
    }

    // Schedule continuous smooth repaint if ballistics level is active
    if (ballistics[0].currentLevelDb > -58.0f || ballistics[1].currentLevelDb > -58.0f)
    {
        juce::MessageManager::callAsync([this]() { repaint(); });
    }
}

} // namespace invis::ui
