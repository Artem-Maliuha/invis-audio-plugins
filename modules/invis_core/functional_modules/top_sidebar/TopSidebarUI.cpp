#include "TopSidebarUI.h"

namespace invis::modules {

TopSidebarUI::TopSidebarUI(juce::AudioProcessorValueTreeState& apvts,
                           const juce::String& paramPrefix,
                           const juce::String& oversamplingPrefix)
    : prefix(paramPrefix), osPrefix(oversamplingPrefix), apvtsRef(apvts)
{
    // Every cell here is a pure VALUE slot - the captions belong to this component's label row,
    // so all controls share one optical baseline.
    auto makeValueCell = [](ui::InvisCellSelector& cell) { cell.setLabel({}); };

    // 1. PRESET: a stepper field over a hierarchical catalogue. Arrows walk the flattened tree
    //    in the same order the menu shows it; the middle opens the tree itself.
    presetField.setStepperSize(ui::InvisStepperSize::M);
    presetField.setMaxValueChars(22);
    presetField.setItems({ "Init" });

    presetField.onIndexChanged = [this](int index) {
        currentPresetIndex = index;
        refreshPresetDisplay();
        if (onPresetChanged != nullptr && index < static_cast<int>(flatPresets.size()))
            onPresetChanged(index, flatPresets[static_cast<size_t>(index)].path);
    };

    presetField.onBuildPopup = [this](juce::PopupMenu& menu) {
        buildPresetMenu(presetRoot, menu, flatPresets, currentPresetIndex);

        // Well clear of the presets, and numbered from a base no preset can reach, so a library
        // that grows can never collide with an action.
        menu.addSeparator();
        menu.addItem(kSaveId, "Save preset...");
        menu.addItem(kRevealId, "Show preset folder");
        menu.addSeparator();
        menu.addItem(kRestoreId, "Restore factory presets");
    };

    presetField.onPopupResult = [this](int id) {
        if (id == kSaveId)    { if (onSavePreset) onSavePreset(); return; }
        if (id == kRevealId)  { if (onRevealPresetFolder) onRevealPresetFolder(); return; }
        if (id == kRestoreId) { if (onRestoreFactory) onRestoreFactory(); return; }

        presetField.setSelectedIndex(id - 1, juce::sendNotificationSync);
    };

    addAndMakeVisible(presetField);
    setPresetTree(PresetNode::folder("", { PresetNode::preset("Init") }));

    // 2. A / B / C compare slots
    for (int i = 0; i < kNumCompareSlots; ++i)
    {
        auto& btn = compareButtons[static_cast<size_t>(i)];
        btn.setButtonSize(ui::InvisButtonSize::S);
        btn.setLedVisible(false);
        btn.setLabel(juce::String::charToString(static_cast<juce::juce_wchar>('A' + i)));
        btn.setToggleMode(false);
        btn.onClick = [this, i]() { selectCompareSlot(i); };
        btn.onSecondaryClick = [this, i]() { showCompareSlotMenu(i); };
        addAndMakeVisible(btn);
    }
    compareButtons[0].setToggleState(true, juce::dontSendNotification);

    // 3. GAIN STAGE - one integrated field: -/+ end caps, scrub, wheel, double-click to type
    gainStageAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvtsRef, prefix + "gain_stage", hiddenGainStageSlider);

    gainStageField.setStepperSize(ui::InvisStepperSize::M);
    gainStageField.setRange(TopSidebarDSP::kMinGainStageDb, TopSidebarDSP::kMaxGainStageDb, 1.0);
    gainStageField.setNumDecimals(0);
    gainStageField.setSuffix(" DB");
    gainStageField.setMaxValueChars(6);
    gainStageField.onValueChanged = [this](double v) {
        hiddenGainStageSlider.setValue(v, juce::sendNotificationSync);
    };
    addAndMakeVisible(gainStageField);
    syncGainStageField();

