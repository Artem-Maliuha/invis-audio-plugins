# =============================================================================
#  CONSTELLATION ROYAL - the all-in-one of the series
# =============================================================================
#  The first real product. Every family lives here: spatial, modulation and
#  saturation, arranged by drawing them.
#
#  Unlike the bench this ships: AU as well as VST3 and Standalone, and it
#  installs into the system plug-in folders by default, because a plugin nobody
#  can load is not a plugin.
#
#  It carries no code of its own beyond the two lines that introduce the chassis
#  to the workspace. Everything else is invis_core, which is the whole reason
#  the workspace was lifted out of the bench.
# =============================================================================

juce_add_plugin(ConstellationRoyal
    COMPANY_NAME "Invis Audio"
    BUNDLE_ID "com.invisaudio.constellationroyal"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    PLUGIN_MANUFACTURER_CODE Invs
    PLUGIN_CODE Croy
    FORMATS AU VST3 Standalone
    COPY_PLUGIN_AFTER_BUILD TRUE
    PRODUCT_NAME "Constellation Royal"
)

target_compile_definitions(ConstellationRoyal PUBLIC
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_sources(ConstellationRoyal PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
)

target_link_libraries(ConstellationRoyal PRIVATE
    invis_core
    juce::juce_audio_utils
    juce::juce_dsp
)

juce_generate_juce_header(ConstellationRoyal)
