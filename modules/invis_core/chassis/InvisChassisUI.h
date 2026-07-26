#pragma once

#include "InvisChassisDSP.h"
#include "../functional_modules/input_sidebar/InputSidebarUI.h"
#include "../functional_modules/output_sidebar/OutputSidebarUI.h"
#include "../functional_modules/top_sidebar/TopSidebarUI.h"
#include "../design_system/InvisLayout.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace invis::modules {

/**
 * THE CHASSIS PANEL - the frame every plugin in the series wears, already wired.
 *
 * The matching half of InvisChassisDSP. It owns the three sidebars, adds them as children, lays
 * them out, connects every callback back to the audio side, and PUMPS ITSELF on its own timer.
 *
 * That last part is the point. Meters, AUTO routines and lamp ballistics are all pull-based: they
 * do nothing until somebody reads the DSP each frame and pushes the values in. Leaving that to the
 * plugin meant roughly twenty lines of pumping that had to be written correctly, in the right
 * order, in a timer somebody remembered to start - and every one of those lines was a place to
 * silently lose a feature. In the bench the meters were dead, the RMS/PEAK readouts did nothing
 * when clicked, and AUTO could not reset the analyser it depended on, all for exactly that reason.
 *
 * Now there is nothing to remember. Construct it, give it a workspace, and the frame is live.
 *
 * The plugin supplies only what goes BETWEEN the ends: one component, whose bounds this hands out.
 */
class InvisChassisUI : public juce::Component, private juce::Timer {
public:
    static constexpr int kOuterMargin = 12;

    /**
     * Total editor size for a plugin whose own content is `workspace` design pixels.
     *
     * A plugin declares the size of its instrument and the chassis works out the panel, so the
     * frame can grow a row without every plugin having to rediscover its own dimensions.
     */
    static juce::Point<int> getDesignSize(juce::Point<int> workspace)
    {
        const int width = 2 * kOuterMargin
                        + 2 * InputSidebarUI::getIntrinsicWidth()
                        + 2 * ui::layout::kGapM
                        + workspace.x;

        const int height = 2 * kOuterMargin
                         + TopSidebarUI::getIntrinsicHeight()
                         + ui::layout::kGapM
                         + std::max(workspace.y, InputSidebarUI::getMinimumHeight());

        return { width, height };
    }

    InvisChassisUI(InvisChassisDSP& dsp, juce::AudioProcessorValueTreeState& apvts);
    ~InvisChassisUI() override;

    void resized() override;

    /**
     * The plugin's own instrument, between the ends.
     *
     * Handed its bounds by the chassis rather than asked to find them: a workspace that has to
     * measure around the sidebars is a workspace that drifts the moment the frame changes.
     */
    void setWorkspace(juce::Component& content);

    /** Where the plugin's content sits, for anything that needs to paint underneath it. */
    juce::Rectangle<int> getWorkspaceBounds() const { return workspaceBounds; }

    TopSidebarUI& getTop()       { return topUI; }
    InputSidebarUI& getInput()   { return inputUI; }
    OutputSidebarUI& getOutput() { return outputUI; }

    /** Anything the plugin wants advanced on the same clock as the frame. */
    std::function<void(float dt)> onTick;

private:
    void timerCallback() override;

    InvisChassisDSP& dspRef;

    TopSidebarUI topUI;
    InputSidebarUI inputUI;
    OutputSidebarUI outputUI;

    juce::Component* workspace { nullptr };
    juce::Rectangle<int> workspaceBounds;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InvisChassisUI)
};

} // namespace invis::modules
