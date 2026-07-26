#pragma once

#include "TopSidebarDSP.h"
#include "../../ui_atoms/InvisButton.h"
#include "../../ui_atoms/InvisCellSelector.h"
#include "../../ui_atoms/InvisStepperField.h"
#include "../InvisPresetTree.h"
#include "../../design_system/InvisThemeSupplier.h"
#include "../../design_system/InvisFonts.h"
#include "../../design_system/InvisLayout.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>

namespace invis::modules {

/**
 * Universal Reusable TOP Sidebar - the standard plugin-global chassis header.
 *
 * Spans the FULL window width, above the left/right channel sidebars: everything it carries is
 * global to the plugin rather than to the signal path, so it visually outranks the channel strips.
 *
 * Contents, left to right:
 *   1. PRESET selector  - storage is NOT owned here; the module exposes callbacks and the host
 *                         plugin decides what a preset actually is.
 *   2. A / B / C        - compare slots holding independent snapshots of the whole parameter tree.
 *   3. GAIN STAGE       - [-] value [+]. The target the AUTO routines trim to. Default -18 dB.
 *   4. BYPASS           - master bypass.
 *   5. OVERSAMPLING     - ONE control. Its dropdown carries the ONLINE (realtime) and OFFLINE
 *                         (bounce) factors as separate sections: they are two facets of a single
 *                         decision, not two unrelated parameters, so they share one chassis slot.
 *
 * ALIGNMENT: the section captions are drawn by THIS component on one fixed label row, and every
 * control sits on one fixed control row. Letting each cell draw its own internal caption (as it
 * does elsewhere) pushed its value text down by the caption height, so cells and buttons ended up
 * on visibly different optical lines. Owning the caption row here makes alignment structural.
 */
class TopSidebarUI : public juce::Component {
public:
    // Layout, in design pixels
    static constexpr int kHeight        = 66;
    static constexpr int kPadding       = 8;
    static constexpr int kLabelRowH     = 11;
    static constexpr int kLabelGap      = 4;
    static constexpr int kControlRowH   = 30;
    static constexpr float kLabelFont   = 9.0f;

    static constexpr int kPresetWidth   = 190;
    static constexpr int kSlotWidth     = 26;

    static constexpr int kOsWidth       = 72;

    static int getIntrinsicHeight() { return kHeight; }

    TopSidebarUI(juce::AudioProcessorValueTreeState& apvts,
                 const juce::String& paramPrefix = "top_",
                 const juce::String& oversamplingPrefix = "os_");
    ~TopSidebarUI() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void tickAnimations(float deltaTimeSeconds = 0.016f);

    float getGainStageTargetDb() const;

    /** Hierarchical catalogue. Storage stays with the host; this only renders and reports. */
    void setPresetTree(PresetNode root);
    void setCurrentPresetPath(const juce::String& path);
    juce::String getCurrentPresetPath() const;

    void setLedPreset(ui::LEDColorPreset preset);

    std::function<void(int index, const juce::String& path)> onPresetChanged;

    /**
     * The plugin's state tree was REPLACED wholesale - by an A/B/C switch or a preset recall.
     *
     * Parameters look after themselves through their attachments. Anything a plugin keeps in the
     * state tree that is NOT a parameter has no such path back, so without this it silently
     * survives a slot switch: the knobs move and the instrument does not.
     */
    std::function<void()> onStateReplaced;

    ui::InvisButton& getBypassButton() { return bypassButton; }

private:
    static constexpr int kNumCompareSlots = 3;
    static constexpr int kNumOsFactors    = 4;
    static constexpr int kOsOnlineIdBase  = 100;
    static constexpr int kOsOfflineIdBase = 200;

    static juce::String getOsFactorName(int index);

    juce::String prefix;
    juce::String osPrefix;
    juce::AudioProcessorValueTreeState& apvtsRef;

    ui::InvisStepperField presetField;
    ui::InvisStepperField gainStageField;
    ui::InvisButton bypassButton;
    ui::InvisCellSelector oversamplingSelector;

    std::array<ui::InvisButton, kNumCompareSlots> compareButtons;
    std::array<juce::ValueTree, kNumCompareSlots> compareSlots;
    PresetNode presetRoot;
    std::vector<FlatPreset> flatPresets;
    int currentPresetIndex { 0 };
    void refreshPresetDisplay();

    juce::ValueTree defaultState;   // captured before anything is restored, for "reset slot"
    int activeCompareSlot { 0 };

    void selectCompareSlot(int slot);
    void showCompareSlotMenu(int slot);
    void applyStateToUi(juce::ValueTree newState);
    void syncGainStageField();
    void syncOversamplingDisplay();

    // Caption row positions, filled by resized() and consumed by paint() so the two never drift
    struct Caption { juce::String text; juce::Rectangle<int> area; };
    std::vector<Caption> captions;

    juce::Slider hiddenGainStageSlider;
    juce::ToggleButton hiddenBypassButton;
    juce::ComboBox hiddenOsOnlineBox;
    juce::ComboBox hiddenOsOfflineBox;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gainStageAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> osOnlineAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> osOfflineAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TopSidebarUI)
};

} // namespace invis::modules
