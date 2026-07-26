#pragma once

#include "../../ui_atoms/InvisConstellation.h"
#include "../../ui_atoms/InvisKnob.h"
#include "../../ui_atoms/InvisButton.h"
#include "../../ui_atoms/InvisCellSelector.h"
#include "../../design_system/InvisPopupLookAndFeel.h"
#include "../../design_system/InvisEffectPalette.h"
#include "../../effects/InvisConstellationEngine.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <optional>

namespace invis::modules {

/**
 * THE CONSTELLATION INSTRUMENT, as a component a plugin can simply own.
 *
 * Everything between the chassis ends: the chart, the tools floating on it, the star inspector and
 * the effect panel - plus the wiring that keeps all three agreeing with the DSP and with the state
 * tree. It lived in the bench, which meant the first real plugin would have started by copying
 * nine hundred lines and then drifting from them.
 *
 * A plugin gives it an engine and a state tree and is finished. It reports the design size it wants
 * so the chassis can work out the panel around it.
 */
class ConstellationWorkspace : public juce::Component {
public:
    /** Height of a titled panel's heading strip, shared by every panel in here. */
    static constexpr int kPanelHeadingHeight = 12;

    static constexpr int kWidth  = 780;
    static constexpr int kHeight = 560;

    static juce::Point<int> getDesignSize() { return { kWidth, kHeight }; }

    ConstellationWorkspace(dsp::InvisConstellationEngine& engine,
                           juce::AudioProcessorValueTreeState& apvts);
    ~ConstellationWorkspace() override;

    void resized() override;

    /** Advance the chart's animation. Drive it from the chassis clock. */
    void tick(float dt) { constellation.tickAnimation(dt); }

    /** After A/B/C or a preset: the chart lives in the state tree and has to come back with it. */
    void reloadFromState();

    ui::InvisConstellation& getChart() { return constellation; }

private:
    ui::InvisConstellation constellation;

    /**
     * TOOLS ON THE GLASS, not furniture around it.
     *
     * These belong to the chart, so a titled panel off to the side made you leave the field to
     * reach them - and a bare row above it gave the sky a lid. Floating them over the corner keeps
     * them within reach of the thing they act on while costing the chart no room at all: they sit
     * back at low opacity and come forward when the cursor arrives.
     */
    struct SkyTools : juce::Component {
        explicit SkyTools(ConstellationWorkspace& o);
        void paint(juce::Graphics& g) override;
        void resized() override;

        // BOTH ROUTE THROUGH ONE PLACE. Setting the alpha directly in each handler made the row
        // flicker: moving the cursor from the panel ONTO A BUTTON is an exit as far as the parent
        // is concerned, so it faded out at the exact moment you reached for something. Asking
        // whether the mouse is over this component OR ANY CHILD is the only question worth asking.
        void mouseEnter(const juce::MouseEvent&) override { updateAlpha(); }
        void mouseExit(const juce::MouseEvent&) override  { updateAlpha(); }
        void updateAlpha() { setAlpha(isMouseOver(true) ? 1.0f : kRestAlpha); }

        /**
         * Exactly as wide as its contents, measured rather than guessed.
         *
         * A hand-tuned constant clipped "MODE:" off the left end the moment the labels grew, and
         * would do it again on the next rename - the row is built from intrinsic sizes and text
         * widths, so it is the only thing that can answer this correctly.
         */
        /** Width of the two-row layout; `singleRowWidth` reports what one row would need. */
        int getRequiredWidth(int* singleRowWidth = nullptr) const;

        /** Greys the ADD keys and updates the count once the chart is full. */
        void refreshLimit();

        static constexpr float kRestAlpha = 0.42f;
        static constexpr int kChannelCellWidth = 54;
        static constexpr int kSkyToolsRowHeight = 22;

        ConstellationWorkspace& owner;
        ui::InvisCellSelector channelModeCell;
        ui::InvisButton addStarButton;      // asks which effect
        ui::InvisButton addRandomButton;    // does not
        ui::InvisButton shuffleStarsButton;
        ui::InvisButton shuffleSensButton;
        ui::InvisButton shuffleObserverButton;
        ui::InvisButton deleteUnusedButton;
        ui::InvisButton deleteLinksButton;

