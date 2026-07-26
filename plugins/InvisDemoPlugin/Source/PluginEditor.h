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

class InvisDemoPluginEditor : public juce::AudioProcessorEditor {
public:
    // FIXED DESIGN RESOLUTION. The entire editor is authored once at this size in design pixels;
    // resizing is a pure zoom applied to a single root canvas. No percentage layout anywhere.
    // The bench declares the size of ITS OWN INSTRUMENT and nothing else. The panel around it is
    // the chassis's business, so a frame that grows a row does not send every plugin hunting for
    // its own dimensions again.
    static constexpr int kWorkspaceWidth  = 736;
    static constexpr int kWorkspaceHeight = 560;

    explicit InvisDemoPluginEditor(InvisDemoPluginProcessor& p);
    ~InvisDemoPluginEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
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

    /** Everything BETWEEN the ends - the only part of this editor that is the bench's own. */
    struct Workspace : juce::Component {
        explicit Workspace(InvisDemoPluginEditor& o) : owner(o) {}
        void resized() override { owner.layoutWorkspace(); }
        InvisDemoPluginEditor& owner;
    };

    void layoutWorkspace();

    // The frame arrives assembled and already wired to the audio side.
    invis::modules::InvisChassisUI chassis;
    Workspace workspace { *this };

    const juce::Point<int> designSize {
        invis::modules::InvisChassisUI::getDesignSize({ kWorkspaceWidth, kWorkspaceHeight })
    };

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
