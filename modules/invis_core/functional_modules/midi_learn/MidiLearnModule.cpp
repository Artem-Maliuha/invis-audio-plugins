#include "MidiLearnModule.h"

namespace invis::modules {

void MidiLearnModule::startLearning(const juce::String& parameterID)
{
    targetParameterForLearning = parameterID;
}

void MidiLearnModule::cancelLearning()
{
    targetParameterForLearning.reset();
}

void MidiLearnModule::bindCcToParameter(int ccNumber, const juce::String& parameterID)
{
    ccToParamMap[ccNumber] = parameterID;
}

void MidiLearnModule::unbindCc(int ccNumber)
{
    ccToParamMap.erase(ccNumber);
}

void MidiLearnModule::processMidi(const juce::MidiBuffer& midiBuffer, juce::AudioProcessorValueTreeState& apvts)
{
    for (const auto metadata : midiBuffer)
    {
        const auto msg = metadata.getMessage();
        if (!msg.isController()) continue;

        const int cc = msg.getControllerNumber();
        const float normValue = msg.getControllerValue() / 127.0f;

        // If in MIDI Learn mode, bind this CC to the active parameter
        if (targetParameterForLearning.has_value())
        {
            bindCcToParameter(cc, *targetParameterForLearning);
            targetParameterForLearning.reset();
        }

        // Apply CC value to bound parameter
        auto it = ccToParamMap.find(cc);
        if (it != ccToParamMap.end())
        {
            if (auto* param = apvts.getParameter(it->second))
            {
                param->beginChangeGesture();
                param->setValueNotifyingHost(normValue);
                param->endChangeGesture();
            }
        }
    }
}

} // namespace invis::modules
