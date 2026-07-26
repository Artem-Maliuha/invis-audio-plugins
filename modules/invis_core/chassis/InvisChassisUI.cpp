#include "InvisChassisUI.h"

namespace invis::modules {

InvisChassisUI::InvisChassisUI(InvisChassisDSP& dsp, juce::AudioProcessorValueTreeState& apvts)
    : dspRef(dsp),
      topUI(apvts, InvisChassisDSP::kTopPrefix, InvisChassisDSP::kOsPrefix),
      inputUI(apvts, InvisChassisDSP::kInputPrefix),
      outputUI(apvts, InvisChassisDSP::kOutputPrefix)
{
    // Added here, not by the plugin. The bench spent this whole session laying the sidebars out
    // correctly and never seeing them, because they had bounds but no parent.
    addAndMakeVisible(topUI);
    addAndMakeVisible(inputUI);
    addAndMakeVisible(outputUI);

    // EVERY CALLBACK, WIRED ONCE. The sidebars raised all of these and nothing on the other end
    // was listening: clicking RMS or PEAK on a meter did nothing at all, and the AUTO routines
    // asked for the analyser reset their own verify phase depends on and never got it.
    inputUI.onRequestMeterReset = [this]() { dspRef.getInput().requestAnalyserReset(); };
    inputUI.onRequestRmsReset   = [this]() { dspRef.getInput().requestRmsReset(); };
    inputUI.onRequestPeakReset  = [this]() { dspRef.getInput().requestPeakReset(); };
    inputUI.onRequestLufsModeCycle = [this]()
    {
        inputUI.setLufsLabel(getLufsModeName(dspRef.getInput().cycleLufsMode()));
    };

    outputUI.onRequestMeterReset = [this]() { dspRef.getOutput().requestAnalyserReset(); };
    outputUI.onRequestRmsReset   = [this]() { dspRef.getOutput().requestRmsReset(); };
    outputUI.onRequestPeakReset  = [this]() { dspRef.getOutput().requestPeakReset(); };
    outputUI.onRequestLufsModeCycle = [this]()
    {
        outputUI.setLufsLabel(getLufsModeName(dspRef.getOutput().cycleLufsMode()));
    };

    topUI.onStateReplaced = [this]() { if (onStateRestored) onStateRestored(); };

    inputUI.setLufsLabel(getLufsModeName(dspRef.getInput().getLufsMode()));
    outputUI.setLufsLabel(getLufsModeName(dspRef.getOutput().getLufsMode()));

    // ITS OWN CLOCK. Handing the pumping to the plugin is what made every one of the faults above
    // possible; a frame that keeps itself alive cannot be forgotten into silence.
    startTimerHz(60);
}

InvisChassisUI::~InvisChassisUI()
{
    stopTimer();
}

void InvisChassisUI::enableMidiLearn(MidiLearnModule& module)
{
    midiLearn = &module;

    // Each knob names its own parameter, because only the chassis knows which one it drives.
    midiControls = {
        { &inputUI.getTrimKnob(),    juce::String(InvisChassisDSP::kInputPrefix)  + "trim" },
        { &inputUI.getHpfKnob(),     juce::String(InvisChassisDSP::kInputPrefix)  + "hpf_freq" },
        { &inputUI.getLpfKnob(),     juce::String(InvisChassisDSP::kInputPrefix)  + "lpf_freq" },
        { &outputUI.getOutputKnob(), juce::String(InvisChassisDSP::kOutputPrefix) + "gain" },
    };

    for (const auto& [knob, id] : midiControls)
        knob->onSecondaryClick = [this, knob, id]() { showMidiMenu(*knob, id); };

    lastMidiVersion = -1;   // force one refresh, so restored bindings show immediately
}

void InvisChassisUI::showMidiMenu(juce::Component& anchor, const juce::String& parameterID)
{
    if (midiLearn == nullptr) return;

    const int bound = midiLearn->getCcForParameter(parameterID);

    juce::PopupMenu menu;
    menu.addSectionHeader(bound >= 0 ? "MIDI CC " + juce::String(bound) : juce::String("MIDI"));
    menu.addItem(1, midiLearn->isLearning() ? "Cancel learn" : "MIDI learn...");
    if (bound >= 0) menu.addItem(2, "Forget CC " + juce::String(bound));

    menu.setLookAndFeel(&popupLook);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&anchor)
                                                 .withMinimumWidth(150)
                                                 .withStandardItemHeight(19),
                       [this, parameterID](int result)
    {
        if (midiLearn == nullptr) return;

        if (result == 1)
        {
            // Arming is a TOGGLE. A learn you cannot call off leaves the next stray controller -
            // a mod wheel knocked on the way past - bound to something you never meant.
            if (midiLearn->isLearning()) midiLearn->cancelLearning();
            else                         midiLearn->startLearning(parameterID);
        }
        else if (result == 2)
        {
            midiLearn->unbindParameter(parameterID);
        }
    });
}

