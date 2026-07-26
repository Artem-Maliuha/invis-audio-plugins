#include "InvisStepperField.h"
#include <cmath>

namespace invis::ui {

InvisStepperField::InvisStepperField()
{
    setRepaintsOnMouseActivity(true);
}

StepperMetrics InvisStepperField::getMetrics(InvisStepperSize size)
{
    // charW is the advance of the display face (Tahoma Bold, ~0.6 em). Sizing this from a
    // narrower face silently clips the last character off every value.
    //        height  stepZone  charW  font   glyph  corner
    switch (size)
    {
        case InvisStepperSize::XS: return { 20.0f, 13.0f, 5.6f,  9.0f,  8.0f, 3.0f };
        case InvisStepperSize::S:  return { 25.0f, 16.0f, 6.4f, 10.0f,  9.5f, 3.5f };
        case InvisStepperSize::L:  return { 36.0f, 22.0f, 8.2f, 13.0f, 13.0f, 5.0f };
        case InvisStepperSize::M:
        default:                   return { 30.0f, 19.0f, 7.2f, 11.5f, 11.0f, 4.0f };
    }
}

juce::Point<int> InvisStepperField::getIntrinsicSize(InvisStepperSize size, int maxValueChars)
{
    const auto m = getMetrics(size);
    const float valueWidth = std::max(30.0f, m.valueCharWidth * static_cast<float>(std::max(2, maxValueChars)));

    return { static_cast<int>(std::ceil(2.0f * m.stepZoneWidth + valueWidth)),
             static_cast<int>(std::ceil(m.height)) };
}

InvisStepperField::Layout InvisStepperField::computeLayout() const
{
    const auto m = getMetrics(stepperSize);

    Layout l;
    l.content = centreIntrinsic(getLocalBounds().toFloat(), getIntrinsicSize());

    auto area = l.content;
    l.minusZone = area.removeFromLeft(m.stepZoneWidth);
    l.plusZone = area.removeFromRight(m.stepZoneWidth);
    l.valueZone = area;

    return l;
}

void InvisStepperField::setRange(double newMin, double newMax, double newStep)
{
    minValue = newMin;
    maxValue = newMax;
    stepSize = (newStep > 0.0) ? newStep : 1.0;
    setValue(value, juce::dontSendNotification);
}

void InvisStepperField::setValue(double newValue, juce::NotificationType notification)
{
    const double clamped = juce::jlimit(minValue, maxValue, newValue);
    if (std::abs(clamped - value) < 1.0e-9) return;

    value = clamped;
    repaint();

    if (notification != juce::dontSendNotification && onValueChanged != nullptr)
        onValueChanged(value);
}

void InvisStepperField::setItems(const juce::StringArray& newItems)
{
    items = newItems;
    listMode = true;
    selectedIndex = juce::jlimit(0, std::max(0, items.size() - 1), selectedIndex);
    repaint();
}

void InvisStepperField::setSelectedIndex(int index, juce::NotificationType notification)
{
    if (items.isEmpty()) return;

    const int clamped = juce::jlimit(0, items.size() - 1, index);
    if (clamped == selectedIndex) return;

    selectedIndex = clamped;
    displayOverride.clear();
    repaint();

    if (notification != juce::dontSendNotification && onIndexChanged != nullptr)
        onIndexChanged(selectedIndex);
}

void InvisStepperField::step(int direction)
{
    if (listMode)
    {
        setSelectedIndex(selectedIndex + direction, juce::sendNotificationSync);
        return;
    }

    setValue(value + direction * stepSize, juce::sendNotificationSync);
}

void InvisStepperField::showPopup()
{
    if (onBuildPopup == nullptr) return;

    juce::PopupMenu menu;
    onBuildPopup(menu);

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this)
                                                  .withMinimumWidth(getWidth()),
                       [this](int result) {
                           if (result > 0 && onPopupResult != nullptr)
                               onPopupResult(result);
                       });
}

juce::String InvisStepperField::getDisplayText() const
{
    if (displayOverride.isNotEmpty())
        return displayOverride;

    if (listMode)
        return items.isEmpty() ? juce::String() : items[juce::jlimit(0, items.size() - 1, selectedIndex)];

    if (valueFormatter != nullptr)
        return valueFormatter(value);

    return juce::String(value, numDecimals) + suffix;
}