        // Filled by resized(), read by paint(): a caption sits beside the group it names, and only
        // the layout knows where that ended up.
        juce::Rectangle<int> modeCaption, addCaption, randomCaption, deleteCaption;
    };

    /** The chassis's own dropdown - a menu here is part of the instrument, not of the host. */
    ui::InvisPopupLookAndFeel popupLook;

    SkyTools skyTools { *this };
    static constexpr int kSkyToolsHeight = 2 * SkyTools::kSkyToolsRowHeight + 4;
    // TWO ROWS. Four groups do not fit across the width of the sky, and a row that overruns just
    // clips its left end off. Splitting by intent - what you MAKE above, what you CHANGE below -
    // is a better division than wrapping wherever the pixels ran out.


    /**
     * Inspector for one star. Lives in the PLUGIN, not in the atom: choosing which effect a star
     * is, and what colour it wears, is product knowledge the constellation atom must not carry.
     */
    struct StarPanel : juce::Component {
        explicit StarPanel(ConstellationWorkspace& o);
        static void paintPanelShell(juce::Graphics& g, juce::Rectangle<float> bounds,
                                    const juce::String& heading);
        void showFor(int starIndex);
        void hide() { setVisible(false); }

        void paint(juce::Graphics& g) override;
        void resized() override;

        ConstellationWorkspace& owner;
        int index { -1 };

        void refreshFromNode();

        ui::InvisCellSelector effectCell;

        // The BLOCK's controls, not the effect's - see ConstellationNode. The dry tap is taken
        // before the filters, so HPF/LPF shape only what feeds the effect.
        ui::InvisKnob sensitivityKnob;
        ui::InvisKnob dryWetKnob;
        ui::InvisKnob hpfKnob;
        ui::InvisKnob lpfKnob;
        ui::InvisButton deleteButton;
    };

    // ALWAYS PRESENT, top right of the workspace. It used to float over the chart and appear only
    // on a click, which put the editing controls on top of the thing being edited and made the
    // chart jump about under the cursor. A prepared panel outside the field just fills in.
    /**
     * What the EFFECT is, as opposed to what the block around it does.
     *
     * The star panel holds the slot: reach, mix, band limiting - the things every star has
     * whatever is dropped into it. An algorithm's own controls are a different kind of thing and
     * change completely from one effect to the next, so they get their own place rather than being
     * appended to a list that would then mean two things at once.
     */
    struct EffectPanel : juce::Component {
        explicit EffectPanel(ConstellationWorkspace& o);
        void paint(juce::Graphics& g) override;
        void resized() override;

        /** Rebuilds the knobs from whatever the selected star's algorithm says it has. */
        void showFor(int starIndex);

        ConstellationWorkspace& owner;
        int index { -1 };
        int numShown { 0 };

        // BUILT FROM DESCRIPTIONS, not hand-written per algorithm. Fourteen catalogue entries with
        // hand-laid panels would be fourteen layouts drifting apart, and product code in the way of
        // every new algorithm.
        std::array<ui::InvisKnob, dsp::kMaxEffectParams> paramKnobs;
    };

    StarPanel starPanel { *this };
    EffectPanel effectPanel { *this };

    // Wider than it was. Two knobs side by side at 200 put their tick labels almost touching,
    // which reads as cramped rather than as small - they were always the standard XS size.
    static constexpr int kStarPanelWidth  = 244;
    static constexpr int kStarPanelHeight = 316;

    void layoutWorkspaceContent();
    void addRandomStar();
    void chooseEffectThen(std::optional<juce::Point<float>> at, juce::Component* anchor = nullptr);
    void pushChartToEngine();
    void pushChartToState();

    dsp::InvisConstellationEngine& engine;
    juce::AudioProcessorValueTreeState& apvtsRef;

    // Restoring writes to the chart, which reports a change, which would write straight back over
    // the state being restored. One flag, because the loop is one call deep.
    bool restoringChart { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ConstellationWorkspace)
};

} // namespace invis::modules
