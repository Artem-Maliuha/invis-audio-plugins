#pragma once

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

namespace invis::modules {

/**
 * Hierarchical preset catalogue: categories, subcategories, presets.
 *
 * ONE recursive node type rather than separate Category / SubCategory / Preset structs. A fixed
 * three-level hierarchy is a guess about how someone will organise their library, and it is the
 * kind of guess that gets expensive later - "Bass / Electric / Fingered / Bright" is four levels
 * and perfectly reasonable. A node with children is a folder; a node without is a preset.
 *
 * This is a CATALOGUE, not storage. The module renders it and reports what was chosen; what a
 * preset actually contains, and where it lives on disk, stays with the host plugin.
 */
struct PresetNode {
    juce::String name;
    std::vector<PresetNode> children;

    bool isFolder() const { return !children.empty(); }

    static PresetNode folder(juce::String n, std::vector<PresetNode> kids)
    {
        return { std::move(n), std::move(kids) };
    }

    static PresetNode preset(juce::String n) { return { std::move(n), {} }; }
};

/** A preset flattened out of the tree, carrying the path it was found at. */
struct FlatPreset {
    juce::String name;   // leaf name, e.g. "Fingered Bright"
    juce::String path;   // full path,  e.g. "Bass/Electric/Fingered Bright"
};

/**
 * Depth-first flatten. Prev/next stepping walks THIS order, so moving through presets with the
 * arrows follows the same sequence the menu shows - a user who has just seen the tree can predict
 * where the next arrow press lands.
 */
inline void flattenPresetTree(const PresetNode& node,
                              std::vector<FlatPreset>& out,
                              const juce::String& prefix = {})
{
    for (const auto& child : node.children)
    {
        const juce::String path = prefix.isEmpty() ? child.name : prefix + "/" + child.name;

        if (child.isFolder())
            flattenPresetTree(child, out, path);
        else
            out.push_back({ child.name, path });
    }
}

/**
 * Builds a PopupMenu mirroring the tree. Leaf ids are 1-based indices into the flattened list,
 * so the menu and the arrows address exactly the same things.
 */
inline void buildPresetMenu(const PresetNode& node,
                            juce::PopupMenu& menu,
                            const std::vector<FlatPreset>& flat,
                            int selectedIndex,
                            const juce::String& prefix = {})
{
    for (const auto& child : node.children)
    {
        const juce::String path = prefix.isEmpty() ? child.name : prefix + "/" + child.name;

        if (child.isFolder())
        {
            juce::PopupMenu sub;
            buildPresetMenu(child, sub, flat, selectedIndex, path);
            menu.addSubMenu(child.name, sub);
        }
        else
        {
            int index = -1;
            for (size_t i = 0; i < flat.size(); ++i)
                if (flat[i].path == path) { index = static_cast<int>(i); break; }

            if (index >= 0)
                menu.addItem(index + 1, child.name, true, index == selectedIndex);
        }
    }
}

} // namespace invis::modules
