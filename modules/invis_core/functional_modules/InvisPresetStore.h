#pragma once

#include "InvisPresetTree.h"
#include "../ui_atoms/InvisConstellation.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace invis::modules {

/**
 * WHERE PRESETS LIVE, AND WHAT THEY ARE.
 *
 * ── THE FORMAT ────────────────────────────────────────────────────────────────────────────────
 *
 * One XML file per preset, holding the plugin's whole state tree. That is nearly free here: the
 * chart already lives as a child of that tree, so a preset carries the parameters AND the
 * constellation without a second serialiser to keep in step. It is also readable, and unknown
 * properties are ignored on load - so a preset written today survives a field added tomorrow.
 *
 * ── WHERE USER PRESETS GO ─────────────────────────────────────────────────────────────────────
 *
 * NOT Documents. Cloud sync (iCloud, OneDrive) reaches into it and turns restoring a library into
 * a support case, and people simply do not want plugin folders there.
 *
 * NOT Application Support. A system-wide install - which AAX requires - leaves it unwritable
 * without elevated rights, so the first save fails on exactly the machines that matter.
 *
 * macOS uses ~/Music, which is the location that survives a SANDBOXED AU host; Windows uses
 * Documents, where it is the norm and where AppData would simply hide the library from its owner.
 *
 * ── WHY THE FACTORY BANK IS IN THE CODE ───────────────────────────────────────────────────────
 *
 * Not shipped as files by an installer. Built in the binary and written out on first run, so
 * nothing between here and the user can corrupt it - not an installer, not an update, not a
 * cleanup - and "restore factory" therefore always works, because the source of truth never left
 * the plugin.
 *
 * ── THE TREE IS THE FOLDERS ───────────────────────────────────────────────────────────────────
 *
 * Sub-folders ARE categories. The user organises a library by making folders, and needs no UI from
 * us to do it.
 */
class InvisPresetStore {
public:
    InvisPresetStore(juce::AudioProcessorValueTreeState& apvts,
                     const juce::String& manufacturer,
                     const juce::String& product);

    /** Materialises the factory bank if it is missing, then scans. Safe to call repeatedly. */
    void refresh();

    /** Overwrites the factory folder from the built-in bank and rescans. */
    void restoreFactory();

    const PresetNode& getTree() const { return tree; }
    const std::vector<juce::File>& getFlatFiles() const { return flatFiles; }

    /** Loads by index into the flattened tree - the same order the menu shows. */
    bool load(int flatIndex);

    /** Writes the current state under `category/name`. Creates the folder if needed. */
    bool save(const juce::String& category, const juce::String& name);

    juce::File getUserFolder() const { return root; }

private:
    void writeFactoryBank();
    void scan();

    juce::AudioProcessorValueTreeState& apvtsRef;
    juce::File root;

    PresetNode tree;
    std::vector<juce::File> flatFiles;
};

/**
 * A FACTORY CONSTELLATION.
 *
 * Real asterisms, and not as decoration: the shape a constellation actually has decides what it is
 * good for. Corona Borealis is a closed arc, so it becomes a parallel bank; Draco is a long winding
 * chain, so it becomes a series of spaces you walk through; Gemini is two parallel runs, so it
 * becomes a pair of modulators you can lean between.
 */
struct FactoryStar { const char* effect; float x, y, sensitivity; };
struct FactoryLink { int a, b; };

struct FactoryPreset {
    const char* category;
    const char* name;
    const char* note;                       // what the shape is doing, for the header
    std::vector<FactoryStar> stars;
    std::vector<FactoryLink> links;
    float observerX, observerY;
};

const std::vector<FactoryPreset>& getFactoryPresets();

} // namespace invis::modules
