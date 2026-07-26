#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>
#include <vector>

namespace invis::modules {

/**
 * MIDI LEARN - continuous controllers bound to parameters.
 *
 * ── NO STRINGS ON THE AUDIO THREAD ────────────────────────────────────────────────────────────
 *
 * The table is parameter INDICES, not IDs, and every cell is an atomic int. It has to be: learning
 * is armed from the message thread and completed by whatever controller arrives next, which happens
 * on the audio thread - so by definition the two touch the same table. The first version kept a map
 * of juce::String and mutated it from the callback, which is a data race and an allocation on the
 * same line.
 *
 * Resolving an ID to an index happens once, on the message thread, where it costs nothing.
 *
 * ── WHY BINDINGS ARE STATE, NOT SETTINGS ──────────────────────────────────────────────────────
 *
 * They travel in the plugin's state tree, so they follow the session and the preset rather than
 * living in one machine-wide file. Two instances on two tracks driven by two different controllers
 * is the normal case, and a global binding list cannot express it.
 */
class MidiLearnModule {
public:
    static constexpr int kNumCc = 128;

    MidiLearnModule();

    /** Message thread, once. Snapshots the parameter list so the callback needs no lookups. */
    void prepare(juce::AudioProcessorValueTreeState& apvts);

    /** Arms learning. The next controller to arrive takes the binding. */
    void startLearning(const juce::String& parameterID);
    void cancelLearning() { learnTarget.store(-1, std::memory_order_release); }
    bool isLearning() const { return learnTarget.load(std::memory_order_acquire) >= 0; }

    void bindCcToParameter(int ccNumber, const juce::String& parameterID);
    void unbindCc(int ccNumber);
    void unbindParameter(const juce::String& parameterID);

    /** -1 when nothing is bound to it. */
    int getCcForParameter(const juce::String& parameterID) const;

    /** Audio thread. */
    void processMidi(const juce::MidiBuffer& midiBuffer, juce::AudioProcessorValueTreeState& apvts);

    static const juce::Identifier& getStateType();
    juce::ValueTree toValueTree() const;
    void restoreFromValueTree(const juce::ValueTree& tree);

private:
    int indexOf(const juce::String& parameterID) const;

    std::vector<juce::RangedAudioParameter*> params;   // fixed after prepare()
    std::array<std::atomic<int>, kNumCc> ccToParam;
    std::atomic<int> learnTarget { -1 };
};

} // namespace invis::modules
