#include "MidiLearnModule.h"

namespace invis::modules {

MidiLearnModule::MidiLearnModule()
{
    for (auto& cell : ccToParam) cell.store(-1, std::memory_order_relaxed);
}

void MidiLearnModule::prepare(juce::AudioProcessorValueTreeState& apvts)
{
    // Snapshotted once and never resized afterwards, which is what lets the callback index it
    // without a lock: a parameter list is fixed the moment the processor is constructed.
    params.clear();

    for (auto* p : apvts.processor.getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(p))
            params.push_back(ranged);
}

int MidiLearnModule::indexOf(const juce::String& parameterID) const
{
    for (size_t i = 0; i < params.size(); ++i)
        if (params[i]->paramID == parameterID) return static_cast<int>(i);

    return -1;
}

void MidiLearnModule::startLearning(const juce::String& parameterID)
{
    learnTarget.store(indexOf(parameterID), std::memory_order_release);
}

void MidiLearnModule::bindCcToParameter(int cc, const juce::String& parameterID)
{
    if (cc < 0 || cc >= kNumCc) return;
    ccToParam[static_cast<size_t>(cc)].store(indexOf(parameterID), std::memory_order_release);
}

void MidiLearnModule::unbindCc(int cc)
{
    if (cc < 0 || cc >= kNumCc) return;
    ccToParam[static_cast<size_t>(cc)].store(-1, std::memory_order_release);
}

void MidiLearnModule::unbindParameter(const juce::String& parameterID)
{
    const int index = indexOf(parameterID);
    if (index < 0) return;

    for (auto& cell : ccToParam)
        if (cell.load(std::memory_order_acquire) == index)
            cell.store(-1, std::memory_order_release);
}

int MidiLearnModule::getCcForParameter(const juce::String& parameterID) const
{
    const int index = indexOf(parameterID);
    if (index < 0) return -1;

    for (int cc = 0; cc < kNumCc; ++cc)
        if (ccToParam[static_cast<size_t>(cc)].load(std::memory_order_acquire) == index) return cc;

    return -1;
}

void MidiLearnModule::processMidi(const juce::MidiBuffer& midiBuffer,
                                  juce::AudioProcessorValueTreeState& apvts)
{
    juce::ignoreUnused(apvts);

    for (const auto metadata : midiBuffer)
    {
        const auto msg = metadata.getMessage();
        if (!msg.isController()) continue;

        const int cc = msg.getControllerNumber();
        if (cc < 0 || cc >= kNumCc) continue;

        // LEARNING COMPLETES HERE, because here is where the controller actually arrives. All it
        // writes is one int into one atomic cell - no allocation, no string, nothing to lock.
        if (const int arming = learnTarget.exchange(-1, std::memory_order_acq_rel); arming >= 0)
            ccToParam[static_cast<size_t>(cc)].store(arming, std::memory_order_release);

        const int index = ccToParam[static_cast<size_t>(cc)].load(std::memory_order_acquire);
        if (index < 0 || index >= static_cast<int>(params.size())) continue;

        auto* param = params[static_cast<size_t>(index)];
        const float normValue = static_cast<float>(msg.getControllerValue()) / 127.0f;

        // Gestures around it, so a host in Touch or Latch records a controller move the same way
        // it records a mouse move. Without them an automation pass driven by a knob writes nothing.
        param->beginChangeGesture();
        param->setValueNotifyingHost(normValue);
        param->endChangeGesture();
    }
}

const juce::Identifier& MidiLearnModule::getStateType()
{
    static const juce::Identifier type { "MIDIMAP" };
    return type;
}

juce::ValueTree MidiLearnModule::toValueTree() const
{
    juce::ValueTree tree { getStateType() };

    for (int cc = 0; cc < kNumCc; ++cc)
    {
        const int index = ccToParam[static_cast<size_t>(cc)].load(std::memory_order_acquire);
        if (index < 0 || index >= static_cast<int>(params.size())) continue;

        juce::ValueTree bind { "BIND" };
        bind.setProperty("cc", cc, nullptr);

        // Written as the parameter ID, not the index. An index is a position in a list that a
        // future version will reorder, and a binding that silently moves to a neighbouring control
        // is worse than one that is simply dropped.
        bind.setProperty("param", params[static_cast<size_t>(index)]->paramID, nullptr);
        tree.appendChild(bind, nullptr);
    }

    return tree;
}

void MidiLearnModule::restoreFromValueTree(const juce::ValueTree& tree)
{
    for (auto& cell : ccToParam) cell.store(-1, std::memory_order_release);

    if (!tree.hasType(getStateType())) return;

    for (int i = 0; i < tree.getNumChildren(); ++i)
    {
        const auto child = tree.getChild(i);
        if (!child.hasType("BIND")) continue;

        bindCcToParameter(static_cast<int>(child.getProperty("cc", -1)),
                          child.getProperty("param", "").toString());
    }
}

} // namespace invis::modules