void InvisChassisUI::setWorkspace(juce::Component& content)
{
    workspace = &content;
    addAndMakeVisible(content);
    resized();
}

void InvisChassisUI::resized()
{
    auto area = getLocalBounds().reduced(kOuterMargin);

    // The top sidebar spans the FULL width above the channel strips: everything it carries is
    // plugin-global rather than signal-path, so it outranks the left and right sidebars.
    topUI.setBounds(area.removeFromTop(TopSidebarUI::getIntrinsicHeight()));
    area.removeFromTop(ui::layout::kGapM);

    const int sidebarWidth = InputSidebarUI::getIntrinsicWidth();

    inputUI.setBounds(area.removeFromLeft(sidebarWidth));
    outputUI.setBounds(area.removeFromRight(sidebarWidth));
    area.removeFromLeft(ui::layout::kGapM);
    area.removeFromRight(ui::layout::kGapM);

    jassert(area.getHeight() >= InputSidebarUI::getMinimumHeight());

    workspaceBounds = area;
    if (workspace != nullptr) workspace->setBounds(workspaceBounds);
}

void InvisChassisUI::timerCallback()
{
    auto& in = dspRef.getInput();
    auto& out = dspRef.getOutput();

    inputUI.updateLevels(in.getPeakLevelL(), in.getPeakLevelR());
    inputUI.setReadouts(in.getRmsDb(), in.getPeakDb(), in.getLufsDb());
    inputUI.updateFilterLamps(in.getHpfEnergyRemoved(), in.isHpfEngaged(),
                              in.getLpfEnergyRemoved(), in.isLpfEngaged());

    outputUI.updateLevels(out.getPeakLevelL(), out.getPeakLevelR());
    outputUI.setReadouts(out.getRmsDb(), out.getPeakDb(), out.getLufsDb());

    // The gain-stage target is a top-sidebar concern that BOTH auto routines calibrate toward and
    // both meters draw as a reference line. One source, pushed to everything that reads it.
    const float gainStageDb = topUI.getGainStageTargetDb();
    inputUI.setGainStageTargetDb(gainStageDb);
    outputUI.setGainStageTargetDb(gainStageDb);

    // BADGES, ON CHANGE ONLY. A learn completes in the audio callback, which the UI has no other
    // way of hearing about - but asking every control every frame means a table scan per knob per
    // frame for something that changes a handful of times in a session.
    if (midiLearn != nullptr && midiLearn->getVersion() != lastMidiVersion)
    {
        lastMidiVersion = midiLearn->getVersion();

        for (const auto& [knob, id] : midiControls)
            knob->setMidiCc(midiLearn->getCcForParameter(id));
    }

    const float dt = 1.0f / 60.0f;
    topUI.tickAnimations(dt);
    inputUI.tickAnimations(dt);
    outputUI.tickAnimations(dt);

    if (onTick) onTick(dt);
}

} // namespace invis::modules