void InvisStepperField::resized()
{
    const auto intrinsic = getIntrinsicSize();
    jassert(getWidth() == 0 || (getWidth() == intrinsic.x && getHeight() == intrinsic.y));

    if (inlineEditor != nullptr)
        inlineEditor->setBounds(computeLayout().valueZone.toNearestInt());
}

void InvisStepperField::mouseDown(const juce::MouseEvent& e)
{
    const auto l = computeLayout();

    if (l.minusZone.contains(e.position)) { minusPressed = true; step(-1); repaint(); return; }
    if (l.plusZone.contains(e.position))  { plusPressed = true;  step(+1); repaint(); return; }

    if (listMode)
    {
        // The middle of a list field is a "show me everything" target, not a scrub surface:
        // a preset tree cannot be scrubbed through meaningfully.
        showPopup();
        return;
    }

    scrubbing = true;
    scrubStartValue = value;
}

void InvisStepperField::mouseDrag(const juce::MouseEvent& e)
{
    if (!scrubbing) return;

    // One step per 6 design px of vertical travel: coarse enough that a nudge does not fly
    // through the whole range, fine enough to feel continuous. Referenced to the value at
    // drag start rather than accumulated per frame, so the field cannot drift.
    const double stepsMoved = std::round(-e.getDistanceFromDragStartY() / 6.0);
    setValue(scrubStartValue + stepsMoved * stepSize, juce::sendNotificationSync);
}

void InvisStepperField::mouseUp(const juce::MouseEvent&)
{
    minusPressed = false;
    plusPressed = false;
    scrubbing = false;
    repaint();
}

void InvisStepperField::mouseDoubleClick(const juce::MouseEvent& e)
{
    if (!listMode && computeLayout().valueZone.contains(e.position))
        showInlineEditor();
}

void InvisStepperField::mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    if (wheel.deltaY == 0.0f) return;
    step(wheel.deltaY > 0.0f ? 1 : -1);
}

void InvisStepperField::showInlineEditor()
{
    if (inlineEditor != nullptr) return;

    const auto theme = getEffectiveTheme();
    const auto m = getMetrics(stepperSize);
    const auto l = computeLayout();

    inlineEditor = std::make_unique<juce::TextEditor>();
    inlineEditor->setFont(juce::FontOptions(m.fontSize)); // plain face: you are typing, not reading a display
    inlineEditor->setText(juce::String(value, numDecimals), false);
    inlineEditor->selectAll();
    inlineEditor->setJustification(juce::Justification::centred);
    inlineEditor->setBorder({});
    inlineEditor->setColour(juce::TextEditor::backgroundColourId, juce::Colour::fromRGB(6, 9, 14));
    inlineEditor->setColour(juce::TextEditor::outlineColourId, theme.accentPrimary);
    inlineEditor->setColour(juce::TextEditor::textColourId, theme.accentPrimary.brighter(0.2f));
    inlineEditor->addListener(this);

    addAndMakeVisible(*inlineEditor);
    inlineEditor->setBounds(l.valueZone.toNearestInt());
    inlineEditor->grabKeyboardFocus();
}

void InvisStepperField::commitInlineEditor()
{
    if (inlineEditor == nullptr) return;

    // Tolerant parse: strip whatever unit the user typed back at us
    auto text = inlineEditor->getText().trim().removeCharacters("dBdb ").trim();
    if (text.isNotEmpty())
        setValue(text.getDoubleValue(), juce::sendNotificationSync);

    dismissInlineEditor();
}

void InvisStepperField::dismissInlineEditor()
{
    if (inlineEditor == nullptr) return;

    inlineEditor->removeListener(this);
    removeChildComponent(inlineEditor.get());
    inlineEditor.reset();
    repaint();
}

void InvisStepperField::textEditorReturnKeyPressed(juce::TextEditor&) { commitInlineEditor(); }
void InvisStepperField::textEditorFocusLost(juce::TextEditor&)        { commitInlineEditor(); }
void InvisStepperField::textEditorEscapeKeyPressed(juce::TextEditor&) { dismissInlineEditor(); }

