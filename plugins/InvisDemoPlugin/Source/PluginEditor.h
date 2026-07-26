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
    // STRIPPED TO THE FRAME. The bench carries the universal chassis - top, input and output
    // sidebars - and the one thing under construction. Demo knobs, the filter panel and the
    // theme/colour button rows are gone: they were exercising atoms that have since grown their
    // own homes, and they were the only reason this canvas had to be so large.
    static constexpr int kDesignWidth  = 1100;
    static constexpr int kDesignHeight = 740;

    static constexpr int kOuterMargin  = 12;

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

    // The universal chassis frame
    invis::modules::TopSidebarUI topSidebarUI;
    invis::modules::InputSidebarUI inputSidebarUI;
    invis::modules::OutputSidebarUI outputSidebarUI;

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

    // ALWAYS PRESENT, top right of the workspace. It used to float over the chart and appear only
    // on a click, which put the editing controls on top of the thing being edited and made the
    // chart jump about under the cursor. A prepared panel outside the field just fills in.
    StarPanel starPanel { *this };
    static constexpr int kStarPanelWidth  = 200;
    static constexpr int kStarPanelHeight = 236;

    // Effect slots the pad hands out as nodes are added. Placeholder identities for now - the
    // real effects arrive one at a time.
    int nextEffectSlot { 0 };

    PanelTheme currentPanelTheme { PanelTheme::DarkSlateCharcoal };

    juce::Image chassisBgImage;
    juce::Image knobCapImage;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InvisDemoPluginEditor)
};
