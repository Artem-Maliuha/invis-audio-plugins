#pragma once

#include "PluginProcessor.h"
#include <invis_core/invis_core.h>

enum class PanelTheme {
    DarkSlateCharcoal, // Deep matte slate gray
    ObsidianBlack,     // Pure dark obsidian black
    MidnightIndigo,    // Deep midnight navy blue gradient
    CyberViolet,       // Deep electric violet gradient
    GunmetalSteel      // Technical dark gunmetal steel
};

class InvisDemoPluginEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    // FIXED DESIGN RESOLUTION. The entire editor is authored once at this size in design pixels;
    // resizing is a pure zoom applied to a single root canvas. No percentage layout anywhere.
    static constexpr int kDesignWidth  = 1340; // the morph pad is the instrument; it gets the room
    static constexpr int kDesignHeight = 880; // taller meter + AUTO row + top bar + slope cells + morph pad

    static constexpr int kOuterMargin  = 12;
    static constexpr int kHeaderHeight = 46;
    static constexpr int kFooterHeight = 108;

    static constexpr int   kFooterLabelHeight  = 16;
    static constexpr int   kFooterButtonHeight = 26;
    static constexpr float kHeaderFontSize     = 22.0f;
    static constexpr float kFooterFontSize     = 10.0f;

    explicit InvisDemoPluginEditor(InvisDemoPluginProcessor& p);
    ~InvisDemoPluginEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;

    /** Root design-pixel canvas. Holds every child; the editor only zooms it. */
    struct Canvas : juce::Component {
        explicit Canvas(InvisDemoPluginEditor& o) : owner(o) { setInterceptsMouseClicks(false, true); }
        void paint(juce::Graphics& g) override { owner.paintCanvas(g); }
        void resized() override { owner.layoutCanvas(); }
        InvisDemoPluginEditor& owner;
    };

    void paintCanvas(juce::Graphics& g);
    void layoutCanvas();

    Canvas canvas { *this };

    InvisDemoPluginProcessor& processorRef;

    // Test Bench Controls & Modules
    invis::modules::TopSidebarUI topSidebarUI;
    invis::modules::InputSidebarUI inputSidebarUI;
    invis::modules::OutputSidebarUI outputSidebarUI;
    invis::modules::InputFilterUI inputFilterUI;

    invis::ui::InvisConstellation constellation;
    invis::ui::InvisButton addNodeButton;
    invis::ui::InvisButton randomiseButton;
    invis::ui::InvisCellSelector channelModeCell;
    static constexpr int kChannelCellWidth = 62;

    /**
     * Inspector for one star. Lives in the PLUGIN, not in the atom: choosing which effect a star
     * is, and what colour it wears, is product knowledge the constellation atom must not carry.
     */
    struct StarPanel : juce::Component {
        explicit StarPanel(InvisDemoPluginEditor& o);
        void showFor(int starIndex);
        void hide() { setVisible(false); }

        void paint(juce::Graphics& g) override;
        void resized() override;

        InvisDemoPluginEditor& owner;
        int index { -1 };

        invis::ui::InvisCellSelector effectCell;
        invis::ui::InvisKnob sensitivityKnob;
        juce::OwnedArray<invis::ui::InvisButton> swatches;
        invis::ui::InvisSeparator divider;
    };

    StarPanel starPanel { *this };
    static constexpr int kStarPanelWidth  = 190;
    static constexpr int kStarPanelHeight = 214;

    // Effect slots the pad hands out as nodes are added. Placeholder identities for now - the
    // real effects arrive one at a time.
    int nextEffectSlot { 0 };

    invis::ui::InvisKnob xsDemoKnob;
    invis::ui::InvisKnob trimKnob;
    invis::ui::InvisKnob outputKnob;

    juce::Slider hiddenTrimSlider;
    juce::Slider hiddenOutputSlider;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> trimAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outputAttachment;

    std::vector<invis::ui::LEDColorPreset> colorPresets {
        invis::ui::LEDColorPreset::NeonCyan,
        invis::ui::LEDColorPreset::WarmAmber,
        invis::ui::LEDColorPreset::ElectricViolet,
        invis::ui::LEDColorPreset::EmeraldPhosphor,
        invis::ui::LEDColorPreset::CrimsonRuby,
        invis::ui::LEDColorPreset::IceWhite,
        invis::ui::LEDColorPreset::CobaltBlue
    };
    juce::OwnedArray<juce::TextButton> colorButtons;

    PanelTheme currentPanelTheme { PanelTheme::DarkSlateCharcoal };
    juce::OwnedArray<juce::TextButton> panelThemeButtons;

    void updateAllLedPresets(invis::ui::LEDColorPreset preset);

    juce::Image chassisBgImage;
    juce::Image knobCapImage;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InvisDemoPluginEditor)
};