    // 4. BYPASS
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        apvtsRef, prefix + "bypass", hiddenBypassButton);

    bypassButton.setButtonSize(ui::InvisButtonSize::M);
    bypassButton.setLabel("BYPASS");
    bypassButton.setLedPosition(ui::ButtonLedPosition::Left);
    bypassButton.setToggleMode(true);
    bypassButton.setLedColour(juce::Colour::fromRGB(255, 171, 0)); // amber: "not processing"
    bypassButton.setToggleState(hiddenBypassButton.getToggleState(), juce::dontSendNotification);
    bypassButton.onToggle = [this](bool state) {
        hiddenBypassButton.setToggleState(state, juce::sendNotificationSync);
    };

    // PARAMETER -> BUTTON, so the lamp follows host automation and compare recall
    hiddenBypassButton.onStateChange = [this]() {
        bypassButton.setToggleState(hiddenBypassButton.getToggleState(), juce::dontSendNotification);
    };

    // ... and the gain-stage field likewise
    hiddenGainStageSlider.onValueChange = [this]() { syncGainStageField(); };
    addAndMakeVisible(bypassButton);

    // 5. OVERSAMPLING - ONE chassis control. Online and offline are two facets of one decision,
    //    so they live in one slot with a structured menu rather than two adjacent widgets.
    // ComboBoxAttachment does NOT populate the box - it only drives the selected index. On an
    // empty box setSelectedItemIndex() is a silent no-op, which is why the factor never changed.
    // The item ids must be 1-based and match the parameter's choice count exactly.
    for (int i = 0; i < kNumOsFactors; ++i)
    {
        hiddenOsOnlineBox.addItem(getOsFactorName(i), i + 1);
        hiddenOsOfflineBox.addItem(getOsFactorName(i), i + 1);
    }

    osOnlineAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        apvtsRef, osPrefix + "online", hiddenOsOnlineBox);
    osOfflineAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        apvtsRef, osPrefix + "offline", hiddenOsOfflineBox);

    // Keep the summary text live even when the parameter moves from the host side
    hiddenOsOnlineBox.onChange  = [this]() { syncOversamplingDisplay(); };
    hiddenOsOfflineBox.onChange = [this]() { syncOversamplingDisplay(); };

    makeValueCell(oversamplingSelector);
    oversamplingSelector.setPopupMode(true);

    oversamplingSelector.onBuildPopup = [this](juce::PopupMenu& menu) {
        const int online = std::max(0, hiddenOsOnlineBox.getSelectedItemIndex());
        const int offline = std::max(0, hiddenOsOfflineBox.getSelectedItemIndex());

        menu.addSectionHeader("ONLINE  (realtime)");
        for (int i = 0; i < kNumOsFactors; ++i)
            menu.addItem(kOsOnlineIdBase + i, getOsFactorName(i), true, i == online);

        menu.addSeparator();
        menu.addSectionHeader("OFFLINE  (bounce)");
        for (int i = 0; i < kNumOsFactors; ++i)
            menu.addItem(kOsOfflineIdBase + i, getOsFactorName(i), true, i == offline);
    };

    oversamplingSelector.onPopupResult = [this](int id) {
        if (id >= kOsOnlineIdBase && id < kOsOnlineIdBase + kNumOsFactors)
            hiddenOsOnlineBox.setSelectedItemIndex(id - kOsOnlineIdBase, juce::sendNotificationSync);
        else if (id >= kOsOfflineIdBase && id < kOsOfflineIdBase + kNumOsFactors)
            hiddenOsOfflineBox.setSelectedItemIndex(id - kOsOfflineIdBase, juce::sendNotificationSync);

        syncOversamplingDisplay();
    };

    addAndMakeVisible(oversamplingSelector);
    syncOversamplingDisplay();

    // Snapshot of every parameter at its declared default, for "reset slot to defaults".
    // Built by overwriting values in a copy of the tree rather than by actually driving the
    // parameters to their defaults - that would be audible, and would fight whatever the host
    // has already restored.
    defaultState = apvtsRef.copyState();
    for (auto child : defaultState)
    {
        const auto paramId = child.getProperty("id").toString();
        if (auto* p = apvtsRef.getParameter(paramId))
            child.setProperty("value", p->convertFrom0to1(p->getDefaultValue()), nullptr);
    }

    // EAGER init: every slot starts as a copy of the state the plugin loaded with.
    //
    // The obvious-looking alternative - fill a slot lazily on first visit - breaks the primary
    // use case. Tweak for ten minutes, hit B, and a lazily-filled B would inherit those ten
    // minutes: nothing changes and there is nothing to compare against. Seeding all slots up
    // front is what makes B mean "where I started".
    for (auto& slot : compareSlots)
        slot = apvtsRef.copyState();
}

juce::String TopSidebarUI::getOsFactorName(int index)
{
    static const char* names[] = { "OFF", "2x", "4x", "8x" };
    return names[juce::jlimit(0, kNumOsFactors - 1, index)];
}

