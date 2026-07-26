# =============================================================================
#  INVIS BENCH - NOT A PLUGIN
# =============================================================================
#  This target is a DEVELOPMENT BENCH for exercising invis_core: a board to play
#  with atoms, sidebars and the chassis. It is NOT a product, NOT a shippable
#  plugin, and NOT the template a real plugin should be copied from.
#
#  Consequences, deliberately:
#    * Standalone + VST3 only. No AU - a bench has no business going through AU
#      validation. VST3 exists purely so the bench can be fed REAL audio in a
#      DAW; its own generator only ever produces a steady 100 Hz sine, which is
#      why the metering, LUFS and AUTO paths have never seen real programme.
#    * It is NOT installed into ~/Library/Audio/Plug-Ins unless explicitly asked.
#      A bench must never turn up in someone's plugin list by accident.
#      Opt in with -DINVIS_INSTALL_BENCH=ON.
#
#  Real plugins get the shared chassis, not a copy of this directory.
# =============================================================================

option(INVIS_INSTALL_BENCH "Copy the Invis bench into the system plug-in folders" OFF)

juce_add_plugin(InvisDemoPlugin
    COMPANY_NAME "Invis Audio"
    BUNDLE_ID "com.invisaudio.bench"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    PLUGIN_MANUFACTURER_CODE Invs
    PLUGIN_CODE Ibnc
    FORMATS Standalone VST3
    COPY_PLUGIN_AFTER_BUILD ${INVIS_INSTALL_BENCH}
    PRODUCT_NAME "Invis Bench"
)

target_compile_definitions(InvisDemoPlugin PUBLIC
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_sources(InvisDemoPlugin PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
)

target_link_libraries(InvisDemoPlugin PRIVATE
    invis_core
    juce::juce_audio_utils
    juce::juce_dsp
)

juce_generate_juce_header(InvisDemoPlugin)
