#pragma once

#include "PluginProcessor.h"
#include <invis_core/invis_core.h>
#include <optional>

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
    /** Height of a titled panel's heading strip, shared by every panel in the workspace. */
    static constexpr int kPanelHeadingHeight = 12;

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

    /** One star, straight to the sky, effect picked for you. */
    void addRandomStar();

    /** Offers the catalogue, and only creates the star once something is chosen. */
    void chooseEffectThen(std::optional<juce::Point<float>> at);

    /** Chart -> state tree, so compare slots and host save actually contain the instrument. */
    void pushChartToState();
    void pullChartFromState();

    // Restoring writes to the chart, which reports a change, which would write straight back over
    // the state being restored. One flag, because the loop is one call deep.
    bool restoringChart { false };

    // The frame arrives assembled and already wired to the audio side.
    invis::modules::InvisChassisUI chassis;
    Workspace workspace { *this };

    const juce::Point<int> designSize {
        invis::modules::InvisChassisUI::getDesignSize({ kWorkspaceWidth, kWorkspaceHeight })
    };

    invis::ui::InvisConstellation constellation;

    /**
     * TOOLS ON THE GLASS, not furniture around it.
     *
     * These belong to the chart, so a titled panel off to the side made you leave the field to
     * reach them - and a bare row above it gave the sky a lid. Floating them over the corner keeps
     * them within reach of the thing they act on while costing the chart no room at all: they sit
     * back at low opacity and come forward when the cursor arrives.
     */
    struct SkyTools : juce::Component {
        explicit SkyTools(InvisDemoPluginEditor& o);
        void resized() override;

        void mouseEnter(const juce::MouseEvent&) override { setAlpha(1.0f); }
        void mouseExit(const juce::MouseEvent&) override  { setAlpha(kRestAlpha); }

        static constexpr float kRestAlpha = 0.42f;

        InvisDemoPluginEditor& owner;
        invis::ui::InvisCellSelector channelModeCell;
        invis::ui::InvisButton addStarButton;      // asks which effect
        invis::ui::InvisButton addRandomButton;    // does not
        invis::ui::InvisButton shuffleStarsButton;
        invis::ui::InvisButton shuffleObserverButton;
    };

    /** The chassis's own dropdown - a menu here is part of the instrument, not of the host. */
    invis::ui::InvisPopupLookAndFeel popupLook;

    SkyTools skyTools { *this };
    static constexpr int kSkyToolsHeight = 22;
    static constexpr int kSkyToolsWidth  = 400;

    /**
     * Inspector for one star. Lives in the PLUGIN, not in the atom: choosing which effect a star
     * is, and what colour it wears, is product knowledge the constellation atom must not carry.
     */
    struct StarPanel : juce::Component {
        explicit StarPanel(InvisDemoPluginEditor& o);
        static void paintPanelShell(juce::Graphics& g, juce::Rectangle<float> bounds,
                                    const juce::String& heading);
        void showFor(int starIndex);
        void hide() { setVisible(false); }

        void paint(juce::Graphics& g) override;
        void resized() override;

        InvisDemoPluginEditor& owner;
        int index { -1 };

        void refreshFromNode();

        invis::ui::InvisCellSelector effectCell;

        // The BLOCK's controls, not the effect's - see ConstellationNode. The dry tap is taken
        // before the filters, so HPF/LPF shape only what feeds the effect.
        invis::ui::InvisKnob sensitivityKnob;
        invis::ui::InvisKnob dryWetKnob;
        invis::ui::InvisKnob hpfKnob;
        invis::ui::InvisKnob lpfKnob;
        invis::ui::InvisButton deleteButton;
    };

    // ALWAYS PRESENT, top right of the workspace. It used to float over the chart and appear only
    // on a click, which put the editing controls on top of the thing being edited and made the
    // chart jump about under the cursor. A prepared panel outside the field just fills in.
    StarPanel starPanel { *this };
    static constexpr int kStarPanelWidth  = 200;
    static constexpr int kStarPanelHeight = 330;

    PanelTheme currentPanelTheme { PanelTheme::DarkSlateCharcoal };

    juce::Image chassisBgImage;
    juce::Image knobCapImage;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InvisDemoPluginEditor)
};