void TopSidebarUI::syncOversamplingDisplay()
{
    // "online / offline" - compact enough for the slot, and unambiguous once the menu has been
    // opened even once.
    const int online = std::max(0, hiddenOsOnlineBox.getSelectedItemIndex());
    const int offline = std::max(0, hiddenOsOfflineBox.getSelectedItemIndex());

    oversamplingSelector.setDisplayTextOverride(getOsFactorName(online) + "/" + getOsFactorName(offline));
}

void TopSidebarUI::syncGainStageField()
{
    gainStageField.setValue(hiddenGainStageSlider.getValue(), juce::dontSendNotification);
}

void TopSidebarUI::showCompareSlotMenu(int slot)
{
    const juce::String name = juce::String::charToString(static_cast<juce::juce_wchar>('A' + slot));

    juce::PopupMenu menu;
    menu.addSectionHeader("SLOT " + name);
    menu.addItem(1, "Copy current state to " + name, slot != activeCompareSlot);
    menu.addItem(2, "Copy current state to ALL slots");
    menu.addSeparator();
    menu.addItem(3, "Reset " + name + " to plugin defaults");

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&compareButtons[static_cast<size_t>(slot)]),
                       [this, slot](int result) {
                           if (result == 1)
                           {
                               compareSlots[static_cast<size_t>(slot)] = apvtsRef.copyState();
                           }
                           else if (result == 2)
                           {
                               // "I have dialled something in and want every slot to start here"
                               const auto snapshot = apvtsRef.copyState();
                               for (auto& s : compareSlots) s = snapshot.createCopy();
                           }
                           else if (result == 3)
                           {
                               compareSlots[static_cast<size_t>(slot)] = defaultState.createCopy();
                               if (slot == activeCompareSlot)
                                   applyStateToUi(defaultState.createCopy());
                           }
                       });
}

void TopSidebarUI::applyStateToUi(juce::ValueTree newState)
{
    apvtsRef.replaceState(std::move(newState));

    syncGainStageField();
    bypassButton.setToggleState(hiddenBypassButton.getToggleState(), juce::dontSendNotification);
    syncOversamplingDisplay();

    if (onStateReplaced) onStateReplaced();
}

void TopSidebarUI::selectCompareSlot(int slot)
{
    if (slot == activeCompareSlot) return;

    // Save what is on screen into the slot we are leaving, then load the one we are entering.
    compareSlots[static_cast<size_t>(activeCompareSlot)] = apvtsRef.copyState();
    applyStateToUi(compareSlots[static_cast<size_t>(slot)].createCopy());

    activeCompareSlot = slot;

    for (int i = 0; i < kNumCompareSlots; ++i)
        compareButtons[static_cast<size_t>(i)].setToggleState(i == slot, juce::dontSendNotification);
}

float TopSidebarUI::getGainStageTargetDb() const
{
    return static_cast<float>(hiddenGainStageSlider.getValue());
}

void TopSidebarUI::setPresetTree(PresetNode root)
{
    presetRoot = std::move(root);

    flatPresets.clear();
    flattenPresetTree(presetRoot, flatPresets);

    if (flatPresets.empty())
        flatPresets.push_back({ "Init", "Init" });

    juce::StringArray names;
    for (const auto& p : flatPresets) names.add(p.name);

    presetField.setItems(names);
    currentPresetIndex = juce::jlimit(0, static_cast<int>(flatPresets.size()) - 1, currentPresetIndex);
    presetField.setSelectedIndex(currentPresetIndex, juce::dontSendNotification);
    refreshPresetDisplay();
}

void TopSidebarUI::setCurrentPresetPath(const juce::String& path)
{
    for (size_t i = 0; i < flatPresets.size(); ++i)
    {
        if (flatPresets[i].path == path)
        {
            currentPresetIndex = static_cast<int>(i);
            presetField.setSelectedIndex(currentPresetIndex, juce::dontSendNotification);
            refreshPresetDisplay();
            return;
        }
    }
}

juce::String TopSidebarUI::getCurrentPresetPath() const
{
    if (currentPresetIndex < static_cast<int>(flatPresets.size()))
        return flatPresets[static_cast<size_t>(currentPresetIndex)].path;

    return {};
}

