#pragma once

#include "PluginProcessor.h"
#include <invis_core/invis_core.h>

/**
 * The whole editor.
 *
 * A chassis, a workspace, and the lines that introduce them. That this is the entire file is the
 * point of everything in invis_core: the instrument, the frame and the wiring between them are
 * shared, so a plugin is the choice of what goes in the middle and nothing else.
 */
class ConstellationRoyalEditor : public juce::AudioProcessorEditor {
public:
    explicit ConstellationRoyalEditor(ConstellationRoyalProcessor& p);
    ~ConstellationRoyalEditor() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    /** Root design-pixel canvas. Holds every child; the editor only zooms it. */
    struct Canvas : juce::Component {
        explicit Canvas(ConstellationRoyalEditor& o) : owner(o) { setInterceptsMouseClicks(false, true); }
        void paint(juce::Graphics& g) override { owner.paintCanvas(g); }
        void resized() override { owner.layoutCanvas(); }
        ConstellationRoyalEditor& owner;
    };

    void paintCanvas(juce::Graphics& g);
    void layoutCanvas();

    Canvas canvas { *this };
    ConstellationRoyalProcessor& processorRef;

    invis::modules::InvisChassisUI chassis;
    invis::modules::ConstellationWorkspace workspace;

    // The library. Owned by the editor because it needs a live state tree to build a factory
    // preset from, and the editor is where one is guaranteed to exist.
    invis::modules::InvisPresetStore presets;

    void refreshPresetTree();

    const juce::Point<int> designSize {
        invis::modules::InvisChassisUI::getDesignSize(
            invis::modules::ConstellationWorkspace::getDesignSize())
    };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ConstellationRoyalEditor)
};
