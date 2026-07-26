#include "InvisPresetStore.h"

namespace invis::modules {

namespace {

constexpr const char* kExtension = ".invispreset";
constexpr const char* kFactoryFolder = "Factory";

juce::File presetRoot(const juce::String& manufacturer, const juce::String& product)
{
    // See the header for why these two and not the obvious ones. In short: Documents is reached
    // into by cloud sync, and Application Support is unwritable after a system-wide install.
   #if JUCE_MAC
    auto base = juce::File::getSpecialLocation(juce::File::userMusicDirectory);
   #else
    auto base = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
   #endif

    return base.getChildFile(manufacturer).getChildFile(product).getChildFile("Presets");
}

/**
 * Builds a preset's chart THROUGH THE REAL CHART.
 *
 * A hand-written ValueTree would have to repeat the property names InvisConstellation writes, and
 * the day one of them changes the factory bank silently loads half a constellation. Driving the
 * component itself and asking it to serialise cannot drift, and costs one off-screen object.
 */
juce::ValueTree buildChart(const FactoryPreset& preset)
{
    ui::InvisConstellation chart;

    for (const auto& s : preset.stars)
    {
        const auto* type = [&]() -> const ui::EffectType* {
            for (const auto& e : ui::getEffectCatalogue())
                if (juce::String(e.name) == s.effect) return &e;
            return nullptr;
        }();

        if (type == nullptr) continue;

        const int index = chart.addNodeAt(type->name, type->getColour(), { s.x, s.y });
        if (index >= 0) chart.setNodeSensitivity(index, s.sensitivity);
    }

    for (const auto& l : preset.links) chart.linkStars(l.a, l.b);

    chart.setObserverPosition(0, { preset.observerX, preset.observerY });
    chart.setObserverPosition(1, { preset.observerX, preset.observerY });

    return chart.toValueTree();
}

} // namespace

// =================================================================================================

InvisPresetStore::InvisPresetStore(juce::AudioProcessorValueTreeState& apvts,
                                   const juce::String& manufacturer,
                                   const juce::String& product)
    : apvtsRef(apvts), root(presetRoot(manufacturer, product))
{
}

void InvisPresetStore::refresh()
{
    if (!root.getChildFile(kFactoryFolder).isDirectory()) writeFactoryBank();
    scan();
}

void InvisPresetStore::restoreFactory()
{
    writeFactoryBank();
    scan();
}

void InvisPresetStore::writeFactoryBank()
{
    // The CURRENT state is the carrier: a factory preset is this plugin with a different chart in
    // it. Building one from nothing would mean inventing every parameter default a second time,
    // in a second place, and letting the two drift.
    const auto base = apvtsRef.copyState();

    for (const auto& preset : getFactoryPresets())
    {
        auto state = base.createCopy();
        state.removeChild(state.getChildWithName(ui::InvisConstellation::getStateType()), nullptr);
        state.appendChild(buildChart(preset), nullptr);

        auto file = root.getChildFile(kFactoryFolder)
                        .getChildFile(preset.category)
                        .getChildFile(juce::String(preset.name) + kExtension);

        file.getParentDirectory().createDirectory();

        if (auto xml = state.createXml()) xml->writeTo(file);
    }
}

void InvisPresetStore::scan()
{
    tree = PresetNode::folder("", {});
    flatFiles.clear();

    if (!root.isDirectory()) return;

    // Depth first, folders before files, so the menu reads in the order the disk is organised.
    std::function<PresetNode(const juce::File&)> walk = [&](const juce::File& dir) -> PresetNode
    {
        std::vector<PresetNode> children;

        for (const auto& sub : juce::RangedDirectoryIterator(dir, false, "*",
                                                             juce::File::findDirectories))
        {
            auto node = walk(sub.getFile());
            if (!node.children.empty()) children.push_back(std::move(node));
        }

        for (const auto& f : juce::RangedDirectoryIterator(dir, false, juce::String("*") + kExtension,
                                                           juce::File::findFiles))
        {
            children.push_back(PresetNode::preset(f.getFile().getFileNameWithoutExtension()));
            flatFiles.push_back(f.getFile());
        }

        return PresetNode::folder(dir.getFileName(), std::move(children));
    };

    tree = walk(root);
    tree.name = {};   // the root is the library itself and needs no heading
}

bool InvisPresetStore::load(int flatIndex)
{
    if (flatIndex < 0 || flatIndex >= static_cast<int>(flatFiles.size())) return false;

    auto xml = juce::XmlDocument::parse(flatFiles[static_cast<size_t>(flatIndex)]);
    if (xml == nullptr || !xml->hasTagName(apvtsRef.state.getType())) return false;

    apvtsRef.replaceState(juce::ValueTree::fromXml(*xml));
    return true;
}

bool InvisPresetStore::save(const juce::String& category, const juce::String& name)
{
    if (name.isEmpty()) return false;

    auto file = root.getChildFile(category.isEmpty() ? "User" : category)
                    .getChildFile(name + kExtension);

    file.getParentDirectory().createDirectory();

    auto state = apvtsRef.copyState();
    if (auto xml = state.createXml()) { xml->writeTo(file); scan(); return true; }

    return false;
}

// =================================================================================================

const std::vector<FactoryPreset>& getFactoryPresets()
{
    // Coordinates run 0..1 across and 0..aspect down - see InvisConstellation::toPixels. The shapes
    // below follow the real asterisms closely enough to be recognised, which is the point: the
    // figure IS the routing, so a constellation that looks like Draco also behaves like a long
    // winding chain.
    static const std::vector<FactoryPreset> bank {

        // ── SPATIAL ───────────────────────────────────────────────────────────────────────────
        { "Spatial", "Draco", "A long winding chain - each space feeds the next",
          { { "HALL",    0.22f, 0.20f, 0.55f }, { "DIGITAL", 0.40f, 0.34f, 0.50f },
            { "CHAMBER", 0.30f, 0.55f, 0.48f }, { "TAPE",    0.52f, 0.70f, 0.52f },
            { "PLATE",   0.72f, 0.58f, 0.45f }, { "ANALOG",  0.80f, 0.34f, 0.42f } },
          { {0,1},{1,2},{2,3},{3,4},{4,5} }, 0.50f, 0.90f },

        { "Spatial", "Cassiopeia", "The W: five rooms in series, entered wherever you stand",
          { { "PLATE",   0.16f, 0.30f, 0.50f }, { "CHAMBER", 0.34f, 0.62f, 0.50f },
            { "HALL",    0.50f, 0.28f, 0.55f }, { "SPRING",  0.68f, 0.64f, 0.48f },
            { "DIGITAL", 0.86f, 0.26f, 0.45f } },
          { {0,1},{1,2},{2,3},{3,4} }, 0.50f, 1.05f },

        { "Spatial", "Lyra", "A closed quadrilateral of delays with Vega hung off it",
          { { "DIGITAL",  0.34f, 0.34f, 0.45f }, { "ANALOG",  0.56f, 0.30f, 0.45f },
            { "TAPE",     0.60f, 0.56f, 0.45f }, { "PINGPONG",0.36f, 0.60f, 0.45f },
            { "HALL",     0.52f, 0.86f, 0.60f } },
          { {0,1},{1,2},{2,3},{3,0},{2,4} }, 0.52f, 1.15f },

        { "Spatial", "Hydra", "The longest figure in the sky, and the longest chain here",
          { { "DIGITAL", 0.14f, 0.22f, 0.42f }, { "PLATE",   0.28f, 0.40f, 0.44f },
            { "ANALOG",  0.42f, 0.30f, 0.44f }, { "CHAMBER", 0.56f, 0.50f, 0.46f },
            { "TAPE",    0.70f, 0.38f, 0.46f }, { "HALL",    0.82f, 0.60f, 0.52f },
            { "SPRING",  0.66f, 0.80f, 0.44f } },
          { {0,1},{1,2},{2,3},{3,4},{4,5},{5,6} }, 0.30f, 1.00f },

        // ── SATURATION - the closed figures. A ring is a parallel bank, and stacking drive in
        //    parallel is how you get weight without the series buildup that turns it to mud.
        { "Saturation", "Corona Borealis", "The crown: a closed arc, so every stage runs at once",
          { { "TAPE SAT", 0.26f, 0.46f, 0.50f }, { "TUBE",   0.36f, 0.32f, 0.50f },
            { "DRIVE",    0.50f, 0.26f, 0.50f }, { "TUBE",   0.64f, 0.32f, 0.50f },
            { "TAPE SAT", 0.74f, 0.46f, 0.50f }, { "CRUSH",  0.50f, 0.52f, 0.40f } },
          { {0,1},{1,2},{2,3},{3,4},{4,5},{5,0} }, 0.50f, 0.72f },

        { "Saturation", "Triangulum", "Three stages, closed - the smallest parallel bank there is",
          { { "TUBE",  0.36f, 0.40f, 0.55f }, { "DRIVE", 0.64f, 0.40f, 0.55f },
            { "TAPE SAT", 0.50f, 0.66f, 0.55f } },
          { {0,1},{1,2},{2,0} }, 0.50f, 0.52f },

        { "Saturation", "Crux", "A closed cross feeding one last stage",
          { { "TUBE",  0.50f, 0.26f, 0.50f }, { "DRIVE", 0.66f, 0.46f, 0.50f },
            { "TAPE SAT", 0.50f, 0.66f, 0.50f }, { "TUBE", 0.34f, 0.46f, 0.50f },
            { "CRUSH", 0.50f, 0.98f, 0.42f } },
          { {0,1},{1,2},{2,3},{3,0},{2,4} }, 0.50f, 1.20f },

        // ── MODULATION ────────────────────────────────────────────────────────────────────────
        { "Modulation", "Gemini", "The twins: two parallel runs to lean between",
          { { "CHORUS",  0.34f, 0.26f, 0.50f }, { "FLANGER", 0.34f, 0.52f, 0.50f },
            { "PHASER",  0.34f, 0.78f, 0.50f },
            { "CHORUS",  0.66f, 0.26f, 0.50f }, { "TREMOLO", 0.66f, 0.52f, 0.50f },
            { "FLANGER", 0.66f, 0.78f, 0.50f } },
          { {0,1},{1,2},{3,4},{4,5} }, 0.50f, 0.52f },

        { "Modulation", "Cygnus", "The swan's cross: a spine of motion with wings either side",
          { { "CHORUS",  0.50f, 0.20f, 0.52f }, { "FLANGER", 0.50f, 0.48f, 0.52f },
            { "PHASER",  0.50f, 0.84f, 0.52f },
            { "TREMOLO", 0.24f, 0.48f, 0.46f }, { "CHORUS",  0.76f, 0.48f, 0.46f } },
          { {0,1},{1,2},{3,1},{1,4} }, 0.50f, 1.05f },

        // ── ROYAL - what this plugin is for: all three families in one figure.
        { "Royal", "Orion", "The hunter: a saturated belt, spaces above, motion below",
          { { "HALL",    0.30f, 0.20f, 0.55f }, { "PLATE",   0.70f, 0.24f, 0.50f },
            { "TUBE",    0.42f, 0.50f, 0.50f }, { "DRIVE",   0.52f, 0.52f, 0.50f },
            { "TAPE SAT",0.62f, 0.54f, 0.50f },
            { "FLANGER", 0.34f, 0.86f, 0.48f }, { "CHORUS",  0.70f, 0.90f, 0.48f } },
          { {0,2},{1,4},{2,3},{3,4},{2,5},{4,6} }, 0.50f, 0.68f },

        { "Royal", "Ursa Major", "The plough: one chain that walks all three families",
          { { "DIGITAL", 0.18f, 0.66f, 0.48f }, { "HALL",    0.32f, 0.58f, 0.50f },
            { "TUBE",    0.46f, 0.56f, 0.50f }, { "DRIVE",   0.58f, 0.62f, 0.50f },
            { "CHORUS",  0.66f, 0.44f, 0.48f }, { "FLANGER", 0.80f, 0.38f, 0.48f },
            { "PLATE",   0.88f, 0.56f, 0.52f } },
          { {0,1},{1,2},{2,3},{3,4},{4,5},{5,6} }, 0.42f, 0.98f },

        { "Royal", "Perseus", "A closed heart of drive with spaces and motion branching off it",
          { { "TUBE",     0.44f, 0.44f, 0.50f }, { "DRIVE",   0.60f, 0.42f, 0.50f },
            { "TAPE SAT", 0.52f, 0.62f, 0.50f },
            { "HALL",     0.26f, 0.26f, 0.55f }, { "PINGPONG",0.80f, 0.26f, 0.48f },
            { "PHASER",   0.52f, 0.96f, 0.46f } },
          { {0,1},{1,2},{2,0},{0,3},{1,4},{2,5} }, 0.50f, 1.20f },
    };

    return bank;
}

} // namespace invis::modules