void TopSidebarUI::refreshPresetDisplay()
{
    // The field shows the LEAF name only. Showing the whole path would be truthful but useless -
    // it would not fit, and the folder is what you just navigated through to get here.
    if (currentPresetIndex < static_cast<int>(flatPresets.size()))
        presetField.setDisplayTextOverride(flatPresets[static_cast<size_t>(currentPresetIndex)].name);
}

void TopSidebarUI::setLedPreset(ui::LEDColorPreset preset)
{
    juce::ignoreUnused(preset);
    // Bypass keeps its amber semantic colour deliberately: it means "your signal is NOT being
    // processed", which must not be repainted into whatever accent the user picked.
}

void TopSidebarUI::tickAnimations(float deltaTimeSeconds)
{
    bypassButton.tickAnimation(deltaTimeSeconds);
    for (auto& btn : compareButtons) btn.tickAnimation(deltaTimeSeconds);
}

void TopSidebarUI::paint(juce::Graphics& g)
{
    const auto theme = ui::InvisThemeSupplier::getParentTheme(this);
    auto bounds = getLocalBounds().toFloat();

    // Chassis header plate, slightly brighter than the channel sidebars so it reads as the
    // global tier of the frame rather than another strip.
    g.setColour(juce::Colour::fromRGB(24, 29, 37).withAlpha(0.95f));
    g.fillRoundedRectangle(bounds, ui::layout::kPanelCorner);
    g.setColour(juce::Colours::white.withAlpha(0.10f));
    g.drawRoundedRectangle(bounds, ui::layout::kPanelCorner, ui::layout::kPanelStroke);

    // Engraved seam along the bottom edge, separating global controls from the channel strips
    g.setColour(juce::Colours::black.withAlpha(0.45f));
    g.drawHorizontalLine(juce::roundToInt(bounds.getBottom() - 1.0f), bounds.getX() + 6.0f, bounds.getRight() - 6.0f);

    // Section captions, all on ONE row - this is what keeps cells and buttons optically aligned
    g.setColour(theme.textSecondary.withAlpha(0.60f));
    g.setFont(ui::InvisFonts::getDisplayFont(kLabelFont));

    for (const auto& c : captions)
        g.drawText(c.text, c.area, juce::Justification::centredLeft, false);
}

void TopSidebarUI::resized()
{
    captions.clear();

    auto area = getLocalBounds().reduced(kPadding);
    auto labelRow = area.removeFromTop(kLabelRowH);
    area.removeFromTop(kLabelGap);
    auto row = area.removeFromTop(kControlRowH);

    auto addCaption = [this, &labelRow](const juce::String& text, int x, int width) {
        captions.push_back({ text, juce::Rectangle<int>(x, labelRow.getY(), width, labelRow.getHeight()) });
    };

    // --- Left cluster: PRESET + A/B/C ---
    auto presetArea = row.removeFromLeft(kPresetWidth);
    presetField.setBoundsCentredIn(presetArea);
    addCaption("PRESET", presetArea.getX(), presetArea.getWidth());

    row.removeFromLeft(ui::layout::kGapM);

    const int compareBlockX = row.getX();
    for (int i = 0; i < kNumCompareSlots; ++i)
    {
        compareButtons[static_cast<size_t>(i)].setBoundsCentredIn(row.removeFromLeft(kSlotWidth));
        row.removeFromLeft(ui::layout::kGapXS);
    }
    addCaption("COMPARE", compareBlockX, kNumCompareSlots * (kSlotWidth + ui::layout::kGapXS));

    // --- Right cluster: OVERSAMPLING, BYPASS, GAIN STAGE (laid out right to left) ---
    auto osArea = row.removeFromRight(kOsWidth);
    oversamplingSelector.setBounds(osArea);
    addCaption("OVERSAMP", osArea.getX(), osArea.getWidth());

    row.removeFromRight(ui::layout::kGapL);

    const auto bypassSize = bypassButton.getIntrinsicSize();
    auto bypassArea = row.removeFromRight(bypassSize.x);
    bypassButton.setBoundsCentredIn(bypassArea);

    row.removeFromRight(ui::layout::kGapL);

    const auto gainSize = gainStageField.getIntrinsicSize();
    auto gainArea = row.removeFromRight(gainSize.x);
    addCaption("GAIN STAGE", gainArea.getX(), gainArea.getWidth());
    gainStageField.setBoundsCentredIn(gainArea);
}

} // namespace invis::modules