void InvisStepperField::paint(juce::Graphics& g)
{
    const auto theme = getEffectiveTheme();
    const auto m = getMetrics(stepperSize);
    const auto l = computeLayout();

    if (l.content.getWidth() <= 0.0f) return;

    // 1. Carved technical slot housing - ONE body spanning both end zones and the value well
    g.setColour(juce::Colour::fromRGB(10, 13, 18));
    g.fillRoundedRectangle(l.content, m.corner);

    g.setColour(juce::Colours::white.withAlpha(0.14f));
    g.drawRoundedRectangle(l.content, m.corner, 1.0f);
    g.setColour(juce::Colours::black.withAlpha(0.50f));
    g.drawRoundedRectangle(l.content.reduced(0.5f), m.corner - 0.5f, 0.75f);

    // 2. End zones: pressed ones sink, and the seams that separate them from the value well are
    //    engraved rather than drawn as separate widget edges
    auto paintZone = [&](const juce::Rectangle<float>& zone, bool pressed) {
        if (pressed)
        {
            g.setColour(juce::Colours::white.withAlpha(0.07f));
            g.fillRect(zone);
        }
    };

    paintZone(l.minusZone, minusPressed);
    paintZone(l.plusZone, plusPressed);

    g.setColour(juce::Colours::black.withAlpha(0.55f));
    g.drawVerticalLine(juce::roundToInt(l.minusZone.getRight()), l.content.getY() + 2.0f, l.content.getBottom() - 2.0f);
    g.drawVerticalLine(juce::roundToInt(l.plusZone.getX()), l.content.getY() + 2.0f, l.content.getBottom() - 2.0f);
    g.setColour(juce::Colours::white.withAlpha(0.07f));
    g.drawVerticalLine(juce::roundToInt(l.minusZone.getRight()) + 1, l.content.getY() + 2.0f, l.content.getBottom() - 2.0f);
    g.drawVerticalLine(juce::roundToInt(l.plusZone.getX()) + 1, l.content.getY() + 2.0f, l.content.getBottom() - 2.0f);

    // 3. Step glyphs, dimmed at the ends of the range so the limits are visible before you hit them
    const bool atMin = listMode ? (selectedIndex <= 0) : (value <= minValue + 1.0e-6);
    const bool atMax = listMode ? (selectedIndex >= items.size() - 1) : (value >= maxValue - 1.0e-6);

    const float bar = m.glyphSize * 0.52f;

    if (listMode)
    {
        // Arrows: stepping through named things is navigation, not arithmetic.
        auto drawArrow = [&](juce::Point<float> centre, bool pointsRight, bool dimmed) {
            const float halfW = bar * 0.42f;
            const float halfH = bar * 0.52f;
            const float dir = pointsRight ? 1.0f : -1.0f;

            juce::Path tri;
            tri.startNewSubPath(centre.x + dir * halfW, centre.y);
            tri.lineTo(centre.x - dir * halfW, centre.y - halfH);
            tri.lineTo(centre.x - dir * halfW, centre.y + halfH);
            tri.closeSubPath();

            g.setColour(theme.textSecondary.withAlpha(dimmed ? 0.25f : 0.85f));
            g.fillPath(tri);
        };

        drawArrow(l.minusZone.getCentre(), false, atMin);
        drawArrow(l.plusZone.getCentre(), true, atMax);
    }
    else
    {
        g.setColour(theme.textSecondary.withAlpha(atMin ? 0.25f : 0.85f));
        g.fillRect(juce::Rectangle<float>(l.minusZone.getCentreX() - bar * 0.5f,
                                           l.minusZone.getCentreY() - 0.75f, bar, 1.5f));

        g.setColour(theme.textSecondary.withAlpha(atMax ? 0.25f : 0.85f));
        g.fillRect(juce::Rectangle<float>(l.plusZone.getCentreX() - bar * 0.5f,
                                           l.plusZone.getCentreY() - 0.75f, bar, 1.5f));
        g.fillRect(juce::Rectangle<float>(l.plusZone.getCentreX() - 0.75f,
                                           l.plusZone.getCentreY() - bar * 0.5f, 1.5f, bar));
    }

    // 4. Value on a 14-segment display, matching every other technical readout on the chassis
    if (inlineEditor == nullptr)
    {
        const auto text = getDisplayText().toUpperCase();
        const auto accent = theme.accentPrimary;

        g.setFont(InvisFonts::getDisplayFont(m.fontSize));

        g.setColour(accent.withAlpha(0.42f));
        g.drawText(text, l.valueZone.translated(-0.6f, -0.6f), juce::Justification::centred, false);
        g.drawText(text, l.valueZone.translated(0.6f, 0.6f), juce::Justification::centred, false);

        g.setColour(accent.brighter(0.20f));
        g.drawText(text, l.valueZone, juce::Justification::centred, false);
    }
}

} // namespace invis::ui
